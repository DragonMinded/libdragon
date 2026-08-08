/**
 * @file sf64_synth.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Polyphonic synthesizer for SF64 sound banks
 * @ingroup mixer
 *
 * A synthesizer plays notes from an #sf64_bank_t on a range of mixer channels.
 * Create with #sf64_synth_create, reserve channels with #sf64_synth_set_channels,
 * select programs with #sf64_synth_set_program, then drive MIDI channels (0–15)
 * with #sf64_synth_note_on / #sf64_synth_note_off. Advance time with
 * #sf64_synth_process using the same sample counts passed to #mixer_poll.
 *
 * The bank is borrowed and must outlive the synthesizer. For MID64 playback,
 * use #sf64_synth_midi_target.
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

/** @brief MIDI semantics mode for #sf64_synth_t. */
typedef enum {
	SF64_MODE_NATIVE,	///< Arbitrary SF2 banks; program change on all channels
	SF64_MODE_GM1,		///< General MIDI Level 1 (channel 10 = percussion)
} sf64_synth_mode_t;

/** @brief SF64 synthesizer. */
typedef struct sf64_synth_s sf64_synth_t;

/**
 * @brief Return the synthesizer as a #midi_target_t for MID64 playback.
 *
 * Controllers mapped: CC7 volume, CC11 expression, CC10 pan, CC64 sustain,
 * CC0/CC32 bank select, CC6/CC38/CC100/CC101 RPN (pitch bend sensitivity),
 * CC120 All Sound Off, CC121 Reset All Controllers, CC123 All Notes Off.
 * #midi_target_ops_t::system_reset handles GM1 System On.
 */
midi_target_t *sf64_synth_midi_target(sf64_synth_t *synth);

/**
 * @brief Create a synthesizer that plays from @p bank.
 *
 * Starts in #SF64_MODE_NATIVE. Each of the #SF64_MIDI_CHANNELS starts at
 * bank/program 0 if present, except MIDI channel 10 (index 9) which defaults
 * to percussion bank 128. Volume and expression are 127, pan is center, pitch
 * bend is center with a ±200 cent range. Call #sf64_synth_set_channels before
 * the first #sf64_synth_note_on. Free with #sf64_synth_close.
 *
 * @param bank   Bank returned by #sf64_load
 * @return       New synthesizer
 */
sf64_synth_t *sf64_synth_create(sf64_bank_t *bank);

/**
 * @brief Select Native or General MIDI Level 1 semantics.
 *
 * Does not silence voices or reset controllers. GM1 System On both switches
 * to #SF64_MODE_GM1 and performs a full reset via #midi_target_ops_t::system_reset.
 *
 * @param synth  Synthesizer
 * @param mode   Desired mode
 */
void sf64_synth_set_mode(sf64_synth_t *synth, sf64_synth_mode_t mode);

/**
 * @brief Current #sf64_synth_mode_t.
 *
 * @param synth  Synthesizer
 */
sf64_synth_mode_t sf64_synth_get_mode(sf64_synth_t *synth);

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
 * Applies immediately to notes already sounding on that channel.
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param volume         Volume (0–127)
 */
void sf64_synth_set_volume(sf64_synth_t *synth, int midi_channel, int volume);

/**
 * @brief Set MIDI CC11 expression for @p midi_channel (0–127).
 *
 * Same voice-update rules as #sf64_synth_set_volume.
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param expression     Expression (0–127)
 */
void sf64_synth_set_expression(sf64_synth_t *synth, int midi_channel, int expression);

/**
 * @brief Set MIDI CC10 pan for @p midi_channel (0 = left, 64 = center, 127 = right).
 *
 * Combined with each region's pan. Same voice-update rules as volume.
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param pan            Pan (0–127)
 */
void sf64_synth_set_pan(sf64_synth_t *synth, int midi_channel, int pan);

/**
 * @brief Set pitch bend for @p midi_channel (0–16383, center 8192).
 *
 * Updates the pitch of notes already sounding on that channel. The bend spans
 * the channel pitch range (default ±200 cents; changeable via RPN).
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param pitch_bend     14-bit bend value
 */
void sf64_synth_set_pitch_bend(sf64_synth_t *synth, int midi_channel, int pitch_bend);

/**
 * @brief Set MIDI CC64 sustain pedal for @p midi_channel (0–127).
 *
 * Values `>= 64` hold the pedal: note-offs keep sounding until the pedal is
 * released (`< 64`).
 *
 * @param synth          Synthesizer
 * @param midi_channel   MIDI channel (0–15)
 * @param value          Pedal value (0–127)
 */
void sf64_synth_set_sustain(sf64_synth_t *synth, int midi_channel, int value);

/**
 * @brief Start a note on @p midi_channel's current program.
 *
 * Starts every matching region for @p key and @p velocity as one note.
 * Allocation is all-or-nothing: if any matching region cannot get a free
 * channel, none are started. A velocity of 0 is treated as #sf64_synth_note_off.
 * Starting a region with a non-zero exclusive class silences other sounding
 * regions of the same preset and class.
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
 * @brief Release the oldest still-held note for @p key on @p midi_channel.
 *
 * Layered regions of that note release together. While the sustain pedal is
 * down they keep sounding until #sf64_synth_set_sustain releases the pedal.
 * Call #sf64_synth_process so release fades can finish and free channels.
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
 * #mixer_add_event callback). Pass 0 to query the next deadline without
 * advancing time.
 *
 * @param synth        Synthesizer
 * @param num_samples  Elapsed output samples (`>= 0`)
 * @return             Samples until the next scheduled change, or `< 0`
 *                     if nothing is pending
 */
int sf64_synth_process(sf64_synth_t *synth, int num_samples);

#ifdef __cplusplus
}
#endif

#endif
