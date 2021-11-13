
void test_dma_read_misalign(TestContext *ctx) {
	uint32_t rom = dfs_rom_addr("counter.dat");
	uint8_t rom_copy[512] __attribute__((aligned(8)));
	uint8_t ram[512] __attribute__((aligned(8)));

	data_cache_hit_writeback_invalidate(rom_copy, sizeof(rom_copy));
	dma_read(rom_copy, rom, 512);

	static const uint8_t expAA[16] = { 0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA, };

	void run(int ram_offset, int rom_offset, int length) {
		memset(ram, 0xAA, sizeof(ram));
		data_cache_hit_writeback_invalidate(ram, sizeof(ram));
		dma_read(ram+ram_offset, rom+rom_offset, length);

		ASSERT_EQUAL_MEM(ram+ram_offset-16, expAA, 16, "invalid prefix [%d/%d/%d]", ram_offset&7, rom_offset, length);
		ASSERT_EQUAL_MEM(ram+ram_offset, rom_copy+rom_offset, length, "invalid data [%d/%d/%d]", ram_offset&7, rom_offset, length);
		ASSERT_EQUAL_MEM(ram+ram_offset+length, expAA, 16, "invalid suffix [%d/%d/%d]", ram_offset&7, rom_offset, length);
	}

	for (int i=56; i<64; i++) {
		for (int j=1;j<256;j++) {
			run(i, i&1, j);
			if (ctx->result == TEST_FAILED)
				return;
		}
	}
}
