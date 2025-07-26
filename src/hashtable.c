/**
 * @file hashtable.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hashtable_internal.h"

#ifdef N64
#include "n64sys.h"
#else
/// @cond
#define PhysicalAddr(p) ((uint32_t)(uintptr_t)(p) & 0xFFFFFF)
#define CachedAddr(v)   ((void*)(uintptr_t)((v) & 0xFFFFFF))
/// @endcond
#endif

#define EMPTY_KEY     0                 ///< Key that represents an empty slot
#define TOMBSTONE_KEY 0xFFFFFFFF        ///< Key that represents a tombstone (deleted key)

/** Tagged pointer containing also a 8-bit reference counter */
typedef uint32_t counted_ptr_t; // [31..24]=refcount, [23..0]=PhysicalAddr

static inline uint8_t refcount(counted_ptr_t v) { return v >> 24; }
static inline void set_refcount(counted_ptr_t *v, uint8_t r) {
    *v = (*v & 0xFFFFFF) | ((uint32_t)r << 24);
}

/** Bucket structure for the hashtable */
typedef struct __attribute__((aligned(16))) {
    uint32_t kv[8]; // [0..3]=keys, [4..7]=vals
} Bucket;

/** Loader function type */
typedef void* (*loader_fn)(uint32_t key);

/** @brief Opaque hashtable type */
typedef struct hashtable_s {
    Bucket    *buckets;
    size_t     n_buckets, size;
    loader_fn  loader;
} hashtable_t;

static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}

/* Returns pointer to key slot: existing key or first free/tombstone slot */
static uint32_t* hashtable_lookup_slot(hashtable_t *h, uint32_t k) {
    size_t mask = h->n_buckets - 1;
    size_t b = hash32(k) & mask;
    uint32_t *tomb_key = NULL;

    for (;;) {
        Bucket *bk = &h->buckets[b];
        for (int i = 0; i < 4; i++) {
            uint32_t *kk = &bk->kv[i];
            if (*kk == k) return kk;
            if (*kk == TOMBSTONE_KEY && !tomb_key) tomb_key = kk;
            if (!*kk) return tomb_key ? tomb_key : kk;
        }
        b = (b + 1) & mask;
    }
}

int hashtable_init(hashtable_t *h, size_t initial_entries, loader_fn loader) {
    size_t n = 1; while (n * 4 < initial_entries) n <<= 1;
    h->n_buckets = n;
    h->buckets = aligned_alloc(16, sizeof(Bucket) * n);
    assert(h->buckets);
    memset(h->buckets, 0, sizeof(Bucket) * n);
    h->size = 0;
    h->loader = loader;
    return 1;
}

void hashtable_free(hashtable_t *h) {
    free(h->buckets);
    h->buckets = NULL;
}

static int hashtable_resize(hashtable_t *h, size_t new_buckets) {
    Bucket *newb = aligned_alloc(16, sizeof(Bucket) * new_buckets);
    assert(newb);
    memset(newb, 0, sizeof(Bucket) * new_buckets);

    Bucket *old = h->buckets;
    size_t old_n = h->n_buckets;

    h->buckets = newb;
    h->n_buckets = new_buckets;
    h->size = 0;

    for (size_t i = 0; i < old_n; i++) {
        for (int j = 0; j < 4; j++) {
            uint32_t k = old[i].kv[j];
            if (!k || k == TOMBSTONE_KEY) continue;
            counted_ptr_t v = old[i].kv[j+4];

            uint32_t *kk = hashtable_lookup_slot(h, k);
            *kk = k;
            kk[4] = v;
            h->size++;
        }
    }
    free(old);
    return 1;
}

void hashtable_insert(hashtable_t *h, uint32_t k, void *value) {
    size_t threshold = (h->n_buckets << 2) * 12 / 16;
    if (h->size > threshold) {
        int ok = hashtable_resize(h, h->n_buckets * 2);
        assert(ok);
    }

    uint32_t *kk = hashtable_lookup_slot(h, k);
    counted_ptr_t *vv = (counted_ptr_t*)(kk + 4);

    if (*kk == k) {
        uint8_t r = refcount(*vv); assert(r < 255);
        if (value) assert(PhysicalAddr(value) == (*vv & 0xFFFFFF));
        set_refcount(vv, r + 1);
    } else {
        void *p = value ? value : (h->loader ? h->loader(k) : NULL);
        *kk = k;
        *vv = (1u << 24) | PhysicalAddr(p);
        h->size++;
    }
}

void* hashtable_lookup(hashtable_t *h, uint32_t k) {
    uint32_t *kk = hashtable_lookup_slot(h, k);
    return (*kk == k) ? CachedAddr(kk[4]) : NULL;
}

int hashtable_remove(hashtable_t *h, uint32_t k) {
    size_t mask = h->n_buckets - 1;
    size_t b = hash32(k) & mask;

    for (;;) {
        Bucket *bk = &h->buckets[b];
        for (int i = 0; i < 4; i++) {
            if (bk->kv[i] == k) {
                uint8_t r = refcount(bk->kv[i+4]);
                if (--r == 0) { bk->kv[i] = TOMBSTONE_KEY; bk->kv[i+4] = 0; h->size--; }
                else set_refcount(&bk->kv[i+4], r);
                return 1;
            }
            if (!bk->kv[i]) return 0;
        }
        b = (b + 1) & mask;
    }
}
