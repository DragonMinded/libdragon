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
    if (x > surf->width) x = surf->width-1;
    if (y > surf->height) y = surf->height-1;
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

void test_rdpq_tex_load(TestContext *ctx) {
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

    rdpq_attach(&fb);
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
                rdpq_tex_load_tlut(tlut, 0, 256);
                rdpq_mode_tlut(TLUT_RGBA16);
            } else {
                rdpq_mode_tlut(TLUT_NONE);
            }

            for (int sub=0; sub < 3; sub++) {
                LOG("    sub: %d\n", sub);
                surface_t surf = surf_full;
                if (sub) surf = surface_make_sub(&surf_full, 0, 0, tex_width-sub, tex_width-sub); 

                // Blit the surface to the framebuffer, and verify the result
                for (int off = 0; off < 3; off++) {
                    LOG("      off: %d\n", off);
                    surface_clear(&fb, 0);
            
                    if (off == 0)
                        rdpq_tex_load(TILE2, &surf, 0);
                    else
                        rdpq_tex_load_sub(TILE2, &surf, 0, off, off, surf.width, surf.width);
                    rdpq_texture_rectangle(TILE2, 
                        5, 5, 5+surf.width-off, 5+surf.width-off,
                        off, off, 1.0f, 1.0f);
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
