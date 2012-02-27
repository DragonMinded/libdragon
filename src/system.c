/**
 * @file system.c
 * @brief newlib Interface Hooks
 * @ingroup system
 */
#include <_ansi.h>
#include <_syslist.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <stdint.h>
#include <malloc.h>
#include "system.h"

#undef errno

/** 
 * @defgroup system newlib Interface Hooks
 * @brief System hooks to provide low level threading and filesystem functionality to newlib.
 *
 * newlib provides all of the standard C libraries for homebrew development.
 * In addition to standard C libraries, newlib provides some additional bridging
 * functionality to allow POSIX function calls to be tied into libdragon.
 * Currently this is used only for filesystems.  The newlib interface hooks here
 * are mostly stubs that allow homebrew applications to compile.
 *
 * The sbrk function is responsible for allowing newlib to find the next chunk
 * of free space for use with malloc calls.  This is made somewhat complicated
 * on the N64 by the fact that precompiled code doesn't know in advance if
 * expanded memory is available.  libdragon attempts to determine if this additional
 * memory is available and return accordingly but can only do so if it knows what
 * type of CIC/bootcode was used.  If you are using a 6102, this has been set for
 * you already.  If you are using a 6105 for some reason, you will need to use
 * #sys_set_boot_cic to notify libdragon or malloc will not work properly!
 *
 * libdragon has defined a custom callback structure for filesystems to use.
 * Providing relevant hooks for calls that your filesystem supports and passing
 * the resulting structure to #attach_filesystem will hook your filesystem into
 * newlib.  Calls to POSIX file operations will be passed on to your filesystem
 * code if the file prefix matches, allowing code to make use of your filesystyem
 * without being rewritten.
 *
 * For example, your filesystem provides libdragon an interface to access a 
 * homebrew SD card interface.  You register a filesystem with "sd:/" as the prefix
 * and then attempt to open "sd://directory/file.txt".  The open callback for your
 * filesystem will be passed the file "/directory/file.txt".  The file handle returned
 * will be passed into all subsequent calls to your filesystem until the file is
 * closed.
 * @{
 */

/**
 * @name STDIN/STDOUT/STDERR definitions from unistd.h
 *
 * We can't just include unistd.h as it redefines several of the functions
 * here that we are attempting to replace.
 *
 * @{
 */
/** @brief Standard input file descriptor */
#define STDIN_FILENO    0
/** @brief Standard output file descriptor */
#define STDOUT_FILENO   1
/** @brief Standard error file descriptor */
#define STDERR_FILENO   2
/** @} */

/**
 * @brief Stack size
 *
 * @todo Dirty hack, should investigate this further
 */
#define STACK_SIZE 0x10000

/**
 * @brief Write to the MESS debug register
 *
 * @param[in] x
 *            32-bit value to write to the MESS debug register
 */
#define DEBUG_OUT( x ) ((uint32_t *)0xA4400044)[0] = ((uint32_t)(x))

/**
 * @brief Environment variables
 */
char *__env[1] = { 0 };

/** 
 * @brief Environment variables
 */
char **environ = __env;

/** 
 * @brief Dummy declaration of timeval
 */
struct timeval;

/**
 * @brief Master definition of errno
 */
int errno;

/* Externs from libdragon */
extern int __bootcic;
extern void enable_interrupts();
extern void disable_interrupts();

/**
 * @brief Filesystem mapping structure
 *
 * This is used to look up what filesystem to use when passed
 * a generic path.
 */
typedef struct
{
    /** @brief Filesystem prefix
     *
     * This controls what filesystem prefix should link to this filesystem
     * (eg. 'rom:/' or 'cf:/') 
     */
    char *prefix;
    /** @brief Filesystem callback pointers */
    filesystem_t *fs;
} fs_mapping_t;

/**
 * @brief Filesystem open handle structure
 *
 * This is used to look up the correct filesystem function to call
 * when working with an open file handle
 */
typedef struct
{
    /** @brief Index into #filesystems */
    int fs_mapping;
    /** @brief The handle assigned to this open file as returned by the 
     *         filesystem code called to handle the open operation.  Will
     *         be passed to all subsequent file operations on the file. */
    void *handle;
    /** @brief The handle assigned by the filesystem code that will be returned
     *         to newlib.  All subsequent newlib calls will use this handle which
     *         will be used to look up the internal reference. */
    int fileno;
} fs_handle_t;

