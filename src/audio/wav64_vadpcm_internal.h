/**
 * @file wav64_vadpcm_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_AUDIO_VADPCM_INTERNAL_H
#define LIBDRAGON_AUDIO_VADPCM_INTERNAL_H

#include "../utils.h"

#define VADPCM_FLAG_HUFFMAN      (1 << 0)	///< Huffman-encoded VADPCM

/**
 * Stereo VADPCM file layout: blocks of this many frames per channel, alternating
 * L then R (mono is a single linear plane). Matches #SAMPLEBUFFER_MARGIN_UNITS
 * so one samplebuffer fill is one file block.
 */
#define WAV64_VADPCM_BLOCK_FRAMES  128

/** @brief A vector of audio samples */
typedef struct __attribute__((aligned(8))) {
	int16_t v[8];						///< Samples
} wav64_vadpcm_vector_t;

/** @brief Huffamn decoding context */
typedef struct {
	uint8_t lengths[8];					///< Length of each symbol (4-bit - 0xF means unused)
	uint8_t values[16];					///< Code used for each symbol
} wav64_vadpcm_huffctx_t;

/** @brief Huffman decoding tables */
typedef struct {
	uint8_t codes[256];				    ///< Symbol+length found in the prefix of each byte
} wav64_vadpcm_hufftable_t;

/** 
 * @brief WAV64 VADPCM decoding state (for a single mixer channel) 
 * 
 * Notice that the state is accessed in part by RSP (state[2]) and in part by
 * the CPU (bitpos) so we need to avoid false-sharing of cachelines. This is
 * why we force a 16-byte alignment, so that it spans exactly 2 cachelines.
 */
typedef struct __attribute__((aligned(16))) {
	wav64_vadpcm_vector_t state[2];		///< Current decompression state
	int bitpos;							///< Current bit position in the input buffer
} wav64_state_vadpcm_t;

/** @brief Skip point in the waveform.
 * 
 * This structure encodes the state of the VADPCM decoder at a skip point in the
 * waveform. It is used to restore the state of the decoder when skiping to a
 * different point in the waveform. 
 */
typedef struct {
	int offset;							///< Samples offset of the skip point in the waveform
	int bitpos;							///< Bit position in the input buffer
} wav64_vadpcm_skippoint_t;

/** @brief Extended header for a WAV64 file with VADPCM compression. */
typedef struct __attribute__((packed, aligned(8))) {
	int8_t npredictors;						///< Number of predictors
	int8_t order;							///< Order of the predictors
	uint16_t flags;							///< VADPCM flags
	int16_t num_skippoints;					///< Number of allowed skip points
	uint16_t padding1;						///< Padding
	wav64_vadpcm_hufftable_t *huff_tbl; 	///< Huffman tables (computed at load time)
	wav64_vadpcm_skippoint_t *skip_points;	///< Information on the skip points (located after the codebook)
	wav64_vadpcm_vector_t *skip_states;		///< Decompression states at the skip point
	wav64_vadpcm_huffctx_t huff_ctx[3]; 	///< Huffman contexts
	uint32_t padding2;						///< Padding
	wav64_vadpcm_vector_t codebook[];		///< Codebook of the predictors
} wav64_header_vadpcm_t;

_Static_assert(sizeof(wav64_header_vadpcm_t) == 96, "invalid wav64_header_vadpcm size");

/**
 * @brief Frames the mixer can address in a waveform of `len` samples.
 *
 * The last frame is partial whenever the length is not a multiple of 16, but
 * it still holds samples that have to be played.
 */
#define WAV64_VADPCM_FRAMES(len)       DIVIDE_CEIL(len, 16)

/**
 * @brief Frames actually stored in the file for a waveform of `len` samples.
 *
 * audioconv64 pads the waveform to a multiple of 32 samples before encoding,
 * but writes the unpadded length in the header, so the file always holds a
 * few frames more than #WAV64_VADPCM_FRAMES. The padding is part of the
 * block-planar layout of stereo files, so it must be accounted for when
 * addressing frames in the file.
 */
#define WAV64_VADPCM_FILE_FRAMES(len)  (DIVIDE_CEIL(len, 32) * 2)

/** @brief Initialize VADPCM decoder for a WAV64 file */
void wav64_vadpcm_init(wav64_t *wav, int state_size);
/** @brief Close VADPCM decoder and free resources */
void wav64_vadpcm_close(wav64_t *wav);
/** @brief Get the bitrate of the VADPCM stream */
int wav64_vadpcm_get_bitrate(wav64_t *wav);

/** @brief Adjust a requested seek position (samples) to a valid VADPCM skip point (samples). */
int wav64_vadpcm_adjust_seek(wav64_t *wav, int wpos);

/** @brief Preload VADPCM body into a fully-planar plain-frame buffer at `dst`. */
void wav64_vadpcm_preload(wav64_t *wav, void *dst);

#endif
