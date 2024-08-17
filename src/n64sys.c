/**
 * @file n64sys.c
 * @brief N64 System Interface
 * @ingroup n64sys
 */

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include "n64sys.h"
#include "regsinternal.h"
#include "interrupt.h"
#include "vi.h"
#include "rsp.h"
#include "rdp.h"
#include "utils.h"

int __boot_memsize;        ///< Memory size as detected by IPL3
int __boot_tvtype;         ///< TV type as detected by IPL3
int __boot_resettype;      ///< Reset type as detected by IPL3 
int __boot_consoletype;    ///< Console type as detected by IPL3

/** @brief Last tick at which the 64-bit counter was updated */
static uint32_t ticks64_base_tick;

/** @brief Last value of the 64-bit counter */
static uint64_t ticks64_base;

/**
 * @brief Helper macro to perform cache refresh operations
 *
 * @param[in] op
 *            Operation to perform
 * @param[in] linesize
 *            Size of a cacheline in bytes
 */
#define cache_op(op, linesize) ({ \
    if (length) { \
        void *cur = (void*)((unsigned long)addr & ~(linesize-1)); \
        int count = (int)length + (addr-cur); \
        for (int i = 0; i < count; i += linesize) \
            asm ("\tcache %0,(%1)\n"::"i" (op), "r" (cur+i)); \
    } \
})

void data_cache_hit_writeback(volatile const void * addr, unsigned long length)
{
    cache_op(0x19, 16);
}

/** @brief Underlying implementation of data_cache_hit_invalidate */
void __data_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x11, 16);
}

void data_cache_hit_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x15, 16);
}

void data_cache_index_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x01, 16);
}

void data_cache_writeback_invalidate_all(void)
{
    // TODO: do an index op instead for better performance
    data_cache_hit_writeback_invalidate(KSEG0_START_ADDR, get_memory_size());
}

void inst_cache_hit_writeback(volatile const void * addr, unsigned long length)
{
    cache_op(0x18, 32);
}

void inst_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x10, 32);
}

void inst_cache_index_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x00, 32);
}

void inst_cache_invalidate_all(void)
{
    // TODO: do an index op instead for better performance
    inst_cache_hit_invalidate(KSEG0_START_ADDR, get_memory_size());
}

void *malloc_uncached(size_t size)
{
    return malloc_uncached_aligned(16, size);
}

void *malloc_uncached_aligned(int align, size_t size)
{
    // Since we will be accessing the buffer as uncached memory, we absolutely
    // need to prevent part of it to ever enter the data cache, even as false
    // sharing with contiguous buffers. So we want the buffer to exclusively
    // cover full cachelines (aligned to minimum 16 bytes, multiple of 16 bytes).
    if (align < 16)
        align = 16;
    size = ROUND_UP(size, 16);
    void *mem = memalign(align, size);
    if (!mem) return NULL;

    // The memory returned by the system allocator could already be partly in
    // cache (eg: it might have been previously used as a normal heap buffer
    // and recently returned to the allocator). Invalidate it so that
    // we don't risk a writeback in the short future.
    data_cache_hit_invalidate(mem, size);

    // Return the pointer as uncached memory.
    return UncachedAddr(mem);
}

void free_uncached(void *buf)
{
    free(CachedAddr(buf));
}

int get_memory_size()
{
    return __boot_memsize;
}

bool is_memory_expanded()
{
    return get_memory_size() >= 0x7C0000;
}


tv_type_t get_tv_type() 
{
    return __boot_tvtype;
}

reset_type_t sys_reset_type(void)
{
    return __boot_resettype;
}

uint64_t get_ticks(void)
{
	uint32_t now = TICKS_READ();
	uint32_t prev = ticks64_base_tick;
	ticks64_base_tick = now;
	ticks64_base += now - prev;
	return ticks64_base;
}

uint64_t get_ticks_us(void)
{
    return TICKS_TO_US(get_ticks());
}

uint64_t get_ticks_ms(void)
{
    return TICKS_TO_MS(get_ticks());
}

void wait_ticks( unsigned long wait )
{
    unsigned int initial_tick = TICKS_READ();
    while( TICKS_READ() - initial_tick < wait );
}

void wait_ms( unsigned long wait_ms )
{
    wait_ticks(TICKS_FROM_MS(wait_ms));
}

/**
 * @brief Force a complete halt of all processors
 *
 * @note It should occur whenever a reset has been triggered 
 * and its past its RESET_TIME_LENGTH grace time period.
 * This function will shut down the RSP and the CPU, blank the VI.
 * Eventually the RDP will flush and complete its work as well.
 * The system will recover after a reset or power cycle.
 * 
 */
