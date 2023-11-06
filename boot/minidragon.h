#ifndef BOOT_MINIDRAGON_H
#define BOOT_MINIDRAGON_H

#include <stdint.h>

#define MEMORY_BARRIER()    asm volatile ("" : : : "memory")

#undef assert
#define assert(x)           ({ if (!(x)) { debugf("ASSERTION FAILED: " #x); abort(); } })
#define assertf(x, ...)     ({ if (!(x)) { debugf(#x); debugf(__VA_ARGS__); abort(); } })

#define abs(x)             __builtin_abs(x)
#define fabsf(x)           __builtin_fabsf(x)

#define PI_STATUS          ((volatile uint32_t*)0xA4600010)
#define PI_STATUS_DMA_BUSY ( 1 << 0 )
#define PI_STATUS_IO_BUSY  ( 1 << 1 )

#define SI_STATUS          ((volatile uint32_t*)0xA4800018)
#define SI_STATUS_DMA_BUSY ( 1 << 0 )
#define SI_STATUS_IO_BUSY  ( 1 << 1 )

#define CACHE_I		            0
#define CACHE_D		            1
#define INDEX_INVALIDATE		0
#define INDEX_STORE_TAG			2
#define BUILD_CACHE_OP(o,c)		(((o) << 2) | (c))
#define INDEX_WRITEBACK_INVALIDATE_D	BUILD_CACHE_OP(INDEX_INVALIDATE,CACHE_D)
#define INDEX_STORE_TAG_I              	BUILD_CACHE_OP(INDEX_STORE_TAG,CACHE_I)
#define INDEX_STORE_TAG_D               BUILD_CACHE_OP(INDEX_STORE_TAG,CACHE_D)

#define cache_op(addr, op, linesize, length) ({ \
    { \
        void *cur = (void*)((unsigned long)addr & ~(linesize-1)); \
        int count = (int)length + (addr-cur); \
        for (int i = 0; i < count; i += linesize) \
            asm ("\tcache %0,(%1)\n"::"i" (op), "r" (cur+i)); \
    } \
})

static inline void cop0_clear_cache(void)
{
    asm("mtc0 $0, $28");  // TagLo
    asm("mtc0 $0, $29");  // TagHi
    cache_op((void*)0x80000000, INDEX_STORE_TAG_D, 0x10, 0x2000);
    cache_op((void*)0x80000000, INDEX_STORE_TAG_I, 0x20, 0x4000);
}

__attribute__((noreturn))
void abort(void);

void io_write(uint32_t vaddrx, uint32_t value);

static inline uint32_t io_read(uint32_t vaddrx)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    volatile uint32_t *vaddr = (uint32_t *)vaddrx;
    return *vaddr;
}

static inline void wait(int n) {
    while (n--) asm volatile("");
}

static inline void si_write(uint32_t offset, uint32_t value)
{
    volatile uint32_t *vaddr = (volatile uint32_t *)(0xBFC00000 + offset);
    while (*SI_STATUS & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {}
    *vaddr = value;
}

// Missing from libgcc
static inline uint32_t __byteswap32(uint32_t x)  
{
    uint32_t y = (x >> 24) & 0xff;
    y |= (x >> 8) & 0xff00;
    y |= (x << 8) & 0xff0000;
    y |= (x << 24) & 0xff000000;
    return y;
}

#define byteswap32(x) (__builtin_constant_p(x) ? __builtin_bswap32(x) : __byteswap32(x))

#endif
