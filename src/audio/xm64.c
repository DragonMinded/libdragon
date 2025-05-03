/**
 * @file xm64.c
 * @brief Efficient XM module player
 * @ingroup mixer
 */

#include "xm64.h"
#include "wav64.h"
#include "mixer.h"
#include "audio.h"
#include <assert.h>
#include "debug.h"
#include "interrupt.h"
#include "dragonfs.h"
#include "wav64_internal.h"
#include "asset_internal.h"
#include "libxm/xm.h"
#include "libxm/xm_internal.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static char *xm64_extsampledir = NULL;

static void stop(xm_context_t *ctx, xm64player_t *xmp) {
	for (int i=0;i<ctx->module.num_channels;i++)
		mixer_ch_stop(xmp->first_ch+i);
	xmp->playing = false;
	xmp->stop_requested = false;
}

static int tick(void *arg) {
	xm64player_t *xmp = (xm64player_t*)arg;
	xm_context_t *ctx = xmp->ctx;
	int first_ch = xmp->first_ch;

	for (int i=0;i<ctx->module.num_channels;i++) {
		xm_channel_context_t *ch = &ctx->channels[i];
		if (mixer_ch_playing(first_ch+i))
			ch->sample_position = mixer_ch_get_pos(first_ch+i);
	}

	// If we're requested to stop playback, do it.
	if (xmp->stop_requested) {
		stop(ctx, xmp);
		return 0;
	}

	if (xmp->seek.patidx >= 0) {
		// Seek was requested. Do it.
		xm_seek(ctx, xmp->seek.patidx, xmp->seek.row, xmp->seek.tick);
		xmp->seek.patidx = -1;
		for (int i=0;i<ctx->module.num_channels;i++) {
			xm_channel_context_t *ch = &ctx->channels[i];
			ch->sample_position = 0;
		}
		// Turn off all currently-playing samples, so that we don't risk keep
		// playing them.
		for (int i=0;i<ctx->module.num_channels;i++)
			mixer_ch_stop(first_ch+i);
	}

	assert(ctx->remaining_samples_in_tick <= 0);
	xm_tick(ctx);

	if (!xmp->looping && ctx->loop_count > 0) {
		stop(ctx, xmp);
		return 0;
	}

	float gvol = ctx->global_volume * ctx->amplification;

	for (int i=0;i<ctx->module.num_channels;i++) {
		xm_channel_context_t *ch = &ctx->channels[i];
		if (ch->sample && ch->sample_position >= 0) {
			wav64_t *w = ch->sample->wave;
			
			// Check if this sample is muted. This is an user-level muting
			// control exposed via the xm.h API that we respect in case the
			// user wants to mute some channels (usually for debugging).
			bool muted = ch->muted || ch->instrument->muted;

			// Play the waveform, if it was not already playing. We don't handle
			// explicit key-on events here since it's a bit complex in XM, so
			// we just passively check whether we need to start playing or not.
			if (mixer_ch_playing_waveform(first_ch+i) != &w->wave)
				wav64_play(w, first_ch+i);

			// Set the position of the sample expected by the playback engine.
			mixer_ch_set_pos(first_ch+i, ch->sample_position);

			// Configure also frequency and volume that might have changed
			// since last tick.
			mixer_ch_set_freq(first_ch+i, ch->frequency);
			mixer_ch_set_vol(first_ch+i,
				muted ? 0 : gvol * ch->actual_volume[0],
				muted ? 0 : gvol * ch->actual_volume[1]);
		} else {
			mixer_ch_stop(first_ch+i);
		}
	}

	// Schedule next tick according to the number of samples in this tick.
	int delay = ceilf(ctx->remaining_samples_in_tick);
	ctx->remaining_samples_in_tick -= delay;
	return delay;
}

/** @brief Header of the XM64 file */
typedef struct __attribute__((packed)) {
	char magic[4]; 					// "XM64"
	uint8_t version;				// version of the file format
	uint32_t metadata_offset; 		// offset of the metadata
	uint32_t metadata_size;			// size of the metadata
} xm64_header_t;

void xm64player_open(xm64player_t *player, const char *fn) {
	memset(player, 0, sizeof(*player));
	player->fd = -1;

	// No pending seek at the moment, we start from beginning anyway.
	player->seek.patidx = -1;

	// Open the file as a file descriptor
	int fd = must_open(fn);

	// Read the header to check if this is a valid XM64 file
	xm64_header_t header;
	read(fd, &header, sizeof(header));
	if (memcmp(header.magic, "XM64", 4) != 0) {
		if (memcmp(header.magic, "Exte", 4) == 0) {
			assertf(0, "cannot load XM file: %s\nPlease convert to XM64 with audioconv64", fn);
		}
		assertf(0, "cannot load XM64 file: %s\nFile corrupted", fn);
	}
	assertf(header.version == 11, "cannot load XM64 file: %s\nVersion %d not supported", fn, header.version);

	// Seek to the beginning of the metadata, that are asset-compressed. We need
	// to read the metadata in small chunks, so we use asset_fopen() for this.
	lseek(fd, header.metadata_offset, SEEK_SET);
	FILE *fh = asset_fdopen(fd, NULL);

	// Load the XM context
	int sample_rate = audio_get_frequency();
	assertf(sample_rate >= 0, "audio_init() and mixer_init() must be called before xm64player_open()");
	int err = xm_context_load(&player->ctx, fh, sample_rate);
	if (err != 0) {
		if (err == 2) {
			assertf(0, "error loading XM64 file: %s\nMemory size estimation by audioconv64 was wrong\n", fn);
		}
		assertf(0, "error loading XM64 file: %s\nFile corrupted", fn);
	}

	fclose(fh);

	// Reopen as unbuffered file descriptor. This will be used for streaming.
	player->fd = must_open(fn);
	player->ctx->fd = player->fd;

	char *extfn = NULL;
	if (player->ctx->external_samples) {
		assertf(xm64_extsampledir != NULL, "%s: external samples enabled but no directory set. Call xm64_set_extsampledir() first", fn);
		extfn = alloca(strlen(xm64_extsampledir) + 1 + 8 + 1 + 5 + 1);
	}
		
	// Open all embedded wav64 files
	for (int i=0; i<xm_get_number_of_instruments(player->ctx); i++) {
		xm_instrument_t *inst = &player->ctx->module.instruments[i];
		for (int j=0;j<inst->num_samples;j++) {
			xm_sample_t *samp = &inst->samples[j];

			if (!player->ctx->external_samples) {
				char filename[128];
				snprintf(filename, sizeof(filename), "%s[%d:%d]", fn, i+1, j);
				lseek(player->fd, samp->data8_offset, SEEK_SET);
				samp->wave = wav64_loadfd(player->fd, filename, NULL);
			} else {
				sprintf(extfn, "%s/%08lx.wav64", xm64_extsampledir, samp->data8_offset);
				samp->wave = wav64_load(strdup(extfn), NULL);
			}
		}
	}

	// By default XM64 files loop
	player->looping = true;
}

