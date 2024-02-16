#ifndef BOOT_MINIDRAGON_H
#define BOOT_MINIDRAGON_H

#include <stdint.h>

typedef uint64_t u_uint64_t __attribute__((aligned(1)));

#define MEMORY_BARRIER()    asm volatile ("" : : : "memory")
#define C0_WRITE_CAUSE(x)   asm volatile("mtc0 %0,$13"::"r"(x))
#define C0_WRITE_COUNT(x)   asm volatile("mtc0 %0,$9"::"r"(x))
#define C0_WRITE_COMPARE(x) asm volatile("mtc0 %0,$11"::"r"(x))
#define C0_WRITE_WATCHLO(x) asm volatile("mtc0 %0,$18"::"r"(x))
#define C0_COUNT() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$9":"=r"(x)); \
    x; \
})

#define C0_TAGLO() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$28":"=r"(x)); \
    x; \
})

#ifndef NDEBUG
#undef assert
#define assert(x)           ({ if (!(x)) { debugf("ASSERTION FAILED: " #x); abort(); } })
#define assertf(x, ...)     ({ if (!(x)) { debugf(#x); debugf(__VA_ARGS__); abort(); } })
#else
#undef assert
#define assert(x)           ({  })
#define assertf(x, ...)     ({  })
#endif

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
#define INDEX_LOAD_TAG			1
#define INDEX_STORE_TAG			2
#define INDEX_CREATE_DIRTY      3
#define BUILD_CACHE_OP(o,c)		(((o) << 2) | (c))
#define INDEX_WRITEBACK_INVALIDATE_D	BUILD_CACHE_OP(INDEX_INVALIDATE,CACHE_D)
#define INDEX_STORE_TAG_I              	BUILD_CACHE_OP(INDEX_STORE_TAG,CACHE_I)
#define INDEX_STORE_TAG_D               BUILD_CACHE_OP(INDEX_STORE_TAG,CACHE_D)
#define INDEX_LOAD_TAG_I              	BUILD_CACHE_OP(INDEX_LOAD_TAG,CACHE_I)
#define INDEX_LOAD_TAG_D                BUILD_CACHE_OP(INDEX_LOAD_TAG,CACHE_D)
#define INDEX_CREATE_DIRTY_D            BUILD_CACHE_OP(INDEX_CREATE_DIRTY,CACHE_D)


#define SP_RSP_ADDR     ((volatile uint32_t*)0xa4040000)
#define SP_DRAM_ADDR    ((volatile uint32_t*)0xa4040004)
#define SP_RD_LEN       ((volatile uint32_t*)0xa4040008)
#define SP_WR_LEN       ((volatile uint32_t*)0xa404000c)
#define SP_STATUS       ((volatile uint32_t*)0xa4040010)
#define SP_DMA_FULL     ((volatile uint32_t*)0xa4040014)
#define SP_DMA_BUSY     ((volatile uint32_t*)0xa4040018)
#define SP_SEMAPHORE    ((volatile uint32_t*)0xa404001c)
#define SP_PC           ((volatile uint32_t*)0xa4080000)
#define SP_DMEM         ((uint32_t*)0xa4000000)
#define SP_IMEM         ((uint32_t*)0xa4001000)

