/**
 * @file dragonfs.h
 * @brief DragonFS
 * @ingroup dfs
 */
#ifndef __LIBDRAGON_DRAGONFS_H
#define __LIBDRAGON_DRAGONFS_H

/**
 * @defgroup dfs DragonFS
 * @ingroup asset
 * @brief DragonFS filesystem implementation and newlib hooks.
 *
 * DragonFS is a read only ROM filesystem for the N64.  It provides an interface
 * that homebrew developers can use to load resources from cartridge space that
 * were not available at compile time.  This can mean sprites or other game assets,
 * or the filesystem can be appended at a later time if the homebrew developer wishes
 * end users to be able to insert custom levels, music or other assets.  It is loosely
 * based off of FAT with consideration into application and limitations of the N64.
 *
 * The filesystem can be generated using 'mkdfs' which is included in the 'tools'
 * directory of libdragon.  Due to the read-only nature, DFS does not support empty
 * files or empty directories.  Attempting to create a filesystem with either of
 * these using 'mkdfs' will result in an error.  If a filesystem contains either empty
 * files or empty directories, the result of manipulating the filesystem is undefined.
 *
 * DragonFS does not support writing, renaming or symlinking of files.  It supports only
 * file and directory types.
 *
 * DFS files have a maximum size of 256 MiB.  Directories can have an unlimited
 * number of files in them.  Each token (separated by a / in the path) can be 243 characters
 * maximum.  Directories can be 100 levels deep at maximum.  There can be 4 files open
 * simultaneously.
 *
 * When DFS is initialized, it will register itself with newlib using 'rom:/' as a prefix.
 * Files can be accessed either with standard POSIX functions (open, fopen) using the 'rom:/'
 * prefix or the lower-level DFS API calls without prefix. In most cases, it is not necessary
 * to use the DFS API directly, given that the standard C functions are more comprehensive.
 * Files can be opened using both sets of API calls simultaneously as long as no more than
 * four files are open at any one time.
 * 
 * DragonFS does not support file compression; if you want to compress your assets,
 * use the asset API (#asset_load / #asset_fopen).
 * 
 * @{
 */

#ifdef N64

#include "ioctl.h"

#endif

/**
 * @brief Default filesystem location 
 *
 * The default value 0 instruct #dfs_init to search for the DFS image
 * within the rompak.
 */
#define DFS_DEFAULT_LOCATION    0

/**
 * @brief Maximum filename length
 *
 * This value is due to the direcory structure
 */
#define MAX_FILENAME_LEN    243

/**
 * @brief Maximum depth of directories supported
 */
#define MAX_DIRECTORY_DEPTH 100

/**
 * @brief Base ROM Address Request ioctl Command Code
 */
#define IODFS_GET_ROM_BASE _IO('D', 0)


/**
 * @name DragonFS Return values
 * @{
 */
/** @brief Success */
#define DFS_ESUCCESS        0
/** @brief Input parameters invalid */
#define DFS_EBADINPUT       -1
/** @brief File does not exist */
#define DFS_ENOFILE         -2
/** @brief Bad filesystem */
#define DFS_EBADFS          -3
/** @brief Too many open files */
#define DFS_ENFILE          -4
/** @brief Invalid file handle */
#define DFS_EBADHANDLE      -5
/** @} */

/** @cond */
// Deprecated naming
#define DFS_ENOMEM          -4
/** @endcond */


/**
 * @brief Macro to extract the file type from a DragonFS file flag
 *
 * @param[in] x
 *            File flags from DFS entry
 *
 * @return The file type flag
 */
#define FILETYPE(x)         ((x) & 3)

/**
 * @name DragonFS file type flags
 * @{
 */
