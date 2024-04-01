#ifndef LIBDRAGON_BB_NAND_H
#define LIBDRAGON_BB_NAND_H

#include <stdint.h>

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
 * @param addr      Address to read from (use NAND_ADDR_MAKE to build)
 * @param buffer    Buffer to read data into
 * @param len       Number of bytes to read
 * 
 * @return 0 if OK, -1 if error
 */
int nand_read_data(nand_addr_t addr, void *buffer, int len);

/**
 * @brief Write sequential data to the NAND.
 * 
 * Writing to NAND is well defined only on erased blocks. If you write to a
 * non-erased block, the data will likely be corrupted.
 * 
 * @param addr      Address to write to (use NAND_ADDR_MAKE to build)
 * @param buffer    Buffer to write data from
 * @param len       Number of bytes to write
 * 
 * @return 0 if OK, -1 if error
 */
int nand_write_data(nand_addr_t addr, const void *buffer, int len);

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
 * @brief Memory-map flash blocks to PI address space via ATB.
 * 
 * On iQue, a special hardware component called the ATB (Address Translation
 * Buffer?) allows to memory map blocks of flash to the PI bus. This allows
 * for instance for a ROM to be mapped at 0x10000000, which is required for
 * booting them.
 * 
 * To map a file from a NAND formatted with the BBFS filesystem, you can use
 * #bbfs_get_file_blocks to get the list of blocks to map.
 * 
 * @param pi_address    PI address to map the blocks to
 * @param blocks        Array of block numbers to map, terminated by -1
 * @param atb_idx       If not NULL, it must contain the first ATB index to use
 *                      and will be filled with the index of the first ATB entry
 *                      after this mapping is done.
 * @param flags         Flags to control the mapping (#NAND_MMAP_ENCRYPTED)
 * @return int          0 if OK, or -1 in case of error. A possible error
 *                      is that there are not enough ATB entries available. In
 *                      this case, it is necessary to defragment the file on
 *                      the NAND (as a single ATB entry can map multiple
 *                      consecutive blocks).
 */
int nand_mmap(uint32_t pi_address, int16_t *blocks, int *atb_id, int flags);

#ifdef __cplusplus
}
#endif

#endif
