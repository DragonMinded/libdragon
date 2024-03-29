#include <stdint.h>

#define PI_DRAM_ADDR                        ((volatile uint32_t*)0xA4600000)
#define PI_CART_ADDR                        ((volatile uint32_t*)0xA4600004)
#define PI_BB_NAND_CTRL                     ((volatile uint32_t*)0xA4600048)
#define PI_BB_RD_LEN                        ((volatile uint32_t*)0xA4600058)
#define PI_BB_WR_LEN                        ((volatile uint32_t*)0xA460005C)
#define PI_BB_NAND_ADDR                     ((volatile uint32_t*)0xA4600070)

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
    NAND_CMD_READ1      = (0x00 << PI_BB_WNAND_CTRL_CMD_SHIFT),
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

static void nand_write_intbuffer(int bufidx, int offset, const void *data, int len)
{
    data_cache_writeback_invalidate(data, len);
    dma_wait();
    *PI_DRAM_ADDR = PhysicalAddr(data);
    *PI_CART_ADDR = offset;
    *PI_BB_RD_LEN = len;
    dma_wait();
}

static void nand_read_intbuffer(int bufidx, int offset, void *data, int len)
{
    data_cache_writeback_invalidate(data, len);
    dma_wait();
    *PI_DRAM_ADDR = PhysicalAddr(data);
    *PI_CART_ADDR = offset;
    *PI_BB_RD_LEN = len;
    dma_wait();
}

static void nand_cmd_readid(int bufidx)
{
    *PI_BB_NAND_ADDR = 0;
    *PI_BB_NAND_CTRL = PI_BB_WNAND_CTRL_EXECUTE | PI_BB_WNAND_CTRL_BUF(bufidx) | 
        NAND_CMD_READID | (4 << PI_BB_WNAND_CTRL_LEN_SHIFT);
    dma_wait();
}

void nand_read_id(uint8_t id[4])
{
    const int bufidx = 0;
    nand_cmd_readid(bufidx);
    nand_read_intbuffer(bufidx, 0, id, 4);
}
