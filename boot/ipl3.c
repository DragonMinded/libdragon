#include <stdint.h>
#include <stdbool.h>
#include "n64sys.h"
#include "cop0.h"
#include "minidragon.h"
#include "debug.h"
#include "rdram.h"

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

__attribute__((noreturn, section(".boot")))
void _start(void)
{
    register uint32_t ipl2_romType   asm ("s3"); (void)ipl2_romType;
    register uint32_t ipl2_tvType    asm ("s4"); (void)ipl2_tvType;
    register uint32_t ipl2_resetType asm ("s5"); (void)ipl2_resetType;
    register uint32_t ipl2_romSeed   asm ("s6"); (void)ipl2_romSeed;
    register uint32_t ipl2_version   asm ("s7"); (void)ipl2_version;

    usb_init();
    debugf("Libdragon IPL3");
    
    C0_WRITE_CR(0);
    C0_WRITE_COUNT(0);
    C0_WRITE_COMPARE(0);

    int memsize = rdram_init(); (void)memsize;
    cop0_clear_cache();

    si_write(0x7FC, 0x8);  // PIF boot terminator

    // Perform a memtest
    // memtest(memsize);

    debugf("IPL3 done");
    while(1) {}
}
