#include <magma.h>
#include <magma_constants.h>
#include "../src/rspq/rspq_internal.h"
#include "../src/magma/rsp_magma.h"

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

#define VTX(cnt, off, buf) (mg_overlay_id | (MG_CMD_LOAD_VERTICES<<24) | (buf)), ((off<<16) | (cnt))
#define INDEX(i) ((i) * MG_VTX_SIZE + RSP_MAGMA_MG_VERTEX_CACHE)
#define TRI(i0, i1, i2) (mg_overlay_id | (MG_CMD_DRAW_INDICES<<24) | (INDEX(i0))), ((INDEX(i1)<<16) | INDEX(i2))

void assert_draw(const uint32_t *expected_commands, uint32_t expected_commands_count, const mg_input_assembly_parms_t *input_assembly_parms, uint32_t count, uint32_t offset, TestContext *ctx)
{
    rspq_block_begin();
        mg_draw(input_assembly_parms, count, offset);
    rspq_block_t *block = rspq_block_end();

    assert_block_contents(expected_commands, expected_commands_count, block, ctx);
}

void assert_draw_indexed(const uint32_t *expected_commands, uint32_t expected_commands_count, const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t count, uint32_t offset, TestContext *ctx)
{
    rspq_block_begin();
        mg_draw_indexed(input_assembly_parms, indices, count, offset);
    rspq_block_t *block = rspq_block_end();

    assert_block_contents(expected_commands, expected_commands_count, block, ctx);
}

void test_mg_set_viewport(TestContext *ctx)
{
    MG_INIT();

    const uint32_t expected_commands[] = {
        mg_overlay_id | (MG_CMD_SET_QUAD<<24) | offsetof(mg_rsp_state_t, viewport),
        (1280<<16) | 960,
        2,
        (640<<16) | 480,
        0
    };

    rspq_block_begin();
        mg_set_viewport(&(mg_viewport_t) {
            .x = 0,
            .y = 0,
            .width = 320,
            .height = 240
        });
    rspq_block_t *block = rspq_block_end();

    assert_block_contents(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), block, ctx);
}

void test_mg_draw_triangle_list(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    uint32_t count = 6;

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
}

void test_mg_draw_triangle_list_non_div3(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    uint32_t count = 8;

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
}

void test_mg_draw_triangle_list_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    uint32_t count = 36;

    const uint32_t expected_commands[] = {
        VTX(30, 0, 0),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
        TRI(6, 7, 8),
        TRI(9, 10, 11),
        TRI(12, 13, 14),
        TRI(15, 16, 17),
        TRI(18, 19, 20),
        TRI(21, 22, 23),
        TRI(24, 25, 26),
        TRI(27, 28, 29),
        VTX(6, 0, 30),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
}

void test_mg_draw_triangle_strip(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
    };

    uint32_t count = 6;

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(3, 5, 4),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
}

void test_mg_draw_triangle_strip_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
    };

    uint32_t count = 36;

    const uint32_t expected_commands[] = {
        VTX(32, 0, 0),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(3, 5, 4),
        TRI(4, 5, 6),
        TRI(5, 7, 6),
        TRI(6, 7, 8),
        TRI(7, 9, 8),
        TRI(8, 9, 10),
        TRI(9, 11, 10),
        TRI(10, 11, 12),
        TRI(11, 13, 12),
        TRI(12, 13, 14),
        TRI(13, 15, 14),
        TRI(14, 15, 16),
        TRI(15, 17, 16),
        TRI(16, 17, 18),
        TRI(17, 19, 18),
        TRI(18, 19, 20),
        TRI(19, 21, 20),
        TRI(20, 21, 22),
        TRI(21, 23, 22),
        TRI(22, 23, 24),
        TRI(23, 25, 24),
        TRI(24, 25, 26),
        TRI(25, 27, 26),
        TRI(26, 27, 28),
        TRI(27, 29, 28),
        TRI(28, 29, 30),
        TRI(29, 31, 30),
        VTX(6, 0, 30),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(3, 5, 4),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
}

void test_mg_draw_triangle_fan(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
    };

    uint32_t count = 6;

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
}

