/**
 * @file h264_decoder.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_VIDEO_H264_DECODER_H
#define LIBDRAGON_VIDEO_H264_DECODER_H

// Activate N64 specific codepath
#define H264BSD_N64         1
#define H264BSD_N64_INTRA   1     // Intraprediction on RSP
#define H264BSD_N64_CAVLC   0     // CAVLC on RSP

// Disable all code related to concealment (recovering of corrupted data).
// This must be defined before including the h264bsd headers, as it changes
// the layout of mbStorage_t (drops the `decoded` field); every translation
// unit that pulls in the h264 decoder must agree on it, so it lives here.
#define OPTIMIZE_NO_DECODED_FLAG

// Maximum number of macroblocks that the RSP will be able to lag behind the
// CPU, and process in background. This basically specifies how big is the
// mbLayers array above.
// There is currently no explicit sync for this, this number is
// experimental. If the number is too little, some corruption might appear on 
// some frames, especially when the RSP is too slow. We could in theory use
// syncpoints for this.
// One macroblock takes about 1.5KB of memory, so 128 macroblocks is
// about 192KB of RAM.
#define NUM_PARALLEL_MACROBLOCKS 128

#include "h264_decoder/h264bsd_decoder.h"
#include "h264_decoder/h264bsd_storage.h"

typedef enum {
	PS_H264,
	PS_H264_NAL,
	PS_H264_MACROB,
	PS_H264_LAYER,
	PS_H264_LAYER_CLEAR,
	PS_H264_LAYER_PRED,
	PS_H264_LAYER_RES,
	PS_H264_LAYER_RES_ENC,
	PS_H264_RESIDUAL_LUMA,
	PS_H264_RESIDUAL_CHROMA,
	PS_H264_INTRAPRED_4X4,
	PS_H264_INTRAPRED_16X16,
	PS_H264_INTERPRED,
	PS_H264_INTERPRED_LUMA,
	PS_H264_INTERPRED_CHROMA,
} H264ProfileSlot;

#endif
