/**
 * @file sram.c
 * @author thekovic <https://github.com/thekovic>
 * @brief SRAM access functions for N64 cartridges
 * 
 */

#include "sram.h"
#include "debug.h"
#include "dma.h"
#include "interrupt.h"
#include "n64sys.h"
#include <errno.h>

/// @cond
#define PI_BSD_DOM2_LAT ((volatile uint32_t*) 0xA4600024)
#define PI_BSD_DOM2_PWD ((volatile uint32_t*) 0xA4600028)
#define PI_BSD_DOM2_PGS ((volatile uint32_t*) 0xA460002C)
#define PI_BSD_DOM2_RLS ((volatile uint32_t*) 0xA4600030)

// TODO: Replace fixed size with detection of other SRAM sizes.
#define SRAM_SIZE    0x00008000 ///< Size of standard SRAM in commercial cartridges (32 KiB).
/// @endcond

bool __sram_inited = false; ///< True if sram_init() was called

void sram_init(void)
{
    if (__sram_inited)
        return;

    // Configure PI DOM2 registers to enable access to SRAM
    disable_interrupts();
    *PI_BSD_DOM2_LAT = 0x5;
    *PI_BSD_DOM2_PWD = 0xc;
    *PI_BSD_DOM2_PGS = 0xd;
    *PI_BSD_DOM2_RLS = 0x2;
    enable_interrupts();

    __sram_inited = true;
}

int sram_detect(void)
{
    assertf(__sram_inited, "sram accessed, but sram_init() hasn't been called yet");

    uint32_t test_pattern = 0x12345678;
    // Convert to virtual address to attempt to perform direct I/O to SRAM.
    volatile uint32_t* sram_ptr = VirtualUncachedAddr(SRAM_ADDRESS);
    // Test read, save the existing data to restore it later.
    uint32_t data_old = *sram_ptr;
    // Write a test pattern to SRAM and check it.
    *sram_ptr = test_pattern;
    uint32_t data_new = *sram_ptr;
    bool is_detected = (data_new == test_pattern);
    // Restore the old data.
    *sram_ptr = data_old;

    return (is_detected) ? SRAM_SIZE : 0;
}

int sram_read(void* dst, size_t offset, size_t len)
{
    assertf(__sram_inited, "sram accessed, but sram_init() hasn't been called yet");

    // Check if the read operation is within bounds.
    if (offset + len > SRAM_SIZE)
    {
        errno = EINVAL;
        return -1;
    }

    pi_addr_t sram_address = SRAM_ADDRESS + offset;
    dma_read_raw_async(dst, sram_address, len);
    dma_wait();

    return (int) len;
}

int sram_write(const void* src, size_t offset, size_t len)
{
    assertf(__sram_inited, "sram accessed, but sram_init() hasn't been called yet");

    // Check if the write operation is within bounds.
    if (offset + len > SRAM_SIZE)
    {
        errno = EINVAL;
        return -1;
    }

    pi_addr_t sram_address = SRAM_ADDRESS + offset;
    dma_write_raw_async(src, sram_address, len);
    dma_wait();

    return (int) len;
}
