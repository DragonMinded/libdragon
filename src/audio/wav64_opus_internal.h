/**
 * @file wav64_opus_internal.h
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief Support for opus-compressed WAV64 files
 */

#ifndef LIBDRAGON_AUDIO_WAV64_OPUS_INTERNAL_H
#define LIBDRAGON_AUDIO_WAV64_OPUS_INTERNAL_H

#include <stdint.h>
#include <stdio.h>
#include "wav64.h"

/** @brief Initialize opus decompression on a wav64 file */
void wav64_opus_init(wav64_t *wav, int fh);

/** @brief Shut down opus decompression on a wav64 file */
void wav64_opus_close(wav64_t *wav);

/** @brief Return the bitrate for a wav64 file */
int wav64_opus_get_bitrate(wav64_t *wav);

#endif
