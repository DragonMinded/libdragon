#include <malloc.h>
#include <rspq.h>
#include <rspq_constants.h>
#include <gfx.h>
#include "../src/gfx/gfx_internal.h"

static volatile int dp_intr_raised;

const unsigned long gfx_timeout = 100;

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

void test_gfx_rdp_interrupt(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    gfx_init();
    DEFER(gfx_close());

    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
}

void test_gfx_dram_buffer(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    gfx_init();
    DEFER(gfx_close());

    const uint32_t fbsize = 32 * 32 * 2;
    void *framebuffer = memalign(64, fbsize);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, fbsize);
    data_cache_hit_writeback_invalidate(framebuffer, fbsize);

    rdp_set_other_modes_raw(SOM_CYCLE_FILL);

    rspq_rdp_begin();
    rdp_set_scissor_raw(0, 0, 32 << 2, 32 << 2);
    rdp_set_fill_color_raw(0xFFFFFFFF);
    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 31);
    rdp_fill_rectangle_raw(0, 0, 32 << 2, 32 << 2);
    rdp_sync_full_raw();
    rspq_rdp_end();

    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");

    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(UncachedUShortAddr(framebuffer)[i], 0xFFFF, "Framebuffer was not cleared properly! Index: %lu", i);
    }
}

void test_gfx_static(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    gfx_init();
    DEFER(gfx_close());

    #define TEST_GFX_FBWIDTH 64
    #define TEST_GFX_FBAREA  TEST_GFX_FBWIDTH * TEST_GFX_FBWIDTH
    #define TEST_GFX_FBSIZE  TEST_GFX_FBAREA * 2

    void *framebuffer = memalign(64, TEST_GFX_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_GFX_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_GFX_FBSIZE);

    static uint16_t expected_fb[TEST_GFX_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));

    rspq_rdp_begin();
    rdp_set_other_modes_raw(SOM_CYCLE_FILL);
    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_GFX_FBWIDTH - 1);

    uint32_t color = 0;

    for (uint32_t y = 0; y < TEST_GFX_FBWIDTH; y++)
    {
        for (uint32_t x = 0; x < TEST_GFX_FBWIDTH; x += 4)
        {
            expected_fb[y * TEST_GFX_FBWIDTH + x] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 1] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 2] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 3] = (uint16_t)color;
            rdp_set_fill_color_raw(color | (color << 16));
            rdp_set_scissor_raw(x << 2, y << 2, (x + 4) << 2, (y + 1) << 2);
            rdp_fill_rectangle_raw(0, 0, TEST_GFX_FBWIDTH << 2, TEST_GFX_FBWIDTH << 2);
            rdp_sync_pipe_raw();
            color += 8;
        }
    }

    rdp_sync_full_raw();
    rspq_rdp_end();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_GFX_FBSIZE);
    //dump_mem(expected_fb, TEST_GFX_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_GFX_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_GFX_FBWIDTH
    #undef TEST_GFX_FBAREA
    #undef TEST_GFX_FBSIZE
}

void test_gfx_dynamic(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    gfx_init();
    DEFER(gfx_close());

    #define TEST_GFX_FBWIDTH 64
    #define TEST_GFX_FBAREA  TEST_GFX_FBWIDTH * TEST_GFX_FBWIDTH
    #define TEST_GFX_FBSIZE  TEST_GFX_FBAREA * 2

    void *framebuffer = memalign(64, TEST_GFX_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_GFX_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_GFX_FBSIZE);

    static uint16_t expected_fb[TEST_GFX_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));

    rdp_set_other_modes_raw(SOM_CYCLE_FILL);
    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_GFX_FBWIDTH - 1);

    uint32_t color = 0;

    for (uint32_t y = 0; y < TEST_GFX_FBWIDTH; y++)
    {
        for (uint32_t x = 0; x < TEST_GFX_FBWIDTH; x += 4)
        {
            expected_fb[y * TEST_GFX_FBWIDTH + x] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 1] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 2] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 3] = (uint16_t)color;
            rdp_set_fill_color_raw(color | (color << 16));
            rdp_set_scissor_raw(x << 2, y << 2, (x + 4) << 2, (y + 1) << 2);
            rdp_fill_rectangle_raw(0, 0, TEST_GFX_FBWIDTH << 2, TEST_GFX_FBWIDTH << 2);
            rdp_sync_pipe_raw();
            color += 8;
        }
    }

    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_GFX_FBSIZE);
    //dump_mem(expected_fb, TEST_GFX_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_GFX_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_GFX_FBWIDTH
    #undef TEST_GFX_FBAREA
    #undef TEST_GFX_FBSIZE
}

