/**
 * @file eepromfs.c
 * @brief EEPROM Filesystem
 * @ingroup eeprom
 */
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include "libdragon.h"
#include "system.h"
#include "utils.h"

/**
 * @brief EEPROM Filesystem file descriptor.
 * 
 * The filesystem uses these to map distinct files onto
 * a range of EEPROM blocks.
 * 
 * Only one descriptor exists per file, so opening a
 * file multiple times will return the same descriptor.
 * 
 * Files must start on a block boundary, and will consume
 * one block for every 8 bytes. Files that are not aligned
 * with the block size will result in wasted EEPROM space.
 * 
 * Every file in the filesystem will always exist and
 * cannot be resized. Erasing a file just means to fill it
 * with zeroes.
 */
typedef struct eepfs_file_t
{
    /** @brief File path */
    const char * const path;
    /** @brief Files must start on a block boundary */
    const size_t start_block;
    /** @brief Size of the file (in bytes) */
    const size_t num_bytes;
} eepfs_file_t;

/**
 * @brief The number of file entries in the filesystem.
 * 
 * 0 is assumed to mean that the filesystem has not been
 * initialized, since there will always be at least one
 * file in an initialized filesystem (the signature file).
 */
static size_t eepfs_files_count = 0;

/**
 * @brief The dynamically-allocated array of file entries.
 * 
 * A `NULL` value means the filesystem is not initialized.
 * 
 * Allocated and assigned by #eepfs_init;
 * freed and set back to NULL by #eepfs_close.
 */
static eepfs_file_t * eepfs_files = NULL;

/**
 * @brief A CRC-16/CCITT checksum of the declared filesystem.
 * 
 * This is set during #eepfs_init and is used as part of the
 * filesystem signature block in #eepfs_generate_signature.
 */
static uint16_t eepfs_files_checksum = 0;

/**
 * @brief Calculates a CRC-16 checksum from an array of bytes.
 * 
 * CRC-16/CCITT-FALSE, CRC-16/IBM-3740:
 * poly=0x1021, init=0xFFFF, xorout=0x0000
 * 
 * @see https://stackoverflow.com/a/23726131
 */
static uint16_t calculate_crc16(const uint8_t * data, size_t len)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while ( len-- )
    {
        x = crc >> 8 ^ *(data++);
        x ^= x>>4;
        crc = (
            (crc << 8) ^ 
            ((uint16_t)(x << 12)) ^ 
            ((uint16_t)(x << 5)) ^ 
            ((uint16_t)x)
        );
    }

    return crc;
}

/**
 * @brief Generates a signature based on the filesystem configuration.
 * 
 * The signature is a combination of a "magic value", the
 * number of files, and the total size of the files in the
 * filesystem.
 * 
 * This all fits into a single EEPROM block (8 bytes).
 * 
 * The signature is a compromise between minimizing overhead
 * of the filesystem and providing a best-effort guarantee
 * that the data in EEPROM matches the file structure that
 * the filesystem expects.
 * 
 * @return An EEPROM block-sized value containing the signature for the filesystem
 */
static uint64_t eepfs_generate_signature()
{
    /* Sanity check */
    if ( eepfs_files_count == 0 || eepfs_files == NULL )
    {
        /* Cannot generate a signature if the 
           filesystem has not been initialized! */
        return 0;
    }

    /* Sum up the total bytes used by all files.
       At most a 16k EEPROM can hold 2048 bytes. 
       Skip the first file (the signature block). */
    uint16_t total_bytes = 0;
    for ( size_t i = 1; i < eepfs_files_count; ++i )
    {
        total_bytes += eepfs_files[i].num_bytes;
    }

    /* Craft an 8-byte block and return it as a value */
    uint64_t signature;
    uint8_t * const sig_bytes = (uint8_t *)&signature;
    sig_bytes[0] = 'e';
    sig_bytes[1] = 'e';
    sig_bytes[2] = 'p';
    sig_bytes[3] = eepfs_files_count - 1;
    sig_bytes[4] = total_bytes >> 8;
    sig_bytes[5] = total_bytes & 0xFF;
    sig_bytes[6] = eepfs_files_checksum >> 8;
    sig_bytes[7] = eepfs_files_checksum & 0xFF;
    return signature;
}

