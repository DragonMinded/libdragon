#include <malloc.h>
#include <math.h>
#include "../src/rspq/rspq_internal.h"
#include "../src/rdpq/rdpq_internal.h"
#include <rdpq_constants.h> 

#define BITS(v, b, e)  ((unsigned int)((v) << (63-(e)) >> (63-(e)+(b)))) 

static uint64_t rdp_stream[4096];
static struct {
    int idx;
    int num_cmds;
    int last_som;
    int last_cc;
} rdp_stream_ctx;

static void debug_rdp_stream(void *ctx, uint64_t *cmd, int sz) {
    if (rdp_stream_ctx.idx+sz >= 4096) return;

    switch (BITS(cmd[0],56,61)) {
    case 0x2F:
        rdp_stream_ctx.last_som = rdp_stream_ctx.idx;
        break;
    case 0x3C:
        rdp_stream_ctx.last_cc = rdp_stream_ctx.idx;
        break;
    }
    memcpy(rdp_stream + rdp_stream_ctx.idx, cmd, sz*8);
    rdp_stream_ctx.idx += sz;
    rdp_stream_ctx.num_cmds++;
}

static void debug_rdp_stream_reset(void) {
    memset(&rdp_stream_ctx, 0, sizeof(rdp_stream_ctx));
    rdp_stream_ctx.last_som = -1;
    rdp_stream_ctx.last_cc = -1;
}

static void debug_rdp_stream_init(void) {
    rspq_wait(); // avoid race conditions with pending commands
    debug_rdp_stream_reset();
    rdpq_debug_install_hook(debug_rdp_stream, NULL);
}

uint64_t debug_rdp_stream_last_som(void) {
    if (rdp_stream_ctx.last_som < 0) return 0;
    return rdp_stream[rdp_stream_ctx.last_som];
}

uint64_t debug_rdp_stream_last_cc(void) {
    if (rdp_stream_ctx.last_cc < 0) return 0;
    return rdp_stream[rdp_stream_ctx.last_cc];
}

const char* debug_rdp_stream_last_cc_disasm(void) {
    uint64_t cmds[1] = { debug_rdp_stream_last_cc() };
    static char buf[256];
    FILE *out = fmemopen(buf, sizeof(buf), "w");
    rdpq_debug_disasm(cmds, out);
    fclose(out);
    return buf;
}

uint32_t debug_rdp_stream_count_cmd(uint32_t cmd_id) {
    uint32_t count = 0;
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        if ((rdp_stream[i] >> 56) == cmd_id) {
            ++count;
        }
    }
    return count;
}

#define RDPQ_INIT() \
    rspq_init(); DEFER(rspq_close()); \
    rdpq_init(); DEFER(rdpq_close()); \
    rdpq_debug_start(); DEFER(rdpq_debug_stop());


static void surface_clear(surface_t *s, uint8_t c) {
    memset(s->buffer, c, s->height * s->stride);
}

__attribute__((unused))
static void debug_surface(const char *name, uint16_t *buf, int w, int h) {
    debugf("Surface %s:\n", name);
    for (int j=0;j<h;j++) {
        for (int i=0;i<w;i++) {
            debugf("%04x ", buf[j*w+i]);
        }
        debugf("\n");
    }
    debugf("\n");
}

__attribute__((unused))
static void debug_surface32(const char *name, uint32_t *buf, int w, int h) {
    debugf("Surface %s:\n", name);
    for (int j=0;j<h;j++) {
        for (int i=0;i<w;i++) {
            debugf("%08lx ", buf[j*w+i]);
        }
        debugf("\n");
    }
    debugf("\n");
}

static void assert_surface(TestContext *ctx, surface_t *surf, color_t (*check)(int, int), int diff)
{
    assertf(surface_get_format(surf) == FMT_RGBA32, "ASSERT_SURFACE only works with RGBA32");
    for (int y=0;y<surf->height;y++) {
        uint32_t *line = (uint32_t*)(surf->buffer + y*surf->stride);
        for (int x=0;x<surf->width;x++) {
            color_t exp = check(x, y);
            uint32_t exp32 = color_to_packed32(exp);
            uint32_t found32 = line[x];
            if (found32 != exp32) {
                if (diff) {
                    bool match = true;
                    for (int i=0;i<4;i++) {
                        uint8_t found = (found32 >> (i*8)) & 0xFF;
                        uint8_t exp = (exp32 >> (i*8)) & 0xFF;
                        if (ABS(found - exp) > diff) {
                            match = false;
                            break;
                        }
                    }
                    if (match)
                        continue;
                }

                debug_surface32("Found:", surf->buffer, surf->width, surf->height);
                ASSERT_EQUAL_HEX(found32, exp32, "invalid pixel at (%d,%d)", x, y);
            }
        }
    }
}

#define ASSERT_SURFACE_THRESHOLD(surf, thresh, func_body) ({ \
    color_t __check_surface(int x, int y) func_body; \
    assert_surface(ctx, surf, __check_surface, thresh); \
    if (ctx->result == TEST_FAILED) return; \
})

#define ASSERT_SURFACE(surf, func_body)  ASSERT_SURFACE_THRESHOLD(surf, 0, func_body)


void test_rdpq_rspqwait(TestContext *ctx)
{
    // Verify that rspq_wait() correctly also wait for RDP to terminate
    // all its scheduled operations.
    surface_t fb = surface_alloc(FMT_RGBA32, 128, 128);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);
    uint32_t *framebuffer = fb.buffer;

    RDPQ_INIT();

    color_t color = RGBA32(0x11, 0x22, 0x33, 0xFF);

    rdpq_set_mode_fill(color);
    rdpq_set_color_image(&fb);
    rdpq_fill_rectangle(0, 0, 128, 128);
    rspq_wait();

    // Sample the end of the buffer immediately after rspq_wait. If rspq_wait
    // doesn't wait for RDP to become idle, this pixel will not be filled at
    // this point. 
    ASSERT_EQUAL_HEX(framebuffer[127*128+127], color_to_packed32(color),
        "invalid color in framebuffer at (127,127)");
}

void test_rdpq_clear(TestContext *ctx)
{
    RDPQ_INIT();

    color_t fill_color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);

    surface_t fb = surface_alloc(FMT_RGBA16, 32, 32);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    rdpq_set_mode_fill(fill_color);
    rdpq_set_color_image(&fb);
    rdpq_fill_rectangle(0, 0, 32, 32);
    rspq_wait();

    uint16_t *framebuffer = fb.buffer;
    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(framebuffer[i], color_to_packed16(fill_color),
            "Framebuffer was not cleared properly! Index: %lu", i);
    }
}

void test_rdpq_dynamic(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 64;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0, sizeof(expected_fb));

    rdpq_set_mode_fill(RGBA32(0,0,0,0));
    rdpq_set_color_image(&fb);

    for (uint32_t y = 0; y < WIDTH; y++)
    {
        for (uint32_t x = 0; x < WIDTH; x += 4)
        {
            color_t c = RGBA16(x, y, x+y, x^y);
            expected_fb[y * WIDTH + x] = color_to_packed16(c);
            expected_fb[y * WIDTH + x + 1] = color_to_packed16(c);
            expected_fb[y * WIDTH + x + 2] = color_to_packed16(c);
            expected_fb[y * WIDTH + x + 3] = color_to_packed16(c);
            rdpq_set_fill_color(c);
            rdpq_set_scissor(x, y, x + 4, y + 1);
            rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
        }
    }

    rspq_wait();
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, "Framebuffer contains wrong data!");
}

void test_rdpq_passthrough_big(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rdpq_set_color_image(&fb);
    rdpq_set_blend_color(RGBA32(255,255,255,255));
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,0), (0,0,0,0)));
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, 0, BLEND_RGB, 1)));

    rdp_draw_filled_triangle(0, 0, WIDTH, 0, WIDTH, WIDTH);
    rdp_draw_filled_triangle(0, 0, 0, WIDTH, WIDTH, WIDTH);

    rspq_wait();
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, "Framebuffer contains wrong data!");
}

void test_rdpq_block(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 64;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0xAA);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0, sizeof(expected_fb));

    rspq_block_begin();
    rdpq_set_mode_fill(RGBA32(0,0,0,0));

    for (uint32_t y = 0; y < WIDTH; y++)
    {
        for (uint32_t x = 0; x < WIDTH; x += 4)
        {
            color_t c = RGBA16(x, y, x+y, x^y);
            expected_fb[y * WIDTH + x]     = color_to_packed16(c);
            expected_fb[y * WIDTH + x + 1] = color_to_packed16(c);
            expected_fb[y * WIDTH + x + 2] = color_to_packed16(c);
            expected_fb[y * WIDTH + x + 3] = color_to_packed16(c);
            rdpq_set_fill_color(c);
            rdpq_set_scissor(x, y, x + 4, y + 1);
            rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
        }
    }
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rdpq_set_color_image(&fb);
    rspq_block_run(block);
    rspq_wait();
    
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, "Framebuffer contains wrong data!");
}

