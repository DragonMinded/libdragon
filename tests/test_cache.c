
void test_cache_invalidate(TestContext *ctx) {
	uint8_t buf[128] __attribute__((aligned(16)));
	const char *dfs_header = "\xde\xad\xbe\xef\xff\xff\xff\xff""DragonFS 2.0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
	const char *aaa = "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa";

	for (int i=0;i<32;i++) {
		for (int j=0;j<32;j++) {

			// Read/write the whole buffer through cache,
			// so it's all populated in D-Cache.
			memset(buf, 0xA0, sizeof(buf));
			for (int i=0;i<sizeof(buf);i++) buf[i] += 0xA;

			// Writeback+Invalidate buf[i..i+j]. Now only
			// those lines should be invalidated.
			data_cache_hit_writeback_invalidate(buf+i, j);
			
			// Read from ROM (header of DFS)
			cart_rom_read(UncachedAddr(buf), DFS_DEFAULT_LOCATION, 32);
			cart_rom_read(UncachedAddr(buf+32), DFS_DEFAULT_LOCATION, 32);

			// For each cache-line, check whether the contents
			// match what we would expect from it:
			//  * Invalidated: we should see the data read from DFS
			//  * Not invalidated: we should see the 0xAA fill in the cache
			for (int c=0;c<4;c++) {
				bool should_be_invalidated = (j!=0) && (c >= i/16 && c <= (i+j-1)/16);

				if (should_be_invalidated)
					ASSERT_EQUAL_MEM(buf+c*16, (uint8_t*)dfs_header+c%2*16, 16,
						"unexpected data in invalidated cacheline %d (%d/%d)", c, i, j);
				else
					ASSERT_EQUAL_MEM(buf+c*16, (uint8_t*)aaa, 16,
						"unexpected data in not-invalidated cached cacheline %d (%d/%d)", c, i, j);
			}
		}
	}
}
