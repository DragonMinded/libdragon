#include <malloc.h>
#include <rspq.h>
#include <rspq_constants.h>
#include <rdpq.h>
#include <rdp_commands.h>
#include "../src/rspq/rspq_commands.h"
#include "../src/rdpq/rdpq_block.h"

void test_rdpq_rspqwait(TestContext *ctx)
{
    // Verify that rspq_wait() correctly also wait for RDP to terminate
    // all its scheduled operations.
    uint32_t *buffer = malloc_uncached_aligned(64, 128*128*4);
    DEFER(free_uncached(buffer));
    memset(buffer, 0, 128*128*4);

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    color_t color = RGBA32(0x11, 0x22, 0x33, 0xFF);

    rdpq_set_mode_fill(color);
    rdpq_set_color_image(buffer, FMT_RGBA32, 128, 128, 128*4);
    rdpq_fill_rectangle(0, 0, 128, 128);
    rspq_wait();

    // Sample the end of the buffer immediately after rspq_wait. If rspq_wait
    // doesn't wait for RDP to become idle, this pixel will not be filled at
    // this point. 
    ASSERT_EQUAL_HEX(buffer[127*128+127], color_to_packed32(color),
        "invalid color in framebuffer at (127,127)");
}

void test_rdpq_clear(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    color_t fill_color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);

    const uint32_t fbsize = 32 * 32 * 2;
    uint16_t *framebuffer = malloc_uncached_aligned(64, fbsize);
    DEFER(free_uncached(framebuffer));
    memset(framebuffer, 0, fbsize);

    rdpq_set_mode_fill(fill_color);
    rdpq_set_color_image(framebuffer, FMT_RGBA16, 32, 32, 32 * 2);
    rdpq_fill_rectangle(0, 0, 32, 32);
    rspq_wait();

    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(framebuffer[i], color_to_packed16(fill_color),
            "Framebuffer was not cleared properly! Index: %lu", i);
    }
}

void test_rdpq_dynamic(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));

    rdpq_set_mode_fill(RGBA32(0,0,0,0));
    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);

    for (uint32_t y = 0; y < TEST_RDPQ_FBWIDTH; y++)
    {
        for (uint32_t x = 0; x < TEST_RDPQ_FBWIDTH; x += 4)
        {
            color_t c = RGBA16(x, y, x+y, x^y);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 1] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 2] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 3] = color_to_packed16(c);
            rdpq_set_fill_color(c);
            rdpq_set_scissor(x, y, x + 4, y + 1);
            rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
        }
    }

    rspq_wait();
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_passthrough_big(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdp_enable_blend_fill();
    rdp_set_blend_color(0xFFFFFFFF);

    rdp_draw_filled_triangle(0, 0, TEST_RDPQ_FBWIDTH, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rdp_draw_filled_triangle(0, 0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);

    rspq_wait();
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_block(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));

    rspq_block_begin();
    rdpq_set_mode_fill(RGBA32(0,0,0,0));

    for (uint32_t y = 0; y < TEST_RDPQ_FBWIDTH; y++)
    {
        for (uint32_t x = 0; x < TEST_RDPQ_FBWIDTH; x += 4)
        {
            color_t c = RGBA16(x, y, x+y, x^y);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x]     = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 1] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 2] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 3] = color_to_packed16(c);
            rdpq_set_fill_color(c);
            rdpq_set_scissor(x, y, x + 4, y + 1);
            rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
        }
    }
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rspq_block_run(block);
    rspq_wait();
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_block_coalescing(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    // The actual commands don't matter because they are never executed
    rspq_block_begin();

    // These 3 commands are supposed to go to the static RDP buffer, and
    // the 3 RSPQ_CMD_RDP commands will be coalesced into one 
    rdpq_set_env_color(RGBA32(0,0,0,0));
    rdpq_set_blend_color(RGBA32(0, 0, 0, 0));
    rdpq_fill_rectangle(0, 0, 0, 0);

    // This command is a fixup
    rdpq_set_fill_color(RGBA16(0, 0, 0, 0));

    // These 3 should also have their RSPQ_CMD_RDP coalesced
    rdpq_set_env_color(RGBA32(0,0,0,0));
    rdpq_set_blend_color(RGBA32(0, 0, 0, 0));
    rdpq_fill_rectangle(0, 0, 0, 0);

    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    uint64_t *rdp_cmds = (uint64_t*)block->rdp_block->cmds;

    uint32_t expected_cmds[] = {
        // auto sync + First 3 commands + auto sync
        (RSPQ_CMD_RDP << 24) | PhysicalAddr(rdp_cmds + 5), PhysicalAddr(rdp_cmds),
        // Fixup command (leaves a hole in rdp block)
        (RDPQ_CMD_SET_FILL_COLOR_32_FIX + 0xC0) << 24, 0,
        // Last 3 commands
        (RSPQ_CMD_RDP << 24) | PhysicalAddr(rdp_cmds + 9), PhysicalAddr(rdp_cmds + 6),
    };

    ASSERT_EQUAL_MEM((uint8_t*)block->cmds, (uint8_t*)expected_cmds, sizeof(expected_cmds), "Block commands don't match!");
}

