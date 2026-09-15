/**
 * @file wav64_ulc_internal.h
 * @author Dominic Szablewski <https://phoboslab.org>
 * @brief Support for ulc-compressed WAV64 files
 */

#ifndef LIBDRAGON_AUDIO_ULC_INTERNAL_H
#define LIBDRAGON_AUDIO_ULC_INTERNAL_H

#include <stdint.h>
#include "wav64.h"

/**
 * @brief Wav64 ULC header extension
 *
 * This structure is stored in the extended header area right after wav64_header_t.
 */
typedef struct __attribute__((packed)) {
    uint16_t block_size;        ///< Transform block size (currently always 1024)
    uint16_t max_block_size;    ///< Largest compressed block, in bytes
    uint32_t blocks_len;        ///< Number of blocks, including codec delay/flush blocks
    uint32_t bitrate_bps;       ///< Actual average bitrate of the encoded stream
    uint32_t seek_table_offset; ///< First-block-relative offset of the trailing seek table
} wav64_header_ulc_t;

_Static_assert(sizeof(wav64_header_ulc_t) == 16, "invalid wav64_header_ulc size");

/** @brief Initialize ULC decompression on a WAV64 file. */
void wav64_ulc_init(wav64_t *wav, int state_size);

/** @brief Return the average bitrate of a ULC-compressed WAV64 file. */
int wav64_ulc_get_bitrate(wav64_t *wav);

/** @brief Adjust a requested seek position to a ULC block boundary. */
int wav64_ulc_adjust_seek(wav64_t *wav, int wpos);


#endif
