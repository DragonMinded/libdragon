/*
    conv_xm64: convert XM files to XM64 format
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
/*
 * We convert XM files to XM64 format. XM64 is a dump of the internal datastructure
 * of libxm, similar to the official "libxmize" but portable across different
 * architectures (and endian friendly).
 *
 * On top of this, XM64 has also several benefits and pre-processing:
 *
 *  * Samples with ping-pong loops are unrolled to forward loops, as the RSP
 *    player does not support ping-pong looping.
 *  * Patterns are recompressed using a custom RLE algorithm. This helps reducing
 *    ROM size while still requiring negligible CPU time each time a new pattern
 *    is loaded (only the current pattern is kept in RAM at any given time).
 *    The decompression also requires no additional memory (RAM).
 *  * The module is analyzed to calculate the minimum amount of RAM to be allocated
 *    for each channel for streaming samples for ROMs. Each channel has a buffer
 *    that must contain enough samples for playing one "tick", so the exact
 *    size depends on the playing speed, sample pitch, etc. across the whole
 *    module.
 */
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include "mixer.h"
#include "samplebuffer.h"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "libxm.h"
#include "../common/crc32.c"
#include "../common/binout.h"
#include "../common/nanotime.h"
#include "../common/polyfill.h"
#include "../common/utils.h"
#include "../common/assetcomp.h"
#include "conv_common.h"

int flag_xm_compress_meta = DEFAULT_COMPRESSION;
int flag_xm_compress_samples = DEFAULT_COMPRESSION;
bool flag_xm_8bit = false;
const char *flag_xm_extsampledir = NULL;

std::map<xm_sample_t*, std::set<int>> sample_skip_points;
/** Per-sample VADPCM attack window (frames), keyed by sample CRC32. */
static std::map<uint32_t, uint8_t> sample_attack_by_hash;
/** Samples marked for full RDRAM preload (keyed by CRC32). */
static std::map<uint32_t, bool> sample_resident_by_hash;

// Loops made by an odd number of bytes and shorter than this length are
// duplicated to prevent frequency changes during playback. See below for more
// information.
#define XM64_SHORT_ODD_LOOP_LENGTH  1024

static uint32_t xm_sample_crc32(xm_sample_t *s)
{
	if (s->bits == 8)
		return crc32((uint8_t*)s->data8, s->length);
	else
		return crc32((uint8_t*)s->data16, s->length * 2);
} 

static void xm_save_wave64(xm_sample_t *s, FILE *out, const char *outfn)
{
	int16_t *samples16 = (int16_t*)malloc(s->length * sizeof(int16_t));
	if (s->bits == 8) {
		for (int k=0;k<s->length;k++)
			samples16[k] = (s->data8[k] << 8) | (uint8_t)s->data8[k];
	} else {
		memcpy(samples16, s->data16, s->length * sizeof(int16_t));
	}

	wav_data_t wav = {
		.samples = samples16,
		.cnt = s->length,
		.channels = 1,
		.bitsPerSample = s->bits,
		.sampleRate = 44100,
		.looping = s->loop_type != 0,
		.loopOffset = s->loop_start,
		.loopEnd = s->loop_type != 0 ? (int)s->loop_end : 0,
	};
	for (auto pos : sample_skip_points[s])
		wav.skipPoints.push_back(pos);
	wav.attack_frames = sample_attack_by_hash[xm_sample_crc32(s)];
	wav.resident = sample_resident_by_hash[xm_sample_crc32(s)];
	// Resident samples already live in RDRAM: no attack window.
	if (wav.resident)
		wav.attack_frames = 0;

	if (!wav64_write("xm", outfn, out, &wav, flag_xm_compress_samples))
		fatal("ERROR: failure while writing %s\n", outfn);

	free(wav.samples);

	if (wav.looping) {
		// Adjust loop information as they might have changed during compression
		int lend = wav.loopEnd ? wav.loopEnd : wav.cnt;
		s->loop_start = wav.loopOffset;
		s->loop_length = lend - wav.loopOffset;
		s->loop_end = lend;
		s->length = wav.cnt;
	}
}

static void xm_save_wave_internally(xm_context_t* ctx, FILE* meta, FILE* out, const char *outfn, int totsamples)
{
	// Use a sample structure to do internal de-duplication of samples
	struct sample_checksum {
		uint32_t hash;
		uint32_t pos;
		xm_sample_t *s;
	};
	std::vector<sample_checksum> wave_sums(totsamples+1);

	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			assert(s->bits == 8 || s->bits == 16);

			// Check if the sample has been already seen in this module
			uint32_t hash = xm_sample_crc32(s);
			int k = 0;
			while (wave_sums[k].pos > 0 && wave_sums[k].hash != hash) {
				k++;
			}
			if (wave_sums[k].pos == 0) {
				walign(out, 2);
				wave_sums[k].hash = hash;
				wave_sums[k].pos = ftell(out);
				wave_sums[k].s = s;

				char *wavfn = NULL; asprintf(&wavfn, "%s.%d.%d.wav64", outfn, i, j); // used only for --debug
				xm_save_wave64(s, out, wavfn);
				free(wavfn);
			} else {
				// Sample already seen. Make sure to update the sample
				// with information that might change during compression
				if (s->loop_type != 0) {
					s->length = wave_sums[k].s->length;
					s->loop_start = wave_sums[k].s->loop_start;
					s->loop_length = wave_sums[k].s->loop_length;
					s->loop_end = wave_sums[k].s->loop_end;
					s->loop_type = wave_sums[k].s->loop_type;
				}
			}

			placeholder_set_offset(meta, wave_sums[k].pos, "sample_%d_%d", i, j);
		}
	}
}

