

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

    rdpq_matdb_t *mdb = rdpq_matdb_open("rom:/materials.mdb", true);
    DEFER(rdpq_matdb_close(mdb));

    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);
    rdpq_matdb_begin(mdb, "glass");
    ASSERT_MAT_SOM("glass", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_ALPHACOMPARE_THRESHOLD);
    ASSERT_MAT_CC("glass", RDPQ_COMBINER1((TEX0, PRIM, K5, 0), (0, 0, 0, TEX0)));
    rdpq_matdb_end(mdb, "glass");
    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);

    // Verify we can successfully record a block for both the begin and end
    rspq_block_begin();
        rdpq_matdb_begin(mdb, "glass"); 
    rspq_block_t *bl_mat_begin = rspq_block_end();
    DEFER(rspq_block_free(bl_mat_begin));

    rspq_block_begin();
        rdpq_matdb_end(mdb, "glass");
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
    rdpq_matdb_end(mdb, "glass");
    ASSERT_MAT_SOM("<none>", SOM_ALPHACOMPARE_MASK | SOM_AA_ENABLE, SOM_AA_ENABLE);
}
