/**
 * @file audio.h
 * @brief Audio Subsystem
 * @ingroup audio
 */
#ifndef __LIBDRAGON_AUDIO_H
#define __LIBDRAGON_AUDIO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Will be called periodically when more sample data is needed.
 *
 * @param[in] buffer
 *            The address to write the sample data to
 * @param[in] numsamples
 *            The number of samples to write to the buffer
 *            Note: this is the number of samples per channel, so clients
 *            should write twice this number of samples (interleaved).
 */
typedef void(*audio_fill_buffer_callback)(short *buffer, size_t numsamples);

void audio_init(const int frequency, int numbuffers);
void audio_set_buffer_callback(audio_fill_buffer_callback fill_buffer_callback);
void audio_pause(bool pause);
volatile int audio_can_write();
void audio_write_silence();
void audio_close();
int audio_get_frequency();
int audio_get_buffer_length();

short* audio_write_begin(void);
void audio_write_end(void);
int audio_push(const short *buffer, int nsamples, bool blocking);


__attribute__((deprecated("use audio_write_begin or audio_push instead")))
void audio_write(const short * const buffer);

#ifdef __cplusplus
}
#endif

#endif
