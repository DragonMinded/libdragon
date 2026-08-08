/*
    conv_mid64: convert Standard MIDI Files to MID64 format
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <algorithm>
#include <cstring>
#include <vector>

#include "../common/binout.h"
#include "../common/utils.h"
#include "../common/assetcomp.h"
#include "audioconv64.h"

int flag_mid_compress = DEFAULT_COMPRESSION;

#define MID64_MAGIC "MD64"
#define MID64_VERSION 1
#define MID64_HEADER_SIZE 32
#define MID64_OP_SET_TEMPO 0xF0
#define MID64_OP_END 0xFF

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
#define MIDI_SYSEX             0xF0
#define MIDI_SYSEX_ESCAPE      0xF7
#define MIDI_META              0xFF
#define MIDI_META_EOT          0x2F
#define MIDI_META_TEMPO        0x51

enum class midi_event_type { CHANNEL, TEMPO };

struct midi_event_t {
	uint64_t tick;
	uint32_t sequence;
	uint16_t track;
	midi_event_type type;
	uint8_t status;
	uint8_t data1;
	uint8_t data2;
	uint32_t tempo_us;
};

struct mid_stats_t {
	int format;
	int tracks;
	int ppqn;
	int input_events;
	int output_events;
	int tempo_changes;
	int sysex_stripped;
	int meta_stripped;
	long input_size;
	long events_size;
	long total_size;
};

static const uint8_t *g_end;

static uint8_t read_u8(const uint8_t *&p)
{
	if (p >= g_end) fatal("ERROR: truncated MIDI file");
	return *p++;
}

static uint16_t read_u16(const uint8_t *&p)
{
	uint16_t v = read_u8(p) << 8;
	return v | read_u8(p);
}

static uint32_t read_u32(const uint8_t *&p)
{
	uint32_t v = (uint32_t)read_u8(p) << 24;
	v |= (uint32_t)read_u8(p) << 16;
	v |= (uint32_t)read_u8(p) << 8;
	return v | read_u8(p);
}

static uint32_t read_vlq(const uint8_t *&p)
{
	uint32_t v = 0;
	for (int i = 0; i < 4; i++) {
		uint8_t b = read_u8(p);
		v = (v << 7) | (b & 0x7f);
		if (!(b & 0x80)) return v;
	}
	fatal("ERROR: MIDI VLQ longer than 4 bytes");
}

static void write_vlq(FILE *f, uint32_t v)
{
	uint8_t buf[4];
	int n = 0;
	buf[n++] = v & 0x7f;
	while (v >>= 7)
		buf[n++] = (v & 0x7f) | 0x80;
	while (n--)
		w8(f, buf[n]);
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

static void parse_track(const uint8_t *&p, uint32_t len, unsigned track,
	std::vector<midi_event_t> &events, uint64_t &duration, mid_stats_t &st)
{
	const uint8_t *end = p + len;
	if (end > g_end) fatal("ERROR: track extends past end of file");

	uint64_t tick = 0;
	uint8_t running = 0;
	uint32_t seq = 0;
	// Restrict the reader to this track's bytes for the duration of the loop.
	const uint8_t *save_end = g_end;
	g_end = end;

	while (p < end) {
		// Advance absolute time by the event's delta-tick VLQ.
		uint32_t delta = read_vlq(p);
		tick += delta;

		uint8_t b = read_u8(p);

		// Meta events: keep Set Tempo, strip everything else; EOT ends the track.
		if (b == MIDI_META) {
			uint8_t type = read_u8(p);
			uint32_t mlen = read_vlq(p);
			if (p + mlen > end) fatal("ERROR: truncated meta event");
			if (type == MIDI_META_EOT) {
				if (mlen != 0) fatal("ERROR: invalid End of Track");
				duration = std::max(duration, tick);
				running = 0;
				st.meta_stripped++;
				break;
			}
			if (type == MIDI_META_TEMPO) {
				if (mlen != 3) fatal("ERROR: invalid Set Tempo length");
				uint32_t tempo = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
				if (tempo == 0) fatal("ERROR: tempo is zero");
				events.push_back({tick, seq++, (uint16_t)track, midi_event_type::TEMPO,
					0, 0, 0, tempo});
				st.tempo_changes++;
			} else {
				st.meta_stripped++;
			}
			p += mlen;
			running = 0; // meta cancels running status
			continue;
		}

		// SysEx: parse length and discard payload (not used in MID64 v1).
		if (b == MIDI_SYSEX || b == MIDI_SYSEX_ESCAPE) {
			uint32_t slen = read_vlq(p);
			if (p + slen > end) fatal("ERROR: truncated SysEx");
			p += slen;
			running = 0; // SysEx cancels running status
			st.sysex_stripped++;
			continue;
		}

		// Channel voice: status byte or reuse previous running status.
		uint8_t status;
		uint8_t data1;
		if (b & MIDI_STATUS_BIT) {
			status = b;
			if (channel_data_len(status) < 0)
				fatal("ERROR: unsupported MIDI status 0x%02x", status);
			running = status;
			data1 = read_u8(p);
		} else {
			if (!running)
				fatal("ERROR: MIDI data byte without running status");
			status = running;
			data1 = b;
		}

		// Consume the remaining data byte(s) and validate 7-bit MIDI data.
		int dlen = channel_data_len(status);
		uint8_t data2 = 0;
		if (dlen == 2)
			data2 = read_u8(p);
		if (data1 & MIDI_STATUS_BIT)
			fatal("ERROR: invalid MIDI data byte 0x%02x", data1);
		if (dlen == 2 && (data2 & MIDI_STATUS_BIT))
			fatal("ERROR: invalid MIDI data byte 0x%02x", data2);

		// Normalize note-on with velocity 0 into an explicit note-off.
		if ((status & MIDI_STATUS_MASK) == MIDI_NOTE_ON && data2 == 0) {
			status = MIDI_NOTE_OFF | (status & MIDI_CHANNEL_MASK);
			data2 = 0;
		}

		events.push_back({tick, seq++, (uint16_t)track, midi_event_type::CHANNEL,
			status, data1, data2, 0});
	}

	if (p != end) fatal("ERROR: track length mismatch");
	g_end = save_end;
	duration = std::max(duration, tick);
}

int mid_convert(const char *infn, const char *outfn)
{
	// Slurp the whole SMF into memory; conversion is host-side only.
	std::vector<uint8_t> data = slurp(infn);
	if (data.empty()) fatal("ERROR: cannot open: %s", infn);

	const uint8_t *p = data.data();
	g_end = data.data() + data.size();

	// Parse and validate the MThd header (format, track count, PPQN).
	if (data.size() < 14 || memcmp(p, "MThd", 4) != 0)
		fatal("ERROR: not a Standard MIDI File: %s", infn);
	p += 4;
	uint32_t hdr_len = read_u32(p);
	if (hdr_len < 6) fatal("ERROR: invalid MIDI header length");
	uint16_t format = read_u16(p);
	uint16_t ntrks = read_u16(p);
	uint16_t division = read_u16(p);
	p += hdr_len - 6;

	if (format > 1)
		fatal("ERROR: MIDI format %u not supported (only 0 and 1)", format);
	if (division & 0x8000)
		fatal("ERROR: SMPTE timing not supported");
	if (division == 0)
		fatal("ERROR: invalid PPQN");
	if (ntrks == 0)
		fatal("ERROR: MIDI file has no tracks");

	mid_stats_t st = {};
	st.format = format;
	st.tracks = ntrks;
	st.ppqn = division;
	st.input_size = (long)data.size();

	// Parse every MTrk into a shared event list (absolute ticks).
	std::vector<midi_event_t> events;
	uint64_t duration = 0;
	for (unsigned t = 0; t < ntrks; t++) {
		if (p + 8 > g_end) fatal("ERROR: truncated track header");
		if (memcmp(p, "MTrk", 4) != 0)
			fatal("ERROR: expected MTrk chunk");
		p += 4;
		uint32_t tlen = read_u32(p);
		parse_track(p, tlen, t, events, duration, st);
	}

	// Merge tracks: sort by tick, then track index, then in-track order.
	st.input_events = (int)events.size() + st.meta_stripped + st.sysex_stripped;
	std::sort(events.begin(), events.end(), [](const midi_event_t &a, const midi_event_t &b) {
		if (a.tick != b.tick) return a.tick < b.tick;
		if (a.track != b.track) return a.track < b.track;
		return a.sequence < b.sequence;
	});

	// Build the uncompressed MID64 in a temp file, then wrap with asset compression.
	FILE *tmp = tmpfile();
	if (!tmp) fatal("ERROR: cannot create temporary file");

	wa(tmp, MID64_MAGIC, 4);
	w8(tmp, MID64_VERSION);
	w8(tmp, 0); // flags
	w16(tmp, division);
	w32(tmp, MID64_HEADER_SIZE); // events_offset
	int events_size_pos = w32_placeholder(tmp);
	int num_events_pos = w32_placeholder(tmp);
	w32(tmp, duration > 0xffffffffu ? 0xffffffffu : (uint32_t)duration);
	w32(tmp, 0); // reserved
	w32(tmp, 0);

	// Encode the merged stream: delta VLQ + event, with running status.
	uint64_t cur_tick = 0;
	uint8_t running = 0;
	uint32_t num_events = 0;
	long events_start = ftell(tmp);

	for (auto &ev : events) {
		uint64_t delta64 = ev.tick - cur_tick;
		if (delta64 > 0x0fffffffu)
			fatal("ERROR: delta tick too large for MIDI VLQ");
		write_vlq(tmp, (uint32_t)delta64);
		cur_tick = ev.tick;

		if (ev.type == midi_event_type::TEMPO) {
			w8(tmp, MID64_OP_SET_TEMPO);
			w8(tmp, (ev.tempo_us >> 16) & 0xff);
			w8(tmp, (ev.tempo_us >> 8) & 0xff);
			w8(tmp, ev.tempo_us & 0xff);
			running = 0;
		} else {
			if (ev.status != running) {
				w8(tmp, ev.status);
				running = ev.status;
			}
			w8(tmp, ev.data1);
			if (channel_data_len(ev.status) == 2)
				w8(tmp, ev.data2);
		}
		num_events++;
	}

	// Trailing END at the max EOT tick (keeps silence after the last note).
	uint64_t end_delta = duration - cur_tick;
	if (end_delta > 0x0fffffffu)
		fatal("ERROR: delta tick too large for MIDI VLQ");
	write_vlq(tmp, (uint32_t)end_delta);
	w8(tmp, MID64_OP_END);
	num_events++;

	long events_end = ftell(tmp);
	uint32_t events_size = (uint32_t)(events_end - events_start);
	w32_at(tmp, events_size_pos, events_size);
	w32_at(tmp, num_events_pos, num_events);

	std::vector<uint8_t> plain = slurp(tmp);
	fclose(tmp);

	FILE *out = fopen(outfn, "wb");
	if (!out) fatal("ERROR: cannot create: %s", outfn);
	// Large window: MID64 is always fully preloaded via asset_load().
	int cmp_size = asset_compress_mem(plain.data(), (int)plain.size(), out,
		flag_mid_compress, 256 * 1024, NULL);
	fclose(out);
	if (cmp_size < 0) fatal("ERROR: compression failed: %s", outfn);

	st.output_events = (int)num_events;
	st.events_size = events_size;
	st.total_size = cmp_size;

	if (flag_verbose) {
		fprintf(stderr, "Converting: %s => %s\n", infn, outfn);
		fprintf(stderr, "  MIDI format:           %d\n", st.format);
		fprintf(stderr, "  Tracks:                %d\n", st.tracks);
		fprintf(stderr, "  PPQN:                  %d\n", st.ppqn);
		fprintf(stderr, "  Input events:          %d\n", st.input_events);
		fprintf(stderr, "  Output events:         %d\n", st.output_events);
		fprintf(stderr, "  Tempo changes:         %d\n", st.tempo_changes);
		fprintf(stderr, "  SysEx stripped:        %d\n", st.sysex_stripped);
		fprintf(stderr, "  Meta events stripped:  %d\n", st.meta_stripped);
		fprintf(stderr, "  Input MIDI:            %.1f KiB\n", st.input_size / 1024.0);
		fprintf(stderr, "  MID64 uncompressed:    %.1f KiB\n", plain.size() / 1024.0);
		fprintf(stderr, "  MID64 compressed:      %.1f KiB (level %d)\n",
			st.total_size / 1024.0, flag_mid_compress);
	}

	return 0;
}
