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
typedef struct eepfs_file
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
static size_t eepfs_num_files = 0;

/**
 * @brief The dynamically-allocated array of file entries.
 * 
 * Allocated and assigned when the filesystem is initialized,
 * and deallocated when the filesystem is de-initialized.
 */
static eepfs_file_t *eepfs_files = NULL;

/**
 * @brief The filesystem prefix used by stdio functions.
 * 
 * This is an optional parameter when initializing the
 * filesystem, but allows for `fopen` and friends to access
 * the EEPROM instead of using `eepfs_*` methods.
 * 
 * This prefix is captured so that it can be detached when
 * the filesystem de-initializes.
 */
static const char * eepfs_stdio_prefix = NULL;

/**
 * @brief Resolves a file handle to a file descriptor.
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
    if ( handle > 0 && handle < eepfs_num_files )
    {
        return &eepfs_files[handle];
    }

    return NULL;
}

/**
 * @brief A signature based on the filesystem configuration.
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
static const uint64_t eepfs_signature() {
    /* If each file takes at least one block,
       at most a 16k EEPROM can hold 255 files.
       Subtract one file for the signature block. */
    const uint8_t num_files = (eepfs_num_files - 1) & 0xFF;

    /* Sum up the total bytes used by all files.
       At most a 16k EEPROM can hold 2048 bytes. 
       Skip the first file (the signature block). */
    uint16_t total_bytes = 0;
    for ( size_t i = 1; i < eepfs_num_files; ++i )
    {
        total_bytes += eepfs_files[i].max_cursor;
    }

    /* Craft an 8-byte block and return it as a value */
    uint64_t signature;
    uint8_t * const sig_bytes = (uint8_t *)&signature;
    sig_bytes[0] = 'e';
    sig_bytes[1] = 'e';
    sig_bytes[2] = 'p';
    sig_bytes[3] = 'f';
    sig_bytes[4] = 's';
    sig_bytes[5] = num_files;
    sig_bytes[6] = total_bytes >> 8;
    sig_bytes[7] = total_bytes & 0xFF;
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
 * @see #eepfs_signature
 * @see #eepfs_wipe
 * 
 * @retval 0 if the first block of EEPROM matches the filesystem signature
 * @retval 1 if the first block of EEPROM does not match the filesystem signature
 */
int eepfs_check_signature()
{
    const uint64_t signature = eepfs_signature();
    const uint8_t * const sig_bytes = (uint8_t *)&signature;

    /* Read the signature block out of EEPROM */
    uint8_t eeprom_buf[EEPROM_BLOCK_SIZE];
    eeprom_read(0, eeprom_buf);

    /* If the signatures don't match, we can be pretty sure
       that the data in EEPROM is not the expected filesystem */
    if ( memcmp(eeprom_buf, sig_bytes, EEPROM_BLOCK_SIZE) != 0 )
    {
        /* Signatures do not match */
        return 1;
    }

    /* Signature matches */
    return 0;
}

/**
 * @brief Erases all blocks in EEPROM and sets a new signature.
 * 
 * This is useful when you want to erase all files in the filesystem.
 * 
 * Be advised: this is a destructive operation that cannot be undone!
 * 
 * @see #eepfs_check_signature
 */
