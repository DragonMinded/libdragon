
#define ROM_FILE "rom:/random.dat"
#define SD_FILE  "sd:/random.dat"

#include <sys/stat.h>
#include <unistd.h>

void test_debug_sdfs(TestContext *ctx) {

	if (!debug_init_sdfs("sd:/", -1)) {
		SKIP("no SD support");
		return;
	}
	DEFER(debug_close_sdfs());

	struct stat info; int ret;
	uint8_t random[8*1024];
	FILE *randf = NULL;
	DEFER(if (randf) fclose(randf));

	// Read file from ROM
	randf = fopen(ROM_FILE, "rb");
	ASSERT(randf, "cannot open file: rom:/%s", ROM_FILE);
	int sz = fread(random, 1, sizeof(random), randf);
	ASSERT_EQUAL_UNSIGNED(sz, sizeof(random), "cannot read enough data");
	fclose(randf); randf = NULL;

	// Write file to SD.
	randf = fopen(SD_FILE, "wb");
	ASSERT(randf, "cannot create file: sd:/%s", SD_FILE);

	// Write random-sized chunks of 1-25 bytes to stress the underlying
	// layer with small unaligned writes. Also disable stdio caching.
	setvbuf(randf, NULL, _IONBF, 0);
	uint8_t *wp = random;
	while (wp < random+sizeof(random)) {
		int left = random+sizeof(random)-wp;
		int n = RANDN(left < 25 ? left : 25) + 1;
		sz = fwrite(wp, 1, n, randf);
		ASSERT_EQUAL_UNSIGNED(n, sz, "invalid write size");
		wp += n;
	}
	fclose(randf); randf = NULL;

	// Verify file metadata (size and attribs)
	ret = stat(SD_FILE, &info);
	ASSERT_EQUAL_SIGNED(ret, 0, "stat failed");
	ASSERT_EQUAL_UNSIGNED(info.st_size, 8*1024, "invalid file size");
	ASSERT(!(info.st_mode & S_IFDIR), "file erroneously marked as directory");

	// Append to the file
	randf = fopen(SD_FILE, "a");
	ASSERT(randf, "cannot append to file: sd:/%s", SD_FILE);
	sz = fwrite(random, 1, 1024, randf);
	ASSERT_EQUAL_UNSIGNED(1024, sz, "invalid write size");
	fclose(randf); randf = NULL;

	// Verify file metadata (size and attribs)
	ret = stat(SD_FILE, &info);
	ASSERT_EQUAL_SIGNED(ret, 0, "stat failed");
	ASSERT_EQUAL_UNSIGNED(info.st_size, 9*1024, "invalid file size");
	ASSERT(!(info.st_mode & S_IFDIR), "file erroneously marked as directory");

	uint8_t read[8*1024];

	// Do large unbuffered reads to test continuous read/writes
	randf = fopen(SD_FILE, "wb");
	setvbuf(randf, NULL, _IONBF, 0);
	fwrite(random, 1024, 8, randf);
	fclose(randf); randf = NULL;

	randf = fopen(SD_FILE, "r");
	setvbuf(randf, NULL, _IONBF, 0);
	fread(read, 1024, 8, randf);
	fclose(randf); randf = NULL;

	ASSERT_EQUAL_MEM(read,
		random,
		8*1024, "Invalid re-read");

	// Remove the file
	ret = unlink(SD_FILE);
	ASSERT_EQUAL_SIGNED(ret, 0, "unlink failed");

	// Verify the file doesn't exist anymore
	randf = fopen(SD_FILE, "rb");
	ASSERT(!randf, "file can be opened after unlink?");
}

#undef ROM_FILE
#undef SD_FILE
