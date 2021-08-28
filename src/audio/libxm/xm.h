/* Author: Romain "Artefact2" Dalmaso <artefact2@gmail.com> */
/* Contributor: Dan Spencer <dan@atomicpotato.net> */

/* This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */

#pragma once
#ifndef __has_xm_h
#define __has_xm_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct xm_context_s;
typedef struct xm_context_s xm_context_t;

/** Create a XM context.
 *
 * @param moddata the contents of the module
 * @param rate play rate in Hz, recommended value of 48000
 *
 * @returns 0 on success
 * @returns 1 if module data is not sane
 * @returns 2 if memory allocation failed
 *
 * @deprecated This function is unsafe!
 * @see xm_create_context_safe()
 */
int xm_create_context(xm_context_t**, const char* moddata, uint32_t rate);

/** Create a XM context.
 *
 * @param moddata the contents of the module
 * @param moddata_length the length of the contents of the module, in bytes
 * @param rate play rate in Hz, recommended value of 48000
 *
 * @returns 0 on success
 * @returns 1 if module data is not sane
 * @returns 2 if memory allocation failed
 */
int xm_create_context_safe(xm_context_t**, const char* moddata, size_t moddata_length, uint32_t rate);

/** Create a XM context.
 *
 * This function will produce smaller code size compared to
 * xm_create_context(), but requires converting the .xm file to a
 * non-portable format beforehand.
 *
 * This function doesn't do any kind of error checking.
 *
 * @param libxmizeddata: pointer to module data, needs to point to a
 * writable area, freeing it is equivalent as calling
 * xm_free_context().
 *
 * @see xm_create_context()
 */
void xm_create_context_from_libxmize(xm_context_t**, char* libxmizeddata, uint32_t rate);

/** Free a XM context created by xm_create_context(). */
void xm_free_context(xm_context_t*);

/** Play the module and put the sound samples in an output buffer.
 *
 * @param output buffer of 2*numsamples elements
 * @param numsamples number of samples to generate
 */
void xm_generate_samples(xm_context_t*, float* output, size_t numsamples);



/** Set the maximum number of times a module can loop. After the
 * specified number of loops, calls to xm_generate_samples will only
 * generate silence. You can control the current number of loops with
 * xm_get_loop_count().
 *
 * @param loopcnt maximum number of loops. Use 0 to loop
 * indefinitely. */
void xm_set_max_loop_count(xm_context_t*, uint8_t loopcnt);

/** Get the loop count of the currently playing module. This value is
 * 0 when the module is still playing, 1 when the module has looped
 * once, etc. */
uint8_t xm_get_loop_count(xm_context_t*);



/** Seek to a specific position in a module.
 *
 * WARNING, WITH BIG LETTERS: seeking modules is broken by design,
 * don't expect miracles.
 */
void xm_seek(xm_context_t*, uint8_t pot, uint8_t row, uint16_t tick);



/** Mute or unmute a channel.
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 *
 * @return whether the channel was muted.
 */
bool xm_mute_channel(xm_context_t*, uint16_t, bool);

/** Mute or unmute an instrument.
 *
 * @note Instrument numbers go from 1 to
 * xm_get_number_of_instruments(...).
 *
 * @return whether the instrument was muted.
 */
bool xm_mute_instrument(xm_context_t*, uint16_t, bool);



/** Get the module name as a NUL-terminated string. */
const char* xm_get_module_name(xm_context_t*);

/** Get the tracker name as a NUL-terminated string. */
const char* xm_get_tracker_name(xm_context_t*);



/** Get the number of channels. */
uint16_t xm_get_number_of_channels(xm_context_t*);

/** Get the module length (in patterns). */
uint16_t xm_get_module_length(xm_context_t*);

/** Get the number of patterns. */
uint16_t xm_get_number_of_patterns(xm_context_t*);

/** Get the number of rows of a pattern.
 *
 * @note Pattern numbers go from 0 to
 * xm_get_number_of_patterns(...)-1.
 */