#define SP_WSTATUS_CLEAR_HALT        0x00001   ///< SP_STATUS write mask: clear #SP_STATUS_HALTED bit
#define SP_WSTATUS_SET_HALT          0x00002   ///< SP_STATUS write mask: set #SP_STATUS_HALTED bit
#define SP_WSTATUS_CLEAR_BROKE       0x00004   ///< SP_STATUS write mask: clear BROKE bit
#define SP_WSTATUS_CLEAR_INTR        0x00008   ///< SP_STATUS write mask: clear INTR bit
#define SP_WSTATUS_SET_INTR          0x00010   ///< SP_STATUS write mask: set HALT bit
#define SP_WSTATUS_CLEAR_SSTEP       0x00020   ///< SP_STATUS write mask: clear SSTEP bit
#define SP_WSTATUS_SET_SSTEP         0x00040   ///< SP_STATUS write mask: set SSTEP bit
#define SP_WSTATUS_CLEAR_INTR_BREAK  0x00080   ///< SP_STATUS write mask: clear #SP_STATUS_INTERRUPT_ON_BREAK bit
#define SP_WSTATUS_SET_INTR_BREAK    0x00100   ///< SP_STATUS write mask: set SSTEP bit
#define SP_WSTATUS_CLEAR_SIG0        0x00200   ///< SP_STATUS write mask: clear SIG0 bit
#define SP_WSTATUS_SET_SIG0          0x00400   ///< SP_STATUS write mask: set SIG0 bit
#define SP_WSTATUS_CLEAR_SIG1        0x00800   ///< SP_STATUS write mask: clear SIG1 bit
#define SP_WSTATUS_SET_SIG1          0x01000   ///< SP_STATUS write mask: set SIG1 bit
#define SP_WSTATUS_CLEAR_SIG2        0x02000   ///< SP_STATUS write mask: clear SIG2 bit
#define SP_WSTATUS_SET_SIG2          0x04000   ///< SP_STATUS write mask: set SIG2 bit
#define SP_WSTATUS_CLEAR_SIG3        0x08000   ///< SP_STATUS write mask: clear SIG3 bit
#define SP_WSTATUS_SET_SIG3          0x10000   ///< SP_STATUS write mask: set SIG3 bit
#define SP_WSTATUS_CLEAR_SIG4        0x20000   ///< SP_STATUS write mask: clear SIG4 bit
#define SP_WSTATUS_SET_SIG4          0x40000   ///< SP_STATUS write mask: set SIG4 bit
#define SP_WSTATUS_CLEAR_SIG5        0x80000   ///< SP_STATUS write mask: clear SIG5 bit
#define SP_WSTATUS_SET_SIG5          0x100000  ///< SP_STATUS write mask: set SIG5 bit
#define SP_WSTATUS_CLEAR_SIG6        0x200000  ///< SP_STATUS write mask: clear SIG6 bit
#define SP_WSTATUS_SET_SIG6          0x400000  ///< SP_STATUS write mask: set SIG6 bit
#define SP_WSTATUS_CLEAR_SIG7        0x800000  ///< SP_STATUS write mask: clear SIG7 bit
#define SP_WSTATUS_SET_SIG7          0x1000000 ///< SP_STATUS write mask: set SIG7 bit


#define PI_DRAM_ADDR    ((volatile uint32_t*)0xA4600000)  ///< PI DMA: DRAM address register
#define PI_CART_ADDR    ((volatile uint32_t*)0xA4600004)  ///< PI DMA: cartridge address register
#define PI_RD_LEN       ((volatile uint32_t*)0xA4600008)  ///< PI DMA: read length register
#define PI_WR_LEN       ((volatile uint32_t*)0xA460000C)  ///< PI DMA: write length register
#define PI_STATUS       ((volatile uint32_t*)0xA4600010)  ///< PI: status register

#define MI_MODE                             ((volatile uint32_t*)0xA4300000)
#define MI_WMODE_CLEAR_REPEAT_MOD           0x80
#define MI_WMODE_SET_REPEAT_MODE            0x100
#define MI_WMODE_REPEAT_LENGTH(n)           ((n)-1)
#define MI_WMODE_SET_UPPER_MODE             0x2000
#define MI_WMODE_CLEAR_UPPER_MODE           0x1000
#define MI_VERSION                          ((volatile uint32_t*)0xA4300004)
#define MI_INTERRUPT                        ((volatile uint32_t*)0xA4300008)
#define MI_MASK                             ((volatile uint32_t*)0xA430000C)
#define MI_IQUE_RNG                         ((volatile uint32_t*)0xA430002C)
#define MI_WINTERRUPT_CLR_SP                0x0001
#define MI_WINTERRUPT_SET_SP                0x0002
#define MI_WINTERRUPT_CLR_SI                0x0004
#define MI_WINTERRUPT_SET_SI                0x0008
#define MI_WINTERRUPT_CLR_AI                0x0010
#define MI_WINTERRUPT_SET_AI                0x0020
#define MI_WINTERRUPT_CLR_VI                0x0040
#define MI_WINTERRUPT_SET_VI                0x0080
#define MI_WINTERRUPT_CLR_PI                0x0100
#define MI_WINTERRUPT_SET_PI                0x0200
#define MI_WINTERRUPT_CLR_DP                0x0400
#define MI_WINTERRUPT_SET_DP                0x0800
#define MI_WMASK_CLR_SP                     0x0001
#define MI_WMASK_SET_SP                     0x0002
#define MI_WMASK_CLR_SI                     0x0004
#define MI_WMASK_SET_SI                     0x0008
#define MI_WMASK_CLR_AI                     0x0010
#define MI_WMASK_SET_AI                     0x0020
#define MI_WMASK_CLR_VI                     0x0040
#define MI_WMASK_SET_VI                     0x0080
#define MI_WMASK_CLR_PI                     0x0100
#define MI_WMASK_SET_PI                     0x0200
#define MI_WMASK_CLR_DP                     0x0400
#define MI_WMASK_SET_DP                     0x0800

#define AI_STATUS                           ((volatile uint32_t*)0xA450000C)

