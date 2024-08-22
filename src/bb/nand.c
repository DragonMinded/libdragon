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

#define PI_BB_BUFFER_0                      ((volatile uint32_t*)0xA4610000)    ///< NAND buffer 0
#define PI_BB_BUFFER_1                      ((volatile uint32_t*)0xA4610200)    ///< NAND buffer 1
#define PI_BB_SPARE_0                       ((volatile uint32_t*)0xA4610400)    ///< NAND spare data 0
#define PI_BB_SPARE_1                       ((volatile uint32_t*)0xA4610410)    ///< NAND spare data 1
#define PI_BB_AES_KEY                       ((volatile uint32_t*)0xA4610420)    ///< AES expanded key
#define PI_BB_AES_IV                        ((volatile uint32_t*)0xA46104D0)    ///< AES initialization vector
#define PI_BB_ATB_LOWER                     ((volatile uint32_t*)0xA4610500)

#define PI_BB_NAND_CTRL_BUSY                (1 << 31)
#define PI_BB_NAND_CTRL_ECC_ERROR           (1 << 10)
#define PI_BB_NAND_CTRL_ECC_CORRECTED       (1 << 11)

#define PI_BB_WNAND_CTRL_CMD_SHIFT          16
#define PI_BB_WNAND_CTRL_LEN_SHIFT          0
#define PI_BB_WNAND_CTRL_MULTICYCLE         (1 << 10)
#define PI_BB_WNAND_CTRL_ECC                (1 << 11)
#define PI_BB_WNAND_CTRL_BUF(n)             ((n) << 14)
#define PI_BB_WNAND_CTRL_INTERRUPT          (1 << 30)
#define PI_BB_WNAND_CTRL_EXECUTE            (1 << 31)

#define PI_BB_WATB_UPPER_DMAREAD            (1 << 4)        ///< This ATB entry will be enabled for DMA
#define PI_BB_WATB_UPPER_CPUREAD            (1 << 5)        ///< This ATB entry will be enabled for CPU read
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
static int mmap_atb_idx = -1;

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

static void nand_cmd_read1(int bufidx, uint32_t addr, int len, bool ecc)
{
    assert(len > 0 && len <= 512+16);
    *PI_BB_NAND_ADDR = addr;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) | 
        ((addr & 0x100) ? NAND_CMD_READ1_H1 : NAND_CMD_READ1_H0) | 
        (ecc ? PI_BB_WNAND_CTRL_ECC : 0) |
        (len << PI_BB_WNAND_CTRL_LEN_SHIFT);
    nand_cmd_wait();
}

static void nand_cmd_pageprog(int bufidx, uint32_t addr, bool ecc)
{
    *PI_BB_NAND_ADDR = addr;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) |
        (ecc ? PI_BB_WNAND_CTRL_ECC : 0) |
        NAND_CMD_PAGEPROG_A | ((NAND_PAGE_SIZE+16) << PI_BB_WNAND_CTRL_LEN_SHIFT);
    nand_cmd_wait();
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) |
        (ecc ? PI_BB_WNAND_CTRL_ECC : 0) |
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
        nand_cmd_read1(bufidx, addr, read_len, false);
        for (int i=0; i<read_len; i++)
            buffer[i] = io_read8((uint32_t)PI_BB_BUFFER_0 + bufidx*0x200 + offset + i);

        addr += read_len;
        buffer += read_len;
        len -= read_len;
    }

    return 0;
}

int nand_read_pages(nand_addr_t addr, int npages, void *buf, void *spare, bool ecc)
{
    assertf(nand_inited, "nand_init() must be called first");
    assertf(addr % NAND_PAGE_SIZE == 0, "NAND address must be page-aligned (0x%08lX)", addr);

    int bufidx = 0;
    uint8_t *buffer = buf;
    uint8_t *spare_data = spare;

    for (int i=0; i<npages; i++) {
        // Read the page from the NAND. Notice that if ECC is requested, it is mandatory to read
        // the spares even if we will not return them to the caller, because otherwise the
        // controller is unable to perform ECC calculation.
        nand_cmd_read1(bufidx, addr, NAND_PAGE_SIZE + (spare || ecc ? 16 : 0), ecc);
        addr += NAND_PAGE_SIZE;

        // If ECC detected an unrecoverable error, abort reading. This will only
        // be set if ECC was requested in the first place.
        if (*PI_BB_NAND_CTRL & PI_BB_NAND_CTRL_ECC_ERROR)
            return -1;

        // Copy data into output buffer
        for (int i=0; i<NAND_PAGE_SIZE; i++)
            buffer[i] = io_read8((uint32_t)PI_BB_BUFFER_0 + bufidx*0x200 + i);
        buffer += NAND_PAGE_SIZE;

        // Copy spare into output buffer
        if (spare_data) {
            for (int i=0; i<16; i++)
                spare_data[i] = io_read8((uint32_t)PI_BB_SPARE_0 + bufidx*4 + i);
            spare_data += 16;
        }
    }

    return npages;
}