uint16_t xm_get_number_of_rows(xm_context_t*, uint16_t);

/** Get the number of instruments. */
uint16_t xm_get_number_of_instruments(xm_context_t*);

/** Get the number of samples of an instrument.
 *
 * @note Instrument numbers go from 1 to
 * xm_get_number_of_instruments(...).
 */
uint16_t xm_get_number_of_samples(xm_context_t*, uint16_t);

/** Get the internal buffer for a given sample waveform.
 *
 * This buffer can be read from or written to, at any time, but the
 * length cannot change. The buffer must be cast to (int8_t*) or
 * (int16_t*) depending on the sample type.
 *
 * @note Instrument numbers go from 1 to
 * xm_get_number_of_instruments(...).
 *
 * @note Sample numbers go from 0 to
 * xm_get_nubmer_of_samples(...,instr)-1.
 */
void* xm_get_sample_waveform(xm_context_t*, uint16_t instr, uint16_t sample, size_t* length, uint8_t* bits);



/** Get the current module speed.
 *
 * @param bpm will receive the current BPM
 * @param tempo will receive the current tempo (ticks per line)
 */
void xm_get_playing_speed(xm_context_t*, uint16_t* bpm, uint16_t* tempo);

/** Get the current position in the module being played.
 *
 * @param pattern_index if not NULL, will receive the current pattern
 * index in the POT (pattern order table)
 *
 * @param pattern if not NULL, will receive the current pattern number
 *
 * @param row if not NULL, will receive the current row
 *
 * @param samples if not NULL, will receive the total number of
 * generated samples (divide by sample rate to get seconds of
 * generated audio)
 */
void xm_get_position(xm_context_t*, uint8_t* pattern_index, uint8_t* pattern, uint8_t* row, uint64_t* samples);

/** Get the latest time (in number of generated samples) when a
 * particular instrument was triggered in any channel.
 *
 * @note Instrument numbers go from 1 to
 * xm_get_number_of_instruments(...).
 */
uint64_t xm_get_latest_trigger_of_instrument(xm_context_t*, uint16_t);

/** Get the latest time (in number of generated samples) when a
 * particular sample was triggered in any channel.
 *
 * @note Instrument numbers go from 1 to
 * xm_get_number_of_instruments(...).
 *
 * @note Sample numbers go from 0 to
 * xm_get_nubmer_of_samples(...,instr)-1.
 */
uint64_t xm_get_latest_trigger_of_sample(xm_context_t*, uint16_t instr, uint16_t sample);

/** Get the latest time (in number of generated samples) when any
 * instrument was triggered in a given channel.
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 */
uint64_t xm_get_latest_trigger_of_channel(xm_context_t*, uint16_t);

/** Checks whether a channel is active (ie: is playing something).
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 */
bool xm_is_channel_active(xm_context_t*, uint16_t);

/** Get the instrument number currently playing in a channel.
 *
 * @returns instrument number, or 0 if channel is not active.
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 *
 * @note Instrument numbers go from 1 to
 * xm_get_number_of_instruments(...).
 */
uint16_t xm_get_instrument_of_channel(xm_context_t*, uint16_t);

/** Get the frequency of the sample currently playing in a channel.
 *
 * @returns a frequency in Hz. If the channel is not active, return
 * value is undefined.
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 */
float xm_get_frequency_of_channel(xm_context_t*, uint16_t);

/** Get the volume of the sample currently playing in a channel. This
 * takes into account envelopes, etc.
 *
 * @returns a volume between 0 or 1. If the channel is not active,
 * return value is undefined.
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 */
float xm_get_volume_of_channel(xm_context_t*, uint16_t);

/** Get the panning of the sample currently playing in a channel. This
 * takes into account envelopes, etc.
 *
 * @returns a panning between 0 (L) and 1 (R). If the channel is not
 * active, return value is undefined.
 *
 * @note Channel numbers go from 1 to xm_get_number_of_channels(...).
 */
float xm_get_panning_of_channel(xm_context_t*, uint16_t);

#endif
