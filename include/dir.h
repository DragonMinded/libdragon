/**
 * @file dir.h
 * @brief Directory handling
 * @ingroup system
 */
#ifndef __LIBDRAGON_DIR_H
#define __LIBDRAGON_DIR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @addtogroup system
 * @{
 */

/**
 * @name Directory entry type definitions
 * @{
 */
/** @brief Regular file */
#define DT_REG 1
/** @brief Directory */
#define DT_DIR 2
/** @} */

/**
 * @brief Directory entry structure
 */
typedef struct
{
    /** @brief The name of the directory entry (relative to the directory path) */
    char d_name[256];
    /** @brief The type of the directory entry.  See #DT_REG and #DT_DIR. */
    int d_type;
    /** @brief Size of the file.
     * 
     * This value is well defined for files. For directories, the value
     * is filesystem-dependent.
     * 
     * If negative, the filesystem does not report the size during directory
     * walking.
     */
    int64_t d_size;
    /** @brief Opaque cookie used to continue walking. */
    uint32_t d_cookie;
} dir_t;

/** @} */

/**
 * @brief Find the first file in a directory
 *
 * This function should be called to start enumerating a directory or whenever
 * a directory enumeration should be restarted.
 *
 * @param[in]  path
 *             Path to the directory structure
 * @param[out] dir
 *             Directory entry structure to populate with first entry
 *
 * @return 0 on successful lookup, -1 if the directory existed and is empty,
 *         or a different negative value on error (in which case, errno will be set).
 */
int dir_findfirst( const char * const path, dir_t *dir );

/**
 * @brief Find the next file in a directory
 *
 * After finding the first file in a directory using #dir_findfirst, call this to retrieve
 * the rest of the directory entries.  Call this repeatedly until a negative error is returned
 * signifying that there are no more directory entries in the directory.
 *
 * @param[in]  path
 *             Path to the directory structure
 * @param[out] dir
 *             Directory entry structure to populate with next entry
 *
 * @return 0 on successful lookup, -1 if there are no more files in the directory,
 *         or a different negative value on error (in which case, errno will be set).
 */
int dir_findnext( const char * const path, dir_t *dir );

#ifdef __cplusplus
}
#endif

#endif
