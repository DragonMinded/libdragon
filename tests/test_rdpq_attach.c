
void test_rdpq_attach_clear(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 64;
    surface_t fb = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb));
    surface_t fbz = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fbz));

    surface_clear(&fb, 0xAA);

    rdpq_attach_clear(&fb, NULL);
    rdpq_detach_wait();

    ASSERT_SURFACE(&fb, { return RGBA32(0,0,0,0xFF); });

    surface_clear(&fb, 0xAA);
    surface_clear(&fbz, 0x22);

    rdpq_attach_clear(&fb, &fbz);
    rdpq_detach_wait();

    ASSERT_SURFACE(&fb, { return RGBA32(0,0,0,0xFF); });
    for (int i=0; i<WIDTH*WIDTH; i++)
        ASSERT_EQUAL_HEX(((uint16_t*)fbz.buffer)[i], ZBUF_MAX,
            "Invalid Z-buffer value at %d", i);
}


void test_rdpq_attach_stack(TestContext *ctx)
{
    RDPQ_INIT();

    const int WIDTH = 64;
    surface_t fb1 = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb1));
    surface_t fb2 = surface_alloc(FMT_RGBA32, WIDTH, WIDTH);
    DEFER(surface_free(&fb2));
    surface_t fbz = surface_alloc(FMT_RGBA16, WIDTH, WIDTH);
    DEFER(surface_free(&fbz));

    surface_clear(&fb1, 0xAA);
    surface_clear(&fb2, 0xAA);
    surface_clear(&fbz, 0xAA);

    rdpq_attach(&fb1, NULL);
    rdpq_attach_clear(&fb2, &fbz);
    rdpq_detach();
    rdpq_detach_wait();

    ASSERT_SURFACE(&fb1, { return RGBA32(0xAA,0xAA,0xAA,0xAA); });
    ASSERT_SURFACE(&fb2, { return RGBA32(0,0,0,0xFF); });
    for (int i=0; i<WIDTH*WIDTH; i++)
        ASSERT_EQUAL_HEX(((uint16_t*)fbz.buffer)[i], ZBUF_MAX,
            "Invalid Z-buffer value at %d", i);
}
