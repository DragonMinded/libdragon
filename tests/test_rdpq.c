#include <malloc.h>
#include <rspq_constants.h>
#include <rdpq.h>
#include <rdp_commands.h>

static volatile int dp_intr_raised;

const unsigned long rdpq_timeout = 100;

void dp_interrupt_handler()
{
    dp_intr_raised = 1;
}

void wait_for_dp_interrupt(unsigned long timeout)
{
    unsigned long time_start = get_ticks_ms();

    while (get_ticks_ms() - time_start < timeout) {
        // Wait until the interrupt was raised
        if (dp_intr_raised) {
            break;
        }
        // Check if the RSP has hit an assert, and if so report it.
        __rsp_check_assert(__FILE__, __LINE__, __func__);
    }
}

void test_rdpq_rdp_interrupt(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    rdpq_sync_full();
    rspq_flush();

    wait_for_dp_interrupt(rdpq_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
}

void test_rdpq_clear(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    color_t fill_color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);

    const uint32_t fbsize = 32 * 32 * 2;
    void *framebuffer = memalign(64, fbsize);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, fbsize);
    data_cache_hit_writeback_invalidate(framebuffer, fbsize);

    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 32, 32, 32 * 2);
    rdpq_set_fill_color(fill_color);
    rdpq_fill_rectangle(0, 0, 32, 32);
    rdpq_sync_full();
    rspq_flush();

    wait_for_dp_interrupt(rdpq_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(UncachedUShortAddr(framebuffer)[i], color_to_packed16(fill_color), "Framebuffer was not cleared properly! Index: %lu", i);
    }
}

void test_rdpq_dynamic(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = memalign(64, TEST_RDPQ_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);

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
            rdpq_set_fill_color(c);
            rdpq_set_scissor(x, y, x + 4, y + 1);
            rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
            rdpq_sync_pipe();
        }
    }

    rdpq_sync_full();
    rspq_flush();

    wait_for_dp_interrupt(rdpq_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_passthrough_big(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = memalign(64, TEST_RDPQ_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    rdp_enable_blend_fill();
    rdp_set_blend_color(0xFFFFFFFF);

    rdp_draw_filled_triangle(0, 0, TEST_RDPQ_FBWIDTH, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rdp_draw_filled_triangle(0, 0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);

    rdpq_sync_full();
    rspq_flush();

    wait_for_dp_interrupt(rdpq_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_block(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH
    #define TEST_RDPQ_FBSIZE  TEST_RDPQ_FBAREA * 2

    void *framebuffer = memalign(64, TEST_RDPQ_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);

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
    rdpq_sync_full();
    rspq_flush();

    wait_for_dp_interrupt(rdpq_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_RDPQ_FBSIZE);
    //dump_mem(expected_fb, TEST_RDPQ_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}


void test_rdpq_fixup_setfillcolor(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 64
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 4)

    const color_t TEST_COLOR = RGBA32(0xAA,0xBB,0xCC,0xDD);

    void *framebuffer = memalign(64, TEST_RDPQ_FBSIZE);
    DEFER(free(framebuffer));

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

    void fillcolor_test(void) {
        rdpq_set_fill_color(TEST_COLOR);
        rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    }

    rdpq_set_other_modes(SOM_CYCLE_FILL);

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*4);
    fillcolor_test();
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb32, TEST_RDPQ_FBAREA*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode)");

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);
    fillcolor_test();
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb16, TEST_RDPQ_FBAREA*2, 
        "Wrong data in framebuffer (16-bit, dynamic mode)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_fixup_setscissor(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    rdpq_init();
    DEFER(rdpq_close());

    #define TEST_RDPQ_FBWIDTH 16
    #define TEST_RDPQ_FBAREA  (TEST_RDPQ_FBWIDTH * TEST_RDPQ_FBWIDTH)
    #define TEST_RDPQ_FBSIZE  (TEST_RDPQ_FBAREA * 2)

    const color_t TEST_COLOR = RGBA32(0xFF,0xFF,0xFF,0xFF);

    void *framebuffer = memalign(64, TEST_RDPQ_FBSIZE);
    DEFER(free(framebuffer));

    static uint16_t expected_fb[TEST_RDPQ_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));
    for (int y=4;y<TEST_RDPQ_FBWIDTH-4;y++) {
        for (int x=4;x<TEST_RDPQ_FBWIDTH-4;x++) {
            expected_fb[y * TEST_RDPQ_FBWIDTH + x] = color_to_packed16(TEST_COLOR);
        }
    }

    rdpq_set_color_image(framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH*2);

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (fill mode)");

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1 cycle mode)");

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_fill_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (fill mode, update)");

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_scissor(4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4);
    rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | 0x80000000);
    rdpq_set_blend_color(TEST_COLOR);
    rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH, TEST_RDPQ_FBWIDTH);
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1 cycle mode, update)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
}

void test_rdpq_fixup_texturerect(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

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

    void *framebuffer = memalign(64, TEST_RDPQ_FBSIZE);
    DEFER(free(framebuffer));

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

    dp_intr_raised = 0;
    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_COPY);
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (copy mode)");

    dp_intr_raised = 0;
    memset(framebuffer, 0xFF, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_other_modes(SOM_CYCLE_1 | SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE | SOM_TC_FILTER | SOM_BLENDING | SOM_SAMPLE_1X1 | SOM_MIDTEXEL);
    rdpq_set_combine_mode(Comb_Rgb(ZERO, ZERO, ZERO, TEX0) | Comb_Alpha(ZERO, ZERO, ZERO, TEX0));
    rdpq_texture_rectangle(0, 4, 4, TEST_RDPQ_FBWIDTH-4, TEST_RDPQ_FBWIDTH-4, 0, 0, 1, 1);
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_RDPQ_FBSIZE, 
        "Wrong data in framebuffer (1cycle mode)");

    #undef TEST_RDPQ_FBWIDTH
    #undef TEST_RDPQ_FBAREA
    #undef TEST_RDPQ_FBSIZE
    #undef TEST_RDPQ_TEXWIDTH
    #undef TEST_RDPQ_TEXAREA
    #undef TEST_RDPQ_TEXSIZE
}

