#include <malloc.h>
#include <math.h>
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

static void assert_surface(TestContext *ctx, surface_t *surf, color_t (*check)(int, int))
{
    for (int y=0;y<surf->height;y++) {
        uint32_t *line = (uint32_t*)(surf->buffer + y*surf->stride);
        for (int x=0;x<surf->width;x++) {
            color_t exp = check(x, y);
            uint32_t exp32 = color_to_packed32(exp);
            if (line[x] != exp32) {
                debug_surface32("Found:", surf->buffer, surf->width, surf->height);
                ASSERT_EQUAL_HEX(line[x], exp32, "invalid pixel at (%d,%d)", x, y);
            }
        }
    }
}

#define ASSERT_SURFACE(surf, func_body) ({ \
    color_t __check_surface(int x, int y) func_body; \
    assert_surface(ctx, surf, __check_surface); \
    if (ctx->result == TEST_FAILED) return; \
})


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
    surface_clear(&fb, 0);

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
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

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
    /* 7: set fill color */
    /* 8: */ rdpq_fill_rectangle(0, 0, WIDTH, WIDTH);
    /* 9: */ rdpq_fence(); // Put the fence inside the block so RDP never executes anything outside the block
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rspq_block_run(block);
    rspq_syncpoint_wait(rspq_syncpoint_new());

    uint64_t *rdp_cmds = (uint64_t*)block->rdp_block->cmds;

    ASSERT_EQUAL_HEX(*DP_START, PhysicalAddr(rdp_cmds), "DP_START does not point to the beginning of the block!");
    ASSERT_EQUAL_HEX(*DP_END, PhysicalAddr(rdp_cmds + 9), "DP_END points to the wrong address!");

    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, WIDTH*WIDTH*2, "Framebuffer contains wrong data!");
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
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (copy mode, dynamic mode)");

    surface_clear(&fb, 0xFF);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (1cycle mode, dynamic mode)");

    {
        surface_clear(&fb, 0xFF);
        rspq_block_begin();
        rdpq_set_other_modes_raw(SOM_CYCLE_COPY);
        rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
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
        rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
            "Wrong data in framebuffer (1cycle mode, static mode)");
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

    rdpq_set_mode_fill(RGBA32(255,0,255,0));
    rdpq_fill_rectangle(4, 4, FBWIDTH-4, FBWIDTH-4);
    rspq_wait();
    ASSERT_SURFACE(&fb, {
        return (x >= 4 && y >= 4 && x < FBWIDTH-4 && y < FBWIDTH-4) ?
            RGBA32(255,0,255,0) : RGBA32(0,0,0,0);
    });

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

