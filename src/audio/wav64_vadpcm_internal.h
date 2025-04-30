#ifndef LIBDRAGON_AUDIO_VADPCM_INTERNAL_H
#define LIBDRAGON_AUDIO_VADPCM_INTERNAL_H

#define VADPCM_FLAG_HUFFMAN      (1 << 0)	///< Huffman-encoded VADPCM

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
	wav64_vadpcm_vector_t state[2];		///< Decompression state at this skip point
	int bitpos;							///< Bit position in the input buffer
	int offset;							///< Samples offset of the skip point in the waveform
} wav64_vadpcm_skippoint_t;

/** @brief Extended header for a WAV64 file with VADPCM compression. */
typedef struct __attribute__((packed, aligned(8))) {
	int8_t npredictors;						///< Number of predictors
	int8_t order;							///< Order of the predictors
	uint8_t flags;							///< VADPCM flags
	int8_t num_skippoints;					///< Number of allowed skip points
	wav64_vadpcm_hufftable_t *huff_tbl; 	///< Huffman tables (computed at load time)
	wav64_vadpcm_skippoint_t *skip_points;	///< Information on the skip points (located after the codebook)
	wav64_vadpcm_huffctx_t huff_ctx[3]; 	///< Huffman contexts
	uint32_t padding;						///< Padding
	wav64_vadpcm_vector_t codebook[];		///< Codebook of the predictors
} wav64_header_vadpcm_t;

_Static_assert(sizeof(wav64_header_vadpcm_t) == 88, "invalid wav64_header_vadpcm size");

void wav64_vadpcm_init(wav64_t *wav, int state_size);
void wav64_vadpcm_close(wav64_t *wav);
int wav64_vadpcm_get_bitrate(wav64_t *wav);

#endif
