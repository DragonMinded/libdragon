/**
 * @file dir.h
 * @brief Directory handling
 * @ingroup system
 */
#ifndef __LIBDRAGON_DIR_H
#define __LIBDRAGON_DIR_H

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
    /** @brief The name of the directory entry */
    char d_name[256];
    /** @brief The type of the directory entry.  See #DT_REG and #DT_DIR. */
    int d_type;
} dir_t;

/** @} */

int dir_findfirst( const char * const path, dir_t *dir );
int dir_findnext( const char * const path, dir_t *dir );

#ifdef __cplusplus
}
#endif

#endif
