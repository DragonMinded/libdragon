/**
 * @file midi_target.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Abstract MIDI synthesizer target for MID64 playback
 * @ingroup mixer
 *
 * A #midi_target_t is a vtable of channel-voice callbacks used by the MID64
 * player. The player knows only MIDI events and timing; the target may be an
 * SF64 synth, a test double, a logger, or another backend.
 *
 * @p poly_pressure and @p channel_pressure may be NULL if the target does not
 * implement them. @p now is an absolute sample clock during playback.
 */
#ifndef LIBDRAGON_MIDI_TARGET_H
#define LIBDRAGON_MIDI_TARGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque MIDI target (synth backend). */
typedef struct midi_target_s midi_target_t;

/** @brief Operations implemented by a MIDI target. */
typedef struct {
	void (*note_on)(midi_target_t *t, int ch, int key, int vel, int64_t now);
	void (*note_off)(midi_target_t *t, int ch, int key, int vel, int64_t now);
	void (*control_change)(midi_target_t *t, int ch, int cc, int value, int64_t now);
	void (*program_change)(midi_target_t *t, int ch, int program, int64_t now);
	void (*pitch_bend)(midi_target_t *t, int ch, int value, int64_t now);
	void (*poly_pressure)(midi_target_t *t, int ch, int key, int pressure, int64_t now);
	void (*channel_pressure)(midi_target_t *t, int ch, int pressure, int64_t now);
	void (*reset)(midi_target_t *t, int64_t now);
	/** Advance synth-internal deadlines; return next absolute sample, or INT64_MAX. */
	int64_t (*process)(midi_target_t *t, int64_t now);
} midi_target_ops_t;

struct midi_target_s {
	const midi_target_ops_t *ops;	///< Backend operations
};

#ifdef __cplusplus
}
#endif

#endif
