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
#define PhysicalAddr(p)        ((uint32_t)(uintptr_t)(p) & 0xFFFFFF)
#define VirtualCachedAddr(v)   ((void*)(uintptr_t)((v) & 0xFFFFFF))
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
static inline void* cached_addr(counted_ptr_t v) {
    return VirtualCachedAddr(v & 0xFFFFFF);
}

static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}

/* Returns pointer to key slot: existing key or first free/tombstone slot */
static uint32_t* hashtable_lookup_slot(hashtable_t *h, uint32_t k) {
    size_t mask = h->capacity - 1;  // capacity is power of 2, so this works
    size_t hash = hash32(k);
    uint32_t *tomb_key = NULL;

    for (size_t i = 0; i < h->capacity; i++) {
        size_t idx = ((hash + i) & mask) * 2; // Each slot is 2 elements apart
        uint32_t *kk = &h->entries[idx];
        if (*kk == k) return kk;
        if (*kk == TOMBSTONE_KEY && !tomb_key) tomb_key = kk;
        if (*kk == EMPTY_KEY) return tomb_key ? tomb_key : kk;
    }
    
    // Should never reach here if load factor is kept reasonable
    assert(0 && "hashtable full");
    return NULL;
}

int hashtable_init(hashtable_t *h, size_t initial_entries, hashtable_loader_fn loader) {
    // Round up to next power of 2, ensuring capacity is at least initial_entries
    size_t capacity = 1;
    while (capacity < initial_entries) capacity <<= 1;
    
    h->capacity = capacity;
    // Allocate interleaved key/value array: capacity pairs = capacity * 2 uint32_t
    h->entries = aligned_alloc(16, sizeof(uint32_t) * capacity * 2);
    assert(h->entries);
    memset(h->entries, 0, sizeof(uint32_t) * capacity * 2);
    h->size = 0;
    h->loader = loader;
    return 1;
}

void hashtable_free(hashtable_t *h) {
    free(h->entries);
    h->entries = NULL;
}

static void hashtable_resize(hashtable_t *h, size_t new_capacity) {
    uint32_t *newb = aligned_alloc(16, sizeof(uint32_t) * new_capacity * 2);
    assert(newb);
    memset(newb, 0, sizeof(uint32_t) * new_capacity * 2);

    uint32_t *old = h->entries;
    size_t old_capacity = h->capacity;

    h->entries = newb;
    h->capacity = new_capacity;
    h->size = 0;

    // Rehash all existing entries
    for (size_t i = 0; i < old_capacity * 2; i += 2) {
        uint32_t k = old[i];     // key
        if (k == EMPTY_KEY || k == TOMBSTONE_KEY) continue;
        counted_ptr_t v = old[i + 1]; // value

        uint32_t *kk = hashtable_lookup_slot(h, k);
        *kk = k;
        kk[1] = v; // value is immediately after key
        h->size++;
    }
    free(old);
}

void* hashtable_insert(hashtable_t *h, uint32_t k, void *value) {
    assert(k != EMPTY_KEY && k != TOMBSTONE_KEY);

    // Resize when load factor exceeds 75%
    size_t threshold = h->capacity * 3 / 4;
    if (h->size > threshold) hashtable_resize(h, h->capacity * 2);

    uint32_t *kk = hashtable_lookup_slot(h, k);
    counted_ptr_t *vv = (counted_ptr_t*)(kk + 1); // value is immediately after key

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
    return cached_addr(*vv);
}

void* hashtable_lookup(hashtable_t *h, uint32_t k) {
    assert(k != EMPTY_KEY && k != TOMBSTONE_KEY);
    uint32_t *kk = hashtable_lookup_slot(h, k);
    return (*kk == k) ? cached_addr(kk[1]) : NULL; // value is immediately after key
}

void* hashtable_remove(hashtable_t *h, uint32_t k) {
    assert(k != EMPTY_KEY && k != TOMBSTONE_KEY);
    size_t mask = h->capacity - 1;  // capacity is power of 2, so this works
    size_t hash = hash32(k);

    for (size_t i = 0; i < h->capacity; i++) {
        size_t idx = ((hash + i) & mask) * 2; // Each slot is 2 elements apart
        uint32_t *kk = &h->entries[idx];
        if (*kk == k) {
            uint8_t r = refcount(kk[1]); // value immediately after key
            void *v = cached_addr(kk[1]);
            if (--r > 0) { set_refcount(&kk[1], r); return NULL; }
            kk[0] = TOMBSTONE_KEY; // mark key as tombstone
            kk[1] = 0;             // clear value
            h->size--;
            return v;
        }
        if (*kk == EMPTY_KEY) return NULL; // Key not found
    }
    return NULL; // Key not found
}

void hashtable_visit(hashtable_t *h, void (*visitor)(uint32_t key, void *value, int refcount)) {
    for (size_t i = 0; i < h->capacity * 2; i += 2) {
        uint32_t k = h->entries[i];
        if (k == EMPTY_KEY || k == TOMBSTONE_KEY) continue;
        counted_ptr_t v = h->entries[i + 1];
        visitor(k, cached_addr(v), refcount(v));
    }
}