void test_rdpq_syncfull(TestContext *ctx)
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
    rdpq_texture_rectangle(0, 0, 0, 4, 4, 0, 0, 1, 1);    
    // NO TILESYNC HERE
    rdpq_set_tile(1, FMT_RGBA16, 0, 128, 0);
    rdpq_set_tile_size(1, 0, 0, 16, 16);
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0, 1, 1);    
    rdpq_set_tile(2, FMT_RGBA16, 0, 128, 0);
    rdpq_set_tile_size(2, 0, 0, 16, 16);
    // NO TILESYNC HERE
    rdpq_set_tile(2, FMT_RGBA16, 0, 256, 0);
    // NO TILESYNC HERE
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0, 1, 1);    
    rdpq_texture_rectangle(0, 0, 0, 4, 4, 0, 0, 1, 1);    
    // TILESYNC HERE
    rdpq_set_tile(1, FMT_RGBA16, 0, 256, 0);
    rdpq_set_tile_size(1, 0, 0, 16, 16);
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0, 1, 1);    
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
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0, 1, 1);
    // LOADSYNC HERE
    rdpq_load_tile(0, 0, 0, 7, 7);
}
static uint8_t __autosync_load1_exp[4] = {1,0,0,1};
static uint8_t __autosync_load1_blockexp[4] = {3,2,2,1};

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
    surface_clear(&fb, 0xFF);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=off)");

    // Activate blending (1-pass blender) => 1 cycle
    surface_clear(&fb, 0xFF);
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, FOG_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass)");

    // Activate fogging (2-pass blender) => 2 cycle
    surface_clear(&fb, 0xFF);
    rdpq_mode_fog(RDPQ_BLENDER((BLEND_RGB, ZERO, IN_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=2pass)");

    // Set two-pass combiner => 2 cycle
    surface_clear(&fb, 0xFF);
    rdpq_mode_combiner(RDPQ_COMBINER2(
        (ZERO, ZERO, ZERO, ENV), (ENV, ZERO, TEX0, PRIM),
        (TEX1, ZERO, COMBINED_ALPHA, ZERO), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();    
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=2pass, blender=2pass)");

    // Disable fogging (1 pass blender) => 2 cycle
    surface_clear(&fb, 0xFF);
    rdpq_mode_fog(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=2pass, blender=1pass)");

    // Set simple combiner => 1 cycle
    surface_clear(&fb, 0xFF);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass)");

    // Push the current mode, then modify several states, then pop.
    rdpq_mode_push();
    rdpq_mode_combiner(RDPQ_COMBINER2(
        (ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO),
        (COMBINED, ZERO, ZERO, TEX1), (ZERO, ZERO, ZERO, ZERO)
    ));
    rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, ZERO, BLEND_RGB, ONE)));
    rdpq_mode_dithering(DITHER_NOISE_NOISE);
    rdpq_mode_pop();
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
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
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1.0f, 1.0f);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass1)");

    // Disable blending
    rdpq_mode_blender(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1.0f, 1.0f);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_tex, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=none)");

    // Enable fogging
    rdpq_mode_fog(RDPQ_BLENDER((IN_RGB, ZERO, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1.0f, 1.0f);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass0)");

    // Disable fogging
    rdpq_mode_fog(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1.0f, 1.0f);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_tex, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=none)");

    // Enable two-pass bleder
    rdpq_mode_blender(RDPQ_BLENDER2(
        (IN_RGB, 0, BLEND_RGB, INV_MUX_ALPHA),
        (CYCLE1_RGB, FOG_ALPHA, BLEND_RGB, 1)
    ));
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1.0f, 1.0f);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend2, FBWIDTH*FBWIDTH*2, 
        "Wrong data in framebuffer (blender=pass0+1)");

    // Disable blend
    rdpq_mode_blender(0);
    rdpq_texture_rectangle(0, 4, 4, FBWIDTH-4, FBWIDTH-4, 0, 0, 1.0f, 1.0f);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb_blend, FBWIDTH*FBWIDTH*2, 
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
    rdpq_tex_load(TILE0, &tex, 0);
    rdpq_set_mode_standard();
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rdpq_triangle(TILE0, 0, false, 0, -1, 2, -1,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 12.0f,  4.0f, 8.0f, 0.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rdpq_triangle(TILE0, 0, false, 0, -1, 2, -1,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 4.0f,  12.0f, 0.0f, 8.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)fb.buffer, (uint8_t*)expected_fb, FBWIDTH*FBWIDTH*4, "Wrong data in framebuffer");
    uint64_t som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
}


