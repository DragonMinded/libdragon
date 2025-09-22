/**
 * @file n64sys.c
 * @author Jennifer Taylor <dragonminded@dragonminded.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
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

/** @brief Records whether the user called a function to check for the presence of expanded memory */
bool __expanded_memory_asserted = false;

/** @brief Last tick at which the 64-bit counter was updated */
static uint32_t ticks64_base_tick;

/** @brief Last value of the 64-bit counter */
static uint64_t ticks64_base;

/// @cond

// Instruction cache defines.
#define CACHE_INST_FLAG                 (0)
#define CACHE_INST_SIZE                 (16 * 1024)
#define CACHE_INST_LINESIZE             (32)
// Data cache defines.
#define CACHE_DATA_FLAG                 (1)
#define CACHE_DATA_SIZE                 (8 * 1024)
#define CACHE_DATA_LINESIZE             (16)
// Cache ops described in VR4300 manual on page 404.
#define INDEX_INVALIDATE                (0)
#define INDEX_LOAD_TAG                  (1)
#define INDEX_STORE_TAG                 (2)
#define INDEX_CREATE_DIRTY              (3)
#define HIT_INVALIDATE                  (4)
#define HIT_WRITEBACK_INVALIDATE        (5)
#define HIT_WRITEBACK                   (6)

/// @endcond

/**
 * @brief Helper macro to create the opcode for a cache operation.
 * Operation is encoded in 5 bits where bits 4..2 contain the operation type
 * and bits 0..1 determine the cache that is to be operated on. 
 * 
 * @param[in] op
 *            Operation type to perform.
 * @param[in] cache
 *            Specific cache to perform the operation on.
 */
#define build_opcode(op, cache)             (((op) << 2) | (cache))

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
    cache_op(build_opcode(HIT_WRITEBACK, CACHE_DATA_FLAG), CACHE_DATA_LINESIZE);
}

void data_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    assert(((uint32_t)addr % 16) == 0 && (length % 16) == 0);
    cache_op(build_opcode(HIT_INVALIDATE, CACHE_DATA_FLAG), CACHE_DATA_LINESIZE);
}

void data_cache_hit_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(build_opcode(HIT_WRITEBACK_INVALIDATE, CACHE_DATA_FLAG), CACHE_DATA_LINESIZE);
}

void data_cache_index_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(build_opcode(INDEX_INVALIDATE, CACHE_DATA_FLAG), CACHE_DATA_LINESIZE);
}

void data_cache_writeback_invalidate_all(void)
{
    data_cache_index_writeback_invalidate(KSEG0_START_ADDR, 0x2000);
}

void inst_cache_hit_writeback(volatile const void * addr, unsigned long length)
{
    cache_op(build_opcode(HIT_WRITEBACK, CACHE_INST_FLAG), CACHE_INST_LINESIZE);
}

void inst_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(build_opcode(HIT_INVALIDATE, CACHE_INST_FLAG), CACHE_INST_LINESIZE);
}

void inst_cache_index_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(build_opcode(INDEX_INVALIDATE, CACHE_INST_FLAG), CACHE_INST_LINESIZE);
}

void inst_cache_invalidate_all(void)
{
    inst_cache_index_invalidate(KSEG0_START_ADDR, 0x4000);
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

int get_memory_size(void)
{
    // If the application checked the size of the memory, we can assume that
    // they know what they are doing and we can avoid emitting the expansion
    // pak warning.
    __expanded_memory_asserted = true;
    return __boot_memsize;
}

bool is_memory_expanded(void)
{
    return get_memory_size() >= 0x7C0000;
}

void assert_memory_expanded(void)
{
    bool expanded = is_memory_expanded();
    if (sys_bbplayer()) 
        assertf(expanded, 
            "This application requires more memory to run. Please allocate 8 MiB (0x800000 bytes) in the ticket and run it again.");
    else
        assertf(expanded, 
            "This application requires the expansion pak to run. Please insert the expansion pak and restart the console.");
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

void sys_get_heap_stats(heap_stats_t *stats)
{
    extern int __heap_total_size;
    struct mallinfo m = mallinfo();

    stats->total = __heap_total_size;
    stats->used = m.uordblks;
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
