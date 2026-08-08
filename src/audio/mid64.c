/**
 * @file mid64.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief MID64 sequence loader, decoder, and MixerEvent player
 * @ingroup mixer
 */
#include "mid64.h"
#include "mid64_internal.h"
#include "asset.h"
#include "audio.h"
#include "mixer.h"
#include "debug.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MIDI_STATUS_MASK       0xF0
#define MIDI_CHANNEL_MASK      0x0F
#define MIDI_STATUS_BIT        0x80
#define MIDI_NOTE_OFF          0x80
#define MIDI_NOTE_ON           0x90
#define MIDI_POLY_PRESSURE     0xA0
#define MIDI_CONTROL_CHANGE    0xB0
#define MIDI_PROGRAM_CHANGE    0xC0
#define MIDI_CHANNEL_PRESSURE  0xD0
#define MIDI_PITCH_BEND        0xE0

struct mid64player_s {
	uint8_t *file_buf;			///< asset_load() buffer (header + events)
	uint8_t *events;			///< Event stream within @ref file_buf
	uint32_t events_size;
	uint32_t cursor;
	uint16_t ppqn;
	uint8_t running_status;
	uint32_t tempo_us;
	uint32_t duration_ticks;
	uint32_t duration_ms;
	uint64_t midi_tick;

	uint32_t sample_rate;		///< From #audio_get_frequency at play
	uint64_t timing_remainder;	///< Fractional samples from tick→sample
	int64_t event_sample;		///< Absolute sample of last peeked delta
	int64_t next_midi_sample;	///< Absolute sample of next MIDI event
	int64_t scheduled_sample;	///< Absolute sample of current MixerEvent
	int64_t song_start_sample;	///< Absolute sample at start of this loop iter
	bool pending;				///< VLQ peeked; payload not yet dispatched

	midi_target_t *target;
	bool playing;
	bool stop_requested;
	bool looping;
};

static inline uint8_t mid64_read_u8(mid64player_t *p)
{
	assertf(p->cursor < p->events_size, "MID64: truncated event stream");
	return p->events[p->cursor++];
}

static uint32_t mid64_read_vlq(mid64player_t *p)
{
	uint32_t v = 0;
	for (int i = 0; i < 4; i++) {
		uint8_t b = mid64_read_u8(p);
		v = (v << 7) | (b & 0x7f);
		if (!(b & 0x80)) return v;
	}
	assertf(0, "MID64: VLQ longer than 4 bytes");
}

static int channel_data_len(uint8_t status)
{
	switch (status & MIDI_STATUS_MASK) {
	case MIDI_PROGRAM_CHANGE:
	case MIDI_CHANNEL_PRESSURE:
		return 1;
	case MIDI_NOTE_OFF:
	case MIDI_NOTE_ON:
	case MIDI_POLY_PRESSURE:
	case MIDI_CONTROL_CHANGE:
	case MIDI_PITCH_BEND:
		return 2;
	default:
		return -1;
	}
}

/** Convert @p delta ticks to samples; updates @ref mid64player_s::timing_remainder. */
static uint64_t mid64_ticks_to_samples(mid64player_t *p, uint32_t delta)
{
	uint64_t den = (uint64_t)p->ppqn * 1000000ull;
	// delta * tempo_us * rate can exceed 64 bits for large VLQs; use 128-bit.
	__int128 num = (__int128)p->timing_remainder
		+ (__int128)delta * p->tempo_us * p->sample_rate;
	uint64_t samples = (uint64_t)(num / den);
	p->timing_remainder = (uint64_t)(num % den);
	return samples;
}

static void mid64_dispatch_channel(mid64player_t *p, midi_target_t *target,
	uint8_t status, uint8_t data1, uint8_t data2, int64_t now)
{
	int ch = status & MIDI_CHANNEL_MASK;
	const midi_target_ops_t *ops = target->ops;

	switch (status & MIDI_STATUS_MASK) {
	case MIDI_NOTE_OFF:
		ops->note_off(target, ch, data1, data2, now);
		break;
	case MIDI_NOTE_ON:
		ops->note_on(target, ch, data1, data2, now);
		break;
	case MIDI_POLY_PRESSURE:
		if (ops->poly_pressure)
			ops->poly_pressure(target, ch, data1, data2, now);
		break;
	case MIDI_CONTROL_CHANGE:
		ops->control_change(target, ch, data1, data2, now);
		break;
	case MIDI_PROGRAM_CHANGE:
		ops->program_change(target, ch, data1, now);
		break;
	case MIDI_CHANNEL_PRESSURE:
		if (ops->channel_pressure)
			ops->channel_pressure(target, ch, data1, now);
		break;
	case MIDI_PITCH_BEND:
		ops->pitch_bend(target, ch, data1 | (data2 << 7), now);
		break;
	}
}

