#include <graphics.h>

static inline void surface_set_pixel(surface_t *surf, int x, int y, uint32_t value)
{
    void *ptr = surf->buffer + y * surf->stride;

    switch (surface_get_format(surf) & 3) {
    case 0: // 4-bit
        ptr += x/2;
        if (x & 1)
            *(uint8_t*)ptr = (*(uint8_t*)ptr & 0xF0) | (value & 0xF);
        else
            *(uint8_t*)ptr = (*(uint8_t*)ptr & 0x0F) | ((value & 0xF) << 4);
        break;
    case 1: // 8-bit
        ptr += x;
        *(uint8_t*)ptr = value;
        break;
    case 2: // 16-bit
        ptr += x*2;
        *(uint16_t*)ptr = value;
        break;
    case 3: // 32-bit
        ptr += x*4;
        *(uint32_t*)ptr = value;
        break;
    }
}

static inline uint32_t surface_get_pixel(surface_t *surf, int x, int y)
{
    void *ptr = surf->buffer + y * surf->stride;

    switch (TEX_FORMAT_BITDEPTH(surface_get_format(surf))) {
    case 4:
        ptr += x/2;
        if (x & 1)
            return *(uint8_t*)ptr & 0xF;
        else
            return (*(uint8_t*)ptr >> 4) & 0xF;
    case 8:
        ptr += x;
        return *(uint8_t*)ptr;
    case 16:
        ptr += x*2;
        return *(uint16_t*)ptr;
    case 32:
        ptr += x*4;
        return *(uint32_t*)ptr;
    default:
        assert(false);
    }
    return 0;
}

static surface_t surface_create_random(int width, int height, tex_format_t fmt)
{
    surface_t surf = surface_alloc(fmt, width, height);
    for (int j=0;j<height;j++) {
        for (int i=0;i<width;i++) {
            surface_set_pixel(&surf, i, j, rand());
        }
    }
    return surf;
}

static color_t palette_debug_color(int idx)
{
    return RGBA32(idx, ((idx+13)*17)&0xFF, ((idx+17)*13)&0xFF, 0xFF);
}

static color_t surface_debug_expected_color(surface_t *surf, int x, int y)
{
    if (x >= surf->width) x = surf->width-1;
    if (y >= surf->height) y = surf->height-1;
    uint32_t px = surface_get_pixel(surf, x, y);
    switch (surface_get_format(surf)) {
    case FMT_I4:
        px = (px << 4) | px;
        return RGBA32(px, px, px, 0xE0);
    case FMT_IA4:
        px &= 0xE;
        px = (px << 4) | (px << 1) | (px >> 2);
        return RGBA32(px, px, px, 0xE0);
    case FMT_I8:
        return RGBA32(px, px, px, 0xE0);
    case FMT_IA8: 
        px = (px & 0xF0) | (px >> 4);
        return RGBA32(px, px, px, 0xE0);    
    case FMT_IA16:
        px >>= 8;
        return RGBA32(px, px, px, 0xE0);
    case FMT_CI4: case FMT_CI8: {
        color_t c = palette_debug_color(px);
        c.r &= 0xF8; c.r |= c.r >> 5;
        c.g &= 0xF8; c.g |= c.g >> 5;
        c.b &= 0xF8; c.b |= c.b >> 5;
        c.a = 0xE0;
        return c;
    }
    case FMT_RGBA16: {
        color_t c = color_from_packed16(px);
        c.r &= 0xF8; c.r |= c.r >> 5;
        c.g &= 0xF8; c.g |= c.g >> 5;
        c.b &= 0xF8; c.b |= c.b >> 5;
        c.a = 0xE0;
        return c;
    }
    case FMT_RGBA32: {
        color_t c = color_from_packed32(px);
        c.a = 0xE0;
        return c;
    }
    default:
        assertf(0, "Unhandled format %s", tex_format_name(surface_get_format(surf)));
    }
}

