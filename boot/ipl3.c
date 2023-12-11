/**
 * @file ipl3.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief IPL3: Stage 1 (RDRAM initialization)
 * 
 * This file contains the first stage of the IPL3 boot code. The second stage
 * is stored in loader.c.
 * 
 * The goal of this task is to perform RDRAM initialization. At the point at
 * which it runs, in fact, RDRAM is not inizialized yet (at least after a cold
 * boot), so it cannot be used. The RDRAM inizialization is a complex task
 * that is detailed in rdram.c.
 * 
 * In addition to performing the initialization (see rdram.c), this stage also
 * clears the RDRAM to zero. This is not strictly necessary, but it is useful
 * to avoid having garbage in memory, which sometimes can cause a different
 * of behaviors with emulators (which will instead initialize memory to zero).
 * 
 * To efficiently clear memory, we use RSP DMA which is the most efficient
 * way available. We use IMEM as a zero-buffer, and we clear memory in
 * background as RDRAM chips are configured, while the rest of the IPL3 code
 * is running and configuring the next chips.
 * 
 * After RAM is inizialized, we copy the second stage of IPL3 (loader.c) from
 * DMEM to the end of RDRAM, and jump to it to continue with the execution.
 * This is done mainly because running from RDRAM is faster than from DMEM,
 * so there is no reason in staying in DMEM after RDRAM is available.
 * 
 * Layout of ROM
 * =============
 * 
 * The following tables detail how a standard libdragon ROM using this IPL3
 * boot code is laid out. There are two different layouts, depending on whether
 * we are using the production or development IPL3.
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
 * 
 * The ROM header is a 64-byte structure that contains information about the
 * ROM. It is not used by IPL3 itself, and is normally just advisory (that is,
 * might be used by flashcart menus to identify a game, provide the game tile,
 * etc.).
 * 
 * The signed IPL3 trampoline is a small piece of code that is used to load
 * the development IPL3 from the flashcart. During development, it is unconvenient
 * to sign each built of IPL3, so the trampoline (coded and signed just once)
 * just allow to load an unsigned IPL3 from offset 0x1040 into DMEM.
 * 
 * The iQue trampoline, stored at 0x1000, is a small piece of code that just
 * loads IPL3 (offset 0x40) into DMEM and jumps to it. It is used only on iQue
 * because the iQue OS tries to skip IPL3 execution and just load a flat binary
 * into RAM and running it (this was done because iQue doesn't use RDRAM chips
 * anywhere, so existing IPL3 code wouldn't work there, nor it is necessary).
 * In our case, since IPL3 Stage 2 has important loading logic we need to
 * preserve (ELF parsing, decompression, etc.) we absolutely need to run it.
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

// These register contains boot flags passed by IPL2. Define them globally
// during the first stage of IPL3, so that the registers are not reused.
register uint32_t ipl2_romType   asm ("s3");
register uint32_t ipl2_tvType    asm ("s4");
register uint32_t ipl2_resetType asm ("s5");
register uint32_t ipl2_romSeed   asm ("s6");
register uint32_t ipl2_version   asm ("s7");

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

void rsp_clear_mem(uint32_t mem, unsigned int size)
{
    while (*SP_DMA_BUSY) {}
    uint32_t *ptr = (uint32_t*)mem;
    uint32_t *ptr_end = (uint32_t*)(mem + size);
    while (ptr < ptr_end)
        *ptr++ = 0;
 
    // *SP_RSP_ADDR = 0x1000; // IMEM
    // *SP_DRAM_ADDR = 8*1024*1024 + 0x2000; // Most RDRAM addresses >8 MiB always return 0
    // *SP_RD_LEN = 4096-1;
    // while (*SP_DMA_BUSY) {}
}

static void bzero8(void *mem)
{
    asm ("sdl $0, 0(%0); sdr $0, 7(%0);" :: "r"(mem));
}

// Clear memory using RSP DMA. We use IMEM as source address, which
// was cleared in rsp_clear_imem(). The size can be anything up to 1 MiB,
// since the DMA would just wrap around in IMEM.
void rsp_bzero_async(uint32_t rdram, int size)
{
    // Use RSP DMA which is fast, but only allows for 8-byte alignment.
    // Data outside of the 8-byte alignment is cleared using the CPU,
    // using uncached writes so that they behave exactly the same as RSP DMA would.
    rdram |= 0xA0000000;
    bzero8((void*)rdram);
    if (size <= 8)
        return;
    bzero8((void*)(rdram + size - 8));
    rdram += 8;
    size -= 8;
    while (size > 0) {
        int sz = size > 1024*1024 ? 1024*1024 : size;
        while (*SP_DMA_FULL) {}
        *SP_RSP_ADDR = 0x1000;
        *SP_DRAM_ADDR = rdram; // this is automatically rounded down
        *SP_WR_LEN = sz-1;   // this is automatically rounded up
        size -= sz;
        rdram += sz;
    }
}

// Callback for rdram_init. We use this to clear the memory banks
// as soon as they are initialized. We use RSP DMA to do this which
// is very quick (~2.5 ms for 1 MiB), and we do that in background
// anyway. RSP DMA allows max 1 MiB per transfer, so we need to
// schedule two transfers for each bank.
static void mem_bank_init(int chip_id, bool last)
{
    uint32_t base = chip_id*1024*1024;
    int size = 2*1024*1024;

    // If we are doing a warm boot, skip the first 0x400 bytes
    // of RAM (on the first chip, because it historically contains
    // some boot flags that some existing code might expect to stay there.
    // For instance, the Everdrive menu expects 0x80000318 to still
    // contain the RDRAM size after a warm boot, and we need to comply
    // with this even if Everdrive itself doesn't use this IPL3 (but
    // might boot a game that does, and that game shouldn't clear
    // 0x80000318).
    if (chip_id == 0 && ipl2_resetType != 0) {
        base += 0x400;
        size -= 0x400;
    } else if (last) {
        // If this is the last chip, we skip the last portion of RDRAM where
        // we store the stage2
        size -= TOTAL_RESERVED_SIZE;
    }
    rsp_bzero_async(base, size);
}

// This function is placed by the linker script immediately below the stage1()
// function. We just change the stack pointer here, as very first thing.
__attribute__((noreturn, section(".stage1.pre")))
void stage1pre(void)
{
    // Move the stack to the data cache. Notice that RAM is not initialized
    // yet but we don't care: if sp points to a cached location, it will
    // just use the cache for that.
    asm ("li $sp, %0"::"i"(STACK1_TOP));
    __builtin_unreachable(); // avoid function epilog, we don't need it
}

__attribute__((noreturn, section(".stage1")))
void stage1(void)
{
    // Clear IMEM (contains IPL2). We don't need it anymore, and we can
    // instead use IMEM as a zero-buffer for RSP DMA.
    rsp_clear_mem((uint32_t)SP_IMEM, 4096);

    entropy_init();
    usb_init();
    debugf("Libdragon IPL3");
    
    entropy_add(C0_COUNT());
    C0_WRITE_CAUSE(0);
    C0_WRITE_COUNT(0);
    C0_WRITE_COMPARE(0);
	C0_WRITE_WATCHLO(0);

    int memsize;
    bool bbplayer = (*MI_VERSION & 0xF0) == 0xB0;
    if (!bbplayer) {
        memsize = rdram_init(mem_bank_init);
        memsize = 8<<20;
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

    // Copy the IPL3 stage2 (loader.c) from ROM to the end of RDRAM.
    extern uint32_t __stage2_start[]; extern int __stage2_size;
    int stage2_size = (int)&__stage2_size;
    void *rdram_stage2 = LOADER_BASE(memsize, stage2_size);
    *PI_DRAM_ADDR = (uint32_t)rdram_stage2;
    *PI_CART_ADDR = (uint32_t)__stage2_start - 0xA0000000;
    *PI_WR_LEN = stage2_size-1;

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

    while (*PI_STATUS & 1) {}

    // Jump to stage 2 in RDRAM.
    MEMORY_BARRIER();
    asm("move $sp, %0"::"r"(STACK2_TOP(memsize, stage2_size)));
    goto *rdram_stage2;
}
