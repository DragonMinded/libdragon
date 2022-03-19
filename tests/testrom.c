#include <libdragon.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Activate this when running under emulators such as cen64
#ifndef IN_EMULATOR
#define IN_EMULATOR  0
#endif

/**********************************************************************
 * SIMPLE TEST FRAMEWORK
 **********************************************************************/

#define TEST_SUCCESS  0
#define TEST_FAILED   1
#define TEST_SKIPPED  2

typedef struct {
	int result;
	char *log;
	int logleft;
} TestContext;

typedef void (*TestFunc)(TestContext *ctx);

#define PPCAT2(n,x) n ## x
#define PPCAT(n,x) PPCAT2(n,x)

// LOG(msg, ...): log something that will be displayed if the test fails.
#define LOG(msg, ...)  ({ \
	int __n = snprintf(ctx->log, ctx->logleft, msg, ##__VA_ARGS__); \
	fwrite(ctx->log, 1, __n, stderr); \
	ctx->log += __n; ctx->logleft -= __n; \
})

// DEFER(stmt): execute "stmt" statement when the current lexical block exits.
// This is useful in tests to execute cleanup functions even if the test fails
// through ASSERT macros.
#define DEFER(stmt) \
	void PPCAT(__cleanup, __LINE__) (int* u) { stmt; } \
	int PPCAT(__var, __LINE__) __attribute__((unused, cleanup(PPCAT(__cleanup, __LINE__ ))));

// SKIP: skip execution of the test.
#define SKIP(msg, ...) ({ \
	LOG("TEST SKIPPED:\n"); \
	LOG(msg "\n", ##__VA_ARGS__); \
	ctx->result = TEST_SKIPPED; \
	return; \
})

// Fair and fast random generation (using xorshift32, with explicit seed)
static uint32_t rand_state = 1;
static uint32_t rand(void) {
	uint32_t x = rand_state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 5;
	return rand_state = x;
}

// RANDN(n): generate a random number from 0 to n-1
#define RANDN(n) ({ \
	__builtin_constant_p((n)) ? \
		(rand()%(n)) : \
		(uint32_t)(((uint64_t)rand() * (n)) >> 32); \
})

// ASSERT(cond, msg): fail the test if the condition is false (with log message)
#define ASSERT(cond, msg, ...) ({ \
	if (!(cond)) { \
		LOG("ASSERTION FAILED (%s:%d):\n", __FILE__, __LINE__); \
		LOG("%s\n", #cond); \
		LOG(msg "\n", ##__VA_ARGS__); \
		ctx->result = TEST_FAILED; \
		return; \
	} \
})

// ASSERT_EQUAL_HEX(a, b, msg): fail the test if a!=b (and log a & b as hex values)
#define ASSERT_EQUAL_HEX(_a, _b, msg, ...) ({ \
	uint64_t a = _a; uint64_t b = _b; \
	if (a != b) { \
		LOG("ASSERTION FAILED (%s:%d):\n", __FILE__, __LINE__); \
		LOG("%s != %s (0x%llx != 0x%llx)\n", #_a, #_b, a, b); \
		LOG(msg "\n", ##__VA_ARGS__); \
		ctx->result = TEST_FAILED; \
		return; \
	} \
})

// ASSERT_EQUAL_UNSIGNED(a,b, msg): fail the test if a!=b (and log a & b as
// unsigned values)
#define ASSERT_EQUAL_UNSIGNED(_a, _b, msg, ...) ({ \
	uint64_t a = _a; uint64_t b = _b; \
	if (a != b) { \
		LOG("ASSERTION FAILED (%s:%d):\n", __FILE__, __LINE__); \
		LOG("%s != %s (%llu != %llu)\n", #_a, #_b, a, b); \
		LOG(msg "\n", ##__VA_ARGS__); \
		ctx->result = TEST_FAILED; \
		return; \
	} \
})

// ASSERT_EQUAL_SIGNED(a, b, msg): fail the test if a!=b (and log a/b as signed values)
#define ASSERT_EQUAL_SIGNED(_a, _b, msg, ...) ({ \
	int64_t a = _a; int64_t b = _b; \
	if (a != b) { \
		LOG("ASSERTION FAILED (%s:%d):\n", __FILE__, __LINE__); \
		LOG("%s != %s (%lld != %lld)\n", #_a, #_b, a, b); \
		LOG(msg "\n", ##__VA_ARGS__); \
		ctx->result = TEST_FAILED; \
		return; \
	} \
})

