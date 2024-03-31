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
 * @brief Initialize the iQue NAND filesystem libraruy
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


#ifdef __cplusplus
}
#endif

#endif