static void xm_save_wave_externally(xm_context_t* ctx, FILE *meta, FILE* out, const char *outfn, int totsamples)
{
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			assert(s->bits == 8 || s->bits == 16);

			// Rewrite the file atomically. Notice that the file might already
			// exist since it's a shared archive of wavs, but we want to rewrite
			// it anyway using our own settings (eg: compression). This gives a
			// better developer experience as changes in Makefile are effective even
			// with "make -B" or in general if any input file is changed.
			uint32_t hash = xm_sample_crc32(s);
			char *filename = NULL;
			asprintf(&filename, "%s/%08x.wav64", flag_xm_extsampledir, hash);

			// Create a temporary unique file name embedding a random number
			char *tmpname = NULL;
			asprintf(&tmpname, "%s/%x.%" PRIx64 ".tmp", flag_xm_extsampledir, getpid(), nanotime());

			int fd = open(tmpname, O_WRONLY | O_CREAT | O_EXCL, 0644);
			if (fd < 0) {
				fprintf(stderr, "FATAL: cannot create %s: %s\n", filename, strerror(errno));
				exit(1);
			}
			
			// Write the file
			FILE *wavf = fdopen(fd, "wb");
			if (!wavf) fatal("ERROR: cannot create %s: %s\n", filename, strerror(errno));
			char *wavfn=NULL; asprintf(&wavfn, "%s.%d.%d.wav64", outfn, i, j); // used only for --debug

			xm_save_wave64(s, wavf, wavfn);

			fclose(wavf);
			free(wavfn);
			rename(tmpname, filename);

			// Save the WAV hash in the XM64 file as "offset" of the waveform
			placeholder_set_offset(meta, hash, "sample_%d_%d", i, j);
			free(filename);
			free(tmpname);
		}
	}
}


static void xm_context_save(xm_context_t* ctx, FILE* xm64, const char *outfn) {
	// Version log:
	//  5: first public version
	//  6: added overread for non-looping samples. The size of optimal
	//     stream sample buffer size must change, hance the version bump.
	//  7: switch to wav64 for samples, and add support for external samples
	//  8: patterns are compressed with asset library
	//  9: metadata compressed with asset library
	// 10: added sample position memory to xm_channel_context_t
	// 11: change sample_position to double
	// 12: extra sample-buffer headroom for syncless mixer in-flight rounds
	// 13: sample-buffer sizing as a rate, scaled at playback by the queue depth
	const uint8_t version = 13;
	wa(xm64, "XM64", 4);
	w8(xm64, version);
	w32_placeholderf(xm64, "metadata_offset");
	w32_placeholderf(xm64, "metadata_size");

	// Write metadata into a temporary file
	FILE *meta = tmpfile();
	assert(meta && "Temporary file creation failed");

	int totsamples = 0;
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		totsamples += ins->num_samples;
	}

	// Write the samples (either internally or externally). We do this before
	// writing the metadata as the process might cause change some samples
	// (eg for compression requirements).
	if (flag_xm_extsampledir)
		xm_save_wave_externally(ctx, meta, xm64, outfn, totsamples);
	else
		xm_save_wave_internally(ctx, meta, xm64, outfn, totsamples);

	w32(meta, ctx->ctx_size);
	w32(meta, ctx->ctx_size_all_patterns);
	w32(meta, ctx->ctx_size_all_samples);
	w32_placeholderf(meta, "ctx_size_stream_pattern_buf");
	for (int i=0; i<32; i++) w32(meta, ctx->ctx_stream_buf_rate[i]);
	for (int i=0; i<32; i++) w32(meta, ctx->ctx_stream_buf_base[i]);
	for (int i=0; i<32; i++) w32(meta, ctx->ctx_stream_buf_min[i]);
	for (int i=0; i<32; i++) w32(meta, ctx->ctx_stream_buf_cap[i]);

	w16(meta, ctx->module.tempo);
	w16(meta, ctx->module.bpm);

#if XM_STRINGS
	wa(meta, ctx->module.name, sizeof(ctx->module.name));
	wa(meta, ctx->module.trackername, sizeof(ctx->module.trackername));
#else
	char name[MODULE_NAME_LENGTH+1] = {0}; char trackername[TRACKER_NAME_LENGTH+1] = {0};
	wa(meta, name, sizeof(name)); wa(meta, trackername, sizeof(trackername));
#endif
	w16(meta, ctx->module.length);
	w16(meta, ctx->module.restart_position);
	w16(meta, ctx->module.num_channels);
	w16(meta, ctx->module.num_patterns);
	w16(meta, ctx->module.num_instruments);
	w32(meta, ctx->module.frequency_type);
	wa(meta, ctx->module.pattern_table, sizeof(ctx->module.pattern_table));

	for (int i=0;i<ctx->module.num_patterns;i++) {
		w16(meta, ctx->module.patterns[i].num_rows);
		w32_placeholderf(meta, "pattern_%d", i);
		w16_placeholderf(meta, "pattern_size_%d", i);
	}

	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
