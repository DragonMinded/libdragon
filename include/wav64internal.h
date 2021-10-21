#ifndef __LIBDRAGON_WAV64_INTERNAL_H
#define __LIBDRAGON_WAV64_INTERNAL_H

#define WAV64_ID            "WV64"
#define WAV64_FILE_VERSION  2
#define WAV64_FORMAT_RAW    0

typedef struct __attribute__((packed)) {
	char id[4];
	int8_t version;
	int8_t format;
	int8_t channels;
	int8_t nbits;
	int32_t freq;
	int32_t len;
	int32_t loop_len;
	int32_t start_offset;
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