void test_rdpq_block_coalescing(TestContext *ctx)
{
    RDPQ_INIT();

    // The actual commands don't matter because they are never executed
    rspq_block_begin();

    // These 3 commands are supposed to go to the static RDP buffer, and
    // the 3 RSPQ_CMD_RDP commands will be coalesced into one 
    rdpq_set_env_color(RGBA32(0,0,0,0));
    rdpq_set_blend_color(RGBA32(0, 0, 0, 0));
    rdpq_set_tile(TILE0, FMT_RGBA16, 0, 16, 0);

    // This command is a fixup
    rdpq_set_fill_color(RGBA16(0, 0, 0, 0));

    // These 3 should also have their RSPQ_CMD_RDP coalesced
    rdpq_set_env_color(RGBA32(0,0,0,0));
    rdpq_set_blend_color(RGBA32(0, 0, 0, 0));
    rdpq_set_tile(TILE0, FMT_RGBA16, 0, 16, 0);

    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    uint64_t *rdp_cmds = (uint64_t*)block->rdp_block->cmds;

    uint32_t expected_cmds[] = {
        // auto sync + First 3 commands + auto sync
        (RSPQ_CMD_RDP_SET_BUFFER << 24) | PhysicalAddr(rdp_cmds + 5), 
            PhysicalAddr(rdp_cmds), 
            PhysicalAddr(rdp_cmds + RDPQ_BLOCK_MIN_SIZE/2),
        // Fixup command (leaves a hole in rdp block)
        (RDPQ_CMD_SET_FILL_COLOR_32 + 0xC0) << 24, 
            0,
        // Last 3 commands
        (RSPQ_CMD_RDP_APPEND_BUFFER << 24) | PhysicalAddr(rdp_cmds + 9),
    };

    ASSERT_EQUAL_MEM((uint8_t*)block->cmds, (uint8_t*)expected_cmds, sizeof(expected_cmds), "Block commands don't match!");
}

void test_rdpq_block_contiguous(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 64;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rspq_block_begin();
    /* 1: implicit sync pipe */
    /* 2: */ rdpq_set_color_image(&fb);
    /* 3: implicit set fill color */ 
    /* 4: implicit set scissor */
    /* 5: */ rdpq_set_mode_fill(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    /* 6: implicit set scissor */
    /* 7: empty slot for potential SET_COMBINE_MODE (not used by rdpq_set_mode_fill) */
    /* 8: set fill color */
    /* 9: */ rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    /*10: */ rdpq_fence(); // Put the fence inside the block so RDP never executes anything outside the block
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rspq_block_run(block);
    rspq_syncpoint_wait(rspq_syncpoint_new());

    uint64_t *rdp_cmds = (uint64_t*)block->rdp_block->cmds;

    ASSERT_EQUAL_HEX(*DP_START, PhysicalAddr(rdp_cmds), "DP_START does not point to the beginning of the block!");
    ASSERT_EQUAL_HEX(*DP_END, PhysicalAddr(rdp_cmds + 10), "DP_END points to the wrong address!");

    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, "Framebuffer contains wrong data!");
}

void test_rdpq_block_dynamic(TestContext *ctx)
{
    RDPQ_INIT();
    debug_rdp_stream_init();

    test_ovl_init();
    DEFER(test_ovl_close());

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);

    surface_clear(&fb, 0);
    rdpq_set_mode_standard();

    void test_with_nops(int nops_to_generate) {
        debug_rdp_stream_reset();

        rspq_block_begin();
            // First, issue a passthrough command
            rdpq_set_fog_color(RGBA32(0x11,0x11,0x11,0x11));
            // Then, issue a command that creates large dynamic commands
            // We use a test command that creates 8 RDP NOPs.
            rspq_test_send_rdp_nops(nops_to_generate);
            // Issue another passhtrough
            rdpq_set_blend_color(RGBA32(0x22,0x22,0x22,0x22));
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));

        rspq_block_run(block);
        rdpq_set_blend_color(RGBA32(0x33,0x33,0x33,0x33));
        rspq_wait();

        int num_fc = debug_rdp_stream_count_cmd(0xF8); // SET_FOG_COLOR
        int num_bc = debug_rdp_stream_count_cmd(0xF9); // SET_BLEND_COLOR
        int num_nops = debug_rdp_stream_count_cmd(0xC0); // NOOP
        ASSERT_EQUAL_SIGNED(num_fc, 1, "invalid number of SET_FOG_COLOR");
        ASSERT_EQUAL_SIGNED(num_bc, 2, "invalid number of SET_BLEND_COLOR");
        ASSERT_EQUAL_SIGNED(num_nops, nops_to_generate, "invalid number of NOP");

        // Check that all the nops come after fog and before blend
        bool found_fog = false;
        bool found_blend = false;
        for (int i=0;i<rdp_stream_ctx.idx;i++) {
            if ((rdp_stream[i] >> 56) == 0xF8) { found_fog = true; continue; }
            if ((rdp_stream[i] >> 56) == 0xF9) { found_blend = true; continue; }
            if ((rdp_stream[i] >> 56) == 0xC0) {
                ASSERT(found_fog && !found_blend, "Invalid position of NOP within the stream");
            }
        }

        // Also test that there is just one static RDP block in the block. This
        // verifies that, in case we switched to the dynamic buffer for the blocks,
        // we correctly reused the block later.
        int num_rdp_blocks = 0;
        rdpq_block_t *rdp_block = block->rdp_block;
        while (rdp_block) {
            ++num_rdp_blocks;
            rdp_block = rdp_block->next;
        }
        ASSERT_EQUAL_SIGNED(num_rdp_blocks, 1, "invalid number of RDP static blocks");
    }

    // Test with a small number of nops: 
    rdpq_debug_log_msg("test 8");
    test_with_nops(8);
    if (ctx->result == TEST_FAILED) return;

    rdpq_debug_log_msg("test 128");
    test_with_nops(128);
    if (ctx->result == TEST_FAILED) return;
}

void test_rdpq_block_nested(TestContext *ctx)
{
    RDPQ_INIT();
    debug_rdp_stream_init();

    test_ovl_init();
    DEFER(test_ovl_close());

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);

    surface_clear(&fb, 0);
    rdpq_set_mode_standard();
    rspq_wait();
    debug_rdp_stream_reset();

    rspq_block_begin();
        rdpq_set_blend_color(RGBA32(0x22,0x22,0x22,0x22));
        rdpq_set_prim_color(RGBA32(0x11,0x11,0x11,0x11));
    rspq_block_t *block1 = rspq_block_end();

    rspq_block_begin();
        rdpq_set_fog_color(RGBA32(0x33,0x33,0x33,0x33));
        rspq_block_run(block1);
        rdpq_set_env_color(RGBA32(0x44,0x44,0x44,0x44));
    rspq_block_t *block2 = rspq_block_end();

    rspq_block_run(block2);
    rspq_wait();
    
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        LOG("%016llx\n", rdp_stream[i]);
    }
    
    int num_fc = debug_rdp_stream_count_cmd(0xF8); // SET_FOG_COLOR
    int num_bc = debug_rdp_stream_count_cmd(0xF9); // SET_BLEND_COLOR
    int num_pc = debug_rdp_stream_count_cmd(0xFA); // SET_PRIMITIVE_COLOR
    int num_ec = debug_rdp_stream_count_cmd(0xFB); // SET_ENV_COLOR
    ASSERT_EQUAL_SIGNED(num_fc, 1, "invalid number of SET_FOG_COLOR");
    ASSERT_EQUAL_SIGNED(num_bc, 1, "invalid number of SET_BLEND_COLOR");
    ASSERT_EQUAL_SIGNED(num_pc, 1, "invalid number of SET_PRIMITIVE_COLOR");
    ASSERT_EQUAL_SIGNED(num_ec, 1, "invalid number of SET_ENV_COLOR");

    // Now check that they appear in the right order
    //   rdp_stream[0] = SYNC
    ASSERT_EQUAL_HEX(rdp_stream[1]>>56, 0xF8, "SET_FOG_COLOR not in position 0");
    //   rdp_stream[2] = SYNC
    ASSERT_EQUAL_HEX(rdp_stream[3]>>56, 0xF9, "SET_BLEND_COLOR not in position 1");
    ASSERT_EQUAL_HEX(rdp_stream[4]>>56, 0xFA, "SET_PRIM_COLOR not in position 2");
    ASSERT_EQUAL_HEX(rdp_stream[5]>>56, 0xFB, "SET_ENV_COLOR not in position 3");
}

void test_rdpq_change_other_modes(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);

    // Set standard mode with a combiner that doesn't use a fixed color
    surface_clear(&fb, 0);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);

    // Switch to fill mode via change other modes, and fill the framebuffer
    rdpq_debug_log_msg("try SOM change (dynamic)");
    rdpq_change_other_modes_raw(SOM_CYCLE_MASK, SOM_CYCLE_FILL);
    rdpq_set_fill_color(RGBA32(255,0,0,255));
    rdpq_fill_rectangle(0,0,WIDTH,WIDTH);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,0,0,255); });

    // Do it again in a block
    surface_clear(&fb, 0);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);

    rspq_block_begin();
        rdpq_debug_log_msg("try SOM change (block)");
        rdpq_change_other_modes_raw(SOM_CYCLE_MASK, SOM_CYCLE_FILL);
        rdpq_set_fill_color(RGBA32(255,0,0,255));
        rdpq_fill_rectangle(0,0,WIDTH,WIDTH);
    rspq_block_t *b = rspq_block_end();
    DEFER(rspq_block_free(b));

    rspq_block_run(b);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,0,0,255); });
}


