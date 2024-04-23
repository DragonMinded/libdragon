/**
 * @file ym64.c
 * @brief Player for the .YM64 module format (Arkos Tracker 2)
 * @ingroup mixer
 */

#include "ym64.h"
#include "ay8910.h"
#include "../compress/lzh5_internal.h"
#include "samplebuffer.h"
#include "debug.h"
#include "asset_internal.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

/** @brief Header of a YM5 file */
typedef struct __attribute__((packed)) {
	uint32_t nframes;         ///< Number of audioframes
	uint32_t attrs;           ///< Attributes (bit 0: interleaved format)
	uint16_t ndigidrums;      ///< Number of digital samples
	uint32_t chipfreq;        ///< Frequency of the emulated chip
	uint16_t playfreq;        ///< Playback frequency in audioframes per seconds (eg: 50)
	uint32_t loop;            ///< Audioframe where the loop starts
	uint16_t sizeext;         ///< Extension (always 0)
} ym5header;

_Static_assert(sizeof(ym5header) == 22, "invalid header size");

static int ymread(ym64player_t *player, void *buf, int sz) {
	if (player->decoder)
		return decompress_lzh5_read(player->decoder, buf, sz);
	return fread(buf, 1, sz, player->f);
}

static void ym_wave_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	ym64player_t *player = (ym64player_t*)ctx;

	// Compute the number of samples per audioframe. Keep it as floating point
	// for higher precision in calculating the mapping between sample numbers
	// and audioframes.
	float f_samples_per_frame = player->wave.frequency / player->playfreq;

	// If seeking was requested (and we can seek aka file not compressed),
	// calculate the audioframe index corresponding to the seeking position
	// and then seek the file there.
	// Notice that the position could theoretically be in the middle of an
	// audioframe, but the current API should make it impossible to do:
	// both ym64player_seek and the looping position are defined in terms of
	// audioframes position not samples, so there should be no issue in
	// converting them back from sample number.
	if (seeking && !player->decoder) {
		player->curframe = ((float)wpos / f_samples_per_frame);
		fseek(player->f, player->start_off + player->curframe * 16, SEEK_SET);
	}

	// Calculate the last audioframe to be reconstructed in this call. Notice
	// that we calculate it from its absolute position using the fractional
	// number of samples per audioframe, so that we are always fully correct
	// with respect fractional frequencies.
	int lastframe = ((float)(wpos+wlen-1) / f_samples_per_frame);

	// Now switch to integers. Number of audioframes to process, and number
	// of (integer) samplers per audioframe.
	int nframes = lastframe - player->curframe + 1;
	int samples_per_frame = (int)f_samples_per_frame;

	// Get the pointer to the sample buffer.
	int16_t *samples = samplebuffer_append(sbuf, nframes*samples_per_frame);

	int16_t *out = samples;
	const int num_channels = AY8910_OUTPUT_STEREO ? 2 : 1;

	for (int i=0;i<nframes;i++) {
		// Read 14 ay8910 registers (+ maybe 2 digidrums regs, unsupported)
		uint8_t regs[16];
		ymread(player, regs, 16);

		// Iterate over the 14 ay8910 registers and see which ones
		// changed since last tick.
		for (int i=0;i<14;i++) {
			if (player->regs[i] != regs[i]) {
				player->regs[i] = regs[i];
				// Envelope register: the special value 0xFF means
				// "don't touch". Writing the reg always restarts the
				// envelope calculation, so it requires special handling.
				if (i == 13 && regs[i] == 0xFF) continue;
				ay8910_write_addr(&player->ay, i);
				ay8910_write_data(&player->ay, regs[i]);
			}
		}

		// Generate the required number of samples, and store them into the
		// sample buffer.
		ay8910_gen(&player->ay, out, samples_per_frame);
		out += (int)samples_per_frame * num_channels;
		player->curframe++;
	}
}

