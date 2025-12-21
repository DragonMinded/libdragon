/**
 * @file profile.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef PROFILE_H
#define PROFILE_H

#define LIBDRAGON_PROFILE 1

// Global enable/disable of libdragon profiler.
//
// You can force this to 0 at compile-time if you want
// to keep PROFILE() calls in your code but remove references
// everywhere.
#ifndef LIBDRAGON_PROFILE
#ifdef N64
	#define LIBDRAGON_PROFILE     1
#else
	// If we're compiling the same codebase on PC, just ignore
	// profile calls.
	#define LIBDRAGON_PROFILE     0
#endif
#endif

#include "n64sys.h"
#include <stdint.h>

typedef enum {
	PS_MPEG,
	PS_MPEG_FINDSTART,
	PS_MPEG_HASSTART,
	PS_MPEG_DECODESLICE,
	PS_MPEG_MB,
	PS_MPEG_MB_MV,
	PS_MPEG_MB_PREDICT,
	PS_MPEG_MB_DECODE,
	PS_MPEG_MB_DECODE_DC,
	PS_MPEG_MB_DECODE_AC,
	PS_MPEG_MB_DECODE_AC_VLC,
	PS_MPEG_MB_DECODE_AC_CODE,
	PS_MPEG_MB_DECODE_AC_DEQUANT,
	PS_MPEG_MB_DECODE_BLOCK,
	PS_MPEG_MB_DECODE_BLOCK_IDCT,
	PS_YUV,
	PS_AUDIO,
	PS_SYNC,

	PS_EMU,
	PS_EMU_AY8910,
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
	PS_H264_SYNC,
	PS_H264_SYNC_OVL,
	PS_MP2,
	PS_MP2_DECODE,
	PS_MP2_TRANSFORM,
	PS_MIXER,

	PS_NUM_SLOTS
} ProfileSlot;

// Internal data structures, exposed here to allow inlining of profile_record
extern uint64_t slot_frame_cur[PS_NUM_SLOTS];

void profile_init(void);
void profile_next_frame(void);
void profile_dump(void);
void profile_set_target_fps(float fps);
static inline void profile_record(ProfileSlot slot, int32_t len) {
	// High part: profile record
	// Low part: number of occurrences
	slot_frame_cur[slot] += ((int64_t)len << 32) + 1;
}

#if LIBDRAGON_PROFILE
	#define PROFILE_START(slot, n) \
		uint32_t __prof_start_##slot##_##n = TICKS_READ(); \

	#define PROFILE_STOP(slot, n) \
		uint32_t __prof_stop_##slot##_##n = TICKS_READ(); \
		profile_record(slot, TICKS_DISTANCE(__prof_start_##slot##_##n, __prof_stop_##slot##_##n));
#else
	#define PROFILE_START(slot, n)  ({ })
	#define PROFILE_STOP(slot, n)   ({ })

#endif /* LIBDRAGON_PROFILE */

#endif /* PROFILE_H */