#define RI_MODE								((volatile uint32_t*)0xA4700000)
#define RI_CONFIG							((volatile uint32_t*)0xA4700004)
#define RI_CURRENT_LOAD						((volatile uint32_t*)0xA4700008)
#define RI_SELECT							((volatile uint32_t*)0xA470000C)
#define RI_REFRESH							((volatile uint32_t*)0xA4700010)
#define RI_LATENCY							((volatile uint32_t*)0xA4700014)
#define RI_ERROR							((volatile uint32_t*)0xA4700018)
#define RI_BANK_STATUS						((volatile uint32_t*)0xA470001C)

#define PI_CLEAR_INTERRUPT                  0x02
#define SI_CLEAR_INTERRUPT                  0
#define SP_CLEAR_INTERRUPT                  0x08
#define DP_CLEAR_INTERRUPT                  0x0800
#define AI_CLEAR_INTERRUPT                  0

#define UncachedAddr(x)                     ((void*)((uint32_t)(x) | 0x20000000))

#define cache_op(addr, op, linesize, length) ({ \
    { \
        void *cur = (void*)((unsigned long)addr & ~(linesize-1)); \
        int count = (int)length + (addr-cur); \
        for (int i = 0; i < count; i += linesize) \
            asm ("\tcache %0,(%1)\n"::"i" (op), "r" (cur+i)); \
    } \
})

/** @brief Round n up to the next multiple of d */
#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

#define cache_op2(addr, op, linesize, length) ({ \
    void *end = (void*)(ROUND_UP(((uint32_t)addr) + length - 1, linesize)); \
    do { \
        asm ("\tcache %0,(%1)\n"::"i" (op), "r" (addr)); \
        addr += linesize; \
    } while (addr < end); \
})

static inline void data_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(addr, 0x11, 16, length);
}   

static inline void data_cache_hit_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(addr, 0x15, 16, length);
}   

static inline void data_cache_writeback_invalidate_all(void)
{
    cache_op((void*)0x80000000, INDEX_WRITEBACK_INVALIDATE_D, 0x10, 0x2000);
}

static inline void cop0_clear_cache(void)
{
    asm("mtc0 $0, $28");  // TagLo
    asm("mtc0 $0, $29");  // TagHi
    cache_op((void*)0x80000000, INDEX_STORE_TAG_D, 0x10, 0x2000);
    cache_op((void*)0x80000000, INDEX_STORE_TAG_I, 0x20, 0x4000);
}

__attribute__((noreturn))
static inline void abort(void)
{
    while(1) {}
}

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

static inline void si_wait(void)
{
    while (*SI_STATUS & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {}
    *SI_STATUS = SI_CLEAR_INTERRUPT;
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

static inline void rsp_dma_to_rdram(void* dmem, void *rdram, int size)
{
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = (uint32_t)dmem;
    *SP_DRAM_ADDR = (uint32_t)rdram;
    *SP_WR_LEN = size-1;
    while (*SP_DMA_BUSY) {}
}

static inline void rcp_reset(void)
{
    *SP_STATUS = SP_WSTATUS_CLEAR_BROKE | SP_WSTATUS_SET_HALT | SP_WSTATUS_CLEAR_INTR | 
                 SP_WSTATUS_CLEAR_SIG0 | SP_WSTATUS_CLEAR_SIG1 | 
                 SP_WSTATUS_CLEAR_SIG2 | SP_WSTATUS_CLEAR_SIG3 | 
                 SP_WSTATUS_CLEAR_SIG4 | SP_WSTATUS_CLEAR_SIG5 | 
                 SP_WSTATUS_CLEAR_SIG6 | SP_WSTATUS_CLEAR_SIG7;
    *SP_PC = 0;
    *SP_SEMAPHORE = 0;
    
    *MI_MASK = MI_WMASK_CLR_SP | MI_WMASK_CLR_SI | MI_WMASK_CLR_AI | MI_WMASK_CLR_VI | MI_WMASK_CLR_PI | MI_WMASK_CLR_DP;
    *MI_INTERRUPT = MI_WINTERRUPT_CLR_SP | MI_WINTERRUPT_CLR_SI | MI_WINTERRUPT_CLR_AI | MI_WINTERRUPT_CLR_VI | MI_WINTERRUPT_CLR_PI | MI_WINTERRUPT_CLR_DP;
    *PI_STATUS = PI_CLEAR_INTERRUPT;
    *SI_STATUS = SI_CLEAR_INTERRUPT;
    *AI_STATUS = AI_CLEAR_INTERRUPT;
    *MI_MODE = DP_CLEAR_INTERRUPT;
}


#define byteswap32(x) (__builtin_constant_p(x) ? __builtin_bswap32(x) : __byteswap32(x))

#endif
