/**
 * @file hashtable_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Simple cache-friendly open addressing hashtable with reference counting.
 * 
 * Keys are 32-bit integers. Values are stored as pointers (void*) internally converted
 * to 24-bit physical addresses via PhysicalAddr()/CachedAddr() macros (user must ensure validity).
 *
 * Each key has an 8-bit reference count (1..255).
 * When inserting an existing key, the reference count is incremented.
 * Removing decrements the reference count, and the key is deleted when it reaches zero.
 */
#ifndef LIBDRAGON_HASHTABLE_INTERNAL_H
#define LIBDRAGON_HASHTABLE_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bucket structure for the hashtable */
typedef struct __attribute__((aligned(16))) {
    uint32_t kv[8]; // [0..3]=keys, [4..7]=vals
} hashtable_bucket_t;

/**
 * @brief Loader callback used to lazily create a value for a given key.
 *
 * This function is called by hashtable_insert() when a key is inserted
 * for the first time and no value is provided.
 *
 * @param key The 32-bit key for which the value should be created.
 * @return A pointer to the created value (may be NULL if no value).
 */
typedef void* (*hashtable_loader_fn)(uint32_t key);

/** @brief A hashtable structure */
typedef struct hashtable_s {
    hashtable_bucket_t *buckets;    ///< Pointer to the array of buckets
    size_t n_buckets;               ///< Number of buckets in the hashtable
    size_t size;                    ///< Current number of entries in the hashtable
    hashtable_loader_fn loader;     ///< Loader function to lazily create values
} hashtable_t;

/**
 * @brief Initialize a hashtable.
 *
 * @param h Pointer to a hashtable_t structure.
 * @param initial_entries Suggested initial number of entries (will be rounded up).
 * @param loader A loader function to lazily create values, or NULL if not used.
 * @return 1 on success, 0 on failure.
 */
int hashtable_init(hashtable_t *h, size_t initial_entries, hashtable_loader_fn loader);

/**
 * @brief Destroy a hashtable and free all associated memory.
 *
 * @param h Pointer to a hashtable_t structure.
 */
void hashtable_free(hashtable_t *h);

/**
 * @brief Insert a key into the hashtable.
 *
 * If the key already exists, its reference count is incremented.
 * If the key does not exist:
 *   - If @p value is not NULL, it is stored directly.
 *   - If @p value is NULL, the loader (if any) is called to create it.
 *
 * @param h Pointer to a hashtable_t.
 * @param key 32-bit key to insert.
 * @param value Optional value to associate with the key, or NULL to use the loader.
 * @return The stored value (void*).
 */
void* hashtable_insert(hashtable_t *h, uint32_t key, void *value);

/**
 * @brief Lookup a value associated with a key.
 *
 * @param h Pointer to a hashtable.
 * @param key 32-bit key to search.
 * @return The stored value (void*), or NULL if the key does not exist.
 */
void* hashtable_lookup(hashtable_t *h, uint32_t key);

/**
 * @brief Remove a key from the hashtable.
 *
 * Decrements the reference count. If the reference count reaches 0,
 * the key is deleted.
 *
 * @param h Pointer to a hashtable_t.
 * @param key 32-bit key to remove.
 * @return void* The value associated with the key before removal, or NULL
 * if the key did not exist or its reference count did not reach zero.
 */
void* hashtable_remove(hashtable_t *h, uint32_t key);

#ifdef __cplusplus
}
#endif

#endif /* LIBDRAGON_HASHTABLE_INTERNAL_H */