void test_rdpq_fixup_setfillcolor(TestContext *ctx)
{
    RDPQ_INIT();

    const color_t TEST_COLOR = RGBA32(0xAA,0xBB,0xCC,0xDD);

    const int WIDTH = 64;
    surface_t fb = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb));

    uint32_t expected_fb32[WIDTH*WIDTH];
    memset(expected_fb32, 0, sizeof(expected_fb32));
    for (int i=0;i<WIDTH*WIDTH;i++)
        expected_fb32[i] = (TEST_COLOR.r << 24) | (TEST_COLOR.g << 16) | (TEST_COLOR.b << 8) | (TEST_COLOR.a);

    uint16_t expected_fb16[WIDTH*WIDTH];
    memset(expected_fb16, 0, sizeof(expected_fb16));
    for (int i=0;i<WIDTH*WIDTH;i++) {
        int r = TEST_COLOR.r >> 3;
        int g = TEST_COLOR.g >> 3;
        int b = TEST_COLOR.b >> 3;        
        expected_fb16[i] = ((r & 0x1F) << 11) | ((g & 0x1F) << 6) | ((b & 0x1F) << 1) | (TEST_COLOR.a >> 7);
    }

    rdpq_set_mode_fill(RGBA32(0,0,0,0));

    surface_clear(&fb, 0);
    rdpq_set_color_image(&fb);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb32, WIDTH*WIDTH*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode)");

    surface_clear(&fb, 0);
    rdpq_set_color_image_raw(0, PhysicalAddr(fb.buffer), FMT_RGBA16, WIDTH, WIDTH, WIDTH*2);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb16, WIDTH*WIDTH*2, 
        "Wrong data in framebuffer (16-bit, dynamic mode)");

    surface_clear(&fb, 0);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_color_image(&fb);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb32, WIDTH*WIDTH*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode, update)");

    surface_clear(&fb, 0);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_color_image_raw(0, PhysicalAddr(fb.buffer), FMT_RGBA16, WIDTH, WIDTH, WIDTH*2);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb16, WIDTH*WIDTH*2, 
        "Wrong data in framebuffer (16-bit, dynamic mode, update)");
}

void test_rdpq_fixup_setscissor(TestContext *ctx)
{
    RDPQ_INIT();

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0, sizeof(expected_fb));
    for (int y=4;y<WIDTH-4;y++) {
        for (int x=4;x<WIDTH-4;x++) {
            expected_fb[y * WIDTH + x] = color_to_packed16(TEST_COLOR);
        }
    }

    rdpq_set_color_image(&fb);

    rdpq_debug_log_msg("Fill mode");
    surface_clear(&fb, 0);
    rdpq_set_mode_fill(TEST_COLOR);
    rdpq_set_scissor(4, 4, WIDTH-4, WIDTH-4);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
        "Wrong data in framebuffer (fill mode)");

    rdpq_debug_log_msg("1-cycle mode");
    surface_clear(&fb, 0);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO,ZERO,ZERO,ZERO),(ZERO,ZERO,ZERO,ONE)));
    rdpq_mode_blender(RDPQ_BLENDER((BLEND_RGB, IN_ALPHA, IN_RGB, INV_MUX_ALPHA)));
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, WIDTH-4, WIDTH-4);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
        "Wrong data in framebuffer (1 cycle mode)");

    rdpq_debug_log_msg("Fill mode (update)");
    surface_clear(&fb, 0);
    rdpq_set_scissor(4, 4, WIDTH-4, WIDTH-4);
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
        "Wrong data in framebuffer (fill mode, update)");

    rdpq_debug_log_msg("1-cycle mode (update)");
    surface_clear(&fb, 0);
    rdpq_set_scissor(4, 4, WIDTH-4, WIDTH-4);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO,ZERO,ZERO,ZERO),(ZERO,ZERO,ZERO,ONE)));
    rdpq_mode_blender(RDPQ_BLENDER((BLEND_RGB, IN_ALPHA, IN_RGB, INV_MUX_ALPHA)));
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
        "Wrong data in framebuffer (1 cycle mode, update)");
}

void test_rdpq_fixup_texturerect(TestContext *ctx)
{
    RDPQ_INIT();

    const int FBWIDTH = 16;
    const int TEXWIDTH = FBWIDTH - 8;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    surface_t tex = surface_alloc(FMT_RGBA16, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));
    surface_clear(&tex, 0);

    uint16_t expected_fb[FBWIDTH*FBWIDTH];
    memset(expected_fb, 0xFF, sizeof(expected_fb));
    for (int y=0;y<TEXWIDTH;y++) {
        for (int x=0;x<TEXWIDTH;x++) {
            color_t c = RGBA16(x, y, x+y, 1);
            expected_fb[(y + 4) * FBWIDTH + (x + 4)] = color_to_packed16(c);
            ((uint16_t*)tex.buffer)[y * TEXWIDTH + x] = color_to_packed16(c);
        }
    }

    rdpq_set_color_image(&fb);
    rdpq_set_texture_image(&tex);
    rdpq_set_tile(0, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_load_tile(0, 0, 0, TEXWIDTH, TEXWIDTH);

    surface_clear(&fb, 0xFF);
    rdpq_set_mode_copy(false);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (copy mode, dynamic mode)");

    surface_clear(&fb, 0xFF);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (1cycle mode, dynamic mode)");

    {
        surface_clear(&fb, 0xFF);
        rspq_block_begin();
        rdpq_set_other_modes_raw(SOM_CYCLE_COPY);
        rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
            "Wrong data in framebuffer (copy mode, static mode)");
    }

    {
        surface_clear(&fb, 0xFF);
        rspq_block_begin();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
        rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
            "Wrong data in framebuffer (1cycle mode, static mode)");
    }

    {
        surface_clear(&fb, 0xFF);
        rdpq_set_other_modes_raw(SOM_CYCLE_COPY);
        rspq_block_begin();
        rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
            "Wrong data in framebuffer (copy mode, static mode, no tracking)");
    }

    {
        surface_clear(&fb, 0xFF);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
        rspq_block_begin();
        rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
            "Wrong data in framebuffer (1cycle mode, static mode, no tracking)");
    }

    #undef TEST_RDPQ_TEXWIDTH
    #undef TEST_RDPQ_TEXAREA
    #undef TEST_RDPQ_TEXSIZE
}

void test_rdpq_fixup_fillrect(TestContext *ctx)
{
    RDPQ_INIT();

    const int FULL_CVG = 7 << 5;
    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);
    rdpq_set_color_image(&fb);

    rdpq_debug_log_msg("rect mode fill");
    rdpq_set_mode_fill(RGBA32(255,0,255,0));
    rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
    rspq_wait();
    ASSERT_SURFACE(&fb, {
        return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
            RGBA32(255,0,255,0) : RGBA32(0,0,0,0);
    });

    rdpq_debug_log_msg("rect mode standard");
    surface_clear(&fb, 0);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rdpq_set_prim_color(RGBA32(255,128,255,0));
    rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
    rspq_wait();
    ASSERT_SURFACE(&fb, {
        return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
            RGBA32(255,128,255,FULL_CVG) : RGBA32(0,0,0,0);
    });

    {
        rdpq_debug_log_msg("rect mode fill (block)");
        surface_clear(&fb, 0);
        rspq_block_begin();
            rdpq_set_mode_fill(RGBA32(255,0,255,0));
            rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_SURFACE(&fb, {
            return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
                RGBA32(255,0,255,0) : RGBA32(0,0,0,0);
        });
    }

    {
        rdpq_debug_log_msg("rect mode standard (block)");
        surface_clear(&fb, 0);
        rspq_block_begin();
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_set_prim_color(RGBA32(255,128,255,0));
            rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_SURFACE(&fb, {
            return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
                RGBA32(255,128,255,FULL_CVG) : RGBA32(0,0,0,0);
        });
    }

    {
        rdpq_debug_log_msg("only rect in block, mode fill");
        surface_clear(&fb, 0);
        rdpq_set_mode_fill(RGBA32(255,0,255,0));
        rspq_block_begin();
            rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_SURFACE(&fb, {
            return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
                RGBA32(255,0,255,0) : RGBA32(0,0,0,0);
        });
    }

    {
        rdpq_debug_log_msg("only rect in block, mode standard");
        surface_clear(&fb, 0);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_set_prim_color(RGBA32(255,128,255,0));
        rspq_block_begin();
            rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_SURFACE(&fb, {
            return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
                RGBA32(255,128,255,FULL_CVG) : RGBA32(0,0,0,0);
        });
    }
}

void test_rdpq_lookup_address(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rdpq_set_mode_fill(TEST_COLOR);

    surface_clear(&fb, 0);
    rspq_block_begin();
    rdpq_set_color_image_raw(1, 0, FMT_RGBA16, WIDTH, WIDTH, WIDTH * 2);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));
    rdpq_set_lookup_address(1, fb.buffer);
    rspq_block_run(block);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
            "Wrong data in framebuffer (static mode)");

    surface_clear(&fb, 0);
    rdpq_set_lookup_address(1, fb.buffer);
    rdpq_set_color_image_raw(1, 0, FMT_RGBA16, WIDTH, WIDTH, WIDTH * 2);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
            "Wrong data in framebuffer (dynamic mode)");
}

