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
 * During mixer-driven playback, @p now is an absolute sample time. With
 * #mid64player_decode_next, @p now is the absolute MIDI tick instead.
 * Optional callbacks may be NULL.
 */
#ifndef LIBDRAGON_MIDI_TARGET_H
#define LIBDRAGON_MIDI_TARGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct midi_target_s;

/** @brief System-level MIDI reset kind (Universal SysEx, etc.). */
typedef enum {
	MIDI_SYSTEM_GM1,	///< General MIDI Level 1 System On
} midi_system_t;

/** @brief Operations implemented by a MIDI target. */
typedef struct midi_target_ops_s {
	/**
	 * @brief Start a note on MIDI channel @p ch.
	 *
	 * @param t    Target
	 * @param ch   MIDI channel (0–15)
	 * @param key  MIDI key (0–127)
	 * @param vel  Velocity (1–127; 0 is normalized to note-off by the converter)
	 * @param now  Absolute time (see file overview)
	 */
	void (*note_on)(struct midi_target_s *t, int ch, int key, int vel, int64_t now);

	/**
	 * @brief Release a note on MIDI channel @p ch.
	 *
	 * @param t    Target
	 * @param ch   MIDI channel (0–15)
	 * @param key  MIDI key (0–127)
	 * @param vel  Release velocity (0–127)
	 * @param now  Absolute time (see file overview)
	 */
	void (*note_off)(struct midi_target_s *t, int ch, int key, int vel, int64_t now);

	/**
	 * @brief MIDI control change on channel @p ch.
	 *
	 * @param t      Target
	 * @param ch     MIDI channel (0–15)
	 * @param cc     Controller number (0–127)
	 * @param value  Controller value (0–127)
	 * @param now    Absolute time (see file overview)
	 */
	void (*control_change)(struct midi_target_s *t, int ch, int cc, int value, int64_t now);

	/**
	 * @brief MIDI program change on channel @p ch.
	 *
	 * @param t        Target
	 * @param ch       MIDI channel (0–15)
	 * @param program  Program number (0–127)
	 * @param now      Absolute time (see file overview)
	 */
	void (*program_change)(struct midi_target_s *t, int ch, int program, int64_t now);

	/**
	 * @brief MIDI pitch bend on channel @p ch.
	 *
	 * @param t      Target
	 * @param ch     MIDI channel (0–15)
	 * @param value  14-bit bend (0–16383, center 8192)
	 * @param now    Absolute time (see file overview)
	 */
	void (*pitch_bend)(struct midi_target_s *t, int ch, int value, int64_t now);

	/**
	 * @brief Polyphonic key pressure on channel @p ch. May be NULL.
	 *
	 * @param t         Target
	 * @param ch        MIDI channel (0–15)
	 * @param key       MIDI key (0–127)
	 * @param pressure  Pressure (0–127)
	 * @param now       Absolute time (see file overview)
	 */
	void (*poly_pressure)(struct midi_target_s *t, int ch, int key, int pressure, int64_t now);

	/**
	 * @brief Channel pressure on channel @p ch. May be NULL.
	 *
	 * @param t         Target
	 * @param ch        MIDI channel (0–15)
	 * @param pressure  Pressure (0–127)
	 * @param now       Absolute time (see file overview)
	 */
	void (*channel_pressure)(struct midi_target_s *t, int ch, int pressure, int64_t now);

	/**
	 * @brief End of sequence: release sounding notes musically. May be NULL.
	 *
	 * Unlike #midi_target_ops_t::reset, this must not hard-cut voices; held
	 * notes and sustain should enter release so tails can finish.
	 *
	 * @param t    Target
	 * @param now  Absolute time (see file overview)
	 */
	void (*finish)(struct midi_target_s *t, int64_t now);

	/**
	 * @brief Immediate silence and restore of channel controllers. May be NULL.
	 *
	 * Used for stop, loop restart, and similar hard resets.
	 *
	 * @param t    Target
	 * @param now  Absolute time (see file overview)
	 */
	void (*reset)(struct midi_target_s *t, int64_t now);

	/**
	 * @brief Apply a system-level reset (e.g. GM1 System On). May be NULL.
	 *
	 * @param t       Target
	 * @param system  Which system reset was received
	 * @param now     Absolute time (see file overview)
	 */
	void (*system_reset)(struct midi_target_s *t, midi_system_t system, int64_t now);

	/**
	 * @brief Advance the target to absolute time @p now. May be NULL.
	 *
	 * @param t    Target
	 * @param now  Absolute time (see file overview)
	 * @return     Next absolute time at which the target needs attention,
	 *             or `INT64_MAX` if nothing is pending
	 */
	int64_t (*process)(struct midi_target_s *t, int64_t now);
} midi_target_ops_t;

/**
 * @brief Opaque MIDI target (synth backend).
 */
typedef struct midi_target_s {
	const midi_target_ops_t *ops;	///< Backend operations
} midi_target_t;

#ifdef __cplusplus
}
#endif

#endif
