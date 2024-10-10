/**
 * @file n64sys.h
 * @brief N64 System Interface
 * @ingroup n64sys
 */
#ifndef __LIBDRAGON_N64SYS_H
#define __LIBDRAGON_N64SYS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "cop0.h"
#include "cop1.h"

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

/**
 * @brief Indicates whether we are running on a vanilla N64 or a iQue player
 */
extern int __boot_consoletype;

/**
 * @brief Frequency of the RCP
 */
#define RCP_FREQUENCY    (__boot_consoletype ? 96000000 : 62500000)

/**
 * @brief Frequency of the MIPS R4300 CPU
 */
#define CPU_FREQUENCY    (__boot_consoletype ? 144000000 : 93750000)

/**
 * @brief void pointer to cached and non-mapped memory start address
 */
#define KSEG0_START_ADDR ((void*)0x80000000)

/** 
 * @brief A physical address on the MIPS bus.
 * 
 * Physical addresses are 32-bit wide, and are used to address the memory
 * space of the MIPS R4300 CPU. The MIPS R4300 CPU has a 32-bit address bus,
 * and can address up to 4 GiB of memory.
 * 
 * Physical addresses are just numbers, they cannot be used as pointers (dereferenced).
 * To access them, you must first convert them virtual addresses using the
 * #VirtualCachedAddr or #VirtualUncachedAddr macros.
 * 
 * In general, libdragon will try to use #phys_addr_t whenever a physical
 * address is expected or returned, and C pointers for virtual addresses.
 * Unfortunately, not all codebase can be changed to follow this convention
 * for backward compatibility reasons.
 */
typedef uint32_t phys_addr_t;

/**
 * @brief Return the physical memory address for a given virtual address (pointer)
 *
 * @param[in] _addr     Virtual address to convert to a physical address
 * 
 * @return A phys_addr_t containing the physical memory address
 */
#define PhysicalAddr(_addr) ({ \
    const volatile void *_addrp = (_addr); \
    (((phys_addr_t)(_addrp))&~0xE0000000); \
})

/**
 * @brief Create a virtual addresses in a cached segment to access a physical address
 * 
 * This macro creates a virtual address that can be used to access a physical
 * address in the cached segment of the memory. The cached segment is the
 * segment of memory that is cached by the CPU, and is the default segment
 * for all memory accesses.
 * 
 * The virtual address created by this macro can be used as a pointer in C
 * to access the physical address.
 *
 * @param[in] _addr     Physical address to convert to a virtual address
 * 
 * @return A void pointer to the cached memory address
 */
#define VirtualCachedAddr(_addr) ((void *)(((unsigned long)(_addr))|0x80000000))

/**
 * @brief Create a virtual addresses in an uncached segment to access a physical address
 * 
 * This macro creates a virtual address that can be used to access a physical
 * address in the uncached segment of the memory. The uncached segment is the
 * segment of memory that is not cached by the CPU, and is used for memory
 * that is accessed by hardware devices, like the RCP.
 * 
 * The virtual address created by this macro can be used as a pointer in C
 * to access the physical address.
 *
 * @param[in] _addr     Physical address to convert to a virtual address
 * 
 * @return A void pointer to the uncached memory address
 */
#define VirtualUncachedAddr(_addr) ((void *)(((unsigned long)(_addr))|0xA0000000))


/**
 * @brief Return the uncached memory address for a given virtual address
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

/** @brief Symbol at the start of code (start of ROM contents after header) */
extern char __libdragon_text_start[];

/** @brief Symbol at the end of code, data, and sdata (set by the linker) */
extern char __rom_end[];

/** @brief Symbol at the end of code, data, sdata, and bss (set by the linker) */
extern char __bss_end[];

/**
 * @brief Void pointer to the start of heap memory
 */
#define HEAP_START_ADDR ((void*)__bss_end)

/**
 * @brief Memory barrier to ensure in-order execution
 *
 * Since GCC seems to reorder volatile at -O2, a memory barrier is required
 * to ensure that DMA setup is done in the correct order.  Otherwise, the
 * library is useless at higher optimization levels.
 */
#define MEMORY_BARRIER() asm volatile ("" : : : "memory")