void test_rdpq_block_contiguous(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rspq_block_begin();
    /* 1: implicit sync pipe */
    /* 2: */ rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    /* 3: implicit set fill color */ 
    /* 4: implicit set scissor */
    /* 5: */ rdpq_set_mode_fill(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    /* 6: implicit set scissor */
    /* 8: */ rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    /* 9: */ rdpq_fence(); // Put the fence inside the block so RDP never executes anything outside the block
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rspq_block_run(block);
    rspq_syncpoint_wait(rspq_syncpoint_new());

    uint64_t *rdp_cmds = (uint64_t*)block->rdp_block->cmds;

    ASSERT_EQUAL_HEX(*DP_START, PhysicalAddr(rdp_cmds), "DP_START does not point to the beginning of the block!");
    ASSERT_EQUAL_HEX(*DP_END, PhysicalAddr(rdp_cmds + 9), "DP_END points to the wrong address!");

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}


void test_rdpq_fixup_setfillcolor(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 4)

    const color_t TEST_COLOR = RGBA32(0xAA,0xBB,0xCC,0xDD);

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));

    static uint32_t expected_fb32[TEST_RDPQ_FBAREA];
    memset(expected_fb32, 0, sizeof(expected_fb32));
    for (int i=0;i<TEST_RDPQ_FBAREA;i++)
        expected_fb32[i] = (TEST_COLOR.r << 24) | (TEST_COLOR.g << 16) | (TEST_COLOR.b << 8) | (TEST_COLOR.a);

    static uint16_t expected_fb16[TEST_RDPQ_FBAREA];
    memset(expected_fb16, 0, sizeof(expected_fb16));
    for (int i=0;i<TEST_RDPQ_FBAREA;i++) {
        int r = TEST_COLOR.r >> 3;
        int g = TEST_COLOR.g >> 3;
        int b = TEST_COLOR.b >> 3;        
        expected_fb16[i] = ((r & 0x1F) << 11) | ((g & 0x1F) << 6) | ((b & 0x1F) << 1) | (TEST_COLOR.a >> 7);
    }

    rdpq_set_mode_fill(RGBA32(0,0,0,0));

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image(framebuffer, FMT_RGBA32, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*4);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb32, TEST_RDPQ_FBAREA*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb16, TEST_RDPQ_FBAREA*2, 
        "Wrong data in framebuffer (16-bit, dynamic mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_color_image(framebuffer, FMT_RGBA32, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb32, TEST_RDPQ_FBAREA*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode, update)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb16, TEST_RDPQ_FBAREA*2, 
        "Wrong data in framebuffer (16-bit, dynamic mode, update)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_fixup_setscissor(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 2)

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));
    for (int y=4;y<TEST_RDPQ_FBWIDTH-4;y++) {
        for (int x=4;x<TEST_RDPQ_FBWIDTH-4;x++) {
            expected_fb[y * TEST_RDPQ_FBWIDTH + x] = color_to_packed16(TEST_COLOR);
        }
    }

    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (fill mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes_raw(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1 cycle mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_set_other_modes_raw(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (fill mode, update)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_set_other_modes_raw(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1 cycle mode, update)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_fixup_texturerect(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 2)

    #define TEST_RDPQ_TEXWIDTH (TEST_RDPQ_FBWIDTH - 8)
    #define TEST_RDPQ_TEXAREA  (TEST_RDPQ_TEXWIDTH * TEST_RDPQ_TEXWIDTH)
    #define TEST_RDPQ_TEXSIZE  (TEST_RDPQ_TEXAREA * 2)

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));

    void *texture = malloc_uncached(TEST_RDPQ_TEXSIZE);
    DEFER(free_uncached(texture));
    memset(texture, 0, TEST_RDPQ_TEXSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));
    for (int y=0;y<TEST_RDPQ_TEXWIDTH;y++) {
        for (int x=0;x<TEST_RDPQ_TEXWIDTH;x++) {
            color_t c = RGBA16(x, y, x+y, 1);
            expected_fb[(y + 4) * TEST_RDPQ_FBWIDTH + (x + 4)] = color_to_packed16(c);
            ((uint16_t*)texture)[y * TEST_RDPQ_TEXWIDTH + x] = color_to_packed16(c);
        }
    }

    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdpq_set_texture_image(texture, FMT_RGBA16, TEST_RDPQ_TEXWIDTH);
    rdpq_set_tile(0, FMT_RGBA16, 0, TEST_RDPQ_TEXWIDTH * 2, 0);
    rdpq_load_tile(0, 0, 0, TEST_RDPQ_TEXWIDTH, TEST_RDPQ_TEXWIDTH);

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_set_mode_copy(false);
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (copy mode, dynamic mode)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1cycle mode, dynamic mode)");

    {
        memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
        rspq_block_begin();
        rdpq_set_other_modes_raw(SOM_CYCLE_COPY);
        rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
            "Wrong data in framebuffer (copy mode, static mode)");
    }

    {
        memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
        rspq_block_begin();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, TEX0)));
        // rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_TC_FILTER | SOM_BLENDING | SOM_SAMPLE_1X1 | SOM_MIDTEXEL);
        // rdpq_set_combine_mode(Comb_Rgb(ZERO, ZERO, ZERO, TEX0) | Comb_Alpha(ZERO, ZERO, ZERO, TEX0));
        rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
        rspq_block_t *block = rspq_block_end();
        DEFER(rspq_block_free(block));
        rspq_block_run(block);
        rspq_wait();
        ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
            "Wrong data in framebuffer (1cycle mode, static mode)");
    }

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
    #undef TEST_RDPQ_TEXWIDTH
    #undef TEST_RDPQ_TEXAREA
    #undef TEST_RDPQ_TEXSIZE
}