/** @brief Array of filesystems registered */
static fs_mapping_t filesystems[MAX_FILESYSTEMS] = { { 0 } };
/** @brief Array of open handles tracked */
static fs_handle_t handles[MAX_OPEN_HANDLES] = { { 0 } };
/** @brief Current stdio hook structure */
static stdio_t stdio_hooks = { 0 };

/* Forward definitions */
int close( int fildes );

/**
 * @brief Simple implementation of strlen
 *
 * @note We can't link against regular libraries, so this is reimplemented
 *
 * @param[in] str
 *            Pointer to null-terminated string
 *
 * @return Length of string
 */
static int __strlen( const char * const str )
{
    if( !str ) { return 0; }

    int len = 0;
    while( str[len] != 0 )
    {
        len++;
    }

    return len;
}

/**
 * @brief Simple implementation of memcpy
 *
 * @note We can't link against regular libraries, so this is reimplemented
 *
 * @param[out] a
 *             Destination pointer to copy to
 * @param[in]  b
 *             Source pointer to copy from
 * @param[in]  len
 *             Length in bytes to copy
 */
static void __memcpy( char * const a, const char * const b, int len )
{
    for( int i = 0; i < len; i++ )
    {
        a[i] = b[i];
    }
}

/**
 * @brief Simple implementation of strdup
 *
 * @note We can't link against regular libraries, so this is reimplemented
 *
 * @param[in] in
 *            String to duplicate
 *
 * @return Pointer to newly allocate memory containing a copy of the input string
 */
static char *__strdup( const char * const in )
{
    if( !in ) { return 0; }

    char *ret = malloc( __strlen( in ) + 1 );
    __memcpy( ret, in, __strlen( in ) + 1 );

    return ret;
}

/**
 * @brief Simple iplementation of strncmp
 *
 * @note We can't link against regular libraries, so this is reimplemented
 *
 * @param[in] a
 *            First string to compare against
 * @param[in] b
 *            Second string to compare against
 * @param[in] len
 *            Number of relevant characters.  Specify -1 for infinite
 *
 * @return 0 if the two strings match or nonzero otherwise
 */
static int __strncmp( const char * const a, const char * const b, int len )
{
    if( !a || !b ) { return 0; }

    int cur = 0;
    while( cur != len )
    {
        /* Only care about equality */
        if( a[cur] != b[cur] ) { return 1; }
        if( a[cur] == 0 && b[cur] == 0 ) { return 0; }
        if( a[cur] == 0 || b[cur] == 0 ) { return 1; }

        /* Next please */
        cur++;
    }

    /* Must have worked */
    return 0;
}

/**
 * @brief Simple implementation of strcmp
 *
 * @note We can't link against regular libraries, so this is reimplemented
 *
 * @param[in] a
 *            First string to compare against
 * @param[in] b
 *            Second string to compare against
 *
 * @return 0 if the two strings match or nonzero otherwise
 */
static int __strcmp( const char * const a, const char * const b )
{
    return __strncmp( a, b, -1 );
}

/**
 * @brief Return a unique filesystem handle
 *
 * @return A unique 32-bit value usable as a filesystem handle
 */
static int __get_new_handle()
{
    /* Start past STDIN, STDOUT, STDERR file handles */
    static int handle = 3;
    int newhandle;

    disable_interrupts();

    /* Always give out a nonzero handle unique to the system */
    newhandle = handle++;

    enable_interrupts();

    return newhandle;
}

/**
 * @brief Register a filesystem with newlib
 *
 * This function will take a prefix in the form of 'prefix:/' and a pointer
 * to a filesystem structure of relevant callbacks and register it with newlib.
 * Any standard open/fopen calls with the registered prefix will be passed
 * to this filesystem.  Userspace code does not need to know the underlying
 * filesystem, only the prefix that it has been registered under.
 *
 * The filesystem pointer passed in to this function should not go out of scope
 * for the lifetime of the filesystem.
 *
 * @param[in] prefix
 *            Prefix of the filesystem
 * @param[in] filesystem
 *            Structure of callbacks for various functions in the filesystem.
 *            If the registered filesystem doesn't support an operation, it
 *            should leave the callback null.
 * 
 * @retval -1 if the parameters are invalid
 * @retval -2 if the prefix is already in use
 * @retval -3 if there are no more slots for filesystems
 * @retval 0 if the filesystem was registered successfully
 */