/** Peek next delta; leave the event payload for #mid64_dispatch_pending. */
static void mid64_peek_next(mid64player_t *p)
{
	uint32_t delta = mid64_read_vlq(p);
	p->midi_tick += delta;
	p->event_sample += (int64_t)mid64_ticks_to_samples(p, delta);
	p->next_midi_sample = p->event_sample;
	p->pending = true;
}

/**
 * Dispatch the pending payload at sample @p now, then peek the following event.
 * @return false if END was reached (no further peek)
 */
static bool mid64_dispatch_pending(mid64player_t *p, int64_t now)
{
	assert(p->pending);
	p->pending = false;

	uint8_t b = mid64_read_u8(p);
	if (b == MID64_OP_END)
		return false;

	if (b == MID64_OP_SET_TEMPO) {
		uint32_t tempo = ((uint32_t)mid64_read_u8(p) << 16)
			| ((uint32_t)mid64_read_u8(p) << 8)
			| mid64_read_u8(p);
		assertf(tempo != 0, "MID64: tempo is zero");
		p->tempo_us = tempo;
		p->running_status = 0;
		mid64_peek_next(p);
		return true;
	}

	uint8_t status, data1;
	if (b & MIDI_STATUS_BIT) {
		status = b;
		p->running_status = status;
		data1 = mid64_read_u8(p);
	} else {
		assertf(p->running_status, "MID64: data byte without running status");
		status = p->running_status;
		data1 = b;
	}

	int dlen = channel_data_len(status);
	assertf(dlen >= 0, "MID64: unsupported status 0x%02x", status);
	uint8_t data2 = (dlen == 2) ? mid64_read_u8(p) : 0;
	mid64_dispatch_channel(p, p->target, status, data1, data2, now);
	mid64_peek_next(p);
	return true;
}

/** Restart timeline after END when looping (sample clock continues at @p now). */
static void mid64_loop_restart(mid64player_t *p, int64_t now)
{
	p->cursor = 0;
	p->running_status = 0;
	p->tempo_us = MID64_DEFAULT_TEMPO;
	p->midi_tick = 0;
	p->timing_remainder = 0;
	p->event_sample = now;
	p->song_start_sample = now;
	p->pending = false;
	mid64_peek_next(p);
}

static int mid64_tick(void *arg)
{
	mid64player_t *p = arg;
	int64_t now = p->scheduled_sample;

	if (p->stop_requested) {
		if (p->target->ops->reset)
			p->target->ops->reset(p->target, now);
		p->playing = false;
		p->stop_requested = false;
		return 0;
	}

	// Sync synth clock / envelopes to this sample before MIDI at `now`.
	if (p->target->ops->process)
		p->target->ops->process(p->target, now);

	while (p->next_midi_sample <= now) {
		if (!mid64_dispatch_pending(p, now)) {
			if (p->looping) {
				if (p->target->ops->reset)
					p->target->ops->reset(p->target, now);
				mid64_loop_restart(p, now);
				continue;
			}
			p->next_midi_sample = INT64_MAX;
			break;
		}
	}

	int64_t synth_next = INT64_MAX;
	if (p->target->ops->process)
		synth_next = p->target->ops->process(p->target, now);

	int64_t next = p->next_midi_sample < synth_next ? p->next_midi_sample : synth_next;
	if (next == INT64_MAX) {
		p->playing = false;
		return 0;
	}

	int64_t delay = next - now;
	assertf(delay > 0, "MID64: non-positive mixer delay");
	if (delay > INT_MAX) delay = INT_MAX;
	p->scheduled_sample = next;
	return (int)delay;
}

