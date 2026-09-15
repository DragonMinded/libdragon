/**
 * @file wav64_opus_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Support for opus-compressed WAV64 files
 */

#ifndef LIBDRAGON_AUDIO_WAV64_OPUS_INTERNAL_H
#define LIBDRAGON_AUDIO_WAV64_OPUS_INTERNAL_H

#include <stdint.h>
#include <stdio.h>
#include "opus/modes.h"
#include "wav64.h"

/**
 * @brief WAV64 Opus seek point entry (stored in the extended header)
 *
 * file_offset_preroll is relative to wav->st->base_offset (start of samples data).
 * intra_skip is the number of samples to skip at the beginning of the target frame.
 */
typedef struct __attribute__((packed)) {
    uint32_t sample_offset;        ///< Absolute sample offset (wpos) of this seek point
    uint32_t file_offset_preroll;  ///< Byte offset (relative to base_offset) of preroll start frame
    uint16_t intra_skip;           ///< Samples to skip inside the target frame
    uint16_t padding;              ///< Padding / reserved
} wav64_opus_seekpoint_t;

/**
 * @brief Wav64 Opus header extension
 *
 * This structure is stored in the extended header area right after wav64_header_t.
 * It is followed by an array of wav64_opus_seekpoint_t entries.
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t frame_size;            ///< Size of an audio frame in samples
    uint32_t max_cmp_frame_size;    ///< Maximum compressed frame size in bytes
    uint32_t bitrate_bps;           ///< Bitrate in bits per second
    OpusCustomMode* mode;           ///< Opus custom mode pointer (at runtime)
    uint16_t preroll_frames;        ///< Number of preroll frames to decode/discard after seeking
    uint16_t num_seekpoints;        ///< Number of seek points entries following this header
    uint32_t reserved;              ///< Reserved / padding
    wav64_opus_seekpoint_t seekpoints[]; ///< Seek points table
} wav64_opus_header_t;

/** @brief Initialize opus decompression on a wav64 file */
void wav64_opus_init(wav64_t *wav, int state_size);

/** @brief Shut down opus decompression on a wav64 file */
void wav64_opus_close(wav64_t *wav);

/** @brief Return the bitrate for a wav64 file */
int wav64_opus_get_bitrate(wav64_t *wav);

/** @brief Adjust a requested seek position (samples) to the previous Opus seek point (samples). */
int wav64_opus_adjust_seek(wav64_t *wav, int wpos);

#endif
