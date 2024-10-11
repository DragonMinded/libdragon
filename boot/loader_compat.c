/**
 * @file loader_compat.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief IPL3: Stage 2 (Flat binary loader)
 * 
 * This module implements the "compatibility" version of the second stage of
 * the loader, which is responsible for loading a flat binary from a fixed
 * ROM address and jumping to it.
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
    // Invalidate the stack1 area, where the first stage put its stack.
    // We don't need it anymore, and we don't want it to be flushed to RDRAM
    // that will be cleared anyway.
    data_cache_hit_invalidate(STACK1_BASE, STACK1_SIZE);

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

// This is the last stage of IPL3. It runs directly from ROM so that we are
// free of cleaning up our breadcrumbs in both DMEM and RDRAM.
__attribute__((far, noreturn))
void stage3(uint32_t entrypoint)
{
    // Read memory size from boot flags
    int memsize = *(volatile uint32_t*)0x80000318;

    // Reset the CPU cache, so that the application starts from a pristine state
    cop0_clear_cache();

    // Clear DMEM and RDRAM stage 2 area
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = 0xA4001000;
    *SP_DRAM_ADDR = memsize - TOTAL_RESERVED_SIZE;
    _Static_assert((TOTAL_RESERVED_SIZE % 1024) == 0, "TOTAL_RESERVED_SIZE must be multiple of 1024");
    *SP_WR_LEN = (((TOTAL_RESERVED_SIZE >> 10) - 1) << 12) | (1024-1);
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = 0xA4000000;
    *SP_DRAM_ADDR = 0x00802000;  // Area > 8 MiB which is guaranteed to be empty
    *SP_RD_LEN = 4096-1;

    // Wait until the DMA is done
    while (*SP_DMA_BUSY) {}

    goto *(void*)entrypoint;
}
