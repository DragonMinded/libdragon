/**
 * @file video_sync.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Video sync helper (frameskipping/seek decisions driven by a master clock)
 * @ingroup video
 *
 * This module provides a reusable, display-agnostic helper to keep a decoded video
 * in sync with an external master clock (typically audio). It does not perform any
 * rendering or pacing; it only decides what the caller should do (render next,
 * skip next, or seek and render).
 */
#ifndef LIBDRAGON_VIDEO_SYNC_H
#define LIBDRAGON_VIDEO_SYNC_H

#include <stdbool.h>

#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct video_sync_s video_sync_t;
///@endcond

/** @brief Action kind returned by #video_sync_step */
typedef enum {
	/** Decode and render the next frame */
	VIDEO_SYNC_RENDER_NEXT = 0,
	/** Decode the next frame but skip rendering it */
	VIDEO_SYNC_SKIP_NEXT = 1,
	/** Seek to a specified frame (caller should then decode+render) */
	VIDEO_SYNC_SEEK_AND_RENDER = 2,
} video_sync_action_kind_t;

/** @brief Action returned by #video_sync_step */
typedef struct {
	video_sync_action_kind_t kind;
	/** Valid only when kind==VIDEO_SYNC_SEEK_AND_RENDER */
	int seek_frame;
} video_sync_action_t;

/** @brief Parameters controlling sync behavior */
typedef struct video_sync_params_s {
	/**
	 * Desired time to recover from lag (in seconds). Larger => softer recovery.
     *
	 * Default: 1.0
	 */
	float recovery_time_sec;
	/**
	 * If a keyframe exists within this window (in seconds) and budget allows,
	 * prefer a seek over present-skip. Use a negative value to disable
     * keyframe-based seeking.
	 *
	 * Default: 0.20
	 */
	float keyframe_window_sec;
	/**
	 * If lag exceeds this threshold (in seconds), allow a more aggressive seek
	 * towards the current target frame. Use a negative value to disable
     * aggressive seeking.
	 * 
	 * Default: 0.75
	 */
	float max_lag_sec;
} video_sync_params_t;

/**
 * @brief Create a video sync helper.
 *
 * @param v     Video handle. Ownership is not transferred.
 * @param p     Optional parameters (NULL for defaults). Libdragon-style: fields left
 *              at 0 are treated as "use default".
 */
video_sync_t* video_sync_create(video_t *v, const video_sync_params_t *p);

/** @brief Destroy a video sync helper */
void video_sync_destroy(video_sync_t *s);

/**
 * @brief Reset internal state (call after external seek/rewind)
 *
 * @param s         Sync helper
 * @param frame_idx Current frame index after the external operation
 */
void video_sync_reset(video_sync_t *s, int frame_idx);

/**
 * @brief Decide what to do next to keep video in sync with master time.
 *
 * The function is pure decision-making: it does not call #video_next_frame or
 * #video_seek by itself. The caller should execute the returned action.
 *
 * @param s               Sync helper
 * @param master_time_sec Master time in seconds (typically audio clock)
 * @param cur_frame_idx   Current frame index (the next frame that would be decoded/rendered)
 */
video_sync_action_t video_sync_step(video_sync_t *s, double master_time_sec, int cur_frame_idx);

#ifdef __cplusplus
}
#endif

#endif


