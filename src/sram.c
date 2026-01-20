/**
 * @file sram.c
 * @author thekovic <https://github.com/thekovic>
 * @brief SRAM access functions for N64 cartridges
 * 
 */

#include "sram.h"
#include "dma.h"
#include "interrupt.h"
#include <errno.h>

#define PI_BSD_DOM2_LAT ((volatile uint32_t*) 0xA4600024)
#define PI_BSD_DOM2_PWD ((volatile uint32_t*) 0xA4600028)
#define PI_BSD_DOM2_PGS ((volatile uint32_t*) 0xA460002C)
#define PI_BSD_DOM2_RLS ((volatile uint32_t*) 0xA4600030)

void sram_init(void)
{
    // Configure PI DOM2 registers to enable access to SRAM
    disable_interrupts();
    *PI_BSD_DOM2_LAT = 0x5;
    *PI_BSD_DOM2_PWD = 0xc;
    *PI_BSD_DOM2_PGS = 0xd;
    *PI_BSD_DOM2_RLS = 0x2;
    enable_interrupts();
}

int sram_read(void* dst, size_t offset, size_t len)
{
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
