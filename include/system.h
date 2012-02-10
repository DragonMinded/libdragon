/**
 * @file system.h
 * @brief newlib Interface Hooks
 * @ingroup system
 */
#ifndef __LIBDRAGON_SYSTEM_H
#define __LIBDRAGON_SYSTEM_H

/**
 * @addtogroup system
 * @{
 */

/** @brief Number of filesystems that can be attached to the system */
#define MAX_FILESYSTEMS     10
/** @brief Number of open handles that can be maintained at one time */
#define MAX_OPEN_HANDLES    100

#ifdef __cplusplus
extern "C" {
#endif

#include <dir.h>

/**
 * @brief Filesystem hook structure
 *
 * Filesystems that do not support one of more of the following methods
 * should provide null pointers instead of empty functions.  The newlib
 * hooks will set errno to ENOSYS and return a proper error to userspace.
 */
typedef struct
{
    /** @brief Function to call when performing an open command */
    void *(*open)( char *name, int flags );
    /** @brief Function to call when performing a fstat command */
    int (*fstat)( void *file, struct stat *st );
    /** @brief Function to call when performing an lseek command */
    int (*lseek)( void *file, int ptr, int dir );
    /** @brief Function to call when performing a read operation */
    int (*read)( void *file, uint8_t *ptr, int len );
    /** @brief Function to call when performing a write operation */
    int (*write)( void *file, uint8_t *ptr, int len );
    /** @brief Function to call when performing a close operation */
    int (*close)( void *file );
    /** @brief Function to call when performing an unlink operation */
    int (*unlink)( char *name );
    /** @brief Function to call when performing a findfirst operation */
    int (*findfirst)( char *path, dir_t *dir );
    /** @brief Function to call when performing a findnext operation */
    int (*findnext)( dir_t *dir );
} filesystem_t;

int attach_filesystem( const char * const prefix, filesystem_t *filesystem );
int detach_filesystem( const char * const prefix );

#ifdef __cplusplus
}
#endif

/** @} */

#endif
