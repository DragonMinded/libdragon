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
#include <map>
#include <set>

#include "libxm.h"
#include "../common/crc32.c"
#include "../common/binout.h"
#include "../common/nanotime.h"
#include "../common/polyfill.h"
#include "../common/assetcomp.h"
#include "conv_common.h"

int flag_xm_compress_meta = DEFAULT_COMPRESSION;
int flag_xm_compress_samples = DEFAULT_COMPRESSION;
bool flag_xm_8bit = false;
const char *flag_xm_extsampledir = NULL;

std::map<xm_sample_t*, std::set<int>> sample_skip_points;

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
	};
	for (auto pos : sample_skip_points[s])
		wav.skipPoints.push_back(pos);

	if (!wav64_write("xm", outfn, out, &wav, flag_xm_compress_samples))
		fatal("ERROR: failure while writing %s\n", outfn);

	free(wav.samples);

	if (wav.looping) {
		// Adjust loop information as they might have changed during compression
		s->loop_start = wav.loopOffset;
		s->loop_length = wav.cnt - wav.loopOffset;
		s->loop_end = wav.cnt;
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
	struct sample_checksum wave_sums[totsamples+1];
	memset(wave_sums, 0, sizeof(wave_sums));

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
	const uint8_t version = 11;
	wa(xm64, "XM64", 4);
	w8(xm64, version);
	w32_placeholderf(xm64, "metadata_offset");
	w32_placeholderf(xm64, "metadata_size");

	// Write metadata into a temporary file
	FILE *meta = tmpfile();

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
	for (int i=0; i<32; i++) w32(meta, ctx->ctx_size_stream_sample_buf[i]);

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
		uint8_t cur_pat[pat_size];
		uint8_t *pp = cur_pat;

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
		asset_compress_mem(cur_pat, pat_size, xm64, flag_xm_compress_meta, 0, &inplace_margin);
		if (inplace_margin > max_inplace_margin)
			max_inplace_margin = inplace_margin;
		placeholder_set_offset(meta, pos, "pattern_%d", i);
		placeholder_set_offset(meta, ftell(xm64) - pos, "pattern_size_%d", i);
	}

	// Add the necessary maximum margin to the size of the pattern buffer
	// See the code in asset_buf_size() for more information.
	ctx->ctx_size_stream_pattern_buf += max_inplace_margin;
	ctx->ctx_size_stream_pattern_buf += 8;  // margin for OOB writes of decompressors
	ctx->ctx_size_stream_pattern_buf = (ctx->ctx_size_stream_pattern_buf + 15) / 16 * 16;
	placeholder_set_offset(meta, ctx->ctx_size_stream_pattern_buf, "ctx_size_stream_pattern_buf");

	// Now we have completed the file. Read back the metadata and compress them
	// to the xm64 output file.
	int metadata_size = ftell(meta);
	walign(xm64, 2);
	placeholder_set(xm64, "metadata_offset");
	placeholder_set_offset(xm64, metadata_size, "metadata_size");

	rewind(meta);
	uint8_t *metadata = (uint8_t*)malloc(metadata_size);
	fread(metadata, 1, metadata_size, meta);
	fclose(meta);

	asset_compress_mem(metadata, metadata_size, xm64, flag_xm_compress_meta, 0, NULL);
	free(metadata);
}