#if XM_STRINGS
		wa(meta, ins->name, sizeof(ins->name));
#else
		char name[INSTRUMENT_NAME_LENGTH + 1] = {0};
		wa(meta, name);
#endif
		wa(meta, ins->sample_of_notes, sizeof(ins->sample_of_notes));

		w8(meta, ins->volume_envelope.num_points);
		for (int j=0;j<ins->volume_envelope.num_points;j++) {
			w16(meta, ins->volume_envelope.points[j].frame);
			w16(meta, ins->volume_envelope.points[j].value);
		}
		w8(meta, ins->volume_envelope.sustain_point);
		w8(meta, ins->volume_envelope.loop_start_point);
		w8(meta, ins->volume_envelope.loop_end_point);
		w8(meta, ins->volume_envelope.enabled);
		w8(meta, ins->volume_envelope.sustain_enabled);
		w8(meta, ins->volume_envelope.loop_enabled);

		w8(meta, ins->panning_envelope.num_points);
		for (int j=0;j<ins->panning_envelope.num_points;j++) {
			w16(meta, ins->panning_envelope.points[j].frame);
			w16(meta, ins->panning_envelope.points[j].value);
		}
		w8(meta, ins->panning_envelope.sustain_point);
		w8(meta, ins->panning_envelope.loop_start_point);
		w8(meta, ins->panning_envelope.loop_end_point);
		w8(meta, ins->panning_envelope.enabled);
		w8(meta, ins->panning_envelope.sustain_enabled);
		w8(meta, ins->panning_envelope.loop_enabled);

		w32(meta, ins->vibrato_type);
		w8(meta, ins->vibrato_sweep);
		w8(meta, ins->vibrato_depth);
		w8(meta, ins->vibrato_rate);
		w16(meta, ins->volume_fadeout);
		w64(meta, ins->latest_trigger);

		w16(meta, ins->num_samples);
		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			// NOTE: use original bitsize here (even if VADPCM is always 16-bit)
			// This is useful at least for 0x9 command (set sample offset) that
			// requires to know the original bitsize of the sample.
			// The WAV64 will be marked as 16-bit with VADPCM instead, so that
			// playback will be correct.
			w8(meta, s->bits); 
			w32(meta, s->length);
			w32(meta, s->loop_start);
			w32(meta, s->loop_length);
			w32(meta, s->loop_end);
			wf32(meta, s->volume);
			w8(meta, s->finetune);
			w32(meta, s->loop_type);
			wf32(meta, s->panning);
			w8(meta, s->relative_note);
			w32_placeholderf(meta, "sample_%d_%d", i, j);
		}
	}
	w8(meta, (flag_xm_extsampledir != NULL));

	// Write the patterns (potentially compressed)
	int max_inplace_margin = 0;
	for (int i=0;i<ctx->module.num_patterns;i++) {
		walign(xm64, 2);
		int pos = ftell(xm64);

		xm_pattern_t *p = &ctx->module.patterns[i];

		int pat_size = p->num_rows*ctx->module.num_channels*5;
		std::vector<uint8_t> cur_pat(pat_size);
		uint8_t *pp = &cur_pat[0];

		xm_pattern_slot_t *s = &p->slots[0];
		for (int k=0;k<ctx->module.num_channels;k++) {
			for (int j=0;j<p->num_rows;j++) {
				*pp++ = s->note;
				*pp++ = s->instrument;
				*pp++ = s->volume_column;
				*pp++ = s->effect_type;
				*pp++ = s->effect_param;
				s++;
			}
		}

		int inplace_margin;
		asset_compress_mem(&cur_pat[0], pat_size, xm64, flag_xm_compress_meta, 0, &inplace_margin);
		if (inplace_margin > max_inplace_margin)
			max_inplace_margin = inplace_margin;
		placeholder_set_offset(meta, pos, "pattern_%d", i);
		placeholder_set_offset(meta, ftell(xm64) - pos, "pattern_size_%d", i);
	}

	// Add the necessary maximum margin to the size of the pattern buffer
	// See the code in asset_buf_size() for more information.
	ctx->ctx_size_stream_pattern_buf += max_inplace_margin;
	ctx->ctx_size_stream_pattern_buf += 4;  // max alignment for 32-bit loads (for shrinkler)
	ctx->ctx_size_stream_pattern_buf += 8;  // margin for OOB writes of decompressors
	ctx->ctx_size_stream_pattern_buf = (ctx->ctx_size_stream_pattern_buf + 15) / 16 * 16;
	placeholder_set_offset(meta, ctx->ctx_size_stream_pattern_buf, "ctx_size_stream_pattern_buf");

	// Now we have completed the file. Read back the metadata and compress them
	// to the xm64 output file.
	std::vector<uint8_t> metadata = slurp(meta);
	fclose(meta);
	walign(xm64, 2);
	placeholder_set(xm64, "metadata_offset");
	placeholder_set_offset(xm64, metadata.size(), "metadata_size");

	asset_compress_mem(metadata.data(), metadata.size(), xm64, flag_xm_compress_meta, 0, NULL);
}