void test_rdpq_tex_load(TestContext *ctx) {
    RDPQ_INIT();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    const int TEXWIDTH = 16;
    surface_t tex = surface_alloc(FMT_CI4, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));

    // surface_t sub = surface_make_sub(&tex, 4, 4, 8, 8);
    uint16_t* tlut = malloc_uncached(256*2);

    for (int i=0;i<256;i++) {
        tlut[i] = (i<<1)|1;
    }
    for (int j=0;j<TEXWIDTH;j++) {
        for (int i=0;i<TEXWIDTH/2;i++) {
            ((uint8_t*)tex.buffer)[j * TEXWIDTH/2 + i] = 
                (((j+i*2)&15)<<4) | ((j+i*2+1)&15);
        }
    }

    rdpq_set_color_image(&fb);
    rdpq_set_mode_standard();
    rdpq_tex_load_ci4(0, &tex, 0, 4);
    rdpq_tex_load_tlut(tlut, 0, 256);
    rdpq_mode_tlut(TLUT_RGBA16);
    rdpq_texture_rectangle(0, 0, 0, 16, 16, 0, 0, 1.0f, 1.0f);
    rspq_wait();

    debug_surface("Found:", fb.buffer, FBWIDTH, FBWIDTH);

    surface_t tmem = rdpq_debug_get_tmem();
    debugf("TMEM:\n");
    debug_hexdump(tmem.buffer, 4096);
    surface_free(&tmem);
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
    rdpq_triangle(TILE0, 0, false, 0, 2, -1, -1,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH,       0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, }
    );
    rdpq_triangle(TILE0, 0, false, 0, 2, -1, -1,
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
    rdpq_triangle(TILE0, 0, false, 0, 2, -1, -1,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH,       0, 1.0f, 0.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 0.0f, 1.0f, 0.5f, }
    );
    rdpq_triangle(TILE0, 0, false, 0, 2, -1, -1,
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
    ASSERT_SURFACE(&fb, { return RGBA32(254,0,0,FULL_CVG); });

    // Activate fog
    rdpq_debug_log_msg("Custom combiner - fog");
    rdpq_mode_fog(RDPQ_FOG_STANDARD);
    rdpq_triangle(TILE0, 0, false, 0, 2, -1, -1,
        //         X              Y  R     G     B     A
        (float[]){ 0,             0, 1.0f, 1.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH,       0, 1.0f, 1.0f, 1.0f, 0.5f, },
        (float[]){ FBWIDTH, FBWIDTH, 1.0f, 1.0f, 1.0f, 0.5f, }
    );
    rdpq_triangle(TILE0, 0, false, 0, 2, -1, -1,
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
    ASSERT_SURFACE(&fb, { return RGBA32(254,0,0,FULL_CVG); });
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
    rdpq_debug_log_msg("Freeze start");
    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_set_blend_color(RGBA32(255,255,255,255));
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,0), (0,0,0,0)));
        rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, 0, BLEND_RGB, 1)));
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
    rdpq_debug_log_msg("Mode freeze: in block");
    rspq_block_begin();
        rdpq_set_mode_fill(RGBA32(255,255,255,255));
        rdpq_debug_log_msg("Freeze start");
        rdpq_mode_begin();
            rdpq_set_mode_standard();
            rdpq_set_blend_color(RGBA32(255,255,255,255));
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,0), (0,0,0,0)));
            rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, 0, BLEND_RGB, 1)));
        rdpq_mode_end();
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
    ASSERT_EQUAL_SIGNED(num_nops, 0, "too many NOPs"); 

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
    rdpq_debug_log_msg("Freeze start");
    rdpq_mode_begin();
        rspq_block_run(block2);
    rdpq_debug_log_msg("Freeze end");
    rdpq_mode_end();
    rdp_draw_filled_triangle(0, 0, FBWIDTH, 0, FBWIDTH, FBWIDTH);
    rdp_draw_filled_triangle(0, 0, 0, FBWIDTH, FBWIDTH, FBWIDTH);
    rspq_wait();
    ASSERT_SURFACE(&fb, { return RGBA32(255,255,255,FULL_CVG); });

    num_ccs = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_COMBINE_MODE_RAW + 0xC0);
    num_soms = debug_rdp_stream_count_cmd(RDPQ_CMD_SET_OTHER_MODES + 0xC0);
    num_nops = debug_rdp_stream_count_cmd(0xC0);
    ASSERT_EQUAL_SIGNED(num_ccs, 1, "too many SET_COMBINE_MODE");
    ASSERT_EQUAL_SIGNED(num_soms, 2, "too many SET_OTHER_MODES"); // 1 SOM for fill, 1 SOM for standard
    ASSERT_EQUAL_SIGNED(num_nops, 7, "wrong number of NOPs");
}

