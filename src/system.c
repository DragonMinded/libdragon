/**
 * @file system.c
 * @brief newlib Interface Hooks
 * @ingroup system
 */
#include <_ansi.h>
#include <_syslist.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>
#include "system.h"
#include "n64sys.h"

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
 * This is the maximum stack size for the purpose of malloc.  Any malloc
 * call that tries to allocate data will not allocate within this range.
 * However, there is no guarantee that user code won't blow the stack and
 * cause heap corruption.  Use this as loose protection at best.
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
 * @brief Assert function pointer (initialized at startup)
 */
void (*__assert_func_ptr)(const char *file, int line, const char *func, const char *failedexpr) = 0;

/// @cond
extern void enable_interrupts();
extern void disable_interrupts();
/// @endcond

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

/** @brief Extract bits from word */
#define BITS(v, b, e)  ((unsigned int)(v) << (31-(e)) >> (31-(e)+(b))) 

/** @brief Number of buckets file handles are divided into */
#define HANDLE_MAX_BUCKETS         32
/** @brief Size of each bucket of file handles */
#define HANDLE_BUCKET_SIZE         32

/** @brief First bucket of file handles (allocated statically) */
static void *handle_first_bucket[HANDLE_BUCKET_SIZE];
/** @brief Array of buckets of file handles */
static void **handle_map[HANDLE_MAX_BUCKETS] = { handle_first_bucket };
/** @brief Number of allocated handle buckets (start from 1, as the first one is allocated statically) */
static int handle_buckets_count = 1;    
/** @brief Number of open handles */
static int handle_open_count;

/** @brief Create a fileno.
 * 
 * Filenos are created as bitfields containing a few fields. 
 * 
 * They contain the indices required to access the handle in handle_map:
 * the bucket index  and the position within the bucket where the handle is.
 * 
 * They also contain the filesystem index, so that we always know which
 * filesystem use to operate on the handle. Notice that the index is stored
 * 1-based instead of 0-based, so that the special filenos 1,2,3 (used for
 * stdin, stdout and stderr by C standard libraries) will never conflict
 * with a fileno made by FILENO_MAKE.
 * 
 * @note POSIX does not specify any specific limit for filenos returned by
 *       open(), besides that they need to be non-negative integers. Newlib
 *       stores them into a 16-bit signed integer though, so we are actually
 *       limited to 15 bits for them.
 */
#define FILENO_MAKE( bkt_idx, bkt_pos, fs_index )   ( (bkt_pos) | ((bkt_idx) << 5) | (((fs_index)+1) << 11) )

/** @brief Extract the filesystem index from a fileno */
#define FILENO_GET_FS_INDEX( fileno )               ((int)BITS( (fileno), 11, 14 ) - 1)
/** @brief Extract the bucket index from a fileno */
#define FILENO_GET_BUCKET_IDX( fileno )             BITS( (fileno),  5, 10 )
/** @brief Extract the bucket position from a fileno */
#define FILENO_GET_BUCKET_POS( fileno )             BITS( (fileno),  0,  4 )

/** @brief Array of filesystems registered */
static fs_mapping_t filesystems[MAX_FILESYSTEMS] = { { 0 } };
/** @brief Current stdio hook structure */
static stdio_t stdio_hooks = { 0 };
/** @brief Function to provide the current time */
time_t (*time_hook)( void ) = NULL;

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
 * @return Pointer to newly allocated memory containing a copy of the input string
 */
static char *__strdup( const char * const in )
{
    if( !in ) { return 0; }

    char *ret = malloc( __strlen( in ) + 1 );
    __memcpy( ret, in, __strlen( in ) + 1 );

    return ret;
}

/**
 * @brief Simple implementation of strncmp
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
 * 
 * @note different from the standard strncmp
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
 * 
 * @note different from the standard strcmp
 */
static int __strcmp( const char * const a, const char * const b )
{
    return __strncmp( a, b, -1 );
}

