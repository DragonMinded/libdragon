
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

	int offset = rom & 2; 
	ASSERT_EQUAL_HEX(io_read(rom+offset), *(u_uint32_t*)(buf1+offset), "direct ROM address is different");
	ASSERT_EQUAL_HEX(io_read(rom+offset+8), *(u_uint32_t*)(buf1+offset+8), "direct ROM address is different");

	data_cache_hit_invalidate(buf2, sizeof(buf2));
	dma_read(buf2, rom, 128);

	ASSERT_EQUAL_MEM(buf1, buf2, 128, "DMA ROM access is different");
}

void test_dfs_rom_size(TestContext *ctx) {
	int fh = dfs_open("counter.dat");
	ASSERT(fh >= 0, "counter.dat not found");
	DEFER(dfs_close(fh));

	int dfs_file_size = dfs_size(fh);
	ASSERT(dfs_file_size >= 0, "Unable to get size of counter.dat");

	int rom_file_size = dfs_rom_size("counter.dat");
	ASSERT_EQUAL_SIGNED(dfs_file_size, rom_file_size, "dfs_rom_size returns a different size from dfs_file_size");
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

typedef struct {
	char path[256];
	int type;
} list_files_data_t;

static int list_files(const char *path, list_files_data_t *files, int *count) {
	*count = 0;
	int list_files(const char *path, dir_t *dir, void *data) {
		list_files_data_t *list = (list_files_data_t *)data;
		strcpy(list[*count].path, path);
		list[*count].type = dir->d_type;
		(*count)++;
		return DIR_WALK_CONTINUE;
	}
	return dir_walk(path, list_files, files);
}

static int list_files_compare(const void *a, const void *b) {
	return strcmp(((list_files_data_t *)a)->path, ((list_files_data_t *)b)->path);
}

void test_dfs_directory(TestContext *ctx) {
	list_files_data_t files[1024];
	int count = 0; int err;

	err = list_files("nofs", files, &count);
	ASSERT(err == -1, "list_files should have failed (ret:%d, errno:%d - %s)", err, errno, strerror(errno));

	err = list_files("nofs:/test_dir", files, &count);
	ASSERT(err == -1, "list_files should have failed (ret:%d, errno:%d - %s)", err, errno, strerror(errno));

	err = list_files("rom:/test_dir", files, &count);
	ASSERT(err == 0, "list_files failed (ret:%d, errno:%d - %s)", err, errno, strerror(errno));

	for (int i=0;i<count;i++) {
		ERR("file %d: %s (type:%d)\n", i, files[i].path, files[i].type);
	}

	ASSERT_EQUAL_SIGNED(count, 7, "test_dir should have 7 files/dirs");
	qsort(files, count, sizeof(list_files_data_t), list_files_compare);

	ASSERT_EQUAL_STR(files[0].path, "rom:/test_dir/file1.txt", "file1.txt should be the first file");
	ASSERT_EQUAL_SIGNED(files[0].type, DT_REG, "file1.txt should be a regular file");
	ASSERT_EQUAL_STR(files[1].path, "rom:/test_dir/subdir", "subdir should be the second file");
	ASSERT_EQUAL_SIGNED(files[1].type, DT_DIR, "subdir should be a directory");
	ASSERT_EQUAL_STR(files[2].path, "rom:/test_dir/subdir/file2.txt", "file2.txt should be the third file");
	ASSERT_EQUAL_SIGNED(files[2].type, DT_REG, "file2.txt should be a regular file");
	ASSERT_EQUAL_STR(files[3].path, "rom:/test_dir/subdir/subempty1", "subempty1 should be the fourth file");
	ASSERT_EQUAL_SIGNED(files[3].type, DT_DIR, "subempty1 should be a directory");
	ASSERT_EQUAL_STR(files[4].path, "rom:/test_dir/subdir/subempty2", "subempty2 should be the fifth file");
	ASSERT_EQUAL_SIGNED(files[4].type, DT_DIR, "subempty2 should be a directory");
	ASSERT_EQUAL_STR(files[5].path, "rom:/test_dir/subdir/subsubdir", "subsubdir should be the sixth file");
	ASSERT_EQUAL_SIGNED(files[3].type, DT_DIR, "subsubdir should be a directory");
	ASSERT_EQUAL_STR(files[6].path, "rom:/test_dir/subdir/subsubdir/file3.txt", "file3.txt should be the seventh file");
	ASSERT_EQUAL_SIGNED(files[6].type, DT_REG, "file3.txt should be a regular file");

}