void test_rdpq_lookup_address_offset(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    #define TEST_RDPQ_RECT_OFF   4
    #define TEST_RDPQ_RECT_WIDTH (WIDTH-(TEST_RDPQ_RECT_OFF*2))

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    uint16_t expected_fb[WIDTH*WIDTH];
    memset(expected_fb, 0, sizeof(expected_fb));
    for (int y=TEST_RDPQ_RECT_OFF;y<WIDTH-TEST_RDPQ_RECT_OFF;y++) {
        for (int x=TEST_RDPQ_RECT_OFF;x<WIDTH-TEST_RDPQ_RECT_OFF;x++) {
            expected_fb[y * WIDTH + x] = color_to_packed16(TEST_COLOR);
        }
    }    

    rdpq_set_mode_fill(TEST_COLOR);

    uint32_t offset = (TEST_RDPQ_RECT_OFF * WIDTH + TEST_RDPQ_RECT_OFF) * 2;

    surface_clear(&fb, 0);
    rspq_block_begin();
    rdpq_set_color_image_raw(1, offset, FMT_RGBA16, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_RECT_WIDTH, WIDTH * 2);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_RECT_WIDTH);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));
    rdpq_set_lookup_address(1, fb.buffer);
    rspq_block_run(block);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
            "Wrong data in framebuffer (static mode)");

    surface_clear(&fb, 0);
    rdpq_set_lookup_address(1, fb.buffer);
    rdpq_set_color_image_raw(1, offset, FMT_RGBA16, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_RECT_WIDTH, WIDTH * 2);
    rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, 
            "Wrong data in framebuffer (dynamic mode)");

    #undef TEST_RDPQ_RECT_OFF
    #undef TEST_RDPQ_RECT_WIDTH
}

void test_rdpq_syncfull_cb(TestContext *ctx)
{
    RDPQ_INIT();

    volatile int cb_called = 0;
    volatile uint32_t cb_value = 0;
    void cb1(void *arg1) {
        cb_called += 1;
        cb_value = (uint32_t)arg1 & 0x0000FFFF;
    }
    void cb2(void *arg1) {
        cb_called += 2;
        cb_value = (uint32_t)arg1 & 0xFFFF0000;
    }

    rdpq_sync_full(cb1, (void*)0x12345678);
    rdpq_sync_full(cb2, (void*)0xABCDEF01);
    rspq_wait();

    ASSERT_EQUAL_SIGNED(cb_called, 3, "sync full callback not called");
    ASSERT_EQUAL_HEX(cb_value, 0xABCD0000, "sync full callback wrong argument");

    rspq_block_begin();
    rdpq_sync_full(cb2, (void*)0xABCDEF01);
    rdpq_sync_full(cb1, (void*)0x12345678);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rspq_block_run(block);
    rspq_wait();

    ASSERT_EQUAL_SIGNED(cb_called, 6, "sync full callback not called");
    ASSERT_EQUAL_HEX(cb_value, 0x00005678, "sync full callback wrong argument");
}

void test_rdpq_syncfull_resume(TestContext *ctx)
{
    // Avoid rdpq_debug_start since it can mess with timing
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    // SYNC_FULL has a hardware bug in case other commands get scheduled via DMA while
    // it is in progress. rdpq works around but we want to test that it works in several
    // situations.
    // This test has no checks because if it fails, the RDP will hang and the RSP crash
    // screen will appear.

    const int WIDTH = 32;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_t tex = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&tex));

    rdpq_set_mode_copy(false);
    rdpq_set_color_image(&fb);

    // Dynamic mode 
    LOG("Dynamic mode\n");
    for (int j=0;j<4;j++) {
        for (int i=0;i<80;i++) {
            rdpq_tex_upload_sub(TILE0, &tex, NULL, 0, 0, WIDTH, WIDTH);
            rdpq_texture_rectangle(TILE0, 0, 0, WIDTH, WIDTH, 0, 0);
        }
        rdpq_sync_full(NULL, NULL);
    }
    rspq_wait();

    // Dynamic mode (multiple syncs per buffer)
    LOG("Dynamic mode with multiple syncs per buffer\n");
    for (int j=0;j<4;j++) {
        for (int i=0;i<6;i++) {
            rdpq_tex_upload_sub(TILE0, &tex, NULL, 0, 0, WIDTH, WIDTH);
            rdpq_texture_rectangle(TILE0, 0, 0, WIDTH, WIDTH, 0, 0);
        }
        rdpq_sync_full(NULL, NULL);
    }
    rspq_wait();

    uint64_t buf[1] = { 0x2700000000000000ull };
    data_cache_index_writeback_invalidate(buf, sizeof(buf));

    // Dynamic mode, forcing buffer change.
    LOG("Dynamic mode with buffer change\n");
    for (int j=0;j<4;j++) {
        for (int i=0;i<80;i++) {
            rdpq_tex_upload_sub(TILE0, &tex, NULL, 0, 0, WIDTH, WIDTH);
            rdpq_texture_rectangle(TILE0, 0, 0, WIDTH, WIDTH, 0, 0);
        }
        rdpq_sync_full(NULL, NULL);
        rdpq_exec(buf, sizeof(buf));
    }
    rspq_wait();

    // Block mode, 
    LOG("Block mode\n");
    rspq_block_begin();
    for (int i=0;i<80;i++) {
        rdpq_tex_upload_sub(TILE0, &tex, NULL, 0, 0, WIDTH, WIDTH);
        rdpq_texture_rectangle(TILE0, 0, 0, WIDTH, WIDTH, 0, 0);
    }
    rspq_block_t *rect_block = rspq_block_end();
    DEFER(rspq_block_free(rect_block));

    for (int j=0;j<4;j++) {
        for (int i=0;i<4;i++)
            rspq_block_run(rect_block);
        rdpq_sync_full(NULL, NULL);
    }
    rspq_wait();

    // Block mode with sync, 
    LOG("Block mode with sync inside\n");
    rspq_block_begin();
    for (int i=0;i<80;i++) {
        rdpq_tex_upload_sub(TILE0, &tex, NULL, 0, 0, WIDTH, WIDTH);
        rdpq_texture_rectangle(TILE0, 0, 0, WIDTH, WIDTH, 0, 0);
    }
    rdpq_sync_full(NULL, NULL);
    rspq_block_t *sync_block = rspq_block_end();
    DEFER(rspq_block_free(sync_block));

    for (int j=0;j<4;j++) {
        rspq_block_run(sync_block);
    }
    rspq_wait();
}

static void __test_rdpq_autosyncs(TestContext *ctx, void (*func)(void), uint8_t exp[4], bool use_block) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int WIDTH = 64;
    surface_t fb = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    rspq_block_t *block = NULL;
    DEFER(if (block) rspq_block_free(block));
    
    rdpq_set_mode_standard();
    rdpq_set_color_image(&fb);

    if (use_block) {
        rspq_block_begin();
        func();
        block = rspq_block_end();
        ASSERT(block->rdp_block, "rdpq block is empty?");
        rspq_block_run(block);
    }

    // Execute the provided function (also after the block, if requested).
    // This allows us also to get coverage of the post-block autosync state    
    func();
    rspq_wait();

    // Go through the stream of RDP commands and count the syncs
    uint8_t cnt[4] = {0};
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        uint8_t cmd = rdp_stream[i] >> 56;
        if (cmd == RDPQ_CMD_SYNC_LOAD+0xC0) cnt[0]++;
        if (cmd == RDPQ_CMD_SYNC_TILE+0xC0) cnt[1]++;
        if (cmd == RDPQ_CMD_SYNC_PIPE+0xC0) cnt[2]++;
        if (cmd == RDPQ_CMD_SYNC_FULL+0xC0) cnt[3]++;
    }
    ASSERT_EQUAL_MEM(cnt, exp, 4, "Unexpected sync commands");
}

static void __autosync_pipe1(void) {
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL);
    rdpq_set_fill_color(RGBA32(0,0,0,0));
    rdpq_fill_rectangle(0, 0, 8, 8);
    // PIPESYNC HERE
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL);
    rdpq_fill_rectangle(0, 0, 8, 8);
    // NO PIPESYNC HERE
    rdpq_set_prim_color(RGBA32(1,1,1,1));
    // NO PIPESYNC HERE
    rdpq_set_prim_depth_raw(0, 1);
    // NO PIPESYNC HERE
    rdpq_set_scissor(0,0,1,1);
    rdpq_fill_rectangle(0, 0, 8, 8);
}
static uint8_t __autosync_pipe1_exp[4] = {0,0,1,1};
static uint8_t __autosync_pipe1_blockexp[4] = {0,0,4,1};

static void __autosync_tile1(void) {
    rdpq_set_tile(0, FMT_RGBA16, 0, 128, 0);
    rdpq_set_tile_size(0, 0, 0, 16, 16);
    rdpq_texture_rectangle(0, 0, 0, 4, 4, 0, 0);    
    // NO TILESYNC HERE
    rdpq_set_tile(1, FMT_RGBA16, 0, 128, 0);
    rdpq_set_tile_size(1, 0, 0, 16, 16);
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0);    
    rdpq_set_tile(2, FMT_RGBA16, 0, 128, 0);
    rdpq_set_tile_size(2, 0, 0, 16, 16);
    // NO TILESYNC HERE
    rdpq_set_tile(2, FMT_RGBA16, 0, 256, 0);
    // NO TILESYNC HERE
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0);    
    rdpq_texture_rectangle(0, 0, 0, 4, 4, 0, 0);    
    // TILESYNC HERE
    rdpq_set_tile(1, FMT_RGBA16, 0, 256, 0);
    rdpq_set_tile_size(1, 0, 0, 16, 16);
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0);    
    // TILESYNC HERE
    rdpq_set_tile_size(1, 0, 0, 32, 32);

}
static uint8_t __autosync_tile1_exp[4] = {0,2,0,1};
static uint8_t __autosync_tile1_blockexp[4] = {0,5,0,1};

