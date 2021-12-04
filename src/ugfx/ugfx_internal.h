#ifndef __UGFX_INTERNAL
#define __UGFX_INTERNAL

#define UGFX_RDP_DMEM_BUFFER_SIZE 0x100
#define UGFX_RDP_DRAM_BUFFER_SIZE 0x1000

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct ugfx_state_t {
    uint8_t rdp_buffer[UGFX_RDP_DMEM_BUFFER_SIZE];
    uint64_t other_modes;
    void *dram_buffer;
    uint32_t dram_buffer_size;
    uint32_t dram_buffer_end;
    uint16_t dmem_buffer_ptr;
    uint16_t rdp_initialised;
} ugfx_state_t;

#endif

#endif
