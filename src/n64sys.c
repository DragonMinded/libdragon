/**
 * @file n64sys.c
 * @brief N64 System Interface
 * @ingroup n64sys
 */

#include <stdint.h>
#include <assert.h>
#include <malloc.h>
#include "n64sys.h"
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
 * if the expansion pak is present, giving 4MB of additional memory.  Aside
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
int __bbplayer = 0;

/** @brief Return true if we are running on a iQue player */
bool sys_bbplayer(void) {
    return __bbplayer != 0;
}

/** 
 * @brief Boot CIC 
 *
 * Defaults to 6102.
 */
int __bootcic = 6102;

/**
 * @brief Return the boot CIC
 *
 * @return The boot CIC as an integer
 */
int sys_get_boot_cic()
{
    return __bootcic;
}

/**
 * @brief Set the boot CIC
 *
 * This function will set the boot CIC.  If the value isn't in the range
 * of 6102-6106, the boot CIC is set to the default of 6102.
 *
 * @param[in] bc
 *            Boot CIC value
 */
void sys_set_boot_cic(int bc)
{
    __bootcic = ( (bc >= 6102) && (bc <= 6106) ) ? bc : 6102;
}

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
 * Use this to force cached memory to be written to RDRAM and then cache updated.
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
    // Since we will be accessing the buffer as uncached memory, we absolutely
    // need to prevent part of it to ever enter the data cache, even as false
    // sharing with contiguous buffers. So we want the buffer to exclusively
    // cover full cachelines (aligned to 16 bytes, multiple of 16 bytes).
    size = ROUND_UP(size, 16);
    void *mem = memalign(16, size);
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
    if (sys_bbplayer()) {
        /* On iQue, memory allocated to the game can be decided by the OS.
           Even if the memory is allocated as 8Mb, the top part handles
           save states (emulation of EEPROM/Flash/SRAM), so we should avoid
           writing there anyway. See also entrypoint.S which sets up the
           stack with the same logic. */
        int size = (*(int*)0xA0000318);
        if (size == 0x800000)
            size = 0x7C0000;
        return size;
    } 
    return (__bootcic != 6105) ? (*(int*)0xA0000318) : (*(int*)0xA00003F0);
}

/**
 * @brief Is expansion pak in use.
 *
 * Checks whether the maximum available memory has been expanded to 8MB
 *
 * @return true if expansion pak detected, false otherwise.
 * 
 * @note On iQue, this function returns true only if the game has been assigned
 *       exactly 8MB of RAM.
 */
bool is_memory_expanded()
{
    return get_memory_size() >= 0x7C0000;
}

/** @brief Memory location to read which determines the TV type. */
#define TV_TYPE_LOC  0x80000300

/**
 * @brief Is system NTSC/PAL/MPAL
 * 
 * Checks enum hard-coded in PIF BootROM to indicate the tv type of the system.
 * 
 * @return enum value indicating PAL, NTSC or MPAL
 */
tv_type_t get_tv_type() 
{
    return *((uint32_t *) TV_TYPE_LOC);
}

/**
 * @brief Spin wait until the number of ticks have elapsed
 *
 * @param[in] wait
 *            Number of ticks to wait
 *            Maximum accepted value is 0xFFFFFFFF ticks
 */
void wait_ticks( unsigned long wait )
{
    unsigned int initial_tick = TICKS_READ();
    while( TICKS_READ() - initial_tick < wait );
}

/**
 * @brief Spin wait until the number of milliseconds have elapsed
 *
 * @param[in] wait_ms
 *            Number of milliseconds to wait
 *            Maximum accepted value is 91625 ms
 */
void wait_ms( unsigned long wait_ms )
{
    wait_ticks(TICKS_FROM_MS(wait_ms));
}


/**
 * @brief Initialize COP1 with default settings that prevent undesirable exceptions.
 *
 */
__attribute__((constructor)) void __init_cop1()
{
    /* Read initialized value from cop1 control register */
    uint32_t fcr31 = C1_FCR31();
    
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
