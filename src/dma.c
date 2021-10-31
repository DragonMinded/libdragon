/**
 * @file dma.c
 * @brief DMA Controller
 * @ingroup dma
 */
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup dma DMA Controller
 * @ingroup lowlevel
 * @brief DMA functionality for transfers between cartridge space and RDRAM
 *
 * The DMA controller is responsible for handling block and word accesses from
 * the catridge domain.  Because of the nature of the catridge interface, code
 * cannot use memcpy or standard pointer accesses on memory mapped to the catridge.
 * Consequently, the peripheral interface (PI) provides a DMA controller for
 * accessing data.
 *
 * The DMA controller requires no initialization.  Using #dma_read and #dma_write
 * will allow reading from the cartridge and writing to the catridge respectively
 * in block mode.  #io_read and #io_write will allow a single 32-bit integer to
 * be read from or written to the cartridge.  These are especially useful for
 * manipulating registers on a cartridge such as a gameshark.  Code should never
 * make raw 32-bit reads or writes in the cartridge domain as it could collide with
 * an in-progress DMA transfer or run into caching issues.
 * @{
 */

/**
 * @name PI Status Register Bit Definitions
 * @{
 */
/** @brief PI DMA Busy */
#define PI_STATUS_DMA_BUSY ( 1 << 0 )
/** @brief PI IO Busy */
#define PI_STATUS_IO_BUSY  ( 1 << 1 )
/** @brief PI Error */
#define PI_STATUS_ERROR    ( 1 << 2 )
/** @} */

/** @brief Structure used to interact with the PI registers */
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

static volatile int __dma_busy(void)
{
    return PI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
}

/** 
 * @brief Return whether the DMA controller is currently busy
 *
 * @return nonzero if the DMA controller is busy or 0 otherwise
 */
volatile int dma_busy(void)
{
    return __dma_busy();
}

/**
 * @brief Start reading data from a peripheral through PI DMA (low-level)
 *
 * This function should be used when reading from a cartridge peripheral (typically
 * ROM). This function just begins executing a raw DMA transfer, which is
 * well-defined only for RAM addresses which are multiple of 8, ROM addresses
 * which are  multiple of 2, and lengths which are multiple of 2.
 * 
 * Use #dma_wait to wait for the end of the transfer.
 * 
 * See #dma_read_async for a higher level primitive which can perform almost
 * arbitrary transfers.
 *
 * @param[out] ram_address
 *             Pointer to a buffer to place read data (must be 8-byte aligned)
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from (must be 2-byte aligned)
 * @param[in]  len
 *             Length in bytes to read into ram_address (must be multiple of 2)
 */
void dma_read_raw_async(void * ram_address, unsigned long pi_address, unsigned long len) 
{
    assert(len > 0);

    disable_interrupts();

    while (__dma_busy()) ;
    MEMORY_BARRIER();
    PI_regs->ram_address = ram_address;
    MEMORY_BARRIER();
    PI_regs->pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    MEMORY_BARRIER();
    PI_regs->write_length = len-1;
    MEMORY_BARRIER();

    enable_interrupts();
}

/**
 * @brief Start writing data to a peripheral through PI DMA (low-level)
 *
 * This function should be used when writing to a cartridge peripheral (typically
 * ROM). This function just begins executing a raw DMA transfer, which is
 * well-defined only for RAM addresses which are multiple of 8, ROM addresses
 * which are  multiple of 2, and lengths which are multiple of 2.
 *
 * Use #dma_wait to wait for the end of the transfer.
 *
 *
 * @param[out] ram_address
 *             Pointer to a buffer to read data from (must be 8-byte aligned)
 * @param[in]  pi_address
 *             Memory address of the peripheral to write to (must be 2-byte aligned)
 * @param[in]  len
 *             Length in bytes to write into pi_address (must be multiple of 2)
 */
void dma_write_raw_async(const void * ram_address, unsigned long pi_address, unsigned long len) 
{
    assert(len > 0);

    disable_interrupts();

    while (__dma_busy()) ;
    MEMORY_BARRIER();
    PI_regs->ram_address = (void*)ram_address;
    MEMORY_BARRIER();
    PI_regs->pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    MEMORY_BARRIER();
    PI_regs->read_length = len-1;
    MEMORY_BARRIER();

    enable_interrupts();
}

/** @brief Low-level 32-bit aligned ROM read.
 * 
 * @note This function must be called with interrupts disabled.
 */
static uint32_t __io_read32(unsigned long pi_address) {
    while (__dma_busy()) {}
    MEMORY_BARRIER();
    return *(volatile uint32_t*)pi_address;
}

/** @brief Low-level 32-bit unaligned PI ROM read.
 * 
 * @note This function must be called with interrupts disabled.
 */
static uint32_t __io_read32u(unsigned long pi_address) {
    uint32_t val = 0;

    // We need to manually issue assembly opcodes here because we must
    // wait for #dma_busy inbetween. So we can't simply tell GCC to 
    // generate the LWL/LWR pair via __attribute__((aligned(1)), otherwise
    // we would only be able to wait once before both of them.
    while (__dma_busy()) {}
    MEMORY_BARRIER();
    __asm volatile("lwl %0,0(%1)" : "+r"(val) : "r"(pi_address));    

    while (__dma_busy()) {}
    MEMORY_BARRIER();
    __asm volatile("lwr %0,3(%1)" : "+r"(val) : "r"(pi_address));

    return val;
}

/** @brief Low-level 16-bit aligned PI ROM read.
 * 
 * 16-bit PI ROM reads are undocumented. Testing on real hardware shows
 * that they only work for 32-bit aligned addresses, so this function
 * falls back to a full 32bit read for misaligned addresses.
 * 
 * @note This function must be called with interrupts disabled.
 */
static uint16_t __io_read16(unsigned long pi_address) {
    if (pi_address & 2) {
        return (uint16_t)__io_read32(pi_address^2);
    } else {
        while (__dma_busy()) {}
        MEMORY_BARRIER();
        return *(volatile uint16_t*)pi_address;
    }
}


/** @brief Low-level 8-bit PI ROM read.
 * 
 * 8-bit PI ROM reads are undocumented. Testing on real hardware shows
 * that they do not consistently work, so this function falls back to using
 * 16-bit reads and extracting the requested byte.
 * 
 * @note This function must be called with interrupts disabled.
 */
static uint8_t __io_read8(unsigned long pi_address) {
    if (pi_address&1)
        return (uint8_t)__io_read16(pi_address^1);
    else
        return __io_read16(pi_address)>>8;
}

/**
 * @brief Start reading data from a peripheral through PI DMA
 *
 * This function must be used when reading a chunk of data from a cartridge 
 * peripheral (typically, ROM). It is a wrapper over #dma_read_raw_async that allows
 * arbitrary aligned addresses and any length (including odd sizes). For
 * fully-aligned addresses it quickly falls back to #dma_read_raw_async, so it can
 * be used generically as "default" PI DMA transfer function.
 * 
 * The only constraint on alignment is that the RAM and PI addresses must have
 * the same 1-bit misalignment, that is they must either be even addresses or
 * odd addresses. Notice that this function will assert if this constraint is
 * not respected.
 * 
 * Use #dma_wait to wait for the end of the transfer.
 *
 * For non performance sensitive tasks such as reading and parsing data from
 * ROM at loading time, a better option is to use DragonFS, where #dfs_read
 * falls back to a CPU memory copy to realign the data when required.
 * 
 * @param[out] ram_address
 *             Pointer to a buffer to place read data
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from
 * @param[in]  len
 *             Length in bytes to read into ram_address
 */
void dma_read_async(void *ram_address, unsigned long pi_address, unsigned long len)
{
    unsigned long ram = (unsigned long)UncachedAddr(ram_address);
    unsigned long rom = pi_address;

    assert(len > 0);
    assert(((ram ^ rom) & 1) == 0); 

    disable_interrupts();
    union { uint64_t mem64; uint32_t mem32[2]; uint16_t mem16[4]; uint8_t mem8[8]; } val;

    // Check if the RDRAM address is misaligned. If so, this requires some
    // handling as it's officially "not supported".
    int misalign = ram & 7;

    if (misalign) {
        if ((misalign&1) == 0 && len < 0x7F-misalign*2) {
            // Fast-path: RDRAM even-misaligned addresses work correctly
            // for small transfers (up to some magic value), though they transfer
            // less then requested. Tweak the length accordingly and do the transfer.
            len += misalign;
        } else {        
            // Manually transfer the first bytes, up to creating a 8-byte
            // alignment. The code is complicated by the fact that we can
            // only use uncached addresses here (to mimic DMA), which means
            // that we can only do 32-bit RDRAM accesses, and PI accesses
            // which have weird bugs.
            uint64_t *ram0 = (uint64_t*)(ram - misalign);
            ram += 8-misalign;

            // Fetch the initial (aligned) 8-byte word, then loop to transfer
            // the required bytes. Only some bytes of the word will be modified.
            val.mem64 = *ram0;

            do {
                // If we're at an odd address (or this is the last byte), read
                // 8-bit from PI into RDRAM, so that we immediately align
                // to do 2-bytes, and we can do 16-bit reads/writes later.
                if ((misalign & 1) || len == 1) {
                    val.mem8[misalign] = __io_read8(rom);
                    misalign += 1; rom += 1; len -= 1;
                } else {
                    val.mem16[misalign/2] = __io_read16(rom);
                    misalign += 2; rom += 2; len -= 2;
                }
            } while (misalign < 8 && len > 0);

            // Store back the modified 8-byte word.
            *ram0 = val.mem64;
        }
    }

    // Now the transfer is 8-byte aligned. Check the length: we can do odd-length
    // transfer (at aligned addresses) up to 0x7E bytes. For larger odd transfers,
    // we need to write the last odd byte ourselves, and we do that with a 32-bit
    // unaligned transfer (LWL/LWR + SWL/SWR).
    if ((len & 1) != 0 && len >= 0x7F) {
        typedef uint32_t u_uint32_t __attribute__((aligned(1)));
        *(u_uint32_t*)(ram+len-4) = __io_read32u(rom+len-4);
        len -= 3;
    }

    // Start the actual DMA transfer, if still needed.
    if (len)
        dma_read_raw_async((void*)ram, (unsigned long)rom, len);

    enable_interrupts();
}

/** @brief Wait until an async DMA transfer is finished. */
void dma_wait(void)
{
    while (__dma_busy()) {}
}


/** @brief Read data from a peripheral through PI DMA, waiting for completion. */
void dma_read(void *ram_address, unsigned long pi_address, unsigned long len)
{
    dma_read_async(ram_address, pi_address, len);
    dma_wait();
}

/**
 * @brief Write to a peripheral
 *
 * This function should be used when writing to the cartridge.
 *
 * @param[in] ram_address
 *            Pointer to a buffer to read data from
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] len
 *            Length in bytes to write to peripheral
 */
void dma_write(const void * ram_address, unsigned long pi_address, unsigned long len) 
{
    assert(len > 0);

    dma_write_raw_async(ram_address, pi_address, len);
    dma_wait();
}

/**
 * @brief Read a 32 bit integer from a peripheral
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to read from
 *
 * @return The 32 bit value read from the peripheral
 */
uint32_t io_read(uint32_t pi_address)
{
    uint32_t retval;

    disable_interrupts();
    retval = __io_read32(pi_address | 0xA0000000);
    enable_interrupts();

    return retval;
}

/**
 * @brief Write a 32 bit integer to a peripheral
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] data
 *            32 bit value to write to peripheral
 */
void io_write(uint32_t pi_address, uint32_t data) 
{
    volatile uint32_t *uncached_address = (uint32_t *)(pi_address | 0xa0000000);

    disable_interrupts();

    while (__dma_busy()) ;
    MEMORY_BARRIER();
    *uncached_address = data;
    MEMORY_BARRIER();

    enable_interrupts();
}

/** @} */ /* dma */