static void __autosync_load1(void) {
    surface_t tex = surface_alloc(FMT_I8, 8, 8);
    DEFER(surface_free(&tex));

    rdpq_set_texture_image(&tex);
    rdpq_set_tile(0, FMT_RGBA16, 0, 128, 0);
    // NO LOADSYNC HERE
    rdpq_load_tile(0, 0, 0, 7, 7);
    rdpq_set_tile(1, FMT_RGBA16, 0, 128, 0);
    // NO LOADSYNC HERE
    rdpq_load_tile(1, 0, 0, 7, 7);
    // NO LOADSYNC HERE
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0);
    // LOADSYNC HERE
    rdpq_load_tile(0, 0, 0, 7, 7);
}
static uint8_t __autosync_load1_exp[4] = {1,1,0,1};
static uint8_t __autosync_load1_blockexp[4] = {3,4,2,1};

void test_rdpq_autosync(TestContext *ctx) {
    LOG("__autosync_pipe1\n");
    __test_rdpq_autosyncs(ctx, __autosync_pipe1, __autosync_pipe1_exp, false);
    if (ctx->result == TEST_FAILED) return;

    LOG("__autosync_pipe1 (block)\n");
    __test_rdpq_autosyncs(ctx, __autosync_pipe1, __autosync_pipe1_blockexp, true);
    if (ctx->result == TEST_FAILED) return;

    LOG("__autosync_tile1\n");
    __test_rdpq_autosyncs(ctx, __autosync_tile1, __autosync_tile1_exp, false);
    if (ctx->result == TEST_FAILED) return;

    LOG("__autosync_tile1 (block)\n");
    __test_rdpq_autosyncs(ctx, __autosync_tile1, __autosync_tile1_blockexp, true);
    if (ctx->result == TEST_FAILED) return;

    LOG("__autosync_load1\n");
    __test_rdpq_autosyncs(ctx, __autosync_load1, __autosync_load1_exp, false);
    if (ctx->result == TEST_FAILED) return;

    LOG("__autosync_load1 (block)\n");
    __test_rdpq_autosyncs(ctx, __autosync_load1, __autosync_load1_blockexp, true);
    if (ctx->result == TEST_FAILED) return;
}


void test_rdpq_automode(TestContext *ctx) {
    RDPQ_INIT();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    const int TEXWIDTH = FBWIDTH - 8;
    surface_t tex = surface_alloc(FMT_RGBA16, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));
    surface_clear(&tex, 0);

    uint16_t expected_fb[FBWIDTH*FBWIDTH];
    memset(expected_fb, 0xFF, sizeof(expected_fb));
    for (int y=0;y<TEXWIDTH;y++) {
        for (int x=0;x<TEXWIDTH;x++) {
            color_t c = RGBA16(RANDN(32), RANDN(32), RANDN(32), 1);
            expected_fb[(y + 4) * FBWIDTH + (x + 4)] = color_to_packed16(c);
            ((uint16_t*)tex.buffer)[y * TEXWIDTH + x] = color_to_packed16(c);
        }
    }

    uint64_t som;
    rdpq_set_color_image(&fb);
    rdpq_set_texture_image(&tex);
    rdpq_set_tile(0, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_set_tile(1, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_load_tile(0, 0, 0, TEXWIDTH, TEXWIDTH);
    rdpq_load_tile(1, 0, 0, TEXWIDTH, TEXWIDTH);
    rdpq_set_mode_standard();
    rdpq_set_blend_color(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    rdpq_set_fog_color(RGBA32(0xEE, 0xEE, 0xEE, 0xFF));
    rdpq_set_env_color(RGBA32(0x0,0x0,0x0,0x7F));
    rdpq_set_prim_color(RGBA32(0x0,0x0,0x0,0x7F));

    // Set simple 1-pass combiner => 1 cycle
    rdpq_debug_log_msg("1pass combiner => 1 cycle");
    surface_clear(&fb, 0xFF);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=off)");

    // Activate blending (1-pass blender) => 1 cycle
    rdpq_debug_log_msg("1pass blender => 1 cycle");
    surface_clear(&fb, 0xFF);
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, FOG_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass)");

    // Activate fogging (2-pass blender) => 2 cycle
    rdpq_debug_log_msg("2pass blender => 2 cycle");
    surface_clear(&fb, 0xFF);
    rdpq_mode_fog(RDPQ_BLENDER((BLEND_RGB, ZERO, IN_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=2pass)");

    // Set two-pass combiner => 2 cycle
    rdpq_debug_log_msg("2pass combiner => 2 cycle");
    surface_clear(&fb, 0xFF);
    rdpq_mode_combiner(RDPQ_COMBINER2(
        (ZERO, ZERO, ZERO, ENV), (ENV, ZERO, TEX0, PRIM),
        (TEX1, ZERO, COMBINED_ALPHA, ZERO), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();    
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=2pass, blender=2pass)");

    // Disable fogging (1 pass blender) => 2 cycle
    rdpq_debug_log_msg("1pass blender => 2 cycle");
    surface_clear(&fb, 0xFF);
    rdpq_mode_fog(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_HEX(som & 0xCCCC0000, 0, "invalid blender formula in first cycle");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=2pass, blender=1pass)");

    // Set simple combiner => 1 cycle
    rdpq_debug_log_msg("1pass combiner => 1 cycle");
    surface_clear(&fb, 0xFF);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass)");

    // Push the current mode, then modify several states, then pop.
    rdpq_debug_log_msg("push/pop");
    rdpq_mode_push();
    rdpq_mode_combiner(RDPQ_COMBINER2(
        (ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO),
        (COMBINED, ZERO, ZERO, TEX1), (ZERO, ZERO, ZERO, ZERO)
    ));
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, ZERO, BLEND_RGB, ONE)));
    rdpq_mode_dithering(DITHER_NOISE_NOISE);
    rdpq_mode_pop();
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass (after pop))");
}

void test_rdpq_blender(TestContext *ctx) {
    RDPQ_INIT();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    const int TEXWIDTH = FBWIDTH;
    surface_t tex = surface_alloc(FMT_RGBA16, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));
    surface_clear(&tex, 0xAA);

    const color_t BLEND_COLOR = RGBA32(0x30, 0x30, 0x30, 0xFF);
    const color_t BLEND_COLOR2 = RGBA32(0x30*2-1, 0x30*2-1, 0x30*2-1, 0xFF);

    uint16_t expected_fb_blend[FBWIDTH*FBWIDTH], expected_fb_blend2[FBWIDTH*FBWIDTH], expected_fb_tex[FBWIDTH*FBWIDTH];
    memset(expected_fb_blend, 0, sizeof(expected_fb_blend));
    memset(expected_fb_blend2, 0, sizeof(expected_fb_blend2));
    memset(expected_fb_tex, 0, sizeof(expected_fb_tex));
    for (int y=4;y<FBWIDTH-4;y++) {
        for (int x=4;x<FBWIDTH-4;x++) {
            expected_fb_blend[y * FBWIDTH + x] = color_to_packed16(BLEND_COLOR);
            expected_fb_blend2[y * FBWIDTH + x] = color_to_packed16(BLEND_COLOR2);
            expected_fb_tex[y * FBWIDTH + x] = 0xAAAA | 1;
        }
    }

    rdpq_set_color_image(&fb);
    rdpq_set_texture_image(&tex);
    rdpq_set_tile(0, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_set_tile(1, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_load_tile(0, 0, 0, TEXWIDTH, TEXWIDTH);
    rdpq_load_tile(1, 0, 0, TEXWIDTH, TEXWIDTH);

    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
    rdpq_set_blend_color(BLEND_COLOR);
    rdpq_set_fog_color(RGBA32(0xEE, 0xEE, 0xEE, 0xFF));

    // Enable blending
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, ZERO, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass1)");

    // Disable blending
    rdpq_mode_blender(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_tex, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=none)");

    // Enable fogging
    rdpq_mode_fog(RDPQ_BLENDER((IN_RGB, ZERO, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass0)");

    // Disable fogging
    rdpq_mode_fog(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_tex, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=none)");

    // Enable two-pass bleder
    rdpq_mode_blender(RDPQ_BLENDER2(
        (IN_RGB, 0, BLEND_RGB, INV_MUX_ALPHA),
        (CYCLE1_RGB, FOG_ALPHA, BLEND_RGB, 1)
    ));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend2, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass0+1)");

    // Disable blend
    rdpq_mode_blender(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_tex, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass0)");
}

void test_rdpq_blender_memory(TestContext *ctx) {
    RDPQ_INIT();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));

    uint32_t expected_fb[FBWIDTH*FBWIDTH];

    for (int j=0;j<FBWIDTH;j++) {
        for (int i=0;i<FBWIDTH;i++) {
            bool alt = (i % (j/2+1) < 3);
            ((uint32_t*)fb.buffer)[j * FBWIDTH + i] = alt ? 0xB0B0B080 : 0x30303080;
            if (i >= 4 && i < 12 && j >= 4 && j <12)
                expected_fb[j * FBWIDTH + i] = alt ? 0x989898e0 : 0x585858e0;
            else
                expected_fb[j * FBWIDTH + i] = alt ? 0xB0B0B080 : 0x30303080;
        }
    }

    const int TEXWIDTH = 8;
    surface_t tex = surface_alloc(FMT_RGBA32, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));
    surface_clear(&tex, 0x80);

    rdpq_set_fog_color(RGBA32(0,0,0,0x80));
    rdpq_set_color_image(&fb);
    rdpq_tex_upload(TILE0, &tex, NULL);
    rdpq_set_mode_standard();
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rdpq_triangle(&TRIFMT_TEX,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 12.0f,  4.0f, 8.0f, 0.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rdpq_triangle(&TRIFMT_TEX,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 4.0f,  12.0f, 0.0f, 8.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*4, "Wrong data in framebuffer");
    uint64_t som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
}

