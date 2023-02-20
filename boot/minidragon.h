
#define PI_STATUS          ((volatile uint32_t*)0xA4600010)
#define PI_STATUS_DMA_BUSY ( 1 << 0 )
#define PI_STATUS_IO_BUSY  ( 1 << 1 )

#define SI_STATUS          ((volatile uint32_t*)0xA4800018)
#define SI_STATUS_DMA_BUSY ( 1 << 0 )
#define SI_STATUS_IO_BUSY  ( 1 << 1 )

__attribute__((noinline))
static void io_write(uint32_t vaddrx, uint32_t value)
{
    volatile uint32_t *vaddr = (volatile uint32_t *)vaddrx;
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    *vaddr = value;
}

static inline uint32_t io_read(uint32_t vaddrx)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    volatile uint32_t *vaddr = (uint32_t *)vaddrx;
    return *vaddr;
}

static void wait(int n) {
    while (n--) asm volatile("");
}

static inline void si_write(uint32_t offset, uint32_t value)
{
    volatile uint32_t *vaddr = (volatile uint32_t *)(0xBFC00000 + offset);
    while (*SI_STATUS & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {}
    *vaddr = value;
}

static inline void dragon_init(void)
{
}


// Missing from libgcc
static uint32_t __byteswap32(uint32_t x)  
{
    uint32_t y = (x >> 24) & 0xff;
    y |= (x >> 8) & 0xff00;
    y |= (x << 8) & 0xff0000;
    y |= (x << 24) & 0xff000000;
    return y;
}

#define byteswap32(x) (__builtin_constant_p(x) ? __builtin_bswap32(x) : __byteswap32(x))
