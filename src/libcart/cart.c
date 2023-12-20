/******************************************************************************/
/*  Adapted for use with libdragon - https://github.com/devwizard64/libcart   */
/******************************************************************************/

#include "n64types.h"
#include "n64sys.h"
#include "dma.h"
#include "libcart/cart.h"

#define PI_BASE_REG             0x04600000
#define PI_BSD_DOM1_LAT_REG     (PI_BASE_REG+0x14)
#define PI_BSD_DOM1_PWD_REG     (PI_BASE_REG+0x18)
#define PI_BSD_DOM1_PGS_REG     (PI_BASE_REG+0x1C)
#define PI_BSD_DOM1_RLS_REG     (PI_BASE_REG+0x20)
#define PI_BSD_DOM2_LAT_REG     (PI_BASE_REG+0x24)
#define PI_BSD_DOM2_PWD_REG     (PI_BASE_REG+0x28)
#define PI_BSD_DOM2_PGS_REG     (PI_BASE_REG+0x2C)
#define PI_BSD_DOM2_RLS_REG     (PI_BASE_REG+0x30)

#define IO_READ(addr)       (*(volatile uint32_t *)PHYS_TO_K1(addr))
#define IO_WRITE(addr,data) \
        (*(volatile uint32_t *)PHYS_TO_K1(addr) = (uint32_t)(data))

#define PHYS_TO_K1(x)   ((uint32_t)(x)|0xA0000000)

#define CART_ABORT()            {__cart_acs_rel(); return -1;}

/* Temporary buffer aligned for DMA */
__attribute__((aligned(16))) static uint64_t __cart_buf[512/8];

static uint32_t __cart_dom1_rel;
static uint32_t __cart_dom2_rel;
static uint32_t __cart_dom1;
static uint32_t __cart_dom2;

uint32_t cart_size;

static void __cart_acs_get(void)
{
    /* Save PI BSD configuration and reconfigure */
    if (__cart_dom1)
    {
        __cart_dom1_rel =
            IO_READ(PI_BSD_DOM1_LAT_REG) <<  0 |
            IO_READ(PI_BSD_DOM1_PWD_REG) <<  8 |
            IO_READ(PI_BSD_DOM1_PGS_REG) << 16 |
            IO_READ(PI_BSD_DOM1_RLS_REG) << 20 |
            1 << 31;
        IO_WRITE(PI_BSD_DOM1_LAT_REG, __cart_dom1 >>  0);
        IO_WRITE(PI_BSD_DOM1_PWD_REG, __cart_dom1 >>  8);
        IO_WRITE(PI_BSD_DOM1_PGS_REG, __cart_dom1 >> 16);
        IO_WRITE(PI_BSD_DOM1_RLS_REG, __cart_dom1 >> 20);
    }
    if (__cart_dom2)
    {
        __cart_dom2_rel =
            IO_READ(PI_BSD_DOM2_LAT_REG) <<  0 |
            IO_READ(PI_BSD_DOM2_PWD_REG) <<  8 |
            IO_READ(PI_BSD_DOM2_PGS_REG) << 16 |
            IO_READ(PI_BSD_DOM2_RLS_REG) << 20 |
            1 << 31;
        IO_WRITE(PI_BSD_DOM2_LAT_REG, __cart_dom2 >>  0);
        IO_WRITE(PI_BSD_DOM2_PWD_REG, __cart_dom2 >>  8);
        IO_WRITE(PI_BSD_DOM2_PGS_REG, __cart_dom2 >> 16);
        IO_WRITE(PI_BSD_DOM2_RLS_REG, __cart_dom2 >> 20);
    }
}

static void __cart_acs_rel(void)
{
    /* Restore PI BSD configuration */
    if (__cart_dom1_rel)
    {
        IO_WRITE(PI_BSD_DOM1_LAT_REG, __cart_dom1_rel >>  0);
        IO_WRITE(PI_BSD_DOM1_PWD_REG, __cart_dom1_rel >>  8);
        IO_WRITE(PI_BSD_DOM1_PGS_REG, __cart_dom1_rel >> 16);
        IO_WRITE(PI_BSD_DOM1_RLS_REG, __cart_dom1_rel >> 20);
        __cart_dom1_rel = 0;
    }
    if (__cart_dom2_rel)
    {
        IO_WRITE(PI_BSD_DOM2_LAT_REG, __cart_dom2_rel >>  0);
        IO_WRITE(PI_BSD_DOM2_PWD_REG, __cart_dom2_rel >>  8);
        IO_WRITE(PI_BSD_DOM2_PGS_REG, __cart_dom2_rel >> 16);
        IO_WRITE(PI_BSD_DOM2_RLS_REG, __cart_dom2_rel >> 20);
        __cart_dom2_rel = 0;
    }
}

static void __cart_dma_rd(void *dram, uint32_t cart, uint32_t size)
{
    data_cache_hit_writeback_invalidate(dram, size);
    dma_read_raw_async(dram, cart, size);
    dma_wait();
}

static void __cart_dma_wr(const void *dram, uint32_t cart, uint32_t size)
{
    data_cache_hit_writeback((void *)dram, size);
    dma_write_raw_async(dram, cart, size);
    dma_wait();
}

static void __cart_buf_rd(const void *addr)
{
    int i;
    const u_uint64_t *ptr = addr;
    for (i = 0; i < 512/8; i += 2)
    {
        uint64_t a = ptr[i+0];
        uint64_t b = ptr[i+1];
        __cart_buf[i+0] = a;
        __cart_buf[i+1] = b;
    }
}

static void __cart_buf_wr(void *addr)
{
    int i;
    u_uint64_t *ptr = addr;
    for (i = 0; i < 512/8; i += 2)
    {
        uint64_t a = __cart_buf[i+0];
        uint64_t b = __cart_buf[i+1];
        ptr[i+0] = a;
        ptr[i+1] = b;
    }
}

#define CMD0    (0x40| 0)
#define CMD1    (0x40| 1)
#define CMD2    (0x40| 2)
#define CMD3    (0x40| 3)
#define CMD7    (0x40| 7)
#define CMD8    (0x40| 8)
#define CMD9    (0x40| 9)
#define CMD12   (0x40|12)
#define CMD18   (0x40|18)
#define CMD25   (0x40|25)
#define CMD55   (0x40|55)
#define CMD58   (0x40|58)
#define ACMD6   (0x40| 6)
#define ACMD41  (0x40|41)

static unsigned char __sd_resp[17];
static unsigned char __sd_cfg;
static unsigned char __sd_type;
static unsigned char __sd_flag;

static int __sd_crc7(const char *src)
{
    int i;
    int n;
    int crc = 0;
    for (i = 0; i < 5; i++)
    {
        crc ^= src[i];
        for (n = 0; n < 8; n++)
        {
            if ((crc <<= 1) & 0x100) crc ^= 0x12;
        }
    }
    return (crc & 0xFE) | 1;
}

/* Thanks to anacierdem for this brilliant implementation. */

/* Spread lower 32 bits into 64 bits */
/* x =     **** **** **** **** abcd efgh ijkl mnop */
/* result: a0b0 c0d0 e0f0 g0h0 i0j0 k0l0 m0n0 o0p0 */
static uint64_t __sd_crc16_spread(uint64_t x)
{
    x = (x << 16 | x) & 0x0000FFFF0000FFFF;
    x = (x <<  8 | x) & 0x00FF00FF00FF00FF;
    x = (x <<  4 | x) & 0x0F0F0F0F0F0F0F0F;
    x = (x <<  2 | x) & 0x3333333333333333;
    x = (x <<  1 | x) & 0x5555555555555555;
    return x;
}

/* Shuffle 32 bits of two values into 64 bits */
/* x =     **** **** **** **** abcd efgh ijkl mnop */
/* y =     **** **** **** **** ABCD EFGH IJKL MNOP */
/* result: aAbB cCdD eEfF gGhH iIjJ kKlL mMnN oOpP */
static uint64_t __sd_crc16_shuffle(uint32_t x, uint32_t y)
{
    return __sd_crc16_spread(x) << 1 | __sd_crc16_spread(y);
}

