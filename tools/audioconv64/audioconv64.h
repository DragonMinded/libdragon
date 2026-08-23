/*
    audioconv64: convert audio files to the format used by the Libdragon SDK
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#pragma once

#include <cstdio>
#include <cstdint>
#include <vector>

// C headers in src/ use _Static_assert; map to C++ static_assert.
#define _Static_assert static_assert

#include "vadpcm_pack.h"

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

enum ulc_mode_t { ULC_MODE_VBR, ULC_MODE_ABR, ULC_MODE_CBR };

typedef struct {
	int16_t *samples;			// Samples (always 16-bit signed)
	int cnt;					// Number of audio frames
	int channels;				// Number of channels
	int bitsPerSample;			// Original bits per sample in input file
	int sampleRate;
	bool looping;
	int loopOffset;				// Exclusive-start of the sustain loop (samples)
	int loopEnd;				// Exclusive end of the sustain loop (0 ⇒ cnt)
	std::vector<int> skipPoints;		// Skip points in the waveform
	/** VADPCM: compressed frames to keep in RDRAM for note-on (0 = none). */
	uint8_t attack_frames;
	/** VADPCM: preload whole sample into RDRAM (no streaming). */
	bool resident;
} wav_data_t;

// Global options
extern int flag_verbose;
extern bool flag_debug;

// WAV options
extern bool flag_wav_looping;
extern int flag_wav_looping_offset;
extern int flag_wav_compress;
extern int flag_wav_compress_vadpcm_huffman;
extern int flag_wav_compress_vadpcm_bits;
extern ulc_mode_t flag_wav_compress_ulc_mode;
extern float flag_wav_compress_ulc_bitrate;
extern float flag_wav_compress_ulc_quality;
extern int flag_wav_resample;
extern double flag_wav_seek_interval_sec;
extern const char *flag_wav_seek_file;
extern bool flag_wav_mono;

// XM options
extern int flag_xm_compress_meta;
extern int flag_xm_compress_samples;
extern bool flag_xm_8bit;
extern const char *flag_xm_extsampledir;

// YM options
extern bool flag_ym_compress;

// SF options
extern int flag_sf_compress;

// MID options
extern int flag_mid_compress;

__attribute__((noreturn, format(printf, 1, 2)))
void fatal(const char *str, ...);

char* changeext(const char* fn, const char *ext);

int wav_convert(const char *infn, const char *outfn);
int xm_convert(const char *infn, const char *outfn);
int ym_convert(const char *infn, const char *outfn);
int sf_convert(const char *infn, const char *outfn);
int mid_convert(const char *infn, const char *outfn);

bool wav64_write(const char *infn, const char *outfn, FILE *out, wav_data_t* wav, int format);
