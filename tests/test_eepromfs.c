void test_eepromfs(TestContext *ctx) {
    const int blocks = eeprom_total_blocks();
    if (blocks == 0) {
        SKIP("EEPROM not found; skipping eepfs tests");
    }
    if (IN_EMULATOR) {
        // Gracefully handle broken default EEPROM in cen64
        uint64_t eeprom_block = 0;
        uint8_t * eeprom_bytes = (uint8_t *)&eeprom_block;
        eeprom_write(0, eeprom_bytes);
        eeprom_read(0, eeprom_bytes);
        if (eeprom_block != 0) {
            SKIP("cen64 defaults to broken 4K EEPROM implementation");
        }
    }

    uint8_t file1_src[256] = {0};
    uint8_t file1_dst[256] = {0};
    uint8_t file2_src[248] = {0};
    uint8_t file2_dst[248] = {0};

    const eepfs_entry_t eeprom_files[] = {
        { "/file1", sizeof(file1_dst) },
        { "/file2", sizeof(file2_dst) },
    };

    int result;

    result = eepfs_init(eeprom_files, 2);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs init failed");
    DEFER(eepfs_close());

    if (!eepfs_verify_signature()) {
        LOG("EEPROM signature is not valid;\nerasing all data!\n");
        eepfs_wipe();
    }

    // Test reading zeroed-out files from wiped EEPROM
    result = eepfs_read("file1", &file1_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file1_src, &file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");
    result = eepfs_read("file2", &file2_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file2_src, &file2_dst, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test writing and reading file1
    for (int i = 0; i < sizeof(file1_src); i++) {
        file1_src[i] = i;
    }
    result = eepfs_write("file1", &file1_src);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs write failed");
    result = eepfs_read("file1", &file1_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file1_src, &file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs write/read mismatch");

    // Test erasing file1
    result = eepfs_erase("file1");
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs erase failed");
    memset(&file1_src, 0, sizeof(file1_src));
    result = eepfs_read("file1", &file1_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file1_src, &file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test writing and reading file2
    for (int i = 0; i < sizeof(file2_src); i++) {
        file2_src[i] = i;
    }
    result = eepfs_write("file2", &file2_src);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs write failed");
    result = eepfs_read("file2", &file2_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file2_src, &file2_dst, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs write/read mismatch");

    // Ensure file1 was not modified
    result = eepfs_read("file1", &file1_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file1_src, &file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test erasing file2
    result = eepfs_erase("file2");
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs erase failed");
    memset(&file2_src, 0, sizeof(file2_src));
    result = eepfs_read("file2", &file2_dst);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(&file2_src, &file2_dst, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");
}