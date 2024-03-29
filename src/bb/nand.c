#include <stdint.h>
#include <assert.h>
#include "n64sys.h"
#include "debug.h"
#include "dma.h"
#include "utils.h"

#define PI_DRAM_ADDR                        ((volatile uint32_t*)0xA4600000)
#define PI_CART_ADDR                        ((volatile uint32_t*)0xA4600004)
#define PI_BB_NAND_CTRL                     ((volatile uint32_t*)0xA4600048)
#define PI_BB_RD_LEN                        ((volatile uint32_t*)0xA4600058)
#define PI_BB_WR_LEN                        ((volatile uint32_t*)0xA460005C)
#define PI_BB_NAND_ADDR                     ((volatile uint32_t*)0xA4600070)

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

typedef enum {
    NAND_CMD_READ1_H0   = (0x00 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<28) | (1<<27) | (1<<26)| (1<<25) | (1<<24) | (1<<15),
    NAND_CMD_READ1_H1   = (0x01 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<28) | (1<<27) | (1<<26)| (1<<25) | (1<<24) | (1<<15),
    NAND_CMD_RESET      = (0xFF << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_READID     = (0x90 << PI_BB_WNAND_CTRL_CMD_SHIFT) | (1<<28) | (1<<24),
    NAND_CMD_PAGEPROG_A = (0x80 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_PAGEPROG_B = (0x10 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_COPYBACK_A = (0x00 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_COPYBACK_B = (0x8A << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_COPYBACK_C = (0x10 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_ERASE_A    = (0x60 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_ERASE_B    = (0xD0 << PI_BB_WNAND_CTRL_CMD_SHIFT),
    NAND_CMD_READSTATUS = (0x70 << PI_BB_WNAND_CTRL_CMD_SHIFT)
} nand_cmd_t;

typedef uint32_t nand_addr_t;

#define NAND_PAGE_SIZE          0x200
#define NAND_BLOCK_SIZE         0x4000

#define ADDR_OFFSET(addr)       ((addr) & 0x1FF)
#define ADDR_PAGE(addr)         (((addr) >> 9) & 0x1F)
#define ADDR_BLOCK(addr)        (((addr) >> 14) & 0xFFF)

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

void nand_read_data(nand_addr_t addr, uint8_t *buffer, int len) 
{
    int bufidx = 0;

    while (len > 0) {
        int read_len = MIN(len, NAND_PAGE_SIZE - ADDR_OFFSET(addr));
        nand_cmd_read1(bufidx, addr, read_len);
        for (int i=0; i<read_len; i++)
            buffer[i] = io_read8((uint32_t)PI_BB_BUFFER_0 + i);

        addr += read_len;
        buffer += read_len;
        len -= read_len;
    }
}
