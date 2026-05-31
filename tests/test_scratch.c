#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include "../include/scratch.h"

static void scratch_fill_pattern(uint8_t *buf, size_t size, uint8_t seed) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

static void scratch_assert_pattern(TestContext *ctx, const uint8_t *buf, size_t size, uint8_t seed, const char *what) {
    for (size_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)(seed + (uint8_t)i);
        ASSERT_EQUAL_HEX(buf[i], expected, "%s: mismatch at offset %u", what, (unsigned)i);
    }
}

static void scratch_assert_invariants(TestContext *ctx, const char *where) {
    scratch_stats_t st;
    scratch_get_stats(&st);

    ASSERT((scratch_empty() && st.live_blocks == 0) || (!scratch_empty() && st.live_blocks != 0),
        "%s: scratch_empty() and live_blocks disagree", where);
    ASSERT(st.peak_bytes >= st.live_bytes, "%s: peak_bytes < live_bytes", where);

    scratch_check();
}

static void scratch_reset_for_test(TestContext *ctx) {
    scratch_stats_t st;
    scratch_get_stats(&st);
    ASSERT(scratch_empty(), "scratch not empty at test start (live_blocks=%u)", (unsigned)st.live_blocks);
    if (!scratch_empty()) return;
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 0, "live_bytes should reset to zero");
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 0, "live_blocks should reset to zero");
    ASSERT_EQUAL_UNSIGNED(st.reserved_bytes, 0, "reserved_bytes should reset to zero");
}

void test_scratch_basics(TestContext *ctx) {
    scratch_reset_for_test(ctx);

    scratch_stats_t before, after;
    scratch_get_stats(&before);
    scratch_free(NULL);
    scratch_get_stats(&after);
    ASSERT_EQUAL_UNSIGNED(after.live_bytes, before.live_bytes, "scratch_free(NULL) changed live_bytes");
    ASSERT_EQUAL_UNSIGNED(after.live_blocks, before.live_blocks, "scratch_free(NULL) changed live_blocks");
    ASSERT_EQUAL_UNSIGNED(after.reserved_bytes, before.reserved_bytes, "scratch_free(NULL) changed reserved_bytes");

    const size_t sizes[] = { 1, 15, 16, 17, 1024 };
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        uint8_t *p = scratch_malloc(sizes[i]);
        ASSERT(p != NULL, "scratch_malloc(%u) failed", (unsigned)sizes[i]);
        ASSERT_EQUAL_UNSIGNED(((uintptr_t)p & 15u), 0, "pointer is not 16-byte aligned");
        scratch_fill_pattern(p, sizes[i], (uint8_t)(0x10 + i));
        scratch_assert_pattern(ctx, p, sizes[i], (uint8_t)(0x10 + i), "basic fill/read");
        scratch_free(p);
        scratch_assert_invariants(ctx, "after aligned malloc/free");
    }

    uint8_t *p0 = scratch_malloc(0);
    ASSERT(p0 != NULL, "scratch_malloc(0) returned NULL");
    scratch_get_stats(&after);
    ASSERT_EQUAL_UNSIGNED(after.live_blocks, 1, "scratch_malloc(0) should allocate one block");
    ASSERT_EQUAL_UNSIGNED(after.live_bytes, 1, "scratch_malloc(0) should account one byte");
    scratch_free(p0);

    uint8_t *p = scratch_malloc(128);
    ASSERT(p != NULL, "scratch_malloc(128) failed");
    scratch_fill_pattern(p, 128, 0x42);
    scratch_assert_pattern(ctx, p, 128, 0x42, "roundtrip pattern");
    scratch_free(p);

    scratch_get_stats(&after);
    ASSERT(scratch_empty(), "scratch should be empty after roundtrip");
    ASSERT_EQUAL_UNSIGNED(after.live_bytes, 0, "live_bytes should be zero");
    ASSERT_EQUAL_UNSIGNED(after.live_blocks, 0, "live_blocks should be zero");
    ASSERT_EQUAL_UNSIGNED(after.reserved_bytes, 0, "reserved_bytes should be zero");
}

