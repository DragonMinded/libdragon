

#define ASSERT_MAT_SOM(mat, mask, value) ({ \
    uint64_t som = rdpq_get_other_modes_raw() & (mask); \
    ASSERT_EQUAL_HEX(som, value, "invalid SOM value for material " mat); \
})

#define ASSERT_MAT_CC(mat, exp) ({ \
    uint64_t cc = rdpq_get_combiner_raw() & ~0x7F00000000000000ull; \
    if (cc != exp) { \
        ERR("ERROR: Combiner found:    %s\n", rdpq_debug_disasm_cc(cc)); \
        ERR("ERROR: Combiner expected: %s\n", rdpq_debug_disasm_cc(exp)); \
    } \
    ASSERT_EQUAL_HEX(cc, exp, "invalid CC value for material " mat); \
})

void test_rdpq_mat_basic(TestContext *ctx)
{
    RDPQ_INIT();
    rdpq_set_mode_standard();
    rdpq_mode_antialias(AA_STANDARD);
    rdpq_mat_set_texture_path("rom:/texdb");

    rdpq_matdb_t *mdb = rdpq_matdb_open("rom:/basic.mdb");
    DEFER(rdpq_matdb_close(mdb));

    rdpq_mat_t *glass = rdpq_matdb_load(mdb, "glass");
    ASSERT(glass != NULL, "Failed to load material 'glass'");

    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);
    rdpq_mat_draw_begin(glass);
    ASSERT_MAT_SOM("glass", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_ALPHACOMPARE_THRESHOLD);
    ASSERT_MAT_CC("glass", RDPQ_COMBINER1((TEX0, PRIM, K5, 0), (0, 0, 0, TEX0)));
    rdpq_mat_draw_end(glass);
    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);

    // Verify we can successfully record a block for both the begin and end
    rspq_block_begin();
        rdpq_mat_draw_begin(glass);
    rspq_block_t *bl_mat_begin = rspq_block_end();
    DEFER(rspq_block_free(bl_mat_begin));

    rspq_block_begin();
        rdpq_mat_draw_end(glass);
    rspq_block_t *bl_mat_end = rspq_block_end();
    DEFER(rspq_block_free(bl_mat_end));

    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);
    rspq_block_run(bl_mat_begin);
    ASSERT_MAT_SOM("glass", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_ALPHACOMPARE_THRESHOLD);
    ASSERT_MAT_CC("glass", RDPQ_COMBINER1((TEX0, PRIM, K5, 0), (0, 0, 0, TEX0)));
    rspq_block_run(bl_mat_end);
    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);

    // Mix-match block and non block
    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);
    rspq_block_run(bl_mat_begin);
    ASSERT_MAT_SOM("glass", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_ALPHACOMPARE_THRESHOLD);
    ASSERT_MAT_CC("glass", RDPQ_COMBINER1((TEX0, PRIM, K5, 0), (0, 0, 0, TEX0)));
    rdpq_mat_draw_end(glass);
    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);
}


void test_rdpq_mat_ext(TestContext *ctx)
{
    RDPQ_INIT();
    rdpq_set_mode_standard();
    rdpq_mode_antialias(AA_STANDARD);
    rdpq_mat_set_texture_path("rom:/texdb");

    rdpq_matdb_t *mdb = rdpq_matdb_open("rom:/basic.mdb");
    DEFER(rdpq_matdb_close(mdb));

    rdpq_mat_t *mat = rdpq_matdb_load(mdb, "ext_test");
    ASSERT(mat != NULL, "Failed to load material 'ext_test'");

    bool bval;
    uint32_t ival;
    float fval;
    char *sval;
    
    ASSERT(rdpq_mat_ext_get_bool(mat, "test.b", &bval), "ext.test.b not found");
    ASSERT(bval, "ext.test.b is false, expected true");
    ASSERT(rdpq_mat_ext_get_int(mat, "test.i1", &ival), "ext.test.i1 not found");
    ASSERT_EQUAL_UNSIGNED(ival, 0x12345678, "ext.test.i1 value mismatch");
    ASSERT(rdpq_mat_ext_get_int(mat, "test.i2", &ival), "ext.test.i2 not found");
    ASSERT_EQUAL_SIGNED((int32_t)ival, -1234567, "ext.test.i2 value mismatch");
    ASSERT(rdpq_mat_ext_get_float(mat, "test.f", &fval), "ext.test.f not found");
    ASSERT_EQUAL_FLOAT(fval, 12345.67f, "ext.test.f value mismatch");
    ASSERT(rdpq_mat_ext_get_string(mat, "test.s", &sval), "ext.test.s not found");
    ASSERT_EQUAL_STR(sval, "this is a string", "ext.test.s value mismatch");
}
