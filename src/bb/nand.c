#include <stdint.h>
#include <assert.h>
#include "n64sys.h"
#include "debug.h"
#include "dma.h"
#include "nand.h"
#include "utils.h"

#define PI_DRAM_ADDR                        ((volatile uint32_t*)0xA4600000)
#define PI_CART_ADDR                        ((volatile uint32_t*)0xA4600004)
#define PI_BB_ATB_UPPER                     ((volatile uint32_t*)0xA4600040)        
#define PI_BB_NAND_CTRL                     ((volatile uint32_t*)0xA4600048)
#define PI_BB_NAND_CFG                      ((volatile uint32_t*)0xA460004C)
#define PI_BB_RD_LEN                        ((volatile uint32_t*)0xA4600058)
#define PI_BB_WR_LEN                        ((volatile uint32_t*)0xA460005C)
#define PI_BB_NAND_ADDR                     ((volatile uint32_t*)0xA4600070)
#define PI_BB_ATB_LOWER                     ((volatile uint32_t*)0xA4600500)

#define PI_BB_BUFFER_0                      ((volatile uint32_t*)0xA4610000)    ///< NAND buffer 0
#define PI_BB_BUFFER_1                      ((volatile uint32_t*)0xA4610200)    ///< NAND buffer 1
#define PI_BB_SPARE_0                       ((volatile uint32_t*)0xA4610400)    ///< NAND spare data 0
#define PI_BB_SPARE_1                       ((volatile uint32_t*)0xA4610410)    ///< NAND spare data 1
#define PI_BB_AES_KEY                       ((volatile uint32_t*)0xA4610420)    ///< AES expanded key
#define PI_BB_AES_IV                        ((volatile uint32_t*)0xA46104D0)    ///< AES initialization vector

#define PI_BB_NAND_CTRL_BUSY                (1 << 31)
#define PI_BB_NAND_CTRL_ERROR               (1 << 10)

#define PI_BB_WNAND_CTRL_CMD_SHIFT          16
#define PI_BB_WNAND_CTRL_LEN_SHIFT          0
#define PI_BB_WNAND_CTRL_MULTICYCLE         (1 << 10)
#define PI_BB_WNAND_CTRL_ECC                (1 << 11)
#define PI_BB_WNAND_CTRL_BUF(n)             ((n) << 14)
#define PI_BB_WNAND_CTRL_INTERRUPT          (1 << 30)
#define PI_BB_WNAND_CTRL_EXECUTE            (1 << 31)

#define PI_BB_WATB_UPPER_IVSOURCE           (1 << 8)

#define PI_BB_ATB_MAX_ENTRIES               192

typedef enum {
    NAND_CMD_READ1_H0   = (0x00 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<28) | (1<<27) | (1<<26)| (1<<25) | (1<<24) | (1<<15),
    NAND_CMD_READ1_H1   = (0x01 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<28) | (1<<27) | (1<<26)| (1<<25) | (1<<24) | (1<<15),
    NAND_CMD_RESET      = (0xFF << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_READID     = (0x90 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<28) | (1<<24),
    NAND_CMD_PAGEPROG_A = (0x80 << PI_BB_WNAND_CTRL_CMD_SHIFT) | PI_BB_WNAND_CTRL_MULTICYCLE | (1<<29) | (1<<27) | (1<<26) | (1<<25) | (1<<24),
    NAND_CMD_PAGEPROG_B = (0x10 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<15),
    NAND_CMD_COPYBACK_A = (0x00 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_COPYBACK_B = (0x8A << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_COPYBACK_C = (0x10 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_ERASE_A    = (0x60 << PI_BB_WNAND_CTRL_CMD_SHIFT) | PI_BB_WNAND_CTRL_MULTICYCLE | (1<<27) | (1<<26)| (1<<25),
    NAND_CMD_ERASE_B    = (0xD0 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<15),
    NAND_CMD_READSTATUS = (0x70 << PI_BB_WNAND_CTRL_CMD_SHIFT)
} nand_cmd_t;

static bool nand_inited;
static uint32_t nand_size;

static void nand_write_intbuffer(int bufidx, int offset, const void *data, int len)
{
    dma_wait();
    *PI_DRAM_ADDR = PhysicalAddr(data);
    *PI_CART_ADDR = offset + bufidx * 0x200;
    *PI_BB_RD_LEN = len;
    dma_wait();
}

static void nand_read_intbuffer(int bufidx, int offset, void *data, int len)
{
    dma_wait();
    *PI_DRAM_ADDR = PhysicalAddr(data);
    *PI_CART_ADDR = offset + bufidx * 0x200;
    *PI_BB_WR_LEN = len;
    dma_wait();
}

static void nand_cmd_wait(void)
{
    while (*PI_BB_NAND_CTRL & PI_BB_NAND_CTRL_BUSY) {}
}

static void nand_cmd_readid(int bufidx)
{
    *PI_BB_NAND_ADDR = 0;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) | 
        NAND_CMD_READID | (4 << PI_BB_WNAND_CTRL_LEN_SHIFT);
    nand_cmd_wait();
}

static void nand_cmd_read1(int bufidx, uint32_t addr, int len)
{
    assert(len > 0 && len <= 512);
    *PI_BB_NAND_ADDR = addr;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) | 
        ((addr & 0x100) ? NAND_CMD_READ1_H1 : NAND_CMD_READ1_H0) | 
        (len << PI_BB_WNAND_CTRL_LEN_SHIFT);
    nand_cmd_wait();
}

static void nand_cmd_pageprog(int bufidx, uint32_t addr, int len)
{
    *PI_BB_NAND_ADDR = addr;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) | 
        NAND_CMD_PAGEPROG_A | (len << PI_BB_WNAND_CTRL_LEN_SHIFT);
    nand_cmd_wait();
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) | 
        NAND_CMD_PAGEPROG_B;
    nand_cmd_wait();
}

