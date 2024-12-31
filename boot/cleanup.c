/**
 * @file cleanup.c
 * @brief IPL3: Stage 3 (Cleanup)
 * 
 * This module implements the third and final stage of the loader,
 * which is responsible for cleaning up after the previous stages.
 * It runs directly from ROM so that we are free to clean up our breadcrumbs
 * in both DMEM and RDRAM.
 * 
 * This stage runs from "high RDRAM", that is, it is placed at the end of RDRAM.
 * The code is compiled to be relocatable via a trick in the Makefile, so that
 * it can be placed at dynamic addresses (though normally only two would be
 * possible: either near 4 MiB or 8 MiB).
 * 
 * The tasks performed by this stage are:
 * 
 *  * Notify the PIF that the boot process is finished (in COMPAT mode,
 *    this is skipped because the game is expected to do it instead).
 *  * Clear DMEM except the boot flags area (in COMPAT mode, all of DMEM is cleared).
 *  * Jump to the entrypoint.
 */

#include "minidragon.h"
#include "loader.h"

// Inform PIF that the boot process is finished. If this is not written,
// the PIF will halt the CPU after 5 seconds. This is not done by official
// IPL3 but rather left to the game to do, but for our open source IPL3,
// it seems better to leave it to the IPL3.
static inline void pif_terminate_boot(void)
{
    si_write(0x7FC, 0x8);
}

// This is the last stage of IPL3. It runs directly from ROM so that we are
// free of cleaning up our breadcrumbs in both DMEM and RDRAM.
__attribute__((far, noreturn))
void stage3(uint32_t entrypoint)
{
#ifndef COMPAT
    // Notify the PIF that the boot process is finished. This will take a while
    // so start it in background.
    pif_terminate_boot();

    // Read memory size from boot flags
    int memsize = *(volatile uint32_t*) 0xA4000000;
#else
    int memsize = *(volatile uint32_t*) 0x80000318;
#endif

    // Reset the CPU cache, so that the application starts from a pristine state
    cop0_clear_cache();

    // Clear the reserved portion of RDRAM. To create a SP_WR_LEN value that works,
    // we assume the reserved size is a multiple of 1024. It can be made to work
    // also with other sizes, but this code will need to be adjusted.
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = 0xA4001000;
    *SP_DRAM_ADDR = memsize - TOTAL_RESERVED_SIZE;
    _Static_assert((TOTAL_RESERVED_SIZE % 1024) == 0, "TOTAL_RESERVED_SIZE must be multiple of 1024");
    *SP_WR_LEN = (((TOTAL_RESERVED_SIZE >> 10) - 1) << 12) | (1024-1);

    // Clear DMEM (leave only the boot flags area intact). Notice that we can't
    // call debugf anymore after this, because a small piece of debugging code
    // (io_write) is in DMEM, so it can't be used anymore.
    while (*SP_DMA_FULL) {}
    *SP_DRAM_ADDR = 0x00802000;  // Area > 8 MiB which is guaranteed to be empty
#ifndef COMPAT
    *SP_RSP_ADDR = 0xA4000010;
    *SP_RD_LEN = 4096-16-1;

    // Wait until the PIF is done. This will also clear the interrupt, so that
    // we don't leave the interrupt pending when we go to the entrypoint.
    si_wait();
#else
    *SP_RSP_ADDR = 0xA4000000;
    *SP_RD_LEN = 4096-1;
#endif

    // RSP DMA is guaranteed to be finished by now because stage3 is running from
    // ROM and it's very slow. Anyway, let's just wait to avoid bugs in the future,
    // because we don't want to begin using the stack (at the end of RDRAM) before it's finished.
    while (*SP_DMA_BUSY) {}

#ifndef COMPAT
    // Configure SP at the end of RDRAM. This is a good default in general,
    // then of course userspace code is free to reconfigure it.
    asm ("move $sp, %0" : : "r" (0x80000000 + memsize - 0x10));
#endif

    goto *(void*)entrypoint;
}
