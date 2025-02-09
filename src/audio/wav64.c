/**
 * @file wav64.c
 * @brief Support for WAV64 audio files
 * @ingroup mixer
 */

#include "wav64.h"
#include "wav64_internal.h"
#include "wav64_vadpcm_internal.h"
#include "wav64_opus_internal.h"
#include "mixer.h"
#include "mixer_internal.h"
#include "dragonfs.h"
#include "n64sys.h"
#include "dma.h"
#include "samplebuffer.h"
#include "debug.h"
#include "utils.h"
#include "rspq.h"
#include "asset_internal.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdalign.h>
#include <fcntl.h>
#include <unistd.h>

/** ID of a standard WAV file */
#define WAV_RIFF_ID   "RIFF"
/** ID of a WAVX file (big-endian WAV) */
#define WAV_RIFX_ID   "RIFX"

/** @brief Profile of DMA usage by WAV64, used for debugging purposes. */
int64_t __wav64_profile_dma = 0;

/** @brief None compression init function */
static void wav64_none_init(wav64_t *wav);
/** @brief None compression get_bitrate function */
static int wav64_none_get_bitrate(wav64_t *wav);

static wav64_compression_t algos[4] = {
	// None compression
    [WAV64_FORMAT_RAW] = {
		.init = wav64_none_init,
		.get_bitrate = wav64_none_get_bitrate,
    },
	// VADPCM compression. This is always linked in as it's the default algorithm
	// for audioconv64, and it's very little code at runtime.
	[WAV64_FORMAT_VADPCM] = {
		.init = wav64_vadpcm_init,
		.close = wav64_vadpcm_close,
		.get_bitrate = wav64_vadpcm_get_bitrate,
	},
};

void raw_waveform_read(samplebuffer_t *sbuf, int current_fd, int wpos, int wlen, int bps) {
	uint8_t* ram_addr = (uint8_t*)samplebuffer_append(sbuf, wlen);
	int bytes = wlen << bps;

	// FIXME: remove CachedAddr() when read() supports uncached addresses
	uint32_t t0 = TICKS_READ();
	read(current_fd, CachedAddr(ram_addr), bytes);
	__wav64_profile_dma += TICKS_READ() - t0;
}

void raw_waveform_read_address(samplebuffer_t *sbuf, int base_rom_addr, int wpos, int wlen, int bps) {
	uint32_t rom_addr = base_rom_addr + (wpos << bps);
	uint8_t* ram_addr = (uint8_t*)samplebuffer_append(sbuf, wlen);
	int bytes = wlen << bps;

	uint32_t t0 = TICKS_READ();
	// Run the DMA transfer. We rely on libdragon's PI DMA function which works
	// also for misaligned addresses and odd lengths.
	// The mixer/samplebuffer guarantees that ROM/RAM addresses are always
	// on the same 2-byte phase, as the only requirement of dma_read.
	dma_read(ram_addr, rom_addr, bytes);
	__wav64_profile_dma += TICKS_READ() - t0;
}

static void waveform_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	int bps = (wav->wave.bits == 8 ? 0 : 1) + (wav->wave.channels == 2 ? 1 : 0);
	if (seeking) {
		lseek(wav->current_fd, wav->base_offset + (wpos << bps), SEEK_SET);
	}
	raw_waveform_read(sbuf, wav->current_fd, wpos, wlen, bps);
}

static void wav64_none_init(wav64_t *wav) {
	// Initialize none compression
	wav->wave.read = waveform_read;
	wav->wave.ctx = wav;
}

static int wav64_none_get_bitrate(wav64_t *wav) {
	return wav->wave.frequency * wav->wave.channels * wav->wave.bits;
}

void wav64_open(wav64_t *wav, const char *file_name) {
	memset(wav, 0, sizeof(*wav));

	// For backwards compatibility with old versions of this file, we support
	// an unprefixed file name as a dfs file. This is deprecated and not documented
	// but we just want to avoid breaking existing code
	if (strchr(file_name, ':') == NULL) {
		char* dfs_name = alloca(5 + strlen(file_name) + 1);
		strcpy(dfs_name, "rom:/");
		strcat(dfs_name, file_name);
		file_name = dfs_name;
	}

	// Open the input file.
	int file_handle = must_open(file_name);
	wav64_header_t head = {0};
	read(file_handle, &head, sizeof(head));
	if (memcmp(head.id, WAV64_ID, 4) != 0) {
		assertf(memcmp(head.id, WAV_RIFF_ID, 4) != 0 && memcmp(head.id, WAV_RIFX_ID, 4) != 0,
			"wav64 %s: use audioconv64 to convert to wav64 format", file_name);
		assertf(0, "wav64 %s: invalid ID: %02x%02x%02x%02x\n",
			file_name, head.id[0], head.id[1], head.id[2], head.id[3]);
	}
	assertf(head.version == WAV64_FILE_VERSION, "wav64 %s: invalid version: %02x\n",
		file_name, head.version);

	wav->wave.name = file_name;
	wav->wave.channels = head.channels;
	wav->wave.bits = head.nbits;
	wav->wave.frequency = head.freq;
	wav->wave.len = head.len;
	wav->wave.loop_len = head.loop_len; 
	wav->current_fd = file_handle;
	wav->base_offset = head.start_offset;
	wav->format = head.format;

	assertf(head.format < WAV64_NUM_FORMATS, "Unknown wav64 compression format %d; corrupted file?", head.format);
	assertf(head.format < WAV64_NUM_FORMATS && algos[head.format].init != NULL,
        "wav64: compression level %d not initialized. Call wav64_init_compression(%d) at initialization time", head.format, head.format);

	algos[head.format].init(wav);

	lseek(wav->current_fd, wav->base_offset, SEEK_SET);
}

void wav64_play(wav64_t *wav, int ch)
{
	// Update the context pointer, so that we try to catch cases where the
	// wav64_t instance was moved.
	wav->wave.ctx = wav;
	mixer_ch_play(ch, &wav->wave);
}

void wav64_set_loop(wav64_t *wav, bool loop) {
	wav->wave.loop_len = loop ? wav->wave.len : 0;

	// Odd loop lengths are not supported for 8-bit waveforms because they would
	// change the 2-byte phase between ROM and RDRAM addresses during loop unrolling.
	// We shorten the loop by 1 sample which shouldn't matter.
	// Notice that audioconv64 does the same during conversion.
	if (wav->wave.bits == 8 && wav->wave.loop_len & 1)
		wav->wave.loop_len -= 1;
}

int wav64_get_bitrate(wav64_t *wav) {
	if (algos[wav->format].get_bitrate)
		return algos[wav->format].get_bitrate(wav);
	return algos[WAV64_FORMAT_RAW].get_bitrate(wav);
}

void wav64_close(wav64_t *wav)
{
	// Stop playing the waveform on all channels
	__mixer_wave_stopall(&wav->wave);

	if (algos[wav->format].close)
		algos[wav->format].close(wav);

	if (wav->current_fd >= 0) {
		close(wav->current_fd);
		wav->current_fd = -1;
	}
}

/** @brief Initialize wav64 compression level 3 */
void __wav64_init_compression_lvl3(void)
{
	algos[WAV64_FORMAT_OPUS] = (wav64_compression_t){
		.init = wav64_opus_init,
		.close = wav64_opus_close,
		.get_bitrate = wav64_opus_get_bitrate,
	};
}