/**
 * @brief Finds a file descriptor's handle given a path.
 * 
 * A leading '/' in the path is optional for your convenience.
 *
 * @param[in] path
 *            Absolute path of the file to open
 *
 * @return A file handle or a negative value on failure.
 */
static int eepfs_find_handle(const char * path)
{
    /* Sanity check */
    if ( path == NULL )
    {
        return EEPFS_EBADINPUT;
    }

    /* Ignore the leading '/' on the path (if present) */
    const char * normalized_path = path + (path[0] == '/');

    /* Check if the path matches an entry in the #eepfs_files array */
    for ( size_t i = 1; i < eepfs_files_count; ++i )
    {
        if ( strcmp(normalized_path, eepfs_files[i].path) == 0 )
        {
            /* A file exists matching that path */
            return i;
        }
    }

    /* File does not exist */
    return EEPFS_ENOFILE;
}

/**
 * @brief Resolves a file handle to a file descriptor.
 * 
 * File descriptors can be used for reading or writing.
 * 
 * Getting a file repeatedly will always return the same pointer.
 * 
 * Negative handles indicate an upstream error occurred.
 * The zero value will never be used as a file handle, even
 * though it does map to an entry in the `eepfs_files` table.
 * The signature entry is not intended to be used as a file.
 * 
 * @param[in] handle
 *            The file handle of an entry in the `eepfs_files` table
 * 
 * @return A pointer to the file descriptor or NULL if the handle is invalid
 */
static const eepfs_file_t * eepfs_get_file(int handle)
{
    /* Check if the handle appears to be valid */
    if ( handle > 0 && handle < eepfs_files_count )
    {
        return &eepfs_files[handle];
    }

    return NULL;
}

int eepfs_init(const eepfs_entry_t * entries, size_t count)
{
    /* Check if EEPROM FS has already been initialized */
    if ( eepfs_files_count != 0 || eepfs_files != NULL )
    {
        return EEPFS_ECONFLICT;
    }

    /* Sanity check the arguments */
    if ( count == 0 || entries == NULL )
    {
        return EEPFS_EBADINPUT;
    }

    /* Add an extra file for the "signature file" */
    eepfs_files_count = count + 1;

    /* Allocate the lookup table of file descriptors */
    const size_t files_size = sizeof(eepfs_files[0]) * eepfs_files_count;
    eepfs_files = (eepfs_file_t *)malloc(files_size);

    if ( eepfs_files == NULL )
    {
        return EEPFS_ENOMEM;
    }

    /* The first file should always be the "signature file" */
    const eepfs_file_t signature_file = { NULL, 0, 8 };
    memcpy(&eepfs_files[0], &signature_file, sizeof(signature_file));

    const char * file_path;
    size_t file_size;
    size_t total_blocks = 1;

    /* Configure a file descriptor for each entry */
    for ( size_t i = 1; i < eepfs_files_count; ++i )
    {
        file_path = entries[i - 1].path;
        file_size = entries[i - 1].size;

        /* Sanity check the file details */
        if ( file_path == NULL || file_size == 0 )
        {
            eepfs_close();
            return EEPFS_EBADINPUT;
        }

        /* Strip the leading '/' on paths for consistency */
        file_path += ( file_path[0] == '/' );

        /* Create a file descriptor and copy it into the table */
        const eepfs_file_t entry_file = {
            file_path,
            total_blocks,
            file_size,
        };
        memcpy(&eepfs_files[i], &entry_file, sizeof(entry_file));

        /* Files must start on a block boundary */
        total_blocks += DIVIDE_CEIL(file_size, EEPROM_BLOCK_SIZE);
    }

    /* Ensure the filesystem will actually fit in available EEPROM */
    if ( total_blocks > eeprom_total_blocks() )
    {
        eepfs_close();
        return EEPFS_EBADFS;
    }

    /* Calculate and store the CRC-16 checksum for the declared entries */
    const size_t entries_size = sizeof(eepfs_entry_t) * count;
    eepfs_files_checksum = calculate_crc16((void *)entries, entries_size);

    return EEPFS_ESUCCESS;
}