static void __sd_crc16(uint64_t *dst, const uint64_t *src)
{
    int i;
    int n;
    uint64_t x;
    uint64_t y;
    uint32_t a;
    uint32_t b;
    uint16_t crc[4] = {0};
    for (i = 0; i < 512/8; i++)
    {
        x = src[i];
        /* Transpose every 2x2 bit block in the 8x8 matrix */
        /* abcd efgh     aick emgo */
        /* ijkl mnop     bjdl fnhp */
        /* qrst uvwx     qys0 u2w4 */
        /* yz01 2345  \  rzt1 v3x5 */
        /* 6789 ABCD  /  6E8G AICK */
        /* EFGH IJKL     7F9H BJDL */
        /* MNOP QRST     MUOW QYS? */
        /* UVWX YZ?!     NVPX RZT! */
        y = (x ^ (x >> 7)) & 0x00AA00AA00AA00AA;
        x ^= y ^ (y << 7);
        /* Transpose 2x2 blocks inside their 4x4 blocks in the 8x8 matrix */
        /* aick emgo     aiqy emu2 */
        /* bjdl fnhp     bjrz fnv3 */
        /* qys0 u2w4     cks0 gow4 */
        /* rzt1 v3x5  \  dlt1 hpx5 */
        /* 6E8G AICK  /  6EMU AIQY */
        /* 7F9H BJDL     7FNV BJRZ */
        /* MUOW QYS?     8GOW CKS? */
        /* NVPX RZT!     9HPX DLT! */
        y = (x ^ (x >> 14)) & 0x0000CCCC0000CCCC;
        x ^= y ^ (y << 14);
        /* Interleave */
        /* x =     aiqy 6EMU bjrz 7FNV cks0 8GOW dlt1 9HPX */
        /* y =     emu2 AIQY fnv3 BJRZ gow4 CKS? hpx5 DLT! */
        /* result: aeim quy2 6AEI MQUY bfjn rvz3 7BFJ NRVZ */
        /*         cgko sw04 8CGK OSW? dhlp tx15 9DHL PTX! */
        x = __sd_crc16_shuffle(
            (x >> 32 & 0xF0F0F0F0) | (x >> 4 & 0x0F0F0F0F),
            (x >> 28 & 0xF0F0F0F0) | (x >> 0 & 0x0F0F0F0F)
        );
        for (n = 3; n >= 0; n--)
        {
            a = crc[n];
            /* (crc >> 8) ^ dat[0] */
            b = ((x ^ a) >> 8) & 0xFF;
            b ^= b >> 4;
            a = (a << 8) ^ b ^ (b << 5) ^ (b << 12);
            /* (crc >> 8) ^ dat[1] */
            b = (x ^ (a >> 8)) & 0xFF;
            b ^= b >> 4;
            a = (a << 8) ^ b ^ (b << 5) ^ (b << 12);
            crc[n] = a;
            x >>= 16;
        }
    }
    /* Interleave CRC */
    x = __sd_crc16_shuffle(crc[0] << 16 | crc[1], crc[2] << 16 | crc[3]);
    *dst = __sd_crc16_shuffle(x >> 32, x);
}

int cart_type = CART_NULL;

int cart_init(void)
{
    static int (*const init[CART_MAX])(void) =
    {
        ci_init,
        edx_init,
        ed_init,
        sc_init,
    };
    int i;
    int result;
    if (!__cart_dom1)
    {
        __cart_dom1 = 0x8030FFFF;
        __cart_acs_get();
        __cart_dom1 = io_read(0x10000000);
        __cart_acs_rel();
    }
    if (!__cart_dom2) __cart_dom2 = __cart_dom1;
    if (cart_type < 0)
    {
        for (i = 0; i < CART_MAX; i++)
        {
            if ((result = init[i]()) >= 0)
            {
                cart_type = i;
                return result;
            }
        }
        return -1;
    }
    return init[cart_type]();
}

int cart_exit(void)
{
    static int (*const exit[CART_MAX])(void) =
    {
        ci_exit,
        edx_exit,
        ed_exit,
        sc_exit,
    };
    if (cart_type < 0) return -1;
    return exit[cart_type]();
}

int cart_card_init(void)
{
    static int (*const card_init[CART_MAX])(void) =
    {
        ci_card_init,
        edx_card_init,
        ed_card_init,
        sc_card_init,
    };
    if (cart_type < 0) return -1;
    return card_init[cart_type]();
}

int cart_card_rd_dram(void *dram, uint32_t lba, uint32_t count)
{
    static int (*const card_rd_dram[CART_MAX])(
        void *dram, uint32_t lba, uint32_t count
    ) =
    {
        ci_card_rd_dram,
        edx_card_rd_dram,
        ed_card_rd_dram,
        sc_card_rd_dram,
    };
    if (cart_type < 0) return -1;
    return card_rd_dram[cart_type](dram, lba, count);
}

char cart_card_byteswap;

int cart_card_rd_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    static int (*const card_rd_cart[CART_MAX])(
        uint32_t cart, uint32_t lba, uint32_t count
    ) =
    {
        ci_card_rd_cart,
        edx_card_rd_cart,
        ed_card_rd_cart,
        sc_card_rd_cart,
    };
    if (cart_type < 0) return -1;
    return card_rd_cart[cart_type](cart, lba, count);
}

int cart_card_wr_dram(const void *dram, uint32_t lba, uint32_t count)
{
    static int (*const card_wr_dram[CART_MAX])(
        const void *dram, uint32_t lba, uint32_t count
    ) =
    {
        ci_card_wr_dram,
        edx_card_wr_dram,
        ed_card_wr_dram,
        sc_card_wr_dram,
    };
    if (cart_type < 0) return -1;
    return card_wr_dram[cart_type](dram, lba, count);
}

int cart_card_wr_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    static int (*const card_wr_cart[CART_MAX])(
        uint32_t cart, uint32_t lba, uint32_t count
    ) =
    {
        ci_card_wr_cart,
        edx_card_wr_cart,
        ed_card_wr_cart,
        sc_card_wr_cart,
    };
    if (cart_type < 0) return -1;
    return card_wr_cart[cart_type](cart, lba, count);
}

#define CI_BASE_REG             0x18000000

#define CI_BUFFER_REG           (CI_BASE_REG+0x0000)
#define CI_SDRAM_ADDR_REG       (CI_BASE_REG+0x0004)

#define CI_STATUS_REG           (CI_BASE_REG+0x0200)
#define CI_COMMAND_REG          (CI_BASE_REG+0x0208)
#define CI_LBA_REG              (CI_BASE_REG+0x0210)
#define CI_LENGTH_REG           (CI_BASE_REG+0x0218)
#define CI_RESULT_REG           (CI_BASE_REG+0x0220)

#define CI_MAGIC_REG            (CI_BASE_REG+0x02EC)
#define CI_VARIANT_REG          (CI_BASE_REG+0x02F0)
#define CI_REVISION_REG         (CI_BASE_REG+0x02FC)

#define CI_STATUS_MASK          0xF000
#define CI_IDLE                 0x0000
#define CI_BUSY                 0x1000

#define CI_RD_BUFFER            0x01
#define CI_RD_SDRAM             0x03
#define CI_WR_BUFFER            0x10
#define CI_WR_SDRAM             0x13
#define CI_SD_RESET             0x1F
#define CI_BYTESWAP_OFF         0xE0
#define CI_BYTESWAP_ON          0xE1
#define CI_CARTROM_WR_ON        0xF0
#define CI_CARTROM_WR_OFF       0xF1
#define CI_EXT_ADDR_ON          0xF8
#define CI_EXT_ADDR_OFF         0xF9
#define CI_ABORT                0xFF

#define CI_MAGIC                0x55444556  /* UDEV */

#define CI_VARIANT_HW1          0x4100  /* A */
#define CI_VARIANT_HW2          0x4200  /* B */

