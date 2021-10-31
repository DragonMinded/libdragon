/**
 * @file eepromfs.h
 * @brief EEPROM Filesystem
 * @ingroup eepfs
 */
#ifndef __LIBDRAGON_EEPROMFS_H
#define __LIBDRAGON_EEPROMFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM filesystem configuration file entry
 * @see #eepfs_init
 */
typedef struct eepfs_entry_t
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
     * 
     * The filesytem does not support entries for directories,
     * nor does it support listing files in a given directory.
     */
    const char * path;
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
    size_t size;
} eepfs_entry_t;

int eepfs_init(const eepfs_entry_t * entries, size_t count);
int eepfs_close(void);

int eepfs_read(const char * path, void * dest, size_t size);
int eepfs_write(const char * path, const void * src, size_t size);
int eepfs_erase(const char * path);

bool eepfs_verify_signature(void);
void eepfs_wipe(void);

#ifdef __cplusplus
}
#endif

#endif