void test_rdpq_fog(TestContext *ctx) {
    RDPQ_INIT();

    const int FULL_CVG = 7 << 5;   // full coverage
    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);
    rdpq_set_fog_color(RGBA32(0,255,0,255));
    rdpq_set_blend_color(RGBA32(0,0,255,255));
    surface_clear(&fb, 0);

    // Draw with standard texturing
    rdpq_debug_log_msg("Standard combiner SHADE - no fog");
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rdpq_triangle(&TRIFMT_SHADE,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH,       0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, }
    );
    rdpq_triangle(&TRIFMT_SHADE,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ 0,       FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, }
    );
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,0,255,FULL_CVG); });

    // Activate fog
    rdpq_debug_log_msg("Standard combiner SHADE - fog");
    rdpq_mode_fog(RDPQ_FOG_STANDARD);
    // Set also a blender that uses IN_ALPHA.
    // This has two effects: it tests the whole pipeline after switching to
    // 2cycle mode, and then also checks that IN_ALPHA is 1, which is what
    // we expect for COMBINER_SHADE when fog is in effect.
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_triangle(&TRIFMT_SHADE,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH,       0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, }
    );
    rdpq_triangle(&TRIFMT_SHADE,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ 0,       FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, }
    );
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(0x77,0x87,0x77,FULL_CVG); });

    // Draw with a custom combiner
    rdpq_debug_log_msg("Custom combiner - no fog");
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((1,0,PRIM,0), (1,0,PRIM,0)));
    rdpq_set_prim_color(RGBA32(255,0,0,255));
    rdpq_fill_rectangle(0, 0, FBWIDTH, FBWIDTH);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,0,0,FULL_CVG); });

    // Activate fog
    rdpq_debug_log_msg("Custom combiner - fog");
    rdpq_mode_fog(RDPQ_FOG_STANDARD);
    rdpq_triangle(&TRIFMT_SHADE,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 1.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH,       0, 1.0f, 1.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 1.0f, 1.0f, 0.5f, }
    );
    rdpq_triangle(&TRIFMT_SHADE,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 1.0f, 1.0f, 0.5f, },
        (float[]){ 0,       FBWIDTH, 1.0f, 1.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 1.0f, 1.0f, 0.5f, }
    );
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(0x77,0x87,0,FULL_CVG); });

    // Disable fog
    rdpq_mode_fog(0);
    rdpq_fill_rectangle(0, 0, FBWIDTH, FBWIDTH);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,0,0,FULL_CVG); });
}

void test_rdpq_mode_antialias(TestContext *ctx) {
    RDPQ_INIT();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);
    surface_clear(&fb, 0);

    void draw_tri(void) {
        rdpq_triangle(&TRIFMT_SHADE,
            //         X              Y  R     G     B     A
            (float[]){ 0,             0, 1.0f, 1.0f, 1.0f, 0.5f, },
            (float[]){ FBWIDTH,       0, 1.0f, 1.0f, 1.0f, 0.5f, },
            (float[]){ FBWIDTH, FBWIDTH, 1.0f, 1.0f, 1.0f, 0.5f, }
        );
    }

    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    draw_tri();
    rspq_wait();
    uint64_t som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
                                                                     SOM_CYCLE_1    | SOM_COVERAGE_DEST_ZAP,
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("aa");
    rdpq_mode_antialias(AA_STANDARD);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE |                SOM_READ_ENABLE |            SOM_CYCLE_1    | SOM_COVERAGE_DEST_CLAMP, 
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("ra");
    rdpq_mode_antialias(AA_REDUCED);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE |                                             SOM_CYCLE_1    | SOM_COVERAGE_DEST_CLAMP, 
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("blender+ra");
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE |                             SOM_CYCLE_1    | SOM_COVERAGE_DEST_WRAP, 
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("blender+aa");
    rdpq_mode_antialias(AA_STANDARD);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE |            SOM_CYCLE_1    | SOM_COVERAGE_DEST_WRAP, 
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("blender");
    rdpq_mode_antialias(AA_NONE);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
                         SOM_BLENDING | SOM_READ_ENABLE |            SOM_CYCLE_1    | SOM_COVERAGE_DEST_ZAP, 
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("blender+aa+fog");
    rdpq_mode_fog(RDPQ_FOG_STANDARD);
    rdpq_mode_antialias(AA_STANDARD);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_2    | SOM_COVERAGE_DEST_WRAP, 
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("blender+ra+fog");
    rdpq_mode_fog(RDPQ_FOG_STANDARD);
    rdpq_mode_antialias(AA_REDUCED);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_2    | SOM_COVERAGE_DEST_WRAP, 
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("ra+fog");
    rdpq_mode_blender(false);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE |                                  SOMX_FOG | SOM_CYCLE_2    | SOM_COVERAGE_DEST_CLAMP, 
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("aa+fog");
    rdpq_mode_antialias(AA_STANDARD);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE |                SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_2    | SOM_COVERAGE_DEST_CLAMP, 
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("fog");
    rdpq_mode_antialias(AA_NONE);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
                         SOM_BLENDING |                   SOMX_FOG | SOM_CYCLE_1    | SOM_COVERAGE_DEST_ZAP, 
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("nothing");
    rdpq_mode_fog(0);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
                                                                     SOM_CYCLE_1    | SOM_COVERAGE_DEST_ZAP,
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0xCCCC0000, 0, "invalid blender formula in first cycle");
    ASSERT_EQUAL_HEX(som & 0x33330000, 0, "invalid blender formula in second cycle");

    rdpq_debug_log_msg("aa+lod");
    rdpq_mode_antialias(AA_STANDARD);
    rdpq_mode_mipmap(MIPMAP_NEAREST, 1);
    draw_tri();
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_AA_ENABLE | SOM_BLENDING | SOM_READ_ENABLE | SOMX_FOG | SOM_CYCLE_MASK | SOM_COVERAGE_DEST_MASK), 
         SOM_AA_ENABLE |                SOM_READ_ENABLE |            SOM_CYCLE_2    | SOM_COVERAGE_DEST_CLAMP,
        "invalid SOM configuration: %08llx", som);
    ASSERT_EQUAL_HEX(som & 0xCCCC0000, 0, "invalid blender formula in first cycle");
}

void test_rdpq_mode_alphacompare(TestContext *ctx) {
    RDPQ_INIT();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);
    surface_clear(&fb, 0);

    void draw_tri(void) {
        rdpq_triangle(&TRIFMT_SHADE,
            //         X              Y  R     G     B     A
            (float[]){ 0,             0, 1.0f, 1.0f, 1.0f, 0.5f, },
            (float[]){ FBWIDTH,       0, 1.0f, 1.0f, 1.0f, 0.5f, },
            (float[]){ FBWIDTH, FBWIDTH, 1.0f, 1.0f, 1.0f, 0.5f, }
        );
    }

    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rdpq_mode_antialias(AA_NONE);

    rdpq_debug_log_msg("threshold=0");
    rdpq_mode_alphacompare(0);
    draw_tri();
    uint64_t som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_ALPHACOMPARE_MASK      | SOM_BLALPHA_MASK), 
         SOM_ALPHACOMPARE_NONE      | SOM_BLALPHA_CC,
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("threshold>0");
    rdpq_mode_alphacompare(127);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_ALPHACOMPARE_MASK      | SOM_BLALPHA_MASK), 
         SOM_ALPHACOMPARE_THRESHOLD | SOM_BLALPHA_CC,
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("threshold<0");
    rdpq_mode_alphacompare(-1);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_ALPHACOMPARE_MASK      | SOM_BLALPHA_MASK), 
         SOM_ALPHACOMPARE_NOISE     | SOM_BLALPHA_CC,
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("aa+threshold=0");
    rdpq_mode_antialias(AA_STANDARD);
    rdpq_mode_alphacompare(0);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_ALPHACOMPARE_MASK      | SOM_BLALPHA_MASK), 
         SOM_ALPHACOMPARE_NONE      | SOM_BLALPHA_CVG,
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("aa+threshold>0");
    rdpq_mode_alphacompare(127);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_ALPHACOMPARE_MASK      | SOM_BLALPHA_MASK), 
         SOM_ALPHACOMPARE_NONE      | SOM_BLALPHA_CVG_TIMES_CC,
        "invalid SOM configuration: %08llx", som);

    rdpq_debug_log_msg("aa+threshold<0");
    rdpq_mode_alphacompare(-1);
    draw_tri();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & 
        (SOM_ALPHACOMPARE_MASK      | SOM_BLALPHA_MASK), 
         SOM_ALPHACOMPARE_NONE      | SOM_BLALPHA_CVG_TIMES_CC,
        "invalid SOM configuration: %08llx", som);
}

