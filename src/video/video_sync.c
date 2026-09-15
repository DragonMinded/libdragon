/**
 * @file video_sync.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Video sync helper (frameskipping/seek decisions driven by a master clock)
 * @ingroup video
 */

#include "video_sync.h"

#include "debug.h"
#include "utils.h"

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct video_sync_s {
	video_t *v;
	float framerate;
	video_sync_params_t p;

	// Smoothed lag (in frames)
	float lag_filt;
	// Skip budget (in frames)
	float skip_budget;

	double last_master_time;
	double last_seek_time;
} video_sync_t;

// Internal constants
static const float VIDEO_SYNC_DEADZONE_SEC = 0.05f;
static const float VIDEO_SYNC_LAG_SMOOTH_TAU_SEC = 0.20f;
static const float VIDEO_SYNC_SEEK_COOLDOWN_SEC = 0.25f;

static video_sync_params_t video_sync_default_params(void)
{
	return (video_sync_params_t){
		.recovery_time_sec = 1.0f,
		.keyframe_window_sec = 0.20f,
		.max_lag_sec = 0.75f,
	};
}

video_sync_t* video_sync_create(video_t *v, const video_sync_params_t *p)
{
	video_sync_t *s = calloc(1, sizeof(video_sync_t));
	assertf(s, "Out of memory");

	s->v = v;
	s->framerate = video_get_info(v).framerate;
	s->p = video_sync_default_params();
	if (p) {
		if (p->recovery_time_sec != 0.0f) s->p.recovery_time_sec = p->recovery_time_sec;
		if (p->keyframe_window_sec != 0.0f) s->p.keyframe_window_sec = p->keyframe_window_sec;
		if (p->max_lag_sec != 0.0f) s->p.max_lag_sec = p->max_lag_sec;
	}

	// Sanitize parameters
	if (s->p.recovery_time_sec <= 0.0f) s->p.recovery_time_sec = video_sync_default_params().recovery_time_sec;
	// Negative values explicitly disable the behavior.
	if (s->p.keyframe_window_sec < 0.0f) s->p.keyframe_window_sec = 0.0f;
	if (s->p.max_lag_sec < 0.0f) s->p.max_lag_sec = 0.0f;

    video_sync_reset(s, 0);
    return s;
}

void video_sync_destroy(video_sync_t *s)
{
	free(s);
}

void video_sync_reset(video_sync_t *s, int frame_idx)
{
	s->lag_filt = 0.0f;
	s->skip_budget = 0.0f;
	s->last_master_time = 0.0;
	s->last_seek_time = -1e30;
}

static inline int frames_from_sec(float framerate, float sec)
{
	if (sec <= 0) return 0;
	return (int)floorf(sec * framerate + 0.5f);
}

static inline float ema_alpha_from_dt_tau(float dt, float tau)
{
	// 1-pole low-pass alpha derived from time constant tau (stable across dt).
	// alpha = 1 - exp(-dt/tau)
	if (tau <= 0.0f) return 1.0f;
	if (dt <= 0.0f) return 0.0f;
	float a = 1.0f - expf(-dt / tau);
	return CLAMP(a, 0.0f, 1.0f);
}

video_sync_action_t video_sync_step(video_sync_t *s, double master_time_sec, int cur_frame_idx)
{
	video_sync_action_t act = { .kind = VIDEO_SYNC_RENDER_NEXT, .seek_frame = -1 };
    assert(master_time_sec >= 0);
    assertf(master_time_sec >= s->last_master_time, "Master time went backwards");

	// Compute time delta in frames, clamped to avoid explosions
	float dt_frames = (master_time_sec - s->last_master_time) * s->framerate;
    dt_frames = CLAMP(dt_frames, 0.0f, 5.0f);
    s->last_master_time = master_time_sec;

    // Convert master time to target frame index. We want floor here to get the
    // last frame that should have been rendered by now. Nonetheless, we add
    // 1e-9 to avoid treating 123.99999999999997 as 123.
	int target_frame = (int)floor(master_time_sec * (double)s->framerate + 1e-9);

    // Compute how much we are behind the target frame.
	int lag_frames = target_frame - cur_frame_idx;
	if (lag_frames <= 0) {
		// If we're not behind, do not accumulate budget.
		s->skip_budget = 0.0f;
		s->lag_filt = 0.0f;
		return act;
	}

	// Smooth lag in frames
	float lag = (float)lag_frames;
	float smooth_alpha = ema_alpha_from_dt_tau(dt_frames / s->framerate, VIDEO_SYNC_LAG_SMOOTH_TAU_SEC);
	s->lag_filt = s->lag_filt + smooth_alpha * (lag - s->lag_filt);

    // Ignore lag smaller than the deadzone.
	int deadzone_frames = frames_from_sec(s->framerate, VIDEO_SYNC_DEADZONE_SEC);
	float lag_over = s->lag_filt - (float)deadzone_frames;
	if (lag_over < 0.0f) lag_over = 0.0f;

	float recovery_window_frames = s->framerate * s->p.recovery_time_sec;
	if (recovery_window_frames < 1.0f) recovery_window_frames = 1.0f;

	// Accumulate budget softly: recover lag over recovery_time_sec (approximately).
	s->skip_budget += dt_frames * (lag_over / recovery_window_frames);
    s->skip_budget = CLAMP(s->skip_budget, 0.0f, 1024.0f);
	if (s->skip_budget < 1.0f) {
		// Not enough budget to skip anything yet.
		return act;
	}

	// Seek cooldown (best-effort on master time)
	bool seek_allowed = (master_time_sec - s->last_seek_time) >= (double)VIDEO_SYNC_SEEK_COOLDOWN_SEC;
    if (seek_allowed) {

        // If we are too far behind (more than max_lag_sec), seek to the target frame.
        // This is a best-effort to catch up, but it is not guaranteed to succeed.
        int max_lag_frames = frames_from_sec(s->framerate, s->p.max_lag_sec);
        if (max_lag_frames > 0 && lag_frames >= max_lag_frames) {
            act.kind = VIDEO_SYNC_SEEK_AND_RENDER;
            act.seek_frame = target_frame;
            // Budget is considered spent by the seek; clamp to zero.
            s->skip_budget = 0.0f;
            s->last_seek_time = master_time_sec;
            return act;
        }

        // Keyframe-aware seek in a small window (only if it helps and budget can pay it)
        int window_frames = frames_from_sec(s->framerate, s->p.keyframe_window_sec);
        window_frames = MIN(window_frames, lag_frames);
        window_frames = MIN(window_frames, 240);
        if (window_frames > 0) {
            // Try to find a keyframe within the window (and within the budget)
            int kmax = MIN(window_frames, (int)floorf(s->skip_budget));
            for (int k = 1; k <= kmax; k++) {
                if (video_is_seekable(s->v, cur_frame_idx + k)) {
                    // Found a keyframe within the window and within the budget: seek to it.
                    act.kind = VIDEO_SYNC_SEEK_AND_RENDER;
                    act.seek_frame = cur_frame_idx + k;
                    s->skip_budget -= (float)k;
                    if (s->skip_budget < 0.0f) s->skip_budget = 0.0f;
                    s->last_seek_time = master_time_sec;
                    return act;
                }
            }
        }
    }

	// Soft skip (drop one frame)
	act.kind = VIDEO_SYNC_SKIP_NEXT;
	s->skip_budget -= 1.0f;
	if (s->skip_budget < 0.0f) s->skip_budget = 0.0f;
	return act;
}
