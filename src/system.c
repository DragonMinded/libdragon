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

/* Dirty hack, should investigate this further */
#define STACK_SIZE 0x10000

#define DEBUG_OUT( x ) ((uint32_t *)0xA4400044)[0] = ((uint32_t)(x))

/* No idea what this is for */
char *__env[1] = { 0 };
char **environ = __env;
struct timeval;

/* Not otherwise supplied */
int errno;

/* bootchip set in n64sys.c */
extern int __bootcic;

/* For dealing with multiple filesystems */
typedef struct
{
    char *prefix;
    filesystem_t *fs;
} fs_mapping_t;

typedef struct
{
    int fs_mapping;
    void *handle;
    int fileno;
} fs_handle_t;

fs_mapping_t filesystems[MAX_FILESYSTEMS] = { { 0 } };
fs_handle_t handles[MAX_OPEN_HANDLES] = { { 0 } };

/* Forward definitions */
int close( int fildes );

/* Simple versions of functions we can't link against */
int __strlen( const char * const str )
{
    if( !str ) { return 0; }

    int len = 0;
    while( str[len] != 0 )
    {
        len++;
    }

    return len;
}

void __memcpy( char * const a, const char * const b, int len )
{
    for( int i = 0; i < len; i++ )
    {
        a[i] = b[i];
    }
}

char *__strdup( const char * const in )
{
    if( !in ) { return 0; }

    char *ret = malloc( __strlen( in ) + 1 );
    __memcpy( ret, in, __strlen( in ) + 1 );

    return ret;
}

int __strncmp( const char * const a, const char * const b, int len )
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

int __strcmp( const char * const a, const char * const b )
{
    return __strncmp( a, b, -1 );
}

/* TODO: Make this function atomic */
int get_new_handle()
{
    /* Always give out a nonzero handle unique to the system */
    static int handle = 1;

    return handle++;
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
    return -1;
}

filesystem_t *__get_fs_pointer_by_handle( int fileno )
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

int __get_fs_link_by_name( const char * const name )
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
filesystem_t *__get_fs_pointer_by_name( const char * const name )
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

void *__get_fs_handle( int fileno )
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

int chown( const char *path, uid_t owner, gid_t group )
{
    /* No permissions support in libdragon */
    errno = ENOSYS;
    return -1;
}

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

int execve( char *name, char **argv, char **env )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

void exit( int rc )
{
    /* Default stub just causes a divide by 0 exception.  */
    int x = rc / INT_MAX;
    x = 4 / x;

    /* Convince GCC that this function never returns.  */
    for( ;; );
}

int fork( void )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

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

int getpid( void )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}


int gettimeofday( struct timeval *ptimeval, void *ptimezone )
{
    errno = ENOSYS;
    return -1;
}

int isatty( int file )
{
    errno = ENOSYS;
    return 0;
}

int kill( int pid, int sig )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

int link( char *existing, char *new )
{
    /* No symbolic links in libdragon */
    errno = ENOSYS;
    return -1;
}

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
                handles[i].fileno = get_new_handle();
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

int read( int file, char *ptr, int len )
{
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

int readlink( const char *path, char *buf, size_t bufsize )
{
    /* No symlinks in libdragon */
    errno = ENOSYS;
    return -1;
}

/* TODO: Make this function atomic */
void *sbrk( int incr )
{
    extern char   end; /* Set by linker.  */
    static char * heap_end, * heap_top;
    char *        prev_heap_end;
    const int     osMemSize = (__bootcic != 6105) ? (*(int*)0xA0000318) : (*(int*)0xA00003F0);

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

    return (void *)prev_heap_end;
}

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

int symlink( const char *path1, const char *path2 )
{
    /* No symlinks in libdragon */
    errno = ENOSYS;
    return -1;
}

clock_t times( struct tms *buf )
{
    errno = ENOSYS;
    return -1;
}

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

int wait( int *status )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

int write( int file, char *ptr, int len )
{
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

int dir_findfirst( const char * const path, dir_t *dir )
{
    filesystem_t *fs = __get_fs_pointer_by_name( path );

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

    return fs->findfirst( (char *)path, dir );
}

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
