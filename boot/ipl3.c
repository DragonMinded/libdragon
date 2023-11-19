/**
 * @file ipl3.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief IPL3 Bootcode
 * 
 * Layout of ROM
 * 
 * Production layout:
 * 0x0000 - HEADER
 * 0x0040 - IPL3
 * 0x1000 - iQue Trampoline (load IPL3 to DMEM, jump back to it)
 * 0x1040 - Rompak TOC
 * ...... - Main ELF file
 * ...... - Other Rompak files (.sym file, .dfs file, etc.)
 * 
 * Development layout:
 * 0x0000 - HEADER
 * 0x0040 - Signed IPL3 Trampoline
 * 0x1000 - iQue Trampoline (load IPL3 to DMEM, jump back to it)
 * 0x1040 - IPL3 development version (unsigned)
 * 0x2000 - Rompak TOC
 * ...... - Main ELF file
 * ...... - Other Rompak files (.sym file, .dfs file, etc.)
 * 
 */

#include <stdint.h>
#include <stdbool.h>
#include "minidragon.h"
#include "debug.h"
#include "rdram.h"
#include "entropy.h"
#include "loader.h"

__attribute__((section(".banner"), used))
const char banner[32] = " Libdragon IPL3 " " Coded by Rasky ";

typedef struct __attribute__((packed)) {
    uint32_t pi_dom1_config;
    uint32_t clock_rate;
    uint32_t boot_address;
    uint32_t sdk_version;
    uint64_t checksum;
    uint64_t reserved1;
    char title[20];
    char reserved2[7];
    uint32_t gamecode;
    uint8_t rom_version;
} rom_header_t;

_Static_assert(sizeof(rom_header_t) == 64, "invalid sizeof(rom_header_t)");

__attribute__((section(".header"), used))
const rom_header_t header = {
    // Standard PI DOM1 config
    .pi_dom1_config = 0x80371240,
    // Our IPL3 does not use directly this field. We do set it
    // mainly for iQue, so that the special iQue trampoline is run,
    // which jumps to our IPL3.
    .boot_address = 0x80000400,
    // Default title name
    .title = "Libdragon           ",
};


#if 0
void memtest(int memsize)
{
    volatile void *RDRAM = (volatile void*)0xA0000000;
    volatile uint8_t *ptr8 = RDRAM;
    volatile uint8_t *ptr8_end = RDRAM + memsize;
    int debug = 0;

    while (ptr8 < ptr8_end) {
        for (int k=0;k<32;k++) {
            for (int j=0;j<16;j++) {
                ptr8[k*32+j] = 0xAA;
            }
            for (int j=16;j<31;j++) {
                ptr8[k*32+j] = 0x55;
            }
        }
        for (int k=0;k<32;k++) {
            for (int j=0;j<16;j++) {
                if (ptr8[k*32+j] != 0xAA) {
                    debugf("Memtest failed at ", ptr8 + k*32 + j);
                    abort();
                }
            }
            for (int j=16;j<31;j++) {
                if (ptr8[k*32+j] != 0x55) {
                    debugf("Memtest failed at ", ptr8 + k*32 + j);
                    abort();
                }
            }
        }
        ptr8 += 32*32;
        if ((++debug % 1024) == 0)
            debugf("Memtest percentage: ", (int)((volatile void*)ptr8 - RDRAM) * 100 / memsize);
    }

    debugf("Memtest OK!");
}
#endif

typedef struct {
    uint32_t memory_size;
    uint32_t entropy;
    uint32_t flags;
    uint32_t padding;
} bootinfo_t;

_Static_assert(sizeof(bootinfo_t) == 16, "invalid sizeof(bootinfo_t)");

static inline void rsp_clear_imem(void)
{
    *SP_RSP_ADDR = 0x1000; // IMEM
    *SP_DRAM_ADDR = 8*1024*1024; // RDRAM addresses >8 MiB always return 0
    *SP_RD_LEN = 4096-1;
    while (*SP_DMA_BUSY) {}
}

__attribute__((noreturn, section(".boot")))
void _start(void)
{
    register uint32_t ipl2_romType   asm ("s3"); (void)ipl2_romType;
    register uint32_t ipl2_tvType    asm ("s4"); (void)ipl2_tvType;
    register uint32_t ipl2_resetType asm ("s5"); (void)ipl2_resetType;
    register uint32_t ipl2_romSeed   asm ("s6"); (void)ipl2_romSeed;
    register uint32_t ipl2_version   asm ("s7"); (void)ipl2_version;

    // Check if we're running on iQue
    bool bbplayer = (*MI_VERSION & 0xF0) == 0xB0;

    // Clear IMEM (contains IPL2). We don't need it anymore, and we can 
    // instead use IMEM as a zero-buffer for RSP DMA.
    // Also, we put our bss in IMEM.
    rsp_clear_imem();

    entropy_init();
    usb_init();
    debugf("Libdragon IPL3");
    
    entropy_add(C0_COUNT());
    C0_WRITE_CAUSE(0);
    C0_WRITE_COUNT(0);
    C0_WRITE_COMPARE(0);
	C0_WRITE_WATCHLO(0);

    int memsize;
    if (!bbplayer) {
        memsize = rdram_init();
    } else {
        // iQue OS put the memory size in a special location. This is the
        // amount of memory that the OS has assigned to the application, so it
        // could be less than the physical total memory. Anyway, it's the value
        // we should use and pass along.
        memsize = *(uint32_t*)0xA0000318;

        // Notice that even if 8 MiB were allocated, the top of the memory is
        // in-use by save state emulation, so we shouldn't access it anyway.
        if (memsize == 0x800000)
            memsize = 0x7C0000;
    }

    // Clear D/I-cache, useful after warm boot. Maybe not useful for cold
    // boots, but the manual says that the cache state is invalid at boot,
    // so a reset won't hurt.
    cop0_clear_cache();

    // Fill boot information at beginning of DMEM. The rest of IMEM has been
    // cleared by now anyway. Notice that we also store BSS in IMEM, so the
    // linker script reserves initial part to boot information.
    bootinfo_t *bootinfo = (bootinfo_t*)0xA4000000;
    bootinfo->memory_size = memsize;
    bootinfo->flags = (ipl2_tvType << 16) | (ipl2_resetType << 8) | (bbplayer ? 1 : 0);
    bootinfo->padding = 0;

    // Perform a memtest
    // memtest(memsize);

    // Copy the IPL3 stage2 (loader.c) from DMEM to the end of RDRAM.
    extern uint32_t __stage2_start[]; extern int __stage2_size;
    int stage2_size = (int)&__stage2_size;
    void *rdram_stage2 = (void*)0x80000000 + memsize - stage2_size;
    rsp_dma_to_rdram(__stage2_start, rdram_stage2, stage2_size);

    // Jump to stage 2 in RDRAM.
    MEMORY_BARRIER();
    asm("move $sp, %0"::"r"(rdram_stage2-8));
    goto *rdram_stage2;
}
