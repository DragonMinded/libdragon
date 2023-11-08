#define STAGE2 1

#include "loader.h"
#include "minidragon.h"
#include "debug.h"

static inline void rsp_clear_dmem_async(void)
{
    *SP_RSP_ADDR = 0x0000;
    *SP_DRAM_ADDR = 8*1024*1024;
    *SP_RD_LEN = 4096-1;
}

__attribute__((used))
void loader(void)
{
    debugf("Hello from RDRAM ", __builtin_frame_address(0));

    rsp_clear_dmem_async();

    si_write(0x7FC, 0x8);  // PIF boot terminator
    while(1){}
}
