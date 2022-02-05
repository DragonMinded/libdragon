
#include <malloc.h>
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

    extern uint8_t __gfx_dram_buffer[];
    data_cache_hit_writeback_invalidate(__gfx_dram_buffer, GFX_RDP_DRAM_BUFFER_SIZE);

    const uint32_t fbsize = 32 * 32 * 2;
    void *framebuffer = memalign(64, fbsize);
    DEFER(free(framebuffer));
    memset(framebuffer, 0, fbsize);

    data_cache_hit_writeback_invalidate(framebuffer, fbsize);

    rdp_set_other_modes_raw(SOM_CYCLE_FILL);
    rdp_set_scissor_raw(0, 0, 32 << 2, 32 << 2);
    rdp_set_fill_color_raw(0xFFFFFFFF);
    rspq_noop();
    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 31);
    rdp_fill_rectangle_raw(0, 0, 32 << 2, 32 << 2);
    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");

    uint64_t expected_data[] = {
        (0x2FULL << 56) | SOM_CYCLE_FILL,
        (0x2DULL << 56) | (32ULL << 14) | (32ULL << 2),
        (0x37ULL << 56) | 0xFFFFFFFFULL,
        (0x3FULL << 56) | ((uint64_t)RDP_TILE_FORMAT_RGBA << 53) | ((uint64_t)RDP_TILE_SIZE_16BIT << 51) | (31ULL << 32) | ((uint32_t)framebuffer & 0x1FFFFFF),
        (0x36ULL << 56) | (32ULL << 46) | (32ULL << 34),
        0x29ULL << 56
    };

    ASSERT_EQUAL_MEM(UncachedAddr(__gfx_dram_buffer), (uint8_t*)expected_data, sizeof(expected_data), "Unexpected data in DRAM buffer!");

    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(UncachedUShortAddr(framebuffer)[i], 0xFFFF, "Framebuffer was not cleared properly! Index: %lu", i);
    }
}

void test_gfx_fill_dmem_buffer(TestContext *ctx)
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
    rdp_set_scissor_raw(0, 0, 32 << 2, 32 << 2);
    rdp_set_fill_color_raw(0xFFFFFFFF);

    for (uint32_t i = 0; i < GFX_RDP_DMEM_BUFFER_SIZE / 8; i++)
    {
        rdp_set_prim_color_raw(0x0);
    }

    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 31);
    rdp_fill_rectangle_raw(0, 0, 32 << 2, 32 << 2);
    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(UncachedUShortAddr(framebuffer)[i], 0xFFFF, "Framebuffer was not cleared properly! Index: %lu", i);
    }
}

void test_gfx_fill_dram_buffer(TestContext *ctx)
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
    rdp_set_scissor_raw(0, 0, 32 << 2, 32 << 2);
    rdp_set_fill_color_raw(0xFFFFFFFF);

    for (uint32_t i = 0; i < GFX_RDP_DRAM_BUFFER_SIZE / 8; i++)
    {
        rdp_set_prim_color_raw(0x0);
    }

    rdp_set_color_image_raw((uint32_t)framebuffer, RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 31);
    rdp_fill_rectangle_raw(0, 0, 32 << 2, 32 << 2);
    rdp_sync_full_raw();
    rspq_flush();

    wait_for_dp_interrupt(gfx_timeout);

    ASSERT(dp_intr_raised, "Interrupt was not raised!");
    
    for (uint32_t i = 0; i < 32 * 32; i++)
    {
        ASSERT_EQUAL_HEX(UncachedUShortAddr(framebuffer)[i], 0xFFFF, "Framebuffer was not cleared properly! Index: %lu", i);
    }
}
