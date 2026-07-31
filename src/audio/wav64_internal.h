/**
 * @file wav64_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef __LIBDRAGON_WAV64_INTERNAL_H
#define __LIBDRAGON_WAV64_INTERNAL_H

#include "mixer_internal.h"

#define WAV64_ID            "WV64"    ///< WAV64 file identifier
#define WAV64_FORMAT_RAW    0         ///< Raw audio format
#define WAV64_FORMAT_VADPCM 1         ///< VADPCM compressed format
#define WAV64_FORMAT_ULC    2         ///< ULC compressed format
#define WAV64_FORMAT_OPUS   3         ///< Opus compressed format
#define WAV64_NUM_FORMATS   4         ///< Number of supported formats

#define WAV64_FLAG_OWNED_FD (1 << 0)  ///< Flag indicating the file descriptor is owned by the wav64 structure

/// @cond
typedef struct wav64_s wav64_t;
typedef struct wav64_loadparms_s wav64_loadparms_t;
typedef struct samplebuffer_s samplebuffer_t;
/// @endcond

/** @brief Header of a WAV64 file. */
typedef struct __attribute__((packed)) {
	char id[4];             ///< ID of the file (WAV64_ID)
	int8_t version;         ///< Version of the file (WAV64_FILE_VERSION)
	int8_t format;          ///< Format of the file (WAV64_FORMAT_RAW)
	int8_t channels;        ///< Number of interleaved channels
	int8_t nbits;           ///< Width of sample in bits (8 or 16)
	int32_t freq;           ///< Default playback frequency
	int32_t len;            ///< Length of the file (in samples)
	int32_t loop_len;       ///< Length of the loop since file end (or 0 if no loop)
	uint32_t start_offset;  ///< Offset of the first sample in the file
	uint32_t state_size;    ///< Size of per-mixer-channel state to allocate at runtime
} wav64_header_t;

_Static_assert(sizeof(wav64_header_t) == 28, "invalid wav64_header size");

/** @brief WAV64 state */
typedef struct wav64_state_s {
	int format;			     ///< Internal format of the file
	void *ext;               ///< Pointer to extended header data (format-dependent)
	void *samples;           ///< Pointer to the preloaded samples (if streaming is disabled)
	int current_fd;			 ///< File descriptor for the wav64 file
	int base_offset;		 ///< Start of Wav64 data (as offset from start of the file)
	/** PI bus address of file start (0 if not a DFS file / async DMA unavailable). */
	uint32_t rom_base;
	uint8_t flags;           ///< Misc flags
	/** Codec side-data for in-mixer VADPCM mono (will be stored in waveform_t::codec). */
	waveform_vadpcm_t vadpcm;
} wav64_state_t;

/** @brief WAV64 pluggable compression algorithm */
typedef struct {
	/** @brief Init function: parses extra header information for the specific codec */
	void (*init)(wav64_t *wav, int state_size);
	/** @brief Close function: deallocates memory for codec-specific data */
	void (*close)(wav64_t *wav);
	/** @brief Return the compressed bitrate, mainly used for statistics */
	int (*get_bitrate)(wav64_t *wav);
	/**
	 * @brief Adjust a requested seek position (in samples) to a valid codec-specific seek point.
	 *
	 * If NULL, the codec supports sample-accurate seeking and no adjustment is performed.
	 * If non-NULL, the function must return a valid seek point (in samples) for this codec.
	 */
	int (*adjust_seek)(wav64_t *wav, int wpos);
} wav64_compression_t;

/**
 * Similar to #wav64_load, but uses a file descriptor instead of a filename.
 */
wav64_t *wav64_loadfd(int fd, const char *debug_file_name, wav64_loadparms_t *parms);

#endif