void test_rdpq_mode_freeze(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FULL_CVG = 7 << 5;   // full coverage
    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);
    surface_clear(&fb, 0);

    rdpq_debug_log_msg("Mode freeze: standard");
    rdpq_set_mode_fill(RGBA32(255,255,255,255));
    rdpq_set_blend_color(RGBA32(1,1,1,255));
    rdpq_debug_log_msg("Freeze start");
    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_set_blend_color(RGBA32(255,255,255,255));
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,0), (0,0,0,0)));
        rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, 0, BLEND_RGB, 1)));
        rdpq_mode_filter(FILTER_POINT);
        rdpq_mode_alphacompare(false);
        rdpq_debug_log_msg("Freeze end");
    rdpq_mode_end();

    rdp_draw_filled_triangle(0, 0, FBWIDTH, 0, FBWIDTH, FBWIDTH);
    rdp_draw_filled_triangle(0, 0, 0, FBWIDTH, FBWIDTH, FBWIDTH);
    rspq_wait();

    ASSERT_SURFACE(&fb, { return RGBA32(255,255,255,FULL_CVG); });

    uint32_t num_ccs = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_COMBINE_MODE_RAW + 0xC0);
    uint32_t num_soms = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_OTHER_MODES + 0xC0);

    // Inspect the dynamic buffer. We want to verify that only the right number of SOM/CC
    ASSERT_EQUAL_SIGNED(num_ccs, 1, "too many SET_COMBINE_MODE");
    ASSERT_EQUAL_SIGNED(num_soms, 2, "too many SET_OTHER_MODES"); // 1 SOM for fill, 1 SOM for standard

    // Try again within a block.
    debug_rdp_stream_reset();
    surface_clear(&fb, 0);
    rdpq_set_blend_color(RGBA32(1,1,1,255));
    rdpq_debug_log_msg("Mode freeze: in block");
    rspq_block_begin();
        rdpq_set_mode_fill(RGBA32(255,255,255,255));
        rdpq_debug_log_msg("Freeze start");
        rdpq_mode_begin();
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,0), (0,0,0,0)));
            rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, 0, BLEND_RGB, 1)));
            rdpq_mode_filter(FILTER_POINT);
            rdpq_mode_alphacompare(false);
            rdpq_debug_log_msg("Freeze end");
        rdpq_mode_end();
        rdpq_set_blend_color(RGBA32(255,255,255,255));
        rdp_draw_filled_triangle(0, 0, FBWIDTH, 0, FBWIDTH, FBWIDTH);
        rdp_draw_filled_triangle(0, 0, 0, FBWIDTH, FBWIDTH, FBWIDTH);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rspq_block_run(block);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,255,255,FULL_CVG); });

    num_ccs = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_COMBINE_MODE_RAW + 0xC0);
    num_soms = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_OTHER_MODES + 0xC0);
    int num_nops = debug_rdp_stream_count_cmd(0xC0);
    ASSERT_EQUAL_SIGNED(num_ccs, 1, "too many SET_COMBINE_MODE");
    ASSERT_EQUAL_SIGNED(num_soms, 2, "too many SET_OTHER_MODES"); // 1 SOM for fill, 1 SOM for standard
    ASSERT_EQUAL_SIGNED(num_nops, 1, "too many NOPs");  // 1 NOP from rrdpq_set_mode_fill (skips generating SET_CC)

    // Try again within a block, but doing the freeze outside of it
    debug_rdp_stream_reset();
    surface_clear(&fb, 0);
    rdpq_debug_log_msg("Mode freeze: calling a block in frozen mode");

    rspq_block_begin();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,0), (0,0,0,0)));
        rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, 0, BLEND_RGB, 1)));
        rdpq_set_blend_color(RGBA32(255,255,255,255));
    rspq_block_t *block2 = rspq_block_end();
    DEFER(rspq_block_free(block2));

    rdpq_set_mode_fill(RGBA32(255,255,255,255));
    rdpq_set_blend_color(RGBA32(1,1,1,255));
    rdpq_debug_log_msg("Freeze start");
    rdpq_mode_begin();
        rspq_block_run(block2);
    rdpq_debug_log_msg("Freeze end");
    rdpq_mode_end();
    rdpq_set_blend_color(RGBA32(255,255,255,255));
    rdp_draw_filled_triangle(0, 0, FBWIDTH, 0, FBWIDTH, FBWIDTH);
    rdp_draw_filled_triangle(0, 0, 0, FBWIDTH, FBWIDTH, FBWIDTH);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,255,255,FULL_CVG); });

    num_ccs = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_COMBINE_MODE_RAW + 0xC0);
    num_soms = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_OTHER_MODES + 0xC0);
    num_nops = debug_rdp_stream_count_cmd(0xC0);
    ASSERT_EQUAL_SIGNED(num_ccs, 1, "too many SET_COMBINE_MODE");
    ASSERT_EQUAL_SIGNED(num_soms, 2, "too many SET_OTHER_MODES"); // 1 SOM for fill, 1 SOM for standard
    ASSERT_EQUAL_SIGNED(num_nops, 8, "wrong number of NOPs");
}

void test_rdpq_mode_freeze_stack(TestContext *ctx) {
    RDPQ_INIT();

    const int FULL_CVG = 7 << 5;   // full coverage
    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);
    surface_clear(&fb, 0);

    rdpq_debug_log_msg("begin / push / end");
    rdpq_set_mode_standard();
    rdpq_mode_begin();
        rdpq_mode_push();
        rdpq_set_mode_fill(RGBA32(255,255,255,0));
    rdpq_mode_end();

    rdpq_fill_rectangle(2, 0, FBWIDTH-2, FBWIDTH);
    rspq_wait();
    
    ASSERT_SURFACE(&fb, { 
        return (x>=2 && x<FBWIDTH-2) ? 
            RGBA32(255,255,255,0) : 
            RGBA32(0,0,0,0); 
    });

    rdpq_debug_log_msg("begin / pop / end");
    surface_clear(&fb, 0);
    rdpq_mode_begin();
        rdpq_mode_pop();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_set_prim_color(RGBA32(255,0,0,0));
    rdpq_mode_end();

    rdpq_fill_rectangle(2, 0, FBWIDTH-2, FBWIDTH);
    rspq_wait();

    ASSERT_SURFACE(&fb, { 
        return (x>=2 && x<FBWIDTH-2) ? 
            RGBA32(255,0,0,FULL_CVG) : 
            RGBA32(0,0,0,0); 
    });

    rspq_wait();
}

void test_rdpq_mipmap(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FBWIDTH = 16;
    const int TEXWIDTH = FBWIDTH - 8;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    surface_t tex = surface_alloc(FMT_RGBA16, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));
    surface_clear(&tex, 0);

    uint16_t expected_fb[FBWIDTH*FBWIDTH];
    memset(expected_fb, 0xFF, sizeof(expected_fb));
    for (int y=0;y<TEXWIDTH;y++) {
        for (int x=0;x<TEXWIDTH;x++) {
            color_t c = RGBA16(x, y, x+y, 1);
            expected_fb[(y + 4) * FBWIDTH + (x + 4)] = color_to_packed16(c);
            ((uint16_t*)tex.buffer)[y * TEXWIDTH + x] = color_to_packed16(c);
        }
    }

    rdpq_set_color_image(&fb);
    rdpq_set_texture_image(&tex);
    rdpq_set_tile(0, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_set_tile(1, FMT_RGBA16, 0, TEXWIDTH * 2, 0);
    rdpq_load_tile(0, 0, 0, TEXWIDTH, TEXWIDTH);
    rdpq_load_tile(1, 0, 0, TEXWIDTH, TEXWIDTH);

    rdpq_set_mode_standard();
    rdpq_mode_mipmap(MIPMAP_NEAREST, 4);
    rdpq_triangle(&TRIFMT_TEX,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 12.0f,  4.0f, 8.0f, 0.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rspq_wait();

    // Check that MIPMAP_NEAREST forced 2-cycle mode, as mipmapping doesn't
    // work in 1-cycle mode.
    uint64_t som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");

    // Check that disabling mipmap switch back to 1-cycle mode
    rdpq_mode_mipmap(MIPMAP_NONE, 0);
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");

    // Go through the generated RDP primitives and check if the triangle
    // was patched the correct number of mipmap levels
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        if ((rdp_stream[i] >> 56) == 0xCB) {
            int levels = ((rdp_stream[i] >> 51) & 7) + 1;
            ASSERT_EQUAL_SIGNED(levels, 4, "invalid number of mipmap levels");
        }
    }
}