static void xm_remove_empty_samples(xm_context_t *ctx)
{
	// Some instruments may have empty samples (length=0). Remove them and
	// remap the sample numbers in the instrument.
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		std::vector<int> sample_remap(ins->num_samples);

		int j = 0;
		for (int k=0;k<ins->num_samples;k++) {
			if (ins->samples[k].length > 0) {
				sample_remap[k] = j;
				if (j != k) ins->samples[j] = ins->samples[k];
				j++;
			} else {
				sample_remap[k] = -1;
			}
		}

		// Update sample_of_notes
		for (int k=0;k<NUM_NOTES;k++) {
			if (ins->sample_of_notes[k] < ins->num_samples)
				ins->sample_of_notes[k] = sample_remap[ins->sample_of_notes[k]];
		}

		// Update total count of samples
		ins->num_samples = j;
	}
}



/** Bytes needed to LOOP_CACHE a loop of @p L samples in the channel buffer. */
static int xm_loop_pin_bytes(int L)
{
	if (flag_xm_compress_samples > 0) {
		int fbytes = vadpcm_frame_bytes(flag_wav_compress_vadpcm_bits);
		int nframes = (L + 15) / 16;
		nframes += (MIXER_LOOP_OVERREAD + fbytes - 1) / fbytes + 1;
		return nframes * fbytes;
	}
	return L * 2 + MIXER_LOOP_OVERREAD + 2;
}

/** RDRAM cost of making @p s fully resident (VADPCM body or PCM). */
static int xm_sample_resident_bytes(xm_sample_t *s)
{
	if (flag_xm_compress_samples > 0)
		return (int)(((s->length + 15) / 16) * vadpcm_frame_bytes(flag_wav_compress_vadpcm_bits));
	return (int)(s->length * (s->bits / 8));
}

/**
 * Per-channel samplebuffer sizing.
 *
 * The ring has to span everything the RSP may still have to read, which is
 * what the mixer lets the CPU enqueue before waiting for it: that span is
 * only known at playback time, so it is kept as a rate here and turned into
 * bytes by #xm64player_play.
 */
struct ch_sizing_t {
	double rate;   ///< Stream bytes consumed per second of playback, at the top pitch
	int base;      ///< Bytes that do not scale with the queue (prefetch, overread)
	int cap;       ///< Bytes of the longest sample: nothing on the channel needs more
	int pin;       ///< Floor to hold a pinned loop, 0 if none
};

/** Reference queue depth for the pin budget: two AI buffers at 44100 Hz,
 *  which is what the mixer lets the CPU run ahead (MIXER_POLL_LOOKAHEAD). */
#define XM_REF_INFLIGHT_SAMPLES  3528
#define XM_REF_OUTPUT_RATE       44100

/**
 * Bytes this sizing asks for when the CPU can run @p inflight output samples
 * ahead, mixing at @p out_rate. The stream is consumed in wall-clock time, so
 * the output rate of the game is what turns that queue into a duration.
 */
static int xm_sizing_bytes(const ch_sizing_t &s, int inflight, int out_rate)
{
	if (s.rate <= 0 && !s.pin)
		return 0;
	int n = s.base + (int)ceil(s.rate * inflight / out_rate);
	if (s.cap && n > s.cap) n = s.cap;
	if (n < s.pin) n = s.pin;
	return (n + 7) / 8 * 8;
}

/**
 * Dry-run the whole module (every pattern order) to collect sizing inputs:
 * per-channel streaming rates (@p ch_sz), max pitch of each looping sample
 * on each channel (@p ch_loop_freq), note-ons from offset 0 and their same-tick
 * burst size, 9xx skip points, and which instrument samples are used.
 */
