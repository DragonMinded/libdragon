

void test_rdpq_sprite_upload(TestContext *ctx)
{
    RDPQ_INIT();

    // Load a sprite without mipmaps, and with texparms set to wrap
    sprite_t *s1 = sprite_load("rom:/grass1sq.rgba32.sprite");
    surface_t s1surf = sprite_get_pixels(s1);
    DEFER(sprite_free(s1));

    surface_t fb = surface_alloc(FMT_RGBA32, s1surf.width+4, s1surf.height+4);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    rdpq_attach(&fb, NULL);
    rdpq_set_mode_standard();
    rdpq_sprite_upload(TILE0, s1, NULL);
    rdpq_texture_rectangle(TILE0, 0, 0, s1surf.width+4, s1surf.height+4, 0, 0);
    rdpq_detach_wait();

    ASSERT_SURFACE(&fb, {
        if (x >= s1surf.width) x -= s1surf.width;
        if (y >= s1surf.height) y -= s1surf.height;
        color_t c = color_from_packed32(((uint32_t*)s1surf.buffer)[y*s1surf.width + x]);
        c.a = 0xE0;
        return c;
    });
}

void test_rdpq_sprite_lod(TestContext *ctx)
{
    RDPQ_INIT();

    // Load a sprite that contains mipmaps. We want to check that they are
    // loaded correctly and mipmap mode is configured.
    sprite_t *s1 = sprite_load("rom:/grass2.rgba32.sprite");
    DEFER(sprite_free(s1));
    surface_t s1surf = sprite_get_pixels(s1);
    surface_t s1lod1 = sprite_get_lod_pixels(s1, 1);
    ASSERT_EQUAL_SIGNED(s1surf.width / 2, s1lod1.width, "invalid width of LOD 1");

    surface_t fb = surface_alloc(FMT_RGBA32, s1surf.width, s1surf.height);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    float scale = 0.499999f;
    float cs = 24 * scale;   // this compute a scale that forces LOD_FRAC to be 1 everywhere

    rdpq_attach(&fb, NULL);
    rdpq_set_mode_standard();
    rdpq_sprite_upload(TILE0, s1, NULL);

    // Draw a 12x12 rectangle with the 24x24 texture. This will blit the first
    // LOD as-is.
    rdpq_triangle(&TRIFMT_TEX,
        (float[]){ 0.0f,   0.0f,  0.0f, 0.0f, 1.0f },
        (float[]){   cs,   0.0f, 24.0f, 0.0f, 1.0f },
        (float[]){   cs,     cs, 24.0f,24.0f, 1.0f }
    );
    rdpq_triangle(&TRIFMT_TEX,
        (float[]){ 0.0f,   0.0f,  0.0f, 0.0f, 1.0f },
        (float[]){   cs,     cs, 24.0f,24.0f, 1.0f },
        (float[]){ 0.0f,     cs,  0.0f,24.0f, 1.0f }
    );

    rdpq_detach_wait();

    // Check with a threshold because LOD interpolation isn't bit perfect
    // (as LOD_FRAC isn't 1.0f but rather 255.0/256.0)
    ASSERT_SURFACE_THRESHOLD(&fb, 0x1, {
        if (x <= (int)cs && y <= (int)cs) {
            color_t c = color_from_packed32(((uint32_t*)s1lod1.buffer)[y*s1lod1.width + x]);
            c.a = 0xE0;
            return c;
        }
        return color_from_packed32(0);
    });
}