/**
 * @brief Returns the 32-bit hardware tick counter
 *
 * This macro returns the current value of the hardware tick counter,
 * present in the CPU coprocessor 0. The counter increments at half of the
 * processor clock speed (see #TICKS_PER_SECOND), and overflows every
 * 91.625 seconds.
 * 
 * It is fine to use this hardware counter for measuring small time intervals,
 * as long as #TICKS_DISTANCE or #TICKS_BEFORE are used to compare different
 * counter reads, as those macros correctly handle overflows.
 * 
 * Most users might find more convenient to use #get_ticks(), a similar function
 * that returns a 64-bit counter with the same frequency that never overflows.
 * 
 * @see #TICKS_BEFORE
 * @see #TICKS_DISTANCE
 * @see #get_ticks
 */
#define TICKS_READ() C0_COUNT()

/**
 * @brief Number of updates to the count register per second
 *
 * Every second, this many counts will have passed in the count register
 */
#define TICKS_PER_SECOND (CPU_FREQUENCY/2)

/**
 * @brief Calculate the time passed between two ticks
 *
 * If "from" is before "to", the distance in time is positive,
 * otherwise it is negative.
 */
#define TICKS_DISTANCE(from, to) ((int32_t)((uint32_t)(to) - (uint32_t)(from)))

/** @brief Return how much time has passed since the instant t0. */
#define TICKS_SINCE(t0)          TICKS_DISTANCE(t0, TICKS_READ())

/**
 * @brief Returns true if "t1" is before "t2".
 *
 * This is similar to t1 < t2, but it correctly handles timer overflows
 * which are very frequent. Notice that the hardware counter overflows every
 * ~91 seconds, so it's not possible to compare times that are more than
 * ~45 seconds apart.
 * 
 * Use #get_ticks() to get a 64-bit counter that never overflows.
 * 
 * @see #get_ticks
 */
#define TICKS_BEFORE(t1, t2) ({ TICKS_DISTANCE(t1, t2) > 0; })

/**
 * @brief Returns equivalent count ticks for the given milliseconds.
 */
#define TICKS_FROM_MS(val) (((val) * (TICKS_PER_SECOND / 1000)))

/**
 * @brief Returns equivalent count ticks for the given microseconds.
 */
#define TICKS_FROM_US(val) (((val) * (8 * TICKS_PER_SECOND / 1000000) / 8))

/**
 * @brief Returns equivalent count ticks for the given microseconds.
 */
#define TICKS_TO_US(val) (((val) * 8 / (8 * TICKS_PER_SECOND / 1000000)))

/**
 * @brief Returns equivalent count ticks for the given microseconds.
 */
#define TICKS_TO_MS(val) (((val) / (TICKS_PER_SECOND / 1000)))


