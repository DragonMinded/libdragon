#pragma once

#include <cstdio>
#include <cstdint>
#include <vector>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#define LE32_TO_HOST(i) __builtin_bswap32(i)
	#define HOST_TO_LE32(i) __builtin_bswap32(i)
	#define LE16_TO_HOST(i) __builtin_bswap16(i)
	#define HOST_TO_LE16(i) __builtin_bswap16(i)

	#define BE32_TO_HOST(i) (i)
	#define HOST_TO_BE32(i) (i)
	#define LE16_TO_HOST(i) (i)
	#define HOST_TO_BE16(i) (i)
#else
	#define BE32_TO_HOST(i) __builtin_bswap32(i)
	#define HOST_TO_BE32(i) __builtin_bswap32(i)
	#define BE16_TO_HOST(i) __builtin_bswap16(i)
	#define HOST_TO_BE16(i) __builtin_bswap16(i)

	#define LE32_TO_HOST(i) (i)
	#define HOST_TO_LE32(i) (i)
	#define HOST_TO_LE16(i) (i)
	#define LE16_TO_HOST(i) (i)
#endif

extern bool flag_verbose;
extern bool flag_debug;

extern int flag_wav_compress_vadpcm_huffman;

typedef struct {
	int16_t *samples;			// Samples (always 16-bit signed)
	int cnt;					// Number of audio frames
	int channels;				// Number of channels
	int bitsPerSample;			// Original bits per sample in input file
	int sampleRate;
	bool looping;
	int loopOffset;				// Offset of the beginning of the loop in samples
	std::vector<int> skipPoints;		// Skip points in the waveform
} wav_data_t;

void fatal(const char *str, ...);

char* changeext(const char* fn, const char *ext);

bool wav64_write(const char *infn, const char *outfn, FILE *out, wav_data_t* wav, int format);