static int __ci_sync(void)
{
    int n = 65536;
    do
    {
        if (--n == 0) return -1;
    }
    while (io_read(CI_STATUS_REG) & CI_STATUS_MASK);
    return 0;
}

int ci_init(void)
{
    __cart_acs_get();
    if (io_read(CI_MAGIC_REG) != CI_MAGIC) CART_ABORT();
    __ci_sync();
    io_write(CI_COMMAND_REG, CI_CARTROM_WR_ON);
    __ci_sync();
    io_write(CI_COMMAND_REG, CI_BYTESWAP_OFF);
    __ci_sync();
    cart_size = 0x4000000; /* 64 MiB */
    __cart_acs_rel();
    return 0;
}

int ci_exit(void)
{
    __cart_acs_get();
    __ci_sync();
    io_write(CI_COMMAND_REG, CI_CARTROM_WR_OFF);
    __ci_sync();
    __cart_acs_rel();
    return 0;
}

int ci_card_init(void)
{
    return 0;
}

int ci_card_rd_dram(void *dram, uint32_t lba, uint32_t count)
{
    char *addr = dram;
    __cart_acs_get();
    __ci_sync();
    while (count-- > 0)
    {
        io_write(CI_LBA_REG, lba);
        io_write(CI_COMMAND_REG, CI_RD_BUFFER);
        if (__ci_sync())
        {
            io_write(CI_COMMAND_REG, CI_ABORT);
            __ci_sync();
            io_write(CI_COMMAND_REG, CI_SD_RESET);
            __ci_sync();
            CART_ABORT();
        }
        if ((long)addr & 7)
        {
            __cart_dma_rd(__cart_buf, CI_BUFFER_REG, 512);
            __cart_buf_wr(addr);
        }
        else
        {
            __cart_dma_rd(addr, CI_BUFFER_REG, 512);
        }
        addr += 512;
        lba++;
    }
    __cart_acs_rel();
    return 0;
}

int ci_card_rd_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    __cart_acs_get();
    __ci_sync();
    if (cart_card_byteswap)
    {
        io_write(CI_COMMAND_REG, CI_BYTESWAP_ON);
        __ci_sync();
    }
    io_write(CI_LBA_REG, lba);
    io_write(CI_LENGTH_REG, count);
    io_write(CI_SDRAM_ADDR_REG, (cart & 0xFFFFFFF) >> 1);
    io_write(CI_COMMAND_REG, CI_RD_SDRAM);
    if (__ci_sync())
    {
        io_write(CI_COMMAND_REG, CI_ABORT);
        __ci_sync();
        io_write(CI_COMMAND_REG, CI_SD_RESET);
        __ci_sync();
        io_write(CI_COMMAND_REG, CI_BYTESWAP_OFF);
        __ci_sync();
        CART_ABORT();
    }
    if (cart_card_byteswap)
    {
        io_write(CI_COMMAND_REG, CI_BYTESWAP_OFF);
        __ci_sync();
    }
    __cart_acs_rel();
    return 0;
}

int ci_card_wr_dram(const void *dram, uint32_t lba, uint32_t count)
{
    const char *addr = dram;
    __cart_acs_get();
    __ci_sync();
    while (count-- > 0)
    {
        if ((long)addr & 7)
        {
            __cart_buf_rd(addr);
            __cart_dma_wr(__cart_buf, CI_BUFFER_REG, 512);
        }
        else
        {
            __cart_dma_wr(addr, CI_BUFFER_REG, 512);
        }
        io_write(CI_LBA_REG, lba);
        io_write(CI_COMMAND_REG, CI_WR_BUFFER);
        if (__ci_sync())
        {
            io_write(CI_COMMAND_REG, CI_ABORT);
            __ci_sync();
            io_write(CI_COMMAND_REG, CI_SD_RESET);
            __ci_sync();
            CART_ABORT();
        }
        addr += 512;
        lba++;
    }
    __cart_acs_rel();
    return 0;
}

int ci_card_wr_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    __cart_acs_get();
    __ci_sync();
    io_write(CI_LBA_REG, lba);
    io_write(CI_LENGTH_REG, count);
    io_write(CI_SDRAM_ADDR_REG, (cart & 0xFFFFFFF) >> 1);
    io_write(CI_COMMAND_REG, CI_WR_SDRAM);
    if (__ci_sync())
    {
        io_write(CI_COMMAND_REG, CI_ABORT);
        __ci_sync();
        io_write(CI_COMMAND_REG, CI_SD_RESET);
        __ci_sync();
        CART_ABORT();
    }
    __cart_acs_rel();
    return 0;
}

#define EDX_BASE_REG            0x1F800000

#define EDX_BOOT_CFG_REG        (EDX_BASE_REG+0x0010)
#define EDX_EDID_REG            (EDX_BASE_REG+0x0014)

#define EDX_SYS_CFG_REG         (EDX_BASE_REG+0x8000)
#define EDX_KEY_REG             (EDX_BASE_REG+0x8004)
#define EDX_DMA_STA_REG         (EDX_BASE_REG+0x8008)
#define EDX_DMA_ADDR_REG        (EDX_BASE_REG+0x8008)
#define EDX_DMA_LEN_REG         (EDX_BASE_REG+0x800C)
#define EDX_SDIO_REG            (EDX_BASE_REG+0x8020)
#define EDX_SDIO_ARD_REG        (EDX_BASE_REG+0x8200)
#define EDX_SD_CMD_RD_REG       (EDX_BASE_REG+0x8020)
#define EDX_SD_CMD_WR_REG       (EDX_BASE_REG+0x8024)
#define EDX_SD_DAT_RD_REG       (EDX_BASE_REG+0x8028)
#define EDX_SD_DAT_WR_REG       (EDX_BASE_REG+0x802C)
#define EDX_SD_STATUS_REG       (EDX_BASE_REG+0x8030)

#define EDX_BCFG_BOOTMOD        0x0001
#define EDX_BCFG_SD_INIT        0x0002
#define EDX_BCFG_SD_TYPE        0x0004
#define EDX_BCFG_GAMEMOD        0x0008
#define EDX_BCFG_CICLOCK        0x8000

#define EDX_DMA_STA_BUSY        0x0001
#define EDX_DMA_STA_ERROR       0x0002
#define EDX_DMA_STA_LOCK        0x0080

#define EDX_SD_CFG_BITLEN       0x000F
#define EDX_SD_CFG_SPD          0x0010
#define EDX_SD_STA_BUSY         0x0080

#define EDX_CFG_SDRAM_ON        0x0000
#define EDX_CFG_SDRAM_OFF       0x0001
#define EDX_CFG_REGS_OFF        0x0002
#define EDX_CFG_BYTESWAP        0x0004

#define EDX_KEY                 0xAA55

#define EDX_SD_CMD_RD           EDX_SD_CMD_RD_REG
#define EDX_SD_CMD_WR           EDX_SD_CMD_WR_REG
#define EDX_SD_DAT_RD           EDX_SD_DAT_RD_REG
#define EDX_SD_DAT_WR           EDX_SD_DAT_WR_REG

#define EDX_SD_CMD_8b           8
#define EDX_SD_CMD_1b           1
#define EDX_SD_DAT_16b          4
#define EDX_SD_DAT_8b           2
#define EDX_SD_DAT_4b           1

#define __edx_sd_dat_wr(val)    io_write(EDX_SD_DAT_WR_REG, (val) << 8 | 0xFF)

int edx_init(void)
{
    __cart_acs_get();
    io_write(EDX_KEY_REG, EDX_KEY);
    if (io_read(EDX_EDID_REG) >> 16 != 0xED64) CART_ABORT();
    io_write(EDX_SYS_CFG_REG, EDX_CFG_SDRAM_ON);
    __cart_dom1 = 0x80370C04;
    cart_size = 0x4000000; /* 64 MiB */
    __cart_acs_rel();
    return 0;
}

int edx_exit(void)
{
    __cart_acs_get();
    io_write(EDX_KEY_REG, 0);
    __cart_acs_rel();
    return 0;
}

