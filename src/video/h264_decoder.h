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

#endif
