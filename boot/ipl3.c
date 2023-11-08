#include <stdint.h>
#include <stdbool.h>
#include "n64sys.h"
#include "cop0.h"
#include "minidragon.h"
#include "debug.h"
#include "rdram.h"
#include "loader.h"

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
    uint32_t tv_type;
    uint32_t reset_type;
} bootinfo_t;

extern int __stage2_size;

static inline void rsp_clear_imem_async(void)
{
    *SP_RSP_ADDR = 0x1000; // IMEM
    *SP_DRAM_ADDR = 8*1024*1024; // RDRAM addresses >8 MiB always return 0
    *SP_RD_LEN = 4096-1;
}

__attribute__((noreturn, section(".boot")))
void _start(void)
{
    register uint32_t ipl2_romType   asm ("s3"); (void)ipl2_romType;
    register uint32_t ipl2_tvType    asm ("s4"); (void)ipl2_tvType;
    register uint32_t ipl2_resetType asm ("s5"); (void)ipl2_resetType;
    register uint32_t ipl2_romSeed   asm ("s6"); (void)ipl2_romSeed;
    register uint32_t ipl2_version   asm ("s7"); (void)ipl2_version;

    // Clear IMEM (contains IPL2). We don't need it anymore, and we can 
    // instead use IMEM as a zero-buffer for RSP DMA.
    // Also, we put our bss in IMEM.
    rsp_clear_imem_async();

    usb_init();
    debugf("Libdragon IPL3");
    
    C0_WRITE_CR(0);
    C0_WRITE_COUNT(0);
    C0_WRITE_COMPARE(0);


    int memsize = rdram_init(); (void)memsize;

    // Clear D/I-cache, useful after warm boot. Maybe not useful for cold
    // boots, but the manual says that the cache state is invalid at boot,
    // so a reset won't hurt.
    cop0_clear_cache();

    // Fill boot information at fixed RDRAM address
    bootinfo_t *bootinfo = (bootinfo_t*)0x80000000;
    bootinfo->memory_size = memsize;
    bootinfo->tv_type = ipl2_tvType;
    bootinfo->reset_type = ipl2_resetType;

    // Perform a memtest
    // memtest(memsize);

    // Copy the IPL3 stage2 (loader.c) from DMEM to the end of RDRAM.
    extern uint32_t __stage2_start[];
    int stage2_size = (int)&__stage2_size;
    void *rdram_stage2 = (void*)0x80000000 + memsize - stage2_size;
    rsp_dma_to_rdram(__stage2_start, rdram_stage2, stage2_size);

    // Jump to stage 2 in RDRAM.
    goto *rdram_stage2;
}