int nand_write_pages(nand_addr_t addr, int npages, const void *buf, bool ecc)
{
    assertf(nand_inited, "nand_init() must be called first");
    assertf(addr % NAND_PAGE_SIZE == 0, "NAND address must be page-aligned (0x%08lX)", addr);

    int bufidx = 0;
    const uint8_t *buffer = buf;

    for (int i=0; i<npages; i++) {
        // Write the data to the buffer
        for (int i=0; i<NAND_PAGE_SIZE; i++)
            io_write8((uint32_t)PI_BB_BUFFER_0 + bufidx*0x200 + i, buffer[i]);

        // Put empty spare data into the buffer
        PI_BB_SPARE_0[bufidx*4 + 0] = 0xFFFFFFFF;
        PI_BB_SPARE_0[bufidx*4 + 1] = 0xFFFFFFFF;
        PI_BB_SPARE_0[bufidx*4 + 2] = 0xFFFFFFFF;
        PI_BB_SPARE_0[bufidx*4 + 3] = 0xFFFFFFFF;

        // Write the page to the NAND
        nand_cmd_pageprog(bufidx, addr, ecc);

        addr += NAND_PAGE_SIZE;
        buffer += NAND_PAGE_SIZE;
    }

    return npages;
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
    assertf(num_blocks_log2 >= 0 && num_blocks_log2 < 16, "invalid ATB entry size: %d", 1<<num_blocks_log2);
    assertf((pi_address & (((1 << num_blocks_log2) * NAND_BLOCK_SIZE)-1)) == 0, 
        "wrong ATB alignment (addr:0x%08lX, nlog2:%d)", pi_address, num_blocks_log2);

    *PI_BB_ATB_UPPER = num_blocks_log2 | PI_BB_WATB_UPPER_DMAREAD | PI_BB_WATB_UPPER_CPUREAD;
    PI_BB_ATB_LOWER[idx] = (nand_block << 16) | (pi_address / NAND_BLOCK_SIZE);
}

static void atb_write_ivsource(int idx, uint32_t pi_address)
{
    assertf((pi_address & (NAND_BLOCK_SIZE-1)) == 0,
        "wrong ATB alignment (addr:0x%08lX)", pi_address);

    *PI_BB_ATB_UPPER = PI_BB_WATB_UPPER_IVSOURCE | PI_BB_WATB_UPPER_DMAREAD | PI_BB_WATB_UPPER_CPUREAD;
    PI_BB_ATB_LOWER[idx] = pi_address / NAND_BLOCK_SIZE;
}

void nand_mmap_begin(void)
{
    assertf(mmap_atb_idx == -1, "nand_mmap_end() was not called");
    mmap_atb_idx = 0;
}

int nand_mmap(uint32_t pi_address, int16_t *blocks, nand_mmap_flags_t flags)
{
    assertf(nand_inited, "nand_init() must be called first");
    assertf(mmap_atb_idx != -1, "nand_mmap_begin() was not called");
    assertf(pi_address >> 30 == 0, "Allowed PI addresses are in range [0 .. 0x3FFFFFFF] (0x%08lX)", pi_address);
    assertf(pi_address % NAND_BLOCK_SIZE == 0, "PI address must be block-aligned (0x%08lX)", pi_address);

    if (mmap_atb_idx > 0) {
        uint32_t prev_pi_address = PI_BB_ATB_LOWER[mmap_atb_idx-1] * NAND_BLOCK_SIZE;
        assertf(pi_address >= prev_pi_address, "PI addresses must be in increasing order (0x%08lX < 0x%08lX)", pi_address, prev_pi_address);
    }

    if (flags & NAND_MMAP_ENCRYPTED) {
        if (mmap_atb_idx >= PI_BB_ATB_MAX_ENTRIES)
            return -1;
        atb_write_ivsource(mmap_atb_idx++, pi_address - NAND_BLOCK_SIZE);
    }

    while (*blocks != -1) {
        // Calculate how many consecutive blocks we can map
        int bidx = *blocks;

        int nseq = 1;
        while (blocks[nseq] == bidx + nseq) nseq++;
        blocks += nseq;

        // Map this sequence as subsequent ATB entries 
        while (nseq > 0) {
            if (mmap_atb_idx >= PI_BB_ATB_MAX_ENTRIES)
                return -1;

            // The longest sequence we can map is limited by the PI address
            // alignment. For instance, given a PI address of 0x10010000,
            // we can only map 0x10000 bytes (4 blocks) in a single ATB entry.
            int nseq_log2 = 31 - __builtin_clz(nseq);
            int piaddr_align = __builtin_ctz(pi_address / NAND_BLOCK_SIZE);
            int n_log2 = MIN(MIN(nseq_log2, piaddr_align), 15);
            int n = 1 << n_log2;

            // Write the ATB entry
            atb_write(mmap_atb_idx++, pi_address, bidx, n_log2);

            bidx += n;
            nseq -= n;
            pi_address += n * NAND_BLOCK_SIZE;
        }
    }

    return 0;
}

