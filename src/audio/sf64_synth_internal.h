/**
 * @file sf64_synth_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Shared state and helpers for the SF64 synthesizer
 * @ingroup mixer
 *
 * Shared by the MIDI control plane (sf64_midi.c) and the voice engine
 * (sf64_synth.c). Not part of the public API.
 */
#ifndef LIBDRAGON_SF64_SYNTH_INTERNAL_H
#define LIBDRAGON_SF64_SYNTH_INTERNAL_H

#include "sf64_synth.h"
#include "sf64_internal.h"
#include "mixer.h"
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

/** GM drum channel (MIDI channel 10 → index 9). */
#define SF64_DRUM_CHANNEL  9
/** Default SF2 percussion bank for #SF64_DRUM_CHANNEL. */
#define SF64_DRUM_BANK     128

/** @brief Envelope phase of a synth voice. */
typedef enum {
	SF64_VOICE_OFF,      ///< Idle; mixer channel is free
	SF64_VOICE_DELAY,    ///< Silent delay before attack
	SF64_VOICE_ATTACK,   ///< Ramping to peak gain
	SF64_VOICE_HOLD,     ///< Holding peak before decay
	SF64_VOICE_DECAY,    ///< Ramping from peak to the sustain level
	SF64_VOICE_SUSTAIN,  ///< Holding the sustain level until note-off
	SF64_VOICE_RELEASE,  ///< Ramping to silence after note-off
} sf64_voice_phase_t;

/** @brief Per-MIDI-channel controllers and program */
typedef struct {
	uint16_t bank;              ///< SF2 bank for program change (0–128)
	uint8_t bank_msb;           ///< CC0 bank select MSB
	uint8_t bank_lsb;           ///< CC32 bank select LSB
	uint8_t program;            ///< MIDI program
	int16_t preset_index;       ///< Index into sf64_bank_t.presets, or -1
	uint8_t volume;             ///< CC7 (0–127); not cleared by CC121
	uint8_t expression;         ///< CC11 (0–127)
	uint8_t pan;                ///< CC10 (0–127, 64 = center)
	uint8_t modulation;         ///< CC1 (0–127)
	uint8_t sustain;            ///< CC64 sustain pedal (`>= 64` = down)
	uint8_t rpn_msb;            ///< CC101; 0x7F/0x7F = null
	uint8_t rpn_lsb;            ///< CC100
	uint8_t data_entry_msb;     ///< CC6 Data Entry MSB
	uint8_t data_entry_lsb;     ///< CC38 Data Entry LSB
	uint16_t pitch_bend;        ///< 14-bit bend (center 8192)
	int16_t pitch_range_cents;  ///< Full bend span in cents (default 200)
	bool muted;                 ///< User mute; not cleared by MIDI reset
} sf64_midi_channel_t;

/** @brief One voice bound to a mixer channel. */
typedef struct {
	sf64_voice_phase_t phase; ///< Current amp-envelope phase
	sf64_voice_phase_t mod_phase; ///< Modulation-envelope phase (#SF64_VOICE_OFF if unused)
	int8_t midi_channel;      ///< MIDI channel that owns this voice, or -1
	int8_t key;               ///< MIDI key that started this voice, or -1
	int16_t preset_index;     ///< Preset used at note-on (for exclusive class)
	int region_index;         ///< Index into sf64_bank_t.regions
	uint32_t note_id;         ///< Note identity shared by layered voices; 0 if off
	int64_t deadline;         ///< Absolute sample time of the next amp phase; INT64_MAX = none
	int64_t mod_deadline;     ///< Absolute sample time of the next mod phase; INT64_MAX = none
	float base_gain;          ///< Region × velocity gain, fixed at note-on
	float channel_gain;       ///< MIDI volume × expression
	float envelope_gain;      ///< Amp-envelope target level for the current phase
	float mod_env_level;      ///< Mod-envelope target level for the current phase (0…1)
	bool sustain_loop;        ///< True if the region uses #SF64_LOOP_SUSTAIN
	bool key_released;        ///< Note-off received (may still be held by pedal)
	bool held_by_sustain;     ///< Sounding only because the sustain pedal is down
} sf64_voice_t;

/**
 * @brief Opaque synthesizer state.
 *
 * Immutable bank pointer; mutable MIDI channels and mixer voices. Not part of
 * the public API — clients use the typedef in sf64_synth.h.
 */
typedef struct sf64_synth_s {
	midi_target_t midi_target;    ///< Must be first; see #sf64_synth_midi_target
	sf64_bank_t *bank;            ///< Bank this synth plays from
	int first_channel;            ///< First mixer channel of the allocated range
	int num_channels;             ///< Number of mixer channels reserved for voices
	int priority;                 ///< Base voice-stealing priority for this synth
	int64_t now;                  ///< Absolute sample time advanced by #sf64_synth_process
	uint32_t next_note_id;        ///< Next note identity to assign (`>= 1`)
	sf64_synth_mode_t mode;       ///< Native vs GM1 channel semantics
	sf64_midi_channel_t midi[SF64_MIDI_CHANNELS]; ///< Per-MIDI-channel state
	sf64_voice_t voices[MIXER_MAX_CHANNELS]; ///< Per-mixer-channel voice state
} sf64_synth_t;

/** True while the voice is in a pre-release envelope phase. */
static inline bool voice_sounding(const sf64_voice_t *v)
{
	return v->phase == SF64_VOICE_DELAY ||
		v->phase == SF64_VOICE_ATTACK ||
		v->phase == SF64_VOICE_HOLD ||
		v->phase == SF64_VOICE_DECAY ||
		v->phase == SF64_VOICE_SUSTAIN;
}

/** True while the key is still down (note-off not yet received). */
static inline bool voice_key_down(const sf64_voice_t *v)
{
	return voice_sounding(v) && !v->key_released;
}

/** #midi_target_ops_t for this synthesizer (defined in sf64_midi.c). */
extern const midi_target_ops_t sf64_midi_ops;

/** Reset every MIDI channel's program and controllers to defaults. */
void midi_channels_reset(sf64_synth_t *synth);

/** Stop one mixer voice and mark it free. */
void voice_stop(sf64_synth_t *synth, int ch);
/** Stop every voice in the synthesizer's channel range. */
void voices_stop_all(sf64_synth_t *synth);
/** Begin the amp (and mod) release phase for a sounding voice. */
void voice_enter_release(sf64_synth_t *synth, int ch);
/** Reapply volume×expression×pan to all voices on a MIDI channel. */
void midi_apply_vol(sf64_synth_t *synth, int midi_channel);
/** Retarget pitch for all voices on a MIDI channel after a bend change. */
void midi_apply_bend(sf64_synth_t *synth, int midi_channel);

#endif
