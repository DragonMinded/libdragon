/**
 * @file dma.c
 * @brief DMA Controller
 * @ingroup dma
 */
#include <stdbool.h>
#include "n64types.h"
#include "n64sys.h"
#include "interrupt.h"
#include "debug.h"
#include "utils.h"
#include "regsinternal.h"

/**
 * @defgroup dma DMA Controller
 * @ingroup lowlevel
 * @brief DMA functionality for transfers between cartridge space and RDRAM
 *
 * The DMA controller is responsible for handling block and word accesses from
 * the cartridge domain.  Because of the nature of the cartridge interface, code
 * cannot use memcpy or standard pointer accesses on memory mapped to the cartridge.
 * Consequently, the peripheral interface (PI) provides a DMA controller for
 * accessing data.
 *
 * The DMA controller requires no initialization.  Using #dma_read and #dma_write
 * will allow reading from the cartridge and writing to the cartridge respectively
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
 * @brief Check whether the specified PI address can be accessed doing I/O from CPU
 * 
 * The PI bus covers the full 32-bit address range. The full range is only accessible
 * via DMA, though. A part of the range is also memory mapped to the CPU and can be
 * accessed via #io_read and #io_write.
 * 
 * The ranges of PI address that can be accessed via CPU are:
 * 
 *  * 0x0500_0000 - 0x0FFF_FFFF: used by N64DD and SRAM on cartridge
 *  * 0x1000_0000 - 0x1FBF_FFFF: cartridge ROM
 *  * 0x1FD0_0000 - 0x1FFF_FFFF: no known PI peripherals use this
 * 
 * The rest of the 32-bit address range is only accessible via DMA.
 * 
 * Notice also that the range 0x2000_0000 - 0x7FFF_FFFF is theoretically accessible
 * by the CPU but only via 64-bit addressing, so it requires assembly instructions
 * (as the libdragon toolchain uses 32-bit pointers). No known PI peripherals use this
 * range anyway.
 * 
 * This function checks whether the specified address falls into the range accessible
 * via CPU or not.
 * 
 * @param pi_address        PI address to check
 * @return                  True if the address is memory mapped, false if it is not
 */
bool io_accessible(uint32_t pi_address)
{
    // Below 0x0500_0000, there is RDRAM and RCP registers.
    if (pi_address < 0x05000000)
        return false;

    // Using 32-bit addresses (like those available from within our C code),
    // the CPU can only access addresses up to 0x1FFF_FFFF. 
    // FIXME: we could theoretically lift this limit up to 0x7FFF_FFFF if the
    // I/O functions were rewritten in assembly using 64-bit addresses, but
    // there is currently no known PI peripheral that operates in that range anyway.
    if (pi_address > 0x1FFFFFFF)
        return false;

    // The SI bus is partially covering the PI range in the CPU memory map
    if (pi_address >= 0x1FC00000 && pi_address <= 0x1FCFFFFF)
        return false;    

    // All other addresses are memory mapped and can be accessed via CPU.
    return true;
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
    PI_regs->pi_address = pi_address;
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
    PI_regs->pi_address = pi_address;
    MEMORY_BARRIER();
    PI_regs->read_length = len-1;
    MEMORY_BARRIER();

    enable_interrupts();
}

/** @brief Low-level 32-bit aligned ROM read.
 * 
 * @note This function must be called with interrupts disabled.
 */
static uint32_t __io_read32(void* pi_pointer) {
    while (__dma_busy()) {}
    MEMORY_BARRIER();
    return *(volatile uint32_t*)pi_pointer;
}

/** @brief Low-level 16-bit aligned PI ROM read.
 * 
 * 16-bit PI ROM reads are undocumented. Testing on real hardware shows
 * that they only work for 32-bit aligned addresses, so this function
 * falls back to a full 32bit read for misaligned addresses.
 * 
 * @note This function must be called with interrupts disabled.
 */
