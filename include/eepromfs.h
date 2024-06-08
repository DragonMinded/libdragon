/**
 * @file eepromfs.h
 * @brief EEPROM Filesystem
 * @ingroup eeprom
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

/**
 * @brief Initializes the EEPROM filesystem.
 * 
 * Creates a lookup table of file descriptors based on the configuration
 * and validates that the current EEPROM data is likely to be compatible
 * with the configured file descriptors.
 * 
 * If the configured filesystem does not fit in the available EEPROM blocks
 * on the cartridge, initialization will fail. Even if your total file size
 * fits in EEPROM, your filesystem may not fit due to overhead and padding.
 * Note that 1 block is reserved for the filesystem signature, and all files
 * must start on a block boundary.
 * 
 * You can mitigate this by ensuring that your files are aligned to the
 * 8-byte block size and minimizing wasted space with packed structs.
 * 
 * Each file will take up a minimum of 1 block, plus the filesystem itself
 * reserves the first block of EEPROM, so the entry count has a practical
 * limit of the number of available EEPROM blocks minus 1:
 * 
 * * 4k EEPROM: 63 files maximum.
 * * 16k EEPROM: 255 files maximum.
 *
 * @param[in] entries
 *            An array of file paths and sizes; see #eepfs_entry_t
 * @param[in] count
 *            The number of entries in the array
 *
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_init(const eepfs_entry_t * entries, size_t count);

/**
 * @brief De-initializes the EEPROM filesystem.
 * 
 * This cleans up the file lookup table.
 * 
 * You probably won't ever need to call this.
 * 
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_close(void);


/**
 * @brief Reads an entire file from the EEPROM filesystem.
 *
 * @param[in]  path
 *             Path of file in EEPROM filesystem to read from
 * @param[out] dest
 *             Buffer to read into
 * @param[in]  size
 *             Size of the destination buffer (in bytes)
 *
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_read(const char * path, void * dest, size_t size);

/**
 * @brief Writes an entire file to the EEPROM filesystem.
 * 
 * Each EEPROM block write takes approximately 15 milliseconds;
 * this operation may block for a while!
 *
 * @param[in] path
 *            Path of file in EEPROM filesystem to write to
 * @param[in] src
 *            Buffer of data to be written
 * @param[in]  size
 *             Size of the source buffer (in bytes)
 *
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_write(const char * path, const void * src, size_t size);

/**
 * @brief Erases a file in the EEPROM filesystem.
 * 
 * Note that "erasing" a file just means writing it full of zeroes.
 * All files in the filesystem must always exist at the size specified
 * during #eepfs_init
 * 
 * Each EEPROM block write takes approximately 15 milliseconds;
 * this operation may block for a while!
 * 
 * Be advised: this is a destructive operation that cannot be undone!
 * 
 * @retval EEPFS_ESUCCESS if successful
 * @retval EEPFS_ENOFILE if the path is not a valid file
 * @retval EEPFS_EBADINPUT if the path is NULL
 */
int eepfs_erase(const char * path);


/**
 * @brief Validates the first block of EEPROM.
 * 
 * There are no guarantees that the data in EEPROM actually matches
 * the expected layout of the filesystem. There are many reasons why
 * a mismatch can occur: EEPROM re-used from another game; a brand new
 * EEPROM that has never been initialized and contains garbage data;
 * the filesystem has changed between builds or version of software 
 * currently in development; EEPROM failing due to age or write limits.
 * 
 * To mitigate these scenarios, it is a good idea to validate that at
 * least the first block of EEPROM matches some known good value.
 * 
 * If the signature matches, the data in EEPROM is probably what the
 * filesystem expects. If not, the best move is to erase everything
 * and start from zero.
 * 
 * @see eepfs_generate_signature
 * @see #eepfs_wipe
 * 
 * @retval true if the signature in EEPROM matches the filesystem signature
 * @retval false if the signature in EEPROM does not match the filesystem signature
 */
bool eepfs_verify_signature(void);

/**
 * @brief Erases all blocks in EEPROM and sets a new signature.
 * 
 * This is useful when you want to erase all files in the filesystem.
 * 
 * Each EEPROM block write takes approximately 15 milliseconds;
 * this operation may block for a while:
 * 
 * * 4k EEPROM: 64 blocks * 15ms = 960ms!
 * * 16k EEPROM: 256 blocks * 15ms = 3840ms!
 * 
 * You may want to pause audio in advance of calling this.
 * 
 * Be advised: this is a destructive operation that cannot be undone!
 * 
 * @see #eepfs_verify_signature
 */
void eepfs_wipe(void);

#ifdef __cplusplus
}
#endif

#endif