mid64player_t *mid64player_load(const char *fn)
{
	int sz;
	uint8_t *buf = asset_load(fn, &sz);
	assertf(sz >= (int)sizeof(mid64_header_t),
		"cannot load MID64 file: %s\nTruncated header", fn);

	mid64_header_t *head = (mid64_header_t *)buf;
	assertf(memcmp(head->magic, MID64_ID, 4) == 0,
		"cannot load MID64 file: %s\nInvalid magic", fn);
	assertf(head->version == MID64_VERSION,
		"cannot load MID64 file: %s\nVersion %d not supported", fn, head->version);
	assertf(head->events_offset >= MID64_HEADER_SIZE
		&& head->events_offset + head->events_size <= (uint32_t)sz,
		"cannot load MID64 file: %s\nInvalid event stream range", fn);

	mid64player_t *player = calloc(1, sizeof(*player));
	assert(player);
	player->file_buf = buf;
	player->events = buf + head->events_offset;
	player->events_size = head->events_size;
	player->ppqn = head->ppqn;
	player->duration_ticks = head->duration_ticks;
	player->duration_ms = head->duration_ms;
	mid64player_rewind(player);
	return player;
}

void mid64player_close(mid64player_t *player)
{
	assert(player);
	if (player->playing) {
		mixer_remove_event(mid64_tick, player);
		player->playing = false;
	}
	free(player->file_buf);
	free(player);
}

void mid64player_rewind(mid64player_t *player)
{
	assert(player);
	player->cursor = 0;
	player->running_status = 0;
	player->tempo_us = MID64_DEFAULT_TEMPO;
	player->midi_tick = 0;
	player->pending = false;
}

bool mid64player_decode_next(mid64player_t *player, midi_target_t *target)
{
	assert(player);
	assert(target && target->ops);

	uint32_t delta = mid64_read_vlq(player);
	player->midi_tick += delta;
	int64_t now = (int64_t)player->midi_tick;

	uint8_t b = mid64_read_u8(player);
	if (b == MID64_OP_END)
		return false;

	if (b == MID64_OP_SET_TEMPO) {
		uint32_t tempo = ((uint32_t)mid64_read_u8(player) << 16)
			| ((uint32_t)mid64_read_u8(player) << 8)
			| mid64_read_u8(player);
		assertf(tempo != 0, "MID64: tempo is zero");
		player->tempo_us = tempo;
		player->running_status = 0;
		return true;
	}

	uint8_t status, data1;
	if (b & MIDI_STATUS_BIT) {
		status = b;
		player->running_status = status;
		data1 = mid64_read_u8(player);
	} else {
		assertf(player->running_status, "MID64: data byte without running status");
		status = player->running_status;
		data1 = b;
	}

	int dlen = channel_data_len(status);
	assertf(dlen >= 0, "MID64: unsupported status 0x%02x", status);
	uint8_t data2 = (dlen == 2) ? mid64_read_u8(player) : 0;
	mid64_dispatch_channel(player, target, status, data1, data2, now);
	return true;
}

void mid64player_play(mid64player_t *player, midi_target_t *target)
{
	assert(player);
	assert(target && target->ops);
	if (player->playing)
		return;

	player->sample_rate = audio_get_frequency();
	assertf(player->sample_rate > 0, "MID64: call audio_init before play");

	player->target = target;
	player->stop_requested = false;
	mid64player_rewind(player);
	player->timing_remainder = 0;
	player->event_sample = 0;
	player->scheduled_sample = 0;
	player->song_start_sample = 0;
	mid64_peek_next(player);

	if (target->ops->reset)
		target->ops->reset(target, 0);

	player->playing = true;
	mixer_add_event(0, mid64_tick, player);
}

void mid64player_stop(mid64player_t *player)
{
	assert(player);
	player->stop_requested = true;
}

void mid64player_set_loop(mid64player_t *player, bool loop)
{
	assert(player);
	player->looping = loop;
}

uint16_t mid64player_get_ppqn(mid64player_t *player)
{
	assert(player);
	return player->ppqn;
}

uint32_t mid64player_get_duration_ticks(mid64player_t *player)
{
	assert(player);
	return player->duration_ticks;
}

uint32_t mid64player_get_duration_ms(mid64player_t *player)
{
	assert(player);
	return player->duration_ms;
}

uint32_t mid64player_get_tempo(mid64player_t *player)
{
	assert(player);
	return player->tempo_us;
}

uint32_t mid64player_tell_ms(mid64player_t *player)
{
	assert(player);
	int64_t samples = player->scheduled_sample - player->song_start_sample;
	uint32_t rate = player->sample_rate;
	if (rate == 0 || samples <= 0)
		return 0;
	uint64_t ms = (uint64_t)samples * 1000ull / rate;
	if (ms > 0xffffffffu)
		return 0xffffffffu;
	return (uint32_t)ms;
}
