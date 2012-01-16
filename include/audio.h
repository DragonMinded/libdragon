/**
 * @file audio.h
 * @brief Audio Subsystem
 * @ingroup audio
 */
#ifndef __LIBDRAGON_AUDIO_H
#define __LIBDRAGON_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(const int frequency, int numbuffers);
void audio_write(const short * const buffer);
volatile int audio_can_write();
void audio_write_silence();
void audio_close();
int audio_get_frequency();
int audio_get_buffer_length();

#ifdef __cplusplus
}
#endif

#endif