/** @brief This is a file entry */
#define FLAGS_FILE          0x0
/** @brief This is a directory entry */
#define FLAGS_DIR           0x1
/** @brief This is the end of a directory list */
#define FLAGS_EOF           0x2
/** @} */

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the filesystem.
 *
 * Given a base offset where the filesystem should be found, this function will
 * initialize the filesystem to read from cartridge space.  This function will
 * also register DragonFS with newlib so that standard POSIX/C file operations
 * work with DragonFS, using the "rom:/" prefix".
 * 
 * The function needs to know where the DFS image is located within the cartridge
 * space. To simplify this, you can pass #DFS_DEFAULT_LOCATION which tells
 * #dfs_init to search for the DFS image by itself, using the rompak TOC (see
 * rompak_internal.h). Most users should use this option.
 * 
 * Otherwise, if the ROM cannot be built with a rompak TOC for some reason,
 * a virtual address should be passed. This is normally 0xB0000000 + the offset
 * used when building your ROM + the size of the header file used (typically 0x1000). 
 *
 * @param[in] base_fs_loc
 *            Virtual address in cartridge space at which to find the filesystem, or
 *            DFS_DEFAULT_LOCATION to automatically search for the filesystem in the
 *            cartridge (using the rompak).
 *
 * @return DFS_ESUCCESS on success or a negative error otherwise.
 */
int dfs_init(uint32_t base_fs_loc);


/**
 * @brief Open a file given a path
 *
 * Check if we have any free file handles, and if we do, try
 * to open the file specified.  Supports absolute and relative
 * paths
 *
 * @param[in] path
 *            Path of the file to open
 *
 * @return A valid file handle to reference the file by or a negative error on failure.
 */
int dfs_open(const char * const path);

/**
 * @brief Read data from a file
 * 
 * Note that no caching is performed: if you need to read small amounts
 * (eg: one byte at a time), consider using standard C API instead (fopen())
 * which performs internal buffering to avoid too much overhead.
 * 
 * @param[out] buf
 *             Buffer to read into
 * @param[in]  size
 *             Size of each element to read
 * @param[in]  count
 *             Number of elements to read
 * @param[in]  handle
 *             A valid file handle as returned from #dfs_open.
 *
 * @return The actual number of bytes read or a negative value on failure.
 */
int dfs_read(void * const buf, int size, int count, uint32_t handle);

/**
 * @brief Seek to an offset in the file
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 * @param[in] offset
 *            A byte offset from the origin to seek from.
 * @param[in] origin
 *            An offset to seek from.  Either `SEEK_SET`, `SEEK_CUR` or `SEEK_END`.
 *  
 * @return DFS_ESUCCESS on success or a negative value on error.
 */
int dfs_seek(uint32_t handle, int offset, int origin);

/**
 * @brief Return the current offset into a file
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return The current byte offset into a file or a negative error on failure.
 */
int dfs_tell(uint32_t handle);

/**
 * @brief Close an already open file handle.
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return DFS_ESUCCESS on success or a negative value on error.
 */
int dfs_close(uint32_t handle);

/**
 * @brief Return whether the end of file has been reached
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return 1 if the end of file is reached, 0 if not, and a negative value on error.
 */
int dfs_eof(uint32_t handle);

/**
 * @brief Return the file size of an open file
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return The file size in bytes or a negative value on failure.
 */
int dfs_size(uint32_t handle);

/**
 * @brief Return the physical address of a file (in ROM space)
 *
 * This function should be used for highly-specialized, high-performance
 * use cases. Using dfs_open / dfs_read is generally acceptable
 * performance-wise, and is easier to use rather than managing
 * direct access to PI space.
 * 
 * Direct access to ROM data must go through io_read or dma_read. Do not
 * dereference directly as the console might hang if the PI is busy.
 *
 * @param[in] path
 *            Name of the file
 *
 * @return A pointer to the physical address of the file body, or 0
 *         if the file was not found.
 * 
 */
uint32_t dfs_rom_addr(const char *path);

/**
 * @brief Convert DFS error code into an error string
 */
const char *dfs_strerror(int error);

__attribute__((deprecated("relative paths support is deprecated; please use only absolute paths when interacting with DragonFS")))
int dfs_chdir(const char * const path);

__attribute__((deprecated("use dir_findfirst instead")))
int dfs_dir_findfirst(const char * const path, char *buf);

__attribute__((deprecated("use dir_findnext instead")))
int dfs_dir_findnext(char *buf);

#ifdef __cplusplus
}
#endif

#endif
