/**
 * @file n64sys.h
 * @brief N64 System Interface
 * @ingroup n64sys
 */
#ifndef __LIBDRAGON_N64SYS_H
#define __LIBDRAGON_N64SYS_H

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
#define CachedAddr(_addr) (((void *)(((unsigned long)(_addr))&~0x20000000))

#ifdef __cplusplus
extern "C" {
#endif

int sys_get_boot_cic();
void sys_set_boot_cic(int bc);
volatile unsigned long get_ticks( void );
volatile unsigned long get_ticks_ms( void );
void wait_ticks( unsigned long wait );
void wait_ms( unsigned long wait );

void data_cache_hit_invalidate(volatile void *, unsigned long);
void data_cache_hit_writeback(volatile void *, unsigned long);
void data_cache_hit_writeback_invalidate(volatile void *, unsigned long);
void data_cache_index_writeback_invalidate(volatile void *, unsigned long);
void inst_cache_hit_writeback(volatile void *, unsigned long);
void inst_cache_hit_invalidate(volatile void *, unsigned long);
void inst_cache_index_invalidate(volatile void *, unsigned long);

#ifdef __cplusplus
}
#endif

/**
 * @brief Number of updates to the count register per second
 *
 * The count register updates at "half maximum instruction issue rate".
 * This appears to be half the CPU clock.  Every second, this many counts
 * will have passed in the count register
 */
#define COUNTS_PER_SECOND (93750000/2)

/** @} */

#endif