int eepfs_close(void)
{
    /* If eepfs was not initialized, don't do anything. */
    if ( eepfs_files == NULL || eepfs_files_count == 0 )
    {
        return EEPFS_EBADFS;
    }

    /* Clear the file descriptor table */
    free(eepfs_files);
    eepfs_files = NULL;
    eepfs_files_checksum = 0;
    eepfs_files_count = 0;

    return EEPFS_ESUCCESS;
}

int eepfs_read(const char * path, void * dest, size_t size)
{
    const int handle = eepfs_find_handle(path);
    const eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return EEPFS_ENOFILE;
    }
    if ( dest == NULL || file->num_bytes != size ) 
    {
        /* Unusable destination buffer */
        return EEPFS_EBADINPUT;
    }

    const size_t start_bytes = file->start_block * EEPROM_BLOCK_SIZE;
    eeprom_read_bytes(dest, start_bytes, file->num_bytes);

    return EEPFS_ESUCCESS;
}

int eepfs_write(const char * path, const void * src, size_t size)
{
    const int handle = eepfs_find_handle(path);
    const eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return EEPFS_ENOFILE;
    }
    if ( src == NULL || file->num_bytes != size ) 
    {
        /* Unusable source buffer */
        return EEPFS_EBADINPUT;
    }

    const size_t start_bytes = file->start_block * EEPROM_BLOCK_SIZE;
    eeprom_write_bytes(src, start_bytes, file->num_bytes);

    return EEPFS_ESUCCESS;
}

int eepfs_erase(const char * path)
{
    const int handle = eepfs_find_handle(path);
    const eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return EEPFS_ENOFILE;
    }

    const size_t num_blocks = DIVIDE_CEIL(file->num_bytes, EEPROM_BLOCK_SIZE);
    size_t current_block = file->start_block;

    /* eeprom_buf is initialized to all zeroes */
    const uint8_t eeprom_buf[EEPROM_BLOCK_SIZE] = {0};

    /* Write the blocks in with zeroes */
    while ( current_block < file->start_block + num_blocks )
    {
        eeprom_write(current_block++, eeprom_buf);
    }

    return EEPFS_ESUCCESS;
}

bool eepfs_verify_signature(void)
{
    /* Generate the expected signature for the filesystem */
    const uint64_t signature = eepfs_generate_signature();

    /* Read the signature block out of EEPROM */
    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE];
    eeprom_read(0, eeprom_buf);

    /* If the signatures don't match, we can be pretty sure
       that the data in EEPROM is not the expected filesystem */
    return memcmp(eeprom_buf, (uint8_t *)&signature, EEPROM_BLOCK_SIZE) == 0;
}

void eepfs_wipe(void)
{
    /* Write the filesystem signature into the first block */
    const uint64_t signature = eepfs_generate_signature();
    eeprom_write(0, (uint8_t *)&signature);

    /* eeprom_buf is initialized to all zeroes */
    const uint8_t eeprom_buf[EEPROM_BLOCK_SIZE] = {0};

    /* Write the rest of the blocks in with zeroes */
    const size_t eeprom_capacity = eeprom_total_blocks();
    size_t current_block = 1;

    while ( current_block < eeprom_capacity )
    {
        eeprom_write(current_block++, eeprom_buf);
    }
}

