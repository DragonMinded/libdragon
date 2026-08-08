/**
 * @file mid64.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief MID64 sequence player
 * @ingroup mixer
 *
 * MID64 is the compiled form of a Standard MIDI File produced by audioconv64.
 * The player loads the compact event stream into RDRAM and dispatches MIDI
 * channel events to a #midi_target_t (for example an SF64 synthesizer).
 *
 * Playback is driven by a single #mixer_add_event callback: MIDI deltas are
 * converted to output samples with an integer remainder (no float clock), and
 * all events that share the same sample time are batched in one callback.
 *
 * #mid64player_decode_next walks the stream without the mixer, using the
 * absolute MIDI tick as @p now (useful for tests and tooling).
 */
#ifndef LIBDRAGON_MID64_H
#define LIBDRAGON_MID64_H

#include <stdbool.h>
#include <stdint.h>
#include "midi_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Player of a .MID64 file. */
typedef struct mid64player_s mid64player_t;

/**
 * @brief Load a MID64 file and preload its event stream.
 *
 * Uses #asset_load, so the file may be asset-compressed by audioconv64.
 * Free with #mid64player_close.
 *
 * @param fn   Filename with filesystem prefix (e.g. `rom:/song.mid64`)
 * @return     Loaded player
 */
mid64player_t *mid64player_load(const char *fn);

/**
 * @brief Free a player returned by #mid64player_load.
 *
 * Stops playback if active. Does not close the #midi_target_t.
 *
 * @param player  Player returned by #mid64player_load
 */
void mid64player_close(mid64player_t *player);

/**
 * @brief Rewind the decoder to the start of the event stream.
 *
 * Resets running status, tempo (500000 μs/qn), and tick to 0.
 * Does not affect an active MixerEvent; call #mid64player_stop first.
 */
void mid64player_rewind(mid64player_t *player);

/**
 * @brief Start playing the sequence through @p target via a MixerEvent.
 *
 * Requires #audio_init (sample rate) and #mixer_init. Rewinds the stream and
 * registers one mixer event. No-op if already playing.
 *
 * @param player  Loaded player
 * @param target  Synth or other MIDI backend (must outlive playback)
 */
void mid64player_play(mid64player_t *player, midi_target_t *target);

/**
 * @brief Request stop; the MixerEvent callback performs the actual teardown.
 *
 * Same pattern as #xm64player_stop.
 */
void mid64player_stop(mid64player_t *player);

/**
 * @brief Decode the next event and dispatch channel messages to @p target.
 *
 * Tempo changes update the player's tempo and are not sent to the target.
 * On END, returns false without calling the target. @p now passed to the
 * target is the absolute MIDI tick of the event (not an output sample).
 *
 * @return true if an event was decoded (including tempo); false on END
 */
bool mid64player_decode_next(mid64player_t *player, midi_target_t *target);

/**
 * @brief Loop the whole sequence from the start when END is reached.
 *
 * Default is false. Loop points inside the file are not supported.
 */
void mid64player_set_loop(mid64player_t *player, bool loop);

/** @brief Pulses (ticks) per quarter note. */
uint16_t mid64player_get_ppqn(mid64player_t *player);

/** @brief Sequence length in ticks. */
uint32_t mid64player_get_duration_ticks(mid64player_t *player);

/**
 * @brief Wall-clock length of the sequence in milliseconds.
 *
 * Computed by audioconv64 from the tempo map and stored in the MID64 header.
 */
uint32_t mid64player_get_duration_ms(mid64player_t *player);

/** @brief Current tempo in microseconds per quarter note. */
uint32_t mid64player_get_tempo(mid64player_t *player);

/**
 * @brief Current playback position within the sequence, in milliseconds.
 *
 * When looping, this is the position in the current iteration. Safe to call
 * from the main thread while the mixer callback is running.
 */
uint32_t mid64player_tell_ms(mid64player_t *player);

#ifdef __cplusplus
}
#endif

#endif