static void xm_dry_run(xm_context_t *ctx, ch_sizing_t ch_sz[32],
	std::map<xm_sample_t*, float> ch_loop_freq[32],
	std::map<xm_sample_t*, int> &noteons_from0,
	std::map<xm_sample_t*, int> &cold_burst,
	bool **used_samples)
{
	int num_orders = xm_get_module_length(ctx);
	bool played_orders[PATTERN_ORDER_TABLE_LENGTH] = {0};

	while (1) {
		do {
			xm_tick(ctx);

			uint8_t pat_idx;
			xm_get_position(ctx, &pat_idx, NULL, NULL, NULL);
			played_orders[pat_idx] = true;

			int nsamples = ceilf(ctx->remaining_samples_in_tick);
			xm_sample_t *cold[32];
			int ncold = 0;
			for (int i = 0; i < ctx->module.num_channels; ++i) {
				xm_channel_context_t *ch = &ctx->channels[i];
				if (!ch->instrument || !ch->sample)
					continue;

				int ins_idx = ch->instrument - ctx->module.instruments;
				int smp_idx = ch->sample - ch->instrument->samples;
				bool *used_samp_inst = used_samples[ins_idx];
				if (smp_idx >= 0 && smp_idx < ch->instrument->num_samples)
					used_samp_inst[smp_idx] = true;

				// Bytes of stream per output sample at this pitch, and the
				// bytes on top of that which the queue depth does not change:
				// what #samplebuffer_prefetch keeps ready, the RSP overread
				// past a loop, and the framing slack of a block codec.
				double bps;      // bytes per source sample
				int ub, extra;   // samplebuffer unit, fixed bytes
				if (flag_xm_compress_samples > 0) {
					int fbytes = vadpcm_frame_bytes(flag_wav_compress_vadpcm_bits);
					ub = fbytes;
					bps = fbytes / 16.0;
					int nframes = (MIXER_LOOP_OVERREAD + fbytes - 1) / fbytes + 2;
					if (ch->sample->loop_type)
						nframes += 2;
					extra = nframes * fbytes;
				} else {
					ub = ch->sample->bits == 16 ? 2 : 1;
					bps = ub;
					extra = MIXER_LOOP_OVERREAD;
				}
				extra += SAMPLEBUFFER_MARGIN_UNITS * ub;

				ch_sizing_t &sz = ch_sz[i];
				double rate = ch->step * bps * ctx->rate;
				if (sz.rate < rate) sz.rate = rate;
				if (sz.base < extra) sz.base = extra;
				int cap = (int)ceil(ch->sample->length * bps) + extra;
				if (sz.cap < cap) sz.cap = cap;

				if (ch->sample->loop_type && ch->sample->loop_length > 0) {
					float &f = ch_loop_freq[i][ch->sample];
					if (ch->frequency > f)
						f = ch->frequency;
				}

				bool key_on = ch->current->note > 0 && ch->current->note < 97;
				if (key_on && ch->current->effect_type == 0x9)
					sample_skip_points[ch->sample].insert(ch->sample_position);
				// Tick 0 of the row, no 9xx (or 9xx=0): cold PI from sample start.
				if (key_on && ctx->current_tick == 0
					&& (ch->current->effect_type != 0x9 || ch->current->effect_param == 0))
					cold[ncold++] = ch->sample;
			}
			for (int k = 0; k < ncold; k++) {
				noteons_from0[cold[k]]++;
				if (cold_burst[cold[k]] < ncold)
					cold_burst[cold[k]] = ncold;
			}
			ctx->remaining_samples_in_tick -= nsamples;
		} while (xm_get_loop_count(ctx) == 0);

		// Force playback of every non-empty pattern order (multi sub-song XMs).
		bool fully_played = true;
		for (int i = 0; i < num_orders; i++) {
			if (!played_orders[i]) {
				if (flag_verbose)
					fprintf(stderr, "  * found potential sub-song starting at pattern index: %d\n", i);
				xm_seek(ctx, i, 0, 0);
				fully_played = false;
				break;
			}
		}
		if (fully_played)
			break;
	}
}

/**
 * Size each channel's samplebuffer for streaming, then optionally grow it to
 * LOOP_CACHE a loop (or mark the sample resident).
 *
 * A loop is worth pinning on a channel when the extra RAM is repaid by about
 * one second of avoided PI traffic at that pitch:
 *   pin_sz - stream_sz  <=  freq · bps
 * Affordable pin candidates (budget ≤ streaming footprint) are ranked by
 * (channels × pitch / loop_len / pin_extra). When a candidate's turn comes,
 * prefer one resident copy of the body if that is cheaper than growing the
 * channel buffers — never against a pin that would not fit the budget.
 *
 * Writes ctx->ctx_stream_buf_*[] and sample_resident_by_hash.
 * @return total samplebuffer bytes; *@p res_bytes is resident RDRAM.
 */
