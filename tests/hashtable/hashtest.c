#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "../../src/hashtable.c"

void* my_loader(uint32_t key) {
    return (void*)(uintptr_t)(0x1000 + key);
}

/* Test data for hashtable_visit */
typedef struct {
    uint32_t expected_keys[10];
    int expected_refcounts[10];
    bool found_keys[10];
    size_t expected_count;
    size_t actual_count;
    bool error;
} visit_data_t;

static visit_data_t g_visit_data;

void visit_collector(uint32_t key, void *value, int refcount) {
    g_visit_data.actual_count++;
    
    // Check if this key is expected
    bool found = false;
    for (size_t i = 0; i < g_visit_data.expected_count; i++) {
        if (g_visit_data.expected_keys[i] == key) {
            if (g_visit_data.expected_refcounts[i] != refcount) {
                printf("[ERROR] Key %u has refcount %d, expected %d\n", key, refcount, g_visit_data.expected_refcounts[i]);
                g_visit_data.error = true;
                return;
            }
            if (g_visit_data.found_keys[i]) {
                printf("[ERROR] Key %u visited multiple times\n", key);
                g_visit_data.error = true;
                return;
            }
            g_visit_data.found_keys[i] = true;
            found = true;
            break;
        }
    }
    
    if (!found) {
        printf("[ERROR] Unexpected key %u visited\n", key);
        g_visit_data.error = true;
    }
}

/* Check internal consistency via lookup only */
static void check_consistency_keys(hashtable_t *h, uint32_t *keys, size_t n) {
    for (size_t i = 0; i < n; i++) {
        void *v = hashtable_lookup(h, keys[i]);
        if (!v) {
            printf("[ERROR] Key %u expected but not found!\n", keys[i]);
            exit(1);
        }
    }
}

/* Test basic insert / refcount / remove operations */
static int test_basic_operations(hashtable_t *h) {
    printf("[TEST] Basic insert / refcount / remove...\n");
    hashtable_insert(h, 42, NULL);
    void *v = hashtable_lookup(h, 42);
    if (v != (void*)(uintptr_t)(0x1000 + 42)) {
        printf("[ERROR] Wrong value for key 42!\n");
        return 1;
    }
    hashtable_insert(h, 42, v); // increment refcount
    hashtable_remove(h, 42);
    hashtable_remove(h, 42);
    if (hashtable_lookup(h, 42) != NULL) {
        printf("[ERROR] Key 42 should be removed!\n");
        return 1;
    }
    return 0;
}

/* Test bulk insert and remove operations */
static int test_bulk_operations(hashtable_t *h) {
    printf("[TEST] Bulk insert & remove...\n");
    const size_t N = 5000;
    uint32_t *keys = malloc(sizeof(uint32_t) * N);
    for (size_t i = 0; i < N; i++) {
        keys[i] = (uint32_t)(i + 1000);
        hashtable_insert(h, keys[i], NULL);
    }
    check_consistency_keys(h, keys, N);
    for (size_t i = 0; i < N; i++) hashtable_remove(h, keys[i]);
    for (size_t i = 0; i < N; i++)
        if (hashtable_lookup(h, keys[i]) != NULL) {
            printf("[ERROR] Key %u should be removed!\n", keys[i]);
            free(keys);
            return 1;
        }

    printf("[TEST] Re-insert previously removed keys...\n");
    for (size_t i = 0; i < N; i++)
        hashtable_insert(h, keys[i], (void*)(uintptr_t)(0x1000 + keys[i]));
    check_consistency_keys(h, keys, N);

    printf("[TEST] Removing all reinserted keys...\n");
    for (size_t i = 0; i < N; i++) hashtable_remove(h, keys[i]);
    free(keys);
    return 0;
}

/* Test massive resize stress */
static int test_resize_stress(hashtable_t *h) {
    printf("[TEST] Massive resize stress...\n");
    const size_t M = 20000;
    for (size_t i = 0; i < M; i++)
        hashtable_insert(h, (uint32_t)(i + 0xABCDE0), NULL);
    for (size_t i = 0; i < M; i++)
        hashtable_remove(h, (uint32_t)(i + 0xABCDE0));
    return 0;
}