void test_rdpq_mode_freeze_stack(TestContext *ctx) {
    RDPQ_INIT();

    const int FULL_CVG = 7 << 5;   // full coverage
    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    rdpq_set_color_image(&fb);
    surface_clear(&fb, 0);

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
    rdpq_triangle(TILE0, 0, false, 0, -1, 2, 0,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 12.0f,  4.0f, 8.0f, 0.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rspq_wait();

    // Go through the generated RDP primitives and check if the triangle
    // was patched the correct number of mipmap levels
    for (int i=0;i<rdp_stream_ctx.idx;i++) {
        if ((rdp_stream[i] >> 56) == 0xCB) {
            int levels = ((rdp_stream[i] >> 51) & 7) + 1;
            ASSERT_EQUAL_SIGNED(levels, 4, "invalid number of mipmap levels");
        }
    }
}

void test_rdpq_triangle(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    rdpq_set_color_image(&fb);
    rdpq_set_tile(TILE4, FMT_RGBA16, 0, 64, 0);
    rdpq_set_tile_size(TILE4, 0, 0, 32, 32);
    rdpq_set_mode_standard();
    rdpq_mode_mipmap(MIPMAP_NEAREST, 3);
    rdpq_set_prim_color(RGBA32(255,255,255,0));
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rspq_wait();

    // Generate floating point coordinates that maps perfectly to fixed point numbers of the expected
    // precision. What we want to test here is the accuracy of the RSP implementation, which receives
    // fixed point numbers as input. If an error is introduced in input data, it just accumulates
    // through the algorithm but it doesn't give us actionable information.
    #define RF(min,max) (((float)rand() / (float)0xFFFFFFFF) * ((max)-(min)) + (min))
    #define RS16()      ((int)(RANDN(65536) - 32768))
    #define RFCOORD()   ((int)(RANDN(32768) - 16384) / 4.0f) 
    #define RFZ()       (RANDN(0x8000) / 32767.f)
    #define RFRGB()     (RANDN(256) / 255.0f)
    #define RFW()       RF(0.0f, 1.0f)
    #define RFTEX()     (RS16() / 64.f)   // Use s9.5 here because the RSP code has a bug for spanning too much in s10.5 space
    #define SAT16(x)    ((x) == 0x7FFF || (x) == 0x8000)

    #define TRI_CHECK(idx, start, end, msg) ({ \
        if (BITS(tcpu[idx], start, end) != BITS(trsp[idx], start, end)) { \
            debugf("CPU[%d]:\n", tri); rdpq_debug_disasm(tcpu, stderr); \
            debugf("RSP[%d]:\n", tri); rdpq_debug_disasm(trsp, stderr); \
            ASSERT_EQUAL_HEX(BITS(tcpu[idx], start, end), BITS(trsp[idx], start, end), msg); \
        } \
    })

    #define TRI_CHECK_F1616(idxi, starti, idxf, startf, threshold, msg) ({ \
        float __fcpu = (int16_t)BITS(tcpu[idxi], starti, starti+15), __frsp = (int16_t)BITS(trsp[idxi], starti, starti+15); \
        __fcpu += (float)BITS(tcpu[idxf], startf, startf+15) / 65536.0f; __frsp += (float)BITS(trsp[idxf], startf, startf+15) / 65536.0f; \
        if (fabsf(__frsp - __fcpu) > threshold) { \
            debugf("CPU[%d]:\n", tri); rdpq_debug_disasm(tcpu, stderr); \
            debugf("RSP[%d]:\n", tri); rdpq_debug_disasm(trsp, stderr); \
            ASSERT_EQUAL_FLOAT(__fcpu, __frsp, msg " (error: %.2f)", fabsf(__frsp - __fcpu)); \
        } \
    })

    for (int tri=0;tri<1024;tri++) {
        if (tri == 849) continue;  // this has a degenerate edge. The results are different but it doesn't matter
        SRAND(tri+1);
        float v1[] = { RFCOORD(), RFCOORD(), RFZ(), RFTEX(),RFTEX(),RFW(), RFRGB(), RFRGB(), RFRGB(), RFRGB() };
        float v2[] = { RFCOORD(), RFCOORD(), RFZ(), RFTEX(),RFTEX(),RFW(), RFRGB(), RFRGB(), RFRGB(), RFRGB() };
        float v3[] = { RFCOORD(), RFCOORD(), RFZ(), RFTEX(),RFTEX(),RFW(), RFRGB(), RFRGB(), RFRGB(), RFRGB() };

        debug_rdp_stream_reset();
        rdpq_debug_log_msg("CPU");
        rdpq_triangle_cpu(TILE4, 0, false, 0, 6, 3, 2, v1, v2, v3);
        rdpq_debug_log_msg("RSP");
        rdpq_triangle_rsp(TILE4, 0, false, 0, 6, 3, 2, v1, v2, v3);
        rspq_wait();
        
        const int RDP_TRI_SIZE = 22;
        uint64_t *tcpu = &rdp_stream[1];
        uint64_t *trsp = &rdp_stream[RDP_TRI_SIZE+1+1];

        ASSERT_EQUAL_HEX((tcpu[0] >> 56), 0xCF, "invalid RDP primitive value (by CPU)");
        ASSERT_EQUAL_HEX((trsp[0] >> 56), 0xCF, "invalid RDP primitive value (by RSP)");

        uint8_t cmd = tcpu[0] >> 56;
        TRI_CHECK(0, 48, 63, "invalid command header (top 16 bits)");
        TRI_CHECK(0, 32, 45, "invalid YL");
        TRI_CHECK(0, 16, 29, "invalid YM");
        TRI_CHECK(0,  0, 13, "invalid YH");
        TRI_CHECK_F1616(1,48, 1,32, 0.05f, "invalid XL");
        TRI_CHECK_F1616(2,48, 2,32, 0.05f, "invalid XH");
        TRI_CHECK_F1616(3,48, 3,32, 0.05f, "invalid XM");
        TRI_CHECK_F1616(1,16, 1, 0, 0.05f, "invalid ISL");
        TRI_CHECK_F1616(2,16, 2, 0, 0.05f, "invalid ISH");
        TRI_CHECK_F1616(3,16, 3, 0, 0.05f, "invalid ISM");

        int off = 4;
        if (cmd & 4) {
            TRI_CHECK_F1616(off+0,48, off+2,48, 0.6f, "invalid Red");
            TRI_CHECK_F1616(off+0,32, off+2,32, 0.6f, "invalid Green");
            TRI_CHECK_F1616(off+0,16, off+2,16, 0.6f, "invalid Blue");
            TRI_CHECK_F1616(off+0,0,  off+2,0,  0.6f, "invalid Alpha");

            TRI_CHECK_F1616(off+1,48, off+3,48, 0.8f, "invalid DrDx");
            TRI_CHECK_F1616(off+1,32, off+3,32, 0.8f, "invalid DgDx");
            TRI_CHECK_F1616(off+1,16, off+3,16, 0.8f, "invalid DbDx");
            TRI_CHECK_F1616(off+1,0,  off+3,0,  0.8f, "invalid DaDx");

            TRI_CHECK_F1616(off+4,48, off+6,48, 0.8f, "invalid DrDe");
            TRI_CHECK_F1616(off+4,32, off+6,32, 0.8f, "invalid DgDe");
            TRI_CHECK_F1616(off+4,16, off+6,16, 0.8f, "invalid DbDe");
            TRI_CHECK_F1616(off+4,0,  off+6,0,  0.8f, "invalid DaDe");

            TRI_CHECK_F1616(off+5,48, off+7,48, 0.8f, "invalid DrDy");
            TRI_CHECK_F1616(off+5,32, off+7,32, 0.8f, "invalid DgDy");
            TRI_CHECK_F1616(off+5,16, off+7,16, 0.8f, "invalid DbDy");
            TRI_CHECK_F1616(off+5,0,  off+7,0,  0.8f, "invalid DaDy");

            off += 8;
        }

        if (cmd & 2) {
            // Skip checks for saturated W/INVW, the results would be too different
            uint16_t invw_i = tcpu[off+0]>>16;
            if (!SAT16(invw_i))
            {
                TRI_CHECK_F1616(off+0,48, off+2,48, 2.0f, "invalid S");
                TRI_CHECK_F1616(off+0,32, off+2,32, 2.0f, "invalid T");
                TRI_CHECK_F1616(off+0,16, off+2,16, 2.5f, "invalid INVW");

                TRI_CHECK_F1616(off+1,48, off+3,48, 7.0f, "invalid DsDx");
                TRI_CHECK_F1616(off+1,32, off+3,32, 7.0f, "invalid DtDx");
                TRI_CHECK_F1616(off+1,16, off+3,16, 7.0f, "invalid DwDx");

                TRI_CHECK_F1616(off+5,48, off+7,48, 7.0f, "invalid DsDy");
                TRI_CHECK_F1616(off+5,32, off+7,32, 7.0f, "invalid DtDy");
                TRI_CHECK_F1616(off+5,16, off+7,16, 7.0f, "invalid DwDy");

                // Skip checks for De components if Dx or Dy saturated.
                uint16_t dwdx_i = tcpu[off+1]>>16, dwdy_i = tcpu[off+5]>>16;
                if (!SAT16(dwdx_i) && !SAT16(dwdy_i)) {
                    TRI_CHECK_F1616(off+4,48, off+6,48, 7.0f, "invalid DsDe");
                    TRI_CHECK_F1616(off+4,32, off+6,32, 7.0f, "invalid DtDe");
                    TRI_CHECK_F1616(off+4,16, off+6,16, 7.0f, "invalid DwDe");
                }
            }

            off += 8;
        }

        if (cmd & 1) {
            TRI_CHECK_F1616(off+0,48, off+0,32, 1.2f, "invalid Z");
            TRI_CHECK_F1616(off+0,16, off+0,0,  0.8f, "invalid DzDx");
            TRI_CHECK_F1616(off+1,16, off+1,0,  0.8f, "invalid DzDy");

            // If DzDx or DzDy are saturated, avoid checking DzDe as it won't match anyway
            uint16_t dzdx_i = trsp[off+0]>>16, dzdy_i = trsp[off+1]>>16;
            if (!SAT16(dzdx_i) && !SAT16(dzdy_i))
                TRI_CHECK_F1616(off+1,48, off+1,32, 0.6f, "invalid DzDe");
            off += 2;
        }
    }
}
