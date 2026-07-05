/**
 * @file lspr3.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Lossy-sprite Level 3: H264I decoder
 *
 * H264I ("H.264 Image") is libdragon's "Lossy-sprite Level 3" container format:
 * lossy compression is useful for large images that need higher storage
 * compression at the cost of some quality loss, such as large backgrounds. The
 * quality factor can be tuned at build-time to achieve the desired size/quality
 * tradeoff. H264I files are produced by `mksprite --lossy=<quality> --compress 3`
 * and always decode to a #FMT_RGBA16 sprite.
 *
 * You must opt-in to support for Level 3 lossy sprites by calling #lspr3_init
 * to register the H264I decoder with #sprite_load before loading H264I files.
 * Once initialized, #sprite_load will recognize H264I-compressed sprites and
 * decode them transparently.
 *
 * H264I is implemented as an H.264 intra-only bitstream (4:2:0 YUV, BT.709
 * full range) plus a small header. At load-time the H.264 slice is decoded
 * and the YUV reconstruction is converted into a #FMT_RGBA16 sprite, so the
 * returned #sprite_t works like a normal sprite.
 *
 * H264I can encode either images with no alpha channel, or images with a
 * 1-bit alpha channel. The alpha channel is stored lossless alongside the
 * H.264 bitstream, and reconstructed at load-time.
 */
#ifndef __LIBDRAGON_LSPR3_H
#define __LIBDRAGON_LSPR3_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sprite_s sprite_t;

/**
 * @brief Optional parameters for advanced lspr3 decoding.
 *
 * Pass a pointer to one of these into #lspr3_load_buf_ex to override the
 * default decode behaviour used by #sprite_load. Each field defaults to
 * "use the standard behaviour" when zero, so a zero-initialised
 * `(lspr3_load_parms_t){}` is equivalent to passing NULL.
 */
typedef struct lspr3_load_parms_s {
    /**
     * @brief Custom output-buffer allocator.
     *
     * When non-NULL, lspr3 invokes this instead of `memalign` for the
     * decoded sprite buffer. The callback receives the size in bytes,
     * the required alignment (in bytes — at least 16, typically 64 for
     * RDP color-image use), and the @ref alloc_ctx opaque pointer.
     *
     * The caller owns the matching `free` — the returned sprite must NOT
     * be passed to #sprite_free, because that path always calls libc
     * `free()`. Pair `alloc` with your own destructor (e.g.
     * `scratch_free` if you allocated from the scratch heap).
     */
    void *(*alloc)(void *ctx, size_t size, size_t alignment);

    /** @brief Opaque pointer passed to @ref alloc. */
    void *alloc_ctx;

    /**
     * @brief Output horizontal divisor.
     *
     * 0 or 1 = native source width (default).
     * 2 = decode to a half-width sprite. The RDP YUV combiner performs
     * the horizontal downsample during the YUV→RGB conversion using its
     * bilinear filter, so no separate downsample pass runs. Useful when
     * the destination framebuffer is narrower than the encoded source
     * (or memory pressure rules out a full-width copy).
     */
    int output_x_divisor;

    /**
     * @brief Output vertical divisor.
     *
     * 0 or 1 = native source height (default).
     * 2 = decode to a half-height sprite. As with @ref output_x_divisor
     * the RDP bilinear filter handles the downsample during the YUV→RGB
     * blit. The two divisors are independent — set both to 2 for a
     * quarter-area sprite, or only one for an anisotropic downscale.
     */
    int output_y_divisor;

    /**
     * @brief Optional allocator for the transient mb-storage buffer.
     *
     * When non-NULL, lspr3 invokes this instead of `calloc` for the
     * per-slice mbStorage_t array (sized `mb_count * sizeof(mbStorage_t)`,
     * worst case ~110 KiB for a 640x240 source). The callback receives
     * the total byte size and the @ref mb_alloc_ctx opaque pointer, and
     * must return zero-initialised memory (the decoder relies on it).
     *
     * If set, @ref mb_free must also be set; lspr3 calls it on the
     * returned pointer before #lspr3_load_buf_ex returns.
     *
     * Useful when the main heap is fragmented enough that a single
     * 100+ KiB chunk can't be satisfied — callers can route this
     * allocation to a dedicated reserve or to scratch.
     */
    void *(*mb_alloc)(void *ctx, size_t size);

    /** @brief Matching deallocator for @ref mb_alloc. Required if @ref mb_alloc is set. */
    void (*mb_free)(void *ctx, void *ptr);

    /** @brief Opaque pointer passed to @ref mb_alloc and @ref mb_free. */
    void *mb_alloc_ctx;

    /**
     * @brief Decode in horizontal bands to cap the transient YUV footprint.
     *
     * When > 0, the frame is reconstructed in bands of this many MB-rows into a
     * small windowed scratch buffer (`band_rows`+2 MB-rows) and each band is
     * converted into the output sprite as it completes, so the peak transient
     * YUV is ~`(band_rows+2)/mb_height` of the full-frame YUV instead of the
     * whole frame. Output is bit-identical to the full-frame path: intra
     * prediction's cross-band top-neighbours are carried, and each band decodes
     * one row ahead so its bottom-row chroma upsample has its real neighbour (no
     * seam at band boundaries). Costs a few % more decode time (extra per-band
     * RSP/RDP syncs), so it is intended for callers trading a little speed for a
     * smaller transient working set.
     *
     * 0 (default) = decode the whole frame at once (fastest; needs the full
     * frame's worth of transient YUV).
     */
    int band_rows;
} lspr3_load_parms_t;

/**
 * @brief Decode an H264I-encoded sprite from memory, with options.
 *
 * Lower-level decode entry point exposed for advanced callers. The
 * #sprite_load path uses #lspr3_load_buf_ex with NULL @p parms, matching
 * the historical behaviour (memalign-allocated output, native source
 * dimensions).
 *
 * Callers passing custom @p parms.alloc are responsible for freeing the
 * returned sprite via the matching deallocator — #sprite_free always
 * calls libc `free()` and is not suitable for custom-allocated sprites.
 *
 * @param encoded_buf  Pointer to the H264I-encoded sprite payload.
 * @param encoded_sz   Size of @p encoded_buf in bytes.
 * @param parms        Optional decode parameters; pass NULL for defaults.
 * @return The decoded sprite, or aborts via #assertf on failure.
 */
sprite_t *lspr3_load_buf_ex(const void *encoded_buf, int encoded_sz,
                            const lspr3_load_parms_t *parms);

/**
 * @brief Register the H264I (Lossy-sprite Level 3) decoder with the sprite loader.
 *
 * After registering, #sprite_load recognizes the `H264` magic and decodes
 * matching files transparently. Until then, loading an H264I file via
 * #sprite_load fails with an assertion.
 * 
 * Refcounted; calling this increments the refcount and will require the
 * same number of #lspr3_close calls to fully unregister.
 */
void lspr3_init(void);

/**
 * @brief Unregister the H264I (Lossy-sprite Level 3) decoder.
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes H264I files and attempts to load them will
 * fail with an assertion.
 */
void lspr3_close(void);

#ifdef __cplusplus
}
#endif

#endif