void test_scratch_calloc(TestContext *ctx) {
    scratch_reset_for_test(ctx);

    const size_t count = 64;
    const size_t elem = 4;
    uint8_t *p = scratch_calloc(count, elem);
    ASSERT(p != NULL, "scratch_calloc(%u,%u) failed", (unsigned)count, (unsigned)elem);
    for (size_t i = 0; i < count * elem; i++) {
        ASSERT_EQUAL_HEX(p[i], 0, "scratch_calloc not zeroed at offset %u", (unsigned)i);
    }

    scratch_stats_t st;
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, count * elem, "calloc live_bytes mismatch");
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 1, "calloc live_blocks mismatch");
    scratch_free(p);

    scratch_stats_t before, after;
    scratch_get_stats(&before);
    void *ov = scratch_calloc(SIZE_MAX, 2);
    ASSERT(ov == NULL, "scratch_calloc overflow should return NULL");
    scratch_get_stats(&after);
    ASSERT_EQUAL_UNSIGNED(after.live_bytes, before.live_bytes, "overflow changed live_bytes");
    ASSERT_EQUAL_UNSIGNED(after.live_blocks, before.live_blocks, "overflow changed live_blocks");
    ASSERT_EQUAL_UNSIGNED(after.reserved_bytes, before.reserved_bytes, "overflow changed reserved_bytes");
}

void test_scratch_stats_and_peak(TestContext *ctx) {
    scratch_reset_for_test(ctx);

    scratch_stats_t st_start;
    scratch_get_stats(&st_start);
    size_t expected_peak = st_start.peak_bytes > 300 ? st_start.peak_bytes : 300;

    void *a = scratch_malloc(100);
    void *b = scratch_malloc(200);
    ASSERT(a && b, "scratch_malloc failed in stats test");

    scratch_stats_t st;
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 300, "live_bytes after two allocations");
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 2, "live_blocks after two allocations");
    ASSERT_EQUAL_UNSIGNED(st.peak_bytes, expected_peak, "peak_bytes after two allocations");
    ASSERT(st.reserved_bytes >= st.live_bytes, "reserved_bytes should include live_bytes");

    scratch_free(b);
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 100, "live_bytes after free(b)");
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 1, "live_blocks after free(b)");
    ASSERT_EQUAL_UNSIGNED(st.peak_bytes, expected_peak, "peak_bytes should not decrease after free(b)");

    scratch_free(a);
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 0, "live_bytes after free(a)");
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 0, "live_blocks after free(a)");
    ASSERT_EQUAL_UNSIGNED(st.peak_bytes, expected_peak, "peak_bytes should keep max value");
    ASSERT_EQUAL_UNSIGNED(st.reserved_bytes, 0, "reserved_bytes should collapse to zero");
}

void test_scratch_collapse_ordering(TestContext *ctx) {
    scratch_reset_for_test(ctx);

    void *a = scratch_malloc(128);
    void *b = scratch_malloc(256);
    void *c = scratch_malloc(384);
    ASSERT(a && b && c, "scratch_malloc failed in collapse test");

    scratch_stats_t st;
    scratch_get_stats(&st);
    size_t reserved_after_alloc = st.reserved_bytes;
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 3, "expected 3 live blocks");
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 128 + 256 + 384, "live_bytes after A/B/C alloc");

    scratch_free(b);
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 2, "live_blocks after free(B)");
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 128 + 384, "live_bytes after free(B)");

    scratch_free(c);
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 1, "live_blocks after free(C)");
    ASSERT_EQUAL_UNSIGNED(st.live_bytes, 128, "live_bytes after free(C)");
    ASSERT(st.reserved_bytes < reserved_after_alloc,
        "reserved_bytes should decrease after trimming free head chain");
    ASSERT(st.reserved_bytes > 0, "reserved_bytes should stay >0 while A is live");

    scratch_free(a);
    scratch_get_stats(&st);
    ASSERT(scratch_empty(), "scratch should be empty after free(A)");
    ASSERT_EQUAL_UNSIGNED(st.reserved_bytes, 0, "reserved_bytes should return to zero");
    scratch_assert_invariants(ctx, "after collapse ordering");
}

