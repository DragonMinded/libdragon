#ifndef __LIBDRAGON_WAV64_INTERNAL_H
#define __LIBDRAGON_WAV64_INTERNAL_H

#define WAV64_ID            "WV64"
#define WAV64_FILE_VERSION  2
#define WAV64_FORMAT_RAW    0

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
	int32_t start_offset;   ///< Offset of the first sample in the file
} wav64_header_t;

_Static_assert(sizeof(wav64_header_t) == 24, "invalid wav64_header size");

typedef struct samplebuffer_s samplebuffer_t;

/**
 * @brief Utility function to help implementing #WaveformRead for uncompressed (raw) samples.
 * 
 * This function uses PI DMA to load samples from ROM into the sample buffer.
 */  
void raw_waveform_read(samplebuffer_t *sbuf, int base_rom_addr, int wpos, int wlen, int bps);

#endif
