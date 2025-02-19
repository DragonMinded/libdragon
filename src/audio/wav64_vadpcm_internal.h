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

/** @brief Extended header for a WAV64 file with VADPCM compression. */
typedef struct __attribute__((packed, aligned(8))) {
	int8_t npredictors;					///< Number of predictors
	int8_t order;						///< Order of the predictors
	uint8_t flags;						///< VADPCM flags
	uint8_t padding;					///< padding
	wav64_vadpcm_hufftable_t *huff_tbl; ///< Huffman tables (computed at load time)
	wav64_vadpcm_vector_t loop_state[2];///< State at the loop point
	wav64_vadpcm_huffctx_t huff_ctx[3]; ///< Huffman contexts
	wav64_vadpcm_vector_t codebook[];	///< Codebook of the predictors
} wav64_header_vadpcm_t;

/** @brief WAV64 VADPCM decoding state (for a single mixer channel) */
typedef struct __attribute__((aligned(16))) {
	wav64_vadpcm_vector_t state[2];		///< Current decompression state
	int bitpos;							///< Current bit position in the input buffer
} wav64_state_vadpcm_t;

void wav64_vadpcm_init(wav64_t *wav, int state_size);
void wav64_vadpcm_close(wav64_t *wav);
int wav64_vadpcm_get_bitrate(wav64_t *wav);

#endif
