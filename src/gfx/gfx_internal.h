#ifndef __GFX_INTERNAL
#define __GFX_INTERNAL

#include <stdint.h>

#define RDP_DMEM_BUFFER_SIZE 0x100
#define RDP_DRAM_BUFFER_SIZE 0x1000

typedef struct gfx_t {
    uint8_t rdp_buffer[RDP_DMEM_BUFFER_SIZE];
    uint64_t other_modes;
    void *dram_buffer;
    uint32_t dram_buffer_size;
    uint32_t dram_buffer_end;
    uint16_t dmem_buffer_ptr;
    uint16_t rdp_initialised;
} gfx_t;

#endif
