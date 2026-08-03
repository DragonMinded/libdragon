/**
 * @file wav64.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Support for WAV64 audio files
 * @ingroup mixer
 */

#include "wav64.h"
#include "wav64_internal.h"
#include "wav64_vadpcm_internal.h"
#include "wav64_opus_internal.h"
#include "wav64_ulc_internal.h"
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
#include <stdalign.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

/** ID of a standard WAV file */
#define WAV_RIFF_ID   "RIFF"
/** ID of a WAVX file (big-endian WAV) */
#define WAV_RIFX_ID   "RIFX"

/** @brief None compression init function */
static void wav64_none_init(wav64_t *wav, int state_size);
/** @brief None compression get_bitrate function */
static int wav64_none_get_bitrate(wav64_t *wav);

static wav64_compression_t algos[WAV64_NUM_FORMATS] = {
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
		.adjust_seek = wav64_vadpcm_adjust_seek,
	},
};

static void raw_waveform_read(samplebuffer_t *sbuf, wav64_t *wav, int wpos, int wlen, int bps) {
	uint32_t rom = wav->st->rom_base;
	while (wlen > 0) {
		int n = MIN(wlen, SAMPLEBUFFER_MARGIN_UNITS);
		uint8_t* ram_addr = (uint8_t*)samplebuffer_append(sbuf, n);
		int bytes = n << bps;
		uint32_t pi_addr = rom + wav->st->base_offset + (wpos << bps);

		if (rom && !(((uint32_t)ram_addr ^ pi_addr) & 1)) {
			samplebuffer_dma_wait(sbuf);
			sbuf->dma_ticket = dma_read_async(ram_addr, pi_addr, bytes);
		} else {
			lseek(wav->st->current_fd, wav->st->base_offset + (wpos << bps), SEEK_SET);
			// FIXME: remove CachedAddr() when read() supports uncached addresses
			read(wav->st->current_fd, CachedAddr(ram_addr), bytes);
		}
		wlen -= n;
		wpos += n;
	}
}

static void wav64_none_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	int bps = (wav->wave.bits == 8 ? 0 : 1) + (wav->wave.channels == 2 ? 1 : 0);
	(void)ctx; (void)seeking;
	raw_waveform_read(sbuf, wav, wpos, wlen, bps);
}

static void wav64_none_init(wav64_t *wav, int state_size) {
	// Initialize none compression. Setup read callback for streaming;
	// preloaded waveforms set wave->mem and need no read callback.
	if (!wav->st->samples) {
		wav->wave.read = wav64_none_read;
		// Samples come straight from ROM via async PI DMA; a file on any other
		// medium is read synchronously instead.
		wav->wave.async_read = wav->st->rom_base != 0;
	} else {
		wav->wave.read = NULL;
		wav->wave.async_read = false;
		wav->wave.mem = wav->st->samples;
	}
	// Also clear start callback (needed in the preloading codepath)
	wav->wave.start = NULL;
}

static int wav64_none_get_bitrate(wav64_t *wav) {
	return wav->wave.frequency * wav->wave.channels * wav->wave.bits;
}