void test_rdpq_mipmap_interpolate(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    rdpq_attach(&fb, NULL);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
    rspq_wait();

    ASSERT_EQUAL_HEX(debug_rdp_stream_last_som() & 
        (SOM_CYCLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERP_MASK), 
         SOM_CYCLE_1                                             ,
        "invalid SOM configuration: %08llx", debug_rdp_stream_last_som());
    ASSERT_EQUAL_HEX(debug_rdp_stream_last_cc() & RDPQ_COMB1_MASK,
        RDPQ_COMBINER1((TEX0, 0, SHADE, 0), (TEX0, 0, SHADE, 0)) & RDPQ_COMB1_MASK,
        "invalid combiner configuration:\n%s", debug_rdp_stream_last_cc_disasm());

    rdpq_mode_mipmap(MIPMAP_NEAREST, 4);
    rspq_wait();

    #define CC_PASSTHROUGH1 (COMBINED,COMBINED,COMBINED,COMBINED)
    #define CC_PASSTHROUGH2 (COMBINED,COMBINED,LOD_FRAC,COMBINED)

    ASSERT_EQUAL_HEX(debug_rdp_stream_last_som() & 
        (SOM_CYCLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERP_MASK), 
         SOM_CYCLE_2    | SOM_TEXTURE_LOD                        ,
        "invalid SOM configuration: %08llx", debug_rdp_stream_last_som());
    ASSERT_EQUAL_HEX(debug_rdp_stream_last_cc() & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        RDPQ_COMBINER2((TEX0,0,SHADE,0), (TEX0,0,SHADE,0), CC_PASSTHROUGH1, CC_PASSTHROUGH2) & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        "invalid combiner configuration:\n%s", debug_rdp_stream_last_cc_disasm());

    rdpq_mode_mipmap(MIPMAP_INTERPOLATE, 4);
    rspq_wait();

    ASSERT_EQUAL_HEX(debug_rdp_stream_last_som() & 
        (SOM_CYCLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERP_MASK), 
         SOM_CYCLE_2    | SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE ,
        "invalid SOM configuration: %08llx", debug_rdp_stream_last_som());
    ASSERT_EQUAL_HEX(debug_rdp_stream_last_cc() & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        RDPQ_COMBINER2((TEX1,TEX0,LOD_FRAC,TEX0), (TEX1,TEX0,LOD_FRAC,TEX0), (COMBINED,0,SHADE,0),(COMBINED,0,SHADE,0)) & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        "invalid combiner configuration:\n%s", debug_rdp_stream_last_cc_disasm());

    rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
    rspq_wait();

    ASSERT_EQUAL_HEX(debug_rdp_stream_last_som() & 
        (SOM_CYCLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERP_MASK), 
         SOM_CYCLE_2    | SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE ,
        "invalid SOM configuration: %08llx", debug_rdp_stream_last_som());
    ASSERT_EQUAL_HEX(debug_rdp_stream_last_cc() & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        RDPQ_COMBINER2((TEX1,TEX0,LOD_FRAC,TEX0), (TEX1,TEX0,LOD_FRAC,TEX0), (COMBINED,0,PRIM,0),(COMBINED,0,PRIM,0)) & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        "invalid combiner configuration:\n%s", debug_rdp_stream_last_cc_disasm());

    rdpq_mode_mipmap(MIPMAP_INTERPOLATE_SHQ, 4);
    rspq_wait();

    ASSERT_EQUAL_HEX(debug_rdp_stream_last_som() & 
        (SOM_CYCLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERP_MASK), 
         SOM_CYCLE_2    | SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE_SHQ,
        "invalid SOM configuration: %08llx", debug_rdp_stream_last_som());
    ASSERT_EQUAL_HEX(debug_rdp_stream_last_cc() & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        RDPQ_COMBINER2((TEX1,TEX0,K5,0), (0,0,0,TEX1), (COMBINED,0,PRIM,0),(COMBINED,0,PRIM,0)) & (RDPQ_COMB0_MASK|RDPQ_COMB1_MASK),
        "invalid combiner configuration:\n%s", debug_rdp_stream_last_cc_disasm());

    rdpq_mode_mipmap(MIPMAP_NONE, 4);
    rspq_wait();

    ASSERT_EQUAL_HEX(debug_rdp_stream_last_som() & 
        (SOM_CYCLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERP_MASK), 
         SOM_CYCLE_1                                             ,
        "invalid SOM configuration: %08llx", debug_rdp_stream_last_som());
    ASSERT_EQUAL_HEX(debug_rdp_stream_last_cc() & (RDPQ_COMB0_MASK),
        RDPQ_COMBINER1((TEX0,0,PRIM,0),(TEX0,0,PRIM,0)) & (RDPQ_COMB0_MASK),
        "invalid combiner configuration:\n%s", debug_rdp_stream_last_cc_disasm());
}

void test_rdpq_autotmem(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();
   
    rdpq_set_tile_autotmem(0);
    rdpq_set_tile(TILE0, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(128);
    rdpq_set_tile(TILE1, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(64);
    rdpq_set_tile(TILE2, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(-1);

    rdpq_set_tile_autotmem(0);
    rdpq_set_tile(TILE3, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(128);
    rdpq_set_tile(TILE4, FMT_RGBA16, 0, 32, NULL);
    rdpq_set_tile_autotmem(-1);

    rdpq_set_tile_autotmem(0);
    rdpq_set_tile(TILE5, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(128);
    rdpq_set_tile_autotmem(0);
    rdpq_set_tile(TILE6, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(64);
    rdpq_set_tile(TILE7, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(-1);
    rdpq_set_tile_autotmem(-1);

    rspq_wait();

    int expected[] = { 0, 128, 128+64, 0, 0, 0, 128, 128+64 };

    int tidx = 0;
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        if ((rdp_stream[i] >> 56) == 0xF5) {  // Find all SET_TILE
            // Check tile number
            int tile = (rdp_stream[i] >> 24) & 7;
            ASSERT_EQUAL_SIGNED(tile, tidx, "invalid tile number");
            tidx++;

            int addr = ((rdp_stream[i] >> 32) & 0x1FF) * 8;
            ASSERT_EQUAL_SIGNED(addr, expected[tile], "invalid tile %d address", tile);
        }
    }

    ASSERT_EQUAL_SIGNED(tidx, 8, "invalid number of tiles");
}

void test_rdpq_autotmem_reuse(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();
   
    rdpq_set_tile_autotmem(0);
    rdpq_set_tile(TILE0, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(128);
    rdpq_set_tile(TILE1, FMT_RGBA16, RDPQ_AUTOTMEM, 32, NULL);
    rdpq_set_tile_autotmem(64);
    rdpq_set_tile(TILE2, FMT_RGBA16, RDPQ_AUTOTMEM_REUSE(0), 32, NULL);
    rdpq_set_tile(TILE3, FMT_RGBA16, RDPQ_AUTOTMEM_REUSE(64), 32, NULL);
    rspq_wait();

    int expected[] = { 0, 128, 128+0, 128+64 };

    int tidx = 0;
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        if ((rdp_stream[i] >> 56) == 0xF5) {  // Find all SET_TILE
            // Check tile number
            int tile = (rdp_stream[i] >> 24) & 7;
            ASSERT_EQUAL_SIGNED(tile, tidx, "invalid tile number");
            tidx++;

            int addr = ((rdp_stream[i] >> 32) & 0x1FF) * 8;
            ASSERT_EQUAL_SIGNED(addr, expected[tile], "invalid tile %d address", tile);
        }
    }

    ASSERT_EQUAL_SIGNED(tidx, 4, "invalid number of tiles");
}

void test_rdpq_texrect_passthrough(TestContext *ctx) {
    RDPQ_INIT();

    rspq_block_t *block;
    uint32_t texrect;

    uint32_t find_block_texrect(uint32_t *cmds) {
        for (int i=0; i<16; i++) {
            if (cmds[i] >> 24 == 0xE4) {
                return cmds[i];
            }
        }
        return 0;
    }
   
    // Block with no mode setting. Must be a fixup.
    rspq_block_begin();
        rdpq_texture_rectangle(TILE0, 0, 0, 16, 16, 0, 0);
    block = rspq_block_end();
    ASSERT_EQUAL_HEX(block->rdp_block->cmds[0] >> 24, 0xC0, "expected NOP in block");
    rspq_block_free(block);

    // Block with standard mode. Should contain a rectangle with exclusive bounds
    rspq_block_begin();
        rdpq_set_mode_standard();
        rdpq_texture_rectangle(TILE0, 0, 0, 16, 16, 0, 0);
    block = rspq_block_end();
    texrect = find_block_texrect(block->rdp_block->cmds);
    ASSERT_EQUAL_HEX(texrect, 0xe4040040, "expected exclusive bounds");
    rspq_block_free(block);

    // Block with copy mode. Should contain a rectangle with exclusive bounds
    rspq_block_begin();
        rdpq_set_mode_copy(true);
        rdpq_texture_rectangle(TILE0, 0, 0, 16, 16, 0, 0);
    block = rspq_block_end();
    texrect = find_block_texrect(block->rdp_block->cmds);
    ASSERT_EQUAL_HEX(texrect, 0xe403c03c, "expected inclusive bounds");
    rspq_block_free(block);

    // Block with standard mode coming from a sub-block.
    // Register a block that sets the standard mode
    rspq_block_begin();
        rdpq_set_mode_standard();
    rspq_block_t *block_mode = rspq_block_end();

    rspq_block_begin();
        rspq_block_run(block_mode);
        rdpq_texture_rectangle(TILE0, 0, 0, 16, 16, 0, 0);
    block = rspq_block_end();
    texrect = find_block_texrect(block->rdp_block->cmds);
    ASSERT_EQUAL_HEX(texrect, 0xe4040040, "expected exclusive bounds");
    rspq_block_free(block);
    rspq_block_free(block_mode);

    // Block with standard mode, with a sub-block that doesn't touch mode
    rspq_block_begin();
        rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, 0x00));
    block_mode = rspq_block_end();

    rspq_block_begin();
        rdpq_set_mode_standard();
        rspq_block_run(block_mode);
        rdpq_texture_rectangle(TILE0, 0, 0, 16, 16, 0, 0);
    block = rspq_block_end();
    texrect = find_block_texrect(block->rdp_block->cmds);
    ASSERT_EQUAL_HEX(texrect, 0xe4040040, "expected exclusive bounds");
    rspq_block_free(block);
    rspq_block_free(block_mode);
}
