/**
 * @file sf64.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief SoundFont 2 banks converted to SF64
 * @ingroup mixer
 *
 * SF64 is the runtime format produced by audioconv64 from a SoundFont 2
 * (`.sf2`) file. A bank holds resolved presets, key/velocity regions, amp
 * envelopes, and embedded WAV64 samples.
 *
 * Load a bank with #sf64_load; free it with #sf64_close. The bank itself is
 * immutable and has no mixer events — playback is driven by a synthesizer
 * that shares the bank.
 */
#ifndef __LIBDRAGON_SF64_H
#define __LIBDRAGON_SF64_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque SF64 sound bank. */
typedef struct sf64_bank_s sf64_bank_t;

/**
 * @brief Load an SF64 bank from disk.
 *
 * Opens the file and prepare for synthesis. Samples are not loaded into memory:
 * they will be streamed directly from the filesystem during synthesis.
 * The returned bank must be freed with #sf64_close.
 *
 * @param fn   Filename with filesystem prefix (e.g. `rom:/bank.sf64`)
 * @return     Loaded bank
 */
sf64_bank_t *sf64_load(const char *fn);

/**
 * @brief Close a bank and free all associated resources.
 *
 * Stops any mixer channel still playing an embedded sample, and frees the bank.
 *
 * @param bank  Bank returned by #sf64_load
 */
void sf64_close(sf64_bank_t *bank);

/**
 * @brief Find a preset by MIDI bank and program number.
 *
 * @param bank        Loaded bank
 * @param midi_bank   MIDI bank (0–127 melodic, 128 percussion)
 * @param program     MIDI program (0–127)
 * @return            Preset index, or -1 if not found
 */
int sf64_find_preset(sf64_bank_t *bank, int midi_bank, int program);

#ifdef __cplusplus
}
#endif

#endif