void hexdump(char *out, const uint8_t *buf, int buflen, int start, int count) {
	for (int i=start;i<start+count;i++) {
		if (i >= 0 && i < buflen) {
			sprintf(out, "%02x", buf[i]);
			out += 2;
		} else {
			*out++ = '-'; *out++ = '-';
		}
	}	
	*out = '\0';
}

int assert_equal_mem(TestContext *ctx, const char *file, int line, const uint8_t *a, const uint8_t *b, int len) {
	for (int i=0;i<len;i++) {
		if (a[i] != b[i]) {
			char dumpa[64];
			char dumpb[64];
			hexdump(dumpa, a, len, i-2, 5);
			hexdump(dumpb, b, len, i-2, 5);

			LOG("ASSERTION FAILED (%s:%d):\n", file, line); \
			LOG("[%s] != [%s]\n", dumpa, dumpb);
			LOG("     ^^              ^^  idx: %d\n", i);
			return 0;
		}
	}
	return 1;
}

// ASSERT_EQUAL_MEM(a, b, len, msg): fail the test if the memory pointer by
// a and b has not the same content (up to len bytes).
#define ASSERT_EQUAL_MEM(_a, _b, _len, msg, ...) ({ \
	const uint8_t *a = (_a); const uint8_t *b = (_b); int len = (_len); \
	if (!assert_equal_mem(ctx, __FILE__, __LINE__, a, b, len)) { \
		LOG(msg "\n", ##__VA_ARGS__); \
		ctx->result = TEST_FAILED; \
		return; \
	} \
})

/**********************************************************************
 * TEST FILES
 **********************************************************************/

#include "test_dfs.c"
#include "test_eepromfs.c"
#include "test_cache.c"
#include "test_ticks.c"
#include "test_timer.c"
#include "test_irq.c"
#include "test_exception.c"
#include "test_debug.c"
#include "test_dma.c"
#include "test_cop1.c"
#include "test_constructors.c"

/**********************************************************************
 * MAIN
 **********************************************************************/

// Testsuite definition
#define TEST_FLAGS_NONE          0x0
#define TEST_FLAGS_IO            0x1  // Test uses I/O, so timing depends on ROM hardware
#define TEST_FLAGS_NO_BENCHMARK  0x2  // Test is too variable, do not attempt to benchmark it
#define TEST_FLAGS_RESET_COUNT   0x4  // Test resets the hardware count register
#define TEST_FLAGS_NO_EMULATOR   0x8  // Test does not work under emulators

#define TEST_FUNC(fn, dur, flags)   { #fn, fn, dur, flags }
static const struct Testsuite
{
	const char *name;
	TestFunc fn;
	uint32_t duration;
	uint32_t flags;
} tests[] = {
	TEST_FUNC(test_exception,                  5, TEST_FLAGS_NO_BENCHMARK),
	TEST_FUNC(test_constructors,               0, TEST_FLAGS_NONE),
	TEST_FUNC(test_ticks,                      0, TEST_FLAGS_NO_BENCHMARK | TEST_FLAGS_NO_EMULATOR),
	TEST_FUNC(test_timer_ticks,              292, TEST_FLAGS_NO_BENCHMARK),
	TEST_FUNC(test_timer_oneshot,            596, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_timer_slow_callback,     1468, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_timer_continuous,         688, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_timer_mixed,             1467, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_timer_context,            186, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_timer_disabled_start,     733, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_timer_disabled_restart,   733, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_irq_reentrancy,           230, TEST_FLAGS_RESET_COUNT),
	TEST_FUNC(test_dfs_read,                 948, TEST_FLAGS_IO),
	TEST_FUNC(test_dfs_rom_addr,              25, TEST_FLAGS_IO),
	TEST_FUNC(test_eepromfs,                   0, TEST_FLAGS_IO),
	TEST_FUNC(test_cache_invalidate,        1763, TEST_FLAGS_NONE),
	TEST_FUNC(test_debug_sdfs,                 0, TEST_FLAGS_NO_BENCHMARK),
	TEST_FUNC(test_dma_read_misalign,       7003, TEST_FLAGS_NONE),
	TEST_FUNC(test_cop1_denormalized_float,    0, TEST_FLAGS_NO_EMULATOR),
};