void eepfs_wipe()
{
    const uint64_t signature = eepfs_signature();

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

/**
 * @brief Open a file given a path.
 * 
 * Check if the path matches an entry in the `eepfs_files` table
 * and return a handle to manipulate the file with.
 * 
 * A leading '/' in the path is optional and is supported to
 * allow for stdio methods to access files using a custom prefix.
 * 
 * Opened files can be used for reading or writing, but cannot be resized.
 * 
 * Opening a file again without closing it will return the same handle as
 * the first time it was opened. This is not recommended.
 * 
 * @param[in] path
 *            Path of the file to open
 * 
 * @return A valid file handle to reference the file by or a negative error on failure
 */
int eepfs_open(const char * const path)
{
    if ( path == NULL )
    {
        return EEPFS_EBADINPUT;
    }

    /* Attempt to find the path in the list of files */
    for ( size_t i = 1; i < eepfs_num_files; ++i )
    {
        if ( strcmp(path, eepfs_files[i].path) == 0 )
        {
            return i;
        }
    }

    /* Allow for the leading slash to be optional */
    if ( path[0] == '/' )
    {
        return eepfs_open(path + 1);
    }

    return EEPFS_ENOFILE;
}

/**
 * @brief Close an already open file handle.
 * 
 * @param[in] handle
 *            A valid file handle as returned by #eepfs_open
 * 
 * @retval EEPFS_ESUCCESS if sucessful
 * @retval EEPFS_EBADHANDLE if the file handle is invalid
 */
int eepfs_close(const int handle)
{
    eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    file->cursor = 0;

    return EEPFS_ESUCCESS;
}

/**
 * @brief Erase a file given a path.
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
    int handle = eepfs_open(path);
    
    eepfs_file_t *file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        /* File does not exist, return error code */
        return handle;
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
 * @brief Seek to an offset in the file.
 * 
 * The offset will be clamped to the beginning and end of the file.
 * 
 * @param[in] handle
 *            A valid file handle as returned by #eepfs_open
 * @param[in] offset
 *            A byte offset from the origin to seek from
 * @param[in] origin
 *            An offset to seek from. Either #SEEK_SET, #SEEK_CUR, or #SEEK_END
 * 
 * @retval EEPFS_ESUCCESS if successful
 * @retval EEPFS_EBADHANDLE if handle is invalid
 * @retval EEPFS_EBADINPUT if origin is invalid
 */
int eepfs_seek(const int handle, const int offset, const int origin)
{
    eepfs_file_t * file = eepfs_get_file(handle);

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

    return EEPFS_ESUCCESS;
}

/**
 * @brief Return the current offset into a file.
 * 
 * @param[in] handle
 *            A valid file handle as returned by #eepfs_open
 * 
 * @return The current byte offset into the file or a negative error on failure
 */
int eepfs_tell(const int handle)
{
    eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    return file->cursor;
}

/**
 * @brief Return the size of a file.
 * 
 * @param[in] handle
 *            A valid file handle as returned by #eepfs_open
 * 
 * @return The file size in bytes or a negative error on failure
 */
int eepfs_size(const int handle)
{
    eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    return file->max_cursor;
}

/**
 * @brief Return whether the end of file has been reached.
 * 
 * @param[in] handle
 *            A valid file handle as returned by #eepfs_open
 * 
 * @retval 1 if the end of the file is reached
 * @retval 0 if the end of the file is not reached
 * @retval EEPFS_EBADHANDLE if handle is invalid
 */
int eepfs_eof(const int handle)
{
    eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    if (file->cursor == file->max_cursor)
    {
        /* Cursor is at the end of the file */
        return 1;
    }

    /* Cursor is not at the end of the file */
    return 0;
}

/**
 * @brief Read data from a file.
 *
 * @param[out] buf
 *             Buffer to read into
 * @param[in]  size
 *             Size of each element to read
 * @param[in]  count
 *             Number of elements to read
 * @param[in]  handle
 *             A valid file handle as returned from #eepfs_open.
 *
 * @return The actual number of bytes read or a negative value on failure.
 */
int eepfs_read(void * const buf, const size_t size, const size_t count, const int handle)
{
    /* Sanity check */
    if ( buf == NULL ) 
    {
        return EEPFS_EBADINPUT;
    }

    eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    size_t cursor = file->cursor;
    size_t bytes_to_read = size * count;

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
    uint8_t * data = buf;

    do {
        eeprom_read(current_block, eeprom_buf);
        /* Fill the buffer with the data from the current block */
        while ( bytes_to_read > 0 && block_byte_offset < EEPROM_BLOCK_SIZE )
        {
            *(data++) = eeprom_buf[block_byte_offset];
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
 * @brief Write data to a file.
 *
 * @param[in] buf
 *            Buffer of data to be written
 * @param[in] size
 *            Size of each element to write
 * @param[in] count
 *            Number of elements to write
 * @param[in] handle
 *            A valid file handle as returned from #eepfs_open.
 *
 * @return The actual number of bytes written or a negative value on failure.
 */
int eepfs_write(const void * const buf, const size_t size, const size_t count, const int handle)
{
    if ( buf == NULL ) 
    {
        return EEPFS_EBADINPUT;
    }

    eepfs_file_t * file = eepfs_get_file(handle);

    if ( file == NULL )
    {
        return EEPFS_EBADHANDLE;
    }

    size_t cursor = file->cursor;
    size_t bytes_to_write = size * count;

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
    const uint8_t * data = buf;

    do {
        /* Only read in the current block if we need to preserve data from it */
        if ( bytes_to_write < EEPROM_BLOCK_SIZE || block_byte_offset != 0 )
        {
            eeprom_read(current_block, eeprom_buf);
        }
        /* Fill the current block with the data to write */
        while ( bytes_to_write > 0 && block_byte_offset < EEPROM_BLOCK_SIZE )
        {
            eeprom_buf[block_byte_offset] = *(data++);
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
 * @brief Newlib-compatible open.
 *
 * @param[in] path
 *            Absolute path of the file to open
 * @param[in] flags
 *            POSIX file flags (ignored)
 *
 * @return A newlib-compatible file handle.
 */
static void *__eepfs_open(char *path, int flags)
{
    /* We disregard flags here */
    int handle = eepfs_open(path);

    if ( handle <= 0 )
    {
        return NULL;
    }

    return (void *)handle;
}

/**
 * @brief Newlib-compatible close.
 *
 * @param[in] handle
 *            File pointer as returned by #__eepfs_open
 *
 * @return 0 on success or a negative error otherwise.
 */
static int __eepfs_close(void *handle)
{
    return eepfs_close((int)handle);
}

/**
 * @brief Newlib-compatible read.
 *
 * @param[in]  handle
 *             File pointer as returned by #__eepfs_open
 * @param[out] ptr
 *             Pointer to buffer to read to
 * @param[in]  len
 *             Length in bytes to read
 *
 * @return The actual amount of data read in bytes
 */
static int __eepfs_read(void *handle, uint8_t *buf, int len)
{
    return eepfs_read(buf, len, 1, (int)handle);
}

/**
 * @brief Newlib-compatible read.
 *
 * @param[in] handle
 *            File pointer as returned by #__eepfs_open
 * @param[in] ptr
 *            Pointer to buffer of data to write
 * @param[in] len
 *            Length in bytes to read
 *
 * @return The actual amount of data written in bytes
 */
static int __eepfs_write(void *handle, uint8_t *buf, int len)
{
    return eepfs_write(buf, len, 1, (int)handle);
}

/**
 * @brief Newlib-compatible lseek.
 *
 * @param[in] handle
 *            File pointer as returned by #__eepfs_open
 * @param[in] offset
 *            Offset based on origin
 * @param[in] origin
 *            A direction to seek from.  Either #SEEK_SET, #SEEK_CUR or #SEEK_END
 *
 * @return The new position in the file after the seek.
 */
static int __eepfs_lseek(void *handle, int offset, int origin)
{
    int ret = eepfs_seek((int)handle, offset, origin);

    if ( ret != EEPFS_ESUCCESS )
    {
        return ret;
    }

    return eepfs_tell((int)handle);
}

/**
 * @brief Newlib-compatible fstat.
 *
 * @param[in]  handle
 *             File pointer as returned by #__eepfs_open
 * @param[out] st
 *             Stat structure to populate
 *
 * @return 0 if successful or negative error otherwise
 */
static int __eepfs_fstat(void *handle, struct stat *st)
{
    eepfs_file_t * file = eepfs_get_file((int)handle);

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
 * @brief Newlib-compatible unlink.
 *
 * @param[in] path
 *            Absolute path of the file to unlink
 *
 * @return 0 if successful or negative error otherwise
 */
static int __eepfs_unlink(char *path)
{
    return eepfs_erase(path);
}

/**
 * @brief Structure used for hooking the EEPROM filesystem into newlib.
 *
 * The following section of code is for bridging into newlib's filesystem hooks 
 * to allow POSIX access to the EEPROM filesystem.
 * 
 * @see #attach_filesystem
 */
static filesystem_t eepfs_stdio_hooks = {
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

/**
 * @brief Initialize the EEPROM filesystem.
 * 
 * Creates a lookup table of file descriptors based on the configuration,
 * validates that the current EEPROM data is likely to be compatible with
 * the configured file descriptors, and hooks the filesystem into stdio.
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
 * If skip_signature_check is `false` and the signature does not match, 
 * all EEPROM data will be erased! If skip_signature_check is `true`, you
 * are on your own to validate the data. Skipping the signature check can be
 * useful to support migrating EEPROM data during development, but should be
 * used with caution and is not recommended for released builds.
 *
 * This function will also register with newlib so that standard POSIX file
 * operations work with the EEPROM filesystem using the given path prefix 
 * (e.g. `eeprom:/`).
 *
 * @param[in] config
 *            A struct containing the configuration options for the filesystem
 *
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_init(const eepfs_config_t config)
{
    /* Check if EEPROM FS has already been initialized */
    if ( eepfs_num_files != 0 || eepfs_files != NULL )
    {
        return EEPFS_ECONFLICT;
    }

    /* Sanity check the configuration */
    if ( config.num_files == 0 || config.files == NULL )
    {
        return EEPFS_EBADFS;
    }

    /* Add an extra entry for the "signature" file */
    eepfs_num_files = config.num_files + 1;

    /* Allocate the lookup table of file descriptors */
    const size_t files_array_size = sizeof(eepfs_file_t) * eepfs_num_files; 
    eepfs_files = (eepfs_file_t *)malloc(files_array_size);

    if ( eepfs_files == NULL )
    {
        return EEPFS_ENOMEM;
    }

    /* The first entry should always be the "signature" file */
    eepfs_file_t signature_file = { NULL, 0, 1, 0, 8 };
    memcpy(&eepfs_files[0], &signature_file, sizeof(eepfs_file_t));

    const char * file_path;
    size_t file_size;
    size_t file_blocks;
    size_t total_blocks = 1;

    /* Add entries for the files in the config */
    for ( size_t i = 1; i < eepfs_num_files; ++i )
    {
        file_path = config.files[i - 1].path;
        file_size = config.files[i - 1].size;

        /* Sanity check the file details */
        if ( file_path == NULL || file_size == 0 )
        {
            eepfs_deinit();
            return EEPFS_EBADFS;
        }

        /* Strip the leading '/' on paths for consistency */
        if ( file_path[0] == '/' )
        {
            file_path += 1;
        }

        /* A file takes up 1 block for every 8 bytes, rounded up */
        file_blocks = divide_ceil(file_size, EEPROM_BLOCK_SIZE);

        /* Create a file descriptor and copy it into the table */
        eepfs_file_t file_descriptor = {
            file_path,
            total_blocks,
            file_blocks,
            0,
            file_size,
        };
        memcpy(&eepfs_files[i], &file_descriptor, sizeof(eepfs_file_t));

        /* Files must start on a block boundary */
        total_blocks += file_blocks;
    }

    /* Ensure the filesystem will actually fit in available EEPROM */
    if ( total_blocks > eeprom_total_blocks() )
    {
        eepfs_deinit();
        return EEPFS_EBADFS;
    }

    /* Check if the EEPROM is roughly compatible with the filesystem */
    if ( !config.skip_signature_check && eepfs_check_signature() != 0 )
    {
        /* If not, erase it and start from scratch */
        eepfs_wipe();
    }

    /* If a prefix is specified, attach a stdio filesystem */
    if ( config.stdio_prefix != NULL )
    {
        if ( attach_filesystem(config.stdio_prefix, &eepfs_stdio_hooks) == 0 )
        {
            eepfs_stdio_prefix = config.stdio_prefix;
        }
    }

    return EEPFS_ESUCCESS;
}

/**
 * @brief De-initialize the EEPROM filesystem.
 * 
 * This detaches the POSIX filesystem and cleans up the file lookup table.
 * 
 * You probably don't ever need to do this, but it can be useful for
 * migrating save data between different configurations during development.
 * 
 * @return EEPFS_ESUCCESS on success or a negative error otherwise
 */
int eepfs_deinit()
{
    /* If eepfs was not initialized, don't do anything. */
    if ( eepfs_files == NULL || eepfs_num_files == 0 )
    {
        return EEPFS_EBADFS;
    }

    /* If a prefix was specified, detach the stdio filesystem */
    if ( eepfs_stdio_prefix != NULL )
    {
        detach_filesystem(eepfs_stdio_prefix);
        eepfs_stdio_prefix = NULL;
    }

    /* Clear the file descriptor table */
    free(eepfs_files);
    eepfs_files = NULL;
    eepfs_num_files = 0;

    return EEPFS_ESUCCESS;
}
