#ifndef LIBDRAGON_BBFS_H
#define LIBDRAGON_BBFS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Filesystem errors
 */
enum bbfs_error_e {
    BBFS_ERR_SUPERBLOCK = -1,     ///< Cannot mount the filesystem (superblock missing or corrupted)
};

/**
 * @brief Initialize the iQue NAND filesystem library
 * 
 * This function mounts the filesystem and initializes the library.
 * After calling this function, you can access the BBFS files using
 * the standard C file I/O functions with the "bbfs:/" prefix.
 * 
 * @return 0 on success, negative value on failure (see #bbfs_error_e)
 */
int bbfs_init(void);

/**
 * @brief Return the indices of NAND blocks that contain the file data
 * 
 * The function lookups the specified file on the filesystem. If found, it
 * returns a list of block indices that contain the file data. The list is
 * dynamically allocated and must be freed by the caller.
 * 
 * The list of block indices is terminated by a -1 value.
 * 
 * @param filename      Filename (*without* filesystem prefix)
 * @return int16_t*     Allocated array of block indices, or NULL 
 *                      if file doesn't exist or the filesystem is corrupted
 */
int16_t* bbfs_get_file_blocks(const char *filename);

/**
 * @brief Verify the integrity of the filesystem, and optionally try to fix it
 * 
 * This function checks the integrity of the filesystem. If the filesystem is
 * corrupted, it can optionally try to fix it. An error reporting function
 * can be provided to log the errors.
 * 
 * @param fix_errors    If true, try to fix the errors, otherwise just check
 * @return number of errors found in the filesystem
 */
int bbfs_fsck(bool fix_errors);


#ifdef __cplusplus
}
#endif

#endif
