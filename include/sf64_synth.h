/**
 * @file sf64_synth.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Polyphonic synthesizer for SF64 sound banks
 * @ingroup mixer
 *
 * A synthesizer binds an #sf64_bank_t to a range of mixer channels and plays
 * notes by matching regions of a per-MIDI-channel program against key and
 * velocity.
 *
 * The bank is not owned: it must outlive the synthesizer. Create with
 * #sf64_synth_create, reserve mixer channels with #sf64_synth_set_channels,
 * choose programs with #sf64_synth_set_program, then drive notes with
 * #sf64_synth_note_on / #sf64_synth_note_off on a MIDI channel (0–15).
 *
 * A single note-on can start several regions at once (SF2 layers). All of
 * those voices share one note identity; #sf64_synth_note_off releases the
 * oldest still-held identity for that key on that MIDI channel. Allocation
 * is atomic: if there are not enough free channels for every matching region,
 * nothing starts.
 *
 * Channel controllers (volume, expression, pan, sustain pedal, pitch bend)
 * apply to future notes and to voices already sounding on that MIDI channel.
 * A program change affects only subsequent note-ons; sounding voices keep
 * their region. SF2 exclusive class: starting a region with a non-zero group
 * immediately stops other voices of the same preset and group.
 *
 * Amp envelopes use the mixer's volume ramps (delay → attack → hold → decay →
 * sustain → release, with SF2 keynum scaling on hold/decay). Advance them with
 * #sf64_synth_process by the same output-sample counts you pass to
 * #mixer_poll; a single call drains every overdue phase. One-shot samples that
 * finish early free their mixer channel on the next process. There is no voice
 * stealing: when every reserved channel is in use, further note-ons return 0
 * and leave existing voices alone (exclusive-class victims are only choked
 * after allocation is known to succeed).
 */
#ifndef LIBDRAGON_SF64_SYNTH_H
#define LIBDRAGON_SF64_SYNTH_H

#include <stdbool.h>
#include <stdint.h>
#include "sf64.h"
#include "midi_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Number of MIDI channels tracked by #sf64_synth_t. */
#define SF64_MIDI_CHANNELS  16

/** @brief SF64 synthesizer. */
typedef struct sf64_synth_s sf64_synth_t;

/**
 * @brief Return the synthesizer as a #midi_target_t for MID64 playback.
 *
 * The synth embeds a #midi_target_t as its first member. Controllers mapped:
 * CC7 volume, CC11 expression, CC10 pan, CC64 sustain, CC0 bank MSB (applied
 * on the next program change). #midi_target_ops_t::reset stops all voices;
 * #midi_target_ops_t::finish releases them musically (end of song).
 * #midi_target_ops_t::process advances #sf64_synth_process to the absolute
 * sample clock used by MID64.
 */
midi_target_t *sf64_synth_midi_target(sf64_synth_t *synth);

/**
 * @brief Create a synthesizer that plays from @p bank.
 *
 * The bank is borrowed, not copied. Each of the #SF64_MIDI_CHANNELS starts
 * at bank/program 0 (resolved if present), except MIDI channel 10 (index 9)
 * which defaults to percussion bank 128. Volume/expression 127, pan center,
 * pitch bend center, pitch range ±200 cents. Call #sf64_synth_set_channels
 * before the first #sf64_synth_note_on. Free with #sf64_synth_close.
 *
 * @param bank   Bank returned by #sf64_load
 * @return       New synthesizer
 */
sf64_synth_t *sf64_synth_create(sf64_bank_t *bank);

/**
 * @brief Stop all sounding notes and free the synthesizer.
 *
 * Does not close the bank.
 *
 * @param synth  Synthesizer returned by #sf64_synth_create
 */
void sf64_synth_close(sf64_synth_t *synth);

/**
 * @brief Reserve a contiguous range of mixer channels for this synthesizer.
 *
 * Stops any notes currently sounding. Must be called before
 * #sf64_synth_note_on. Polyphony is limited to @p num_channels (no stealing).
 *
 * @param synth           Synthesizer
 * @param first_channel   First mixer channel index (see #mixer_init)
 * @param num_channels    Number of channels to reserve (`>= 1`)
 */
