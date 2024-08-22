/**
 * @file nand.h
 * @brief iQue NAND (flash) support
 * @ingroup ique
 * 
 * This module contains low-level functions to perform read/write operations
 * on the iQue flash memory (NAND). The flash memory is used to store the
 * operating system and the games. Contents in the flash are arranged using
 * a custom filesystem called BBFS, which is implemented in the bbfs.h module.
 */

/**
 * @defgroup ique iQue-specific hardware interfaces
 * @ingroup libdragon
 * @brief Hardware interfaces and libraries specific for the iQue Player
 * 
 * This module contains both lower-level hardware interfaces and higher-level
 * libraries that are specific to the iQue Player (also called "BB player").
 * It is a console that was released in China that is compatible with the
 * original N64 hardware while being faster (higher clock, faster RAM).
 * 
 * Libdragon supports iQue transparently: all ROMs built with libdragon will
 * also run on the iQue player without modifications (and be even faster!).
 * 
 * Given that the iQue player also has specific additional hardware components,
 * libdragon also features some libraries to access them and use them in your
 * programs (though, at that point, your program will not be compatible with
 * the original N64).
 * 
 * To perform a runtime check on what console the ROM is running, just use
 * the #sys_bbplayer function, that will return true when running on iQue.
 * 
 */
#ifndef LIBDRAGON_BB_NAND_H
#define LIBDRAGON_BB_NAND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief An absolute address in the NAND.
 * 
 * The address can be interpreted as a simple offset byte address from the
 * start of the NAND, but given the hardware layout of the NAND, it can be
 * thought as composed of three components:
 * 
 *  - Block number (12 bits)
 *  - Page number (5 bits)
 *  - Offset within the page (9 bits)
 * 
 * You can use #NAND_ADDR_MAKE to build an address from the three
 * components, and #NAND_ADDR_BLOCK, #NAND_ADDR_PAGE, #NAND_ADDR_OFFSET
 * to extract the components from an address.
 */
typedef uint32_t nand_addr_t;

#define NAND_BLOCK_SIZE         0x4000          ///< Size of a NAND block in bytes
#define NAND_PAGE_SIZE          0x200           ///< Size of a NAND page in bytes

/** @brief Build a NAND address given the block, page and offset. */
#define NAND_ADDR_MAKE(block, page, offset)      (((block) << 14) | ((page) << 9) | (offset))   
/** @brief Extract the page offset from an address */
#define NAND_ADDR_OFFSET(addr)                   (((addr) >>  0) & 0x1FF)
/** @brief Extract the page index from an address */
#define NAND_ADDR_PAGE(addr)                     (((addr) >>  9) & 0x01F)
/** @brief Extract the block index from an address */
#define NAND_ADDR_BLOCK(addr)                    (((addr) >> 14) & 0xFFF)

/** @brief Flags used for #nand_mmap */
typedef enum {
    NAND_MMAP_ENCRYPTED = (1<<0),               ///< Data in the filesystem will be decrypted by the mmap
} nand_mmap_flags_t;

/**
 * @brief Initialize the library to access the NAND flash on the iQue Player
 */
void nand_init(void);

/**
 * @brief Return the size of the installed NAND.
 * 
 * @return int  Size of the NAND in bytes (either 64 MiB or 128 MiB)
 */
int nand_get_size(void);

/**
 * @brief Read one or multiple full pages from the NAND
 * 
 * This is the lower level function to read data from the NAND. It reads
 * only full pages and optionally perform ECC correction while reading them.
 * 
 * Each page of the flash contains 1024 bytes of data (#NAND_PAGE_SIZE) and
 * 16 bytes of so-called "spare" data. The ECC codes are stored in 6 bytes of
 * spare data, while some of the other bytes seem used by the official iQue OS
 * for marking bad blocks or other not fully understood purposes.
 * 
 * You can read the spare data by providing a pointer to a buffer where to
 * store it (16 bytes per each requested page). If you don't need the spare data,
 * you can pass NULL. Notice that ECC correction can be performed even if you
 * pass NULL to spare.
 * 
 * @param addr      Address to read from (use NAND_ADDR_MAKE to build)
 * @param npages    Number of pages to read
 * @param buffer    Buffer to read data into (1024 bytes per page, aka #NAND_PAGE_SIZE)
 * @param spare     If not NULL, read also the spare area into the specified buffer (16 bytes per page)
 * @param ecc       Whether to use ECC to correct/verify errors
 * @return >=0      If OK (number of pages read)
 * @return -1       If at least one page had an unrecoverable ECC error
 */
int nand_read_pages(nand_addr_t addr, int npages, void *buffer, void *spare, bool ecc);

/**
 * @brief Write pages to the NAND.
 * 
 * Writing to NAND is well defined only on erased blocks. If you write to a
 * non-erased block, the data will likely be corrupted.
 * 
 * While technically it is possible to write even data smaller than a page,
 * that doesn't allow the flash controller to recalculate the ECC, so 
 * it is not recommended because ECC is required to detect failures.
 * 
 * @param addr      Address to write to (use NAND_ADDR_MAKE to build)
 * @param npages    Number of pages to write
 * @param buffer    Buffer to write data from
 * @param ecc       Whether to compute and write ECC for the pages
 * 
 * @return >=0      If OK (number of pages written)
 * @return -1       If error during writing
 */
