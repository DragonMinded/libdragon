/**
 * @file n64sys.h
 * @brief N64 System Interface
 * @ingroup n64sys
 */
#ifndef __LIBDRAGON_N64SYS_H
#define __LIBDRAGON_N64SYS_H

#include <stdbool.h>
#include <assert.h>
#include "cop0.h"
#include "cop1.h"

/**
 * @addtogroup n64sys
 * @{
 */

/**
 * @brief Frequency of the MIPS R4300 CPU
 */
#define CPU_FREQUENCY    93750000 


/**
 * @brief void pointer to cached and non-mapped memory start address
 */
#define KSEG0_START_ADDR ((void*)0x80000000)

/**
 * @brief Return the uncached memory address for a given address
 *
 * @param[in] _addr
 *            Address in RAM to convert to an uncached address
 * 
 * @return A void pointer to the uncached memory address in RAM
 */
#define UncachedAddr(_addr) ((void *)(((unsigned long)(_addr))|0x20000000))

/**
 * @brief Return the uncached memory address for a given address
 *
 * @param[in] _addr
 *            Address in RAM to convert to an uncached address
 * 
 * @return A short pointer to the uncached memory address in RAM
 */
#define UncachedShortAddr(_addr) ((short *)(((unsigned long)(_addr))|0x20000000))

/**
 * @brief Return the uncached memory address for a given address
 *
 * @param[in] _addr
 *            Address in RAM to convert to an uncached address
 * 
 * @return An unsigned short pointer to the uncached memory address in RAM
 */
#define UncachedUShortAddr(_addr) ((unsigned short *)(((unsigned long)(_addr))|0x20000000))

/**
 * @brief Return the uncached memory address for a given address
 *
 * @param[in] _addr
 *            Address in RAM to convert to an uncached address
 * 
 * @return A long pointer to the uncached memory address in RAM
 */
#define UncachedLongAddr(_addr) ((long *)(((unsigned long)(_addr))|0x20000000))

/**
 * @brief Return the uncached memory address for a given address
 *
 * @param[in] _addr
 *            Address in RAM to convert to an uncached address
 * 
 * @return An unsigned long pointer to the uncached memory address in RAM
 */
#define UncachedULongAddr(_addr) ((unsigned long *)(((unsigned long)(_addr))|0x20000000))

/**
 * @brief Return the cached memory address for a given address
 *
 * @param[in] _addr
 *            Address in RAM to convert to a cached address
 * 
 * @return A void pointer to the cached memory address in RAM
 */
#define CachedAddr(_addr) ((void *)(((unsigned long)(_addr))&~0x20000000))

/**
 * @brief Memory barrier to ensure in-order execution
 *
 * Since GCC seems to reorder volatile at -O2, a memory barrier is required
 * to ensure that DMA setup is done in the correct order.  Otherwise, the
 * library is useless at higher optimization levels.
 */
#define MEMORY_BARRIER() asm volatile ("" : : : "memory")

/**
 * @brief Returns the COP0 register $9 (count).
 *
 * The coprocessor 0 (system control coprocessor - COP0) register $9 is
 * incremented at "half maximum instruction issue rate" which is the processor
 * clock speed (93.75MHz) divided by two. (also see TICKS_PER_SECOND) It will
 * always produce a 32-bit unsigned value which overflows back to zero in
 * 91.625 seconds. The counter will increment irrespective of instructions
 * actually being executed. This macro is for reading that value.
 * Do not use for comparison without special handling.
 */
#define TICKS_READ() C0_COUNT()

/**
 * @brief Number of updates to the count register per second
 *
 * Every second, this many counts will have passed in the count register
 */
#define TICKS_PER_SECOND (CPU_FREQUENCY/2)

/**
 * @brief The signed difference of time between "from" and "to".
 *
 * If "from" is before "to", the distance in time is positive,
 * otherwise it is negative.
 */
