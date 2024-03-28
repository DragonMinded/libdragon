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

/**
 * @defgroup n64sys N64 System Interface
 * @ingroup lowlevel
 * @brief N64 bootup and cache interfaces.
 *
 * The N64 system interface provides a way for code to interact with
 * the memory setup on the system.  This includes cache operations to
 * invalidate or flush regions and the ability to set the boot CIC.
 * The @ref system use the knowledge of the boot CIC to properly determine
 * if the expansion pak is present, giving 4 MiB of additional memory.  Aside
 * from this, the MIPS r4300 uses a manual cache management strategy, where
 * SW that requires passing buffers to and from hardware components using
 * DMA controllers needs to ensure that cache and RDRAM are in sync.  A
 * set of operations to invalidate and/or write back cache is provided for
 * both instruction cache and data cache.
 * @{
 */

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

/**
 * @brief Force a data cache writeback over a memory region
 *
 * Use this to force cached memory to be written to RDRAM.
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void data_cache_hit_writeback(volatile const void * addr, unsigned long length)
{
    cache_op(0x19, 16);
}

/** @brief Underlying implementation of data_cache_hit_invalidate */
void __data_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x11, 16);
}

/**
 * @brief Force a data cache writeback invalidate over a memory region
 *
 * Use this to force cached memory to be written to RDRAM
 * and then invalidate the corresponding cache lines.
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void data_cache_hit_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x15, 16);
}

/**
 * @brief Force a data cache index writeback invalidate over a memory region
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void data_cache_index_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x01, 16);
}

/**
 * @brief Force a data cache writeback invalidate over whole memory
 *
 * Also see #data_cache_hit_writeback_invalidate
 *
 */
void data_cache_writeback_invalidate_all(void)
{
    // TODO: do an index op instead for better performance
    data_cache_hit_writeback_invalidate(KSEG0_START_ADDR, get_memory_size());
}

/**
 * @brief Force an instruction cache writeback over a memory region
 *
 * Use this to force cached memory to be written to RDRAM.
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void inst_cache_hit_writeback(volatile const void * addr, unsigned long length)
{
    cache_op(0x18, 32);
}

/**
 * @brief Force an instruction cache invalidate over a memory region
 *
 * Use this to force the N64 to update cache from RDRAM.
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void inst_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x10, 32);
}

/**
 * @brief Force an instruction cache index invalidate over a memory region
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void inst_cache_index_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x00, 32);
}

/**
 * @brief Force an instruction cache invalidate over whole memory
 *
 * Also see #inst_cache_hit_invalidate
 *
 */
void inst_cache_invalidate_all(void)
{
    // TODO: do an index op instead for better performance
    inst_cache_hit_invalidate(KSEG0_START_ADDR, get_memory_size());
}

/**
 * @brief Allocate a buffer that will be accessed as uncached memory.
 * 
 * This function allocates a memory buffer that can be safely read and written
 * through uncached memory accesses only. It makes sure that that the buffer
 * does not share any cacheline with other buffers in the heap, and returns
 * a pointer in the uncached segment (0xA0000000).
 * 
 * The buffer contents are uninitialized.
 * 
 * To free the buffer, use #free_uncached.
 * 
 * @param[in]  size  The size of the buffer to allocate
 *
 * @return a pointer to the start of the buffer (in the uncached segment)
 * 
 * @see #free_uncached
 */
void *malloc_uncached(size_t size)
{
    return malloc_uncached_aligned(16, size);
}

/**
 * @brief Allocate a buffer that will be accessed as uncached memory, specifying alignment
 * 
 * This function is similar to #malloc_uncached, but allows to force a higher
 * alignment to the buffer (just like memalign does). See #malloc_uncached
 * for reference.
 * 
 * @param[in]  align The alignment of the buffer in bytes (eg: 64)
 * @param[in]  size  The size of the buffer to allocate
 * 
 * @return a pointer to the start of the buffer (in the uncached segment)
 * 
 * @see #malloc_uncached 
 */
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

/**
 * @brief Free an uncached memory buffer
 * 
 * This function frees a memory buffer previously allocated via #malloc_uncached.
 * 
 * @param[in]  buf  The buffer to free
 * 
 * @see #malloc_uncached
 */
void free_uncached(void *buf)
{
    free(CachedAddr(buf));
}

/**
 * @brief Get amount of available memory.
 *
 * @return amount of total available memory in bytes.
 */
int get_memory_size()
{
    return __boot_memsize;
}

/**
 * @brief Is expansion pak in use.
 *
 * Checks whether the maximum available memory has been expanded to 8 MiB
 *
 * @return true if expansion pak detected, false otherwise.
 * 
 * @note On iQue, this function returns true only if the game has been assigned
 *       exactly 8 MiB of RAM.
 */
bool is_memory_expanded()
{
    return get_memory_size() >= 0x7C0000;
}


/**
 * @brief Is system NTSC/PAL/MPAL
 * 
 * Checks enum hard-coded in PIF BootROM to indicate the tv type of the system.
 * 
 * @return enum value indicating PAL, NTSC or MPAL
 */
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

/** @} */

/* Inline instantiations */
extern inline uint8_t mem_read8(uint64_t vaddr);
extern inline uint16_t mem_read16(uint64_t vaddr);
extern inline uint32_t mem_read32(uint64_t vaddr);
extern inline uint64_t mem_read64(uint64_t vaddr);
extern inline bool sys_bbplayer(void);
