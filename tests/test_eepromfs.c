void test_eepromfs(TestContext *ctx) {
    // Skip these tests if no EEPROM is present
    const size_t eeprom_capacity = eeprom_total_blocks();
    if (eeprom_capacity == 0) {
        SKIP("EEPROM not found; skipping eepfs tests");
    } else {
        LOG("EEPROM Detected: %d blocks\n", eeprom_capacity);
    }

    // Zero out the first block of EEPROM to invalidate the filesystem signature
    const uint64_t zero_eeprom_block = 0; 
    eeprom_write(0, (uint8_t *)&zero_eeprom_block);

    uint8_t file1_src[256] = {0};
    uint8_t file1_dst[256] = {0};
    uint8_t file2_src[248] = {0};
    uint8_t file2_dst[248] = {0};

    const eepfs_entry_t eeprom_files1[] = {
        { "/file1", sizeof(file1_dst) },
        { "/file2", sizeof(file2_dst) },
    };
    const size_t eeprom_files1_count = sizeof(eeprom_files1) / sizeof(eepfs_entry_t);
    const eepfs_entry_t eeprom_files2[] = {
        { "/file1", sizeof(file1_dst) },
    };
    const size_t eeprom_files2_count = sizeof(eeprom_files2) / sizeof(eepfs_entry_t);

    int result;

    result = eepfs_init(eeprom_files1, eeprom_files1_count);
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs init failed");
    DEFER(eepfs_close());
    ASSERT(eepfs_verify_signature() == false, "expected invalid eepfs signature");
    eepfs_wipe();
    ASSERT(eepfs_verify_signature() == true, "expected valid eepfs signature");

    // Test reading zeroed-out files from wiped EEPROM
    result = eepfs_read("file1", file1_dst, sizeof(file1_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file1_src, file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");
    result = eepfs_read("file2", file2_dst, sizeof(file2_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file2_src, file2_dst, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test writing and reading file1
    for (int i = 0; i < sizeof(file1_src); i++) {
        file1_src[i] = i;
    }
    result = eepfs_write("file1", file1_src, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs write failed");
    result = eepfs_read("file1", file1_dst, sizeof(file1_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file1_src, file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs write/read mismatch");

    // Test erasing file1
    result = eepfs_erase("file1");
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs erase failed");
    memset(file1_src, 0, sizeof(file1_src));
    result = eepfs_read("file1", file1_dst, sizeof(file1_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file1_src, file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test writing and reading file2
    for (int i = 0; i < sizeof(file2_src); i++) {
        file2_src[i] = i;
    }
    result = eepfs_write("file2", file2_src, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs write failed");
    result = eepfs_read("file2", file2_dst, sizeof(file2_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file2_src, file2_dst, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs write/read mismatch");

    // Ensure file1 was not modified
    result = eepfs_read("file1", file1_dst, sizeof(file1_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file1_src, file1_dst, sizeof(file1_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test erasing file2
    result = eepfs_erase("file2");
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs erase failed");
    memset(file2_src, 0, sizeof(file2_src));
    result = eepfs_read("file2", file2_dst, sizeof(file2_dst));
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs read failed");
    result = memcmp(file2_src, file2_dst, sizeof(file2_src));
    ASSERT_EQUAL_SIGNED(result, 0, "eepfs erase/read mismatch");

    // Test signature verification
    result = eepfs_close();
    ASSERT_EQUAL_SIGNED(result, EEPFS_ESUCCESS, "eepfs close failed");
    result = eepfs_init(eeprom_files2, eeprom_files2_count);
    ASSERT(eepfs_verify_signature() == false, "expected invalid eepfs signature"); 
    eepfs_wipe();
    ASSERT(eepfs_verify_signature() == true, "expected valid eepfs signature"); 
}