static void __edx_sd_mode(uint32_t reg, int val)
{
    static uint32_t mode;
    if (mode != reg)
    {
        mode = reg;
        io_write(EDX_SD_STATUS_REG, __sd_cfg);
        io_write(reg, 0xFFFF);
        while (io_read(EDX_SD_STATUS_REG) & EDX_SD_STA_BUSY);
    }
    io_write(EDX_SD_STATUS_REG, __sd_cfg | val);
}

static uint32_t __edx_sd_cmd_rd(void)
{
    io_write(EDX_SD_CMD_RD_REG, 0xFFFF);
    while (io_read(EDX_SD_STATUS_REG) & EDX_SD_STA_BUSY);
    return io_read(EDX_SD_CMD_RD_REG);
}

static void __edx_sd_cmd_wr(uint32_t val)
{
    io_write(EDX_SD_CMD_WR_REG, val);
    while (io_read(EDX_SD_STATUS_REG) & EDX_SD_STA_BUSY);
}

static uint32_t __edx_sd_dat_rd(void)
{
    io_write(EDX_SD_DAT_RD_REG, 0xFFFF);
    return io_read(EDX_SD_DAT_RD_REG);
}

static int __edx_sd_cmd(int cmd, uint32_t arg)
{
    int i;
    int n;
    char buf[6];
    buf[0] = cmd;
    buf[1] = arg >> 24;
    buf[2] = arg >> 16;
    buf[3] = arg >>  8;
    buf[4] = arg >>  0;
    buf[5] = __sd_crc7(buf);
    /* Send the command */
    __edx_sd_mode(EDX_SD_CMD_WR, EDX_SD_CMD_8b);
    __edx_sd_cmd_wr(0xFF);
    for (i = 0; i < 6; i++) __edx_sd_cmd_wr(buf[i] & 0xFF);
    if (cmd == CMD18) return 0;
    /* Read the first response byte */
    __edx_sd_mode(EDX_SD_CMD_RD, EDX_SD_CMD_8b);
    __sd_resp[0] = __edx_sd_cmd_rd();
    __edx_sd_mode(EDX_SD_CMD_RD, EDX_SD_CMD_1b);
    n = 2048;
    while (__sd_resp[0] & 0xC0)
    {
        if (--n == 0) return -1;
        __sd_resp[0] = __edx_sd_cmd_rd();
    }
    /* Read the rest of the response */
    __edx_sd_mode(EDX_SD_CMD_RD, EDX_SD_CMD_8b);
    n = cmd == CMD2 || cmd == CMD9 ? 17 : 6;
    for (i = 1; i < n; i++) __sd_resp[i] = __edx_sd_cmd_rd();
    return 0;
}

static int __edx_sd_close(void)
{
    int n;
    /* CMD12: STOP_TRANSMISSION */
    if (__edx_sd_cmd(CMD12, 0) < 0) return -1;
    /* Wait for card */
    __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_4b);
    __edx_sd_dat_rd();
    __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_8b);
    __edx_sd_dat_rd();
    n = 65536;
    do
    {
        if (--n == 0) break;
    }
    while ((__edx_sd_dat_rd() & 0xFF) != 0xFF);
    return 0;
}

int edx_card_init(void)
{
    int i;
    int n;
    uint32_t rca;
    uint32_t boot_cfg;
    __cart_acs_get();
    /* Check if already init */
    boot_cfg = io_read(EDX_BOOT_CFG_REG);
    if (boot_cfg & EDX_BCFG_SD_INIT)
    {
        __sd_flag = boot_cfg & EDX_BCFG_SD_TYPE;
    }
    else
    {
        __sd_cfg = 0;
        /* Card needs 74 clocks, we do 80 */
        __edx_sd_mode(EDX_SD_CMD_WR, EDX_SD_CMD_8b);
        for (i = 0; i < 10; i++) __edx_sd_cmd_wr(0xFF);
        /* CMD0: GO_IDLE_STATE */
        __edx_sd_cmd(CMD0, 0);
        /* CMD8: SEND_IF_COND */
        /* If it returns an error, it is SD V1 */
        if (__edx_sd_cmd(CMD8, 0x1AA))
        {
            /* SD V1 */
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
                if (__edx_sd_cmd(CMD55, 0) < 0) CART_ABORT();
                if (__edx_sd_cmd(ACMD41, 0x40300000) < 0) CART_ABORT();
            }
            while (__sd_resp[1] == 0);
            __sd_flag = 0;
        }
        else
        {
            /* SD V2 */
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
                if (__edx_sd_cmd(CMD55, 0) < 0) CART_ABORT();
                if (!(__sd_resp[3] & 1)) continue;
                __edx_sd_cmd(ACMD41, 0x40300000);
            }
            while (!(__sd_resp[1] & 0x80));
            /* Card is SDHC */
            __sd_flag = __sd_resp[1] & 0x40;
        }
        /* CMD2: ALL_SEND_CID */
        if (__edx_sd_cmd(CMD2, 0) < 0) CART_ABORT();
        /* CMD3: SEND_RELATIVE_ADDR */
        if (__edx_sd_cmd(CMD3, 0) < 0) CART_ABORT();
        rca =
            __sd_resp[1] << 24 |
            __sd_resp[2] << 16 |
            __sd_resp[3] <<  8 |
            __sd_resp[4] <<  0;
        /* CMD9: SEND_CSD */
        if (__edx_sd_cmd(CMD9, rca) < 0) CART_ABORT();
        /* CMD7: SELECT_CARD */
        if (__edx_sd_cmd(CMD7, rca) < 0) CART_ABORT();
        /* ACMD6: SET_BUS_WIDTH */
        if (__edx_sd_cmd(CMD55, rca) < 0) CART_ABORT();
        if (__edx_sd_cmd(ACMD6, 2) < 0) CART_ABORT();
    }
    __sd_cfg = EDX_SD_CFG_SPD;
    __cart_acs_rel();
    return 0;
}