void die(void)
{
    // Can't have any interrupts here
    disable_interrupts();
    // Halt the RSP
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    // Flush the RDP
    *DP_STATUS = DP_WSTATUS_SET_FLUSH | DP_WSTATUS_SET_FREEZE;
    *DP_STATUS = DP_WSTATUS_RESET_FLUSH | DP_WSTATUS_RESET_FREEZE;
    // Shut the video off
    *VI_CTRL = *VI_CTRL & (~VI_CTRL_TYPE);
    // Terminate the CPU execution
    abort();
}

/**
 * @brief Initialize COP1 with default settings that prevent undesirable exceptions.
 *
 */
__attribute__((constructor)) void __init_cop1(void)
{
    /* Read initialized value from cop1 control register */
    uint32_t fcr31 = C1_FCR31();
    
    /* Disable all pending exceptions to avoid triggering one immediately.
       These can be survived from a soft reset. */
    fcr31 &= ~(C1_CAUSE_OVERFLOW | 
               C1_CAUSE_UNDERFLOW | 
               C1_CAUSE_NOT_IMPLEMENTED | 
               C1_CAUSE_DIV_BY_0 | 
               C1_CAUSE_INEXACT_OP | 
               C1_CAUSE_INVALID_OP);

#ifndef NDEBUG
    /** 
     * Enable FPU exceptions that can help programmers avoid bugs in their code. 
     * Underflow exceptions are not enabled because they are triggered whenever
     * a denormalized float is generated, *even if* the FS bit is set (see below).
     * So basically having the underflow exception enabled seems to be useless
     * unless also the underflow (and the inexact) exceptions are off.
     * Notice that underflows can happen also with library code such as
     * tanf(BITCAST_I2F(0x3f490fdb)) (0.785398185253).
     */
    fcr31 |= C1_ENABLE_OVERFLOW | 
             C1_ENABLE_DIV_BY_0 | 
             C1_ENABLE_INVALID_OP;
#endif

    /* Set FS bit to allow flashing of denormalized floats
       The FPU inside the N64 CPU does not implement denormalized floats
       and will generate an unmaskable exception if a denormalized float
       is generated by any floating point operations. In order to prevent
       this exception we set the FS bit in the fcr31 control register to
       instead "flash" and "flush" the denormalized number. To understand 
       the flashing rules please read pg. 213 of the VR4300 programmers manual */
    fcr31 |= C1_FCR31_FS;

    /* Write back updated cop1 control register */
    C1_WRITE_FCR31(fcr31);
}

/************* BSS CHECK **************/
// This code is useful only while debugging IPL3 changes. It is not run by default
// and requires manually changing entrypoint.S to be activated. It is left in the
// preview branch for reference, but it's not meant to be merged into stable.

/// @cond
static uint32_t io_read32(uint32_t vaddrx)
{
    vaddrx |= 0xA0000000;
    volatile uint32_t *vaddr = (uint32_t *)vaddrx;
    return *vaddr;
}

static uint8_t io_read8(uint32_t vaddrx)
{
    uint32_t value = io_read32(vaddrx & ~3);
    return value >> ((~vaddrx & 3)*8);
}

static const unsigned char font[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x7e, 0xa1, 0x99, 0x85, 0x7e, 0x84, 0x82, 0xff, 0x80, 0x80, 0xc1, 0xa1,
    0x91, 0x89, 0x86, 0x89, 0x89, 0x89, 0x89, 0x76, 0x18, 0x14, 0x12, 0xff,
    0x10, 0x8f, 0x89, 0x89, 0x89, 0x71, 0x7e, 0x89, 0x89, 0x89, 0x72, 0x01,
    0x81, 0x61, 0x19, 0x07, 0x62, 0x95, 0x89, 0x95, 0x62, 0x4e, 0x91, 0x91,
    0x91, 0x7e, 0xfe, 0x11, 0x11, 0x11, 0xfe, 0xff, 0x89, 0x89, 0x89, 0x76,
    0x7e, 0x81, 0x81, 0x81, 0x81, 0xff, 0x81, 0x81, 0x81, 0x7e, 0xff, 0x89,
    0x89, 0x89, 0x89, 0xff, 0x09, 0x09, 0x09, 0x09, 0x7e, 0x81, 0x91, 0x51,
    0xf1, 0xff, 0x08, 0x08, 0x08, 0xff, 0x00, 0x81, 0xff, 0x81, 0x00, 0x40,
    0x80, 0x80, 0x80, 0x7f, 0xff, 0x08, 0x14, 0x22, 0xc1, 0xff, 0x80, 0x80,
    0x80, 0x80, 0xff, 0x02, 0x04, 0x02, 0xff, 0xff, 0x06, 0x18, 0x60, 0xff,
    0x7e, 0x81, 0x81, 0x81, 0x7e, 0xff, 0x11, 0x11, 0x11, 0x0e, 0x7e, 0x81,
    0xa1, 0xc1, 0xfe, 0xff, 0x11, 0x11, 0x11, 0xee, 0x86, 0x89, 0x89, 0x89,
    0x71, 0x01, 0x01, 0xff, 0x01, 0x01, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x1f,
    0x60, 0x80, 0x60, 0x1f, 0xff, 0x40, 0x20, 0x40, 0xff, 0xc7, 0x28, 0x10,
    0x28, 0xc7, 0x07, 0x08, 0xf0, 0x08, 0x07, 0xc1, 0xa1, 0x99, 0x85, 0x83,
};