#ifdef __cplusplus
extern "C" {
#endif

/** @brief Return true if we are running on a iQue player */
inline bool sys_bbplayer(void) {
    extern int __boot_consoletype;
    return __boot_consoletype != 0;
}

/**
 * @brief Read the number of ticks since system startup
 *
 * The frequency of this counter is #TICKS_PER_SECOND. The counter will
 * never overflow, being a 64-bit number.
 *
 * @return The number of ticks since system startup
 */
uint64_t get_ticks(void);

/**
 * @brief Read the number of microseconds since system startup
 *
 * This is similar to #get_ticks, but converts the result in integer
 * microseconds for convenience.
 * 
 * @return The number of microseconds since system startup
 */
uint64_t get_ticks_us(void);

/**
 * @brief Read the number of millisecounds since system startup
 * 
 * This is similar to #get_ticks, but converts the result in integer
 * milliseconds for convenience.
 * 
 * @return The number of millisecounds since system startup
 */
uint64_t get_ticks_ms(void);

/**
 * @brief Spin wait until the number of ticks have elapsed
 *
 * @param[in] wait
 *            Number of ticks to wait
 *            Maximum accepted value is 0xFFFFFFFF ticks
 */
void wait_ticks( unsigned long wait );

/**
 * @brief Spin wait until the number of milliseconds have elapsed
 *
 * @param[in] wait_ms
 *            Number of milliseconds to wait
 *            Maximum accepted value is 91625 ms
 */
void wait_ms( unsigned long wait_ms );

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
__attribute__((noreturn)) 
void die(void);

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
void data_cache_hit_writeback(volatile const void *, unsigned long);

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
void data_cache_hit_writeback_invalidate(volatile void *, unsigned long);

/**
 * @brief Force a data cache index writeback invalidate over a memory region
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void data_cache_index_writeback_invalidate(volatile void *, unsigned long);

/**
 * @brief Force a data cache writeback invalidate over whole memory
 *
 * Also see #data_cache_hit_writeback_invalidate
 *
 */
void data_cache_writeback_invalidate_all(void);

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
void inst_cache_hit_writeback(volatile const void *, unsigned long);

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
void inst_cache_hit_invalidate(volatile void *, unsigned long);

/**
 * @brief Force an instruction cache index invalidate over a memory region
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void inst_cache_index_invalidate(volatile void *, unsigned long);

/**
 * @brief Force an instruction cache invalidate over whole memory
 *
 * Also see #inst_cache_hit_invalidate
 *
 */
void inst_cache_invalidate_all(void);


/**
 * @brief Get amount of available memory.
 *
 * @return amount of total available memory in bytes.
 */
int get_memory_size();

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
bool is_memory_expanded();

/**
 * @brief Heap statistics
 */
typedef struct {
    int total;      ///< Total heap size in bytes
    int used;       ///< Used heap size in bytes
} heap_stats_t;

/**
 * @brief Return information about memory usage of the heap
 */
void sys_get_heap_stats(heap_stats_t *stats);

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
void *malloc_uncached(size_t size);

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
void *malloc_uncached_aligned(int align, size_t size);

/**
 * @brief Free an uncached memory buffer
 * 
 * This function frees a memory buffer previously allocated via #malloc_uncached.
 * 
 * @param[in]  buf  The buffer to free
 * 
 * @see #malloc_uncached
 */
void free_uncached(void *buf);

/** @brief Type of TV video output */
typedef enum {
    TV_PAL = 0,      ///< Video output is PAL
    TV_NTSC = 1,     ///< Video output is NTSC
    TV_MPAL = 2      ///< Video output is M-PAL
} tv_type_t;

/**
 * @brief Is system NTSC/PAL/MPAL
 * 
 * Checks enum hard-coded in PIF BootROM to indicate the tv type of the system.
 * 
 * @return enum value indicating PAL, NTSC or MPAL
 */
tv_type_t get_tv_type();

/** @brief Reset types */
typedef enum {
    RESET_COLD = 0,  ///< Cold reset (power on)
    RESET_WARM = 1,  ///< Warm reset (reset button)
} reset_type_t;

/** 
 * @brief Get reset type
 * 
 * This function returns the reset type, that can be used to differentiate
 * a cold boot from a warm boot (that is, after pressing the reset button).
 * 
 * For instance, a game might want to skip mandatory intros (eg: logos)
 * on a warm boot.
 */
reset_type_t sys_reset_type(void);

/**
 * @name 64-bit address space access
 * @brief Functions to access the full 64-bit address space
 *
 * Libdragon uses the O64 ABI, in which pointers are 32-bit wide. This
 * is the right choice for basically all standard use cases because
 * doubling the size of the pointers would waste more memory in all data
 * structures where pointers are stored.
 * 
 * The VR4300 CPU does support a full 64-bit virtual address space
 * though, which might be used for some very niche use case
 * (like e.g. emulator tests) Since it is not possible to create a
 * 64-bit pointer in C because of the chosen ABI, these functions
 * are provided in substitution.
 * 
 * The virtual address must be provided as a 64-bit integer.
 * 
 * @{
 */

/**
 * @brief Read a 8-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint8_t mem_read8(uint64_t vaddr) {
    uint8_t value;
    asm volatile (
        "lbu %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Read a 16-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint16_t mem_read16(uint64_t vaddr) {
    uint16_t value;
    asm volatile (
        "lhu %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Read a 32-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint32_t mem_read32(uint64_t vaddr) {
    uint32_t value;
    asm volatile (
        "lwu %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Read a 64-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint64_t mem_read64(uint64_t vaddr) {
    uint64_t value;
    asm volatile (
        "ld %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/** @} */

/** @cond */
/* Deprecated version of get_ticks */
__attribute__((deprecated("use get_ticks instead")))
static inline volatile unsigned long read_count(void) {
    return get_ticks();
}
/** @endcond */

#ifdef __cplusplus
}
#endif

/** @} */

#endif
