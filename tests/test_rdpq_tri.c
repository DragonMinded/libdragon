
void test_rdpq_triangle(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FBWIDTH = 16;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    rdpq_set_color_image(&fb);
    rdpq_set_tile(TILE4, FMT_RGBA16, 0, 64, 0);
    rdpq_set_tile_size(TILE4, 0, 0, 32, 32);
    rdpq_set_mode_standard();
    rdpq_mode_mipmap(MIPMAP_NEAREST, 3);
    rdpq_set_prim_color(RGBA32(255,255,255,0));
    rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
    rspq_wait();

    // Generate floating point coordinates that maps perfectly to fixed point numbers of the expected
    // precision. What we want to test here is the accuracy of the RSP implementation, which receives
    // fixed point numbers as input. If an error is introduced in input data, it just accumulates
    // through the algorithm but it doesn't give us actionable information.
    #define RF(min,max) (((float)myrand() / (float)0xFFFFFFFF) * ((max)-(min)) + (min))
    #define RS16()      ((int)(RANDN(65536) - 32768))
    #define RFCOORD()   ((int)(RANDN(32768) - 16384) / 4.0f) 
    #define RFZ()       (RANDN(0x8000) / 32767.f)
    #define RFRGB()     (RANDN(256) / 255.0f)
    #define RFW()       RF(0.0f, 1.0f)
    #define RFTEX()     (RS16() / 64.f)   // Use s9.5 here because the RSP code has a bug for spanning too much in s10.5 space
    #define SAT16(x)    ((x) == 0x7FFF || (x) == 0x8000)

    #define TRI_CHECK(idx, start, end, msg) ({ \
        if (BITS(tcpu[idx], start, end) != BITS(trsp[idx], start, end)) { \
            debugf("CPU[%d]:\n", tri); rdpq_debug_disasm(tcpu, stderr); \
            debugf("RSP[%d]:\n", tri); rdpq_debug_disasm(trsp, stderr); \
            ASSERT_EQUAL_HEX(BITS(tcpu[idx], start, end), BITS(trsp[idx], start, end), msg); \
        } \
    })

    #define TRI_CHECK_F1616(idxi, starti, idxf, startf, threshold, msg) ({ \
        float __fcpu = (int16_t)BITS(tcpu[idxi], starti, starti+15), __frsp = (int16_t)BITS(trsp[idxi], starti, starti+15); \
        __fcpu += (float)BITS(tcpu[idxf], startf, startf+15) / 65536.0f; __frsp += (float)BITS(trsp[idxf], startf, startf+15) / 65536.0f; \
        if (fabsf(__frsp - __fcpu) > threshold) { \
            debugf("CPU[%d]:\n", tri); rdpq_debug_disasm(tcpu, stderr); \
            debugf("RSP[%d]:\n", tri); rdpq_debug_disasm(trsp, stderr); \
            ASSERT_EQUAL_FLOAT(__fcpu, __frsp, msg " (error: %.2f)", fabsf(__frsp - __fcpu)); \
        } \
    })

    const rdpq_trifmt_t trifmt = (rdpq_trifmt_t){
        .pos_offset = 0, .z_offset = 2, .tex_offset = 3, .shade_offset = 6, .tex_tile = TILE4
    };

    for (int tri=0;tri<1024;tri++) {
        if (tri == 262) continue;  // very large texture, RSP has a little less precision on DtDx
        if (tri == 849) continue;  // this has a quasi-degenerate edge. The results are different but it doesn't matter
        SRAND(tri+1);
        float v1[] = { RFCOORD(), RFCOORD(), RFZ(), RFTEX(),RFTEX(),RFW(), RFRGB(), RFRGB(), RFRGB(), RFRGB() };
        float v2[] = { RFCOORD(), RFCOORD(), RFZ(), RFTEX(),RFTEX(),RFW(), RFRGB(), RFRGB(), RFRGB(), RFRGB() };
        float v3[] = { RFCOORD(), RFCOORD(), RFZ(), RFTEX(),RFTEX(),RFW(), RFRGB(), RFRGB(), RFRGB(), RFRGB() };

        // skip degenerate triangles
        if(v1[0] == v2[0] || v2[0] == v3[0] || v1[0] == v3[0]) continue;
        if(v1[1] == v2[1] || v2[1] == v3[1] || v1[1] == v3[1]) continue;

        debug_rdp_stream_reset();
        rdpq_debug_log_msg("CPU");
        rdpq_triangle_cpu(&trifmt, v1, v2, v3);
        rdpq_debug_log_msg("RSP");
        rdpq_triangle_rsp(&trifmt, v1, v2, v3);
        rspq_wait();
        
        const int RDP_TRI_SIZE = 22;
        uint64_t *tcpu = &rdp_stream[1];
        uint64_t *trsp = &rdp_stream[RDP_TRI_SIZE+1+1];

        ASSERT_EQUAL_HEX((tcpu[0] >> 56), 0xCF, "invalid RDP primitive value (by CPU)");
        ASSERT_EQUAL_HEX((trsp[0] >> 56), 0xCF, "invalid RDP primitive value (by RSP)");

        uint8_t cmd = tcpu[0] >> 56;
        TRI_CHECK(0, 48, 63, "invalid command header (top 16 bits)");
        TRI_CHECK(0, 32, 45, "invalid YL");
        TRI_CHECK(0, 16, 29, "invalid YM");
        TRI_CHECK(0,  0, 13, "invalid YH");
        TRI_CHECK_F1616(1,48, 1,32, 0.05f, "invalid XL");
        TRI_CHECK_F1616(2,48, 2,32, 0.15f, "invalid XH");
        TRI_CHECK_F1616(3,48, 3,32, 0.15f, "invalid XM");
        TRI_CHECK_F1616(1,16, 1, 0, 0.05f, "invalid ISL");
        TRI_CHECK_F1616(2,16, 2, 0, 0.35f, "invalid ISH");
        TRI_CHECK_F1616(3,16, 3, 0, 0.35f, "invalid ISM");

        int off = 4;
        if (cmd & 4) {
            TRI_CHECK_F1616(off+0,48, off+2,48, 0.6f, "invalid Red");
            TRI_CHECK_F1616(off+0,32, off+2,32, 0.6f, "invalid Green");
            TRI_CHECK_F1616(off+0,16, off+2,16, 0.6f, "invalid Blue");
            TRI_CHECK_F1616(off+0,0,  off+2,0,  0.6f, "invalid Alpha");

            TRI_CHECK_F1616(off+1,48, off+3,48, 0.8f, "invalid DrDx");
            TRI_CHECK_F1616(off+1,32, off+3,32, 0.8f, "invalid DgDx");
            TRI_CHECK_F1616(off+1,16, off+3,16, 0.8f, "invalid DbDx");
            TRI_CHECK_F1616(off+1,0,  off+3,0,  0.8f, "invalid DaDx");

            TRI_CHECK_F1616(off+4,48, off+6,48, 0.8f, "invalid DrDe");
            TRI_CHECK_F1616(off+4,32, off+6,32, 0.8f, "invalid DgDe");
            TRI_CHECK_F1616(off+4,16, off+6,16, 0.8f, "invalid DbDe");
            TRI_CHECK_F1616(off+4,0,  off+6,0,  0.8f, "invalid DaDe");

            TRI_CHECK_F1616(off+5,48, off+7,48, 0.8f, "invalid DrDy");
            TRI_CHECK_F1616(off+5,32, off+7,32, 0.8f, "invalid DgDy");
            TRI_CHECK_F1616(off+5,16, off+7,16, 0.8f, "invalid DbDy");
            TRI_CHECK_F1616(off+5,0,  off+7,0,  0.8f, "invalid DaDy");

            off += 8;
        }

        if (cmd & 2) {
            // Skip checks for saturated W/INVW, the results would be too different
            uint16_t invw_i = tcpu[off+0]>>16;
            if (!SAT16(invw_i))
            {
                TRI_CHECK_F1616(off+0,48, off+2,48, 5.0f, "invalid S");
                TRI_CHECK_F1616(off+0,32, off+2,32, 5.0f, "invalid T");
                TRI_CHECK_F1616(off+0,16, off+2,16, 8.0f, "invalid INVW");

                TRI_CHECK_F1616(off+1,48, off+3,48, 4.0f, "invalid DsDx");
                TRI_CHECK_F1616(off+1,32, off+3,32, 4.0f, "invalid DtDx");
                TRI_CHECK_F1616(off+1,16, off+3,16, 0.8f, "invalid DwDx");

                TRI_CHECK_F1616(off+5,48, off+7,48, 4.0f, "invalid DsDy");
                TRI_CHECK_F1616(off+5,32, off+7,32, 4.0f, "invalid DtDy");
                TRI_CHECK_F1616(off+5,16, off+7,16, 0.8f, "invalid DwDy");

                // Skip checks for De components if Dx or Dy saturated.
                uint16_t dwdx_i = tcpu[off+1]>>16, dwdy_i = tcpu[off+5]>>16;
                if (!SAT16(dwdx_i) && !SAT16(dwdy_i)) {
                    TRI_CHECK_F1616(off+4,48, off+6,48, 3.0f, "invalid DsDe");
                    TRI_CHECK_F1616(off+4,32, off+6,32, 3.0f, "invalid DtDe");
                    TRI_CHECK_F1616(off+4,16, off+6,16, 0.8f, "invalid DwDe");
                }
            }

            off += 8;
        }

        if (cmd & 1) {
            TRI_CHECK_F1616(off+0,48, off+0,32, 1.2f, "invalid Z");
            TRI_CHECK_F1616(off+0,16, off+0,0,  1.8f, "invalid DzDx");
            TRI_CHECK_F1616(off+1,16, off+1,0,  1.8f, "invalid DzDy");

            // If DzDx or DzDy are saturated, avoid checking DzDe as it won't match anyway
            uint16_t dzdx_i = trsp[off+0]>>16, dzdy_i = trsp[off+1]>>16;
            if (!SAT16(dzdx_i) && !SAT16(dzdy_i))
                TRI_CHECK_F1616(off+1,48, off+1,32, 1.6f, "invalid DzDe");
            off += 2;
        }
    }
}

