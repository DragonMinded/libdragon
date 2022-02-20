#ifndef __GFX_INTERNAL
#define __GFX_INTERNAL

#define GFX_RDP_DMEM_BUFFER_SIZE 0x100

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct gfx_state_s {
    uint8_t rdp_buffer[GFX_RDP_DMEM_BUFFER_SIZE];
    uint64_t other_modes;
} gfx_state_t;

#endif

#endif