void test_rdpq_tex_upload(TestContext *ctx) {
    RDPQ_INIT();

    static const tex_format_t fmts[] = { 
        FMT_RGBA32,
        FMT_RGBA16, FMT_IA16,
        FMT_CI8, FMT_I8, FMT_IA8,
        FMT_CI4, FMT_I4, FMT_IA4,
    };

    const int FBWIDTH = 32;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    uint16_t* tlut = malloc_uncached(256*2);
    for (int i=0;i<256;i++) {
        tlut[i] = color_to_packed16(palette_debug_color(i));
    }

    rdpq_attach(&fb, NULL);
    DEFER(rdpq_detach());
    rdpq_set_mode_standard();

    for (int i=0; i<sizeof(fmts) / sizeof(fmts[0]); i++) {
        LOG("Testing format %s\n", tex_format_name(fmts[i]));
        SRAND(i);
        tex_format_t fmt = fmts[i];

        // Create the random surface
        for (int tex_width = 16; tex_width < 19; tex_width++)  {
            LOG("  tex_width: %d\n", tex_width);
            surface_t surf_full = surface_create_random(tex_width, tex_width, fmt);
            DEFER(surface_free(&surf_full));

            // Activate the palette if needed for this format
            if (fmt == FMT_CI4 || fmt == FMT_CI8) {
                rdpq_tex_upload_tlut(tlut, 0, 256);
                rdpq_mode_tlut(TLUT_RGBA16);
            } else {
                rdpq_mode_tlut(TLUT_NONE);
            }

            for (int sub=0; sub < 3; sub++) {
                LOG("    sub: %d\n", sub);
                surface_t surf = surf_full;
                if (sub) surf = surface_make_sub(&surf_full, 0, 0, tex_width-sub, tex_width-sub); 

                // Blit the surface to the framebuffer, and verify the result
                for (int off = 0; off < 9; off++) {
                    LOG("      off: %d,%d\n", off%3, off/3);
                    surface_clear(&fb, 0);

                    if (off == 0)
                        rdpq_tex_upload(TILE2,&surf, NULL);
                    else
                        rdpq_tex_upload_sub(TILE2,&surf, NULL, off%3, off/3, surf.width, surf.width);
                    rdpq_texture_rectangle(TILE2, 
                        5, 5, 5+surf.width-off, 5+surf.width-off,
                        off, off);
                    rspq_wait();

                    #if 0
                    surface_t tmem = rdpq_debug_get_tmem();
                    debug_hexdump(tmem.buffer, 4096);
                    surface_free(&tmem);
                    #endif

                    ASSERT_SURFACE(&fb, {
                        if (x >= 5 && x < 5+surf.width-off && y >= 5 && y < 5+surf.width-off)
                            return surface_debug_expected_color(&surf, x-5+off, y-5+off);
                        else
                            return color_from_packed32(0);
                    });
                }
            }
        }
    }
}

void test_rdpq_tex_upload_multi(TestContext *ctx) {
    RDPQ_INIT();

    surface_t tex1 = surface_alloc(FMT_RGBA32, 8, 8);
    DEFER(surface_free(&tex1));
    surface_t tex2 = surface_alloc(FMT_RGBA32, 8, 8);
    DEFER(surface_free(&tex2));
    surface_t empty = surface_alloc(FMT_RGBA32, 32, 32);
    DEFER(surface_free(&empty));

    const int FBWIDTH = 32;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    surface_clear(&empty, 0x0);
    surface_clear(&tex1, 0x24);
    surface_clear(&tex2, 0x10);

    void do_test(void) {
        // Combine them via addition
        rdpq_attach(&fb, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER2(
            (1, 0, TEX0, TEX1),  (0, 0, 0, 0),
            (0,0,0,COMBINED),    (0,0,0,COMBINED)));
        rdpq_texture_rectangle(TILE1, 0, 0, 8, 8, 0, 0);
        rdpq_detach();
        rspq_wait();

        // Check result
        ASSERT_SURFACE(&fb, {
            if (x < 8 && y < 8)
                return color_from_packed32(0x343434e0);
            else
                return color_from_packed32(0x0);
        });
    }

    // Clear tmem
    rdpq_tex_upload(TILE0, &empty, NULL);

    // Load the two textures to TMEM
    rdpq_tex_multi_begin();
        rdpq_tex_upload(TILE1, &tex1, NULL);
        rdpq_tex_upload(TILE2, &tex2, NULL);
    rdpq_tex_multi_end();
    do_test();
    if (ctx->result == TEST_FAILED)
        return;

    // Create loader blocks
    rspq_block_begin();
    rdpq_tex_multi_begin();
        rdpq_tex_upload(TILE1, &tex1, NULL);
    rdpq_tex_multi_end();
    rspq_block_t *tex1_loader = rspq_block_end();
    DEFER(rspq_block_free(tex1_loader));

    rspq_block_begin();
    rdpq_tex_multi_begin();
        rdpq_tex_upload(TILE2, &tex2, NULL);
    rdpq_tex_multi_end();
    rspq_block_t *tex2_loader = rspq_block_end();
    DEFER(rspq_block_free(tex2_loader));

    // Load the two textures to TMEM via block loading
    rdpq_tex_upload(TILE0, &empty, NULL);
    rdpq_tex_multi_begin();
        rspq_block_run(tex1_loader);
        rspq_block_run(tex2_loader);
    rdpq_tex_multi_end();
    do_test();
    if (ctx->result == TEST_FAILED)
        return;

    // Load one texture via block loading and the other normally
    rdpq_tex_upload(TILE0, &empty, NULL);
    rdpq_tex_multi_begin();
        rdpq_tex_upload(TILE1, &tex1, NULL);
        rspq_block_run(tex2_loader);
    rdpq_tex_multi_end();
    do_test();
    if (ctx->result == TEST_FAILED)
        return;

    // Create a block that contains both tiles
    rspq_block_begin();
        rdpq_tex_multi_begin();
            rdpq_tex_upload(TILE1, &tex1, NULL);
            rdpq_tex_upload(TILE2, &tex2, NULL);
        rdpq_tex_multi_end();
    rspq_block_t *tex1_tex2_loader = rspq_block_end();

    // Load them both via block loading
    rdpq_tex_upload(TILE0, &empty, NULL);
    rspq_block_run(tex1_tex2_loader);
    do_test();
    if (ctx->result == TEST_FAILED)
        return;

    // Load them both via block loading, with explicit multi
    rdpq_tex_upload(TILE0, &empty, NULL);
    rdpq_tex_multi_begin();
        rspq_block_run(tex1_tex2_loader);
    rdpq_tex_multi_end();
    do_test();
    if (ctx->result == TEST_FAILED)
        return;

}

