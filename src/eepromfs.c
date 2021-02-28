/**
 * @file eepromfs.c
 * @brief EEPROM Filesystem
 * @ingroup eepfs
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <sys/stat.h>
#include "libdragon.h"
#include "system.h"

#define divide_ceil( x, y ) ((x / y) + (x % y != 0))

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
 * 
 * @see #eepfs_open
 */
typedef struct
{
    /** @brief File path */
    const char * const path;
    /** @brief Files must start on a block boundary */
    const size_t start_block;
    /** @brief Size of the file (in blocks) */
    const size_t num_blocks;
    /** @brief Current read/write position (in bytes) */
    size_t cursor;
    /** @brief Maxiumum read/write position (in bytes) */
    const size_t max_cursor;
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
static eepfs_file_t *eepfs_files = NULL;

/**
 * @brief A CRC-16/CCITT checksum of the declared filesystem.
 * 
 * This is set during #eepfs_init and is used as part of the
 * filesystem signature block in #eepfs_generate_signature.
 */
static uint16_t eepfs_files_checksum = 0;

/**
 * @brief The filesystem prefix used by stdio functions.
 * 
 * Assigned by #eepfs_attach and unassigned by #eepfs_detach.
 */
static const char * eepfs_stdio_prefix = NULL;

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

    while (len--)
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
static const uint64_t eepfs_generate_signature()
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
        total_bytes += eepfs_files[i].max_cursor;
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
 * @see #eepfs_generate_signature
 * @see #eepfs_wipe
 * 
 * @retval true if the first block of EEPROM matches the filesystem signature
 * @retval false if the first block of EEPROM does not match the filesystem signature
 */
static bool eepfs_verify_signature(void)
{
    const uint64_t signature = eepfs_generate_signature();
    const uint8_t * const sig_bytes = (uint8_t *)&signature;

    /* Read the signature block out of EEPROM */
    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE];
    eeprom_read(0, eeprom_buf);

    /* If the signatures don't match, we can be pretty sure
       that the data in EEPROM is not the expected filesystem */
    return memcmp(eeprom_buf, sig_bytes, EEPROM_BLOCK_SIZE) == 0;
}

/**
 * @brief Finds a file descriptor's handle given a path.
 * 
 * A leading '/' in the path is optional and is supported to
 * allow for consistency with stdio functions.
 *
 * @param[in] path
 *            Absolute path of the file to open (without stdio prefix)
 *
 * @return A file handle or a negative value on failure.
 */
static int eepfs_find_handle(const char * const path)
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
 * This means that the cursor is shared across all usage of the file.
 * Be sure to reset the cursor when you are done with it!
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
static eepfs_file_t * const eepfs_get_file(const int handle)
{
    /* Check if the handle appears to be valid */
    if ( handle > 0 && handle < eepfs_files_count )
    {
        return &eepfs_files[handle];
    }

    return NULL;
}

/**
 * @brief Reads a specific amount of data from a file starting from the cursor.
 *
 * @param[in]  path
 *             Path of file in EEPROM filesystem to read from
 * @param[out] dest
 *             Buffer to read into
 * @param[in]  len
 *             Number of bytes to read
 *
 * @return The actual number of bytes read or a negative value on failure.
 */
static int eepfs_cursor_read(eepfs_file_t * const file, void * const dest, const size_t len)
{
    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    if ( dest == NULL ) 
    {
        return EEPFS_EBADINPUT;
    }

    size_t cursor = file->cursor;
    size_t bytes_to_read = len;

    /* Bounds check to make sure it doesn't read past the end */
    if ( cursor + bytes_to_read >= file->max_cursor )
    {
        /* It will, let's shorten it */
        bytes_to_read = file->max_cursor - cursor;
    }

    if ( bytes_to_read <= 0 )
    {
        /* No bytes to read; bail early */
        return 0;
    }

    /* Calculate the starting point to read out of EEPROM from */
    size_t current_block = file->start_block + (cursor / EEPROM_BLOCK_SIZE);
    size_t block_byte_offset = cursor % EEPROM_BLOCK_SIZE;
    int bytes_read = 0;

    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE];
    uint8_t * ptr = dest;

    do {
        eeprom_read(current_block, eeprom_buf);
        /* Fill the buffer with the data from the current block */
        while ( bytes_to_read > 0 && block_byte_offset < EEPROM_BLOCK_SIZE )
        {
            *(ptr++) = eeprom_buf[block_byte_offset];
            bytes_read++;
            bytes_to_read--;
            block_byte_offset++;
        }
        /* Move on to the next block */
        block_byte_offset = 0;
        current_block++;
    } while ( bytes_to_read > 0 );

    file->cursor = cursor + bytes_read;

    return bytes_read;
}

