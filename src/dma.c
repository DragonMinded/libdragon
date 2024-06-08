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

void dma_wait(void)
{
    while (__dma_busy()) {}
}


void dma_read(void *ram_address, unsigned long pi_address, unsigned long len)
{
    pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    dma_read_async(ram_address, pi_address, len);
    dma_wait();
}

void dma_write(const void * ram_address, unsigned long rom_address, unsigned long len) 
{
    rom_address = (rom_address | 0x10000000) & 0x1FFFFFFF;
    dma_write_raw_async(ram_address, rom_address, len);
    dma_wait();
}

uint32_t io_read(uint32_t pi_address)
{
    uint32_t retval;

    disable_interrupts();
    retval = __io_read32((void*)(pi_address | 0xA0000000));
    enable_interrupts();

    return retval;
}

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
