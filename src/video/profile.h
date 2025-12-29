/**
 * @file profile.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef PROFILE_H
#define PROFILE_H

#include "n64sys.h"
#include <stdint.h>

#ifndef LIBDRAGON_PROFILE
#define LIBDRAGON_PROFILE 1
#endif

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

	PS_NUM_SLOTS
} ProfileSlot;

typedef struct {
	/**
	 * @brief Number of profiling slots (default: 128)
	 * 
	 * This is the number of different categories that can be
	 * used to profile code sections.
	 */
	int num_slots;

	/**
	 * @brief Interval in seconds to dump profiling info to stderr (default: 5.0s)
	 * 
	 * Use a negative number to disable periodic dumps.
	 */
	float dump_stderr_interval;
} profile_parms_t;

/** @brief Parameters for the profile OSD pane */
typedef struct {
	/** @brief Side of the screen to use (0=right (default), 1=left) */
	int side;

	/** @brief Size of the pane (default: 20% of the screen) */
	int size;
} profile_osdparms_t;

void profile_init(profile_parms_t *parms);
void profile_close(void);
void profile_reset(void);
void profile_register(int slot, const char *name, int nest_level);
void profile_next_frame(void);
void profile_dump(void);
void profile_set_target_fps(float fps);

inline void profile_record(int slot, int32_t len) {
	extern uint64_t *__profile_counters;
	if (__profile_counters) __profile_counters[slot] += ((int64_t)len << 32) + 1;
}

#if LIBDRAGON_PROFILE
	#define PROFILE_START(slot, n) \
		int64_t __prof_start_##slot##_##n = TICKS_READ() - get_system_ticks(); \
		MEMORY_BARRIER();

	#define PROFILE_STOP(slot, n) \
		MEMORY_BARRIER(); \
		profile_record(slot, TICKS_READ() - get_system_ticks() - __prof_start_##slot##_##n);

	#define PROFILE_SCOPE(slot) \
		for (int64_t __prof_start_##slot = TICKS_READ() - get_system_ticks(); ; ) \
			for (int __prof_once_##slot = ({ MEMORY_BARRIER(); 1; }); __prof_once_##slot; __prof_once_##slot = 0, \
				({ MEMORY_BARRIER(); }), \
				profile_record(slot, TICKS_READ() - get_system_ticks() - __prof_start_##slot))
#else
	#define PROFILE_START(slot, n)  ((void)(false), false)
	#define PROFILE_STOP(slot, n)   ((void)(false), false)
	#define PROFILE_SCOPE(slot)     for (bool __prof_once_##slot = true; __prof_once_##slot; __prof_once_##slot = false)

#endif /* LIBDRAGON_PROFILE */

#endif /* PROFILE_H */