/**
 * @brief Writes a specific amount of data to a file starting from the cursor.
 *
 * @param[in] path
 *            Path of file in EEPROM filesystem to write to
 * @param[in] src
 *            Buffer containing the data to write
 * @param[in] len
 *            Number of bytes to write
 *
 * @return The actual number of bytes written or a negative value on failure.
 */
static int eepfs_cursor_write(eepfs_file_t * const file, const void * const src, const size_t len)
{
    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    if ( src == NULL ) 
    {
        return EEPFS_EBADINPUT;
    }

    size_t cursor = file->cursor;
    size_t bytes_to_write = len;

    /* Bounds check to make sure it doesn't write past the end */
    if ( cursor + bytes_to_write >= file->max_cursor )
    {
        /* It will, let's shorten it */
        bytes_to_write = file->max_cursor - cursor;
    }

    if ( bytes_to_write <= 0 )
    {
        /* No bytes to write; bail early */
        return 0;
    }

    /* Calculate the starting point to write to EEPROM from */
    size_t current_block = file->start_block + (cursor / EEPROM_BLOCK_SIZE);
    size_t block_byte_offset = cursor % EEPROM_BLOCK_SIZE;
    int bytes_written = 0;

    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE];
    const uint8_t * ptr = src;

    do {
        /* Only read in the current block if we need to preserve data from it */
        if ( bytes_to_write < EEPROM_BLOCK_SIZE || block_byte_offset != 0 )
        {
            eeprom_read(current_block, eeprom_buf);
        }
        /* Fill the current block with the data to write */
        while ( bytes_to_write > 0 && block_byte_offset < EEPROM_BLOCK_SIZE )
        {
            eeprom_buf[block_byte_offset] = *(ptr++);
            bytes_written++;
            bytes_to_write--;
            block_byte_offset++;
        }
        /* Write the block and move on to the next one */
        eeprom_write(current_block, eeprom_buf);
        block_byte_offset = 0;
        current_block++;
    } while ( bytes_to_write > 0 );

    file->cursor = cursor + bytes_written;

    return bytes_written;
}

/**
 * @brief Opens a file given a path.
 *
 * @param[in] path
 *            Absolute path of the file to open (without stdio prefix)
 * @param[in] flags
 *            POSIX file flags (ignored)
 *
 * @return A newlib-compatible file handle or NULL if the file does not exist.
 */
static void * __eepfs_open(char * path, int flags)
{
    const int handle = eepfs_find_handle(path);

    /* Check if the handle is valid */
    if ( handle > 0 )
    {
        return (void *)handle;
    }

    return NULL;
}

/**
 * @brief Closes an already open file handle.
 *
 * @param[in] handle
 *            File handle as returned by #__eepfs_open
 *
 * @return 0 on success or a negative error otherwise.
 */