void nand_mmap_end(void)
{
    assertf(mmap_atb_idx != -1, "nand_mmap_begin() was not called");
 
    // Fill all ATB entries with increasing addresses
    if (mmap_atb_idx < PI_BB_ATB_MAX_ENTRIES) {
        uint32_t pi_address = 0;
        if (mmap_atb_idx > 0)
            pi_address = PI_BB_ATB_LOWER[mmap_atb_idx-1] * NAND_BLOCK_SIZE + NAND_BLOCK_SIZE;
        for (; mmap_atb_idx < PI_BB_ATB_MAX_ENTRIES; mmap_atb_idx++) {
            atb_write(mmap_atb_idx, pi_address, 0, 0);
            pi_address += NAND_BLOCK_SIZE;
        }
    }
 
    // ATB requires a read command to be programmed into PI_BB_NAND_CTRL.
    *PI_BB_NAND_CTRL = NAND_CMD_READ1_H0;
    mmap_atb_idx = -1;
}

void nand_compute_page_ecc(const uint8_t *buf, uint8_t *ecc)
{
    // This function implements the ECC algorithm used in SmartMedia, and in
    // some flash filesystems like YAFFS2. Another C implementation can be found
    // in the YAFFS2 source code:
    //    https://github.com/m-labs/rtems-yaffs2/blob/23be2ac372173ecf701a0ab179c510ca54b678d0/yaffs_ecc.c

    // ECC parity table, calculated via gen-ecc.py. This parity table contains
    // various parity bits for each byte of the input data. Bit 1 is always 0,
    // and bit 0 is the parity of the while byte.
    // The table is vertically symmetric (when viewed as a 16x16 matrix), so
    // we store only the first half, and the mirror vertically the accesses for
    // the second half.
    static const uint8_t ecc_table[128] = {
        0x00, 0x55, 0x59, 0x0c, 0x65, 0x30, 0x3c, 0x69, 0x69, 0x3c, 0x30, 0x65, 0x0c, 0x59, 0x55, 0x00,
        0x95, 0xc0, 0xcc, 0x99, 0xf0, 0xa5, 0xa9, 0xfc, 0xfc, 0xa9, 0xa5, 0xf0, 0x99, 0xcc, 0xc0, 0x95,
        0x99, 0xcc, 0xc0, 0x95, 0xfc, 0xa9, 0xa5, 0xf0, 0xf0, 0xa5, 0xa9, 0xfc, 0x95, 0xc0, 0xcc, 0x99,
        0x0c, 0x59, 0x55, 0x00, 0x69, 0x3c, 0x30, 0x65, 0x65, 0x30, 0x3c, 0x69, 0x00, 0x55, 0x59, 0x0c,
        0xa5, 0xf0, 0xfc, 0xa9, 0xc0, 0x95, 0x99, 0xcc, 0xcc, 0x99, 0x95, 0xc0, 0xa9, 0xfc, 0xf0, 0xa5,
        0x30, 0x65, 0x69, 0x3c, 0x55, 0x00, 0x0c, 0x59, 0x59, 0x0c, 0x00, 0x55, 0x3c, 0x69, 0x65, 0x30,
        0x3c, 0x69, 0x65, 0x30, 0x59, 0x0c, 0x00, 0x55, 0x55, 0x00, 0x0c, 0x59, 0x30, 0x65, 0x69, 0x3c,
        0xa9, 0xfc, 0xf0, 0xa5, 0xcc, 0x99, 0x95, 0xc0, 0xc0, 0x95, 0x99, 0xcc, 0xa5, 0xf0, 0xfc, 0xa9,
    };

    for (int j=0; j<2; j++) {
        uint8_t ecc2=0;
        uint32_t l0=0, l1=0;

        for (int i=0; i<256; i++) {
            // Lookup the parity table (with vertical symmetry for the second half)
            uint8_t val = *buf++;
            val = ecc_table[val < 128 ? val : val ^ 0xf0];
            // If byte has odd parity, update the line parity 
            if (val & 1) { l0 ^= i; l1 ^= ~i; val ^= 1; }
            // Update column parity
            ecc2 ^= val;
        }

        // Interleave the bits of the two line parity (l0 and l1)
        l0 = (l0 | (l0 << 4)) & 0x0F0F;
        l0 = (l0 | (l0 << 2)) & 0x3333;
        l0 = (l0 | (l0 << 1)) & 0x5555;
        l1 &= 0xFF;
        l1 = (l1 | (l1 << 4)) & 0x0F0F;
        l1 = (l1 | (l1 << 2)) & 0x3333;
        l1 = (l1 | (l1 << 1)) & 0x5555;
        l0 = (l1 << 0) | (l0 << 1);

        // Store the inverted line parity and column parity
        *ecc++ = ~l0 >> 0;
        *ecc++ = ~l0 >> 8;
        *ecc++ = ~ecc2;
    }
}
