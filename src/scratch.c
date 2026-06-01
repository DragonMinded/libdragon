/**
 * @file scratch.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Scratch malloc/free allocator for short-lived allocations
 * @ingroup n64sys
 *
 * This is a small first-fit allocator built on top of sbrk_top().
 * It is intentionally simple:
 *   - doubly-linked physical block list;
 *   - arbitrary malloc/free/realloc API;
 *   - no split and no coalescing.
 *
 * Address model:
 *
 *   low addresses                                      high addresses
 *   sh_low                                                   sh_high
 *     |                                                        |
 *     v                                                        v
 *     +--------------------------------------------------------+
 *     | [block][block][block]... (used/free mixed)            |
 *     +--------------------------------------------------------+
 *
 * The full scratch reservation is [sh_low, sh_high). New memory is reserved
 * from sbrk_top(need) and prepended to the block list.
 *
 * A free block is reused only if it is already big enough. Since blocks are
 * never split, this can create internal fragmentation; therefore each block
 * stores both:
 *   - total block size + free flag (size_flags);
 *   - requested user size (requested), used for stats/realloc/debug.
 *
 * Freeing a block does not merge neighbors. Memory can be returned to
 * sbrk_top() only when free blocks are at the head (lowest-address side) of
 * the list, via trim_head().
 *
 * Important invariant: all scratch blocks must remain contiguous. When growing,
 * the allocator asserts that the new range is exactly adjacent to sh_low
 * ("non-contiguous scratch heap"), otherwise internal assumptions break.
 *
 * Returned pointers are SCRATCH_ALIGN-byte aligned (16 bytes).
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "debug.h"
#include "scratch.h"
#include "system_internal.h"
#include "utils.h"

#define SCRATCH_ALIGN           16                ///< Alignment of scratch allocations
#define BLOCK_FREE              ((size_t)1)       ///< Free block flag
#define BLOCK_SIZE_MASK         (~BLOCK_FREE)     ///< Mask for the block size
#define MAGIC_USED              0x53435553        ///< Magic value for used blocks
#define MAGIC_FREE              0x53434652        ///< Magic value for free blocks

/** @brief Block header for the scratch allocator. */
typedef struct block_s {
    size_t size_flags;        ///< Total block size + free flag
    size_t requested;         ///< Requested user size
    struct block_s *prev;     ///< Previous block in the list
    struct block_s *next;     ///< Next block in the list
    uint32_t magic;           ///< Magic value to identify the block type
} block_t;

/** @brief Lowest address reserved for scratch memory. */
static uint8_t *sh_low;
/** @brief Highest address (exclusive) reserved for scratch memory. */
static uint8_t *sh_high;
/** @brief Head of the physical block list. */
static block_t *sh_head;
/** @brief Sum of requested bytes for all currently live allocations. */
static size_t sh_live_bytes;
/** @brief Peak value reached by sh_live_bytes since boot. */
static size_t sh_peak_bytes;
/** @brief Number of currently live allocations. */
static size_t sh_live_blocks;

static size_t header_size(void) { return ROUND_UP(sizeof(block_t), SCRATCH_ALIGN); }
static size_t block_size(const block_t *b) { return b->size_flags & BLOCK_SIZE_MASK; }
static bool block_is_free(const block_t *b) { return (b->size_flags & BLOCK_FREE) != 0; }
static size_t alloc_total_size(size_t req) { return ROUND_UP(header_size() + req, SCRATCH_ALIGN); }
static void *block_user_ptr(block_t *b) { return (uint8_t *)b + header_size(); }
static block_t *block_from_user_ptr(void *p) { return (block_t *)((uint8_t *)p - header_size()); }
static bool ptr_in_heap(const void *p) { return sh_low && p >= (void *)sh_low && p < (void *)sh_high; }

static void block_mark_free(block_t *b) {
    b->size_flags |= BLOCK_FREE;
    b->requested = 0;
    b->magic = MAGIC_FREE;
}

static void block_mark_used(block_t *b, size_t requested) {
    b->size_flags &= ~BLOCK_FREE;
    b->requested = requested;
    b->magic = MAGIC_USED;
}

static block_t *find_fit(size_t need) {
    for (block_t *b = sh_head; b; b = b->next)
        if (block_is_free(b) && block_size(b) >= need)
            return b;
    return NULL;
}

static void trim_head(void) {
    while (sh_head && block_is_free(sh_head)) {
        size_t n = block_size(sh_head);
        sh_head = sh_head->next;
        if (sh_head) sh_head->prev = NULL;
        sh_low += n;
        sbrk_top(-(ptrdiff_t)n);
    }
    if (!sh_head) {
        sh_low = NULL;
        sh_high = NULL;
    }
}

