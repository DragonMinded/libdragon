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

void nand_read_data(nand_addr_t addr, void *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif
