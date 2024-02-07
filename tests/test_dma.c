#include <malloc.h>

void test_dma_read_misalign(TestContext *ctx) {
	uint32_t rom = dfs_rom_addr("counter.dat");
	uint8_t rom_copy[4096] __attribute__((aligned(8)));
	uint8_t *ram = memalign(0x1000, 8192);
	DEFER(free(ram));

	data_cache_hit_writeback_invalidate(rom_copy, sizeof(rom_copy));
	dma_read(rom_copy, rom, 4096);

	memset(ram, 0xAA, 8192);
	data_cache_hit_writeback_invalidate(ram, 8192);

	static const uint8_t expAA[16] = { 0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA, };

	void run(int ram_offset, int rom_offset, int length) {
		dma_read(ram+ram_offset, rom+rom_offset, length);

		ASSERT_EQUAL_MEM(ram+ram_offset-16, expAA, 16, "invalid prefix [%d/%d/%d]", ram_offset, rom_offset, length);
		ASSERT_EQUAL_MEM(ram+ram_offset, rom_copy+rom_offset, length, "invalid data [0x%x/%d/%d]", ram_offset, rom_offset, length);
		ASSERT_EQUAL_MEM(ram+ram_offset+length, expAA, 16, "invalid suffix [0x%x/%d/%d]", ram_offset, rom_offset, length);

		memset(ram+ram_offset, 0xAA, length);
		data_cache_hit_writeback_invalidate(ram+ram_offset, length+1);
	}

	for (int i=0x7e0; i<0x800; i++) {
		for (int j=1;j<224;j++) {
			run(i, i&1, j);
			if (ctx->result == TEST_FAILED)
				return;
		}
	}
}
