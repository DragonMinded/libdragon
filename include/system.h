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
    /** 
     * @brief Function to call when performing an open command
     *
     * @param[in] name
     *            Full path of the file to be opened, relative to the root
     *            of the filesystem.
     * @param[in] flags
     *            Open flags, such as binary, append, etc.  Follows POSIX flags.
     *
     * @return A pointer to an arbitrary file handle assigned by the filesystem code 
     *         or NULL on error.
     */
    void *(*open)( char *name, int flags );
    /** 
     * @brief Function to call when performing a fstat command
     *
     * @param[in]  file
     *             Arbitrary file handle returned by #filesystem_t::open
     * @param[out] st
     *             Stat structure to populate with file statistics
     *
     * @return 0 on success or a negative value on error.
     */
    int (*fstat)( void *file, struct stat *st );
    /** 
     * @brief Function to call when performing an lseek command
     *
     * @param[in] file
     *            Arbitrary file handle returned by #filesystem_t::open
     * @param[in] ptr
     *            An offset from dir to seek.
     * @param[in] dir
     *            A direction to see, either SEEK_SET, SEEK_CUR or SEEK_END
     *
     * @return The absolute offset in bytes after the seek.
     */
    int (*lseek)( void *file, int ptr, int dir );
    /** 
     * @brief Function to call when performing a read operation
     *
     * @param[in]  file
     *             Arbitrary file handle returned by #filesystem_t::open
     * @param[out] ptr
     *             Buffer to place data read into
     * @param[in]  len
     *             Length of data that should be read into ptr
     *
     * @return The actual number of bytes read into ptr or a negative value on failure.
     */
    int (*read)( void *file, uint8_t *ptr, int len );
    /** @brief Function to call when performing a write operation
     *
     * @param[in]  file
     *             Arbitrary file handle returned by #filesystem_t::open
     * @param[out] ptr
     *             Buffer to grab the data to be written
     * @param[in]  len
     *             Length of data that should be written out of ptr
     *
     * @return The actual number of bytes written or a negative value on failure.
     */
    int (*write)( void *file, uint8_t *ptr, int len );
    /** 
     * @brief Function to call when performing a close operation
     *
     * @param[in] file
     *            Arbitrary file handle returned by #filesystem_t::open
     *
     * @return 0 on success or a negative value on failure.
     */
    int (*close)( void *file );
    /** 
     * @brief Function to call when performing an unlink operation
     *
     * @param[in] name
     *            Full path of the file to be opened, relative to the root
     *            of the filesystem.
     *
     * @return 0 on success or a negative value on failure.
     */
    int (*unlink)( char *name );
    /** 
     * @brief Function to call when performing a findfirst operation
     *
     * @param[in]  path
     *             Full path of the directory to list files from, relative to the
     *             root of the filesystem.
     * @param[out] dir
     *             Directory structure to place information on the first file in the
     *             directory.
     *
     * @return 0 on successful lookup or a negative value on failure or empty directory.
     */
    int (*findfirst)( char *path, dir_t *dir );
    /** 
     * @brief Function to call when performing a findnext operation
     *
     * @param[out] dir
     *             Directory structure to place information on the next file in the
     *             directory.
     *
     * @return 0 on successful lookup or a negative value on failure or empty directory.
     */
    int (*findnext)( dir_t *dir );
} filesystem_t;

/**
 * @brief Standard I/O hook structure
 *
 * This structure provides optional callback hooks for code wishing to
 * respond to reads from STDIN or writes to STDOUT or STDERR.  Any function
 * that code does not wish to handle should be left as a null pointer.
 */
typedef struct
{
    /** 
     * @brief Function to call when performing a STDIN read
     *
     * @param[out] data
     *             Pointer to data buffer to place the read data
     * @param[in]  len
     *             Length of data in bytes expected to be read
     * 
     * @return Actual number of bytes read into data, not to exceed the original length.
     */
    int (*stdin_read)( char *data, unsigned int len );
    /** 
     * @brief Function to call when performing a STDOUT write
     *
     * @param[in] data
     *            Pointer to data buffer containing the data to write
     * @param[in] len
     *            Length of data in bytes expected to be written
     *
     * @return Actual number of bytes written from data, not to exceed the original length.
     */
    int (*stdout_write)( char *data, unsigned int len );
    /** 
     * @brief Function to call when performing a STDERR write
     *
     * @param[in] data
     *            Pointer to data buffer containing the data to write
     * @param[in] len
     *            Length of data in bytes expected to be written
     *
     * @return Actual number of bytes written from data, not to exceed the original length.
     */
    int (*stderr_write)( char *data, unsigned int len );
} stdio_t;

int attach_filesystem( const char * const prefix, filesystem_t *filesystem );
int detach_filesystem( const char * const prefix );

int hook_stdio_calls( stdio_t *stdio_calls );
int unhook_stdio_calls();

#ifdef __cplusplus
}
#endif

/** @} */

#endif
