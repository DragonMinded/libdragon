/**
 * @file loader_compat.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief IPL3: Stage 2 (Flat binary loader)
 * 
 * This module implements the "compatibility" version of the second stage of
 * the loader, which is responsible for loading a flat binary from a fixed
 * ROM address.
 */
#include "minidragon.h"
#include "loader.h"

__attribute__((far, noreturn))
void stage3(uint32_t entrypoint);

static uint32_t io_read32(uint32_t vaddrx)
{
    vaddrx |= 0xA0000000;
    volatile uint32_t *vaddr = (uint32_t *)vaddrx;
    return *vaddr;
}

static void pi_read_async(void *dram_addr, uint32_t cart_addr, uint32_t len)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    *PI_DRAM_ADDR = (uint32_t)dram_addr;
    *PI_CART_ADDR = cart_addr;
    *PI_WR_LEN = len-1;
}

static void pi_wait(void)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
}

__attribute__((used))
void stage2(void)
{
    uint32_t entrypoint = io_read32(0x10000008);
    uint32_t size = io_read32(0x10000010);
    if (size == 0 || size > (8<<20) - (entrypoint & 0x1FFFFFFF) - TOTAL_RESERVED_SIZE)
        size = 1<<20;

    pi_read_async((void*)entrypoint, 0x10001000, size);
    pi_wait();

    // Reset the RCP hardware
    rcp_reset();

    // Jump to the ROM finish function
    stage3(entrypoint);
}
