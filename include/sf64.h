/**
 * @file sf64.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief SoundFont 2 banks converted to SF64
 * @ingroup mixer
 *
 * SF64 is the runtime format produced by audioconv64 from a SoundFont 2
 * (`.sf2`) file. Load a bank with #sf64_load and free it with #sf64_close.
 * Playback is performed by #sf64_synth_t (see sf64_synth.h).
 */
#ifndef LIBDRAGON_SF64_H
#define LIBDRAGON_SF64_H

#include "preview.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief SF64 sound bank. */
typedef struct sf64_bank_s sf64_bank_t;

/**
 * @brief Load an SF64 bank from disk.
 * @preview
 *
 * Samples are streamed from the filesystem during playback rather than
 * preloaded into memory. Free with #sf64_close.
 *
 * @param fn   Filename with filesystem prefix (e.g. `rom:/bank.sf64`)
 * @return     Loaded bank
 */
LIBDRAGON_PREVIEW_API
sf64_bank_t *sf64_load(const char *fn);

/**
 * @brief Close a bank and free all associated resources.
 * @preview
 *
 * @param bank  Bank returned by #sf64_load
 */
LIBDRAGON_PREVIEW_API
void sf64_close(sf64_bank_t *bank);

/**
 * @brief Find a preset by MIDI bank and program number.
 * @preview
 *
 * @param bank        Loaded bank
 * @param midi_bank   MIDI bank (0–127 melodic, 128 percussion)
 * @param program     MIDI program (0–127)
 * @return            Preset index, or -1 if not found
 */
LIBDRAGON_PREVIEW_API
int sf64_find_preset(sf64_bank_t *bank, int midi_bank, int program);

/**
 * @brief Number of presets in the bank.
 * @preview
 *
 * Presets are indexed `0 .. count-1` for #sf64_preset_name / #sf64_preset_id.
 */
LIBDRAGON_PREVIEW_API
int sf64_preset_count(sf64_bank_t *bank);

/**
 * @brief Name of the preset at @p index (NUL-terminated, owned by the bank).
 * @preview
 */
LIBDRAGON_PREVIEW_API
const char *sf64_preset_name(sf64_bank_t *bank, int index);

/**
 * @brief MIDI bank and program of the preset at @p index.
 * @preview
 *
 * @param bank         Loaded bank
 * @param index        Preset index (`0 .. #sf64_preset_count - 1`)
 * @param[out] midi_bank  MIDI bank number (may be NULL)
 * @param[out] program    MIDI program number (may be NULL)
 */
LIBDRAGON_PREVIEW_API
void sf64_preset_id(sf64_bank_t *bank, int index, int *midi_bank, int *program);

/**
 * @brief First region of a preset that matches @p key and @p velocity.
 * @preview
 *
 * @param bank           Loaded bank
 * @param preset_index   Preset index (`0 .. #sf64_preset_count - 1`)
 * @param key            MIDI key (0–127)
 * @param velocity       MIDI velocity (1–127)
 * @return               Region index, or -1 if none match
 */
LIBDRAGON_PREVIEW_API
int sf64_find_region(sf64_bank_t *bank, int preset_index, int key, int velocity);

#ifdef __cplusplus
}
#endif

#endif
