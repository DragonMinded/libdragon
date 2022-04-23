#include <malloc.h>
#include <rspq_constants.h>

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

void test_rdpq_dram_buffer(TestContext *ctx)
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

    extern void *rspq_rdp_dynamic_buffers[2];

    const uint32_t fbsize = 32 * 32 * 2;
    void *framebuffer = memalign(64, fbsize);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, fbsize);
    data_cache_hit_writeback_invalidate(framebuffer, fbsize);

    rdpq_set_other_modes(SOM_CYCLE_FILL);
    rdpq_set_scissor(0, 0, 31 << 2, 32 << 2);
    rdpq_set_fill_color(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    rspq_noop();
    rdpq_set_color_image((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 31);
    rdpq_fill_rectangle(0, 0, 32 << 2, 32 << 2);
    rdpq_sync_full();
    rspq_flush();

    wait_for_dp_interrupt(rdpq_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");

    uint64_t expected_data[] = {
        (0xEFULL << 56) | SOM_CYCLE_FILL,
        (0xEDULL << 56) | (31ULL << 14) | (32ULL << 2),
        (0xF7ULL << 56) | 0xFFFFFFFFULL,
        (0xFFULL << 56) | ((uint64_t)RDP_TILE_FORMAT_RGBA << 53) | ((uint64_t)RDP_TILE_SIZE_16BIT << 51) | (31ULL << 32) | ((uint32_t)framebuffer & 0x1FFFFFF),
        (0xF6ULL << 56) | (32ULL << 46) | (32ULL << 34),
        0xE9ULL << 56
    };

    ASSERT_EQUAL_MEM((uint8_t*)rspq_rdp_dynamic_buffers[0], (uint8_t*)expected_data, sizeof(expected_data), "Unexpected data in dynamic DRAM buffer!");

    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(UncachedUShortAddr(framebuffer)[i], 0xFFFF, "Framebuffer was not cleared properly! Index: %lu", i);
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
    rdpq_set_color_image((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH - 1);

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
            rdpq_set_scissor(x << 2, y << 2, (x + 3) << 2, (y + 1) << 2);
            rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH << 2, TEST_RDPQ_FBWIDTH << 2);
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

    rdpq_set_color_image((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH - 1);
    rdpq_set_scissor(0, 0, TEST_RDPQ_FBWIDTH << 2, TEST_RDPQ_FBWIDTH << 2);
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
            rdpq_set_scissor(x << 2, y << 2, (x + 3) << 2, (y + 1) << 2);
            rdpq_fill_rectangle(0, 0, TEST_RDPQ_FBWIDTH << 2, TEST_RDPQ_FBWIDTH << 2);
            rdpq_sync_pipe();
        }
    }
    rspq_block_t *block = rspq_block_end();
    DEFER(rspq_block_free(block));

    rdpq_set_color_image((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH - 1);
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
        rdpq_set_scissor(0 << 2, 0 << 2, (TEST_RDPQ_FBWIDTH-1) << 2, TEST_RDPQ_FBWIDTH << 2);
        rdpq_fill_rectangle(0 << 2, 0 << 2, TEST_RDPQ_FBWIDTH << 2, TEST_RDPQ_FBWIDTH << 2);
    }

    rdpq_set_other_modes(SOM_CYCLE_FILL);

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, TEST_RDPQ_FBWIDTH - 1);
    fillcolor_test();
    rdpq_sync_full();        
    rspq_flush();
    wait_for_dp_interrupt(rdpq_timeout);
    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb32, TEST_RDPQ_FBAREA*4, 
        "Wrong data in framebuffer (32-bit, dynamic mode)");

    dp_intr_raised = 0;
    memset(framebuffer, 0, TEST_RDPQ_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_RDPQ_FBSIZE);
    rdpq_set_color_image((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_RDPQ_FBWIDTH - 1);
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

