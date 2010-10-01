#include <_ansi.h>
#include <_syslist.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <stdint.h>
#include "dragonfs.h"

#undef errno

/* Dirty hack, should investigate this further */
#define STACK_SIZE 0x10000

/* No idea what this is for */
char *__env[1] = { 0 };
char **environ = __env;
struct timeval;

/* Not otherwise supplied */
int errno;

/* bootchip set in n64sys.c */
extern int __bootcic;

int
chown( const char *path, uid_t owner, gid_t group )
{
    /* No permissions on files in DFS */
    errno = ENOSYS;
    return -1;
}

int
close( int fildes )
{
    return dfs_close( (uint32_t)fildes );
}

int
execve( char *name, char **argv, char **env )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

void
exit( int rc )
{
    /* Default stub just causes a divide by 0 exception.  */
    int x = rc / INT_MAX;
    x = 4 / x;

    /* Convince GCC that this function never returns.  */
    for( ;; );
}

int
fork( void )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

int
fstat( int fildes, struct stat *st )
{
     if( st == NULL )
     {
        errno = EINVAL;
        return -1;
     }
     else
     {
        st->st_dev = 0;
        st->st_ino = 0;
        st->st_mode = S_IFREG;
        st->st_nlink = 1;
        st->st_uid = 0;
        st->st_gid = 0;
        st->st_rdev = 0;
        st->st_size = dfs_size( (uint32_t)fildes );
        st->st_atime = 0;
        st->st_mtime = 0;
        st->st_ctime = 0;
        st->st_blksize = 0;
        st->st_blocks = 0;
        //st->st_attr = S_IAREAD | S_IAREAD;

        return 0;
     }
}

int
getpid( void )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}


int
gettimeofday( struct timeval *ptimeval, void *ptimezone )
{
    errno = ENOSYS;
    return -1;
}

int
isatty( int file )
{
    errno = ENOSYS;
    return 0;
}

int
kill( int pid, int sig )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

int
link( char *existing, char *new )
{
    /* No symbolic links in DFS */
    errno = ENOSYS;
    return -1;
}

int
lseek( int file, int ptr, int dir )
{
    /* DFS uses same seek constants */
    return dfs_seek( (uint32_t)file, ptr, dir );
}

int
open( char *file, int flags, int mode )
{
    return dfs_open( file );
}

int
read( int file, char *ptr, int len )
{
    return dfs_read( ptr, 1, len, (uint32_t)file );
}

int
readlink( const char *path, char *buf, size_t bufsize )
{
    /* No symlinks */
    errno = ENOSYS;
    return -1;
}

void *
sbrk( int incr )
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

int
stat( const char *file, struct stat *st )
{
    int fd = dfs_open( file );

    if( fd > 0 )
    {
        int ret = fstat( fd, st );
        dfs_close( fd );

        return ret;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

int
symlink( const char *path1, const char *path2 )
{
    /* No symlinks in DFS */
    errno = ENOSYS;
    return -1;
}

clock_t
times( struct tms *buf )
{
    errno = ENOSYS;
    return -1;
}

int
unlink( char *name )
{
    /* DFS is read only */
    errno = ENOSYS;
    return -1;
}

int
wait( int *status )
{
    /* No threads (yet??) */
    errno = ENOSYS;
    return -1;
}

int
write( int file, char *ptr, int len )
{
    /* DFS is read only */
    errno = ENOSYS;
    return -1;
}

