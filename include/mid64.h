/**
 * @file mid64.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief MID64 sequence player
 * @ingroup mixer
 *
 * MID64 is the compiled form of a Standard MIDI File produced by audioconv64.
 * The player dispatches MIDI channel events to a #midi_target_t (for example
 * an SF64 synthesizer) with sample-accurate timing.
 *
 * Use #mid64player_play / #mid64player_stop for mixer-driven playback, or
 * #mid64player_decode_next to walk the stream offline (tests and tooling).
 */
#ifndef LIBDRAGON_MID64_H
#define LIBDRAGON_MID64_H

#include <stdbool.h>
#include <stdint.h>
#include "preview.h"
#include "midi_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Player of a .MID64 file. */
typedef struct mid64player_s mid64player_t;

/**
 * @brief Load a MID64 file.
 * @preview
 *
 * Free with #mid64player_close.
 *
 * @param fn   Filename with filesystem prefix (e.g. `rom:/song.mid64`)
 * @return     Loaded player
 */
LIBDRAGON_PREVIEW_API
mid64player_t *mid64player_load(const char *fn);

/**
 * @brief Free a player returned by #mid64player_load.
 * @preview
 *
 * Stops playback if active and silences the target. Does not close the
 * #midi_target_t.
 *
 * @param player  Player returned by #mid64player_load
 */
LIBDRAGON_PREVIEW_API
void mid64player_close(mid64player_t *player);

/**
 * @brief Rewind the decoder to the start of the sequence.
 * @preview
 *
 * Restores the default tempo. Call #mid64player_stop first if the player
 * is currently playing.
 */
LIBDRAGON_PREVIEW_API
void mid64player_rewind(mid64player_t *player);

/**
 * @brief Start playing the sequence through @p target.
 * @preview
 *
 * Requires #audio_init and #mixer_init. Rewinds the stream. No-op if already
 * playing.
 *
 * @param player  Loaded player
 * @param target  Synth or other MIDI backend (must outlive playback)
 */
LIBDRAGON_PREVIEW_API
void mid64player_play(mid64player_t *player, midi_target_t *target);

/**
 * @brief Stop playback as soon as possible.
 * @preview
 *
 * Silences the target. No-op if not playing.
 */
LIBDRAGON_PREVIEW_API
void mid64player_stop(mid64player_t *player);

/**
 * @brief Decode the next event and dispatch channel messages to @p target.
 * @preview
 *
 * Tempo meta-events update the player but are not sent to the target.
 * On end of sequence, returns false without calling the target. The @p now
 * value passed to the target is the absolute MIDI tick of the event.
 *
 * @return true if an event was decoded (including tempo); false on end of sequence
 */
LIBDRAGON_PREVIEW_API
bool mid64player_decode_next(mid64player_t *player, midi_target_t *target);

/**
 * @brief Loop the whole sequence from the start when it ends.
 * @preview
 *
 * Default is false. Loop points inside the file are not supported.
 */
LIBDRAGON_PREVIEW_API
void mid64player_set_loop(mid64player_t *player, bool loop);

/**
 * @brief Pulses (ticks) per quarter note.
 * @preview
 */
LIBDRAGON_PREVIEW_API
uint16_t mid64player_get_ppqn(mid64player_t *player);

/**
 * @brief Sequence length in ticks.
 * @preview
 */
LIBDRAGON_PREVIEW_API
uint32_t mid64player_get_duration_ticks(mid64player_t *player);

/**
 * @brief Sequence length in milliseconds (accounting for the tempo map).
 * @preview
 */
LIBDRAGON_PREVIEW_API
uint32_t mid64player_get_duration_ms(mid64player_t *player);

/**
 * @brief Current tempo in microseconds per quarter note.
 * @preview
 */
LIBDRAGON_PREVIEW_API
uint32_t mid64player_get_tempo(mid64player_t *player);

/**
 * @brief Current playback position within the sequence, in milliseconds.
 * @preview
 *
 * When looping, this is the position in the current iteration.
 */
LIBDRAGON_PREVIEW_API
uint32_t mid64player_tell_ms(mid64player_t *player);

#ifdef __cplusplus
}
#endif

#endif