static wav64_t* internal_open(wav64_t *wav, int file_handle, const char *file_name, wav64_loadparms_t *parms)
{
	wav64_loadparms_t default_parms = {0};
	if (!parms) parms = &default_parms;

	// Open the input file, and read the header
	int start_offset = 0;
	bool owned_fd = false;
	if (file_handle < 0) {
		// For backwards compatibility with old versions of this file, we support
		// an unprefixed file name as a dfs file. This is deprecated and not documented
		// but we just want to avoid breaking existing code
		if (file_name && strchr(file_name, ':') == NULL) {
			char* dfs_name = alloca(5 + strlen(file_name) + 1);
			strcpy(dfs_name, "rom:/");
			strcat(dfs_name, file_name);
			file_name = dfs_name;
		}

		file_handle = must_open(file_name);
		owned_fd = true;
	} else {
		start_offset = lseek(file_handle, 0, SEEK_CUR);
	}

	wav64_header_t head;
	read(file_handle, &head, sizeof(head));

	if (memcmp(head.id, WAV64_ID, 4) != 0) {
		assertf(memcmp(head.id, WAV_RIFF_ID, 4) != 0 && memcmp(head.id, WAV_RIFX_ID, 4) != 0,
			"wav64 %s: use audioconv64 to convert to wav64 format", file_name);
		assertf(0, "wav64 %s: invalid ID: %02x%02x%02x%02x\n",
			file_name, head.id[0], head.id[1], head.id[2], head.id[3]);
	}
	assertf(head.version == 9, "wav64 %s: invalid version: %02x\n",
		file_name, head.version);
	assertf(head.format < WAV64_NUM_FORMATS, "Unknown wav64 compression format %d; corrupted file?", head.format);
	assertf(head.format < WAV64_NUM_FORMATS && algos[head.format].init != NULL,
        "wav64: compression level %d not initialized. Call wav64_init_compression(%d) at initialization time", head.format, head.format);

	int ext_size = head.start_offset - sizeof(wav64_header_t);
	bool preload = parms->streaming_mode == WAV64_STREAMING_NONE;
	// VADPCM preloads stay compressed, laid out fully planar in RDRAM for
	// direct MIX_CHANNEL addressing. The frame size depends on the residual
	// width, which lives in the extended header: that is only read into its
	// final home further down, so peek at its prefix to size the buffer.
	bool preload_vadpcm = preload && head.format == WAV64_FORMAT_VADPCM;
	int vframe_bytes = 0;
	if (head.format == WAV64_FORMAT_VADPCM) {
		uint8_t prefix[offsetof(wav64_header_vadpcm_t, huff_tbl)];
		read(file_handle, prefix, sizeof(prefix));
		lseek(file_handle, -(int)sizeof(prefix), SEEK_CUR);
		int bits = prefix[offsetof(wav64_header_vadpcm_t, residual_bits)];
		vframe_bytes = VADPCM_FRAME_BYTES(VADPCM_RESIDUAL_BITS(bits));
	}
	int nframes = WAV64_VADPCM_FRAMES(head.len);
	int preload_size = preload_vadpcm
		? ROUND_UP(nframes * vframe_bytes * head.channels, 16)
		: ROUND_UP(head.len * head.channels * (head.nbits >> 3), 16);
	if (preload && !preload_vadpcm) {
		int ub = head.channels * (head.nbits >> 3);
		preload_size += SAMPLEBUFFER_MARGIN_UNITS * ub;
		preload_size = ROUND_UP(preload_size, 16);
	}
	int preload_extra_alloc = ROUND_UP(
		(head.format == WAV64_FORMAT_RAW || preload_vadpcm) ? 0 : 4096, 16);
	int state_size = ROUND_UP(head.state_size, 16);

	// Calculate required allocation
	int heap_size = 0;
	heap_size += ROUND_UP(sizeof(wav64_state_t), 16);				// wav64_state_t

	int heap_off_waveform = heap_size;
	if (!wav) heap_size += ROUND_UP(sizeof(wav64_t), 16);		// wav64_t (wave + st)

	int heap_off_name = heap_size;
	heap_size += ROUND_UP(strlen(file_name) + 1, 16);				// Filename

	int heap_off_samples = heap_size;
	if (preload) heap_size += preload_size;							// Preloaded samples

	int heap_off_preload_end = heap_size;
	if (preload) heap_size += preload_extra_alloc + state_size;		// Extra allocation for preload

	int heap_off_ext = heap_size;
	heap_size += ROUND_UP(ext_size, 16);							// Extended header data
	
	// Allocate heap memory
	assert(heap_size % 16 == 0);
	void *heap = memalign(16, heap_size);
	assertf(heap, "Out of memory");
	if (!wav) wav = heap + heap_off_waveform;
	wav->st = heap;
	wav->st->ext = heap + heap_off_ext;
	wav->st->samples = NULL;

	// Fill waveforms struct
	memset(&wav->wave, 0, sizeof(waveform_t));
	wav->wave.name = heap + heap_off_name;
	strcpy(heap + heap_off_name, file_name);
	wav->wave.channels = head.channels;
	wav->wave.bits = head.nbits;
	wav->wave.frequency = head.freq;
	wav->wave.len = head.len;
	wav->wave.loop_len = head.loop_len;
	wav->wave.state_size = head.state_size;

	// Read ext data
	read(file_handle, wav->st->ext, ext_size);
	data_cache_hit_writeback(wav->st->ext, ext_size);

	// Finish initialization of wav64 state
	wav->st->format = head.format;
	wav->st->current_fd = file_handle;
	wav->st->base_offset = head.start_offset + start_offset;
	wav->st->flags = owned_fd ? WAV64_FLAG_OWNED_FD : 0;
	wav->st->rom_base = 0;
	ioctl(file_handle, IODFS_GET_ROM_BASE, &wav->st->rom_base);

	// Initialize the compression algorithm
	algos[wav->st->format].init(wav, head.state_size);

	// Preload the samples if requested
	if (preload) {
		data_cache_hit_invalidate(heap + heap_off_samples, preload_size + preload_extra_alloc + state_size);
		wav->st->samples = UncachedAddr(heap + heap_off_samples);

		if (wav->wave.format == WAVEFORM_FORMAT_VADPCM) {
			wav64_vadpcm_preload(wav, wav->st->samples);
			wav->wave.mem = wav->st->samples;
			if (algos[wav->st->format].close)
				algos[wav->st->format].close(wav);
		} else {
			samplebuffer_t sbuf;
			samplebuffer_init(&sbuf, wav->st->samples, preload_size + preload_extra_alloc, state_size);
			int wlen = wav->wave.len;
			samplebuffer_set_bps(&sbuf, wav->wave.bits * wav->wave.channels);
			samplebuffer_set_waveform(&sbuf, &wav->wave, wav->wave.read);
			if (wav->wave.start) wav->wave.start(wav->wave.ctx, &sbuf);
			samplebuffer_get(&sbuf, 0, &wlen);
			samplebuffer_dma_wait(&sbuf);
			rspq_highpri_sync();
			assertf(wlen == wav->wave.len, "wav64: preload failed for %s: wlen=%x/%x", wav->wave.name, wlen, wav->wave.len);

			if (algos[wav->st->format].close)
				algos[wav->st->format].close(wav);

			wav->st = realloc(wav->st, heap_off_preload_end);
			wav->st->ext = NULL;

			// Reinitialize as RAW format after preloading
			wav->st->format = WAV64_FORMAT_RAW;
			algos[wav->st->format].init(wav, head.state_size);
		}
	}

	return wav;
}


