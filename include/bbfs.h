/**
 * @file bbfs.h
 * @brief iQue BBFS flash filesytem
 * @ingroup ique
 * 
 * This module contains higher-level functions to interact with BBFS, the flash
 * filesystem used by the iQue Player.
 * 
 * Access to the filesystem is provided through the standard C/POSIX file I/O
 * functions, after calling #bbfs_init to mount the filesystem. To name files
 * on the filesystem, use the "bbfs:/" prefix.
 * 
 * Filesystem operations
 * ---------------------
 * 
 * Most standard operations are supported, including reading, writing, seeking.
 * All write operations also update the ECC code for each page, and all read
 * operations verify the ECC code and use it to correct single-bit errors.
 * If the ECC code cannot correct the error, the read operation will fail
 * and errno will be set to EIO. In this case, the data in the filesystem
 * is likely corrupted.
 * 
 * In general, writing on a flash filesystem always requires erasing each
 * block being written (with a block being 16 KiB). This is performed
 * lazily so that it is possible to write multiple times to the same block
 * without performance penalty (the block will be written down once). The
 * library also performs a basic wear leveling to avoid wearing out the
 * same blocks over and over.
 * 
 * Truncating files via #truncate is also possible, and can be used to either
 * reduce or increase the file size. When increasing the file size, the new
 * data is zeroed out, unless otherwise written. Truncation happens lazily,
 * so that it is possible to truncate a file to a larger size and then write
 * data to it without causing a performance penalty.
 * 
 * ROMs and memory mapping
 * ----------------------
 * 
 * To be able to boot a ROM, the ROM must be written in the filesystem, and
 * then memory mapped via #nand_mmap. This requires providing the list of the
 * blocks that contain the ROM data. To facilitate this, #bbfs_get_file_blocks
 * is provided to retrieve the list of blocks, that can then be passed to
 * #nand_mmap.
 * 
 * Since #nand_mmap only supports discontiguous blocks up to a certain limit,
 * the filesystem tries to keep ROM data in contiguous blocks as much as possible.
 * To achieve this, the filesystem is virtually split in two areas: a
 * "large files" area which covers most of the flash and where blocks are
 * allocated as contiguous as possible, and a "small files" area of the flash
 * where blocks are allocated randomically (to minimize wear leveling).
 * The small area is normally just 1 MiB, but will grow automatically when
 * almost full.
 * 
 * A file is stored in the large area as soon as its size is larger than 512 KiB.
 * This means that if you write a ROM file that is larger than 512 KiB, the first
 * blocks up to 512 KiB will still be stored in the small area and will be fragmented,
 * but the rest will be stored in the large area and will be contiguous.
 * 
 * While this is a suboptimal allocation, it will not create any immediate issue.
 * Anyway to perform an optimal allocation, there are two possible ways:
 * 
 *  * #ftruncate the file immediately after opening it, to communicate the final
 *    size right away. This will force the filesystem to allocate the file in
 *    the large area and contiguous.
 *  * Use the #IOBBFS_SET_CONTIGUOUS #ioctl to force the filesystem to immediately
 *    use the contiguous block allocation algorithm for the file, irrespective
 *    of its initial (or final) size.
 * 
 * For instance this is how you can use #ftruncate to obtain the optimal allocation,
 * for a ROM file:
 * 
 * \code{.c}
 *      FILE *f = fopen("bbfs:/my_rom.z64", "wb");
 *      
 *      // Truncate the file to the final size right away. Since the final
 *      // size is larger than 512 KiB, the file will immediately switch
 *      // to the large area and contiguous allocation.
 *      ftruncate(fileno(f), rom_size);
 * 
 *      // Write the ROM data
 *      fwrite(rom_data, 1, rom_size, f);
 * 
 *      // Close the file 
 *      fclose(f);
 * \endcode
 * 
 * To see an example of using the #ioctl instead, see #IOBBFS_SET_CONTIGUOUS.
 * 
 * Filesytem consistency checks
 * ----------------------------
 * 
 * The library also offers a function to check the filesystem consistency:
 * #bbfs_fsck. This function will scan the filesystem and check for logic errors
 * in the filesystem structure. If the filesystem is corrupted, it can optionally
 * try to fix the errors.
 * 
 * For errors that affect specific files, the portions of the files that can
 * be recovered are saved with the name "FSCK1234.XXX" where 1234 is a random
 * number. 
 * 
 * Notice that currently this function does not check the integrity of the
 * data stored in the filesystem (via ECC), only the filesystem structure.
 */

