/**
 * @file yuv_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef __LIBDRAGON_YUV_INTERNAL_H
#define __LIBDRAGON_YUV_INTERNAL_H

#define ASSERT_INVALID_INPUT_Y   0x0001
#define ASSERT_INVALID_INPUT_CB  0x0002
#define ASSERT_INVALID_INPUT_CR  0x0003
#define ASSERT_INVALID_OUTPUT    0x0004

#define BLOCK_W 32
#define BLOCK_H 16

// This header is also included by rsp_yuv.S for the macros above; keep the C
// declarations below out of the assembler's view.
#ifndef __ASSEMBLER__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocator for the YUV library's internal interleave buffer.
 *
 * The blitter keeps a small internal buffer (used to interleave the U and V
 * chroma planes before feeding them to the RDP). By default it is allocated
 * from the system heap. On memory-constrained or long-running targets that heap
 * can fragment to the point where this allocation fails mid-session; callers can
 * install a custom allocator (e.g. backed by a contiguous scratch/transient
 * heap) to source it from elsewhere. See #yuv_set_internal_allocator.
 */
typedef struct yuv_allocator_s {
    /** @brief Allocate @p size bytes aligned to at least @p align. Must return
     *         an uncached pointer (the RDP/RSP access the buffer directly), or
     *         NULL on failure. */
    void *(*alloc)(void *ctx, size_t size, int align);
    /** @brief Free a pointer previously returned by alloc(). */
    void  (*free)(void *ctx, void *ptr);
    /** @brief Opaque context passed to alloc()/free(). */
    void  *ctx;
} yuv_allocator_t;

/**
 * @brief Install a custom allocator for the internal interleave buffer.
 *
 * Pass NULL to revert to the default (system-heap) allocator. Both @p alloc and
 * @p free must be provided, otherwise the default allocator is used.
 *
 * The allocator must be installed up front, before the interleave buffer is
 * first allocated (i.e. before the first blit), and cleared only once it has
 * been released (e.g. after the final #yuv_close()): the buffer is freed through
 * whichever allocator is installed at release time, so changing it while the
 * buffer is live would strand it on the wrong allocator. This is asserted.
 *
 * @note Internal/advanced API: deliberately not declared in the public
 * <yuv.h>. The allocator-lifetime coupling above is a sharp edge not yet fit
 * for general use; declared here for opt-in by code embedding libdragon that
 * needs to control the interleave buffer's placement.
 *
 * @param allocator   Allocator to install, or NULL for the default.
 */
void yuv_set_internal_allocator(const yuv_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif // !__ASSEMBLER__

#endif
