/**
 * @file n64sys.c
 * @brief N64 System Interface
 * @ingroup n64sys
 */
#include "n64sys.h"

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
 * @brief Read the number of ticks since system startup
 *
 * @note This is the clock rate divided by two.
 *
 * @return The number of ticks since system startup
 */
volatile unsigned long get_ticks(void)
{
    unsigned long count;
    // reg $9 on COP0 is count
    asm("\tmfc0 %0,$9\n\tnop":"=r"(count));

    return count;
}

/**
 * @brief Read the number of millisecounds since system startup
 *
 * @return The number of millisecounds since system startup
 */
volatile unsigned long get_ticks_ms(void)
{
    unsigned long count;
    // reg $9 on COP0 is count
    asm("\tmfc0 %0,$9\n\tnop":"=r"(count));

    return count / (COUNTS_PER_SECOND / 1000);
}

/**
 * @brief Spin wait until the number of ticks have elapsed
 *
 * @param[in] wait
 *            Number of ticks to wait
 */
void wait_ticks( unsigned long wait )
{
    unsigned int stop = wait + get_ticks();

    while( stop > get_ticks() );
}

/**
 * @brief Spin wait until the number of millisecounds have elapsed
 *
 * @param[in] wait
 *            Number of millisecounds to wait
 */
void wait_ms( unsigned long wait )
{
    unsigned int stop = wait + get_ticks_ms();

    while( stop > get_ticks_ms() );
}

/**
 * @brief Helper macro to perform cache refresh operations
 *
 * @param[in] op
 *            Operation to perform
 */
#define cache_op(op) \
    addr=(void*)(((unsigned long)addr)&(~3));\
    for (;length>0;length-=4,addr+=4) \
	asm ("\tcache %0,(%1)\n"::"i" (op), "r" (addr))

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
void data_cache_hit_writeback(volatile void * addr, unsigned long length)
{
    cache_op(0x19);
}

/**
 * @brief Force a data cache invalidate over a memory region
 *
 * Use this to force the N64 to update cache from RDRAM.
 *
 * @param[in] addr
 *            Pointer to memory in question
 * @param[in] length
 *            Length in bytes of the data pointed at by addr
 */
void data_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x11);
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
    cache_op(0x15);
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
    cache_op(0x01);
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
void inst_cache_hit_writeback(volatile void * addr, unsigned long length)
{
    cache_op(0x18);
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
    cache_op(0x10);
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
    cache_op(0x00);
}

/** @} */