static int xm_size_loop_buffers(xm_context_t *ctx, ch_sizing_t ch_sz[32],
	std::map<xm_sample_t*, float> ch_loop_freq[32], int *res_bytes_out)
{
	const float PIN_LOOP_HORIZON_SEC = 1.0f;

	// Baseline streaming size per channel, at a reference queue depth: the
	// real one is a playback-time choice, but the pin decisions below only
	// need the two costs to be comparable. Cap the pin budget at this total
	// so pin RAM cannot exceed what streaming already spends.
	int stream_sz[32] = {0};
	int stream_total = 0;
	for (int i = 0; i < ctx->module.num_channels; i++) {
		ch_sz[i].rate *= 1.05;
		stream_sz[i] = xm_sizing_bytes(ch_sz[i], XM_REF_INFLIGHT_SAMPLES, XM_REF_OUTPUT_RATE);
		stream_total += stream_sz[i];
	}

	float bps = flag_xm_compress_samples > 0
		? vadpcm_frame_bytes(flag_wav_compress_vadpcm_bits) / 16.0f : 2.0f;

	// Aggregate identical looping samples (same CRC) across channels: one
	// body may play on several channels, so pin/resident decisions must be
	// per unique waveform, not per (channel, sample) pair.
	struct loop_info {
		xm_sample_t *s;
		std::vector<int> chs;
		float freq_ch[32];
		int psz, pin_extra;
		float score;
	};
	std::map<uint32_t, loop_info> by_hash;
	for (int i = 0; i < ctx->module.num_channels; i++) {
		for (auto const &kv : ch_loop_freq[i]) {
			uint32_t h = xm_sample_crc32(kv.first);
			auto &info = by_hash[h];
			if (!info.s) {
				info.s = kv.first;
				memset(info.freq_ch, 0, sizeof(info.freq_ch));
			}
			info.chs.push_back(i);
			if (kv.second > info.freq_ch[i])
				info.freq_ch[i] = kv.second;
		}
	}

	// Keep only loops worth pinning: on at least one channel, the extra RAM
	// to hold the full loop is repaid by ~1s of PI traffic at that pitch.
	// Score favors short hot loops shared by many channels (PI win / cost).
	std::vector<std::pair<float, uint32_t>> pin_cands;
	for (auto &kv : by_hash) {
		loop_info &info = kv.second;
		xm_sample_t *s = info.s;
		int L = s->loop_length;
		if (L <= 0) continue;
		info.psz = xm_loop_pin_bytes(L);
		info.pin_extra = 0;
		bool worth = false;
		float max_freq = 0;
		for (int ch : info.chs) {
			int extra = info.psz > stream_sz[ch] ? info.psz - stream_sz[ch] : 0;
			info.pin_extra += extra;
			float freq = info.freq_ch[ch];
			if (freq > max_freq) max_freq = freq;
			if (extra > 0 && (double)extra <= (double)PIN_LOOP_HORIZON_SEC * (double)freq * (double)bps)
				worth = true;
		}
		if (!worth || info.pin_extra <= 0)
			continue;
		info.score = (float)info.chs.size() * max_freq / (float)L / (float)info.pin_extra;
		pin_cands.push_back({ info.score, kv.first });
	}
	std::sort(pin_cands.begin(), pin_cands.end(),
		[](const std::pair<float, uint32_t> &a, const std::pair<float, uint32_t> &b) {
			return a.first > b.first;
		});

	// Spend the pin budget in score order. For each affordable candidate,
	// prefer a single resident copy of the body when cheaper than growing
	// every channel's samplebuffer; otherwise grow those buffers (LOOP_CACHE).
	// Candidates that do not fit the remaining budget are left streaming.
	int grown[32];
	memcpy(grown, stream_sz, sizeof(grown));
	int pin_budget = stream_total;
	int pin_spent = 0;
	int res_n = 0, res_bytes = 0;
	for (auto const &pc : pin_cands) {
		loop_info &info = by_hash[pc.second];
		int cost = 0;
		for (int ch : info.chs)
			if (info.psz > grown[ch])
				cost += info.psz - grown[ch];
		if (cost <= 0 || cost > pin_budget - pin_spent)
			continue;
		int rcost = xm_sample_resident_bytes(info.s);
		if (rcost <= cost) {
			sample_resident_by_hash[pc.second] = true;
			res_n++;
			res_bytes += rcost;
			if (flag_verbose)
				fprintf(stderr, "  * resident loop=%d len=%d: %d B once < %d B pin (%d ch)\n",
					info.s->loop_length, (int)info.s->length, rcost, cost, (int)info.chs.size());
			continue;
		}
		pin_spent += cost;
		for (int ch : info.chs)
			if (info.psz > grown[ch])
				grown[ch] = info.psz;
		if (flag_verbose)
			fprintf(stderr, "  * pin loop=%d: +%d B across %d ch (score=%.3g)\n",
				info.s->loop_length, cost, (int)info.chs.size(), info.score);
	}

	// Commit the sizing into the XM context. A channel grown to hold a pinned
	// loop carries that as a floor; the rest is recomputed at playback time
	// from the queue depth in use.
	int sam_size = 0;
	for (int i = 0; i < ctx->module.num_channels; i++) {
		if (grown[i] > stream_sz[i])
			ch_sz[i].pin = ((grown[i] + 7) / 8) * 8;
		ctx->ctx_stream_buf_rate[i] = (uint32_t)ceil(ch_sz[i].rate);
		ctx->ctx_stream_buf_base[i] = ch_sz[i].base;
		ctx->ctx_stream_buf_min[i] = ch_sz[i].pin;
		ctx->ctx_stream_buf_cap[i] = ch_sz[i].cap;
		sam_size += xm_sizing_bytes(ch_sz[i], XM_REF_INFLIGHT_SAMPLES, XM_REF_OUTPUT_RATE);
	}
	if (flag_verbose) {
		if (res_n)
			fprintf(stderr, "  * resident: %d samples → %d KiB\n", res_n, (res_bytes + 1023) / 1024);
		fprintf(stderr, "  * loop pin: +%d KiB (budget %d KiB)\n",
			(pin_spent + 1023) / 1024, (pin_budget + 1023) / 1024);
	}
	*res_bytes_out = res_bytes;
	return sam_size;
}

/**
 * Prefetch the first SAMPLEBUFFER_MARGIN VADPCM frames of selected samples
 * into RDRAM (attack cache). A note that starts at sample offset 0 must DMA
 * those frames from ROM before it can play; when several such note-ons land
 * on the same tick they queue on the PI bus. Keep a small head of the worst
 * offenders resident so those note-ons hit RDRAM instead.
 *
 * Budget is 1/8 of the samplebuffer RAM (@p sam_size), spent on samples with
 * the highest (note-on count × same-tick burst) score. Results go into
 * sample_attack_by_hash.
 */