void test_rdpq_tex_multi_i4(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FBWIDTH = 128;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    surface_t surf = surface_alloc(FMT_I4, 124, 63);
    DEFER(surface_free(&surf));
    surface_clear(&surf, 0xAA);

    // Make sure we can correctly load a large I4 surface. We had a bug where
    // the autotmem engine was confuse by a CI8 internal tile used to perform
    // the upload.
    rdpq_tex_multi_begin();
    rdpq_tex_upload(TILE0, &surf, NULL);
    rdpq_tex_multi_end();

    rdpq_set_color_image(&fb);
    rdpq_set_mode_standard();
    rdpq_texture_rectangle(TILE0, 0, 0, 124, 63, 0, 0);
    rspq_wait();

    ASSERT_SURFACE(&fb, {
        if (x < 124 && y < 63)
            return color_from_packed32(0xAAAAAAE0);
        else
            return color_from_packed32(0x00);
    });
}

void test_rdpq_tex_blit_normal(TestContext *ctx)
{
    RDPQ_INIT();

    static const tex_format_t fmts[] = { 
        FMT_RGBA32,
        FMT_RGBA16, FMT_IA16,
        FMT_CI8, FMT_I8, FMT_IA8,
        FMT_CI4, FMT_I4, FMT_IA4,
    };

    const int FBWIDTH = 32;
    surface_t fb = surface_alloc(FMT_RGBA32, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    uint16_t* tlut = malloc_uncached(256*2);
    for (int i=0;i<256;i++) {
        tlut[i] = color_to_packed16(palette_debug_color(i));
    }

    rdpq_attach(&fb, NULL);
    DEFER(rdpq_detach());
    rdpq_set_mode_standard();

    for (int i=0; i<sizeof(fmts) / sizeof(fmts[0]); i++) {
        LOG("Testing format %s\n", tex_format_name(fmts[i]));
        SRAND(i);
        tex_format_t fmt = fmts[i];

        // Create the random surface
        for (int tex_width = 72; tex_width < 75; tex_width++)  {
            LOG("  tex_width: %d\n", tex_width);
            surface_t surf_full = surface_create_random(tex_width, tex_width, fmt);
            DEFER(surface_free(&surf_full));
        
            // Activate the palette if needed for this format
            if (fmt == FMT_CI4 || fmt == FMT_CI8) {
                rdpq_tex_upload_tlut(tlut, 0, 256);
                rdpq_mode_tlut(TLUT_RGBA16);
            } else {
                rdpq_mode_tlut(TLUT_NONE);
            }
        
            // Blit the surface to the framebuffer, and verify the result
            // Constraint to get good coverage:
            //  s0=[0..1]
            //  t0=[0..2]  t0=2 is an interesting case: it can LOAD_BLOCK (t0=1 cannot) and requires offseting of initial pointer
            //  width=[-0..-2]  we need width-2 to have an effect on 4bpp textures (width-1 uses the same bytes of width in 4bpp)
            for (int s0=0; s0<3; s0++) for (int t0=0; t0<3; t0++) for (int width=tex_width-s0; width>tex_width-s0-3; width--) {
                LOG("    s0/t0/w: %d %d %d\n", s0, t0, width);
                rdpq_tex_blit(&surf_full, 0, 0, &(rdpq_blitparms_t){
                    .s0 = s0, .width = width, .t0 = t0, .height = tex_width-t0,
                });
                rspq_wait();

                ASSERT_SURFACE(&fb, {
                    return surface_debug_expected_color(&surf_full, x+s0, y+t0);
                });
            }
        }
    }
}
