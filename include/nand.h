#ifndef LIBDRAGON_BB_NAND_H
#define LIBDRAGON_BB_NAND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t nand_addr_t;

#define NAND_PAGE_SIZE          0x200
#define NAND_BLOCK_SIZE         0x4000

#define NAND_ADDR_MAKE(block, page, offset)      (((block) << 14) | ((page) << 9) | (offset))
#define NAND_ADDR_OFFSET(addr)                   (((addr) >>  0) & 0x1FF)
#define NAND_ADDR_PAGE(addr)                     (((addr) >>  9) & 0x01F)
#define NAND_ADDR_BLOCK(addr)                    (((addr) >> 14) & 0xFFF)

#define NAND_MMAP_ENCRYPTED                      (1<<0)   ///< The mapping refers to encrypted data

/**
 * @brief Initialize the library to access the NAND flags on iQue Player
 */
void nand_init(void);

/**
 * @brief Return the size of the installed NAND.
 * 
 * @return int  Size of the NAND in bytes (either 64 MiB or 128 MiB)
 */
int nand_get_size(void);

/**
 * @brief Read sequential data from the NAND.
 * 
 * This function reads a sequence of bytes from the NAND, across different
 * pages and/or blocks. It will only fetch the requested bytes. It is not
 * possible to check ECC when using this function, because ECC only works
 * when reading full pages.
 * 
 * @param addr      Address to read from (use NAND_ADDR_MAKE to build)
 * @param buffer    Buffer to read data into
 * @param len       Number of bytes to read
 * 
 * @return 0 if OK, -1 if error
 */
int nand_read_data(nand_addr_t addr, void *buffer, int len);

/**
 * @brief Read one or multiple full pages from the NAND
 * 
 * This is the lower level function to read data from the NAND. It reads
 * only full pages, and is provided for symmetry with #nand_write_pages.
 * Since it is indeed possible to read also partial pages, most users
 * will prefer to use #nand_read_data instead.
 * 
 * @param addr      Address to read from (use NAND_ADDR_MAKE to build)
 * @param npages    Number of pages to read
 * @param buffer    Buffer to read data into (1024 bytes per page, aka #NAND_PAGE_SIZE)
 * @param spare     If not NULL, read also the spare area into the specified buffer (16 bytes per page)
 * @param ecc       Whether to use ECC to correct/verify errors
 * @return 0        If OK
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
 * @return 0 if OK, -1 if error
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
int nand_mmap(uint32_t pi_address, int16_t *blocks, int flags);

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
 * (via #nand_read_page).
 * 
 * @param buf               Buffer containing the 1024-byte page
 * @param ecc               Buffer to store the 6-byte ECC code
 */
void nand_compute_page_ecc(const uint8_t *buf, uint8_t *ecc);


#ifdef __cplusplus
}
#endif

#endif
