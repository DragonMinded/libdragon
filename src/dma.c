/**
 * @file dma.c
 * @author Jennifer Taylor <dragonminded@dragonminded.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief DMA Controller
 * @ingroup dma
 */
#include <stdbool.h>
#include "dma.h"
#include "n64types.h"
#include "n64sys.h"
#include "vaddr64.h"
#include "interrupt.h"
#include "debug.h"
#include "utils.h"
#include "regsinternal.h"
#include "interrupt_internal.h"
#include "kernel/kernel_internal.h"
#include "kirq.h"

/** @brief Structure used to interact with the PI registers */
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

static volatile int __dma_busy(void)
{
    return PI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
}

__attribute__((noinline, warn_unused_result))
static uint32_t wait_dma_and_disable_interrupts(void)
{
    while (1) {
        while (__dma_busy()) {} 
        uint32_t sr = __disable_interrupts();
        if (LIKELY(!__dma_busy()))
            return sr;
        __enable_interrupts(sr);
    }
}

bool io_accessible(pi_addr_t pi_address)
{
    // Below 0x0500_0000, there is RDRAM and RCP registers.
    if (pi_address < 0x05000000)
        return false;

    // The SI bus is partially covering the PI range in the CPU memory map
    if (pi_address >= 0x1FC00000 && pi_address <= 0x1FCFFFFF)
        return false;

    // The CPU-copy path maps the PI address through KSEG1 (pi_address | 0xA0000000),
    // which only reaches physical addresses below 0x20000000. A cart address at or
    // above that (e.g. a large disc image streamed from a high cart offset on a
    // >256 MiB ROM) is reachable only via DMA; report it as not CPU-accessible so the
    // aligned raw-DMA path is taken with the full 32-bit address (PhysicalAddr would
    // otherwise mask it to 29 bits and read the wrong location).
    if (pi_address >= 0x20000000)
        return false;

    // Upper half of the PI range is not memory mapped
    if (pi_address >= 0x80000000)
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

__attribute__((noinline))
void dma_read_raw_async(void * ram_address, pi_addr_t pi_address, unsigned long len) 
{
    assert(len > 0);

    uint32_t sr = wait_dma_and_disable_interrupts();
    *PI_DRAM_ADDR = PhysicalAddr(ram_address);
    *PI_CART_ADDR = pi_address;
    *PI_WR_LEN = len-1;
    __enable_interrupts(sr);
}

void dma_write_raw_async(const void * ram_address, pi_addr_t pi_address, unsigned long len) 
{
    assert(len > 0);

    uint32_t sr = wait_dma_and_disable_interrupts();
    *PI_DRAM_ADDR = PhysicalAddr(ram_address);
    *PI_CART_ADDR = pi_address;
    *PI_RD_LEN = len-1;
    __enable_interrupts(sr);
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
        return (uint16_t)*(volatile uint32_t*)(pi_address^2);
    } else {
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

void dma_read_async(void *ram_pointer, pi_addr_t pi_address, unsigned long len)
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

void dma_read(void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    // HORROR: this code makes no sense, but it's always been here. The original
    // goal was to convert virtual addresses to PI addresses, but it is also
    // preventing a large span of the PI address space from being used.
    pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    dma_read_async(ram_address, pi_address, len);
    dma_wait();
}

void dma_write(const void * ram_address, pi_addr_t rom_address, unsigned long len) 
{
    // HORROR: this code makes no sense, but it's always been here. The original
    // goal was to make virtual addresses to PI addresses, but it is also
    // preventing a large span of the PI address space from being used.
    rom_address = (rom_address | 0x10000000) & 0x1FFFFFFF;
    dma_write_raw_async(ram_address, rom_address, len);
    dma_wait();
}

uint32_t io_read(pi_addr_t pi_address)
{
    // HACK: to maintain backward compatibility, this function used to accept
    // also CPU virtual addresses. To still allow for that, we need to convert
    // them to physical addresses first.
    if (UNLIKELY(pi_address >= 0x80000000 && pi_address <= 0xBFFFFFFF)) {
        debugf("io_read: WARNING: deprecated usage of virtual address: %08lX\n", pi_address);
        pi_address = PhysicalAddr((void*)pi_address);
    }
    
    // Convert the PI address into a 64-bit virtual address, which allows a wider
    // range of PI addresses to be accessed.
    vaddr64_t va64 = VirtualUncachedAddr64(pi_address);

    uint32_t sr = wait_dma_and_disable_interrupts();
    uint32_t retval = sys_vaddr_read32(va64);
    __enable_interrupts(sr);

    return retval;
}

void io_write(pi_addr_t pi_address, uint32_t data) 
{
    // HACK: to maintain backward compatibility, this function used to accept
    // also CPU virtual addresses. To still allow for that, we need to convert
    // them to physical addresses first. Keep this undocumented though, as we
    // want to deprecate this behavior.
    if (UNLIKELY(pi_address >= 0x80000000 && pi_address <= 0xBFFFFFFF)) {
        debugf("io_write: WARNING: deprecated usage of virtual address: %08lX\n", pi_address);
        pi_address = PhysicalAddr((void*)pi_address);
    }

    // Convert the PI address into a 64-bit virtual address, which allows a wider
    // range of PI addresses to be accessed.
    vaddr64_t va64 = VirtualUncachedAddr64(pi_address);

    uint32_t sr = wait_dma_and_disable_interrupts();
    sys_vaddr_write32(va64, data);
    __enable_interrupts(sr);
}