static int __eepfs_close(void * handle)
{
    eepfs_file_t * const file = eepfs_get_file((int)handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    /* Reset the cursor */
    file->cursor = 0;

    return EEPFS_ESUCCESS;
}

/**
 * @brief Reads data from a file.
 *
 * @param[in]  handle
 *             File handle as returned by #__eepfs_open
 * @param[out] dest
 *             Pointer to buffer to read to
 * @param[in]  len
 *             Length in bytes to read
 *
 * @return The actual amount of data read in bytes
 */
static int __eepfs_read(void * handle, uint8_t * dest, int len)
{
    eepfs_file_t * const file = eepfs_get_file((int)handle);

    return eepfs_cursor_read(file, dest, len);
}

/**
 * @brief Writes data to a file
 *
 * @param[in] handle
 *            File handle as returned by #__eepfs_open
 * @param[in] src
 *            Pointer to buffer of data to write
 * @param[in] len
 *            Length in bytes to read
 *
 * @return The actual amount of data written in bytes
 */
static int __eepfs_write(void * handle, uint8_t * src, int len)
{
    eepfs_file_t * const file = eepfs_get_file((int)handle);

    return eepfs_cursor_write(file, src, len);
}

/**
 * @brief Seeks to an offset in the file.
 * 
 * The offset will be clamped to the beginning and end of the file.
 *
 * @param[in] handle
 *            File handle as returned by #__eepfs_open
 * @param[in] offset
 *            Offset based on origin
 * @param[in] origin
 *            A direction to seek from.  Either #SEEK_SET, #SEEK_CUR or #SEEK_END
 *
 * @return The new position in the file after the seek.
 */
static int __eepfs_lseek(void * handle, int offset, int origin)
{
    eepfs_file_t * const file = eepfs_get_file((int)handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    size_t new_cursor = file->cursor;
    const size_t max_cursor = file->max_cursor;

    /* Adjust cursor based on origin and offset */
    if ( origin == SEEK_SET )
    {
        new_cursor = offset;
    }
    else if ( origin == SEEK_CUR )
    {
        new_cursor = new_cursor + offset;
    }
    else if ( origin == SEEK_END )
    {
        new_cursor = max_cursor + offset;
    }
    else
    {
        return EEPFS_EBADINPUT;
    }

    /* Clamp cursor to the beginning and end of the file */
    if ( new_cursor > max_cursor )
    {
        new_cursor = max_cursor;
    }
    if ( new_cursor < 0 )
    {
        new_cursor = 0;
    }

    file->cursor = new_cursor;

    return new_cursor;
}

/**
 * @brief Newlib-compatible fstat.
 *
 * @param[in]  handle
 *             File handle as returned by #__eepfs_open
 * @param[out] st
 *             Stat structure to populate
 *
 * @return 0 if successful or negative error otherwise
 */
static int __eepfs_fstat(void * handle, struct stat * st)
{
    const eepfs_file_t * const file = eepfs_get_file((int)handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    st->st_dev = 0;
    st->st_ino = 0;
    st->st_mode = S_IFREG;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = file->max_cursor;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    st->st_blksize = EEPROM_BLOCK_SIZE;
    st->st_blocks = file->num_blocks;

    return EEPFS_ESUCCESS;
}

/**
 * @brief Erases a file given a path.
 * 
 * Note that "erasing" a file just means writing it full of zeroes.
 * All files in the filesystem must always exist at the size specified
 * during #eepfs_init
 * 
 * Be advised: this is a destructive operation that cannot be undone!
 *
 * @param[in] path
 *            Absolute path of the file to unlink
 *
 * @return 0 if successful or negative error otherwise
 */
static int __eepfs_unlink(char * path)
{
    return eepfs_erase(path);
}

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
int eepfs_init(const eepfs_entry_t * const entries, const size_t count)
{
    /* Check if EEPROM FS has already been initialized */
    if ( eepfs_files_count != 0 || eepfs_files != NULL )
    {
        return EEPFS_ECONFLICT;
    }

    /* Sanity check the arguments */
    if ( count == 0 || entries == NULL )
    {
        return EEPFS_EBADFS;
    }

    /* Add an extra file for the "signature file" */
    eepfs_files_count = count + 1;

    /* Allocate the lookup table of file descriptors */
    const size_t files_size = sizeof(eepfs_file_t) * eepfs_files_count;
    eepfs_files = (eepfs_file_t *)malloc(files_size);

    if ( eepfs_files == NULL )
    {
        return EEPFS_ENOMEM;
    }

    /* The first file should always be the "signature file" */
    const eepfs_file_t signature_file = { NULL, 0, 1, 0, 8 };
    memcpy(&eepfs_files[0], &signature_file, sizeof(eepfs_file_t));

    const char * file_path;
    size_t file_size;
    size_t file_blocks;
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
            return EEPFS_EBADFS;
        }

        /* Strip the leading '/' on paths for consistency */
        file_path += ( file_path[0] == '/' );

        /* A file takes up 1 block for every 8 bytes, rounded up */
        file_blocks = divide_ceil(file_size, EEPROM_BLOCK_SIZE);

        /* Create a file descriptor and copy it into the table */
        const eepfs_file_t entry_file = {
            file_path,
            total_blocks,
            file_blocks,
            0,
            file_size,
        };
        memcpy(&eepfs_files[i], &entry_file, sizeof(eepfs_file_t));

        /* Files must start on a block boundary */
        total_blocks += file_blocks;
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

    /* Check if the EEPROM is roughly compatible with the filesystem */
    if ( !eepfs_verify_signature() )
    {
        /* If not, erase it and start from scratch */
        eepfs_wipe();
    }

    return EEPFS_ESUCCESS;
}

/**
 * @brief De-initializes the EEPROM filesystem.
 * 
 * This detaches the POSIX filesystem and cleans up the file lookup table.
 * 
 * You probably won't ever need to call this.
 * 
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_close(void)
{
    /* If eepfs was not initialized, don't do anything. */
    if ( eepfs_files == NULL || eepfs_files_count == 0 )
    {
        return EEPFS_EBADFS;
    }

    eepfs_detach();

    /* Clear the file descriptor table */
    free(eepfs_files);
    eepfs_files = NULL;
    eepfs_files_checksum = 0;
    eepfs_files_count = 0;

    return EEPFS_ESUCCESS;
}

/**
 * @brief Attaches EEPROM filesystem to POSIX stdio with the supplied prefix.
 * 
 * @see #attach_filesystem
 * 
 * @return 0 on success or a negative error otherwise
 */
int eepfs_attach(const char * const prefix)
{
    static filesystem_t eepfs_stdio_filesystem = {
        __eepfs_open,
        __eepfs_fstat,
        __eepfs_lseek,
        __eepfs_read,
        __eepfs_write,
        __eepfs_close,
        __eepfs_unlink,
        NULL, /* findfirst (not implemented) */
        NULL  /* findnext (not implemented) */
    };

    int retval = attach_filesystem(prefix, &eepfs_stdio_filesystem); 

    /* If successful... */
    if ( retval == 0 )
    {
        /* Cache the attached prefix so that it can be detached later */
        eepfs_stdio_prefix = prefix;
    }

    return retval;
}

/**
 * @brief Detaches the stdio prefix that was attached by #eepfs_attach
 * 
 * This happens implicitly as part of #eepfs_close and is
 * a no-op if the filesystem is not currently attached.
 */
void eepfs_detach(void)
{
    if ( eepfs_stdio_prefix != NULL )
    {
        detach_filesystem(eepfs_stdio_prefix);
        eepfs_stdio_prefix = NULL;
    }
}

/**
 * @brief Reads an entire file from the EEPROM filesystem.
 *
 * @param[in]  path
 *             Path of file in EEPROM filesystem to read from
 * @param[out] dest
 *             Buffer to read into
 *
 * @return The number of bytes read or a negative value on failure.
 */
int eepfs_read(const char * const path, void * const dest)
{
    const int handle = eepfs_find_handle(path);
    eepfs_file_t * const file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return EEPFS_ENOFILE;
    }

    /* Reset the cursor to read from the beginning of the file */
    const size_t cursor_restore = file->cursor;
    file->cursor = 0;

    /* Read to the end of the file */
    const int bytes_read = eepfs_cursor_read(file, dest, file->max_cursor);

    /* Restore the cursor to its previous value */
    file->cursor = cursor_restore;

    return bytes_read;
}