#define TICKS_DISTANCE(from, to) ((int32_t)((uint32_t)(to) - (uint32_t)(from)))

/**
 * @brief Returns true if "t1" is before "t2".
 *
 * This is similar to t1 < t2, but it correctly handles timer overflows
 * which are very frequent. Notice that the N64 counter overflows every
 * ~91 seconds, so it's not possible to compare times that are more than
 * ~45 seconds apart.
 */
#define TICKS_BEFORE(t1, t2) ({ TICKS_DISTANCE(t1, t2) > 0; })

/**
 * @brief Returns equivalent count ticks for the given millis.
 */
#define TICKS_FROM_MS(val) ((uint32_t)((val) * (TICKS_PER_SECOND / 1000)))

#ifdef __cplusplus
extern "C" {
#endif

int sys_get_boot_cic();
void sys_set_boot_cic(int bc);
/**
 * @brief Read the number of ticks since system startup
 *
 * @note Also see the TICKS_READ macro
 *
 * @return The number of ticks since system startup
 */
static inline volatile unsigned long get_ticks(void)
{
    return TICKS_READ();
}

/**
 * @brief Read the number of millisecounds since system startup
 *
 * @note It will wrap back to 0 after about 91.6 seconds.
 * Also see the TICKS_READ macro. Do not use for comparison
 * without special handling.
 *
 * @return The number of millisecounds since system startup
 */
static inline volatile unsigned long get_ticks_ms(void)
{
    return TICKS_READ() / (TICKS_PER_SECOND / 1000);
}

void wait_ticks( unsigned long wait );
void wait_ms( unsigned long wait_ms );

/**
 * @brief Force a data cache invalidate over a memory region
 *
 * Use this to force the N64 to update cache from RDRAM.
 *
 * The cache is made by cachelines of 16 bytes. If a memory region is invalidated
 * and the memory region is not fully aligned to cachelines, a larger area
 * than that requested will be invalidated; depending on the arrangement of
 * the data segments and/or heap, this might make data previously
 * written by the CPU in regular memory locations to be unexpectedly discarded,
 * causing bugs.
 *
 * For this reason, this function must only be called with an address aligned
 * to 16 bytes, and with a length which is an exact multiple of 16 bytes; it
 * will assert otherwise.
 *
 * As an alternative, consider using #data_cache_hit_writeback_invalidate,
 * that first writebacks the affected cachelines to RDRAM, guaranteeing integrity
 * of memory areas that share cachelines with the region that must be invalidated.
 *
 * @param[in] addr_
 *            Pointer to memory in question
 * @param[in] sz_
 *            Length in bytes of the data pointed at by addr
 */
#define data_cache_hit_invalidate(addr_, sz_) ({ \
	void *addr = (addr_); unsigned long sz = (sz_); \
	assert(((uint32_t)addr % 16) == 0 && (sz % 16) == 0); \
	__data_cache_hit_invalidate(addr, sz); \
})

void __data_cache_hit_invalidate(volatile void * addr, unsigned long length);
void data_cache_hit_writeback(volatile const void *, unsigned long);
void data_cache_hit_writeback_invalidate(volatile void *, unsigned long);
void data_cache_index_writeback_invalidate(volatile void *, unsigned long);
void data_cache_writeback_invalidate_all(void);
void inst_cache_hit_writeback(volatile const void *, unsigned long);
void inst_cache_hit_invalidate(volatile void *, unsigned long);
void inst_cache_index_invalidate(volatile void *, unsigned long);
void inst_cache_invalidate_all(void);

int get_memory_size();
bool is_memory_expanded();

/** @brief Type of TV video output */
typedef enum {
    TV_PAL = 0,      ///< Video output is PAL
    TV_NTSC = 1,     ///< Video output is NTSC
    TV_MPAL = 2      ///< Video output is M-PAL
} tv_type_t;

tv_type_t get_tv_type();

#ifdef __cplusplus
}
#endif

/** @} */

#endif