int edx_card_rd_dram(void *dram, uint32_t lba, uint32_t count)
{
    char *addr = dram;
    int n;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD18: READ_MULTIPLE_BLOCK */
    if (__edx_sd_cmd(CMD18, lba) < 0) CART_ABORT();
    while (count-- > 0)
    {
        /* Wait for card */
        __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_4b);
        n = 65536;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while (__edx_sd_dat_rd() & 0xF);
        /* Read data */
        __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_16b);
        if ((long)addr & 7)
        {
            __cart_dma_rd(__cart_buf, EDX_SDIO_ARD_REG, 512);
            __cart_buf_wr(addr);
        }
        else
        {
            __cart_dma_rd(addr, EDX_SDIO_ARD_REG, 512);
        }
        /* 4x16-bit CRC (8 byte) */
        /* We ignore the CRC */
        __cart_dma_rd(__cart_buf, EDX_SDIO_ARD_REG, 8);
        addr += 512;
    }
    if (__edx_sd_close()) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int edx_card_rd_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    uint32_t resp;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD18: READ_MULTIPLE_BLOCK */
    if (__edx_sd_cmd(CMD18, lba) < 0) CART_ABORT();
    if (cart_card_byteswap)
    {
        io_write(EDX_SYS_CFG_REG, EDX_CFG_SDRAM_ON|EDX_CFG_BYTESWAP);
    }
    io_write(EDX_DMA_ADDR_REG, cart & 0x3FFFFFF);
    io_write(EDX_DMA_LEN_REG, count);
    __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_16b);
    while ((resp = io_read(EDX_DMA_STA_REG)) & EDX_DMA_STA_BUSY)
    {
        if (resp & EDX_DMA_STA_ERROR)
        {
            io_write(EDX_SYS_CFG_REG, EDX_CFG_SDRAM_ON);
            CART_ABORT();
        }
    }
    if (cart_card_byteswap)
    {
        io_write(EDX_SYS_CFG_REG, EDX_CFG_SDRAM_ON);
    }
    if (__edx_sd_close()) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int edx_card_wr_dram(const void *dram, uint32_t lba, uint32_t count)
{
    const char *addr = dram;
    int i;
    int n;
    int resp;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD25: WRITE_MULTIPLE_BLOCK */
    if (__edx_sd_cmd(CMD25, lba) < 0) CART_ABORT();
    while (count-- > 0)
    {
        /* SD: start bit (why not only write F0?) */
        __edx_sd_mode(EDX_SD_DAT_WR, EDX_SD_DAT_8b);
        __edx_sd_dat_wr(0xFF);
        __edx_sd_dat_wr(0xF0);
        /* Write data and CRC */
        __edx_sd_mode(EDX_SD_DAT_WR, EDX_SD_DAT_16b);
        if ((long)addr & 7)
        {
            __cart_buf_rd(addr);
            __cart_dma_wr(__cart_buf, EDX_SDIO_ARD_REG, 512);
            __sd_crc16(__cart_buf, __cart_buf);
        }
        else
        {
            __cart_dma_wr(addr, EDX_SDIO_ARD_REG, 512);
            __sd_crc16(__cart_buf, (const uint64_t *)addr);
        }
        __cart_dma_wr(__cart_buf, EDX_SDIO_ARD_REG, 8);
        /* End bit */
        __edx_sd_mode(EDX_SD_DAT_WR, EDX_SD_DAT_4b);
        __edx_sd_dat_wr(0xFF);
        /* Wait for start of response */
        __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_4b);
        n = 1024;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while (__edx_sd_dat_rd() & 1);
        /* Read response */
        resp = 0;
        for (i = 0; i < 3; i++) resp = resp << 1 | (__edx_sd_dat_rd() & 1);
        if (resp != 2) CART_ABORT();
        /* Wait for card */
        n = 65536;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while ((__edx_sd_dat_rd() & 0xFF) != 0xFF);
        addr += 512;
    }
    if (__edx_sd_close()) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int edx_card_wr_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    int i;
    int n;
    int resp;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD25: WRITE_MULTIPLE_BLOCK */
    if (__edx_sd_cmd(CMD25, lba) < 0) CART_ABORT();
    while (count-- > 0)
    {
        /* SD: start bit (why not only write F0?) */
        __edx_sd_mode(EDX_SD_DAT_WR, EDX_SD_DAT_8b);
        __edx_sd_dat_wr(0xFF);
        __edx_sd_dat_wr(0xF0);
        /* Write data and CRC */
        __edx_sd_mode(EDX_SD_DAT_WR, EDX_SD_DAT_16b);
        __cart_dma_rd(__cart_buf, cart, 512);
        __cart_dma_wr(__cart_buf, EDX_SDIO_ARD_REG, 512);
        __sd_crc16(__cart_buf, __cart_buf);
        __cart_dma_wr(__cart_buf, EDX_SDIO_ARD_REG, 8);
        /* End bit */
        __edx_sd_mode(EDX_SD_DAT_WR, EDX_SD_DAT_4b);
        __edx_sd_dat_wr(0xFF);
        /* Wait for start of response */
        __edx_sd_mode(EDX_SD_DAT_RD, EDX_SD_DAT_4b);
        n = 1024;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while (__edx_sd_dat_rd() & 1);
        /* Read response */
        resp = 0;
        for (i = 0; i < 3; i++) resp = resp << 1 | (__edx_sd_dat_rd() & 1);
        if (resp != 2) CART_ABORT();
        /* Wait for card */
        n = 65536;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while ((__edx_sd_dat_rd() & 0xFF) != 0xFF);
        cart += 512;
    }
    if (__edx_sd_close()) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

#define ED_BASE_REG             0x08040000

#define ED_CFG_REG              (ED_BASE_REG+0x00)
#define ED_STATUS_REG           (ED_BASE_REG+0x04)
#define ED_DMA_LEN_REG          (ED_BASE_REG+0x08)
#define ED_DMA_ADDR_REG         (ED_BASE_REG+0x0C)
#define ED_MSG_REG              (ED_BASE_REG+0x10)
#define ED_DMA_CFG_REG          (ED_BASE_REG+0x14)
#define ED_SPI_REG              (ED_BASE_REG+0x18)
#define ED_SPI_CFG_REG          (ED_BASE_REG+0x1C)
#define ED_KEY_REG              (ED_BASE_REG+0x20)
#define ED_SAV_CFG_REG          (ED_BASE_REG+0x24)
#define ED_SEC_REG              (ED_BASE_REG+0x28)
#define ED_VER_REG              (ED_BASE_REG+0x2C)

#define ED_CFG_CNT_REG          (ED_BASE_REG+0x40)
#define ED_CFG_DAT_REG          (ED_BASE_REG+0x44)
#define ED_MAX_MSG_REG          (ED_BASE_REG+0x48)
#define ED_CRC_REG              (ED_BASE_REG+0x4C)

#define ED_DMA_SD_TO_RAM        1
#define ED_DMA_RAM_TO_SD        2
#define ED_DMA_FIFO_TO_RAM      3
#define ED_DMA_RAM_TO_FIFO      4

#define ED_CFG_SDRAM_OFF        (0 << 0)
#define ED_CFG_SDRAM_ON         (1 << 0)
#define ED_CFG_BYTESWAP         (1 << 1)

#define ED_STATE_DMA_BUSY       (1 << 0)
#define ED_STATE_DMA_TOUT       (1 << 1)
#define ED_STATE_TXE            (1 << 2)
#define ED_STATE_RXF            (1 << 3)
#define ED_STATE_SPI            (1 << 4)

#define ED_SPI_SPD_50           (0 << 0)
#define ED_SPI_SPD_25           (1 << 0)
#define ED_SPI_SPD_LO           (2 << 0)
#define ED_SPI_SS               (1 << 2)
#define ED_SPI_WR               (0 << 3)
#define ED_SPI_RD               (1 << 3)
#define ED_SPI_CMD              (0 << 4)
#define ED_SPI_DAT              (1 << 4)
#define ED_SPI_8BIT             (0 << 5)
#define ED_SPI_1BIT             (1 << 5)

#define ED_SAV_EEP_ON           (1 << 0)
#define ED_SAV_SRM_ON           (1 << 1)
#define ED_SAV_EEP_SIZE         (1 << 2)
#define ED_SAV_SRM_SIZE         (1 << 3)

#define ED_KEY                  0x1234

#define ED_SD_CMD_RD            (ED_SPI_CMD|ED_SPI_RD)
#define ED_SD_CMD_WR            (ED_SPI_CMD|ED_SPI_WR)
#define ED_SD_DAT_RD            (ED_SPI_DAT|ED_SPI_RD)
#define ED_SD_DAT_WR            (ED_SPI_DAT|ED_SPI_WR)

#define ED_SD_CMD_8b            ED_SPI_8BIT
#define ED_SD_CMD_1b            ED_SPI_1BIT
#define ED_SD_DAT_8b            ED_SPI_8BIT
#define ED_SD_DAT_1b            ED_SPI_1BIT

#define __ed_sd_mode(reg, val)  io_write(ED_SPI_CFG_REG, __sd_cfg|(reg)|(val))
#define __ed_sd_cmd_rd(val)     __ed_spi((val) & 0xFF)
#define __ed_sd_cmd_wr(val)     __ed_spi((val) & 0xFF)
#define __ed_sd_dat_rd()        __ed_spi(0xFF)
#define __ed_sd_dat_wr(val)     __ed_spi((val) & 0xFF)

int ed_init(void)
{
    uint32_t ver;
    __cart_acs_get();
    io_write(ED_KEY_REG, ED_KEY);
    ver = io_read(ED_VER_REG) & 0xFFFF;
    if (ver < 0x100 || ver >= 0x400) CART_ABORT();
    io_write(ED_CFG_REG, ED_CFG_SDRAM_ON);
    __cart_dom2 = 0x80370404;
    /* V1/V2/V2.5 do not have physical SRAM on board */
    /* The end of SDRAM is used for SRAM or FlashRAM save types */
    if (ver < 0x300)
    {
        uint32_t sav = io_read(ED_SAV_CFG_REG);
        /* Have 1M SRAM or FlashRAM */
        if (sav & ED_SAV_SRM_SIZE)
        {
            cart_size = 0x3FE0000; /* 64 MiB - 128 KiB */
        }
        /* Have 256K SRAM */
        else if (sav & ED_SAV_SRM_ON)
        {
            cart_size = 0x3FF8000; /* 64 MiB - 32KiB */
        }
        else
        {
            cart_size = 0x4000000; /* 64 MiB */
        }
    }
    __cart_acs_rel();
    return 0;
}