void test_mg_draw_triangle_fan_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
    };

    uint32_t count = 66;

    const uint32_t expected_commands[] = {
        VTX(32, 0, 0),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
        TRI(5, 6, 0),
        TRI(6, 7, 0),
        TRI(7, 8, 0),
        TRI(8, 9, 0),
        TRI(9, 10, 0),
        TRI(10, 11, 0),
        TRI(11, 12, 0),
        TRI(12, 13, 0),
        TRI(13, 14, 0),
        TRI(14, 15, 0),
        TRI(15, 16, 0),
        TRI(16, 17, 0),
        TRI(17, 18, 0),
        TRI(18, 19, 0),
        TRI(19, 20, 0),
        TRI(20, 21, 0),
        TRI(21, 22, 0),
        TRI(22, 23, 0),
        TRI(23, 24, 0),
        TRI(24, 25, 0),
        TRI(25, 26, 0),
        TRI(26, 27, 0),
        TRI(27, 28, 0),
        TRI(28, 29, 0),
        TRI(29, 30, 0),
        TRI(30, 31, 0),
        VTX(31, 1, 31),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
        TRI(5, 6, 0),
        TRI(6, 7, 0),
        TRI(7, 8, 0),
        TRI(8, 9, 0),
        TRI(9, 10, 0),
        TRI(10, 11, 0),
        TRI(11, 12, 0),
        TRI(12, 13, 0),
        TRI(13, 14, 0),
        TRI(14, 15, 0),
        TRI(15, 16, 0),
        TRI(16, 17, 0),
        TRI(17, 18, 0),
        TRI(18, 19, 0),
        TRI(19, 20, 0),
        TRI(20, 21, 0),
        TRI(21, 22, 0),
        TRI(22, 23, 0),
        TRI(23, 24, 0),
        TRI(24, 25, 0),
        TRI(25, 26, 0),
        TRI(26, 27, 0),
        TRI(27, 28, 0),
        TRI(28, 29, 0),
        TRI(29, 30, 0),
        TRI(30, 31, 0),
        VTX(5, 1, 61),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
    };

    assert_draw(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, count, 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(3, 0, 0),
        TRI(0, 1, 2),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(4, 0, 0),
        TRI(0, 1, 2),
        TRI(3, 2, 1),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_full_cache(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const size_t count = 36;
    uint16_t indices[count];
    for (size_t i = 0; i < count; i++) indices[i] = i;

    const uint32_t expected_commands[] = {
        VTX(30, 0, 0),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
        TRI(6, 7, 8),
        TRI(9, 10, 11),
        TRI(12, 13, 14),
        TRI(15, 16, 17),
        TRI(18, 19, 20),
        TRI(21, 22, 23),
        TRI(24, 25, 26),
        TRI(27, 28, 29),
        VTX(6, 0, 30),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_full_one_extra(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const uint16_t indices[] = {
        0, 1, 2, 
        3, 4, 5,
        6, 7, 8,
        9, 10, 11,
        12, 13, 14,
        15, 16, 17,
        18, 19, 20,
        21, 22, 23,
        24, 25, 26,
        27, 28, 29,
        30, 29, 50
    };

    const uint32_t expected_commands[] = {
        VTX(31, 0, 0),
        VTX(1, 31, 50),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
        TRI(6, 7, 8),
        TRI(9, 10, 11),
        TRI(12, 13, 14),
        TRI(15, 16, 17),
        TRI(18, 19, 20),
        TRI(21, 22, 23),
        TRI(24, 25, 26),
        TRI(27, 28, 29),
        TRI(30, 29, 31),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(3, 0, 0),
        VTX(3, 3, 41),
        TRI(0, 1, 2),
        TRI(3, 4, 5),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(3, 0, 0),
        VTX(3, 3, 41),
        TRI(3, 4, 5),
        TRI(0, 1, 2),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(1, 0, 0),
        VTX(1, 1, 4),
        VTX(1, 2, 15),
        TRI(0, 1, 2),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(4, 0, 0),
        TRI(0, 2, 1),
        TRI(0, 3, 2),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(5, 0, 3),
        TRI(1, 4, 2),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(3, 5, 4),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_strip_full(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitive_restart_enabled = false
    };

    const size_t count = 36;
    uint16_t indices[count];
    for (size_t i = 0; i < count; i++) indices[i] = i;

    const uint32_t expected_commands[] = {
        VTX(32, 0, 0),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(3, 5, 4),
        TRI(4, 5, 6),
        TRI(5, 7, 6),
        TRI(6, 7, 8),
        TRI(7, 9, 8),
        TRI(8, 9, 10),
        TRI(9, 11, 10),
        TRI(10, 11, 12),
        TRI(11, 13, 12),
        TRI(12, 13, 14),
        TRI(13, 15, 14),
        TRI(14, 15, 16),
        TRI(15, 17, 16),
        TRI(16, 17, 18),
        TRI(17, 19, 18),
        TRI(18, 19, 20),
        TRI(19, 21, 20),
        TRI(20, 21, 22),
        TRI(21, 23, 22),
        TRI(22, 23, 24),
        TRI(23, 25, 24),
        TRI(24, 25, 26),
        TRI(25, 27, 26),
        TRI(26, 27, 28),
        TRI(27, 29, 28),
        TRI(28, 29, 30),
        TRI(29, 31, 30),
        VTX(6, 0, 30),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(3, 5, 4),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(6, 0, 0),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}

void test_mg_draw_indexed_fan_full(TestContext *ctx)
{
    MG_INIT();

    mg_input_assembly_parms_t parms = {
        .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        .primitive_restart_enabled = false
    };

    const size_t count = 66;
    uint16_t indices[count];
    for (size_t i = 0; i < count; i++) indices[i] = i;

    const uint32_t expected_commands[] = {
        VTX(32, 0, 0),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
        TRI(5, 6, 0),
        TRI(6, 7, 0),
        TRI(7, 8, 0),
        TRI(8, 9, 0),
        TRI(9, 10, 0),
        TRI(10, 11, 0),
        TRI(11, 12, 0),
        TRI(12, 13, 0),
        TRI(13, 14, 0),
        TRI(14, 15, 0),
        TRI(15, 16, 0),
        TRI(16, 17, 0),
        TRI(17, 18, 0),
        TRI(18, 19, 0),
        TRI(19, 20, 0),
        TRI(20, 21, 0),
        TRI(21, 22, 0),
        TRI(22, 23, 0),
        TRI(23, 24, 0),
        TRI(24, 25, 0),
        TRI(25, 26, 0),
        TRI(26, 27, 0),
        TRI(27, 28, 0),
        TRI(28, 29, 0),
        TRI(29, 30, 0),
        TRI(30, 31, 0),
        VTX(31, 1, 31),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
        TRI(5, 6, 0),
        TRI(6, 7, 0),
        TRI(7, 8, 0),
        TRI(8, 9, 0),
        TRI(9, 10, 0),
        TRI(10, 11, 0),
        TRI(11, 12, 0),
        TRI(12, 13, 0),
        TRI(13, 14, 0),
        TRI(14, 15, 0),
        TRI(15, 16, 0),
        TRI(16, 17, 0),
        TRI(17, 18, 0),
        TRI(18, 19, 0),
        TRI(19, 20, 0),
        TRI(20, 21, 0),
        TRI(21, 22, 0),
        TRI(22, 23, 0),
        TRI(23, 24, 0),
        TRI(24, 25, 0),
        TRI(25, 26, 0),
        TRI(26, 27, 0),
        TRI(27, 28, 0),
        TRI(28, 29, 0),
        TRI(29, 30, 0),
        TRI(30, 31, 0),
        VTX(5, 1, 61),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(4, 5, 0),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(9, 0, 0),
        TRI(0, 1, 2),
        TRI(1, 3, 2),
        TRI(2, 3, 4),
        TRI(5, 6, 7),
        TRI(6, 8, 7),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
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

    const uint32_t expected_commands[] = {
        VTX(9, 0, 0),
        TRI(1, 2, 0),
        TRI(2, 3, 0),
        TRI(3, 4, 0),
        TRI(6, 7, 5),
        TRI(7, 8, 5),
    };

    assert_draw_indexed(expected_commands, sizeof(expected_commands)/sizeof(expected_commands[0]), &parms, indices, sizeof(indices)/sizeof(indices[0]), 0, ctx);
}
