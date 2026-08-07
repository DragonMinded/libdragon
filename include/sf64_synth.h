/**
 * @file sf64_synth.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Monophonic synthesizer for SF64 sound banks
 * @ingroup mixer
 *
 * A synthesizer binds an #sf64_bank_t to a single mixer channel and plays
 * notes by matching the selected preset's regions against key and velocity.
 *
 * The bank is not owned: it must outlive the synthesizer. Create with
 * #sf64_synth_create, bind a mixer channel with #sf64_synth_set_channel,
 * select a preset with #sf64_synth_set_preset, then drive notes with
 * #sf64_synth_note_on / #sf64_synth_note_off.
 *
 * This version is monophonic: a new note-on replaces the previous note.
 * Note-off is immediate (no amp envelope yet).
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
 * The bank is borrowed, not copied. Call #sf64_synth_set_channel before
 * the first #sf64_synth_note_on. Free with #sf64_synth_close.
 *
 * @param bank   Bank returned by #sf64_load
 * @return       New synthesizer
 */
sf64_synth_t *sf64_synth_create(sf64_bank_t *bank);

/**
 * @brief Stop any sounding note and free the synthesizer.
 *
 * Does not close the bank.
 *
 * @param synth  Synthesizer returned by #sf64_synth_create
 */
void sf64_synth_close(sf64_synth_t *synth);

/**
 * @brief Bind the synthesizer to a mixer channel.
 *
 * Stops the note currently sounding on the previous channel, if any.
 * Must be called before #sf64_synth_note_on.
 *
 * @param synth           Synthesizer
 * @param mixer_channel   Mixer channel index (see #mixer_init)
 */
void sf64_synth_set_channel(sf64_synth_t *synth, int mixer_channel);

/**
 * @brief Select the preset used by subsequent note-ons.
 *
 * Does not affect a note that is already sounding. Equivalent to a MIDI
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
 * stops any previous note, and plays that region's sample at the SF2 pitch
 * and volume for the note. A velocity of 0 is treated as #sf64_synth_note_off.
 *
 * @param synth     Synthesizer
 * @param key       MIDI key (0–127)
 * @param velocity  MIDI velocity (1–127; 0 = note-off)
 * @return          true if a matching region was found and playback started
 */
bool sf64_synth_note_on(sf64_synth_t *synth, int key, int velocity);

/**
 * @brief Stop the current note if it was started with @p key.
 *
 * A note-off for a different key is ignored. Stop is immediate.
 *
 * @param synth  Synthesizer
 * @param key    MIDI key that should be released
 */
void sf64_synth_note_off(sf64_synth_t *synth, int key);

#ifdef __cplusplus
}
#endif

#endif
