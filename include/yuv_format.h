/**
 * @file yuv_format.h
 * @brief YUV plane layout / chroma subsampling enum
 * @ingroup video
 *
 * This is the small subset of #yuv.h that other public headers (notably
 * #sprite.h) need to forward-reference. It pulls in no rdpq/N64 specifics so
 * it remains safe to include from host-side tools that consume #sprite.h.
 */
#ifndef __LIBDRAGON_YUV_FORMAT_H
#define __LIBDRAGON_YUV_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief YUV chroma subsampling and memory layout.
 *
 * Selects how the chroma planes of a #yuv_frame_t are laid out in memory.
 * This determines what #yuv_tex_blit (and #yuv_blitter_run) expects to find
 * in the frame's surfaces and whether an RSP-side interleave pass is needed
 * before the RDP draw.
 *
 * The default value (0) matches the historical behavior, so existing callers
 * that build a #yuv_frame_t with designated initializers do not need to set
 * @ref yuv_frame_t::format explicitly.
 */
typedef enum yuv_format_e {
    /** @brief 3-plane planar 4:2:0 (I420).
     *
     *  Y, U, and V are three separate #FMT_I8 surfaces; U and V are at half
     *  width and half height. RSP interleaves U+V into a temporary IA16
     *  buffer before the RDP draw, so #yuv_init must have been called. */
    YUV_I420 = 0,
    /** @brief Semi-planar 4:2:0 (NV12).
     *
     *  Y is a #FMT_I8 surface; U+V are pre-interleaved into a single
     *  #FMT_IA16 surface (U in the high byte, V in the low byte) at half
     *  width and half height. The RSP interleave step is skipped, so this
     *  layout does not require #yuv_init.
     *  The @ref yuv_frame_t::v surface is unused. */
    YUV_NV12 = 1,
    /** @brief Packed 4:2:2 (UYVY).
     *
     *  A single #FMT_YUV16 surface storing macropixels in U Y V Y byte order.
     *  This is the layout the RDP consumes natively, so no plane-splitting,
     *  RSP interleave, or chroma upsampling is needed: the surface is drawn
     *  straight through. The @ref yuv_frame_t::u and @ref yuv_frame_t::v
     *  surfaces are unused. */
    YUV_UYVY = 2,
    /** @brief Semi-planar 4:2:2 (NV16).
     *
     *  Y is a #FMT_I8 surface at full size; U+V are pre-interleaved into a
     *  single #FMT_IA16 surface (U in the high byte, V in the low byte) at
     *  half width and **full** height. Each Y scanline therefore consumes a
     *  distinct UV row (no vertical chroma reuse). The RSP interleave step
     *  is skipped, so this layout does not require #yuv_init.
     *  The @ref yuv_frame_t::v surface is unused. */
    YUV_NV16 = 3,
    /** @brief 3-plane planar 4:2:2 (I422).
     *
     *  Y, U, and V are three separate #FMT_I8 surfaces; U and V are at half
     *  width and **full** height. RSP interleaves U+V into a temporary IA16
     *  buffer (full-height variant) before the RDP draw, so #yuv_init must
     *  have been called. */
    YUV_I422 = 4,
} yuv_format_t;

#ifdef __cplusplus
}
#endif

#endif
