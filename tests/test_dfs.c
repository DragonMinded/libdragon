
void test_dfs_read(TestContext *ctx) {
	int fh = dfs_open("counter.dat");
	ASSERT(fh >= 0, "counter.dat not found");
	DEFER(dfs_close(fh));

	uint8_t buf[128] __attribute__((aligned(16)));

	// random stress, unaligned buffer
	for (int i=0;i<256;i++) {
		uint8_t *ubuf = buf+RANDN(64)+2;
		int to_read = RANDN(8)+1;
		int seek = RANDN(8)*256;

		dfs_seek(fh, seek, SEEK_SET);
		memset(buf, 0xAA, sizeof(buf));
		dfs_read(ubuf, 1, to_read, fh);
		ASSERT_EQUAL_MEM(ubuf,
			(uint8_t*)"\x00\x01\x02\x03\x04\x05\x06\x07",
			to_read, "invalid unaligned read (%d/%d)", ubuf-buf, to_read);
		ASSERT_EQUAL_MEM(ubuf+to_read, (uint8_t*)"\xaa\xaa", 2, "unaligned buffer overflow");
		ASSERT_EQUAL_MEM(ubuf-2, (uint8_t*)"\xaa\xaa", 2, "unaligned buffer underflow");
	}

	// random stress, aligned buffer
	for (int i=0;i<256;i++) {
		uint8_t *ubuf = buf+8+RANDN(4)*8;
		int to_read = 1+RANDN(7);
		int seek = RANDN(16);

		dfs_seek(fh, seek, SEEK_SET);
		memset(buf, 0xAA, sizeof(buf));
		dfs_read(ubuf, 1, to_read, fh);
		ASSERT_EQUAL_MEM(ubuf,
			(uint8_t*)"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17" + seek,
			to_read, "invalid aligned read (%d/%d)", ubuf-buf, to_read);
		ASSERT_EQUAL_MEM(ubuf+to_read, (uint8_t*)"\xaa\xaa", 2, "aligned buffer overflow");
		ASSERT_EQUAL_MEM(ubuf-2, (uint8_t*)"\xaa\xaa", 2, "aligned buffer underflow");
	}

	uint8_t *abuf = buf+8;
	memset(buf, 0xAA, sizeof(buf));

	// check subsequent reads
	dfs_seek(fh, 8, SEEK_SET);
	dfs_read(abuf, 1, 16, fh);
	ASSERT_EQUAL_MEM(abuf,
		(uint8_t*)"\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18",
		16, "invalid read #2");
	ASSERT_EQUAL_MEM(abuf+16, (uint8_t*)"\xaa\xaa", 2, "buffer overflow #2");
	ASSERT_EQUAL_MEM(abuf-2, (uint8_t*)"\xaa\xaa", 2, "buffer underflow #2");

	dfs_read(abuf, 1, 16, fh);
	ASSERT_EQUAL_MEM(abuf,
		(uint8_t*)"\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28",
		16, "invalid read #3");
	ASSERT_EQUAL_MEM(abuf+16, (uint8_t*)"\xaa\xaa", 2, "buffer overflow #3");
	ASSERT_EQUAL_MEM(abuf-2, (uint8_t*)"\xaa\xaa", 2, "buffer underflow #3");

	// cross sector boundary
	dfs_seek(fh, 510, SEEK_SET);
	dfs_read(abuf, 1, 16, fh);
	ASSERT_EQUAL_MEM(abuf,
		(uint8_t*)"\xfe\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d",
		16, "invalid read #3");
	ASSERT_EQUAL_MEM(abuf+16, (uint8_t*)"\xaa\xaa", 2, "buffer overflow #3");
	ASSERT_EQUAL_MEM(abuf-2, (uint8_t*)"\xaa\xaa", 2, "buffer underflow #3");	
}

void test_dfs_rom_addr(TestContext *ctx) {
	int fh = dfs_open("counter.dat");
	ASSERT(fh >= 0, "counter.dat not found");
	DEFER(dfs_close(fh));

	uint8_t buf1[128] __attribute__((aligned(16)));
	uint8_t buf2[128] __attribute__((aligned(16)));

	dfs_read(buf1, 1, 128, fh);

	uint32_t rom = dfs_rom_addr("counter.dat");
	ASSERT(rom != 0, "counter.dat not found by dfs_rom_addr");

	ASSERT_EQUAL_HEX(io_read(rom), *(uint32_t*)buf1, "direct ROM address is different");
	ASSERT_EQUAL_HEX(io_read(rom+8), *(uint32_t*)(buf1+8), "direct ROM address is different");

	dma_read(buf2, rom, 128);
	data_cache_hit_invalidate(buf2, sizeof(buf2));

	ASSERT_EQUAL_MEM(buf1, buf2, 128, "DMA ROM access is different");
}

void test_dfs_ioctl(TestContext *ctx) {
    FILE *file = fopen("rom:/counter.dat", "rb");
    ASSERT(file, "counter.dat not found");
    DEFER(fclose(file));
    uint32_t rom_addr = 0;
    int ret = ioctl(fileno(file), IODFS_GET_ROM_BASE, &rom_addr);
    ASSERT(ret >= 0, "DFS ioctl failed");
    ASSERT(rom_addr == (dfs_rom_addr("counter.dat") & 0x1FFFFFFF), "IODFS_GET_ROM_BASE ioctl returns wrong address");
}
