/**
 * @file rsph264_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
/*
 * RSPH264 - low-level interface library to RSP ucode
 *
 * This library is used internally by the H264 player
 * to initialize and communicate with the RSP ucode. It
 * is not meant for high-level usage.
 *
 */

#ifndef RSPH264_H
#define RSPH264_H

#include <stdint.h>

/***********************************************************
 * Main API.
 ***********************************************************/

// Initialize RSPH264 library (call it once)
void rsph264_init(void);

// Start working on a frame. This function uploads the ucode
// to RSP and starts RSP processing.
// After calling this function (and before rsph264_end_frame()),
// RSP is exclusively used by RSPH264 and should not be concurrently
// used for other tasks.
void rsph264_begin_frame(void);

// Sync RSP: wait for currently queued functions to terminate.
// This can be used to force sync points in which all previously enqueued
// tasks have been performed, but obviously wastes CPU cycles.
// It should be used only for debugging purposes.
void rsph264_sync(void);


/***********************************************************
 * Task queueing API.
 *
 * These functions enqueue a new task for execution by RSP.
 * The task will be performed in background as soon as the RSP
 * finishes previously enqueued tasks.
 *
 * Tasks are strictly executed in enqueued order, as they might
 * depend one on each other.
 ***********************************************************/

// Cache flag: Do not force cache writeback of source buffer
#define RSPH264_CACHE_SKIP_SOURCE  (1<<0)

// Cache flag: Do not force cache invalidation of destination buffer
#define RSPH264_CACHE_SKIP_DEST    (1<<1)

// Cache flag: Do not force cache writeback/invalidate at all
#define RSPH264_CACHE_SKIP_ALL     (RSPH264_CACHE_SKIP_SOURCE | RSPH264_CACHE_SKIP_DEST)


void rsph264_queue_interpolate_luma(
	int cache_flags,
	const uint8_t *src, uint32_t src_pitch,
	uint8_t *dst, uint32_t dst_pitch,
	uint32_t width, uint32_t height,
	uint32_t dx, uint32_t dy);

void rsph264_queue_interpolate_chroma(
	int cache_flags,
	const uint8_t *src, uint32_t src_pitch,
	uint8_t *dst, uint32_t dst_pitch,
	uint32_t width, uint32_t height,
	uint32_t dx, uint32_t dy);

void rsph264_queue_interpolate_luma_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst, uint32_t dst_pitch,
    uint32_t frame_size,
    uint32_t block_size,
    uint32_t mv, uint32_t pos);

void rsph264_queue_interpolate_chroma_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst, uint32_t dst_pitch,
    uint32_t frame_size,
    uint32_t block_size,
    uint32_t mv, uint32_t pos);

void rsph264_queue_interpolate_all_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst_luma, uint8_t *dst_chroma_1, uint8_t *dst_chroma_2, uint32_t dst_pitch,
    uint32_t frame_size,
    uint32_t block_size,
    uint32_t mv, uint32_t pos);

void rsph264_queue_intrapred_luma_4x4(
    int cache_flags,
    const uint8_t *src_l, const uint8_t *src_u, const uint8_t *src_ul,
    uint8_t *dst, uint32_t left_pitch, uint32_t dst_pitch,
    uint32_t mode, uint32_t availability);

void rsph264_queue_intrapred_luma_16x16(
    int cache_flags,
    const uint8_t *src_l, const uint8_t *src_u, const uint8_t *src_ul,
    uint8_t *dst, uint32_t left_pitch, uint32_t dst_pitch,
    uint32_t mode, uint32_t availability);

void rsph264_queue_intrapred_chroma_8x8(
    int cache_flags,
    const uint8_t *src_l, const uint8_t *src_u, const uint8_t *src_ul,
    uint8_t *dst, uint32_t left_pitch, uint32_t dst_pitch,
    uint32_t mode, uint32_t availability);

void rsph264_queue_set_packed_delta_buffer(
	int cache_flags,
	const uint8_t *src);

void rsph264_queue_set_packed_delta_buffer_if_changed(
	int cache_flags,
	const uint8_t *src);

void rsph264_queue_reset_packed_delta_buffer(void);

const uint8_t* rsph264_cur_delta_buffer(const uint8_t *buf);

void rsph264_queue_dequant_transform_residual(
	int cache_flags,
	uint8_t *dst, uint32_t dst_pitch,
	const int16_t *dc, uint32_t qp, uint32_t ac);

void rsph264_queue_transform_dequant_lumadc(
	int cache_flags,
	int16_t *dst, uint32_t qp);

void rsph264_queue_transform_dequant_chromadc(
	int cache_flags,
	int16_t *dst, uint32_t qp);

void rsph264_queue_process_luma_inter_residual(
	int cache_flags,
	uint8_t *dst, uint32_t dst_pitch,
	const int16_t *dc, uint32_t qp, uint32_t totalCoeffMask);

void rsph264_queue_process_chroma_residual(
	int cache_flags,
	uint8_t *dst1, uint8_t *dst2, uint32_t dst_pitch,
	uint32_t qp, uint32_t totalCoeffMask);

void rsph264_queue_process_luma_intra4_residual(
    int cache_flags,
    const uint8_t *src, uint8_t *dst,
    uint32_t src_pitch, uint32_t dst_pitch,
    const uint8_t *modeAvail,
    uint32_t qp, uint32_t totalCoeffMask);

void rsph264_queue_process_luma_intra16_residual(
    int cache_flags,
    const uint8_t *src, uint8_t *dst,
    uint32_t src_pitch, uint32_t dst_pitch,
    const uint32_t mode, const uint32_t availability,
    uint32_t qp, uint32_t totalCoeffMask);

void rsph264_queue_write_macroblock(
	int cache_flags,
	const uint8_t *src,
	uint8_t *dst_luma, uint8_t *dst_cb, uint8_t *dst_cr,
	uint32_t mb_width);

void rsph264_queue_set_cavlc_buffer(
	int cache_flags,
	const uint8_t *src,
    uint8_t bitOff);

void rsph264_queue_set_cavlc_buffer_if_changed(
    int cache_flags,
    const uint8_t *src,
    uint8_t bitOff);

void rsph264_queue_decode_coeffs_pair_cavlc(
    int cache_flags,
    uint8_t nc,
    uint8_t maxcoeffs);

void rsph264_queue_decode_coeffs_pair_cavlc(
    int cache_flags,
    uint8_t nc,
    uint8_t maxcoeffs);

void rsph264_queue_decode_chromadc_coeffs_pair_cavlc(
    int cache_flags
);

int rsph264_DEBUG_cavlc(uint8_t* packed_delta, int *len, uint8_t *numCoeffs, uint8_t *totalZeroes);

void rsph264_queue_decode_residual(
    int cache_flags,
    uint8_t *packedCoeff, uint8_t *totalCoeff, 
    const uint8_t *totalCoeffLeft, const uint8_t *totalCoeffUp,
    uint32_t codedBlockPattern, int is16x16
);

int rsph264_cur_cavlc_buffer(const uint8_t **ptr, int *bitOff);

// debugging functions (only for tests)
void rsph264_queue_debug_random_status(void);
void rsph264_queue_debug_load_overlay(const char* ovlname);

#endif