void test_rdpq_lookup_address(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 2)

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rdpq_set_mode_fill(TEST_COLOR);

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rspq_block_begin();
    rdpq_set_color_image_lookup(1, 0, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH * 2);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));
    rdpq_set_lookup_address(1, framebuffer);
    rspq_block_run(block);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
            "Wrong data in framebuffer (static mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_lookup_address(1, framebuffer);
    rdpq_set_color_image_lookup(1, 0, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH * 2);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
            "Wrong data in framebuffer (dynamic mode)");
}

void test_rdpq_lookup_address_offset(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH    16
    #define TEST_RDPQ_FBAREA     (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE     (TEST_RDPQ_FBAREA * 2)
    #define TEST_RDPQ_RECT_OFF   4
    #define TEST_RDPQ_RECT_WIDTH (TEST_RDPQ_FBWIDTH-(TEST_RDPQ_RECT_OFF*2))

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));
    for (int y=TEST_RDPQ_RECT_OFF;y<TEST_RDPQ_FBWIDTH-TEST_RDPQ_RECT_OFF;y++) {
        for (int x=TEST_RDPQ_RECT_OFF;x<TEST_RDPQ_FBWIDTH-TEST_RDPQ_RECT_OFF;x++) {
            expected_fb[y * TEST_RDPQ_FBWIDTH + x] = color_to_packed16(TEST_COLOR);
        }
    }    

    rdpq_set_mode_fill(TEST_COLOR);

    uint32_t offset = (TEST_RDPQ_RECT_OFF * TEST_RDPQ_FBWIDTH + TEST_RDPQ_RECT_OFF) * 2;

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rspq_block_begin();
    rdpq_set_color_image_lookup(1, offset, FMT_RGBA16, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_FBWIDTH * 2);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_RECT_WIDTH);
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));
    rdpq_set_lookup_address(1, framebuffer);
    rspq_block_run(block);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
            "Wrong data in framebuffer (static mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_lookup_address(1, framebuffer);
    rdpq_set_color_image_lookup(1, offset, FMT_RGBA16, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_RECT_WIDTH, TEST_RDPQ_FBWIDTH * 2);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
            "Wrong data in framebuffer (dynamic mode)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
    #undef TEST_RDPQ_RECT_OFF
    #undef TEST_RDPQ_RECT_WIDTH
}

