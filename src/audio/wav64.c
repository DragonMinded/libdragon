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
#include <stdalign.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

/** ID of a standard WAV file */
#define WAV_RIFF_ID   "RIFF"
/** ID of a WAVX file (big-endian WAV) */
#define WAV_RIFX_ID   "RIFX"

/** @brief Profile of DMA usage by WAV64, used for debugging purposes. */
int64_t __wav64_profile_dma = 0;

/** @brief None compression init function */
static void wav64_none_init(wav64_t *wav, int state_size);
/** @brief None compression get_bitrate function */
static int wav64_none_get_bitrate(wav64_t *wav);

static wav64_compression_t algos[4] = {
	// None compression
    [WAV64_FORMAT_RAW] = {
		.init = wav64_none_init,
		.get_bitrate = wav64_none_get_bitrate,
		.default_simul = 0, // infinite
    },
	// VADPCM compression. This is always linked in as it's the default algorithm
	// for audioconv64, and it's very little code at runtime.
	[WAV64_FORMAT_VADPCM] = {
		.init = wav64_vadpcm_init,
		.close = wav64_vadpcm_close,
		.get_bitrate = wav64_vadpcm_get_bitrate,
		.default_simul = 4,
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

static void wav64_none_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	int bps = (wav->wave.bits == 8 ? 0 : 1) + (wav->wave.channels == 2 ? 1 : 0);
	
	// Always seek to allow for simultaneous playback on multiple channels with
	// a single file descriptor
	lseek(wav->st->current_fd, wav->st->base_offset + (wpos << bps), SEEK_SET);
	raw_waveform_read(sbuf, wav->st->current_fd, wpos, wlen, bps);
}

static void wav64_none_read_memcopy(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	int bps = (wav->wave.bits == 8 ? 0 : 1) + (wav->wave.channels == 2 ? 1 : 0);
	
	uint8_t* src_addr = wav->st->samples + (wpos << bps);
	uint8_t* dst_addr = (uint8_t*)samplebuffer_append(sbuf, wlen);
	memcpy(dst_addr, src_addr, wlen << bps);
}

static void wav64_none_init(wav64_t *wav, int state_size) {
	// Initialize none compression. Setup read callback
	if (!wav->st->samples) {
		wav->wave.read = wav64_none_read;
	} else {
		wav->wave.read = wav64_none_read_memcopy;
	}
	
	// NOTE: we don't need a stop callback because the none compression mode
	// supports infinite simultaneous playbacks, so there's nothing to track
	assert(wav->st->nsimul == 0);
	wav->wave.stop = NULL;
}

static int wav64_none_get_bitrate(wav64_t *wav) {
	return wav->wave.frequency * wav->wave.channels * wav->wave.bits;
}

static wav64_t* internal_open(wav64_t *wav, const char *file_name, wav64_loadparms_t *parms)
{
	wav64_loadparms_t default_parms = {0};
	if (!parms) parms = &default_parms;

	// For backwards compatibility with old versions of this file, we support
	// an unprefixed file name as a dfs file. This is deprecated and not documented
	// but we just want to avoid breaking existing code
	if (strchr(file_name, ':') == NULL) {
		char* dfs_name = alloca(5 + strlen(file_name) + 1);
		strcpy(dfs_name, "rom:/");
		strcat(dfs_name, file_name);
		file_name = dfs_name;
	}

	// Open the input file, and read the header
	int file_handle = must_open(file_name);

	wav64_header_t head;
	read(file_handle, &head, sizeof(head));

	if (memcmp(head.id, WAV64_ID, 4) != 0) {
		assertf(memcmp(head.id, WAV_RIFF_ID, 4) != 0 && memcmp(head.id, WAV_RIFX_ID, 4) != 0,
			"wav64 %s: use audioconv64 to convert to wav64 format", file_name);
		assertf(0, "wav64 %s: invalid ID: %02x%02x%02x%02x\n",
			file_name, head.id[0], head.id[1], head.id[2], head.id[3]);
	}
	assertf(head.version == 3, "wav64 %s: invalid version: %02x\n",
		file_name, head.version);
	assertf(head.format < WAV64_NUM_FORMATS, "Unknown wav64 compression format %d; corrupted file?", head.format);
	assertf(head.format < WAV64_NUM_FORMATS && algos[head.format].init != NULL,
        "wav64: compression level %d not initialized. Call wav64_init_compression(%d) at initialization time", head.format, head.format);

	int ext_size = head.start_offset - sizeof(wav64_header_t);
	int nsimul = parms->max_simultaneous_playbacks ? parms->max_simultaneous_playbacks : algos[head.format].default_simul;
	bool preload = parms->streaming_mode == WAV64_STREAMING_NONE;
	int preload_size = ROUND_UP(head.len * head.channels * (head.nbits >> 3), 16);
	int preload_extra_alloc = ROUND_UP(head.format == WAV64_FORMAT_RAW ? 0 : 4096, 16);

	// Calculate required allocation
	int heap_size = 0;
	heap_size += ROUND_UP(sizeof(wav64_state_t) + nsimul, 16);		// wav64_state_t

	int heap_off_waveform = heap_size;
	if (!wav) heap_size += ROUND_UP(sizeof(waveform_t), 16);		// Waveform

	int heap_off_samples = heap_size;
	if (preload) heap_size += preload_size;							// Preloaded samples

	int heap_off_preload_end = heap_size;
	if (preload) heap_size += preload_extra_alloc;					// Extra allocation for preload

	int heap_off_ext = heap_size;
	heap_size += ROUND_UP(ext_size, 16);							// Extended header data

	int heap_off_chstate = heap_size;
	heap_size += ROUND_UP(nsimul * head.state_size, 16);			// Per-channel state
	
	// Allocate heap memory
	assert(heap_size % 16 == 0);
	void *heap = memalign(16, heap_size);
	assertf(heap != NULL, "wav64: failed to allocate %d bytes for %s", heap_size, file_name);
	if (!wav) wav = heap + heap_off_waveform;
	wav->st = heap;
	wav->st->ext = heap + heap_off_ext;
	wav->st->states = heap + heap_off_chstate;
	wav->st->samples = NULL;

	// Fill waveforms struct
	memset(&wav->wave, 0, sizeof(waveform_t));
	wav->wave.name = file_name;
	wav->wave.channels = head.channels;
	wav->wave.bits = head.nbits;
	wav->wave.frequency = head.freq;
	wav->wave.len = head.len;
	wav->wave.loop_len = head.loop_len;

	// Read ext data
	read(file_handle, wav->st->ext, ext_size);
	data_cache_hit_writeback(wav->st->ext, ext_size);

	// Finish initialization of wav64 state
	wav->st->format = head.format;
	wav->st->current_fd = file_handle;
	wav->st->base_offset = head.start_offset;
	wav->st->nsimul = nsimul;
	wav->st->flags = 0;
	if (nsimul > 0)
		memset(wav->st->mixer_channels, -1, nsimul * sizeof(int8_t));

	// Initialize the compression algorithm
	algos[wav->st->format].init(wav, head.state_size);

	// Preload the samples if requested
	if (preload) {
		data_cache_hit_invalidate(heap + heap_off_samples, preload_size+preload_extra_alloc);
		wav->st->samples = UncachedAddr(heap + heap_off_samples);

		int wlen = wav->wave.len;
		samplebuffer_t sbuf;
		samplebuffer_init(&sbuf, wav->st->samples, preload_size + preload_extra_alloc);
		samplebuffer_set_bps(&sbuf, wav->wave.bits);
		samplebuffer_set_waveform(&sbuf, &wav->wave, wav->wave.read, 0);
		samplebuffer_get(&sbuf, 0, &wlen);
		rspq_highpri_sync();
		assertf(wlen == wav->wave.len, "wav64: preload failed for %s: wlen=%x/%x", wav->wave.name, wlen, wav->wave.len);

		// Now remove the extra allocation
		if (algos[wav->st->format].close)
			algos[wav->st->format].close(wav);

		wav->st = realloc(wav->st, heap_off_preload_end);
		wav->st->ext = NULL;
		wav->st->states = NULL;
		wav->st->nsimul = 0;

		// Reinitialize as RAW format after preloading
		wav->st->format = WAV64_FORMAT_RAW;
		algos[wav->st->format].init(wav, head.state_size);
	}

	return wav;
}


void wav64_open(wav64_t *wav, const char *file_name)
{
	internal_open(wav, file_name, NULL);
}

wav64_t* wav64_load(const char *file_name, wav64_loadparms_t *parms)
{
	return internal_open(NULL, file_name, parms);
}


void __wav64_channel_stopped(wav64_t *wav, int chidx) {
	assert(chidx >= 0 && chidx < wav->st->nsimul);
	assert(wav->st->mixer_channels[chidx] >= 0);
	wav->st->mixer_channels[chidx] = -1;
}

void wav64_play(wav64_t *wav, int ch)
{
	if (wav->st->nsimul == 0) {
		// Infinite simultaneous playbacks, no need to track anything
		mixer_ch_play(ch, &wav->wave);
		return;
	}

	// Find a state slot
	int chidx = -1;
	for (int i = 0; i < wav->st->nsimul; i++) {
		if (wav->st->mixer_channels[i] == ch || wav->st->mixer_channels[i] < 0) {
			chidx = i;
			break;
		}
	}

	if (chidx < 0) {
		if (!(wav->st->flags & WAV64_FLAG_WARN_SIMULTANEITY)) {
			debugf("wav64: too many simultaneous playbacks for %s\n", wav->wave.name);
			debugf("wav&4: (this warning will appear only once per waveform)\n");
			wav->st->flags |= WAV64_FLAG_WARN_SIMULTANEITY;
		}

		// Stop a random playing channel.
		chidx = TICKS_READ() % wav->st->nsimul;
		mixer_ch_stop(wav->st->mixer_channels[chidx]);
		assert(wav->st->mixer_channels[chidx] < 0);
	}

	mixer_ch_play_ctx(ch, &wav->wave, (void*)chidx);
	wav->st->mixer_channels[chidx] = ch;
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

	// Stop playing the waveform on all channels
	for (int i=0; i<wav->st->nsimul; i++) {
		if (wav->st->mixer_channels[i] >= 0)
			mixer_ch_stop(wav->st->mixer_channels[i]);
	}

	if (algos[wav->st->format].close)
		algos[wav->st->format].close(wav);

	if (wav->st->current_fd >= 0) {
		close(wav->st->current_fd);
		wav->st->current_fd = -1;
	}

	memset(wav, 0, sizeof(wav64_t));

	// Free the heap allocation (that might or might not include the wav64_t instance)
	free(heap);
}

/** @brief Initialize wav64 compression level 3 */
void __wav64_init_compression_lvl3(void)
{
	algos[WAV64_FORMAT_OPUS] = (wav64_compression_t){
		.init = wav64_opus_init,
		.close = wav64_opus_close,
		.get_bitrate = wav64_opus_get_bitrate,
		.default_simul = 1,
	};
}
