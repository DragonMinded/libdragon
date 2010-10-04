#ifndef __LIBDRAGON_SYSTEM_H
#define __LIBDRAGON_SYSTEM_H

#define MAX_FILESYSTEMS     10
#define MAX_OPEN_HANDLES    100

#ifdef __cplusplus
extern "C" {
#endif

#include <dir.h>

typedef struct
{
    void *(*open)( char *name, int flags );
    int (*fstat)( void *file, struct stat *st );
    int (*lseek)( void *file, int ptr, int dir );
    int (*read)( void *file, uint8_t *ptr, int len );
    int (*write)( void *file, uint8_t *ptr, int len );
    int (*close)( void *file );
    int (*unlink)( char *name );
    int (*findfirst)( char *path, dir_t *dir );
    int (*findnext)( dir_t *dir );
} filesystem_t;

int attach_filesystem( const char * const prefix, filesystem_t *filesystem );
int detach_filesystem( const char * const prefix );

#ifdef __cplusplus
}
#endif

#endif
