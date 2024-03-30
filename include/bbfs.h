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

#ifdef __cplusplus
}
#endif

#endif
