/**
 * @file mixer_reverb.h
 * @brief Global Schroeder reverb effect for the audio mixer
 * @ingroup mixer
 *
 * Applies a single global reverb to the mixer's stereo output via a preset
 * table and independent left/right wet levels. The DSP runs on the CPU today
 * and is structured to be swapped for an RSP overlay later without changing
 * this API.
 *
 * Lifecycle:
 *   mixer_init();
 *   mixer_reverb_init(audio_get_frequency());
 *   ...
 *   mixer_reverb_set_type(4);
 *   mixer_reverb_set_depth(0.5f, 0.5f);
 *   mixer_reverb_set_enabled(true);
 *   ...
 *   mixer_reverb_close();   // optional, also handled by mixer_close()
 */

#ifndef __LIBDRAGON_MIXER_REVERB_H
#define __LIBDRAGON_MIXER_REVERB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the reverb effect at the mixer's output sample rate.
 *
 * Allocates the delay-line work area in RDRAM, sized for the largest preset
 * at the supplied rate (~14 KiB at 32 kHz, scales linearly with sample_rate).
 * Idempotent: a second call closes and re-allocates. Must be called after
 * mixer_init().
 *
 * The algorithm is sample-rate independent: presets keep the same RT60 (in
 * seconds) and the same brightness (in Hz) at any reasonable rate.
 */
void mixer_reverb_init(int sample_rate);

/** @brief Tear down the reverb effect and free its work area. */
void mixer_reverb_close(void);

/** @brief Master enable/disable. Disabled state is a zero-cost fast path. */
void mixer_reverb_set_enabled(bool enable);

/**
 * @brief Select a preset by index (0..9).
 *
 * Preset 0 is OFF (equivalent to set_enabled(false)). The remaining presets
 * range from short rooms through long halls and discrete echoes — see the
 * preset table in mixer_reverb.c for the character of each entry.
 *
 * Changing preset zeroes the delay-line state to avoid bleed.
 */
void mixer_reverb_set_type(int preset);

/**
 * @brief Set master wet level per stereo channel.
 *
 * Both arguments are clamped to [0, 1].
 */
void mixer_reverb_set_depth(float left, float right);

/** @brief Zero the delay-line state without changing preset or depth. */
void mixer_reverb_clear_work_area(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBDRAGON_MIXER_REVERB_H */
