/**
 * @file sys_alloc.h
 * @brief Single tagged allocation seam for large/fragmenting subsystem buffers.
 *
 * Several subsystems (asset, lspr3, yuv, wav64, h264, dlfcn) make large or
 * numerous heap allocations that, on memory-constrained targets, an application
 * may want to source from memory of its choosing (e.g. a pre-reserved region)
 * to control heap layout and avoid mid-session fragmentation failures.
 *
 * Rather than a bespoke allocator setter per subsystem, all of these funnel
 * through a single pair of hooks, ::__sys_alloc / ::__sys_free, tagged with a
 * ::sys_alloc_purpose_t so the application can dispatch by purpose. Both are
 * declared @c weak and default to a standard heap allocation (memalign/free, and
 * uncached for #SYS_ALLOC_YUV), so this is a no-op for callers that do not
 * override them. An application opts in simply by providing a strong definition
 * of ::__sys_alloc / ::__sys_free.
 *
 * @note Freeing is symmetric: a block obtained via ::__sys_alloc with a given
 * purpose must be released via ::__sys_free with the SAME purpose, because the
 * override typically routes each purpose to a different backend.
 */
#ifndef __LIBDRAGON_SYS_ALLOC_H
#define __LIBDRAGON_SYS_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Identifies which subsystem allocation ::__sys_alloc is servicing.
 *
 * The purpose lets an overriding application route each subsystem's buffer to a
 * different backend (scratch, a reserve, an arena, ...). The parenthesised note
 * on each value is the behaviour of the weak default.
 */
typedef enum {
    SYS_ALLOC_ASSET = 0,    ///< asset_load() decompressed payload (memalign). Freed by the caller with the C free() unless overridden.
    SYS_ALLOC_LSPR3_SPRITE, ///< lspr3 decoded output sprite buffer (memalign, 64-byte aligned).
    SYS_ALLOC_LSPR3_MB,     ///< lspr3 transient mbStorage array (memalign; the decoder zeroes it itself).
    SYS_ALLOC_YUV,          ///< yuv U/V interleave buffer. MUST be uncached (RSP-written, RDP-read); default is malloc_uncached().
    SYS_ALLOC_WAV64,        ///< wav64 streaming handle block (memalign). Streaming loads only.
    SYS_ALLOC_H264,         ///< h264 player instance (large; malloc).
    SYS_ALLOC_DSO_MODULE,   ///< dlfcn resident DSO module image (memalign).
} sys_alloc_purpose_t;

/**
 * @brief Allocate @p size bytes aligned to @p align for the given @p purpose.
 *
 * Weak default: uncached for #SYS_ALLOC_YUV, otherwise memalign(align|16).
 * Override with a strong definition to dispatch by purpose. Returns NULL on
 * failure (callers assert).
 */
void *__sys_alloc(sys_alloc_purpose_t purpose, size_t size, size_t align);

/**
 * @brief Free a block previously returned by ::__sys_alloc for @p purpose.
 *
 * Must be called with the same @p purpose the block was allocated under.
 */
void __sys_free(sys_alloc_purpose_t purpose, void *ptr);

#ifdef __cplusplus
}
#endif

#endif