static void xm_select_attack_cache(
	const std::map<xm_sample_t*, int> &noteons_from0,
	const std::map<xm_sample_t*, int> &cold_burst,
	int sam_size)
{
	const int ATTACK_FRAMES = 128; // matches SAMPLEBUFFER_MARGIN_UNITS
	int fbytes = flag_xm_compress_samples > 0
		? vadpcm_frame_bytes(flag_wav_compress_vadpcm_bits) : 0;
	if (!fbytes)
		return;

	int attack_budget = sam_size / 8;
	struct att_cand { int score; xm_sample_t *s; };
	std::vector<att_cand> cands;
	for (auto const &kv : noteons_from0)
		cands.push_back({ kv.second * cold_burst.at(kv.first), kv.first });
	std::sort(cands.begin(), cands.end(), [](att_cand a, att_cand b) {
		return a.score > b.score;
	});

	int attack_n = 0, attack_bytes = 0;
	int budget = attack_budget;
	int cost = ATTACK_FRAMES * fbytes;
	for (auto const &c : cands) {
		if (budget < cost) break;
		uint32_t hash = xm_sample_crc32(c.s);
		if (sample_attack_by_hash[hash] >= ATTACK_FRAMES) continue;
		sample_attack_by_hash[hash] = ATTACK_FRAMES;
		budget -= cost;
		attack_n++;
		attack_bytes += cost;
	}
	if (flag_verbose)
		fprintf(stderr, "  * attack cache: %d/%d samples → %d KiB (budget %d KiB = 1/8 of %d KiB samplebufs)\n",
			attack_n, (int)noteons_from0.size(),
			(attack_bytes + 1023) / 1024, (attack_budget + 1023) / 1024,
			sam_size / 1024);
}