void test_gfx_mixed(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    gfx_init();
    DEFER(gfx_close());

    #define TEST_GFX_FBWIDTH 64
    #define TEST_GFX_FBAREA  TEST_GFX_FBWIDTH * TEST_GFX_FBWIDTH
    #define TEST_GFX_FBSIZE  TEST_GFX_FBAREA * 2

    void *framebuffer = memalign(64, TEST_GFX_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_GFX_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_GFX_FBSIZE);

    void *texture = malloc_uncached(TEST_GFX_FBWIDTH * 2);
    DEFER(free_uncached(texture));
    for (uint16_t i = 0; i < TEST_GFX_FBWIDTH; i++)
    {
        ((uint16_t*)texture)[i] = 0xFFFF - i;
    }
    

    static uint16_t expected_fb[TEST_GFX_FBAREA];
    memset(expected_fb, 0, sizeof(expected_fb));

    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_GFX_FBWIDTH - 1);

    uint32_t color = 0;

    for (uint32_t y = 0; y < TEST_GFX_FBWIDTH; y++)
    {
        rdp_set_other_modes_raw(SOM_CYCLE_FILL);
        
        for (uint32_t x = 0; x < TEST_GFX_FBWIDTH; x += 4)
        {
            expected_fb[y * TEST_GFX_FBWIDTH + x + 0] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 1] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 2] = (uint16_t)color;
            expected_fb[y * TEST_GFX_FBWIDTH + x + 3] = (uint16_t)color;
            rdp_set_fill_color_raw(color | (color << 16));
            rdp_set_scissor_raw(x << 2, y << 2, (x + 4) << 2, (y + 1) << 2);
            rdp_fill_rectangle_raw(0, 0, TEST_GFX_FBWIDTH << 2, TEST_GFX_FBWIDTH << 2);
            rdp_sync_pipe_raw();
            color += 8;
        }

        ++y;

        rdp_set_other_modes_raw(SOM_CYCLE_COPY);

        rspq_rdp_begin();
        rdp_set_texture_image_raw((uint32_t)texture, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_GFX_FBWIDTH - 1);
        rdp_set_tile_raw(
            RDP_TILE_FORMAT_RGBA, 
            RDP_TILE_SIZE_16BIT,
            16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        rdp_load_tile_raw(0, 0, 0, TEST_GFX_FBWIDTH << 2, 1 << 2);
        for (uint32_t x = 0; x < TEST_GFX_FBWIDTH; x += 4)
        {
            expected_fb[y * TEST_GFX_FBWIDTH + x + 0] = (uint16_t)(0xFFFF - (x + 0));
            expected_fb[y * TEST_GFX_FBWIDTH + x + 1] = (uint16_t)(0xFFFF - (x + 1));
            expected_fb[y * TEST_GFX_FBWIDTH + x + 2] = (uint16_t)(0xFFFF - (x + 2));
            expected_fb[y * TEST_GFX_FBWIDTH + x + 3] = (uint16_t)(0xFFFF - (x + 3));
            rdp_set_scissor_raw(x << 2, y << 2, (x + 4) << 2, (y + 1) << 2);
            rdp_texture_rectangle_raw(0, 
                x << 2, y << 2, (x + 4) << 2, (y + 1) << 2,
                x << 5, 0, 4 << 10, 1 << 10);
            rdp_sync_pipe_raw();
        }
        rspq_rdp_end();
    }

    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_GFX_FBSIZE);
    //dump_mem(expected_fb, TEST_GFX_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_GFX_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_GFX_FBWIDTH
    #undef TEST_GFX_FBAREA
    #undef TEST_GFX_FBSIZE
}

void test_gfx_passthrough_big(TestContext *ctx)
{
    dp_intr_raised = 0;
    register_DP_handler(dp_interrupt_handler);
    DEFER(unregister_DP_handler(dp_interrupt_handler));
    set_DP_interrupt(1);
    DEFER(set_DP_interrupt(0));

    rspq_init();
    DEFER(rspq_close());
    gfx_init();
    DEFER(gfx_close());

    #define TEST_GFX_FBWIDTH 16
    #define TEST_GFX_FBAREA  TEST_GFX_FBWIDTH * TEST_GFX_FBWIDTH
    #define TEST_GFX_FBSIZE  TEST_GFX_FBAREA * 2

    void *framebuffer = memalign(64, TEST_GFX_FBSIZE);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, TEST_GFX_FBSIZE);
    data_cache_hit_writeback_invalidate(framebuffer, TEST_GFX_FBSIZE);

    static uint16_t expected_fb[TEST_GFX_FBAREA];
    memset(expected_fb, 0xFF, sizeof(expected_fb));

    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, TEST_GFX_FBWIDTH - 1);
    rdp_set_scissor_raw(0, 0, TEST_GFX_FBWIDTH << 2, TEST_GFX_FBWIDTH << 2);
    rdp_enable_blend_fill();
    rdp_set_blend_color(0xFFFFFFFF);

    rdp_draw_filled_triangle(0, 0, TEST_GFX_FBWIDTH, 0, TEST_GFX_FBWIDTH, TEST_GFX_FBWIDTH);
    rdp_draw_filled_triangle(0, 0, 0, TEST_GFX_FBWIDTH, TEST_GFX_FBWIDTH, TEST_GFX_FBWIDTH);

    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    //dump_mem(framebuffer, TEST_GFX_FBSIZE);
    //dump_mem(expected_fb, TEST_GFX_FBSIZE);

    ASSERT_EQUAL_MEM((uint8_t*)framebuffer, (uint8_t*)expected_fb, TEST_GFX_FBSIZE, "Framebuffer contains wrong data!");

    #undef TEST_GFX_FBWIDTH
    #undef TEST_GFX_FBAREA
    #undef TEST_GFX_FBSIZE
}
