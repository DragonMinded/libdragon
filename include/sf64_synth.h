/**
 * @file sf64_synth.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Polyphonic synthesizer for SF64 sound banks
 * @ingroup mixer
 *
 * A synthesizer binds an #sf64_bank_t to a range of mixer channels and plays
 * notes by matching the selected preset's regions against key and velocity.
 *
 * The bank is not owned: it must outlive the synthesizer. Create with
 * #sf64_synth_create, reserve mixer channels with #sf64_synth_set_channels,
 * select a preset with #sf64_synth_set_preset, then drive notes with
 * #sf64_synth_note_on / #sf64_synth_note_off.
 *
 * Amp envelopes use the mixer's volume ramps (attack into the note, release
 * to silence). Advance them with #sf64_synth_process by the same output-sample
 * counts you pass to #mixer_poll — there is no absolute clock. The synthesizer
 * does not register a mixer event; the application calls process from its own
 * loop or from a #mixer_add_event callback.
 *
 * There is no voice stealing: when every reserved channel is in use, further
 * note-ons are ignored. A new note-on on a key that is already sounding
 * replaces the previous voice for that key.
 */
#ifndef LIBDRAGON_SF64_SYNTH_H
#define LIBDRAGON_SF64_SYNTH_H

#include <stdbool.h>
#include "sf64.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SF64 synthesizer. */
typedef struct sf64_synth_s sf64_synth_t;

/**
 * @brief Create a synthesizer that plays from @p bank.
 *
 * The bank is borrowed, not copied. Call #sf64_synth_set_channels before
 * the first #sf64_synth_note_on. Free with #sf64_synth_close.
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
 * @brief Select the preset used by subsequent note-ons.
 *
 * Does not affect notes that are already sounding. Equivalent to a MIDI
 * bank/program change on a single channel.
 *
 * @param synth       Synthesizer
 * @param midi_bank   MIDI bank (0–127 melodic, 128 percussion)
 * @param program     MIDI program (0–127)
 * @return            true if the preset exists in the bank
 */
bool sf64_synth_set_preset(sf64_synth_t *synth, int midi_bank, int program);

/**
 * @brief Start a note on the current preset.
 *
 * Finds the first region of the preset that matches @p key and @p velocity,
 * stops any previous voice on the same key, allocates a free mixer channel,
 * and plays that region's sample at the SF2 pitch. Volume ramps through
 * attack then decay to the region's sustain level. Returns false if no region
 * matches or no channel is free. A velocity of 0 is treated as
 * #sf64_synth_note_off.
 *
 * @param synth     Synthesizer
 * @param key       MIDI key (0–127)
 * @param velocity  MIDI velocity (1–127; 0 = note-off)
 * @return          true if a voice was started
 */
bool sf64_synth_note_on(sf64_synth_t *synth, int key, int velocity);

/**
 * @brief Release every active voice that was started with @p key.
 *
 * Volume fades out over each region's release; call #sf64_synth_process until
 * those fades end so the channels are freed. A zero-length release stops at
 * once.
 *
 * @param synth  Synthesizer
 * @param key    MIDI key that should be released
 */
void sf64_synth_note_off(sf64_synth_t *synth, int key);

/**
 * @brief Tell the synthesizer that @p num_samples of audio have elapsed.
 *
 * Call this with the same sample counts you feed to #mixer_poll (or from a
 * #mixer_add_event callback). It finishes attack, decay, or release when their
 * time is up. Pass 0 to only query the next deadline without advancing time.
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