int xm_convert(const char *infn, const char *outfn) {
	if (flag_verbose)
		fprintf(stderr, "Converting: %s => %s\n", infn, outfn);

	// For xm64 conversions, deactivate huffman by default
	if (flag_wav_compress_vadpcm_huffman < 0) 
		flag_wav_compress_vadpcm_huffman = 0;

	FILE *xm = fopen(infn, "rb");
	if (!xm) fatal("cannot open: %s\n", infn);

	fseek(xm, 0, SEEK_END);
	int fsize = ftell(xm);
	fseek(xm, 0, SEEK_SET);

	char *xmdata = (char*)malloc(fsize);
	fread(xmdata, 1, fsize, xm);

	size_t mem_ctx, mem_pat, mem_sam;
	xm_get_memory_needed_for_context(xmdata, fsize, &mem_ctx, &mem_pat, &mem_sam);

	// Load the XM into a XM context. The specified playback frequency is
	// arbitrary, and it doesn't affect the calculations being done of the buffer
	// sizes (as those depend on the instrument notes, not the output frequency).
	xm_context_t* ctx;
	xm_create_context_safe(&ctx, xmdata, fsize, 48000);
	if (!ctx) fatal("cannot read XM file: invalid format?");
	free(xmdata);

	// Remove 0-length samples
	xm_remove_empty_samples(ctx);

	// Pre-process all waveforms:
	//   1) Ping-pong loops will be unrolled as regular forward
	//   2) Repeat initial data after loop end for MIXER_LOOP_OVERREAD bytes
	//      to speed up decoding in RSP.
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];

		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			int bps = s->bits / 8;

			if (flag_xm_8bit && bps == 2) {
				// Convert 16-bit samples to 8-bit
				int8_t *data8 = (int8_t*)malloc(s->length);
				for (int k=0;k<s->length;k++)
					data8[k] = s->data16[k] >> 8;
				memcpy(s->data8, data8, s->length);
				free(data8);
				s->bits = 8;
				bps = 1;
			}

			uint32_t length = s->length * bps;
			uint32_t loop_length = s->loop_length * bps;
			uint32_t loop_end = s->loop_end * bps;

			uint8_t *sout, *out;
			switch (s->loop_type) {
			default:
				fatal("invalid loop type: %d\n", s->loop_type);
			case XM_NO_LOOP:
				sout = (uint8_t*)malloc(length);
				memcpy(sout, s->data8, length);
				break;
			case XM_FORWARD_LOOP:
				// Special case for odd-sized loops of 8-bit samples. We cannot
				// properly handle these at runtime because they cannot be DMA'd
				// as they change the 2-byte phase between ROM and RAM. 
				// xm64.c will decrease the loop length by 1 byte to playback them,
				// but this can affect the period in case of short loops:
				// for instance, a 13-bytes loop shortened 12-bytes change the
				// period by 7%, which can be several notes of difference at
				// high frequencies.
				// So for short loops (<1024 bytes), we just duplicate the loop
				// itself to make it of even size. For longer loops, the period
				// error made by xm64 when shortening is < 0.1%, which isn't
				// audible.
				if (bps == 1 && loop_length%2 == 1 && loop_length < XM64_SHORT_ODD_LOOP_LENGTH) {
					sout = (uint8_t*)malloc(loop_end + loop_length);
					length = loop_end+loop_length;
					// Copy waveform until loop end
					memcpy(sout, s->data8, loop_end);
					// Duplicate loop
					memmove(sout + loop_end, s->data8 + loop_end - loop_length, loop_length);
					loop_end += loop_length;
					loop_length *= 2;
				} else {				
					sout = (uint8_t*)malloc(loop_end);
					length = loop_end;
					// Copy waveform until loop end
					memcpy(sout, s->data8, loop_end);
				}
				break;
			case XM_PING_PONG_LOOP:
				length = loop_end + loop_length;
				sout = (uint8_t*)malloc(length);
				out = sout;

				memcpy(out, s->data8, loop_end);
				out += loop_end;
				for (int x=0;x<loop_length;x++)
					*out++ = s->data8[(loop_end-x-1) ^ (bps>>1)];

				loop_end += loop_length;
				loop_length *= 2;
				s->loop_type = XM_FORWARD_LOOP;
				break;
			}

			// If the sample length changed, update the memory
			// required for the context.
			if (length != s->length*bps)
			{
				#define ALIGN8(n)  ((((n) + 7) >> 3) << 3)
				ctx->ctx_size             -= ALIGN8(s->length*bps);
				ctx->ctx_size_all_samples -= ALIGN8(s->length*bps);
				ctx->ctx_size             += ALIGN8(length);
				ctx->ctx_size_all_samples += ALIGN8(length);
			}
			s->length = length / bps;
			s->loop_length = loop_length / bps;
			s->loop_end = loop_end / bps;
			s->data8 = (int8_t*)sout;
		}
	}

	// Analyze playback, then size streaming / pin / attack RAM.
	ch_sizing_t ch_sz[32] = {};
	std::map<xm_sample_t*, float> ch_loop_freq[32];
	std::map<xm_sample_t*, int> sample_noteons_from0;
	std::map<xm_sample_t*, int> sample_cold_burst;

	bool** used_samples = (bool**)calloc(ctx->module.num_instruments, sizeof(bool*));
	for (int i=0; i<ctx->module.num_instruments; i++)
		if (ctx->module.instruments[i].num_samples > 0)
			used_samples[i] = (bool*)calloc(ctx->module.instruments[i].num_samples, sizeof(bool));

	sample_skip_points.clear();
	sample_attack_by_hash.clear();
	sample_resident_by_hash.clear();

	xm_dry_run(ctx, ch_sz, ch_loop_freq, sample_noteons_from0, sample_cold_burst, used_samples);

	int res_bytes = 0;
	int sam_size = xm_size_loop_buffers(ctx, ch_sz, ch_loop_freq, &res_bytes);
	xm_select_attack_cache(sample_noteons_from0, sample_cold_burst, sam_size);

	// Free unused samples, to save ROM space. We only remove the last unused samples
	// to avoid renumbering the samples, which would require updating all the pattern
	// data. This is OK most of the times since 99% of XM files only has 1 sample per
	// instrument anyway.
	for (int i=0; i<ctx->module.num_instruments; i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		while (ins->num_samples > 0 && !used_samples[i][ins->num_samples-1]) {
			if (flag_verbose) fprintf(stderr, "  * removing unused sample %d from instrument %d\n", ins->num_samples-1, i+1);
			free(ins->samples[ins->num_samples-1].data8);
			ins->samples[ins->num_samples-1].data8 = NULL;
			memset(&ins->samples[ins->num_samples-1], 0, sizeof(xm_sample_t));
			ins->num_samples--;
		}
		free(used_samples[i]);
	}
	free(used_samples);

	FILE *out = fopen(outfn, "wb");
	if (!out) fatal("cannot create: %s", outfn);
	xm_context_save(ctx, out, outfn);
	int romsize = ftell(out);
	fclose(out);

	// Dump some statistics for the conversion
	if (flag_verbose) {
		fprintf(stderr, "  * ROM size: %u KiB (samples:%zu)\n",
			romsize / 1024, mem_sam / 1024);
		fprintf(stderr, "  * RAM size: %zu KiB (ctx:%zu, patterns:%u, samples:%u, resident:%u)\n",
			(mem_ctx+sam_size+res_bytes+ctx->ctx_size_stream_pattern_buf)/1024,
			mem_ctx / 1024,
			ctx->ctx_size_stream_pattern_buf / 1024,
			sam_size / 1024,
			res_bytes / 1024
		);
		fprintf(stderr, "  * Samples RAM per channel (at %d queued samples): [", XM_REF_INFLIGHT_SAMPLES);
		for (int i=0;i<ctx->module.num_channels;i++) {
			if (i!=0) fprintf(stderr, ", ");
			fprintf(stderr, "%d", xm_sizing_bytes(ch_sz[i], XM_REF_INFLIGHT_SAMPLES, XM_REF_OUTPUT_RATE));
		}
		fprintf(stderr, "]\n");
		// Only the queued part follows the audio buffer chain of the game, so
		// break the sizing down: it is what an app trades away by polling in
		// smaller steps.
		for (int i=0;i<ctx->module.num_channels;i++)
			fprintf(stderr, "    ch%-2d %6d B = %d fixed + %.2f B/sample x %d%s%s\n",
				i, xm_sizing_bytes(ch_sz[i], XM_REF_INFLIGHT_SAMPLES, XM_REF_OUTPUT_RATE),
				ch_sz[i].base, ch_sz[i].rate / XM_REF_OUTPUT_RATE, XM_REF_INFLIGHT_SAMPLES,
				ch_sz[i].pin ? " [loop pin]" : "",
				ch_sz[i].cap && ch_sz[i].base + ch_sz[i].rate * XM_REF_INFLIGHT_SAMPLES / XM_REF_OUTPUT_RATE > ch_sz[i].cap
					? " [capped by sample length]" : "");
	}

	return 0;
}