/**
 * @brief Writes an entire file to the EEPROM filesystem.
 *
 * @param[in] path
 *            Path of file in EEPROM filesystem to write to
 * @param[in] src
 *            Buffer of data to be written
 *
 * @return The number of bytes written or a negative value on failure.
 */
int eepfs_write(const char * const path, const void * const src)
{
    const int handle = eepfs_find_handle(path);
    eepfs_file_t * const file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return EEPFS_ENOFILE;
    }

    /* Reset the cursor to write from the beginning of the file */
    const size_t cursor_restore = file->cursor;
    file->cursor = 0;

    /* Write to the end of the file */
    const int bytes_written = eepfs_cursor_write(file, src, file->max_cursor);
 
    /* Restore the cursor to its previous value */
    file->cursor = cursor_restore;

    return bytes_written;
}

/**
 * @brief Erases a file in the EEPROM filesystem.
 * 
 * Note that "erasing" a file just means writing it full of zeroes.
 * All files in the filesystem must always exist at the size specified
 * during #eepfs_init
 * 
 * Be advised: this is a destructive operation that cannot be undone!
 * 
 * @retval EEPFS_ESUCCESS if successful
 * @retval EEPFS_ENOFILE if the path is not a valid file
 * @retval EEPFS_EBADINPUT if the path is NULL
 */
int eepfs_erase(const char * const path)
{
    const int handle = eepfs_find_handle(path);
    const eepfs_file_t * const file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return EEPFS_ENOFILE;
    }

    size_t current_block = file->start_block;
    const size_t total_blocks = file->num_blocks;

    /* eeprom_buf is initialized to all zeroes */
    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE] = {0};

    /* Write the blocks in with zeroes */
    while ( current_block < total_blocks )
    {
        eeprom_write(current_block++, eeprom_buf);
    }

    /* Success */
    return EEPFS_ESUCCESS;
}

/**
 * @brief Erases all blocks in EEPROM and sets a new signature.
 * 
 * This is useful when you want to erase all files in the filesystem.
 * 
 * Be advised: this is a destructive operation that cannot be undone!
 * 
 * @see #eepfs_verify_signature
 */
void eepfs_wipe(void)
{
    const uint64_t signature = eepfs_generate_signature();

    /* Write the filesystem signature into the first block */
    eeprom_write(0, (uint8_t *)&signature);

    size_t current_block = 1;
    const size_t total_blocks = eeprom_total_blocks();

    /* eeprom_buf is initialized to all zeroes */
    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE] = {0};

    /* Write the rest of the blocks in with zeroes */
    while ( current_block < total_blocks )
    {
        eeprom_write(current_block++, eeprom_buf);
    }
}

