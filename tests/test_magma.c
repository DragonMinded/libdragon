#include <magma.h>
#include <magma_constants.h>
#include "../src/rspq/rspq_internal.h"
#include "../src/magma/rsp_magma.h"
#include "../src/utils.h"

#define RSPQ_INIT() \
    rspq_init(); DEFER(rspq_close());

#define MG_INIT() \
    RSPQ_INIT(); \
    mg_init(); DEFER(mg_close());

void assert_block_contents(const uint32_t *expected_commands, uint32_t expected_commands_count, const rspq_block_t *block, TestContext *ctx) 
{
    const uint32_t *current_cmd = block->cmds;
    uint32_t block_size = RSPQ_BLOCK_MIN_SIZE;

    const uint32_t *block_end = current_cmd + block_size;
    while (*--block_end == 0x00) {}
    uint32_t commands_left = block_end - current_cmd;

    for (size_t i = 0; i < expected_commands_count; i++)
    {
        uint32_t expected_cmd = expected_commands[i];
        uint32_t actual_cmd = *(current_cmd++);
        ASSERT_EQUAL_HEX(actual_cmd, expected_cmd, "unexpected block content at word %d", i);

        // Check if we need to jump to the next buffer
        if (--commands_left == 0) {
            uint32_t cmd = *current_cmd;
            if ((cmd>>24) == RSPQ_CMD_JUMP) {
                current_cmd = (const uint32_t*)VirtualUncachedAddr(cmd & 0xFFFFFF);
                if (block_size < RSPQ_BLOCK_MAX_SIZE) block_size *= 2;
                const uint32_t *block_end = current_cmd + block_size;
                while (*--block_end == 0x00) {}
                commands_left = block_end - current_cmd;
            } else {
                break;
            }
        }
    }

    uint32_t last_cmd = *current_cmd;
    const uint32_t return_cmd = RSPQ_CMD_RET<<24;
    ASSERT_EQUAL_HEX(last_cmd, return_cmd, "block is not %ld words long", expected_commands_count);
}

typedef struct {
    uint32_t buffer[0x1000];
    uint32_t count;
} cmd_buffer;

#define CMD_BUFFER_APPEND(buf, cmd) buf.buffer[buf.count++] = (cmd)

#define VTX(cmd_buf, cnt, off, buf) CMD_BUFFER_APPEND(cmd_buf, mg_overlay_id | (MG_CMD_LOAD_VERTICES<<24) | (buf)); CMD_BUFFER_APPEND(cmd_buf, ((off)<<16) | (cnt));
#define INDEX(i) ((i) * MG_VTX_SIZE + RSP_MAGMA_MG_VERTEX_CACHE)
#define TRI(cmd_buf, i0, i1, i2) CMD_BUFFER_APPEND(cmd_buf, mg_overlay_id | (MG_CMD_DRAW_INDICES<<24) | (INDEX(i0))); CMD_BUFFER_APPEND(cmd_buf, (INDEX(i1)<<16) | INDEX(i2));

void assert_draw(const uint32_t *expected_commands, uint32_t expected_commands_count, const mg_input_assembly_parms_t *input_assembly_parms, uint32_t count, uint32_t offset, TestContext *ctx)
{
    rspq_block_begin();
        mg_draw(input_assembly_parms, count, offset);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    assert_block_contents(expected_commands, expected_commands_count, block, ctx);
}

void assert_draw_indexed(const uint32_t *expected_commands, uint32_t expected_commands_count, const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t count, uint32_t offset, TestContext *ctx)
{
    rspq_block_begin();
        mg_draw_indexed(input_assembly_parms, indices, count, offset);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    assert_block_contents(expected_commands, expected_commands_count, block, ctx);
}

void test_mg_draw_triangle_list(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint32_t count = 6;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 3, 4, 5);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, count, 0, ctx);
}

void test_mg_draw_triangle_list_non_div3(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint32_t count = 8;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 3, 4, 5);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, count, 0, ctx);
}

void test_mg_draw_triangle_list_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint32_t max_vtx_count = ROUND_DOWN(MG_VERTEX_CACHE_COUNT, 3);
    const uint32_t vtx_count = max_vtx_count + 6;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, max_vtx_count, 0, 0);
    for (size_t i = 0; i < max_vtx_count / 3; i++)
    {
        TRI(cmd_buf, i*3+0, i*3+1, i*3+2);
    }
    VTX(cmd_buf, 6, 0, max_vtx_count);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 3, 4, 5);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, vtx_count, 0, ctx);
}

