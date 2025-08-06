#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "../../src/hashtable.c"

void* my_loader(uint32_t key) {
    return (void*)(uintptr_t)(0x1000 + key);
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

int main(void) {
    srand((unsigned)time(NULL));
    hashtable_t h;
    hashtable_init(&h, 8, my_loader);

    printf("[TEST] Basic insert / refcount / remove...\n");
    hashtable_insert(&h, 42, NULL);
    void *v = hashtable_lookup(&h, 42);
    if (v != (void*)(uintptr_t)(0x1000 + 42)) {
        printf("[ERROR] Wrong value for key 42!\n");
        return 1;
    }
    hashtable_insert(&h, 42, v); // increment refcount
    hashtable_remove(&h, 42);
    hashtable_remove(&h, 42);
    if (hashtable_lookup(&h, 42) != NULL) {
        printf("[ERROR] Key 42 should be removed!\n");
        return 1;
    }

    printf("[TEST] Bulk insert & remove...\n");
    const size_t N = 5000;
    uint32_t *keys = malloc(sizeof(uint32_t) * N);
    for (size_t i = 0; i < N; i++) {
        keys[i] = (uint32_t)(i + 1000);
        hashtable_insert(&h, keys[i], NULL);
    }
    check_consistency_keys(&h, keys, N);
    for (size_t i = 0; i < N; i++) hashtable_remove(&h, keys[i]);
    for (size_t i = 0; i < N; i++)
        if (hashtable_lookup(&h, keys[i]) != NULL) {
            printf("[ERROR] Key %u should be removed!\n", keys[i]);
            return 1;
        }

    printf("[TEST] Re-insert previously removed keys...\n");
    for (size_t i = 0; i < N; i++)
        hashtable_insert(&h, keys[i], (void*)(uintptr_t)(0x1000 + keys[i]));
    check_consistency_keys(&h, keys, N);

    printf("[TEST] Removing all reinserted keys...\n");
    for (size_t i = 0; i < N; i++) hashtable_remove(&h, keys[i]);
    free(keys);

    printf("[TEST] Massive resize stress...\n");
    const size_t M = 20000;
    for (size_t i = 0; i < M; i++)
        hashtable_insert(&h, (uint32_t)(i + 0xABCDE0), NULL);
    for (size_t i = 0; i < M; i++)
        hashtable_remove(&h, (uint32_t)(i + 0xABCDE0));

    printf("[TEST] Random stress (balanced ops, bounded refcount)...\n");
    const size_t OPS = 50000, KEYSPACE = 2000;
    size_t inserts = 0, removes = 0;
    for (size_t i = 0; i < OPS; i++) {
        uint32_t key = rand() % KEYSPACE;
        void *val = hashtable_lookup(&h, key);
        
        // Bias towards insert when table is empty, remove when getting full
        bool should_insert = (!val) || (h.size < KEYSPACE/4 && (rand() % 3 != 0));
        
        if (should_insert && (!val || ((uintptr_t)val & 0xFFFFFF) == PhysicalAddr(val))) {
            hashtable_insert(&h, key, val);
            inserts++;
        } else if (val) {
            hashtable_remove(&h, key);
            removes++;
        }
    }
    printf("[INFO] Performed %zu inserts, %zu removes\n", inserts, removes);

    printf("[TEST] ALL TESTS PASSED. Final size=%zu\n", h.size);
    hashtable_free(&h);
    return 0;
}