static uint16_t __io_read16(void *pi_pointer) {
    uint32_t pi_address = (uint32_t)pi_pointer;
    if (pi_address & 2) {
        return (uint16_t)__io_read32((void*)(pi_address^2));
    } else {
        while (__dma_busy()) {}
        MEMORY_BARRIER();
        return *(volatile uint16_t*)pi_pointer;
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
static uint8_t __io_read8(void *pi_pointer) {
    uint32_t pi_address = (uint32_t)pi_pointer;
    if (pi_address&1)
        return (uint8_t)__io_read16((void*)(pi_address^1));
    else
        return __io_read16(pi_pointer)>>8;
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
 * @param[out] ram_pointer
 *             Pointer to a buffer in RDRAM to place read data
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from
 * @param[in]  len
 *             Length in bytes to read into ram_pointer
 */
void dma_read_async(void *ram_pointer, unsigned long pi_address, unsigned long len)
{
    void *ram = UncachedAddr(ram_pointer);
    uint32_t ram_address = (uint32_t)ram;
    void *rom = (void*)(pi_address | 0xA0000000);

    assert(len > 0);
    assert(((ram_address ^ pi_address) & 1) == 0); (void)ram_address;

    disable_interrupts();

    // Check if the PI address can be accessed with CPU.
    // If not, we cannot perform a misaligned transfer.
    if (!io_accessible(pi_address)) {        
        assertf((pi_address & 2) == 0 && (ram_address & 7) == 0,
            "misaligned transfer not supported at this PI address");
        dma_read_raw_async(ram_pointer, pi_address, len);
        enable_interrupts();
        return;
    }

    // Check if the address in RAM is misaligned.
    if ((uint32_t)ram & 7) {
        // Transfer the first bytes manually up until the next 8-byte aligned
        // address. Make sure to not transfer more than requested.
        if ((uint32_t)ram & 1) {
            *(uint8_t*)ram = __io_read16(rom - 1);
            ram++; rom++; len--;
        }
        if ((uint32_t)ram & 2 && len >= 2) {
            *(uint16_t*)ram = __io_read16(rom);
            ram += 2; rom += 2; len -= 2;
        }
        if ((uint32_t)ram & 4 && len >= 4) {
            *(uint32_t*)ram = (__io_read16(rom) << 16) | __io_read16(rom+2);
            ram += 4; rom += 4; len -= 4;
        }
        while ((uint32_t)ram & 7 && len > 0) {
            *(uint8_t*)ram = __io_read8(rom);
            ram++; rom++; len--;
        }
    }

    // If there's an odd number of bytes left to transfer, check if the DMA
    // will do that correctly. This happens only if the transfers fits the
    // first DMA block, which is either 127 bytes or up to the end of the
    // current RDRAM row (0x800 bytes).
    int first_block_len = MIN(127, 0x800 - ((uint32_t)ram & 0x7ff));
    if ((len & 1) && len >= first_block_len) {
        // Odd transfers would not work correctly. Transfer the last byte
        // manually.
        *(uint8_t*)(ram+len-1) = __io_read16(rom+len-1) >> 8;
        len -= 1;
    }

    // Start the actual DMA transfer, if still needed.
    if (len)
        dma_read_raw_async(ram, PhysicalAddr(rom), len);

    enable_interrupts();
}

/** 
 * @brief Wait until an async DMA or I/O transfer is finished.
 */
void dma_wait(void)
{
    while (__dma_busy()) {}
}


/** 
 * @brief Read data from a peripheral through PI DMA, waiting for completion.
 * 
 * This function performs a blocking read. See #dma_read_async for more information.
 * 
 * @param[out] ram_address
 *             Pointer to a buffer in RDRAM to place read data
 * @param[in]  pi_address
 *             ROM address to read from (must be in range (0x10000000-0x1FFFFFFF).
 * @param[in]  len
 *             Length in bytes to read into ram_address
 * 
 * @note This function has always had an historical mistake: the pi_address is mangled
 *       to be forced into the ROM area (0x10000000-0x1FFFFFFF). This is wrong as the
 *       PI bus has full 32-bit address, and the same function could have been used
 *       to access the whole range.
 *       If you need to read outside the ROM area, use #dma_read_async instead.
 */
void dma_read(void *ram_address, unsigned long pi_address, unsigned long len)
{
    pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
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
 * @param[in] rom_address
 *            Cartridge address to write to (must be in range (0x10000000-0x1FFFFFFF).
 * @param[in] len
 *            Length in bytes to write to peripheral
 * 
 * @note This function has always had an historical mistake: the pi_address is mangled
 *       to be forced into the ROM area (0x10000000-0x1FFFFFFF). This is wrong as the
 *       PI bus has full 32-bit address, and the same function could have been used
 *       to access the whole range.
 *       If you need to read outside the ROM area, use #dma_write_raw_async instead.
 */
void dma_write(const void * ram_address, unsigned long rom_address, unsigned long len) 
{
    rom_address = (rom_address | 0x10000000) & 0x1FFFFFFF;
    dma_write_raw_async(ram_address, rom_address, len);
    dma_wait();
}

/**
 * @brief Read a 32 bit integer from a peripheral using the CPU.
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to read from
 *
 * @return The 32 bit value read from the peripheral
 * 
 * @note This function only works if the specified PI address falls within a range
 *       which is memory mapped on the CPU. See #io_accessible for more information.
 * 
 * @see #io_accessible
 */
uint32_t io_read(uint32_t pi_address)
{
    uint32_t retval;

    disable_interrupts();
    retval = __io_read32((void*)(pi_address | 0xA0000000));
    enable_interrupts();

    return retval;
}

/**
 * @brief Write a 32 bit integer to a peripheral using the CPU.
 * 
 * Notice that writes are performed asynchronously, so the data might have not been
 * fully written to the peripheral yet when the function returns. Use #dma_wait if
 * you need to wait for the transfer to be finished.
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] data
 *            32 bit value to write to peripheral
 *
 * @note This function only works if the specified PI address falls within a range
 *       which is memory mapped on the CPU. See #io_accessible for more information.
 *
 * @see #io_accessible
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