int ed_exit(void)
{
    __cart_acs_get();
    io_write(ED_KEY_REG, 0);
    __cart_acs_rel();
    return 0;
}

/* SPI exchange */
static int __ed_spi(int val)
{
    io_write(ED_SPI_REG, val);
    while (io_read(ED_STATUS_REG) & ED_STATE_SPI);
    return io_read(ED_SPI_REG);
}

static int __ed_sd_cmd(int cmd, uint32_t arg)
{
    int i;
    int n;
    char buf[6];
    buf[0] = cmd;
    buf[1] = arg >> 24;
    buf[2] = arg >> 16;
    buf[3] = arg >>  8;
    buf[4] = arg >>  0;
    buf[5] = __sd_crc7(buf);
    /* Send the command */
    __ed_sd_mode(ED_SD_CMD_WR, ED_SD_CMD_8b);
    __ed_sd_cmd_wr(0xFF);
    for (i = 0; i < 6; i++) __ed_sd_cmd_wr(buf[i]);
    /* Read the first response byte */
    __sd_resp[0] = 0xFF;
    __ed_sd_mode(ED_SD_CMD_RD, ED_SD_CMD_1b);
    n = 2048;
    while (__sd_resp[0] & 0xC0)
    {
        if (--n == 0) return -1;
        __sd_resp[0] = __ed_sd_cmd_rd(__sd_resp[0]);
    }
    /* Read the rest of the response */
    n = !__sd_type ?
        cmd == CMD8 || cmd == CMD58 ? 5 : 1 :
        cmd == CMD2 || cmd == CMD9 ? 17 : 6;
    __ed_sd_mode(ED_SD_CMD_RD, ED_SD_CMD_8b);
    for (i = 1; i < n; i++) __sd_resp[i] = __ed_sd_cmd_rd(0xFF);
    /* SPI: return "illegal command" flag */
    return !__sd_type ? (__sd_resp[0] & 4) : 0;
}

static int __ed_sd_close(int flag)
{
    int n;
    if (!flag)
    {
        /* SPI: Stop token (write) */
        __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_8b);
        __ed_sd_dat_wr(0xFD);
        __ed_sd_dat_wr(0xFF);
    }
    else
    {
        /* CMD12: STOP_TRANSMISSION */
        if (__ed_sd_cmd(CMD12, 0) < 0) return -1;
    }
    /* Wait for card */
    __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_8b);
    n = 65536;
    do
    {
        if (--n == 0) break;
    }
    while ((__ed_sd_dat_rd() & 0xFF) != 0xFF);
    return 0;
}

int ed_card_init(void)
{
    int i;
    int n;
    uint32_t rca;
    __cart_acs_get();
    /* Detect SD interface */
    /* 0: use SPI */
    /* 1: use SD */
    __sd_type = 0;
    if ((io_read(ED_VER_REG) & 0xFFFF) >= 0x116)
    {
        /* Check bootloader ROM label for "ED64 SD boot" */
        io_write(ED_CFG_REG, ED_CFG_SDRAM_OFF);
        /* label[4:8] == " SD " */
        if (io_read(0x10000024) == 0x20534420) __sd_type = 1;
        io_write(ED_CFG_REG, ED_CFG_SDRAM_ON);
    }
    /* SPI: SS = 0 */
    /* SD : SS = 1 */
    __sd_cfg = ED_SPI_SPD_LO;
    if (__sd_type) __sd_cfg |= ED_SPI_SS;
    /* Card needs 74 clocks, we do 80 */
    __ed_sd_mode(ED_SD_CMD_WR, ED_SD_CMD_8b);
    for (i = 0; i < 10; i++) __ed_sd_cmd_wr(0xFF);
    /* CMD0: GO_IDLE_STATE */
    __ed_sd_cmd(CMD0, 0);
    /* CMD8: SEND_IF_COND */
    /* If it returns an error, it is SD V1 */
    if (__ed_sd_cmd(CMD8, 0x1AA))
    {
        /* SD V1 */
        if (!__sd_type)
        {
            if (__ed_sd_cmd(CMD55, 0) < 0) CART_ABORT();
            if (__ed_sd_cmd(ACMD41, 0x40300000) < 0)
            {
                n = 1024;
                do
                {
                    if (--n == 0) CART_ABORT();
                    if (__ed_sd_cmd(CMD1, 0) < 0) CART_ABORT();
                }
                while (__sd_resp[0] != 0);
            }
            else
            {
                n = 1024;
                do
                {
                    if (--n == 0) CART_ABORT();
                    if (__ed_sd_cmd(CMD55, 0) < 0) CART_ABORT();
                    if (__sd_resp[0] != 1) continue;
                    if (__ed_sd_cmd(ACMD41, 0x40300000) < 0) CART_ABORT();
                }
                while (__sd_resp[0] != 0);
            }
        }
        else
        {
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
                if (__ed_sd_cmd(CMD55, 0) < 0) CART_ABORT();
                if (__ed_sd_cmd(ACMD41, 0x40300000) < 0) CART_ABORT();
            }
            while (__sd_resp[1] == 0);
        }
        __sd_flag = 0;
    }
    else
    {
        /* SD V2 */
        if (!__sd_type)
        {
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
                if (__ed_sd_cmd(CMD55, 0) < 0) CART_ABORT();
                if (__sd_resp[0] != 1) continue;
                if (__ed_sd_cmd(ACMD41, 0x40300000) < 0) CART_ABORT();
            }
            while (__sd_resp[0] != 0);
            if (__ed_sd_cmd(CMD58, 0) < 0) CART_ABORT();
        }
        else
        {
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
                if (__ed_sd_cmd(CMD55, 0) < 0) CART_ABORT();
                if (!(__sd_resp[3] & 1)) continue;
                __ed_sd_cmd(ACMD41, 0x40300000);
            }
            while (!(__sd_resp[1] & 0x80));
        }
        /* Card is SDHC */
        __sd_flag = __sd_resp[1] & 0x40;
    }
    if (!__sd_type)
    {
        __sd_cfg = ED_SPI_SPD_25;
    }
    else
    {
        /* CMD2: ALL_SEND_CID */
        if (__ed_sd_cmd(CMD2, 0) < 0) CART_ABORT();
        /* CMD3: SEND_RELATIVE_ADDR */
        if (__ed_sd_cmd(CMD3, 0) < 0) CART_ABORT();
        rca =
            __sd_resp[1] << 24 |
            __sd_resp[2] << 16 |
            __sd_resp[3] <<  8 |
            __sd_resp[4] <<  0;
        /* CMD9: SEND_CSD */
        if (__ed_sd_cmd(CMD9, rca) < 0) CART_ABORT();
        /* CMD7: SELECT_CARD */
        if (__ed_sd_cmd(CMD7, rca) < 0) CART_ABORT();
        /* ACMD6: SET_BUS_WIDTH */
        if (__ed_sd_cmd(CMD55, rca) < 0) CART_ABORT();
        if (__ed_sd_cmd(ACMD6, 2) < 0) CART_ABORT();
        __sd_cfg = ED_SPI_SPD_50|ED_SPI_SS;
    }
    __cart_acs_rel();
    return 0;
}

