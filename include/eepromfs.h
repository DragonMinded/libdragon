/**
 * @file eepromfs.h
 * @brief EEPROM Filesystem
 * @ingroup eepfs
 */
#ifndef __LIBDRAGON_EEPROMFS_H
#define __LIBDRAGON_EEPROMFS_H

#include <stddef.h>
#include <stdint.h>
#include "controller.h"

/**
 * @name EEPROM filesystem return values
 * @{
 */
/** @brief Success */
#define EEPFS_ESUCCESS   0
/** @brief Input parameters invalid */
#define EEPFS_EBADINPUT  -1
/** @brief File does not exist */
#define EEPFS_ENOFILE    -2
/** @brief Bad filesystem */
#define EEPFS_EBADFS     -3
/** @brief No memory for operation */
#define EEPFS_ENOMEM     -4
/** @brief Invalid file handle */
#define EEPFS_EBADHANDLE -5
/** @brief Filesystem already initialized */
#define EEPFS_ECONFLICT  -6
/** @} */

/** @brief The number of files in a fixed-size array of #eepfs_entry_t */
#define EEPFS_NUM_FILES(arr) (sizeof(arr) / sizeof(eepfs_entry_t))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM filesystem configuration file entry
 * @see #eepfs_config_t
 * @see #eepfs_init
 */
typedef struct eepfs_entry
{
    /**
     * @brief File path.
     * 
     * This cannot be NULL and must be a `\0`-terminated string.
     * 
     * There are no enforced limitations on directory structure
     * or file naming conventions except that all paths within
     * the filesystem must be unique and at least one character.
     * 
     * A leading '/' is optional and will be ignored if set.
     * Files accessed through stdio will always have the leading
     * '/', but files accessed through #eepfs_open can omit it.
     * 
     * The filesytem does not support entries for directories,
     * nor does it support listing files in a given directory.
     */
    const char * const path;
    /**
     * @brief File size in bytes.
     * 
     * In order to make the most use of limited EEPROM space,
     * files should be (but are not required to be) aligned to the
     * 8-byte block size. Unaligned bytes at the end of a file will
     * be wasted as padding; files must start on a block boundary.
     * 
     * The filesystem itself reserves the first block of EEPROM,
     * so your total filesystem size cannot exceed the available
     * EEPROM size minus 8 bytes (64 bits):
     * 
     * * 4k EEPROM: 512 - 8 = 504 bytes (63 blocks) free.
     * * 16k EEPROM: 2048 - 8 = 2040 bytes (255 blocks) free.
     */
    const size_t size;
} eepfs_entry_t;

/**
 * @brief EEPROM filesystem configuration options
 * @see #eepfs_init
 */
typedef struct eepfs_config
{
    /** @brief Array of file entries; see #eepfs_entry_t */
    const eepfs_entry_t * const files;
    /**
     * @brief Number of file entries in the array
     * 
     * Each file will take up a minimum of 1 block, plus the
     * filesystem itself reserves the first block of EEPROM,
     * so the number of files in your filesystem has a practical
     * limit of the number of available EEPROM blocks minus 1:
     * 
     * * 4k EEPROM: 63 files maximum.
     * * 16k EEPROM: 255 files maximum.
     * 
     * In practice, this limitation is unlikely to matter since
     * most files will span multiple blocks and run up against
     * the storage capacity limits of EEPROM first.
     */
    const size_t num_files;
    /**
     * @brief POSIX standard input and output filesystem prefix.
     * 
     * This can be NULL if you only want to use `eepfs_*` functions,
     * but setting this (e.g. "eeprom:/") allows POSIX file operations
     * to access the EEPROM filesystem with stdio library functions.
     */
    const char * const stdio_prefix;
    /**
     * @brief Whether to validate EEPROM data during initialization.
     * 
     * If this is false and the signature does not match,
     * all EEPROM data will be erased!
     * 
     * @see #eepfs_check_signature
     */
    const bool skip_signature_check;
} eepfs_config_t;

int eepfs_init(const eepfs_config_t config);
int eepfs_deinit();

int eepfs_read(const char * const path, void * const dest);
int eepfs_write(const char * const path, const void * const src);
int eepfs_erase(const char * path);

void eepfs_wipe();
int eepfs_check_signature();

#ifdef __cplusplus
}
#endif

#endif