int main() {
	display_init(RESOLUTION_320x240, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
	console_init();
	console_set_debug(false);
	debug_init_isviewer();
	debug_init_usblog();

	if (dfs_init( DFS_DEFAULT_LOCATION ) != DFS_ESUCCESS) {
		printf("Invalid ROM: cannot initialize DFS\n");
		return 0;
	}

	printf("libdragon testsuite (%s)\n\n", sys_bbplayer() ? "iQue" : "N64");
	int failures = 0;
	int successes = 0;
	int skipped = 0;

	const int NUM_TESTS = sizeof(tests) / sizeof(tests[0]);
	uint32_t start = TICKS_READ();
	for (int i=0; i < NUM_TESTS; i++) {
		static char logbuf[16384];

		printf("%-59s", tests[i].name);
		fflush(stdout);
		debugf("**** Starting test: %s\n", tests[i].name);

		// Skip the test if we're running under emulation and the test is
		// not compatible with emulators by design (eg: too strict timing).
		if (IN_EMULATOR && tests[i].flags & TEST_FLAGS_NO_EMULATOR) {
			skipped++;
			printf("SKIP\n");
			debugf("SKIP\n");			
			continue;
		}

		// Prepare the test context
		TestContext ctx;
		ctx.log = logbuf;
		ctx.logleft = sizeof(logbuf);
		ctx.result = TEST_SUCCESS;
		rand_state = 1; // reset to be fully reproducible

		// Do a complete cache flush before running each test
		data_cache_writeback_invalidate_all();
		inst_cache_invalidate_all();

		uint32_t test_start = TICKS_READ();

		// Run the test!
		tests[i].fn(&ctx);

		// Compute the test duration
		uint32_t test_stop = TICKS_READ();

		// If the test reset the hardware counter, just consider its timing
		// as relative to 0, so move test_stop to realign, and update the
		// hardware counter as well.
		if (tests[i].flags & TEST_FLAGS_RESET_COUNT) {
			test_stop += test_start;
            C0_WRITE_COUNT(test_stop);
		}

		int32_t test_duration = TICKS_DISTANCE(test_start, test_stop) / 1024;
		int32_t test_diff = (test_duration - tests[i].duration);
		if (test_diff < 0) test_diff = -test_diff;

		if (ctx.result == TEST_FAILED) {
			failures++;
			printf("FAIL\n\n");

			if (ctx.log != logbuf) {
				printf("%s\n\n", logbuf);
			}
		} else if (ctx.result == TEST_SKIPPED) {
			skipped++;
			printf("SKIP\n");
			debugf("SKIP\n");
		}

		// If there's more than a 5% (10% for IO tests) drift on the running time
		// (/1024) compared to the expected one, make the test fail. Something
		// happened and we need to double check this.
		// In general, this benchmarking is extremely hard to get right for
		// emulators, so don't even attempt it because we would get too many failures.
		else if (!IN_EMULATOR && !sys_bbplayer() &&
			!(tests[i].flags & TEST_FLAGS_NO_BENCHMARK) &&
			((float)test_diff / (float)test_duration > ((tests[i].flags & TEST_FLAGS_IO) ? 0.1f : 0.05f))
		) {
			failures++;
			printf("FAIL\n\n");
			debugf("TIMING FAIL\n");

			printf("Duration changed by %.1f%%\n", (float)test_diff * 100.0 / (float)test_duration);
			printf("(expected: %ldK, measured: %ldK)\n\n", tests[i].duration, test_duration);
		} else {
			successes++;
			printf("PASS\n");
		}
	}
	uint32_t stop = TICKS_READ();

	int64_t total_time = TIMER_MICROS(stop-start) / 1000000;

	console_set_debug(true);
	printf("\nTestsuite finished in %02lld:%02lld\n", total_time/60, total_time%60);
	printf("Passed: %d out of %d (%d skipped)\n", successes, NUM_TESTS, skipped);
}