/**
 * @brief Simple implementation of rand()
 * 
 * @param state         Random state
 * @return uint32_t     New random value
 */
static uint32_t __rand( uint32_t *state )
{
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 5;
	return *state = x;
}

/**
 * @brief Generate a random number in range [0..n[
 * 
 * @param state         Random state
 * @param n             Upper bound (exclusive)
 * @return uint32_t     Random number
 */
static inline uint32_t __randn( uint32_t *state, int n )
{
    if(__builtin_constant_p( n )) return __rand( state ) % n;
    return ((uint64_t)__rand( state ) * n) >> 32;
}

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
 * @brief Allocate a new fileno for the given handle
 * 
 * @param handle        Filesystem handle
 * @param fs_index      Filesystem index
 * @return int          New fileno, or -1 if it cannot be allocated (errno will be set)
 */
static int __allocate_fileno( void *handle, int fs_index )
{
    /* Allocate whenever the handle map is full at 75% to avoid wasting too
     * much time looking for an empty ID. */
    if( !handle_buckets_count || 
        (handle_buckets_count < HANDLE_MAX_BUCKETS &&
         handle_open_count + (handle_open_count>>2) > handle_buckets_count * HANDLE_BUCKET_SIZE) )
    {
        void *mem = calloc( HANDLE_BUCKET_SIZE, sizeof( void* ) );
        if( !mem ) 
        {
            errno = ENOMEM;
            return -1;
        }
        handle_map[handle_buckets_count++] = mem;
    }

    /* Select a random bucket and a random initial position. This should
     * help finding an empty slot fast enough. Use the handle pointer
     * as seed; avoid using C0_COUNT because aggressively changing fileno
     * might cause some headaches during debugging sessions. */
    uint32_t rand_state = (uint32_t)handle ^ (uint32_t)fs_index;
    uint32_t bkt_idx = handle_buckets_count > 1 ? __randn( &rand_state, handle_buckets_count ) : 0;
    uint32_t bkt_pos = __randn( &rand_state, HANDLE_BUCKET_SIZE );

    /* Go through all buckets and positions and look for an empty slot. */
    for (int i=0; i<handle_buckets_count; i++)
    {
        for (int j=0; j<HANDLE_BUCKET_SIZE; j++)
        {
            if (handle_map[bkt_idx][bkt_pos] == 0)
            {
                handle_map[bkt_idx][bkt_pos] = handle;
                handle_open_count++;
                return FILENO_MAKE( bkt_idx, bkt_pos, fs_index );
            }
            bkt_pos = (bkt_pos+1) % HANDLE_BUCKET_SIZE;
        }

        bkt_idx++;
        if (bkt_idx == handle_buckets_count)
            bkt_idx = 0;
    }

    /* All slots are full. Set ENFILE and return error */
    errno = ENFILE;
    return -1;
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

    int fs_index = FILENO_GET_FS_INDEX( fileno );

    if ( fs_index < 0 || fs_index >= MAX_FILESYSTEMS || filesystems[fs_index].fs == NULL )
    {
        return 0;
    }

    return filesystems[fs_index].fs;
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
 * @return The pointer to the handle map slot containing the handle
 */
static void **__get_fs_handle( int fileno )
{
    /* Invalid */
    if( fileno <= 0 )
    {
        return 0;
    }

    int bkt_idx = FILENO_GET_BUCKET_IDX( fileno );
    int bkt_pos = FILENO_GET_BUCKET_POS( fileno );
    
    if ( bkt_idx >= handle_buckets_count || bkt_pos >= HANDLE_BUCKET_SIZE ||
         handle_map[bkt_idx][bkt_pos] == 0 )
    {
        return 0;
    }

    return &handle_map[bkt_idx][bkt_pos];
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
 * @param[in] fileno
 *            File handle of the file to close
 *
 * @return 0 on success or a negative value on error.
 */
int close( int fileno )
{
    filesystem_t *fs = __get_fs_pointer_by_handle( fileno );

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

    void **handle_ptr = __get_fs_handle( fileno );

    if( handle_ptr == 0 ) 
    {
        errno = EINVAL;
        return -1;
    }

    /* Access the filesystem handle */
    void *handle = *handle_ptr;

    /* Clear the map slot */
    *handle_ptr = 0;
    handle_open_count--;

    /* Tell the filesystem to close the file */
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
void _exit( int rc )
{
    /* Loop infinitely. */
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
 * @param[in]  fileno
 *             File handle
 * @param[out] st
 *             Pointer to stat struct to be filled
 *
 * @return 0 on success or a negative value on error.
 */
int fstat( int fileno, struct stat *st )
{
    if( st == NULL )
    {
        errno = EINVAL;
        return -1;
    }
    else if ( fileno < 3 )
    {
        errno = EINVAL;
        return -1;
    }
    else
    {
        filesystem_t *fs = __get_fs_pointer_by_handle( fileno );
        void **handle_ptr = __get_fs_handle( fileno );

        if( fs == 0 || handle_ptr == 0 )
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

        return fs->fstat( *handle_ptr, st );
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
 * @param[out] ptimeval
 *             Time structure to be filled with current time.
 * @param[out] ptimezone
 *             Timezone information to be filled. (Not supported)
 *
 * @return 0 on success or a negative value on error.
 */
int gettimeofday( struct timeval *ptimeval, void *ptimezone )
{
    if( time_hook != NULL )
    {
        time_t time = time_hook();
        if( time != -1 )
        {
            ptimeval->tv_sec = time;
            ptimeval->tv_usec = 0;
            return 0;
        }
    }

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
    void **handle_ptr = __get_fs_handle( file );

    if( fs == 0 || handle_ptr == 0 )
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

    return fs->lseek( *handle_ptr, ptr, dir );
}

/**
 * @brief Open a file given a path
 *
 * @param[in] file
 *            File name of the file to open
 * @param[in] flags
 *            Flags specifying open flags, such as binary, append.
 * @param[in] ... mode
 *            Mode of the file (currently ignored).
 *
 * @return File handle to refer to this file on success, or a negative value on error.
 */
int open( const char *file, int flags, ... )
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

    /* Use this to get the mode argument if needed (for O_CREAT and O_TMPFILE). */
    if(0)
    {
        __attribute__((unused)) int mode;
        va_list ap;

        va_start (ap, flags);
        mode = va_arg (ap, int);
        va_end (ap);
    }

    /* Search the filesystem associated with this file */
    int fs_index = __get_fs_link_by_name( file );

    if( fs_index < 0 )
    {
        errno = EINVAL;
        return -1;
    }

    /* Clear errno so we can check whether the fs->open() call sets it. 
        This is for backward compatibility, because we used not to require
        errno to be set. */
    errno = 0;

    /* Use the old open() call that will cause an additional allocation */
    void *handle = fs->open( (char *)( file + __strlen( filesystems[fs_index].prefix ) ), flags );

    if( handle )
    {
        return __allocate_fileno( handle, fs_index );
    }

    /* Couldn't open for some reason */
    if( errno == 0 )
        errno = ENOENT;

    return -1;
}

/**
 * @brief Read data from a file
 *
 * @param[in]  fileno
 *             Fileno for this file
 * @param[out] ptr
 *             Data pointer to read data to
 * @param[in]  len
 *             Length in bytes of data to read
 *
 * @return Actual number of bytes read or a negative value on error.
 */
int read( int fileno, char *ptr, int len )
{
    if( fileno == STDIN_FILENO )
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
    else if( fileno == STDOUT_FILENO ||
             fileno == STDERR_FILENO )
    {
        /* Can't read from output buffers */
        errno = EBADF;
        return -1;
    }
    else
    {
        /* Read from file */
        filesystem_t *fs = __get_fs_pointer_by_handle( fileno );
        void **handle_ptr = __get_fs_handle( fileno );

        if( fs == 0 || handle_ptr == 0 )
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

        return fs->read( *handle_ptr, (uint8_t *)ptr, len );
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
 * @return A pointer to the memory or ((void*)-1) on error allocating.
 */
void *sbrk( int incr )
{
    static char * heap_end = 0;
    static char * heap_top = 0;
    char *        prev_heap_end;

    disable_interrupts();

    if( heap_end == 0 )
    {
        heap_end = (char*)HEAP_START_ADDR;
        heap_top = (char*)KSEG0_START_ADDR + get_memory_size() - STACK_SIZE;
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
    int fd = open( (char *)file, O_RDONLY );
    if( fd < 0 )
        return fd;

    /* Run fstat */
    int ret = fstat( fd, st );
    close( fd );

    return ret;
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
        void **handle_ptr = __get_fs_handle( file );

        if( fs == 0 || handle_ptr == 0 )
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

        return fs->write( *handle_ptr, (uint8_t *)ptr, len );
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

    if( fs == 0 || mapping < 0 || dir == 0 )
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

    /* Initialize dir_t structure. Set size to -1 in case the filesystem
       does not report it. */
    __builtin_memset( dir, 0, sizeof( dir_t ) );
    dir->d_size = -1;

    return fs->findfirst( (char *)path + __strlen( filesystems[mapping].prefix ) - 1, dir );
}

int dir_findnext( const char * const path, dir_t *dir )
{
    filesystem_t *fs = __get_fs_pointer_by_name( path );

    if( fs == 0 || dir == 0 )
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

int hook_stdio_calls( stdio_t *stdio_calls )
{
    if( stdio_calls == NULL )
    {
        /* Failed to hook, bad input */
        return -1;
    }

    /* Safe to hook */
    if (stdio_calls->stdin_read)
        stdio_hooks.stdin_read = stdio_calls->stdin_read;
    if (stdio_calls->stdout_write)
        stdio_hooks.stdout_write = stdio_calls->stdout_write;
    if (stdio_calls->stderr_write)
        stdio_hooks.stderr_write = stdio_calls->stderr_write;

    /* Success */
    return 0;
}

int unhook_stdio_calls( stdio_t *stdio_calls )
{
    /* Just wipe out internal variable */
    if (stdio_calls->stdin_read == stdio_hooks.stdin_read)
        stdio_hooks.stdin_read = 0;
    if (stdio_calls->stdout_write == stdio_hooks.stdout_write)
        stdio_hooks.stdout_write = 0;
    if (stdio_calls->stderr_write == stdio_hooks.stderr_write)
        stdio_hooks.stderr_write = 0;

    /* Always successful for now */
    return 0;
}

int hook_time_call( time_t (*time_fn)( void ) )
{
    if( time_fn == NULL )
    {
        return -1;
    }

    time_hook = time_fn;

    return 0;
}

int unhook_time_call( time_t (*time_fn)( void ) )
{
    if( time_hook == time_fn )
    {
        time_hook = NULL;
    }

    return 0;
}

/**
 * @brief Implement _flush_cache as required by GCC for nested functions.
 *
 * When using the nested function extensions of GCC, a call to _flush_cache
 * is generated which must be supplied by the operating system or runtime
 * to allow flushing the instruction cache for the generated trampoline.
 */
void _flush_cache(uint8_t* addr, unsigned long bytes) {
    data_cache_hit_writeback(addr, bytes);
    inst_cache_hit_invalidate(addr, bytes);
}

/**
 * @brief Implement underlying function for assert()
 *
 * Implementation of the function called when an assert fails. By default,
 * we just abort execution, but this will be overriden at startup with
 * a function that prints the assertion on the screen and via the debug
 * channel (if initialized).
 *
 */
void __assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
    if (__assert_func_ptr)
        __assert_func_ptr(file, line, func, failedexpr);
    abort();
}
