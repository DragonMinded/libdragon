#ifndef __GFX_INTERNAL
#define __GFX_INTERNAL

#define GFX_RDP_DMEM_BUFFER_SIZE 0x100
#define GFX_RDP_DRAM_BUFFER_SIZE 0x1000

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct gfx_state_s {
    uint8_t rdp_buffer[GFX_RDP_DMEM_BUFFER_SIZE];
    uint64_t other_modes;
    uint32_t dram_buffer;
    uint32_t dram_buffer_size;
    uint32_t dram_buffer_end;
    uint16_t dmem_buffer_ptr;
    uint16_t rdp_initialised;
} gfx_state_t;

#endif

#endif