void test_scratch_realloc(TestContext *ctx) {
    scratch_reset_for_test(ctx);

    uint8_t *p = scratch_realloc(NULL, 64);
    ASSERT(p != NULL, "scratch_realloc(NULL,64) failed");
    scratch_fill_pattern(p, 64, 0x31);
    p = scratch_realloc(p, 0);
    ASSERT(p == NULL, "scratch_realloc(ptr,0) should return NULL");
    ASSERT(scratch_empty(), "realloc(ptr,0) should free allocation");

    uint8_t *upper = scratch_malloc(64);
    uint8_t *lower = scratch_malloc(64);
    ASSERT(upper && lower, "malloc failed in realloc grow test");
    scratch_fill_pattern(lower, 64, 0x55);
    scratch_free(upper);
    uint8_t *grown = scratch_realloc(lower, 96);
    ASSERT(grown != NULL, "realloc grow failed");
    scratch_assert_pattern(ctx, grown, 64, 0x55, "realloc grow preserves prefix");

    uint8_t *shrunk = scratch_realloc(grown, 32);
    ASSERT(shrunk != NULL, "realloc shrink failed");
    scratch_assert_pattern(ctx, shrunk, 32, 0x55, "realloc shrink preserves prefix");
    scratch_free(shrunk);
    ASSERT(scratch_empty(), "scratch should be empty after grow/shrink sequence");
    scratch_stats_t mid;
    scratch_get_stats(&mid);
    ASSERT_EQUAL_UNSIGNED(mid.reserved_bytes, 0, "scratch should have no reserved bytes after grow/shrink sequence");
    scratch_check();

    uint8_t *x = scratch_malloc(128);
    uint8_t *y = scratch_malloc(128);
    ASSERT(x && y, "malloc failed in realloc relocate test");
    scratch_fill_pattern(y, 128, 0x72);
    uint8_t *new_y = scratch_realloc(y, 512);
    ASSERT(new_y != NULL, "realloc relocate grow failed");
    scratch_assert_pattern(ctx, new_y, 128, 0x72, "realloc relocate preserves data");
    scratch_check();
    scratch_free(new_y);
    scratch_check();
    scratch_free(x);

    uint8_t *z = scratch_malloc(256);
    ASSERT(z != NULL, "malloc failed before realloc failure test");
    scratch_fill_pattern(z, 256, 0x93);
    void *z2 = scratch_realloc(z, (size_t)(32u * 1024u * 1024u));
    ASSERT(z2 == NULL, "realloc to huge size should fail");
    scratch_assert_pattern(ctx, z, 256, 0x93, "realloc failure keeps original data");
    scratch_free(z);

    scratch_stats_t st;
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 0, "realloc test should end with no live blocks");
    ASSERT_EQUAL_UNSIGNED(st.reserved_bytes, 0, "realloc test should fully collapse");
}

void test_scratch_exhaustion_recovery(TestContext *ctx) {
    scratch_reset_for_test(ctx);

    uint8_t *big = scratch_malloc(70000);
    ASSERT(big != NULL, "large allocation (>64KiB) failed");
    big[0] = 0xAA;
    big[69999] = 0x55;
    ASSERT_EQUAL_HEX(big[0], 0xAA, "large allocation first byte mismatch");
    ASSERT_EQUAL_HEX(big[69999], 0x55, "large allocation last byte mismatch");
    scratch_free(big);
    ASSERT(scratch_empty(), "scratch should be empty after freeing big allocation");

    enum { MAX_PTRS = 512 };
    void *ptrs[MAX_PTRS];
    memset(ptrs, 0, sizeof(ptrs));

    int count = 0;
    bool hit_null = false;
    for (int i = 0; i < MAX_PTRS; i++) {
        ptrs[i] = scratch_malloc(64 * 1024);
        if (!ptrs[i]) {
            hit_null = true;
            break;
        }
        count++;
    }
    ASSERT(hit_null, "exhaustion loop did not hit NULL within %d allocations", MAX_PTRS);
    ASSERT(count > 0, "exhaustion loop should allocate at least one block");

    for (int i = count - 1; i >= 0; i--) {
        scratch_free(ptrs[i]);
    }

    scratch_stats_t st;
    scratch_get_stats(&st);
    ASSERT_EQUAL_UNSIGNED(st.live_blocks, 0, "live_blocks should be zero after recovery");
    ASSERT_EQUAL_UNSIGNED(st.reserved_bytes, 0, "reserved_bytes should be zero after recovery");

    void *p = scratch_malloc(256);
    ASSERT(p != NULL, "allocation after recovery failed");
    scratch_free(p);
}
