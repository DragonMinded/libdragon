/**
 * @file wav64.c
 * @brief Support for WAV64 audio files
 * @ingroup audio
 */

#include "libdragon.h"
#include "wav64internal.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/** @brief Profile of DMA usage by WAV64, used for debugging purposes. */
int64_t __wav64_profile_dma = 0;


/**
 * @brief Utility function to help implementing #WaveformRead for uncompressed (raw) samples.
 * 
 * This function uses PI DMA to load samples from ROM into the sample buffer.
 */  
void raw_waveform_read(samplebuffer_t *sbuf, int base_rom_addr, int wpos, int wlen, int bps) {
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
	raw_waveform_read(sbuf, wav->rom_addr, wpos, wlen, bps);
}

void wav64_open(wav64_t *wav, const char *fn) {
	memset(wav, 0, sizeof(*wav));

	int fh = dfs_open(fn);
	assertf(fh >= 0, "file does not exist: %s", fn);

	wav64_header_t head;
	dfs_read(&head, 1, sizeof(head), fh);
	assertf(strncmp(head.id, WAV64_ID, 4) == 0, "wav64 %s: invalid ID: %02x%02x%02x%02x\n",
		fn, head.id[0], head.id[1], head.id[2], head.id[3]);
	assertf(head.version == WAV64_FILE_VERSION, "wav64 %s: invalid version: %02x\n",
		fn, head.version);
	assertf(head.format == WAV64_FORMAT_RAW, "wav64 %s: invalid format: %02x\n",
		fn, head.format);

	wav->wave.name = fn;
	wav->wave.channels = head.channels;
	wav->wave.bits = head.nbits;
	wav->wave.frequency = head.freq;
	wav->wave.len = head.len;
	wav->wave.loop_len = head.loop_len; 
	wav->rom_addr = dfs_rom_addr(fn) + head.start_offset;
	dfs_close(fh);

	wav->wave.read = waveform_read;
	wav->wave.ctx = wav;
}

void wav64_set_loop(wav64_t *wav, bool loop) {
	wav->wave.loop_len = wav->wave.len;

	// Odd loop lengths are not supported for 8-bit waveforms because they would
	// change the 2-byte phase between ROM and RDRAM addresses during loop unrolling.
	// We shorten the loop by 1 sample which shouldn't matter.
	// Notice that audioconv64 does the same during conversion.
	if (wav->wave.bits == 8 && wav->wave.loop_len & 1)
		wav->wave.loop_len -= 1;
}