int ed_card_rd_dram(void *dram, uint32_t lba, uint32_t count)
{
    char *addr = dram;
    int i;
    int n;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD18: READ_MULTIPLE_BLOCK */
    if (__ed_sd_cmd(CMD18, lba) < 0) CART_ABORT();
    while (count-- > 0)
    {
        /* Wait for card */
        __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_1b);
        n = 65536;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while (__ed_sd_dat_rd() & 1);
        /* Read data */
        __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_8b);
        for (i = 0; i < 512; i++) addr[i] = __ed_sd_dat_rd();
        /* SPI: 1x16-bit CRC (2 byte) */
        /* SD:  4x16-bit CRC (8 byte) */
        /* We ignore the CRC */
        n = !__sd_type ? 2 : 8;
        for (i = 0; i < n; i++) __ed_sd_dat_rd();
        addr += 512;
    }
    if (__ed_sd_close(1)) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int ed_card_rd_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    int i;
    int n;
    uint32_t resp;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD18: READ_MULTIPLE_BLOCK */
    if (__ed_sd_cmd(CMD18, lba) < 0) CART_ABORT();
    /* DMA requires 2048-byte alignment */
    if (cart & 0x7FF)
    {
        while (count-- > 0)
        {
            /* Wait for card */
            __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_1b);
            n = 65536;
            do
            {
                if (--n == 0) CART_ABORT();
            }
            while (__ed_sd_dat_rd() & 1);
            /* Read data */
            __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_8b);
            for (i = 0; i < 512; i++)
            {
                ((char *)__cart_buf)[i] = __ed_sd_dat_rd();
            }
            /* SPI: 1x16-bit CRC (2 byte) */
            /* SD:  4x16-bit CRC (8 byte) */
            /* We ignore the CRC */
            n = !__sd_type ? 2 : 8;
            for (i = 0; i < n; i++) __ed_sd_dat_rd();
            __cart_dma_wr(__cart_buf, cart, 512);
            cart += 512;
        }
    }
    else
    {
        if (cart_card_byteswap)
        {
            io_write(ED_CFG_REG, ED_CFG_SDRAM_ON|ED_CFG_BYTESWAP);
        }
        __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_8b);
        io_write(ED_DMA_LEN_REG, count-1);
        io_write(ED_DMA_ADDR_REG, (cart & 0x3FFFFFF) >> 11);
        io_write(ED_DMA_CFG_REG, ED_DMA_SD_TO_RAM);
        while ((resp = io_read(ED_STATUS_REG)) & ED_STATE_DMA_BUSY)
        {
            if (resp & ED_STATE_DMA_TOUT)
            {
                io_write(ED_CFG_REG, ED_CFG_SDRAM_ON);
                CART_ABORT();
            }
        }
        if (cart_card_byteswap)
        {
            io_write(ED_CFG_REG, ED_CFG_SDRAM_ON);
        }
    }
    if (__ed_sd_close(1)) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int ed_card_wr_dram(const void *dram, uint32_t lba, uint32_t count)
{
    const char *addr = dram;
    int i;
    int n;
    int resp;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD25: WRITE_MULTIPLE_BLOCK */
    if (__ed_sd_cmd(CMD25, lba) < 0) CART_ABORT();
    if (!__sd_type)
    {
        /* SPI: padding (why 2 bytes?) */
        __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_8b);
        __ed_sd_dat_wr(0xFF);
        __ed_sd_dat_wr(0xFF);
    }
    while (count-- > 0)
    {
        __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_8b);
        if (!__sd_type)
        {
            /* SPI: data token */
            __ed_sd_dat_wr(0xFC);
        }
        else
        {
            /* SD: start bit (why not only write F0?) */
            __ed_sd_dat_wr(0xFF);
            __ed_sd_dat_wr(0xF0);
        }
        /* Write data */
        for (i = 0; i < 512; i++) __ed_sd_dat_wr(addr[i]);
        if (!__sd_type)
        {
            /* SPI: write dummy CRC */
            for (i = 0; i < 2; i++) __ed_sd_dat_wr(0xFF);
        }
        else
        {
            /* SD: write real CRC */
            if ((long)addr & 7)
            {
                __cart_buf_rd(addr);
                __sd_crc16(__cart_buf, __cart_buf);
            }
            else
            {
                __sd_crc16(__cart_buf, (const uint64_t *)addr);
            }
            for (i = 0; i < 8; i++) __ed_sd_dat_wr(((char *)__cart_buf)[i]);
            /* End bit */
            __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_1b);
            __ed_sd_dat_wr(0xFF);
            /* Wait for start of response */
            __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_1b);
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
            }
            while (__ed_sd_dat_rd() & 1);
            /* Read response */
            resp = 0;
            for (i = 0; i < 3; i++) resp = resp << 1 | (__ed_sd_dat_rd() & 1);
            if (resp != 2) CART_ABORT();
        }
        /* Wait for card */
        __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_8b);
        n = 65536;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while ((__ed_sd_dat_rd() & 0xFF) != 0xFF);
        addr += 512;
    }
    if (__ed_sd_close(__sd_type)) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int ed_card_wr_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    int i;
    int n;
    int resp;
    __cart_acs_get();
    /* SDSC takes byte address, SDHC takes LBA */
    if (!__sd_flag) lba *= 512;
    /* CMD25: WRITE_MULTIPLE_BLOCK */
    if (__ed_sd_cmd(CMD25, lba) < 0) CART_ABORT();
    if (!__sd_type)
    {
        /* SPI: padding (why 2 bytes?) */
        __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_8b);
        __ed_sd_dat_wr(0xFF);
        __ed_sd_dat_wr(0xFF);
    }
    while (count-- > 0)
    {
        __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_8b);
        if (!__sd_type)
        {
            /* SPI: data token */
            __ed_sd_dat_wr(0xFC);
        }
        else
        {
            /* SD: start bit (why not only write F0?) */
            __ed_sd_dat_wr(0xFF);
            __ed_sd_dat_wr(0xF0);
        }
        __cart_dma_rd(__cart_buf, cart, 512);
        /* Write data */
        for (i = 0; i < 512; i++) __ed_sd_dat_wr(((char *)__cart_buf)[i]);
        if (!__sd_type)
        {
            /* SPI: write dummy CRC */
            for (i = 0; i < 2; i++) __ed_sd_dat_wr(0xFF);
        }
        else
        {
            /* SD: write real CRC */
            __sd_crc16(__cart_buf, __cart_buf);
            for (i = 0; i < 8; i++) __ed_sd_dat_wr(((char *)__cart_buf)[i]);
            /* End bit */
            __ed_sd_mode(ED_SD_DAT_WR, ED_SD_DAT_1b);
            __ed_sd_dat_wr(0xFF);
            /* Wait for start of response */
            __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_1b);
            n = 1024;
            do
            {
                if (--n == 0) CART_ABORT();
            }
            while (__ed_sd_dat_rd() & 1);
            /* Read response */
            resp = 0;
            for (i = 0; i < 3; i++) resp = resp << 1 | (__ed_sd_dat_rd() & 1);
            if (resp != 2) CART_ABORT();
        }
        /* Wait for card */
        __ed_sd_mode(ED_SD_DAT_RD, ED_SD_DAT_8b);
        n = 65536;
        do
        {
            if (--n == 0) CART_ABORT();
        }
        while ((__ed_sd_dat_rd() & 0xFF) != 0xFF);
        cart += 512;
    }
    if (__ed_sd_close(__sd_type)) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

#define SC_BASE_REG             0x1FFF0000
#define SC_BUFFER_REG           0x1FFE0000

#define SC_STATUS_REG           (SC_BASE_REG+0x00)
#define SC_COMMAND_REG          (SC_BASE_REG+0x00)
#define SC_DATA0_REG            (SC_BASE_REG+0x04)
#define SC_DATA1_REG            (SC_BASE_REG+0x08)
#define SC_IDENTIFIER_REG       (SC_BASE_REG+0x0C)
#define SC_KEY_REG              (SC_BASE_REG+0x10)

#define SC_CMD_BUSY             0x80000000
#define SC_CMD_ERROR            0x40000000
#define SC_IRQ_PENDING          0x20000000