/* Test random stress with balanced operations */
static int test_random_stress(hashtable_t *h) {
    printf("[TEST] Random stress (balanced ops, bounded refcount)...\n");
    const size_t OPS = 50000, KEYSPACE = 2000;
    size_t inserts = 0, removes = 0;
    for (size_t i = 0; i < OPS; i++) {
        uint32_t key = (rand() % KEYSPACE) + 1; // Avoid key 0 (EMPTY_KEY)
        void *val = hashtable_lookup(h, key);
        
        // Bias towards insert when table is empty, remove when getting full
        bool should_insert = (!val) || (h->size < KEYSPACE/4 && (rand() % 3 != 0));
        
        if (should_insert && (!val || ((uintptr_t)val & 0xFFFFFF) == PhysicalAddr(val))) {
            hashtable_insert(h, key, val);
            inserts++;
        } else if (val) {
            hashtable_remove(h, key);
            removes++;
        }
    }
    printf("[INFO] Performed %zu inserts, %zu removes\n", inserts, removes);
    return 0;
}

/* Test hashtable_visit functionality */
static int test_visit_functionality(hashtable_t *h) {
    printf("[TEST] hashtable_visit functionality...\n");
    // Clear the table and add some known keys with different refcounts
    for (size_t i = 0; i < h->capacity * 2; i += 2) {
        if (h->entries[i] != 0 && h->entries[i] != 0xFFFFFFFF) {
            hashtable_remove(h, h->entries[i]);
        }
    }
    
    // Add test keys with known refcounts
    hashtable_insert(h, 100, NULL);  // refcount 1
    hashtable_insert(h, 200, NULL);  // refcount 1
    hashtable_insert(h, 200, hashtable_lookup(h, 200)); // refcount 2
    hashtable_insert(h, 300, NULL);  // refcount 1
    hashtable_insert(h, 300, hashtable_lookup(h, 300)); // refcount 2
    hashtable_insert(h, 300, hashtable_lookup(h, 300)); // refcount 3
    
    // Setup expected data for visit test
    g_visit_data.expected_keys[0] = 100;
    g_visit_data.expected_keys[1] = 200;
    g_visit_data.expected_keys[2] = 300;
    g_visit_data.expected_refcounts[0] = 1;
    g_visit_data.expected_refcounts[1] = 2;
    g_visit_data.expected_refcounts[2] = 3;
    g_visit_data.expected_count = 3;
    g_visit_data.actual_count = 0;
    g_visit_data.error = false;
    for (size_t i = 0; i < 10; i++) g_visit_data.found_keys[i] = false;
    
    // Call hashtable_visit
    hashtable_visit(h, visit_collector);
    
    // Check results
    if (g_visit_data.error) {
        return 1;
    }
    
    if (g_visit_data.actual_count != g_visit_data.expected_count) {
        printf("[ERROR] Expected to visit %zu keys, but visited %zu\n", g_visit_data.expected_count, g_visit_data.actual_count);
        return 1;
    }
    
    for (size_t i = 0; i < g_visit_data.expected_count; i++) {
        if (!g_visit_data.found_keys[i]) {
            printf("[ERROR] Key %u was not visited\n", g_visit_data.expected_keys[i]);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    srand((unsigned)time(NULL));
    hashtable_t h;
    hashtable_init(&h, 8, my_loader);

    // Run all tests
    if (test_basic_operations(&h) != 0) return 1;
    if (test_bulk_operations(&h) != 0) return 1;
    if (test_resize_stress(&h) != 0) return 1;
    if (test_random_stress(&h) != 0) return 1;
    if (test_visit_functionality(&h) != 0) return 1;

    printf("[TEST] ALL TESTS PASSED. Final size=%zu\n", h.size);
    hashtable_free(&h);
    return 0;
}