void wav64_open(wav64_t *wav, const char *file_name)
{
	internal_open(wav, -1, file_name, NULL);
}

wav64_t* wav64_load(const char *file_name, wav64_loadparms_t *parms)
{
	return internal_open(NULL, -1, file_name, parms);
}

wav64_t* wav64_loadfd(int fd, const char *debug_file_name, wav64_loadparms_t *parms)
{
	return internal_open(NULL, fd, debug_file_name, parms);
}

void wav64_play(wav64_t *wav, int ch)
{
	mixer_ch_play(ch, &wav->wave);
}

double wav64_seek(wav64_t *wav, int ch, double time_sec)
{
	// Compute a target sample position (rounded to nearest integer sample)
	int spos = (int)(time_sec * wav->wave.frequency + 0.5);
	spos = CLAMP(spos, 0, wav->wave.len);

	// Apply codec-specific seek constraints (if any).
	if (algos[wav->st->format].adjust_seek)
		spos = algos[wav->st->format].adjust_seek(wav, spos);

	mixer_ch_set_pos(ch, spos);

	// Return the adjusted time in seconds
	return (double)spos / wav->wave.frequency;
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
	if (algos[wav->st->format].get_bitrate)
		return algos[wav->st->format].get_bitrate(wav);
	return algos[WAV64_FORMAT_RAW].get_bitrate(wav);
}

void wav64_close(wav64_t *wav)
{
	// Heap allocation always begins at wav->st.
	void *heap = wav->st;

	// For user-allocated wav64_t instances (opened via wav64_open), we allowed
	// in the past to call this function multiple times without crashing. Let's
	// keep this working, as it's easy to do.
	if (!heap)
		return;

	// Stop playing the waveform on all channels
	for (int i=0; i<MIXER_MAX_CHANNELS; i++) {
		if (mixer_ch_playing_waveform(i) == &wav->wave)
			mixer_ch_stop(i);
	}

	// Wait for in-flight mix/decode rounds that may still reference
	// codebook/state/ext before freeing the heap.
	rspq_highpri_sync();

	if (algos[wav->st->format].close)
		algos[wav->st->format].close(wav);

	if (wav->st->current_fd >= 0 && (wav->st->flags & WAV64_FLAG_OWNED_FD)) {
		close(wav->st->current_fd);
		wav->st->current_fd = -1;
	}

	memset(wav, 0, sizeof(wav64_t));

	// Free the heap allocation (that might or might not include the wav64_t instance)
	free(heap);
}

/** @brief Initialize wav64 compression level 2 */
void __wav64_init_compression_lvl2(void)
{
	algos[WAV64_FORMAT_ULC] = (wav64_compression_t){
		.init = wav64_ulc_init,
		.get_bitrate = wav64_ulc_get_bitrate,
		.adjust_seek = wav64_ulc_adjust_seek,
	};
}

/** @brief Initialize wav64 compression level 3 */
void __wav64_init_compression_lvl3(void)
{
	algos[WAV64_FORMAT_OPUS] = (wav64_compression_t){
		.init = wav64_opus_init,
		.close = wav64_opus_close,
		.get_bitrate = wav64_opus_get_bitrate,
		.adjust_seek = wav64_opus_adjust_seek,
	};
}