int xm64player_num_channels(xm64player_t *player) {
	return xm_get_number_of_channels(player->ctx);
}

void xm64player_set_loop(xm64player_t *player, bool loop) {
	player->looping = loop;
}

void xm64player_play(xm64player_t *player, int first_ch) {
	assert(first_ch + xm_get_number_of_channels(player->ctx) <= MIXER_MAX_CHANNELS);

	if (!player->playing) {
		// XM64 header contains the optimal size for sample buffers on each
		// channel, to minimize memory consumption. To configure it, bump
		// the frequency of each channel to an unreasonably high value (we don't
		// know how much we need, so shoot high), but then limit the buffer size
		// to the optimal value.
		for (int i=0; i<player->ctx->module.num_channels; i++) {
			// If the value is 0, the channel is not used. We don't have a way
			// to convey this (0 would be interpreted as "no limit"), so just
			// avoid calling the limit function altogether.
			if (player->ctx->ctx_size_stream_sample_buf[i] != 0)
				mixer_ch_set_limits(first_ch+i, 0, 1e9, player->ctx->ctx_size_stream_sample_buf[i]);
		}

		mixer_add_event(0, tick, player);
		player->first_ch = first_ch;
		player->playing = true;
	}
}

void xm64player_stop(xm64player_t *player) {
	// Let the mixer callback stop playing
	player->stop_requested = true;
}

void xm64player_tell(xm64player_t *player, int *patidx, int *row, float *secs) {
	// Disable interrupts to try to avoid race conditions with the player
	// running in a different thread. Notice that this is not sufficient
	// (you would need some kind of mutex), but let's say good enough,
	// especially since the audio thread is going to be higher priority.
	disable_interrupts();
	uint8_t patidx_, row_; uint64_t samples;
	xm_get_position(player->ctx, &patidx_, NULL, &row_, &samples);
	bool has_pending_seek = player->seek.patidx >= 0;
	if (patidx) *patidx = has_pending_seek ? player->seek.patidx : patidx_;
	if (row) *row = has_pending_seek ? player->seek.row : row_;
	if (secs) *secs = (float)samples / (float)player->ctx->rate;
	enable_interrupts();
}

void xm64player_seek(xm64player_t *player, int patidx, int row, int tick) {
	// Schedule seeking at next tick. Don't seek here to avoid
	// race conditions when the mixer is running in another thread.
	disable_interrupts();
	player->seek.patidx = patidx;
	player->seek.row = row;
	player->seek.tick = tick;
	enable_interrupts();
}

void xm64player_set_vol(xm64player_t *player, float volume) {
	// Store the volume in the libxm context as amplification.
	// 0.25f is the default suggested value, so we scale by it.
	player->ctx->amplification = volume * 0.25f;
}

void xm64player_set_effect_callback(xm64player_t *player, void (*cb)(void*, uint8_t, uint8_t, uint8_t), void *ctx) {
	xm_set_effect_callback(player->ctx, cb, ctx);
}

void xm64player_close(xm64player_t *player) {
	if (player->playing) {
		mixer_remove_event(tick, player);
		player->playing = false;
	}

	for (int i=0;i<player->ctx->module.num_channels;i++) {
		mixer_ch_stop(player->first_ch+i);
		mixer_ch_set_limits(player->first_ch+i, 0, 0, 0);
	}

	// Close all embedded wav64 files
	for (int i=0; i<player->ctx->module.num_instruments; i++) {
		xm_instrument_t *inst = &player->ctx->module.instruments[i];
		for (int j=0; j<inst->num_samples; j++) {
			xm_sample_t *samp = &inst->samples[j];
			if (samp->wave) {
				wav64_close(samp->wave);
				samp->wave = NULL;
			}
		}
	}

	if (player->fd >= 0) {
		close(player->fd);
		player->fd = -1;
	}

	if (player->ctx) {
		xm_free_context(player->ctx);
		player->ctx = NULL;
	}
}

void xm64_set_extsampledir(const char *dir) {
	if (dir == NULL) {
		free(xm64_extsampledir);
		xm64_extsampledir = NULL;
	} else {
		xm64_extsampledir = strdup(dir);
	}
}