int attach_filesystem( const char * const prefix, filesystem_t *filesystem )
{
    /* Sanity checking */
    if( !prefix || !filesystem ) 
    { 
        errno = EINVAL;
        return -1; 
    }

    /* Make sure prefix is valid */
    int len = __strlen( prefix );

    if( len < 3 || prefix[len - 1] != '/' || prefix[len - 2] != ':' )
    {
        errno = EINVAL;
        return -1;
    }

    /* Make sure the prefix doesn't match one thats already inserted */
    for( int i = 0; i < MAX_FILESYSTEMS; i++ )
    {
        if( filesystems[i].prefix )
        {
            if( __strcmp( filesystems[i].prefix, prefix ) == 0 )
            {
                /* Filesystem has already been inserted */
                errno = EPERM;
                return -2;
            }
        }
    }

    /* Find an open handle */
    int handle = -1;
    for( int i = 0; i < MAX_FILESYSTEMS; i++ )
    {
        if( !filesystems[i].prefix )
        {
            /* Found one */
            handle = i;
            break;
        }
    }

    if( handle == -1 )
    {
        /* No more filesystem handles available */
        errno = ENOMEM;
        return -3;
    }

    /* Attach the prefix */
    filesystems[handle].prefix = __strdup( prefix );

    /* Attach the inputted filesystem */
    filesystems[handle].fs = filesystem;

    /* All went well */
    return 0;
}

/**
 * @brief Unregister a filesystem from newlib
 *
 * @note This function will make sure all files are closed before unregistering
 *       the filesystem.
 *
 * @param[in] prefix
 *            The prefix that was used to register the filesystem
 *
 * @retval -1 if the parameters were invalid
 * @retval -2 if the filesystem couldn't be found
 * @retval 0 if the filesystem was successfully unregistered
 */
int detach_filesystem( const char * const prefix )
{
    /* Sanity checking */
    if( !prefix ) 
    { 
        errno = EINVAL;
        return -1; 
    }

    for( int i = 0; i < MAX_FILESYSTEMS; i++ )
    {
        if( filesystems[i].prefix )
        {
            if( __strcmp( filesystems[i].prefix, prefix ) == 0 )
            {
                /* We found the filesystem, now go through and close every open file handle */
                for( int j = 0; j < MAX_OPEN_HANDLES; j++ )
                {
                    if( handles[j].fileno > 0 && handles[j].fs_mapping == i )
                    {
                        close( handles[j].fileno );
                    }
                }

                /* Now free the memory associated with the prefix and zero out the filesystem */
                free( filesystems[i].prefix );
                filesystems[i].prefix = 0;
                filesystems[i].fs = 0;

                /* All went well */
                return 0;
            }
        }
    }

    /* Couldn't find the filesystem to free */
    errno = EPERM;
    return -2;
}

/**
 * @brief Get a filesystem pointer by handle
 *
 * Given a file handle, return the filesystem callback structure.
 *
 * @param[in] fileno
 *            File handle
 * 
 * @return Pointer to a filesystem callback structure or null if not found.
 */
static filesystem_t *__get_fs_pointer_by_handle( int fileno )
{
    /* Invalid */
    if( fileno <= 0 )
    {
        return 0;
    }

    for( int i = 0; i < MAX_OPEN_HANDLES; i++ )
    {
        if( handles[i].fileno == fileno )
        {
            /* Found it */
            return filesystems[handles[i].fs_mapping].fs;
        }
    }

    /* Couldn't find it */
    return 0;
}

/**
 * @brief Get the index of the registered filesystem based on fully qualified filename
 *
 * @param[in] name
 *            The filename of the file being opened including the prefix
 *
 * @return The index of the registered filesystem if found or -1 if not found.
 */
