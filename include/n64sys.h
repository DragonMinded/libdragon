/**
 * @file n64sys.h
 * @brief N64 System Interface
 * @ingroup n64sys
 */
#ifndef __LIBDRAGON_N64SYS_H
#define __LIBDRAGON_N64SYS_H

#include <stdbool.h>

/**
 * @addtogroup n64sys
 * @{
 */

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
 * @brief Number of updates to the count register per second
 *
 * The count register updates at "half maximum instruction issue rate".
 * This appears to be half the CPU clock.  Every second, this many counts
 * will have passed in the count register
 */
#define COUNTS_PER_SECOND (93750000/2)

// Returns the signed difference of time between "from" and "to". If "from"
// is before "to", the distance in time is positive, otherwise it is
// negative .
#define TICKS_DISTANCE(from, to) ((int32_t)((uint32_t)(to) - (uint32_t)(from)))

// Returns true if "t1" is before "t2". This is similar to t1 < t2,
// but it correctly handles timer overflows which are very frequent.
//
// Notice that the N64 counter overflows every 90 seconds, so it's
// not possible to compare times that are more than 45 seconds apart.
#define TICKS_BEFORE(t1, t2) ({ TICKS_DISTANCE(t1, t2) > 0; })

// Reads the COP0 register $9 (count) into the provided variable.
// It will always produce a 32 bit unsigned value.
#define READ_TICKS(variable) do { \
    _Static_assert(sizeof(variable) >= 4, "target variable is narrower than 32 bits"); \
    asm volatile("mfc0 %0,$9":"=r"(variable)); \
} while(0)

#define WAIT_TICKS(wait) do { \
    unsigned int initial_tick; \
    unsigned int current_tick; \
    READ_TICKS(initial_tick); \
    READ_TICKS(current_tick); \
    while( current_tick - initial_tick < wait ) { \
        READ_TICKS(current_tick); \
    }; \
} while(0)

#ifdef __cplusplus
extern "C" {
#endif

int sys_get_boot_cic();
void sys_set_boot_cic(int bc);
/**
 * @brief Read the number of ticks since system startup
 *
 * @note This is the clock rate divided by two.
 * It will wrap back to 0 after about 91.6 seconds
 * Do not use for comparison without special handling
 *
 * @return The number of ticks since system startup
 */
static inline volatile unsigned long get_ticks(void)
{
    unsigned long count;
    READ_TICKS(count);
    return count;
}

/**
 * @brief Read the number of millisecounds since system startup
 *
 * It will wrap back to 0 after about 91.6 seconds
 * Do not use for comparison without special handling
 *
 * @return The number of millisecounds since system startup
 */
static inline volatile unsigned long get_ticks_ms(void)
{
    unsigned long count;
    READ_TICKS(count);
    return count / (COUNTS_PER_SECOND / 1000);
}

/**
 * @brief Spin wait until the number of ticks have elapsed
 *
 * @param[in] wait
 *            Number of ticks to wait
 *            Maximum accepted value is 0xFFFFFFFF ticks
 */
static inline void wait_ticks( unsigned long wait )
{
    WAIT_TICKS(wait);
}

/**
 * @brief Spin wait until the number of millisecounds have elapsed
 *
 * @param[in] wait
 *            Number of millisecounds to wait
 *            Maximum accepted value is 91625 ms
 */
static inline void wait_ms( unsigned long wait_ms )
{
    unsigned int wait = wait_ms * (COUNTS_PER_SECOND / 1000);
    WAIT_TICKS(wait);
}

void data_cache_hit_invalidate(volatile void *, unsigned long);
void data_cache_hit_writeback(volatile void *, unsigned long);
void data_cache_hit_writeback_invalidate(volatile void *, unsigned long);
void data_cache_index_writeback_invalidate(volatile void *, unsigned long);
void inst_cache_hit_writeback(volatile void *, unsigned long);
void inst_cache_hit_invalidate(volatile void *, unsigned long);
void inst_cache_index_invalidate(volatile void *, unsigned long);
int get_memory_size();
bool is_memory_expanded();

typedef enum {
    TV_PAL = 0,
    TV_NTSC = 1,
    TV_MPAL = 2
} tv_type_t;

tv_type_t get_tv_type();

#ifdef __cplusplus
}
#endif

/** @} */

#endif
