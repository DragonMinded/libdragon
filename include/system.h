/**
 * @file system.h
 * @brief newlib Interface Hooks
 * @ingroup system
 */
#ifndef __LIBDRAGON_SYSTEM_H
#define __LIBDRAGON_SYSTEM_H

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
 * of free space for use with malloc calls. The size of the available heap is
 * computed using the memory size computed by the boot code (IPL3), and available
 * via #get_memory_size(), which is normally either 4 MiB or 8 MiB if the expansion
 * pak is available.
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

/** @brief Number of filesystems that can be attached to the system */
#define MAX_FILESYSTEMS     10
/** @brief Number of open handles that can be maintained at one time */
#define MAX_OPEN_HANDLES    4096

#ifdef __cplusplus
extern "C" {
#endif

#include <dir.h>
#include <sys/stat.h>

/**
 * @brief Filesystem hook structure
 *
 * Filesystems that do not support one of more of the following methods
 * should provide null pointers instead of empty functions.  The newlib
 * hooks will set errno to ENOSYS and return a proper error to userspace.
 * 
 * All filesystems functions must set errno in case of error, to report
 * the proper error to userspace.
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
     *         or NULL on error (and errno is set).
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
     * @return 0 on success or a negative value on error (and errno is set).
     */
    int (*fstat)( void *file, struct stat *st );
    /** 
     * @brief Function to call when performing a stat command
     *
     * @param[in]  file
     *             Full path of the file to be examined, relative to the root
     *             of the filesystem.
     * @param[out] st
     *             Stat structure to populate with file statistics
     *
     * @return 0 on success or a negative value on error (and errno is set).
     */
    int (*stat)( char *name, struct stat *st );
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
     * @return The absolute offset in bytes after the seek or a negative value on failure
     *         (and errno is set).
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
     * @return The actual number of bytes read into ptr or a negative value on failure
     *         (and errno is set).
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
     * @return The actual number of bytes written or a negative value on failure
     *          (and errno is set).
     */
    int (*write)( void *file, uint8_t *ptr, int len );
    /** 
     * @brief Function to call when performing a close operation
     *
     * @param[in] file
     *            Arbitrary file handle returned by #filesystem_t::open
     *
     * @return 0 on success or a negative value on failure (and errno is set).
     */
    int (*close)( void *file );
    /** 
     * @brief Function to call when performing an unlink operation
     *
     * @param[in] name
     *            Full path of the file to be opened, relative to the root
     *            of the filesystem.
     *
     * @return 0 on success or a negative value on failure (and errno is set).
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
     * @return 0 on successful lookup, -1 if the directory existed and is empty,
     *         or a different negative value on error (in which case, errno will be set).
     */
    int (*findfirst)( char *path, dir_t *dir );
    /** 
     * @brief Function to call when performing a findnext operation
     *
     * @param[out] dir
     *             Directory structure to place information on the next file in the
     *             directory.
     *
     * @return 0 on successful lookup, -1 if the directory existed and is empty,
     *         or a different negative value on error (in which case, errno will be set).
     */
    int (*findnext)( dir_t *dir );
    /**
     * @brief Truncate a file to a specified length
     * 
     * @param file    Arbitrary file handle returned by #filesystem_t::open
     * @param length  New length of the file
     * 
     * @return 0 on success or a negative value on failure (and errno is set)
     */
    int (*ftruncate)( void *file, int length );
    /**
     * @brief Create a directory
     * 
     * @param[in] path
     *            Full path of the directory to create, relative to the root of the filesystem.
     * @param[in] mode
     *            Directory permissions
     * 
     * @return 0 on success or a negative value on failure (errno must be set)
     */
    int (*mkdir)( char *path, mode_t mode );
    /**
     * @brief Perform IO Control Request
     *
     * @param[in] file
     *            File handle
     * @param[in] cmd
     *            Request ioctl command code 
     * @param[in] argp
     *            Pointer to a request-specific data structure
     *
     * @return 0 on success or a negative value on failure (errno must be set)
     */
    int (*ioctl)(void *file, unsigned long cmd, void *argp);
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
int attach_filesystem( const char * const prefix, filesystem_t *filesystem );

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
int detach_filesystem( const char * const prefix );


/**
 * @brief Hook into stdio for STDIN, STDOUT and STDERR callbacks
 *
 * @param[in] stdio_calls
 *            Pointer to structure containing callbacks for stdio functions
 *
 * @return 0 on successful hook or a negative value on failure.
 */
int hook_stdio_calls( stdio_t *stdio_calls );

/**
 * @brief Unhook from stdio
 *
 * @param[in] stdio_calls
 *            Pointer to structure containing callbacks for stdio functions
 *
 * @return 0 on successful hook or a negative value on failure.
 */
int unhook_stdio_calls( stdio_t *stdio_calls );


/**
 * @brief Hook into gettimeofday with a current time callback.
 *
 * @param[in] time_fn
 *            Pointer to callback for the current time function
 *
 * @return 0 if successful or a negative value on failure.
 */
int hook_time_call( time_t (*time_fn)( void ) );

/**
 * @brief Unhook from gettimeofday current time callback.
 *
 * @param[in] time_fn
 *            Pointer to callback for the current time function
 *
 * @return 0 if successful or a negative value on failure.
 */
int unhook_time_call( time_t (*time_fn)( void ) );

#ifdef __cplusplus
}
#endif

/** @} */

#endif