static int __get_fs_link_by_name( const char * const name )
{
    /* Invalid */
    if( !name )
    {
        return -1;
    }

    for( int i = 0; i < MAX_FILESYSTEMS; i++ )
    {
        if( filesystems[i].prefix )
        {
            if( __strncmp( filesystems[i].prefix, name, __strlen( filesystems[i].prefix ) ) == 0 )
            {
                /* Found it */
                return i;
            }
        }
    }

    /* Couldn't find it */
    return -1;

}

/**
 * @brief Get the filesystem callback structure based on a fully qualified filename
 *
 * @param[in] name
 *            The filename of the file being opened including the prefix
 *
 * @return Pointer to a filesystem callback structure or null if not found.
 */
static filesystem_t *__get_fs_pointer_by_name( const char * const name )
{
    int fs = __get_fs_link_by_name( name );

    if( fs >= 0 )
    {
        return filesystems[fs].fs;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Look up the internal handle of a file given a file handle
 *
 * @param[in] fileno
 *            File handle
 *
 * @return The internal file handle to be passed to the filesystem function or null if not found.
 */
static void *__get_fs_handle( int fileno )
{
    /* Invalid */
    if( fileno <= 0 )
    {
        return 0;
    }

    for( int i = 0; i < MAX_OPEN_HANDLES; i++ )
    {
        if( handles[i].fileno == fileno )
        {
            /* Found it */
            return handles[i].handle;
        }
    }

    /* Couldn't find it */
    return 0;
}

/**
 * @brief Change ownership on a file or directory
 *
 * @note Not supported in libdragon
 *
 * @param[in] path
 *            Path of the file or directory to operate on
 * @param[in] owner
 *            New owner of the file
 * @param[in] group
 *            New group of the file
 *
 * @return 0 on success or a negative value on error.
 */
int chown( const char *path, uid_t owner, gid_t group )
{
    /* No permissions support in libdragon */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Close a file
 *
 * @param[in] fildes
 *            File handle of the file to close
 *
 * @return 0 on success or a negative value on error.
 */
int close( int fildes )
{
    filesystem_t *fs = __get_fs_pointer_by_handle( fildes );
    void *handle = __get_fs_handle( fildes );

    if( fs == 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( fs->close == 0 )
    {
        /* Filesystem doesn't support close */
        errno = ENOSYS;
        return -1;
    }

    return fs->close( handle );
}

/**
 * @brief Load and execute an executable given a path
 *
 * @note Not supported in libdragon
 *
 * @param[in] name
 *            Filename of the executable
 * @param[in] argv
 *            Array of pointers to arguments
 * @param[in] env
 *            Array of pointers to environment variables
 *
 * @return 0 on success or a negative value on error.
 */
int execve( char *name, char **argv, char **env )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief End execution on current thread
 *
 * @note This function does not exit.  If this is the last thread, the
 *       system will hang.
 *
 * @param[in] rc
 *            Return value of the exiting program
 */
void exit( int rc )
{
    /* Default stub just causes a divide by 0 exception.  */
    int x = rc / INT_MAX;
    x = 4 / x;

    /* Convince GCC that this function never returns.  */
    for( ;; );
}

/**
 * @brief Fork execution into two threads
 *
 * @note Not supported in libdragon.
 *
 * @return PID of child process if parent, 0 if child, negative value on failure.
 */
int fork( void )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Return stats on an open file handle
 *
 * @param[in]  fildes
 *             File handle
 * @param[out] st
 *             Pointer to stat struct to be filled
 *
 * @return 0 on success or a negative value on error.
 */
int fstat( int fildes, struct stat *st )
{
    if( st == NULL )
    {
        errno = EINVAL;
        return -1;
    }
    else
    {
        filesystem_t *fs = __get_fs_pointer_by_handle( fildes );
        void *handle = __get_fs_handle( fildes );

        if( fs == 0 )
        {
            errno = EINVAL;
            return -1;
        }

        if( fs->fstat == 0 )
        {
            /* Filesystem doesn't support fstat */
            errno = ENOSYS;
            return -1;
        }

        return fs->fstat( handle, st );
    }
}

/**
 * @brief Return the PID of the current thread
 *
 * @note Not supported in libdragon.
 *
 * @return The PID on success or a negative value on error.
 */
int getpid( void )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Return the current time
 *
 * @note Not supported in libdragon.
 *
 * @param[out] ptimeval
 *             Time structure to be filled with current time.
 * @param[out] ptimezone
 *             Timezone information to be filled.
 *
 * @return 0 on success or a negative value on error.
 */
int gettimeofday( struct timeval *ptimeval, void *ptimezone )
{
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Return whether a file is a TTY or a regular file
 *
 * @param[in] file
 *            File handle
 *
 * @return 1 if the file is a TTY, or 0 if not.
 */
int isatty( int file )
{
    if( file == STDIN_FILENO ||
        file == STDOUT_FILENO ||
        file == STDERR_FILENO )
    {
        /* This is a TTY for libdragon's purposes */
        return 1;
    }
    else
    {
        /* Not a TTY */
        errno = EBADF;
        return 0;
    }
}

/**
 * @brief Send a signal to a PID
 *
 * @note Not supported in libdragon.
 *
 * @param[in] pid
 *            The PID of the process
 * @param[in] sig
 *            The signal to send
 *
 * @return 0 on success or a negative value on error.
 */
int kill( int pid, int sig )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Link a new file to an existing file
 *
 * @note Not supported in libdragon.
 *
 * @param[in] existing
 *            The path of the existing file
 * @param[in] new
 *            The path of the new file
 *
 * @return 0 on success or a negative value on error.
 */
int link( char *existing, char *new )
{
    /* No symbolic links in libdragon */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Seek to a location in a file
 *
 * @param[in] file
 *            The file handle of the file to seek
 * @param[in] ptr
 *            The offset in bytes to seek to, given the direction in dir
 * @param[in] dir
 *            The direction to seek.  Use SEEK_SET to start from the beginning.
 *            Use SEEK_CUR to seek based on the current offset.  Use SEEK_END
 *            to seek starting at the end of the file.
 *
 * @return The new location of the file in bytes or -1 on error.
 */
int lseek( int file, int ptr, int dir )
{
    filesystem_t *fs = __get_fs_pointer_by_handle( file );
    void *handle = __get_fs_handle( file );

    if( fs == 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( fs->lseek == 0 )
    {
        /* Filesystem doesn't support lseek */
        errno = ENOSYS;
        return -1;
    }

    return fs->lseek( handle, ptr, dir );
}

/**
 * @brief Open a file given a path
 *
 * @param[in] file
 *            File name of the file to open
 * @param[in] flags
 *            Flags specifying open flags, such as binary, append.
 * @param[in] mode
 *            Mode of the file.
 *
 * @return File handle to refer to this file on success, or a negative value on error.
 */
int open( char *file, int flags, int mode )
{
    filesystem_t *fs = __get_fs_pointer_by_name( file );

    if( fs == 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( fs->open == 0 )
    {
        /* Filesystem doesn't support open */
        errno = ENOSYS;
        return -1;
    }

    /* Do we have room for a new file? */
    for( int i = 0; i < MAX_OPEN_HANDLES; i++ )
    {
        if( handles[i].fileno == 0 )
        {
            /* Yes, we have room, try the open */
            int mapping = __get_fs_link_by_name( file );

            if( mapping < 0 )
            {
                errno = ENOMEM;
                return -1;
            }
 
            void *ptr = fs->open( file + __strlen( filesystems[mapping].prefix ), flags );

            if( ptr )
            {
                /* Create new internal handle */
                handles[i].fileno = __get_new_handle();
                handles[i].handle = ptr;
                handles[i].fs_mapping = mapping;

                /* Return our own handle */
                return handles[i].fileno;
            }
            else
            {
                /* Couldn't open for some reason */
                errno = EPERM;
                return -1;
            }
        }
    }

    /* No file handles available */
    errno = ENOMEM;
    return -1;
}

/**
 * @brief Read data from a file
 *
 * @param[in]  file
 *             File handle
 * @param[out] ptr
 *             Data pointer to read data to
 * @param[in]  len
 *             Length in bytes of data to read
 *
 * @return Actual number of bytes read or a negative value on error.
 */
int read( int file, char *ptr, int len )
{
    if( file == STDIN_FILENO )
    {
        if( stdio_hooks.stdin_read )
        {
            return stdio_hooks.stdin_read( ptr, len );
        }
        else
        {
            /* No hook for this */
            errno = EBADF;
            return -1;
        }
    }
    else if( file == STDOUT_FILENO ||
             file == STDERR_FILENO )
    {
        /* Can't read from output buffers */
        errno = EBADF;
        return -1;
    }
    else
    {
        /* Read from file */
        filesystem_t *fs = __get_fs_pointer_by_handle( file );
        void *handle = __get_fs_handle( file );

        if( fs == 0 )
        {
            errno = EINVAL;
            return -1;
        }

        if( fs->read == 0 )
        {
            /* Filesystem doesn't support read */
            errno = ENOSYS;
            return -1;
        }

        return fs->read( handle, (uint8_t *)ptr, len );
    }
}

/**
 * @brief Read a link
 *
 * @note Not supported in libdragon.
 *
 * @param[in]  path
 *             Path of the link
 * @param[in]  buf
 *             Buffer to read the link into
 * @param[in]  bufsize
 *             Size of the buffer
 * 
 * @return 0 on success or a negative value on error.
 */
int readlink( const char *path, char *buf, size_t bufsize )
{
    /* No symlinks in libdragon */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Return a new chunk of memory to be used as heap
 *
 * @param[in] incr
 *            The amount of memory needed in bytes
 *
 * @return A pointer to the memory or null on error allocating.
 */
void *sbrk( int incr )
{
    extern char   end; /* Set by linker.  */
    static char * heap_end, * heap_top;
    char *        prev_heap_end;
    const int     osMemSize = (__bootcic != 6105) ? (*(int*)0xA0000318) : (*(int*)0xA00003F0);

    disable_interrupts();

    if( heap_end == 0 )
    {
        heap_end = &end;
        heap_top = (char*)0x80000000 + osMemSize - STACK_SIZE;
    }

    prev_heap_end = heap_end;
    heap_end += incr;

    // check if out of memory
    if (heap_end > heap_top)
    {
        heap_end -= incr;
        prev_heap_end = (char *)-1;
        errno = ENOMEM;
    }

    enable_interrupts();

    return (void *)prev_heap_end;
}

/**
 * @brief Return file stats based on a file name
 *
 * @param[in]  file
 *             File name of the file in question
 * @param[out] st
 *             Stat struct to populate with information from the file
 *
 * @return 0 on success or a negative value on error.
 */
int stat( const char *file, struct stat *st )
{
    /* Dirty hack, open read only */
    int fd = open( (char *)file, 0, 777 );

    if( fd > 0 )
    {
        int ret = fstat( fd, st );
        close( fd );

        return ret;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

/**
 * @brief Create a symbolic link to a file
 *
 * @note Not supported in libdragon.
 *
 * @param[in] path1
 *            Path to symlink to
 * @param[in] path2
 *            Path to symlink from
 *
 * @return 0 on success or a negative value on error.
 */
int symlink( const char *path1, const char *path2 )
{
    /* No symlinks in libdragon */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Return time information on the current process
 *
 * @note Not supported in libdragon.
 *
 * @param[out] buf
 *             Buffer to place timing information
 *
 * @return Timing information or a negative value on error.
 */
clock_t times( struct tms *buf )
{
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Remove a file based on filename
 *
 * @param[in] name
 *            Name of the file to remove
 *
 * @return 0 on success or a negative value on error.
 */
int unlink( char *name )
{
    filesystem_t *fs = __get_fs_pointer_by_name( name );
    int mapping = __get_fs_link_by_name( name );

    if( fs == 0 || mapping < 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( fs->unlink == 0 )
    {
        /* Filesystem doesn't support unlink */
        errno = ENOSYS;
        return -1;
    }

    /* Must offset past the prefix */
    return fs->unlink( name + __strlen( filesystems[mapping].prefix ) );
}

/**
 * @brief Wait for a child process
 *
 * @note Not supported in libdragon.
 *
 * @param[out] status
 *             Status of the wait operation
 *
 * @return 0 on success or a negative value on error.
 */
int wait( int *status )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

/**
 * @brief Write data to a file
 *
 * @param[in] file
 *            File handle
 * @param[in] ptr
 *            Pointer to buffer to write to file
 * @param[in] len
 *            Length of data in bytes to be written
 *
 * @return The actual number of bytes written, or a negative value on error.
 */
int write( int file, char *ptr, int len )
{
    if( file == STDIN_FILENO )
    {
        /* Can't write to input buffers */
        errno = EBADF;
        return -1;
    }
    else if( file == STDOUT_FILENO )
    {
        if( stdio_hooks.stdout_write )
        {
            return stdio_hooks.stdout_write( ptr, len );
        }
        else
        {
            errno = EBADF;
            return -1;
        }
    }
    else if( file == STDERR_FILENO )
    {
        if( stdio_hooks.stderr_write )
        {
            return stdio_hooks.stderr_write( ptr, len );
        }
        else
        {
            errno = EBADF;
            return -1;
        }
    }
    else
    {
        /* Filesystem write */
        filesystem_t *fs = __get_fs_pointer_by_handle( file );
        void *handle = __get_fs_handle( file );

        if( fs == 0 )
        {
            errno = EINVAL;
            return -1;
        }

        if( fs->write == 0 )
        {
            /* Filesystem doesn't support write */
            errno = ENOSYS;
            return -1;
        }

        return fs->write( handle, (uint8_t *)ptr, len );
    }
}

/**
 * @brief Find the first file in a directory
 *
 * This function should be called to start enumerating a directory or whenever
 * a directory enumeration should be restarted.
 *
 * @param[in]  path
 *             Path to the directory structure
 * @param[out] dir
 *             Directory entry structure to populate with first entry
 *
 * @return 0 on successful lookup or a negative value on error.
 */
int dir_findfirst( const char * const path, dir_t *dir )
{
    filesystem_t *fs = __get_fs_pointer_by_name( path );
    int mapping = __get_fs_link_by_name( path );

    if( fs == 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( fs->findfirst == 0 )
    {
        /* Filesystem doesn't support findfirst */
        errno = ENOSYS;
        return -1;
    }

    return fs->findfirst( (char *)path + + __strlen( filesystems[mapping].prefix ), dir );
}

/**
 * @brief Find the next file in a directory
 *
 * After finding the first file in a directory using #dir_findfirst, call this to retrieve
 * the rest of the directory entries.  Call this repeatedly until a negative error is returned
 * signifying that there are no more directory entries in the directory.
 *
 * @param[in]  path
 *             Path to the directory structure
 * @param[out] dir
 *             Directory entry structure to populate with next entry
 *
 * @return 0 on successful lookup or a negative value on error.
 */
int dir_findnext( const char * const path, dir_t *dir )
{
    filesystem_t *fs = __get_fs_pointer_by_name( path );

    if( fs == 0 )
    {
        errno = EINVAL;
        return -1;
    }

    if( fs->findnext == 0 )
    {
        /* Filesystem doesn't support findfirst */
        errno = ENOSYS;
        return -1;
    }

    return fs->findnext( dir );
}

/**
 * @brief Hook into stdio for STDIN, STDOUT and STDERR callbacks
 *
 * @param[in] stdio_calls
 *            Pointer to structure containing callbacks for stdio functions
 *
 * @return 0 on successful hook or a negative value on failure.
 */
int hook_stdio_calls( stdio_t *stdio_calls )
{
    if( stdio_calls == NULL )
    {
        /* Failed to hook, bad input */
        return -1;
    }

    /* Safe to hook */
    stdio_hooks.stdin_read = stdio_calls->stdin_read;
    stdio_hooks.stdout_write = stdio_calls->stdout_write;
    stdio_hooks.stderr_write = stdio_calls->stderr_write;

    /* Success */
    return 0;
}

/**
 * @brief Unhook from stdio
 *
 * @return 0 on successful hook or a negative value on failure.
 */
int unhook_stdio_calls()
{
    /* Just wipe out internal variable */
    stdio_hooks.stdin_read = 0;
    stdio_hooks.stdout_write = 0;
    stdio_hooks.stderr_write = 0;

    /* Always successful for now */
    return 0;
}

/** @} */