void sf64_synth_set_channels(sf64_synth_t *synth, int first_channel, int num_channels);

/**
 * @brief Select bank and program for subsequent notes on @p midi_channel.
 *
 * Does not affect notes that are already sounding on that channel.
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param bank           MIDI bank (0–127 melodic, 128 percussion)
 * @param program        MIDI program (0–127)
 * @return               true if the preset exists in the bank
 */
bool sf64_synth_set_program(sf64_synth_t *synth, int midi_channel,
	int bank, int program);

/**
 * @brief Set MIDI CC7 volume for @p midi_channel (0–127).
 *
 * Updates sounding voices on that channel (sustain immediately; attack,
 * decay, and release keep their remaining ramp time).
 */
void sf64_synth_set_volume(sf64_synth_t *synth, int midi_channel, int volume);

/**
 * @brief Set MIDI CC11 expression for @p midi_channel (0–127).
 *
 * Same voice-update rules as #sf64_synth_set_volume.
 */
void sf64_synth_set_expression(sf64_synth_t *synth, int midi_channel, int expression);

/**
 * @brief Set MIDI CC10 pan for @p midi_channel (0 = left, 64 = center, 127 = right).
 *
 * Combined with each region's SF2 pan. Same voice-update rules as volume.
 */
void sf64_synth_set_pan(sf64_synth_t *synth, int midi_channel, int pan);

/**
 * @brief Set pitch bend for @p midi_channel (0–16383, center 8192).
 *
 * Recalculates #mixer_ch_set_freq for sounding voices on that channel.
 * The bend spans the channel pitch range (default ±200 cents).
 */
void sf64_synth_set_pitch_bend(sf64_synth_t *synth, int midi_channel, int pitch_bend);

/**
 * @brief Set MIDI CC64 sustain pedal for @p midi_channel (0–127).
 *
 * Values `>= 64` hold the pedal. Note-offs while held mark voices but do not
 * enter release until the pedal is released (`< 64`). Sustain loops stay
 * enabled until release actually begins.
 */
void sf64_synth_set_sustain(sf64_synth_t *synth, int midi_channel, int value);

/**
 * @brief Start a note on @p midi_channel's current program.
 *
 * Starts every region that matches @p key and @p velocity, each on its own
 * mixer channel, all sharing one note identity. Volume ramps through attack
 * then decay to each region's sustain level, scaled by channel volume,
 * expression, and pan.
 *
 * Allocation is all-or-nothing: if any matching region cannot get a free
 * channel, none are started. A velocity of 0 is treated as #sf64_synth_note_off.
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param key            MIDI key (0–127)
 * @param velocity       MIDI velocity (1–127; 0 = note-off)
 * @return               Note identity (`> 0`) if at least one voice started, else 0
 */
uint32_t sf64_synth_note_on(sf64_synth_t *synth, int midi_channel,
	int key, int velocity);

/**
 * @brief Release the oldest still-held note identity for @p key on @p midi_channel.
 *
 * All voices that share that identity enter release together, unless the
 * sustain pedal is down — then they keep sounding until
 * #sf64_synth_set_sustain releases the pedal. Call #sf64_synth_process until
 * fades end so the channels are freed. A zero-length release stops at once.
 * Repeated calls peel stacked note-ons on the same key from oldest to newest.
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param key            MIDI key that should be released
 */
void sf64_synth_note_off(sf64_synth_t *synth, int midi_channel, int key);

/**
 * @brief Tell the synthesizer that @p num_samples of audio have elapsed.
 *
 * Call this with the same sample counts you feed to #mixer_poll (or from a
 * #mixer_add_event callback). It finishes delay/attack/hold/decay/release when
 * their time is up (catching up multiple phases if needed) and reclaims
 * channels whose oneshot playback has ended. Pass 0 to only query the next
 * deadline without advancing time.
 *
 * @param synth        Synthesizer
 * @param num_samples  Elapsed output samples (`>= 0`)
 * @return             Samples until the next attack/release end, or `< 0`
 *                     if nothing is pending
 */
int sf64_synth_process(sf64_synth_t *synth, int num_samples);

#ifdef __cplusplus
}
#endif

#endif
