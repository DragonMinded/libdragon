#include <libdragon.h>
#include <string.h>
#include <stdlib.h>

char *cmpfiles[1024];
int num_files = 0;

// crc32 algorithm
//
// This is a simple implementation of the crc32 algorithm. It is not
// optimized for speed, but it is small and easy to understand.
uint32_t crc32(void *buf, int sz) {
	uint32_t crc = 0xFFFFFFFF;
	uint8_t *p = buf;
	for (int i = 0; i < sz; i++) {
		crc ^= p[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
		}
	}
	return ~crc;
}

static bool strendswith(const char *str, const char *suffix) {
	char *p = strstr(str, suffix);
	return p && p[strlen(suffix)] == '\0';
}

typedef struct {
	const char *fn;
	int size;
	uint32_t full_crc;
	uint32_t partial_crc;
	uint32_t full_us;
	uint32_t partial_us;
} benchmark;

benchmark run_bench(const char *fn) {

	int sz1;
	disable_interrupts();
	uint32_t t0s = get_ticks();
	void *buf1 = asset_load(fn, &sz1);	
	uint32_t t0e = get_ticks();
	enable_interrupts();
	uint32_t crc1 = crc32(buf1, sz1);

	disable_interrupts();
	int sz;
	FILE *f = asset_fopen(fn, &sz);
	void *buf2 = malloc(sz);
	uint32_t t1s = get_ticks();
	fread(buf2, 1, sz, f);
	uint32_t t1e = get_ticks();
	enable_interrupts();
	uint32_t crc2 = crc32(buf2, sz);

	fclose(f);
	free(buf1);
	free(buf2);

	return (benchmark){
		.fn = fn,
		.size = sz,
		.full_crc = crc1,
		.partial_crc = crc2,
		.full_us = TIMER_MICROS(t0e-t0s),
		.partial_us = TIMER_MICROS(t1e-t1s),
	};
}

int file_size(const char *fn) {
	FILE *f = fopen(fn, "rb");
	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	fclose(f);
	return sz;
}

int main(void) {
    debug_init_usblog();
    debug_init_isviewer();

    console_init();
    console_set_debug(true);
    dfs_init(DFS_DEFAULT_LOCATION);
    asset_init_compression(2);

	char sbuf[1024];
	strcpy(sbuf, "rom:/");
	if (dfs_dir_findfirst(".", sbuf+5) == FLAGS_FILE) {
		do {
			if (strendswith(sbuf, ".c0") || strendswith(sbuf, ".c1") || 
				strendswith(sbuf, ".c2") || strendswith(sbuf, ".c3"))
				cmpfiles[num_files++] = strdup(sbuf);
		} while (dfs_dir_findnext(sbuf+5) == FLAGS_FILE);
	}

	// Sort cmpfiles by name
	int sort_name(const void *a, const void *b) {
		return strcmp(*(char **)a, *(char **)b);
	}
	qsort(cmpfiles, num_files, sizeof(char *), sort_name);

    printf("Decompression benchmark: %d files\n", num_files);
	printf("%-28s: %-4s | %-7s | %-5s | %-5s\n", "File", "KiB", "Ratio", "Full", "Partial");

    for (int i=0; i<num_files; i++) {
		char *fn = cmpfiles[i];
		int cmp_size = file_size(fn);

        // if (strendswith(fn, ".c0")) level = 0;
        // if (strendswith(fn, ".c1")) level = 1;
        // if (strendswith(fn, ".c2")) level = 2;
        // if (strendswith(fn, ".c3")) level = 3;

		benchmark b = run_bench(fn);
		float ratio = (float)cmp_size * 100.0f / (float)b.size;

		printf("%-28s: %4d | %6.1f%% | %5.1f | %5.1f\n", fn+5, b.size/1024, ratio, b.full_us / 1000.0f, b.partial_us / 1000.0f);
		// debugf("CRC %08lx %08lx\n", b.full_crc, b.partial_crc);
		if (b.full_crc != b.partial_crc) {
			debugf("CRC mismatch\n");
			return 1;
		}
    }
}
