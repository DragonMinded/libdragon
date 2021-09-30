
void test_cache_invalidate(TestContext *ctx) {
	// Interrupts causing other code to run can easily invalidate cache and make
	// this test useless. So we need them disabled while running this.
	disable_interrupts();
	DEFER(enable_interrupts());

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
			dma_read(UncachedAddr(buf), DFS_DEFAULT_LOCATION, 32);
			dma_read(UncachedAddr(buf+32), DFS_DEFAULT_LOCATION, 32);

			// For each cache-line, check whether the contents
			// match what we would expect from it:
			//  * Invalidated: we should see the data read from DFS
			//  * Not invalidated: we should see the 0xAA fill in the cache
			for (int c=0;c<4;c++) {
				bool should_be_invalidated = (j!=0) && (c >= i/16 && c <= (i+j-1)/16);

				// NOTE: ASSERT_EQUAL_MEM would do the byte-by-byte check for us
				// but it does touch the stack a lot, and that can invalidate
				// cachelines. We do the check here inline so that we don't
				// touch memory. If there's a failure, we can fallback on
				// ASSERT_EQUAL_MEM to provide error reporting.
				if (should_be_invalidated) {
					for (int k=0;k<16;k++) {
						if (buf[c*16+k] != (uint8_t)dfs_header[c%2*16+k]) {
							ASSERT_EQUAL_MEM(buf+c*16, (uint8_t*)dfs_header+c%2*16, 16,
								"unexpected data in invalidated cacheline %d (%d/%d)", c, i, j);
						}
					}
				} else {
					for (int k=0;k<16;k++) {
						if (buf[c*16+k] != (uint8_t)aaa[k]) {
							ASSERT_EQUAL_MEM(buf+c*16, (uint8_t*)aaa, 16,
								"unexpected data in not-invalidated cached cacheline %d (%d/%d)", c, i, j);
						}
					}
				}
			}
		}
	}
}