static void nand_cmd_erase(uint32_t addr)
{
    *PI_BB_NAND_ADDR = addr;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | NAND_CMD_ERASE_A;
    nand_cmd_wait();
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | NAND_CMD_ERASE_B;
    nand_cmd_wait();
}

void nand_read_id(uint8_t id[4])
{
    uint8_t aligned_buf[16] __attribute__((aligned(16)));
    data_cache_hit_invalidate(aligned_buf, 16);
    
    const int bufidx = 0;
    nand_cmd_readid(bufidx);
    nand_read_intbuffer(bufidx, 0, aligned_buf, 4);

    memcpy(id, aligned_buf, 4);
}

static uint8_t io_read8(uint32_t addr)
{
    uint32_t data = io_read(addr & ~3);
    return (data >> ((~addr & 3) * 8)) & 0xFF;
}

static void io_write8(uint32_t addr, uint8_t data)
{
    uint32_t mask = 0xFF << ((~addr & 3) * 8);
    uint32_t shift = (~addr & 3) * 8;
    uint32_t old_data = io_read(addr & ~3);
    uint32_t new_data = (old_data & ~mask) | ((uint32_t)data << shift);
    io_write(addr & ~3, new_data);
}

void nand_init(void)
{
    assertf(sys_bbplayer(), "NAND is only present on iQue Player");
    if (nand_inited) return;

    uint8_t id[4];
    *PI_BB_NAND_CFG = 0x753E3EFF;
    nand_read_id(id);

    uint16_t id16 = (id[0] << 8) | id[1];
    switch (id16) {
    default:
        debugf("Unknown NAND ID: %04X", id16);
        break;
    case 0xEC76: // K9F1208U0M
    case 0x2076: // NAND512W3A
        *PI_BB_NAND_CFG = 0x441F1F3F;
        nand_size = 64 * 1024 * 1024;
        break;
    case 0x9876: // TC58512FT
        *PI_BB_NAND_CFG = 0x753E1F3F;
        nand_size = 64 * 1024 * 1024;
        break;
    case 0xEC79: // K9K1G08U0B
        *PI_BB_NAND_CFG = 0x441F1F3F;
        nand_size = 128 * 1024 * 1024;
        break;
    }

    nand_inited = true;
}

int nand_get_size(void)
{
    assertf(nand_inited, "nand_init() must be called first");
    return nand_size;
}

int nand_read_data(nand_addr_t addr, void *buf, int len) 
{
    assertf(nand_inited, "nand_init() must be called first");

    int bufidx = 0;
    uint8_t *buffer = buf;

    while (len > 0) {
        int offset = NAND_ADDR_OFFSET(addr);
        int read_len = MIN(len, NAND_PAGE_SIZE - offset);
        nand_cmd_read1(bufidx, addr, read_len);
        for (int i=0; i<read_len; i++)
            buffer[i] = io_read8((uint32_t)PI_BB_BUFFER_0 + bufidx*0x200 + offset + i);

        addr += read_len;
        buffer += read_len;
        len -= read_len;
    }

    return 0;
}

