#ifndef __LIBDRAGON_N64SYS_H
#define __LIBDRAGON_N64SYS_H

#define UncachedAddr(_addr) ((void *)(((unsigned long)(_addr))|0x20000000))
#define UncachedShortAddr(_addr) ((short *)(((unsigned long)(_addr))|0x20000000))
#define UncachedUShortAddr(_addr) ((unsigned short *)(((unsigned long)(_addr))|0x20000000))
#define UncachedLongAddr(_addr) ((long *)(((unsigned long)(_addr))|0x20000000))
#define UncachedULongAddr(_addr) ((unsigned long *)(((unsigned long)(_addr))|0x20000000))

#define CachedAddr(_addr) (((void *)(((unsigned long)(_addr))&~0x20000000))

volatile unsigned long read_count(void);
void data_cache_invalidate(volatile void *, unsigned long);
void data_cache_writeback(volatile void *, unsigned long);
void data_cache_writeback_invalidate(volatile void *, unsigned long);
void inst_cache_invalidate(volatile void *, unsigned long);
void inst_cache_writeback(volatile void *, unsigned long);
void inst_cache_writeback_invalidate(volatile void *, unsigned long);
void enable_interrupts();
void disable_interrupts();

/* Count register updates at "half maximum instruction issue rate".
 * This appears to be half the CPU clock.
 */
#define COUNTS_PER_SECOND (93750000/2)

#endif
