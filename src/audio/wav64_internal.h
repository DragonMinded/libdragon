#ifndef __LIBDRAGON_WAV64_INTERNAL_H
#define __LIBDRAGON_WAV64_INTERNAL_H

#define WAV64_ID            "WV64"
#define WAV64_FORMAT_RAW    0
#define WAV64_FORMAT_VADPCM 1
#define WAV64_FORMAT_OPUS   3
#define WAV64_NUM_FORMATS   4

#define WAV64_FLAG_WARN_SIMULTANEITY	 (1 << 0)

typedef struct wav64_s wav64_t;
typedef struct samplebuffer_s samplebuffer_t;

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
	void *states;            ///< Pointer to per-mixer-channel state data (format-dependent)
	void *samples;           ///< Pointer to the preloaded samples (if streaming is disabled)
	int current_fd;			 ///< File descriptor for the wav64 file
	int base_offset;		 ///< Start of Wav64 data (as offset from start of the file)
	int nsimul;				 ///< Number of maximum simultaneous playbacks
	uint8_t flags;           ///< Misc flags
	int8_t mixer_channels[]; ///< Mixer channels in which the waveform is reproduced
} wav64_state_t;

/** @brief WAV64 pluggable compression algorithm */
typedef struct {
	/** @brief Default number of simultaneous playbacks */
	int default_simul;
	/** @brief Init function: parses extra header information for the specific codec */
	void (*init)(wav64_t *wav, int state_size);
	/** @brief Close function: deallocates memory for codec-specific data */
	void (*close)(wav64_t *wav);
	/** @brief Return the compressed bitrate, mainly used for statistics */
	int (*get_bitrate)(wav64_t *wav);
} wav64_compression_t;

/**
 * @brief Utility function to help implementing #WaveformRead for uncompressed (raw) samples.
 * 
 * This function uses a file descriptor to load samples from ROM into the sample buffer.
 */  
void raw_waveform_read(samplebuffer_t *sbuf, int fd, int wpos, int wlen, int bps);

/**
 * @brief Utility function to help implementing #WaveformRead for uncompressed (raw) samples.
 * 
 * This function uses PI DMA to load samples from ROM into the sample buffer.
 * Note: Tempory function should be removed when XM64 moves to using FILE*.
 */  
void raw_waveform_read_address(samplebuffer_t *sbuf, int rom_addr, int wpos, int wlen, int bps);

/** 
 * @brief Inform wav64 that a certain channel has finished playback.
 * 
 * This is meant to be called by waveform's stop callbacks, so that the
 * generic code can track the number of simultaneous playbacks.
 */
void __wav64_channel_stopped(wav64_t *wav, int chidx);

#endif