#define _(x)    ((x) >= '0' && (x) <= '9' ? (x) - '0' + 2 : \
                 (x) >= 'A' && (x) <= 'Z' ? (x) - 'A' + 12 : \
                 (x) == ' ' ? 1 : 0)

// "BSS CHECK ERROR"
static const char MSG_BSS_CHECK_ERROR[] = {
    _('B'), _('S'), _('S'), _(' '), _('C'), _('H'), _('E'), _('C'), _('K'), _(' '), _('E'), _('R'), _('R'), _('O'), _('R'), 0
};

__attribute__((noreturn))
static void fatal(const char *str)
{
#if 1
#define FATAL_TEXT  1
#if FATAL_TEXT
#endif
    static const uint32_t vi_regs_p[3][7] =  {
    {   /* PAL */   0x0404233a, 0x00000271, 0x00150c69,
        0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b },
    {   /* NTSC */  0x03e52239, 0x0000020d, 0x00000c15,
        0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204 },
    {   /* MPAL */  0x04651e39, 0x0000020d, 0x00040c11,
        0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204 },
    };

    #undef RGBA32
    #define RGBA(r,g,b,a)   (((r)<<11) | ((g)<<6) | ((b)<<1) | (a))
    #define RGBA32(rgba32)  RGBA((((rgba32)>>19) & 0x1F), (((rgba32)>>11) & 0x1F), ((rgba32>>3) & 0x1F), (((rgba32)>>31) & 1))

    uint16_t *fb_base = (uint16_t*)0xA0100000;
    volatile uint32_t* regs = (uint32_t*)0xA4400000;
    regs[1] = (uint32_t)fb_base;
    for (int i=0; i<320*240; i++) 
        fb_base[i] = RGBA32(0xDF8A7B);

    regs[2] = 320;
    regs[12] = 0x200;
    regs[13] = 0x400;

#if FATAL_TEXT
    const int RES_WIDTH = 320;
    const int X = 40;
    const int Y = 40;
    const uint16_t COLOR = RGBA32(0xF3F9D2);

    fb_base += Y*RES_WIDTH + X;
    uint16_t *fb = fb_base;
    while (1) {
        char ch = io_read8((uint32_t)str); str++;
        if (!ch) break;
        const uint8_t *glyph = font + (ch-1)*5;
        for (int x=0; x<5; x++) {
            uint8_t g = io_read8((uint32_t)glyph);
            for (int y=0; y<8; y++) {
                if (g & (1 << y))
                    fb[RES_WIDTH*y] = COLOR;
            }
            fb++;
            glyph++;
        }

        fb += 2; // spacing
    }

#endif
    int tv_type = io_read8(0xA4000009);
    bool ique = io_read8(0xA400000B);
    #pragma GCC unroll 0
    for (int reg=0; reg<7; reg++)
        regs[reg+5] = vi_regs_p[tv_type][reg];
    regs[0] = ique ? 0x1202 : 0x3202;
#endif
    abort();
}

void __bss_check(void)
{
    extern char __bss_start[];
    void *bss_start = (void*)__bss_start;
    void *bss_end = (void*)__bss_end;
    for (uint32_t *p = bss_start; p < (uint32_t*)bss_end; p++)
        if (*p != 0)
            fatal(MSG_BSS_CHECK_ERROR);
}
/// @endcond

/* Inline instantiations */
extern inline uint8_t mem_read8(uint64_t vaddr);
extern inline uint16_t mem_read16(uint64_t vaddr);
extern inline uint32_t mem_read32(uint64_t vaddr);
extern inline uint64_t mem_read64(uint64_t vaddr);
extern inline bool sys_bbplayer(void);