void test_rdpq_syncfull(TestContext *ctx)
{
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

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
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    const int TEST_RDPQ_FBWIDTH = 64;
    const int TEST_RDPQ_FBAREA = TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH;
    const int TEST_RDPQ_FBSIZE = TEST_RDPQ_FBAREA * 2;
    extern void *rspq_rdp_dynamic_buffers[2];

    // clear the buffer; we're going to inspect it and it contains random data
    // (rspq doesn't need to clear it)
    memset(rspq_rdp_dynamic_buffers[0], 0, 32*8);

    rspq_block_t *block = NULL;
    DEFER(if (block) rspq_block_free(block));

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));
    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH * 2);

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

    uint8_t cnt[4] = {0};
    void count_syncs(uint64_t *cmds, int n) {
        for (int i=0;i<n;i++) {
            uint8_t cmd = cmds[i] >> 56;
            if (cmd == RDPQ_CMD_SYNC_LOAD+0xC0) cnt[0]++;
            if (cmd == RDPQ_CMD_SYNC_TILE+0xC0) cnt[1]++;
            if (cmd == RDPQ_CMD_SYNC_PIPE+0xC0) cnt[2]++;
            if (cmd == RDPQ_CMD_SYNC_FULL+0xC0) cnt[3]++;            
        }
    }

    if (use_block) {
        rdpq_block_t *bb = block->rdp_block;
        while (bb) {
            count_syncs((uint64_t*)bb->cmds, 32);
            bb = bb->next;
        }        
    }
    
    count_syncs(rspq_rdp_dynamic_buffers[0], 32);

    for (int j=0;j<4;j++) {
        if (cnt[j] != exp[j]) {
            uint64_t *cmds = rspq_rdp_dynamic_buffers[0];
            for (int i=0;i<32;i++) {
                LOG("cmd: %016llx @ %p\n", cmds[i], &cmds[i]);
            }
            ASSERT_EQUAL_MEM(cnt, exp, 4, "Unexpected sync commands");
        }
    }
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
    rdpq_set_prim_depth(0, 1);
    // NO PIPESYNC HERE
    rdpq_set_scissor(0,0,1,1);
    rdpq_fill_rectangle(0, 0, 8, 8);
}
static uint8_t __autosync_pipe1_exp[4] = {0,0,1,1};
static uint8_t __autosync_pipe1_blockexp[4] = {0,0,4,1};