void test_mg_draw_triangle_strip(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
    };

    const uint32_t count = 6;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 1, 3, 2);
    TRI(cmd_buf, 2, 3, 4);
    TRI(cmd_buf, 3, 5, 4);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, count, 0, ctx);
}

void test_mg_draw_triangle_strip_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
    };

    const uint32_t count = MG_VERTEX_CACHE_COUNT + 4;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, MG_VERTEX_CACHE_COUNT, 0, 0);
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT - 2; i++)
    {
        TRI(cmd_buf, i, i+(1+i%2), i+(2-i%2));
    }
    VTX(cmd_buf, 6, 0, MG_VERTEX_CACHE_COUNT-2);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 1, 3, 2);
    TRI(cmd_buf, 2, 3, 4);
    TRI(cmd_buf, 3, 5, 4);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, count, 0, ctx);
}

void test_mg_draw_triangle_fan(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
    };

    const uint32_t count = 6;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 1, 2, 0);
    TRI(cmd_buf, 2, 3, 0);
    TRI(cmd_buf, 3, 4, 0);
    TRI(cmd_buf, 4, 5, 0);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, count, 0, ctx);
}

void test_mg_draw_triangle_fan_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
    };

    const uint32_t count = MG_VERTEX_CACHE_COUNT*2 + 2;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, MG_VERTEX_CACHE_COUNT, 0, 0);
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT - 2; i++)
    {
        TRI(cmd_buf, i+1, i+2, 0);
    }
    VTX(cmd_buf, MG_VERTEX_CACHE_COUNT-1, 1, MG_VERTEX_CACHE_COUNT-1);
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT - 2; i++)
    {
        TRI(cmd_buf, i+1, i+2, 0);
    }
    VTX(cmd_buf, 5, 1, 1 + (MG_VERTEX_CACHE_COUNT-2)*2);
    TRI(cmd_buf, 1, 2, 0);
    TRI(cmd_buf, 2, 3, 0);
    TRI(cmd_buf, 3, 4, 0);
    TRI(cmd_buf, 4, 5, 0);

    assert_draw(cmd_buf.buffer, cmd_buf.count, &parms, count, 0, ctx);
}

