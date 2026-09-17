#include "mg_ex.h"
#include "magma_constants.h"

#define MAX_QUADS_IN_BATCH_COUNT (MG_VERTEX_CACHE_COUNT/4)
#define QUAD_BATCH_SIZE (MAX_QUADS_IN_BATCH_COUNT*5)

static void mg_ex_draw_quads(const mg_input_assembly_parms_t *input_assembly_parms, uint32_t count, uint32_t first)
{
    uint16_t batch_indices[QUAD_BATCH_SIZE];

    const uint32_t prim_count = count / 4;
    for (uint32_t prim_index = 0; prim_index < prim_count; prim_index += MAX_QUADS_IN_BATCH_COUNT)
    {
        uint32_t prims_in_batch_count = MIN(prim_count - prim_index, MAX_QUADS_IN_BATCH_COUNT);
        for (uint32_t i = 0; i < prims_in_batch_count; i++)
        {
            for (uint32_t j = 0; j < 4; j++) 
                batch_indices[i*5 + j] = first + (prim_index+i)*4 + j;
            
            batch_indices[i*5 + 4] = -1;
        }

        mg_draw_indexed(input_assembly_parms, batch_indices, prims_in_batch_count*5, 0);
    }
}

void mg_ex_draw(const mg_input_assembly_parms_t *input_assembly_parms, uint32_t count, uint32_t first, GLenum mode)
{
    switch (mode)
    {
    case GL_QUADS:
        // Quads need special handling
        mg_ex_draw_quads(input_assembly_parms, count, first);
        break;
    case GL_QUAD_STRIP:
        // Quad strips are equivalent to triangle strips, except that only every 2 vertices form a new quad.
        // -> Round down to nearest multiple of 2 to avoid "half" a  quad being drawn.
        mg_draw(input_assembly_parms, ROUND_DOWN(count, 2), first);
        break;
    default:
        // Everything else is supported as-is, just pass through.
        mg_draw(input_assembly_parms, count, first);
        break;
    }
}

static void mg_ex_draw_quads_indexed(const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t count, int32_t offset)
{
    uint16_t batch_indices[QUAD_BATCH_SIZE];

    const uint32_t prim_count = count / 4;
    for (uint32_t prim_index = 0; prim_index < prim_count; prim_index += MAX_QUADS_IN_BATCH_COUNT)
    {
        uint32_t prims_in_batch_count = MIN(prim_count - prim_index, MAX_QUADS_IN_BATCH_COUNT);
        for (uint32_t i = 0; i < prims_in_batch_count; i++)
        {
            memcpy(batch_indices + i*5, indices + (prim_index+i)*4, sizeof(uint16_t)*4);
            batch_indices[i*5 + 4] = -1;
        }

        mg_draw_indexed(input_assembly_parms, batch_indices, prims_in_batch_count*5, offset);
    }
}

void mg_ex_draw_indexed(const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t count, int32_t offset, GLenum mode)
{
    switch (mode)
    {
    case GL_QUADS:
        mg_ex_draw_quads_indexed(input_assembly_parms, indices, count, offset);
        break;
    case GL_QUAD_STRIP:
        // Quad strips are equivalent to triangle strips, except that only every 2 vertices form a new quad.
        // -> Round down to nearest multiple of 2 to avoid "half" a  quad being drawn.
        mg_draw_indexed(input_assembly_parms, indices, ROUND_DOWN(count, 2), offset);
        break;
    default:
        // Everything else is supported as-is, just pass through.
        mg_draw_indexed(input_assembly_parms, indices, count, offset);
        break;
    }
}