#ifndef LIBDRAGON_BBFS_H
#define LIBDRAGON_BBFS_H

#include <stdint.h>
#include <stdbool.h>
#include "ioctl.h"

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
 * @brief ioctl to notify the filesystem that a certain file must be stored as contiguous.
 * 
 * This is normally used for ROM files. Since ROM files must be memory mapped
 * via #nand_mmap, and #nand_mmap only supports discontiguous blocks up to a certain
 * limit, this #ioctl will inform the filesystem that the current file must be
 * stored in contiguous blocks as much as possible.
 * 
 * By default, the filesystem will use a heuristic: smaller files will be stored
 * fragmented, will larger files (> 512 KiB) will be stored contiguous. This
 * means that if you write a ROM file that is larger than 512 KiB, the first
 * blocks up to 512 KiB will still be fragmented, but the rest will be contiguous.
 * 
 * If you want to avoid this, you can either call #ftruncate immediately after opening
 * the file to communicate the final size right away, or use this #ioctl to force
 * to immediately use the contiguous block allocation algorithm.
 * 
 * \code{.c}
 *   FILE *f = fopen("bbfs:/my_rom.z64", "wb");
 * 
 *   // Notify the filesystem that this file must be stored as contiguous
 *   bool value = true;
 *   ioctl(fileno(f), IOBBFS_SET_CONTIGUOUS, &value);
 * 
 *   // Write the ROM data
 *   fwrite(rom_data, 1, rom_size, f);
 * 
 *   // Close the file
 *   fclose(f);
 * \endcode
 * 
 * Calling this #ioctl only affects blocks allocated after the call; there is
 * no way to change the allocation of blocks that have already been written
 * (short of truncating the file to zero, and writing it again).
 * 
 * Calling this #ioctl with the value set to false will revert the file
 * to the default allocation algorithm based on the heuristics. There is no way
 * to force a file to be always stored as fragmented.
 * 
 * See also the top comment of this file for more information.
 */
#define IOBBFS_SET_CONTIGUOUS   _IO('B', 0)

/**
 * @brief Get the current block number of an open file
 * 
 * This function is used to retrieve the current block number of an open file.
 * The block number is a 16-bit value that represents the current flash block
 * where the file is being read or written.
 * 
 * This can be used for debugging purposes, or to implement something similar
 * to #bbfs_get_file_blocks for an open file, by seeking into the file at each
 * block boundary.
 * 
 * \code{.c}
 *   FILE *f = fopen("bbfs:/my_rom.z64", "rb");
 * 
 *   // Get the first block in which the file is stored
 *   // WARNING: make sure to use int16_t for blocks.
 *   int16_t block = 0;
 *   ioctl(fileno(f), IOBBFS_GET_BLOCK, &block);
 * \endcode
 */
#define IOBBFS_GET_BLOCK        _IO('B', 1)

/**
 * @brief Initialize the iQue NAND filesystem library
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

/**
 * @brief Verify the integrity of the filesystem, and optionally try to fix it
 * 
 * This function checks the integrity of the filesystem. If the filesystem is
 * corrupted, it can optionally try to fix it.
 * 
 * @param fix_errors    If true, try to fix the errors, otherwise just check
 * @return number of errors found in the filesystem
 */
int bbfs_fsck(bool fix_errors);


#ifdef __cplusplus
}
#endif

#endif
