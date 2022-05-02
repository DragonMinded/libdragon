#include <malloc.h>
#include <rspq_constants.h>
#include <rdpq.h>
#include <rdp_commands.h>

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

    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_color_image(buffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, 128, 128, 128*4);
    rdpq_set_fill_color(color);
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

    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 32, 32, 32 * 2);
    rdpq_set_fill_color(fill_color);
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

    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);

    for (uint32_t y = 0; y < TEST_RDPQ_FBWIDTH; y++)
    {
        for (uint32_t x = 0; x < TEST_RDPQ_FBWIDTH; x += 4)
        {
            color_t c = RGBA16(x, y, x+y, x^y);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 1] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 2] = color_to_packed16(c);
            expected_fb[y * TEST_RDPQ_FBWIDTH + x + 3] = color_to_packed16(c);
            rdpq_sync_pipe();
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

    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
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
    rdpq_set_other_modes(SOM_CYCLE_FILL);

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
            rdpq_sync_pipe();
        }
    }
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rspq_block_run(block);
    rspq_wait();
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

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

    rdpq_set_other_modes(SOM_CYCLE_FILL);

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*4);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb32, TEST_RDPQ_FBAREA*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb16, TEST_RDPQ_FBAREA*2, 
        "Wrong data in framebuffer (16-bit, dynamic mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb32, TEST_RDPQ_FBAREA*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode, update)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
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

    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (fill mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1 cycle mode)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (fill mode, update)");

    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
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

    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdpq_set_texture_image(texture, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_TEXWIDTH);
    rdpq_set_tile(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_TEXWIDTH / 4, 0, 0,0,0,0,0,0,0,0,0,0);
    rdpq_load_tile(0, 0, 0, TEST_RDPQ_TEXWIDTH, TEST_RDPQ_TEXWIDTH);

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_COPY);
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (copy mode)");

    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_TC_FILTER | SOM_BLENDING | SOM_SAMPLE_1X1 | SOM_MIDTEXEL);
    rdpq_set_combine_mode(Comb_Rgb(ZERO, ZERO, ZERO, TEX0) | Comb_Alpha(ZERO, ZERO, ZERO, TEX0));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rspq_wait();
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1cycle mode)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
    #undef TEST_RDPQ_TEXWIDTH
    #undef TEST_RDPQ_TEXAREA
    #undef TEST_RDPQ_TEXSIZE
}

