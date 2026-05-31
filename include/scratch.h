/**
 * @file scratch.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Public API for the scratch allocator.
 *
 * The scratch allocator provides malloc/free-like functions backed by memory
 * that is allocated from a separate section of the main heap.
 *
 * It is intended for short-lived allocations such as load buffers,
 * decompression workspaces, staging areas, and temporary conversion data.
 * Long-lived allocations should continue to use the regular heap API.
 * The goal is to reduce memory fragmentation, by avoiding leaving holes in 
 * the main heap caused by these short-lived allocations.
 *
 * API invariants:
 *   - pointers returned by scratch_malloc/scratch_calloc/scratch_realloc must
 *     be released with scratch_free (not free);
 *   - only pointers from this allocator may be passed to scratch_free/realloc;
 */

 #ifndef LIBDRAGON_SCRATCH_H
 #define LIBDRAGON_SCRATCH_H
 
 #include <stddef.h>
 #include <stdbool.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
  
 /**
  * @brief Allocate memory from the scratch heap.
  *
 * Allocates @p size bytes from the scratch allocator.
 * The returned pointer is suitably aligned for normal C object access.
  *
  * @param size         Number of bytes to allocate.
  * @return Pointer to the allocated memory, or NULL if there is not enough
  *         memory.
  *
  * @note Memory returned by this function is uninitialized.
  *
  * @see scratch_free
  * @see scratch_calloc
  * @see scratch_realloc
  */
 void *scratch_malloc(size_t size);
 
 /**
  * @brief Allocate zero-initialized memory from the scratch heap.
  *
  * Allocates enough memory for @p count objects of @p size bytes each and clears
  * the allocation to zero.
  *
  * This function performs overflow checking on the multiplication
  * @p count * @p size.
  *
  * @param count            Number of elements to allocate.
  * @param size             Size of each element, in bytes.
  *
  * @return Pointer to the allocated zero-filled memory, or NULL if there is not
  *         enough memory or if the requested size overflows.
  *
  * @see scratch_malloc
  * @see scratch_free
  */
 void *scratch_calloc(size_t count, size_t size);
 
 /**
  * @brief Resize a scratch allocation.
  *
  * Changes the size of the allocation pointed to by @p ptr to at least
  * @p size bytes.
  *
  * The contents are preserved up to the minimum of the old and new sizes. Newly
  * allocated bytes, if any, are uninitialized.
  *
  * @param ptr              Pointer previously returned by #scratch_malloc(), #scratch_calloc(),
  *                         or #scratch_realloc(). It may be NULL.
  * @param size             New requested size in bytes.
  *
  * @return Pointer to the resized allocation, or NULL if the allocation could not
  *         be resized. If NULL is returned because of allocation failure, the
  *         original allocation remains valid.
  *
  * @note If @p ptr is NULL, this function behaves like scratch_malloc().
  * @note If @p size is zero, this function frees @p ptr and returns NULL.
  *
  * @see scratch_malloc
  * @see scratch_free
  */
 void *scratch_realloc(void *ptr, size_t size);
 
 /**
  * @brief Free a scratch allocation.
  *
  * Releases a block previously returned by #scratch_malloc(), #scratch_calloc(), or
  * #scratch_realloc().
  *
  * Passing NULL is allowed and has no effect.
  *
  * @param ptr          Pointer to the scratch allocation to free, or NULL.
  */
 void scratch_free(void *ptr);
 
 /**
  * @brief Check internal consistency of the scratch heap.
  *
  * Runs a full consistency check over the scratch heap metadata.
  *
  * This function validates allocator invariants and may assert
  * if corruption or inconsistent state is detected.
  */
 void scratch_check(void);
 
/**
 * @brief Scratch allocator statistics.
 */
typedef struct {
    size_t live_bytes;      ///< Sum of user-requested sizes for live allocations
    size_t peak_bytes;      ///< Peak value of live_bytes since boot
    size_t live_blocks;     ///< Number of currently live scratch allocations
    size_t reserved_bytes;  ///< Bytes currently reserved by the scratch allocator
} scratch_stats_t;
 
 /**
 * @brief Return statistics about scratch allocator usage.
  *
 * This function writes all current scratch allocator counters to @p stats.
  *
 * @param[out] stats
 *            Pointer to destination structure.
  */
void scratch_get_stats(scratch_stats_t *stats);
 
 /**
  * @brief Check whether the scratch heap has no live allocations.
  *
  * @return true if there are no live scratch allocations, false otherwise.
  */
 bool scratch_empty(void);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* LIBDRAGON_SCRATCH_H */
