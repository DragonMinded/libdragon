/**
 * @file wav64_vadpcm_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_AUDIO_VADPCM_INTERNAL_H
#define LIBDRAGON_AUDIO_VADPCM_INTERNAL_H

/** @brief Initialize VADPCM decoder for a WAV64 file */
void wav64_vadpcm_init(wav64_t *wav);
/** @brief Close VADPCM decoder and free resources */
void wav64_vadpcm_close(wav64_t *wav);
/** @brief Get the bitrate of the VADPCM stream */
int wav64_vadpcm_get_bitrate(wav64_t *wav);

#endif