int nand_write_data(nand_addr_t addr, const void *buf, int len)
{
    assertf(nand_inited, "nand_init() must be called first");
    assertf(addr % NAND_PAGE_SIZE == 0, "NAND address must be page-aligned (0x%08lX)", addr);

    int bufidx = 0;
    const uint8_t *buffer = buf;

    while (len > 0) {
        int offset = NAND_ADDR_OFFSET(addr);
        int write_len = MIN(len, NAND_PAGE_SIZE - offset);

        // Write the data to the buffer
        for (int i=0; i<write_len; i++)
            io_write8((uint32_t)PI_BB_BUFFER_0 + bufidx*0x200 + offset + i, buffer[i]);

        // Write the page to the NAND
        nand_cmd_pageprog(bufidx, addr, write_len);

        addr += write_len;
        buffer += write_len;
        len -= write_len;
    }

    return 0;
}

int nand_erase_block(nand_addr_t addr)
{
    assertf(nand_inited, "nand_init() must be called first");
    assertf(addr % NAND_BLOCK_SIZE == 0, "NAND address must be block-aligned (0x%08lX)", addr);

    nand_cmd_erase(addr);
    return 0;
}

static void atb_write(int idx, uint32_t pi_address, int nand_block, int num_blocks_log2)
{
    assertf(num_blocks_log2 > 0 && num_blocks_log2 <= 16, "invalid ATB entry size: %d", 1<<num_blocks_log2);
    assertf((pi_address & (((1 << num_blocks_log2) * NAND_BLOCK_SIZE)-1)) == 0, 
        "wrong ATB alignment (addr:0x%08lX, nlog2:%d)", pi_address, num_blocks_log2);

    *PI_BB_ATB_UPPER = num_blocks_log2-1;
    PI_BB_ATB_LOWER[idx] = (nand_block << 16) | (pi_address / NAND_BLOCK_SIZE);
}

static void atb_write_ivsource(int idx, uint32_t pi_address)
{
    assertf((pi_address & (NAND_BLOCK_SIZE-1)) == 0,
        "wrong ATB alignment (addr:0x%08lX)", pi_address);

    *PI_BB_ATB_UPPER = PI_BB_WATB_UPPER_IVSOURCE;
    PI_BB_ATB_LOWER[idx] = pi_address / NAND_BLOCK_SIZE;
}

int nand_mmap(uint32_t pi_address, int16_t *blocks, int *atb_idx_ptr, int flags)
{
    int nseq;
    int atb_idx = atb_idx_ptr ? *atb_idx_ptr : 0;

    assertf(nand_inited, "nand_init() must be called first");
    assertf(pi_address >> 30 == 0, "Allowed PI addresses are in range [0 .. 0x3FFFFFFF] (0x%08lX)", pi_address);
    assertf(pi_address % NAND_BLOCK_SIZE == 0, "PI address must be block-aligned (0x%08lX)", pi_address);

    if (flags & NAND_MMAP_ENCRYPTED) {
        if (atb_idx >= PI_BB_ATB_MAX_ENTRIES)
            return -1;
        atb_write_ivsource(atb_idx++, pi_address - NAND_BLOCK_SIZE);
    }

    while (*blocks != -1) {
        // Calculate how many consecutive blocks we can map
        int bidx = *blocks;

        nseq = 1;
        while (blocks[nseq] == bidx + nseq) nseq++;
        blocks += nseq;

        // Map this sequence as subsequent ATB entries 
        while (nseq > 0) {
            if (atb_idx >= PI_BB_ATB_MAX_ENTRIES)
                return -1;

            // The longest sequence we can map is limited by the PI address
            // alignment. For instance, given a PI address of 0x10010000,
            // we can only map 0x10000 bytes (4 blocks) in a single ATB entry.
            int nseq_log2 = 32 - __builtin_clz(nseq);
            int piaddr_align = __builtin_ctz(pi_address / NAND_BLOCK_SIZE);
            int n_log2 = MIN(MIN(nseq_log2, piaddr_align), 16);
            int n = 1 << n_log2;

            // Write the ATB entry
            atb_write(atb_idx++, pi_address, bidx, n_log2);

            bidx += n;
            nseq -= n;
            pi_address += n * NAND_BLOCK_SIZE;
        }
    }

    // ATB requires a read command to be programmed into PI_BB_NAND_CTRL.
    *PI_BB_NAND_CTRL = NAND_CMD_READ1_H0;

    if (atb_idx_ptr) *atb_idx_ptr = atb_idx;
    return 0;
}