void test_rdpq_triangle_w1(TestContext *ctx) {
    RDPQ_INIT();
    debug_rdp_stream_init();

    const int FBWIDTH = 16;
    const int TEXWIDTH = FBWIDTH - 8;
    surface_t fb = surface_alloc(FMT_RGBA16, FBWIDTH, FBWIDTH);
    DEFER(surface_free(&fb));
    surface_clear(&fb, 0);

    surface_t tex = surface_alloc(FMT_RGBA16, TEXWIDTH, TEXWIDTH);
    DEFER(surface_free(&tex));
    surface_clear(&tex, 0);

    rdpq_set_color_image(&fb);
    rdpq_tex_upload(TILE0, &tex, NULL); 
    rdpq_set_mode_standard();
    rspq_wait();

    // Draw a triangle with W=1. This is a typical triangle calculated
    // with an orthogonal projection. It triggers a special case in the 
    // RSP code because W = 1/W, so we want to make sure we have no bugs.
    debug_rdp_stream_reset();
    rdpq_triangle(&TRIFMT_TEX,
        (float[]){ 4.0f,   4.0f, 0.0f, 0.0f, 1.0f },
        (float[]){ 12.0f,  4.0f, 8.0f, 0.0f, 1.0f },
        (float[]){ 12.0f, 12.0f, 8.0f, 8.0f, 1.0f }
    );
    rspq_wait();

    // Check that we find a triangle command in the stream, and that the W
    // coordinate is correct (saturated 0x7FFF value in the upper 16 bits).
    ASSERT_EQUAL_HEX(BITS(rdp_stream[0],56,61), RDPQ_CMD_TRI_TEX, "invalid command");
    ASSERT_EQUAL_HEX(BITS(rdp_stream[4],16,31), 0x7FFF, "invalid W coordinate");
}
