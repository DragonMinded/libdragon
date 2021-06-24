void test_eeprom(TestContext *ctx) {
    uint64_t pif_in[2];
    uint64_t pif_out[2];
    uint8_t pif_recv_buf[3];
    eeprom_type_t eeprom_type;

    // Test Joybus Reset command with 0xFFFFFF recv buf
    pif_in[0] = 0x00000000ff010300;
    pif_in[1] = 0xfffffffe00000000;
    pif_execute( pif_in, sizeof(pif_in), pif_out, sizeof(pif_out) );
    memcpy( pif_recv_buf, &pif_out[1], sizeof(pif_recv_buf) );
    ASSERT_EQUAL_HEX(pif_out[0], pif_in[0], "pif response mismatch");
    if (pif_recv_buf[1] == 0xc0) {
        eeprom_type = EEPROM_16K;
        LOG( "16K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x00c000fe00000000, "pif response mismatch");
    } else if (pif_recv_buf[1] == 0x80) {
        eeprom_type = EEPROM_4K;
        LOG( "4K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x008000fe00000000, "pif response mismatch");
    } else {
        eeprom_type = EEPROM_NONE;
        LOG( "EEPROM not detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0xfffffffe00000000, "pif response mismatch");
    }

    // Test Joybus Reset command with 0x000000 recv buf
    pif_in[0] = 0x00000000ff010300;
    pif_in[1] = 0x000000fe00000000;
    pif_execute( pif_in, sizeof(pif_in), pif_out, sizeof(pif_out) );
    memcpy( pif_recv_buf, &pif_out[1], sizeof(pif_recv_buf) );
    ASSERT_EQUAL_HEX(pif_out[0], pif_in[0], "pif response mismatch");
    if (pif_recv_buf[1] == 0xc0) {
        LOG( "16K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x00c000fe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_16K, "eeprom type changed?");
    } else if (pif_recv_buf[1] == 0x80) {
        LOG( "4K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x008000fe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_4K, "eeprom type changed?");
    } else {
        LOG( "EEPROM not detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x000000fe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_NONE, "eeprom type changed?");
    }

    // Test Joybus Info command with 0xFFFFFF recv buf
    pif_in[0] = 0x00000000ff0103ff;
    pif_in[1] = 0xfffffffe00000000;
    pif_execute( pif_in, sizeof(pif_in), pif_out, sizeof(pif_out) );
    memcpy( pif_recv_buf, &pif_out[1], sizeof(pif_recv_buf) );
    ASSERT_EQUAL_HEX(pif_out[0], pif_in[0], "pif response mismatch");
    if (pif_recv_buf[1] == 0xc0) {
        LOG( "16K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x00c000fffe000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_16K, "eeprom type changed?");
    } else if (pif_recv_buf[1] == 0x80) {
        LOG( "4K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x008000fffe000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_4K, "eeprom type changed?");
    } else {
        LOG( "EEPROM not detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0xfffffffe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_NONE, "eeprom type changed?");
    }

    // Test Joybus Info command with 0x000000 recv buf
    pif_in[0] = 0x00000000ff0103ff;
    pif_in[1] = 0x000000fe00000000;
    pif_execute( pif_in, sizeof(pif_in), pif_out, sizeof(pif_out) );
    memcpy( pif_recv_buf, &pif_out[1], sizeof(pif_recv_buf) );
    ASSERT_EQUAL_HEX(pif_out[0], pif_in[0], "pif response mismatch");
    if (pif_recv_buf[1] == 0xc0) {
        LOG( "16K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x00c000fe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_16K, "eeprom type changed?");
    } else if (pif_recv_buf[1] == 0x80) {
        LOG( "4K EEPROM detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x008000fe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_4K, "eeprom type changed?");
    } else {
        LOG( "EEPROM not detected.\n" );
        ASSERT_EQUAL_HEX(pif_out[1], (uint64_t)0x000000fe00000000, "pif response mismatch");
        ASSERT(eeprom_type == EEPROM_NONE, "eeprom type changed?");
    }
}