void ym64player_open(ym64player_t *player, const char *fn, ym64player_songinfo_t *info) {
	memset(player, 0, sizeof(*player));

	player->f = must_fopen(fn);

	int offset = 0;
	int _ymread(void *buf, int sz) {
		offset += sz;
		return ymread(player, buf, sz);
	}

	char head[13]; head[12] = 0;
	_ymread(head, 12);

	// Check if it's a LHA archive.
	if (head[2] == '-' && head[3] == 'l' && head[6] == '-') {
		assertf(head[4] == 'h' && head[5] == '5', "Unsupported LHA compression algorithm: -l%c%c-", head[4], head[5]);

		// Skip the header. We don't need anything else from it, just go straight
		// to the first compressed file which ought to be our YM file.
		fseek(player->f, head[0]+2, SEEK_SET);

		// Initialize decompressor and re-read the header (this time, it will
		// be decompressed and we should find a valid YM header).
		player->decoder = malloc(DECOMPRESS_LZH5_STATE_SIZE + DECOMPRESS_LZH5_DEFAULT_WINDOW_SIZE);
		offset = 0;
		decompress_lzh5_init(player->decoder, player->f, DECOMPRESS_LZH5_DEFAULT_WINDOW_SIZE);
		_ymread(head, 12);
	}

	int loop_pos = 0;

	if (strncmp(head, "YM6!", 4) == 0 || strncmp(head, "YM5!", 4) == 0) {
		assertf(strncmp(head+4, "LeOnArD!", 8) == 0, "invalid YM check string: %s", head+4);

		ym5header h; char buf[512];
		_ymread(&h, sizeof(h));

		// Interleaved format is hard to support while streaming (especially compressed)
		// so let's punt for now.
		assertf((h.attrs & 1) == 0, "Interleaved YM format not supported");

		player->nframes = h.nframes;
		player->chipfreq = h.chipfreq;
		player->playfreq = h.playfreq;
		loop_pos = h.loop;

		// Skip digidrum, not supported yet
		if (h.ndigidrums) {
			debugf("ymplayer: %s: digidrums are not supported, skipped\n", fn);
			for (int i=0;i<h.ndigidrums;i++) {
				uint32_t sz;
				_ymread(&sz, 4);
				while (sz > 0)
					sz -= _ymread(buf, MIN(sz, sizeof(buf)));
			}
		}

		// Read Song info
		int i = 0; 
		do _ymread(&buf[i], 1);
		while (buf[i++] != '\0');
		if (info) strlcpy(info->name, buf, sizeof(info->name));

		i = 0;
		do _ymread(&buf[i], 1);
		while (buf[i++] != '\0');
		if (info) strlcpy(info->author, buf, sizeof(info->author));

		i = 0;
		do _ymread(&buf[i], 1);
		while (buf[i++] != '\0');
		if (info) strlcpy(info->comment, buf, sizeof(info->comment));
	} else if (strncmp(head, "YM3!", 4) == 0) {
		assertf(0, "YM3 format cannot be played -- convert with audioconv64");
	} else {
		assertf(0, "invalid YM header: %s", head);
	}

	// Record the file offset at the beginning of audio frames. This will
	// be useful for looping.
	player->start_off = offset;

	// Compute playback frequency. Use floating point for accurate representation
	// of what is requested by the module definition. The mixer supports fractional
	// frequency so we don't want to waste precision.
	float freq = (float)player->chipfreq / 8 / AY8910_DECIMATE;

	// Compute the waveform length and loop start position in sample. Again, use
	// floating point here to try to be as accurate as possible.
	int len = (float)player->nframes * freq / player->playfreq;
	int loop_start = (float)loop_pos * freq / player->playfreq;

	// Create the mixer waveform
	player->wave = (waveform_t){
		.name = fn,
		.bits = 16,
		.channels = AY8910_OUTPUT_STEREO ? 2 : 1,
		.frequency = freq,
		.len = len,
		.loop_len = len-loop_start,
		.read = ym_wave_read,
		.ctx = player,
	};

	ay8910_reset(&player->ay);
	player->first_ch = -1;
	debugf("ym64: loading %s (freq:%ld, wfreq:%ld)\n", fn, player->chipfreq/8, player->chipfreq/8/AY8910_DECIMATE);
}

int ym64player_num_channels(ym64player_t *player) {
	return player->wave.channels;
}

void ym64player_play(ym64player_t *player, int first_ch) {
	player->first_ch = first_ch;
	mixer_ch_play(first_ch, &player->wave);
	mixer_ch_set_vol(first_ch, 1.0f, 1.0f);
	mixer_ch_set_pos(first_ch, (float)player->curframe * player->wave.frequency / player->playfreq);
}

void ym64player_stop(ym64player_t *player) {
	if (player->first_ch >= 0) {
		mixer_ch_stop(player->first_ch);
		player->first_ch = 0;
	}
}

void ym64player_duration(ym64player_t *player, int *len, float *secs) {
	if (len) *len = player->nframes;
	if (secs) *secs = (float)player->nframes / (float)player->playfreq;
}

void ym64player_tell(ym64player_t *player, int *pos, float *secs) {
	if (pos) *pos = player->curframe;
	if (secs) *secs = (float)player->curframe / (float)player->playfreq;
}

bool ym64player_seek(ym64player_t *player, int pos) {
	// Cannot seek in a compressed file
	if (player->decoder)
		return false;

	// If playing, seek through the mixer. Otherwise, at least record
	// the current audioframe, that will be applied later when ym64player_play
	// is called.
	if (player->first_ch >= 0)
		mixer_ch_set_pos(player->first_ch, (float)pos * player->wave.frequency / player->playfreq);
	player->curframe = pos;
	return true;
}

void ym64player_close(ym64player_t *player) {
	ym64player_stop(player);

	if (player->decoder) {
		free(player->decoder);
		player->decoder = NULL;
	}

	if (player->f) {
		fclose(player->f);
		player->f = NULL;
	}
}
