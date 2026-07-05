/**
 * @file h264_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal (non-stable) hooks for the H.264 player
 */
#ifndef LIBDRAGON_H264_INTERNAL_H
#define LIBDRAGON_H264_INTERNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom allocator for the H.264 player instance.
 *
 * By default h264_open() allocates its player instance (the large h264_t state
 * struct, ~268 KiB) via malloc(). Install a custom allocator to source that one
 * block from a caller-owned region instead, so the player does not have to be
 * carved out of the regular heap. The matching free() is called from
 * h264_close() for a player that was allocated while a custom allocator was
 * installed (the player records its own origin).
 *
 * @param alloc  Returns @p sz bytes aligned to @p align (>= 16), or NULL on failure.
 * @param free   Releases a block previously returned by @p alloc.
 */
typedef struct h264_allocator_s {
    void* (*alloc)(size_t sz, size_t align);
    void  (*free)(void* p);
} h264_allocator_t;

/**
 * @brief Install (or clear) the custom allocator used for H.264 player instances.
 *
 * Pass NULL to restore the default malloc/free. The allocator only affects
 * players opened while it is installed; each player remembers how it was
 * allocated and is freed accordingly in h264_close().
 */
void h264_set_allocator(const h264_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif
