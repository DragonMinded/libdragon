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

#ifndef __ASSEMBLER__
#include <stdint.h>

/**
 * @brief Convert an I420 (YUV 4:2:0 planar) frame to packed UYVY (FMT_YUV16)
 *        on the RSP.
 *
 * Reuses the existing rsp_yuv overlay's `interleave4` command, which already
 * emits UYVY-packed output (32x16 px tiles). The frame is iterated in 32x16
 * tiles, so width must be a multiple of 32 and height a multiple of 16.
 *
 * Caller must have called yuv_init() and is responsible for cache management:
 * the source planes must be visible to the RSP (uncached or written-back),
 * and the destination must be invalidated before this call (RSP DMA writes
 * through to RAM, bypassing the CPU cache).
 *
 * Does not call rspq_wait(). Caller must sync before reading dst.
 *
 * @param y         Y plane (width * height bytes, row stride = y_pitch).
 * @param cb        CB plane (width/2 * height/2 bytes, row stride = y_pitch/2).
 * @param cr        CR plane (width/2 * height/2 bytes, row stride = y_pitch/2).
 * @param y_pitch   Row pitch of the Y plane in bytes.
 * @param dst       Destination UYVY buffer (width * height * 2 bytes).
 * @param dst_pitch Row pitch of the destination buffer in bytes (typ width*2).
 * @param width     Frame width in pixels (must be a multiple of 32).
 * @param height    Frame height in pixels (must be a multiple of 16).
 */
void yuv_i420_to_uyvy(const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
                      int y_pitch,
                      uint8_t *dst, int dst_pitch,
                      int width, int height);

/**
 * @brief Interleave I420 U+V planes into an NV12 IA16 chroma plane on the RSP.
 *
 * Reads U and V at half luma resolution (luma_w/2 x luma_h/2) and writes
 * an IA16 plane of the same dimensions, with U in the high byte and V in
 * the low byte of each pixel — the layout the RDP YUV combiner expects
 * for an NV12 sprite. Caller must invalidate `dst` before calling, and
 * sync (e.g. rspq_wait) before reading it. Does not handle the Y plane.
 *
 * @param u             Source CB plane (chroma_pitch x luma_h/2 bytes).
 * @param v             Source CR plane (chroma_pitch x luma_h/2 bytes).
 * @param chroma_pitch  Source row pitch of U/V planes in bytes.
 * @param dst           Destination IA16 plane (dst_pitch x luma_h/2 bytes).
 * @param dst_pitch     Destination row pitch in bytes (typically luma_w).
 * @param luma_w        Frame luma width in pixels (must be a multiple of 32).
 * @param luma_h        Frame luma height in pixels (must be a multiple of 16).
 */
void yuv_i420_chroma_to_nv12(const uint8_t *u, const uint8_t *v, int chroma_pitch,
                             uint8_t *dst, int dst_pitch,
                             int luma_w, int luma_h);

/**
 * @brief Interleave I420 U+V planes into an NV16 IA16 chroma plane on the RSP.
 *
 * Like #yuv_i420_chroma_to_nv12 but each input chroma row is duplicated
 * to two output rows (vertical chroma upsample 4:2:0 -> 4:2:2). The
 * destination plane has full luma height (dst_pitch x luma_h bytes).
 */
void yuv_i420_chroma_to_nv16(const uint8_t *u, const uint8_t *v, int chroma_pitch,
                             uint8_t *dst, int dst_pitch,
                             int luma_w, int luma_h);

/**
 * @brief 2D RDRAM-to-RDRAM tiled copy of a single byte plane on the RSP.
 *
 * Faster than CPU memcpy from uncached source: the RSP DMA pulls cache-line
 * bursts at near-peak RDRAM bandwidth, while CPU loads from uncached memory
 * pay full RDRAM latency per word.
 *
 * Caller must invalidate `dst` before calling and sync (rspq_wait) before
 * reading it. width must be %32, height must be %16.
 */
void yuv_plane_copy(const uint8_t *src, int src_pitch,
                    uint8_t *dst, int dst_pitch,
                    int width, int height);
#endif

#endif