#define SC_CONFIG_GET           'c'
#define SC_CONFIG_SET           'C'
#define SC_SD_OP                'i'
#define SC_SD_SECTOR_SET        'I'
#define SC_SD_READ              's'
#define SC_SD_WRITE             'S'

#define SC_CFG_ROM_WRITE        1
#define SC_CFG_DD_MODE          3
#define SC_CFG_SAVE_TYPE        6

#define SC_SD_DEINIT            0
#define SC_SD_INIT              1
#define SC_SD_GET_STATUS        2
#define SC_SD_GET_INFO          3
#define SC_SD_BYTESWAP_ON       4
#define SC_SD_BYTESWAP_OFF      5

#define SC_DD_MODE_REGS         1
#define SC_DD_MODE_IPL          2

#define SC_IDENTIFIER           0x53437632  /* SCv2 */

#define SC_KEY_RESET            0x00000000
#define SC_KEY_LOCK             0xFFFFFFFF
#define SC_KEY_UNL              0x5F554E4C  /* _UNL */
#define SC_KEY_OCK              0x4F434B5F  /* OCK_ */

static int __sc_sync(void)
{
    while (io_read(SC_STATUS_REG) & SC_CMD_BUSY);
    if (io_read(SC_STATUS_REG) & SC_CMD_ERROR) return -1;
    return 0;
}

int sc_init(void)
{
    uint32_t cfg;
    __cart_acs_get();
    io_write(SC_KEY_REG, SC_KEY_RESET);
    io_write(SC_KEY_REG, SC_KEY_UNL);
    io_write(SC_KEY_REG, SC_KEY_OCK);
    if (io_read(SC_IDENTIFIER_REG) != SC_IDENTIFIER) CART_ABORT();
    __sc_sync();
    io_write(SC_DATA0_REG, SC_CFG_ROM_WRITE);
    io_write(SC_DATA1_REG, 1);
    io_write(SC_COMMAND_REG, SC_CONFIG_SET);
    __sc_sync();
    /* SC64 uses SDRAM for 64DD */
    io_write(SC_DATA0_REG, SC_CFG_DD_MODE);
    io_write(SC_COMMAND_REG, SC_CONFIG_GET);
    __sc_sync();
    cfg = io_read(SC_DATA1_REG);
    /* Have registers */
    if (cfg & SC_DD_MODE_REGS)
    {
        cart_size = 0x2000000; /* 32 MiB */
    }
    /* Have IPL */
    else if (cfg & SC_DD_MODE_IPL)
    {
        cart_size = 0x3BC0000; /* 59.75 MiB */
    }
    else
    {
        /* SC64 does not have physical SRAM on board */
        /* The end of SDRAM is used for SRAM or FlashRAM save types */
        io_write(SC_DATA0_REG, SC_CFG_SAVE_TYPE);
        io_write(SC_COMMAND_REG, SC_CONFIG_GET);
        __sc_sync();
        /* Have SRAM or FlashRAM */
        if (io_read(SC_DATA1_REG) >= 3)
        {
            cart_size = 0x3FE0000; /* 64 MiB - 128 KiB */
        }
        else
        {
            cart_size = 0x4000000; /* 64 MiB */
        }
    }
    __cart_acs_rel();
    return 0;
}

int sc_exit(void)
{
    __cart_acs_get();
    __sc_sync();
    io_write(SC_DATA1_REG, SC_SD_DEINIT);
    io_write(SC_COMMAND_REG, SC_SD_OP);
    __sc_sync();
    io_write(SC_DATA0_REG, SC_CFG_ROM_WRITE);
    io_write(SC_DATA1_REG, 0);
    io_write(SC_COMMAND_REG, SC_CONFIG_SET);
    __sc_sync();
    io_write(SC_KEY_REG, SC_KEY_RESET);
    io_write(SC_KEY_REG, SC_KEY_LOCK);
    __cart_acs_rel();
    return 0;
}

int sc_card_init(void)
{
    __cart_acs_get();
    __sc_sync();
    io_write(SC_DATA1_REG, SC_SD_INIT);
    io_write(SC_COMMAND_REG, SC_SD_OP);
    if (__sc_sync()) CART_ABORT();
    __cart_acs_rel();
    return 0;
}

int sc_card_rd_dram(void *dram, uint32_t lba, uint32_t count)
{
    char *addr = dram;
    int i;
    int n;
    __cart_acs_get();
    __sc_sync();
    while (count > 0)
    {
        n = count < 16 ? count : 16;
        io_write(SC_DATA0_REG, lba);
        io_write(SC_COMMAND_REG, SC_SD_SECTOR_SET);
        if (__sc_sync()) CART_ABORT();
        io_write(SC_DATA0_REG, SC_BUFFER_REG);
        io_write(SC_DATA1_REG, n);
        io_write(SC_COMMAND_REG, SC_SD_READ);
        if (__sc_sync()) CART_ABORT();
        if ((long)addr & 7)
        {
            for (i = 0; i < n; i++)
            {
                __cart_dma_rd(__cart_buf, SC_BUFFER_REG+512*i, 512);
                __cart_buf_wr(addr);
                addr += 512;
            }
        }
        else
        {
            __cart_dma_rd(addr, SC_BUFFER_REG, 512*n);
            addr += 512*n;
        }
        lba += n;
        count -= n;
    }
    __cart_acs_rel();
    return 0;
}

int sc_card_rd_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    __cart_acs_get();
    __sc_sync();
    if (cart_card_byteswap)
    {
        io_write(SC_DATA1_REG, SC_SD_BYTESWAP_ON);
        io_write(SC_COMMAND_REG, SC_SD_OP);
        if (__sc_sync()) CART_ABORT();
    }
    io_write(SC_DATA0_REG, lba);
    io_write(SC_COMMAND_REG, SC_SD_SECTOR_SET);
    if (__sc_sync()) CART_ABORT();
    io_write(SC_DATA0_REG, cart);
    io_write(SC_DATA1_REG, count);
    io_write(SC_COMMAND_REG, SC_SD_READ);
    if (__sc_sync()) CART_ABORT();
    if (cart_card_byteswap)
    {
        io_write(SC_DATA1_REG, SC_SD_BYTESWAP_OFF);
        io_write(SC_COMMAND_REG, SC_SD_OP);
        if (__sc_sync()) CART_ABORT();
    }
    __cart_acs_rel();
    return 0;
}

int sc_card_wr_dram(const void *dram, uint32_t lba, uint32_t count)
{
    const char *addr = dram;
    int i;
    int n;
    __cart_acs_get();
    __sc_sync();
    while (count > 0)
    {
        n = count < 16 ? count : 16;
        if ((long)addr & 7)
        {
            for (i = 0; i < n; i++)
            {
                __cart_buf_rd(addr);
                __cart_dma_wr(__cart_buf, SC_BUFFER_REG+512*i, 512);
                addr += 512;
            }
        }
        else
        {
            __cart_dma_wr(addr, SC_BUFFER_REG, 512*n);
            addr += 512*n;
        }
        io_write(SC_DATA0_REG, lba);
        io_write(SC_COMMAND_REG, SC_SD_SECTOR_SET);
        if (__sc_sync()) CART_ABORT();
        io_write(SC_DATA0_REG, SC_BUFFER_REG);
        io_write(SC_DATA1_REG, n);
        io_write(SC_COMMAND_REG, SC_SD_WRITE);
        if (__sc_sync()) CART_ABORT();
        lba += n;
        count -= n;
    }
    __cart_acs_rel();
    return 0;
}

int sc_card_wr_cart(uint32_t cart, uint32_t lba, uint32_t count)
{
    __cart_acs_get();
    __sc_sync();
    io_write(SC_DATA0_REG, lba);
    io_write(SC_COMMAND_REG, SC_SD_SECTOR_SET);
    if (__sc_sync()) CART_ABORT();
    io_write(SC_DATA0_REG, cart);
    io_write(SC_DATA1_REG, count);
    io_write(SC_COMMAND_REG, SC_SD_WRITE);
    if (__sc_sync()) CART_ABORT();
    __cart_acs_rel();
    return 0;
}