void test_mg_draw_indexed_one_tri(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        0, 1, 2
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 3, 0, 0);
    TRI(cmd_buf, 0, 1, 2);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_two_tris(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        0, 1, 2, 3, 2, 1
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 4, 0, 0);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 3, 2, 1);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint32_t max_vtx_count = ROUND_DOWN(MG_VERTEX_CACHE_COUNT, 3);
    const uint32_t vtx_count = max_vtx_count + 6;

    uint16_t indices[vtx_count];
    for (size_t i = 0; i < vtx_count; i++) indices[i] = i;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, max_vtx_count, 0, 0);
    for (size_t i = 0; i < max_vtx_count / 3; i++)
    {
        TRI(cmd_buf, i*3+0, i*3+1, i*3+2);
    }
    VTX(cmd_buf, 6, 0, max_vtx_count);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 3, 4, 5);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_full_one_extra(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

     // TODO: This test is incorrect if MG_VERTEX_CACHE_COUNT is not a multiple of 3
    const uint32_t max_vtx_count = ROUND_DOWN(MG_VERTEX_CACHE_COUNT, 3);
    
    uint16_t indices[max_vtx_count];
    for (size_t i = 0; i < max_vtx_count-1; i++) indices[i] = i;
    indices[max_vtx_count-1] = 50;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, max_vtx_count-1, 0, 0);
    VTX(cmd_buf, 1, max_vtx_count-1, 50);
    for (size_t i = 0; i < max_vtx_count / 3; i++)
    {
        TRI(cmd_buf, i*3+0, i*3+1, i*3+2);
    }

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_fragmented_batch(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        0, 1, 2, 41, 42, 43
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 3, 0, 0);
    VTX(cmd_buf, 3, 3, 41);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 3, 4, 5);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_frag_backwards(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        41, 42, 43, 0, 1, 2
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 3, 0, 0);
    VTX(cmd_buf, 3, 3, 41);
    TRI(cmd_buf, 3, 4, 5);
    TRI(cmd_buf, 0, 1, 2);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_holes(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        0, 4, 15
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 1, 0, 0);
    VTX(cmd_buf, 1, 1, 4);
    VTX(cmd_buf, 1, 2, 15);
    TRI(cmd_buf, 0, 1, 2);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_out_of_order(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        0, 2, 1, 0, 3, 2
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 4, 0, 0);
    TRI(cmd_buf, 0, 2, 1);
    TRI(cmd_buf, 0, 3, 2);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_coalescing(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        5, 0, 3, 1, 4, 2
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 5, 0, 3);
    TRI(cmd_buf, 1, 4, 2);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_strip(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitive_restart_enabled = false
    };

    const uint16_t indices[] = {
        0, 1, 2, 3, 4, 5
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 1, 3, 2);
    TRI(cmd_buf, 2, 3, 4);
    TRI(cmd_buf, 3, 5, 4);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_strip_full(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitive_restart_enabled = false
    };

    const size_t count = MG_VERTEX_CACHE_COUNT + 4;
    uint16_t indices[count];
    for (size_t i = 0; i < count; i++) indices[i] = i;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, MG_VERTEX_CACHE_COUNT, 0, 0);
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT - 2; i++)
    {
        TRI(cmd_buf, i, i+(1+i%2), i+(2-i%2));
    }
    VTX(cmd_buf, 6, 0, MG_VERTEX_CACHE_COUNT-2);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 1, 3, 2);
    TRI(cmd_buf, 2, 3, 4);
    TRI(cmd_buf, 3, 5, 4);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_fan(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        .primitive_restart_enabled = false
    };

    const uint16_t indices[] = {
        0, 1, 2, 3, 4, 5
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 6, 0, 0);
    TRI(cmd_buf, 1, 2, 0);
    TRI(cmd_buf, 2, 3, 0);
    TRI(cmd_buf, 3, 4, 0);
    TRI(cmd_buf, 4, 5, 0);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_fan_full(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        .primitive_restart_enabled = false
    };

    const uint32_t count = MG_VERTEX_CACHE_COUNT*2 + 2;
    uint16_t indices[count];
    for (size_t i = 0; i < count; i++) indices[i] = i;

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, MG_VERTEX_CACHE_COUNT, 0, 0);
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT - 2; i++)
    {
        TRI(cmd_buf, i+1, i+2, 0);
    }
    VTX(cmd_buf, MG_VERTEX_CACHE_COUNT-1, 1, MG_VERTEX_CACHE_COUNT-1);
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT - 2; i++)
    {
        TRI(cmd_buf, i+1, i+2, 0);
    }
    VTX(cmd_buf, 5, 1, 1 + (MG_VERTEX_CACHE_COUNT-2)*2);
    TRI(cmd_buf, 1, 2, 0);
    TRI(cmd_buf, 2, 3, 0);
    TRI(cmd_buf, 3, 4, 0);
    TRI(cmd_buf, 4, 5, 0);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_restart_strip(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitive_restart_enabled = true
    };

    const uint16_t indices[] = {
        0, 1, 2, 3, 4, -1, 5, 6, 7, 8
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 9, 0, 0);
    TRI(cmd_buf, 0, 1, 2);
    TRI(cmd_buf, 1, 3, 2);
    TRI(cmd_buf, 2, 3, 4);
    TRI(cmd_buf, 5, 6, 7);
    TRI(cmd_buf, 6, 8, 7);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_restart_fan(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        .primitive_restart_enabled = true
    };

    const uint16_t indices[] = {
        0, 1, 2, 3, 4, -1, 5, 6, 7, 8
    };

    cmd_buffer cmd_buf = {0};
    VTX(cmd_buf, 9, 0, 0);
    TRI(cmd_buf, 1, 2, 0);
    TRI(cmd_buf, 2, 3, 0);
    TRI(cmd_buf, 3, 4, 0);
    TRI(cmd_buf, 6, 7, 5);
    TRI(cmd_buf, 7, 8, 5);

    assert_draw_indexed(cmd_buf.buffer, cmd_buf.count, &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}