void *scratch_malloc(size_t size) {
    if (!size) size = 1;

    size_t need = alloc_total_size(size);
    block_t *b;
    if (!(b = find_fit(need))) {
        uint8_t *p = sbrk_top((ptrdiff_t)need);
        if (p == (void *)-1) return NULL;
        assertf(!sh_low || p + need == sh_low, "non-contiguous scratch heap");
        assert((((uintptr_t)p) & (SCRATCH_ALIGN - 1)) == 0);

        b = (block_t *)p;
        b->size_flags = need;
        b->prev = NULL;
        b->next = sh_head;
        block_mark_free(b);

        if (sh_head) sh_head->prev = b;
        sh_head = b; 
        sh_low = p;
        if (!sh_high) sh_high = p + need;
    }

    block_mark_used(b, size);
    sh_live_blocks++;
    sh_live_bytes += size;
    if (sh_live_bytes > sh_peak_bytes) sh_peak_bytes = sh_live_bytes;
    return block_user_ptr(b);
}

void scratch_free(void *ptr) {
    if (!ptr) return;

    block_t *b = block_from_user_ptr(ptr);
    assert(ptr_in_heap(b));
    assert(!block_is_free(b));
    assert(b->magic == MAGIC_USED);
    assert(sh_live_blocks > 0);
    assert(sh_live_bytes >= b->requested);

    sh_live_blocks--;
    sh_live_bytes -= b->requested;
    block_mark_free(b);
    if (b == sh_head) trim_head();
}

void *scratch_calloc(size_t count, size_t size) {
    if (count && size > ((size_t)-1) / count) return NULL;

    size_t n = count * size;
    void *p = scratch_malloc(n);
    if (p) memset(p, 0, n);
    return p;
}

void *scratch_realloc(void *ptr, size_t size) {
    if (!ptr) return scratch_malloc(size);
    if (!size) {
        scratch_free(ptr);
        return NULL;
    }

    block_t *b = block_from_user_ptr(ptr);
    assertf(ptr_in_heap(b), "pointer not allocated via scratch_malloc");
    assert(!block_is_free(b));
    assert(b->magic == MAGIC_USED);

    size_t old_requested = b->requested;
    size_t capacity = block_size(b) - header_size();
    if (size <= capacity) {
        sh_live_bytes = sh_live_bytes - old_requested + size;
        b->requested = size;
        return ptr;
    }

    void *q = scratch_malloc(size);
    if (!q) return NULL;
    memcpy(q, ptr, old_requested < size ? old_requested : size);
    scratch_free(ptr);
    return q;
}

void scratch_check(void) {
    if (!sh_head) {
        assert(!sh_low);
        assert(!sh_high);
        assert(sh_live_blocks == 0);
        assert(sh_live_bytes == 0);
        return;
    }

    assert(sh_low && sh_high);
    assert((uint8_t *)sh_head == sh_low);
    assert(sh_low < sh_high);

    size_t live_blocks = 0;
    size_t live_bytes = 0;
    uint8_t *cursor = sh_low;

    for (block_t *b = sh_head; b; b = b->next) {
        size_t sz = block_size(b);
        assert((uint8_t *)b == cursor);
        assert(((uintptr_t)b & (SCRATCH_ALIGN - 1u)) == 0);
        assert((sz & (SCRATCH_ALIGN - 1u)) == 0);
        assert(sz >= alloc_total_size(1));

        if (block_is_free(b)) {
            assert(b->requested == 0);
            assert(b->magic == MAGIC_FREE);
        } else {
            assert(b->magic == MAGIC_USED);
            assert(b->requested <= sz - header_size());
            live_blocks++;
            live_bytes += b->requested;
        }

        if (b->next) assert(b->next->prev == b);
        cursor += sz;
    }

    assert(cursor == sh_high);
    assert(live_blocks == sh_live_blocks);
    assert(live_bytes == sh_live_bytes);
    assert(sh_peak_bytes >= sh_live_bytes);
}

void scratch_get_stats(scratch_stats_t *stats) {
    stats->live_bytes = sh_live_bytes;
    stats->peak_bytes = sh_peak_bytes;
    stats->live_blocks = sh_live_blocks;
    stats->reserved_bytes = sh_low && sh_high ? (size_t)(sh_high - sh_low) : 0;
}

bool scratch_empty(void) {
    return sh_live_blocks == 0;
}
