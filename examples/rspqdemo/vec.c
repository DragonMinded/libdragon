#include "vec.h"
#include <string.h>

DEFINE_RSP_UCODE(rsp_vec);

uint32_t vec_id;

void vec_init()
{
    rspq_init();

    // Initialize the saved state
    void* state = UncachedAddr(rspq_overlay_get_state(&rsp_vec));
    memset(state, 0, 0x400);

    // Register the overlay
    vec_id = rspq_overlay_register(&rsp_vec);
}

void vec_close()
{
    rspq_overlay_unregister(vec_id);
}

void floats_to_vectors(vec_slot_t *dest, const float *source, uint32_t source_length)
{
    for (uint32_t i = 0; i < source_length; ++i)
    {
        int32_t fixed = source[i]*(1<<16);
        uint32_t dest_idx = i/8;
        uint32_t lane_idx = i%8;
        dest[dest_idx].i[lane_idx] = (uint16_t)((fixed & 0xFFFF0000) >> 16);
        dest[dest_idx].f[lane_idx] = (uint16_t)(fixed & 0x0000FFFF);
    }
}

void vectors_to_floats(float *dest, const vec_slot_t *source, uint32_t dest_length)
{
    for (uint32_t i = 0; i < dest_length; ++i)
    {
        uint32_t src_idx = i/8;
        uint32_t lane_idx = i%8;
        int32_t integer = source[src_idx].i[lane_idx];
        uint32_t fraction = source[src_idx].f[lane_idx];

        int32_t fixed = integer << 16 | fraction;

        dest[i] = ((float)fixed) / (1<<16);
    }
}

