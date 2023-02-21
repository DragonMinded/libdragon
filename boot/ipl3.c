#include <stdint.h>
#include <stdbool.h>
#include "n64sys.h"
#include "cop0.h"
#include "minidragon.h"

#define DEBUG 1
#include "debug.h"

#include "rdram.c"

__attribute__((noreturn, section(".boot")))
void _start(void)
{
    register uint32_t ipl2_romType   asm ("s3"); (void)ipl2_romType;
    register uint32_t ipl2_tvType    asm ("s4"); (void)ipl2_tvType;
    register uint32_t ipl2_resetType asm ("s5"); (void)ipl2_resetType;
    register uint32_t ipl2_romSeed   asm ("s6"); (void)ipl2_romSeed;
    register uint32_t ipl2_version   asm ("s7"); (void)ipl2_version;

    dragon_init();
    usb_init();
    debugf("Libdragon IPL3");
    debugf("C0_STATUS: ", C0_STATUS());
    
    C0_WRITE_CR(0);
    C0_WRITE_COUNT(0);
    C0_WRITE_COMPARE(0);

    rdram_init();

    debugf("END");
    si_write(0x7FC, 0x8);  // PIF boot terminator
    while(1) {}
}
