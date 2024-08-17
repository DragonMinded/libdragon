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
    // which jumps to our IPL3. Notice that n64tool will also overwrite
    // this to align it to the one found in the ELF, to try to coerce
    // iQue OS to use the same memory region used by the ELF.
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

static void bzero8(void *mem)
{
    asm ("sdl $0, 0(%0); sdr $0, 7(%0);" :: "r"(mem));
}

static void rsp_bzero_init(bool bbplayer)
{
    while (*SP_DMA_BUSY) {} 
    if (!bbplayer) {
        // We run a DMA from RDRAM address > 8MiB where many areas return 0 on read.
        // Notice that we can do this only after RI has been initialized.
        *SP_RSP_ADDR = 0x1000;
        *SP_DRAM_ADDR = 8*1024*1024 + 0x2000;
        *SP_RD_LEN = 4096-1;
    } else {
        // iQue RAM is mirrored instead, so we can't use the above trick. Just use
        // CPU to clear IMEM.
        for (int i=0; i<4096/4; i++) {
            SP_IMEM[i] = 0;
        }
    }
}

// Clear memory using RSP DMA. We use IMEM as source address, which
// was cleared in mem_bank_init(). The size can be anything up to 1 MiB,
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
        int sz;
        if (size >= 1024*1024) sz = 1024*1024;
        else if (size >= 4096) sz = (size >> 12) << 12;
        else sz = size;

        while (*SP_DMA_FULL) {}
        *SP_RSP_ADDR = 0x1000;
        *SP_DRAM_ADDR = rdram; // this is automatically rounded down
        *SP_WR_LEN = sz-1;     // this is automatically rounded up
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
    if (chip_id == -1) {
        // First call, we clear SP_IMEM that will be used later.
        rsp_bzero_init(false);
        return;
    }
 
    uint32_t base = chip_id*1024*1024;
    int size = 2*1024*1024;

    if (last) {
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
    // Move the stack to DMEM. An alternative would be to use the cache
    // instead, which is technically faster (though the final performance
    // difference is not measurable), but it would make the life harder
    // for emulators for very little gain.
    asm ("li $sp, %0"::"i"((uint32_t)SP_DMEM + 4096 - 0x10));
    __builtin_unreachable(); // avoid function epilog, fallthrough to stage1 (see linker script)
}

__attribute__((noreturn, section(".stage1")))
void stage1(void)
{
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

    if (!bbplayer && *RI_SELECT == 0) {
        memsize = rdram_init(mem_bank_init);
    } else {
        if (bbplayer) {
            // iQue doesn't have a IPL2 and the OS provides boot flags already in lowmem.
            // Read the ones we need for our bootflags.
            ipl2_tvType    = *(uint32_t *)0xA0000300;
            ipl2_resetType = *(uint32_t *)0xA000030C;

            // iQue OS put the memory size in a special location. This is the
            // amount of memory that the OS has assigned to the application, so it
            // could be less than the physical total memory. Anyway, it's the value
            // we should use and pass along.
            memsize = *(uint32_t*)0xA0000318;

            // Notice that even if 8 MiB were allocated, the top of the memory is
            // in-use by save state emulation, so we shouldn't access it anyway.
            if (memsize == 0x800000)
                memsize = 0x7C0000;

            if (memsize == 0x400000 && io_read(0xB0000008) >= 0x80400000) {
                // This is a special case for writing code in SA2 context on iQue.
                // In that case, the ELF must be located above 4 MiB, but 0xA0000318
                // is initialized to 4 MiB. We need to boot the application by
                // reporting the true size of the memory.
                memsize = 0x800000;
            }

            // iQue has a hardware RNG. Use that to fetch 32 bits of entropy
            uint32_t rng = 0;
            for (int i=0;i<32;i++)
                rng = (rng << 1) | (*MI_IQUE_RNG & 1);
            entropy_add(rng);
        } else {
            // On warm boots, 
            int chip_id = 0;
            memsize = 0;
            for (chip_id=0; chip_id<8; chip_id+=2) {
                volatile uint32_t *ptr = (void*)0xA0000000 + chip_id * 1024 * 1024;
                ptr[0]=0;
                ptr[0]=0x12345678;
                if (ptr[0] != 0x12345678) break;
                memsize += 2*1024*1024;
            }
        }

        // Clear memory. Skip the first 0x400 bytes of RAM because it
        // historically contains some boot flags that some existing code
        // might expect to stay there.
        // For instance, the Everdrive menu expects 0x80000318 to still
        // contain the RDRAM size after a warm boot, and we need to comply
        // with this even if Everdrive itself doesn't use this IPL3 (but
        // might boot a game that does, and that game shouldn't clear
        // 0x80000318).
        rsp_bzero_init(bbplayer);
        rsp_bzero_async(0xA0000400, memsize-0x400-TOTAL_RESERVED_SIZE);
    }

    debugf("Total memory: ", memsize);

    // Copy the IPL3 stage2 (loader.c) from ROM to the end of RDRAM.
    extern uint32_t __stage2_start[];
    uint32_t stage2_start = (uint32_t)__stage2_start;
    int stage2_size = io_read(stage2_start);
    stage2_start += 8;
    debugf("stage2 ", stage2_start, stage2_size);

    void *rdram_stage2 = LOADER_BASE(memsize, stage2_size);
    *PI_DRAM_ADDR = (uint32_t)rdram_stage2;
    *PI_CART_ADDR = (uint32_t)stage2_start - 0xA0000000;
    while (*SP_DMA_BUSY) {}         // Make sure RDRAM clearing is finished before reading data
    *PI_WR_LEN = stage2_size-1;

    // Clear D/I-cache, useful after warm boot. Maybe not useful for cold
    // boots, but the manual says that the cache state is invalid at boot,
    // so a reset won't hurt.
    cop0_clear_cache();

    // Fill boot information at beginning of DMEM. The rest of IMEM has been
    // cleared by now anyway. Notice that we also store BSS in IMEM, so the
    // linker script reserves initial part to boot information.
#ifndef COMPAT
    bootinfo_t *bootinfo = (bootinfo_t*)0xA4000000;
    bootinfo->memory_size = memsize;
    bootinfo->flags = (ipl2_romType << 24) | (ipl2_tvType << 16) | (ipl2_resetType << 8) | (bbplayer ? 1 : 0);
    bootinfo->padding = 0;
#else
    if (!bbplayer) {
        *(volatile uint32_t *)0x80000300 = ipl2_tvType;
        *(volatile uint32_t *)0x80000304 = ipl2_romType;
        *(volatile uint32_t *)0x80000308 = ipl2_romType ? 0xA6000000 : 0xB0000000;
        *(volatile uint32_t *)0x8000030C = ipl2_resetType;
        *(volatile uint32_t *)0x80000314 = ipl2_version;
        *(volatile uint32_t *)0x80000318 = memsize;
        data_cache_hit_writeback_invalidate((void*)0x80000300, 0x20);
    }
#endif

    // Wait until stage 2 is fully loaded into RDRAM
    while (*PI_STATUS & 1) {}

    // Jump to stage 2 in RDRAM.
    MEMORY_BARRIER();
    asm("move $sp, %0"::"r"(STACK2_TOP(memsize, stage2_size)));
    goto *rdram_stage2;
}