int nand_write_pages(nand_addr_t addr, int npages, const void *buffer, bool ecc);

/**
 * @brief Erase a block on the NAND.
 * 
 * You must erase a block before being able to write to it. Notice that erasing
 * only works on the whole block, so all the pages inside are erased.
 * 
 * @param addr      Address of the block to erase (must be a block address)
 * 
 * @return 0 if OK, -1 if error
 */
int nand_erase_block(nand_addr_t addr);

/**
 * @brief Read sequential data from the NAND.
 * 
 * This function reads a sequence of bytes from the NAND, across different
 * pages and/or blocks. It will only fetch the requested bytes. 
 * 
 * Notice that it is not possible to check ECC when using this function,
 * because ECC only works when reading full pages. Not using ECC while reading
 * might cause data corruption if the NAND has errors, so carefully consider
 * whether to use this function.
 * 
 * @param addr      Address to read from (use NAND_ADDR_MAKE to build)
 * @param buffer    Buffer to read data into
 * @param len       Number of bytes to read
 * 
 * @return 0 if OK, -1 if error
 */
int nand_read_data(nand_addr_t addr, void *buffer, int len);

/**
 * @brief Initialize configuration of the NAND memory mapping to PI address space.
 * 
 * On iQue, a special hardware component called the ATB (Address Translation
 * Buffer?) allows to memory map blocks of flash to the PI bus. This allows
 * for instance for a ROM to be mapped at 0x10000000, which is required for
 * booting them.
 * 
 * The configuration must be done from scratch every time it is changed; it
 * is thus necessary to call #nand_mmap_begin to initialize the configuration,
 * then call #nand_mmap for each block to map, and finally call #nand_mmap_end
 * to finish the configuration.
 * 
 * @see #nand_mmap
 * @see #nand_mmap_end
 */
void nand_mmap_begin(void);

/**
 * @brief Memory-map flash blocks to PI address space via ATB.
 * 
 * On iQue, a special hardware component called the ATB (Address Translation
 * Buffer?) allows to memory map blocks of flash to the PI bus. This allows
 * for instance for a ROM to be mapped at 0x10000000, which is required for
 * booting them.
 * 
 * This function configures a specific mapping from a sequence of blocks to
 * a PI address area. The flash blocks can be non consecutive, though there
 * are some limits to the number of ATB entries that can be configured,
 * so it is better to use consecutive blocks if possible.
 * 
 * If multiple calls to #nand_mmap are done, they must be done in increasing
 * @a pi_address order.
 * 
 * The BBFS filesystem is configured to use consecutive blocks as much as possible
 * for large files (> 512 KiB). To map a file from a NAND formatted with the
 * BBFS filesystem, you can use #bbfs_get_file_blocks to get the list of blocks
 * to map.
 * 
 * @param pi_address    PI address to map the blocks to
 * @param blocks        Array of block numbers to map, terminated by -1
 * @param flags         Flags to control the mapping (#NAND_MMAP_ENCRYPTED)
 * @return int          0 if OK, or -1 in case of error. A possible error
 *                      is that there are not enough ATB entries available. In
 *                      this case, it is necessary to defragment the file on
 *                      the NAND (as a single ATB entry can map multiple
 *                      consecutive blocks).
 * 
 * @see #nand_mmap_begin
 * @see #nand_mmap_end
 */
int nand_mmap(uint32_t pi_address, int16_t *blocks, nand_mmap_flags_t flags);

/**
 * @brief Finish configuration of the NAND memory mapping to PI address space.
 * 
 * On iQue, a special hardware component called the ATB (Address Translation
 * Buffer?) allows to memory map blocks of flash to the PI bus. This allows
 * for instance for a ROM to be mapped at 0x10000000, which is required for
 * booting them.
 * 
 * This function must be called to finish the configuration. Notice that it
 * must be called even if #nand_mmap failed.
 */
void nand_mmap_end(void);

/**
 * @brief Compute the ECC code for a page of data.
 * 
 * iQue NAND contains a 6-byte ECC code for each 1024-byte page. This is actually
 * the combination of two 3-byte ECC for each 512-byte half of the page. The
 * code is stored in the spare date of each page (bytes 0x8-0xA contain the
 * ECC of the second half, and bytes 0xD-0xF contain the ECC of the first half).
 * 
 * This function implements the same algorithm and is provided for completeness.
 * It is normally not required to compute the ECC code manually, as the flash
 * controller will do that automatically when writing to the NAND (via
 * #nand_write_pages) and used to correct errors when reading from the NAND
 * (via #nand_read_pages).
 * 
 * @param buf               Buffer containing the 1024-byte page
 * @param ecc               Buffer to store the 6-byte ECC code
 */
void nand_compute_page_ecc(const uint8_t *buf, uint8_t *ecc);


#ifdef __cplusplus
}
#endif

#endif