static void xm_remove_empty_samples(xm_context_t *ctx)
{
	// Some instruments may have empty samples (length=0). Remove them and
	// remap the sample numbers in the instrument.
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		int sample_remap[ins->num_samples];

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

	// Calculate the optimal sample buffer size for each channel.
	// To do this, go through the whole song once doing a "dry run" playback;
	// for every tick, check which waveforms are currently played and at what
	// frequency, calculate the sample buffer size required at that tick,
	// and keep the maximum.
	int ch_buf[32] = {0};
	int num_orders = xm_get_module_length(ctx);
	bool played_orders[PATTERN_ORDER_TABLE_LENGTH] = {0};

	// Keep information of which samples in which instruments are used
	bool** used_samples = (bool**)calloc(ctx->module.num_instruments, sizeof(bool*));
	for (int i=0; i<ctx->module.num_instruments; i++)
		if (ctx->module.instruments[i].num_samples > 0)
			used_samples[i] = (bool*)calloc(ctx->module.instruments[i].num_samples, sizeof(bool));

	sample_skip_points.clear();

	while (1) {
		do {
			xm_tick(ctx);

			// Remember which pattern index we already played
			uint8_t pat_idx;
			xm_get_position(ctx, &pat_idx, NULL, NULL, NULL);
			played_orders[pat_idx] = true;

			// Number of samples that will be generated for this tick.
			int nsamples = ceilf(ctx->remaining_samples_in_tick);
			for(int i = 0; i < ctx->module.num_channels; ++i) {
				xm_channel_context_t *ch = &ctx->channels[i];

				if (ch->instrument && ch->sample) {
					// Mark the sample as used. Notice that sometimes ch->sample
					// is not part of the current ch->instrument->samples array
					// (the instrument can change before key on).
					int ins_idx = ch->instrument - ctx->module.instruments;
					int smp_idx = ch->sample - ch->instrument->samples;
					bool *used_samp_inst = used_samples[ins_idx];
					if (smp_idx >= 0 && smp_idx < ch->instrument->num_samples)
						used_samp_inst[smp_idx] = true;

					// Number of samples for this waveform at this playback frequency
					// (capped at the waveform length)
					int n = ceilf(ch->step * nsamples);
					if (n > ch->sample->length) {
						n = ch->sample->length;
					}

					// Convert samples to bytes (compressed samples are always 16-bit)
					if (ch->sample->bits == 16 || flag_xm_compress_samples > 0)
						n *= 2;

					// Take overread buffer into account
					n += MIXER_LOOP_OVERREAD;

					if (flag_xm_compress_samples > 0) {
						// In VADPCM mode, we always decompress in chunks of 32 bytes
						// Always make space for one more frame than strictly required,
						// as partially played back frames might exist in the buffer
						// at any point.
						n = (n + 31) / 32 * 32; // round up to 32 bytes
						n += 32; // one more frame

						// During loop, decoding of this tick could be split in two
						// (before loop end and at loop start), and this will require
						// two different 32-byte roundings. We approximate this by
						// adding yet another frame to the buffer size for this
						// sample.
						if (ch->sample->loop_type)
							n += 32;
					}

					// Keep the maximum
					if (ch_buf[i] < n)
						ch_buf[i] = n;

					// Check if the current effect is the "set offset" effect. This
					// is used to play samples at different positions in the waveform.
					// We must record the target position as skip point for the
					// current sample.
					bool key_on = ch->current->note > 0 && ch->current->note < 97;
					if (key_on && ch->current->effect_type == 0x9) {
						sample_skip_points[ch->sample].insert(ch->sample_position);
					}
				}
			}
			ctx->remaining_samples_in_tick -= nsamples;
		} while (xm_get_loop_count(ctx) == 0);

		// Check if we played all pattern orders, otherwise go to the first free one
		// This is made to support the XM files that contain multiple sub-tracks.
		// If we just play them from the start, we don't play all the patterns,
		// as the user is expected to manually seek to each sub-song. So we force
		// playing back all non-empty patterns at least one.
		bool fully_played = true;
		for (int i=0; i<num_orders; i++) {
			if (!played_orders[i]) {
				if (flag_verbose) fprintf(stderr, "  * found potential sub-song starting at pattern index: %d\n", i);
				xm_seek(ctx, i, 0, 0);
				fully_played = false;
				break;
			}
		}
		if (fully_played)
			break;
	}

	int sam_size = 0;
	for (int i=0;i<ctx->module.num_channels;i++) {
		// Add a 5% of margin, just in case there is a bug somewhere. We're still
		// pretty tight on RAM so let's not exaggerate.
		ch_buf[i] = ch_buf[i] * 1.05;

		// Round up to 8 bytes, which is the required alignment for a sample buffer.
		ch_buf[i] = ((ch_buf[i] + 7) / 8) * 8;

		// Save the size in the context structure. It will be used at playback
		// time to allocate the correct amount of sample buffers.
		ctx->ctx_size_stream_sample_buf[i] = ch_buf[i];
		sam_size += ch_buf[i];
	}

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
		fprintf(stderr, "  * RAM size: %zu KiB (ctx:%zu, patterns:%u, samples:%u)\n",
			(mem_ctx+sam_size+ctx->ctx_size_stream_pattern_buf)/1024,
			mem_ctx / 1024,
			ctx->ctx_size_stream_pattern_buf / 1024,
			sam_size / 1024
		);
		fprintf(stderr, "  * Samples RAM per channel: [");
		for (int i=0;i<ctx->module.num_channels;i++) {
			if (i!=0) fprintf(stderr, ", ");
			fprintf(stderr, "%d", ch_buf[i]);
		}
		fprintf(stderr, "]\n");
	}

	return 0;
}