static void __autosync_tile1(void) {
    rdpq_set_tile(0, FMT_RGBA16, 0, 128, 0);
    rdpq_texture_rectangle(0, 0, 0, 4, 4, 0, 0, 1, 1);    
    // NO TILESYNC HERE
    rdpq_set_tile(1, FMT_RGBA16, 0, 128, 0);
    rdpq_texture_rectangle(1, 0, 0, 4, 4, 0, 0, 1, 1);    
    rdpq_set_tile(2, FMT_RGBA16, 0, 128, 0);
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
    uint8_t *tex = malloc_uncached(8*8);
    DEFER(free_uncached(tex));

    rdpq_set_texture_image(tex, FMT_I8, 8);
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
static uint8_t __autosync_load1_blockexp[4] = {3,3,2,1};

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
    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 2)

    #define TEST_RDPQ_TEXWIDTH (TEST_RDPQ_FBWIDTH - 8)
    #define TEST_RDPQ_TEXAREA  (TEST_RDPQ_TEXWIDTH * TEST_RDPQ_TEXWIDTH)
    #define TEST_RDPQ_TEXSIZE  (TEST_RDPQ_TEXAREA * 2)

    void *framebuffer = malloc_uncached_aligned(64, TEST_RDPQ_FBSIZE);
    DEFER(free_uncached(framebuffer));

    void *texture = malloc_uncached(TEST_RDPQ_TEXSIZE);
    DEFER(free_uncached(texture));
    memset(texture, 0, TEST_RDPQ_TEXSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));
    for (int y=0;y<TEST_RDPQ_TEXWIDTH;y++) {
        for (int x=0;x<TEST_RDPQ_TEXWIDTH;x++) {
            color_t c = RGBA16(x, y, x+y, 1);
            expected_fb[(y + 4) * TEST_RDPQ_FBWIDTH + (x + 4)] = color_to_packed16(c);
            ((uint16_t*)texture)[y * TEST_RDPQ_TEXWIDTH + x] = color_to_packed16(c);
        }
    }

    uint64_t som;
    rdpq_set_color_image(framebuffer, FMT_RGBA16, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdpq_set_texture_image(texture, FMT_RGBA16, TEST_RDPQ_TEXWIDTH);
    rdpq_set_tile(0, FMT_RGBA16, 0, TEST_RDPQ_TEXWIDTH * 2, 0);
    rdpq_load_tile(0, 0, 0, TEST_RDPQ_TEXWIDTH, TEST_RDPQ_TEXWIDTH);
    rdpq_set_mode_standard();
    rdpq_set_blend_color(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    rdpq_set_fog_color(RGBA32(0xEE, 0xEE, 0xEE, 0xFF));

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (comb=1pass, blender=off)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_mode_blender(RDPQ_BLENDER1((PIXEL_RGB, FOG_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_mode_blender(RDPQ_BLENDER2(
        (BLEND_RGB, ZERO, PIXEL_RGB, ONE),
        (CYCLE1_RGB, FOG_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (comb=1pass, blender=2pass)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_mode_combiner(RDPQ_COMBINER2(
        (ZERO, ZERO, ZERO, ONE), (ZERO, ZERO, ZERO, ZERO),
        (COMBINED, ZERO, TEX0, ZERO), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (comb=2pass, blender=2pass)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_mode_blender(RDPQ_BLENDER1((PIXEL_RGB, FOG_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_2, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (comb=2pass, blender=1pass)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_mode_combiner(RDPQ_COMBINER1((ZERO, ZERO, ZERO, TEX0), (ZERO, ZERO, ZERO, ZERO)));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    som = rdpq_get_other_modes_raw();
    ASSERT_EQUAL_HEX(som & SOM_CYCLE_MASK, SOM_CYCLE_1, "invalid cycle type");
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (comb=1pass, blender=1pass)");
}
