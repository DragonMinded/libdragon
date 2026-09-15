/**
 * @file h264_decoder.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_VIDEO_H264_DECODER_H
#define LIBDRAGON_VIDEO_H264_DECODER_H

// Activate N64 specific codepath
#define H264BSD_N64         1
#define H264BSD_N64_INTRA   1     // Intraprediction on RSP

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

#include "profile.h"
#include "h264_decoder/h264bsd_decoder.h"
#include "h264_decoder/h264bsd_storage.h"

// Profile slots, listed in call order: profile_dump() prints them by slot
// number, and indents each one by the nesting level declared in
// __h264_profile_init(). Keep the two in sync when adding a slot.
typedef enum {
	PS_H264 = PROFILE_SLOT_H264,    // h264bsdDecode
	PS_H264_NAL,                    // h264bsdExtractNalUnit + h264bsdDecodeNalUnit
	PS_H264_LAYER,                  // h264bsdDecodeMacroblockLayer (bitstream)
	PS_H264_LAYER_CLEAR,
	PS_H264_LAYER_PRED,             // DecodeMbPred / DecodeSubMbPred
	PS_H264_LAYER_RES,              // DecodeResidual
	PS_H264_LAYER_RES_ENC,          // CAVLC coefficient decoding
	PS_H264_MACROB,                 // h264bsdDecodeMacroblock (reconstruction)
	PS_H264_INTERPRED,              // h264bsdInterPrediction
	PS_H264_INTERPRED_LUMA,         // C fallback interpolation (unused on N64)
	PS_H264_INTERPRED_CHROMA,
	PS_H264_RESIDUAL_LUMA,          // inter luma residual
	PS_H264_INTRAPRED_4X4,          // ProcessIntra4x4Residual
	PS_H264_INTRAPRED_16X16,        // ProcessIntra16x16Residual
	PS_H264_RESIDUAL_CHROMA,        // ProcessChromaResidual
} H264ProfileSlot;

#endif
