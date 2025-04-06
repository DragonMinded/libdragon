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
#include "mixer.h"

#include "../common/crc32.c"
#include "../common/nanotime.h"
#include "../common/polyfill.h"

bool flag_xm_8bit = false;
const char *flag_xm_extsampledir = NULL;

// Loops made by an odd number of bytes and shorter than this length are
// duplicated to prevent frequency changes during playback. See below for more
// information.
#define XM64_SHORT_ODD_LOOP_LENGTH  1024

// Bring libxm in
#include "../../src/audio/libxm/play.c"
#include "../../src/audio/libxm/context.c"
#include "../../src/audio/libxm/load.c"

static uint32_t xm_sample_crc32(xm_sample_t *s)
{
	if (s->bits == 8)
		return crc32((void*)s->data8, s->length);
	else
		return crc32((void*)s->data16, s->length * 2);
} 

static void xm_save_wave64(xm_sample_t *s, FILE *out, const char *outfn)
{
	int16_t *samples16 = malloc(s->length * sizeof(int16_t));
	if (s->bits == 8) {
		for (int k=0;k<s->length;k++)
			samples16[k] = (s->data8[k] << 8) | (uint8_t)s->data8[k];
	} else {
		memcpy(samples16, s->data16, s->length * sizeof(int16_t));
	}

	wav_data_t wav = {
		.cnt = s->length,
		.channels = 1,
		.sampleRate = 44100,
		.bitsPerSample = s->bits,
		.loopOffset = s->loop_start,
		.looping = s->loop_type != 0,
		.samples = samples16,
	};

	if (!wav64_write("xm", outfn, out, &wav, s->bits, flag_wav_compress))
		fatal("ERROR: failure while writing %s\n", outfn);

	free(wav.samples);
}

static void xm_save_wave_internally(xm_context_t* ctx, FILE* out, const char *outfn, int totsamples)
{
	// Use a sample structure to do internal de-duplication of samples
	struct sample_checksum {
		uint32_t hash;
		uint32_t pos;
	};
	struct sample_checksum wave_sums[totsamples+1];
	memset(wave_sums, 0, sizeof(wave_sums));

	wa(out, "WAVE", 4);
	w32_placeholderf(out, "wave_size");
	int beginpos = ftell(out);

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
				walign(out, 8);
				placeholder_set(out, "sample_%d_%d", i, j);
				wave_sums[k].hash = hash;
				wave_sums[k].pos = ftell(out);

				char *wavfn = NULL; asprintf(&wavfn, "%s.%d.%d.wav64", outfn, i, j); // used only for --debug
				xm_save_wave64(s, out, wavfn);
				free(wavfn);
			} else {
				// Deduplicate sample, use the same position
				placeholder_set_offset(out, wave_sums[k].pos, "sample_%d_%d", i, j);
			}
		}
	}

	// Save size of WAVE section
	placeholder_set_offset(out, ftell(out)-beginpos, "wave_size");
}

static void xm_save_wave_externally(xm_context_t* ctx, FILE* out, const char *outfn, int totsamples)
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
			placeholder_set_offset(out, hash, "sample_%d_%d", i, j);
			free(filename);
			free(tmpname);
		}
	}
}


static void xm_context_save(xm_context_t* ctx, FILE* out, const char *outfn) {
	const uint8_t version = 7;
	wa(out, "XM64", 4);
	w8(out, version);
	w32(out, ctx->ctx_size);
	w32(out, ctx->ctx_size_all_patterns);
	w32(out, ctx->ctx_size_all_samples);
	w32(out, ctx->ctx_size_stream_pattern_buf);
	for (int i=0; i<32; i++) w32(out, ctx->ctx_size_stream_sample_buf[i]);

	w16(out, ctx->module.tempo);
	w16(out, ctx->module.bpm);

#if XM_STRINGS
	wa(out, ctx->module.name, sizeof(ctx->module.name));
	wa(out, ctx->module.trackername, sizeof(ctx->module.trackername));
#else
	char name[MODULE_NAME_LENGTH+1] = {0}; char trackername[TRACKER_NAME_LENGTH+1] = {0};
	wa(out, name, sizeof(name)); wa(out, trackername, sizeof(trackername));
#endif
	w16(out, ctx->module.length);
	w16(out, ctx->module.restart_position);
	w16(out, ctx->module.num_channels);
	w16(out, ctx->module.num_patterns);
	w16(out, ctx->module.num_instruments);
	w32(out, ctx->module.frequency_type);
	wa(out, ctx->module.pattern_table, sizeof(ctx->module.pattern_table));

	int totsamples = 0;
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		totsamples += ins->num_samples;
	}

	for (int i=0;i<ctx->module.num_patterns;i++) {
		w16(out, ctx->module.patterns[i].num_rows);
		w32_placeholderf(out, "pattern_%d", i);
		w16_placeholderf(out, "pattern_size_%d", i);
	}

	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
#if XM_STRINGS
		wa(out, ins->name, sizeof(ins->name));
#else
		char name[INSTRUMENT_NAME_LENGTH + 1] = {0};
		wa(out, name);
#endif
		wa(out, ins->sample_of_notes, sizeof(ins->sample_of_notes));

		w8(out, ins->volume_envelope.num_points);
		for (int j=0;j<ins->volume_envelope.num_points;j++) {
			w16(out, ins->volume_envelope.points[j].frame);
			w16(out, ins->volume_envelope.points[j].value);
		}
		w8(out, ins->volume_envelope.sustain_point);
		w8(out, ins->volume_envelope.loop_start_point);
		w8(out, ins->volume_envelope.loop_end_point);
		w8(out, ins->volume_envelope.enabled);
		w8(out, ins->volume_envelope.sustain_enabled);
		w8(out, ins->volume_envelope.loop_enabled);

		w8(out, ins->panning_envelope.num_points);
		for (int j=0;j<ins->panning_envelope.num_points;j++) {
			w16(out, ins->panning_envelope.points[j].frame);
			w16(out, ins->panning_envelope.points[j].value);
		}
		w8(out, ins->panning_envelope.sustain_point);
		w8(out, ins->panning_envelope.loop_start_point);
		w8(out, ins->panning_envelope.loop_end_point);
		w8(out, ins->panning_envelope.enabled);
		w8(out, ins->panning_envelope.sustain_enabled);
		w8(out, ins->panning_envelope.loop_enabled);

		w32(out, ins->vibrato_type);
		w8(out, ins->vibrato_sweep);
		w8(out, ins->vibrato_depth);
		w8(out, ins->vibrato_rate);
		w16(out, ins->volume_fadeout);
		w64(out, ins->latest_trigger);

		w16(out, ins->num_samples);
		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			w8(out, s->bits);
			w32(out, s->length);
			w32(out, s->loop_start);
			w32(out, s->loop_length);
			w32(out, s->loop_end);
			wf32(out, s->volume);
			w8(out, s->finetune);
			w32(out, s->loop_type);
			wf32(out, s->panning);
			w8(out, s->relative_note);
			w32_placeholderf(out, "sample_%d_%d", i, j);
		}
	}

	if (flag_xm_extsampledir)
		xm_save_wave_externally(ctx, out, outfn, totsamples);
	else
		xm_save_wave_internally(ctx, out, outfn, totsamples);

	wa(out, "PATT", 4);
	w32_placeholderf(out, "pattern_size");
	int patt_beginpos = ftell(out);

	for (int i=0;i<ctx->module.num_patterns;i++) {
		walign(out, 8);
		placeholder_set(out, "pattern_%d", i);
		int pos = ftell(out);

		xm_pattern_t *p = &ctx->module.patterns[i];

		int pat_size = p->num_rows*ctx->module.num_channels*5;
		uint8_t cur_pat[pat_size];
		uint8_t *pp = cur_pat;

		xm_pattern_slot_t *s = &p->slots[0];
		for (int j=0;j<p->num_rows;j++) {
			for (int k=0;k<ctx->module.num_channels;k++) {
				*pp++ = s->note;
				*pp++ = s->instrument;
				*pp++ = s->volume_column;
				*pp++ = s->effect_type;
				*pp++ = s->effect_param;
				s++;
			}
		}

		// RLE-compress pattern
		//
		// The compressed stream is a sequence of "blocks". The number of blocks
		// is not encoded, so the compressed size must be provided off-band.
		// The following describes the format of a block.
		//
		// Each block begins with one varint-encoded number. The lowest 3 bits
		// of this number represents the number of "runs" in this block, while
		// the remaining bits are the number of "zeros". If the number of "runs"
		// is 7, another varint-encoded number follows, that must be added to 7
		// to obtain the real number of runs. After this, the block contains
		// "runs" bytes, which are the literal data.
		// The decompressor must first emit the specified number of zeros,
		// and then copy the literal data.
		//
		// Varint encoding: a sequence of bytes where for each byte, the MSB
		// is the continuation bit (1=another byte follow, 0=this is the last byte),
		// while the other 7 bits are the actual payload that must be concatenated
		// across all bytes. Notice that the encoding is big-endian, so the first
		// byte contains the highest 7 bits of the encoded number. 
		//
		int x = 0;
		while (x < pat_size) {
			// Detect sequence of zeros
			int zeros = 0;
			while (x+zeros < pat_size && cur_pat[x+zeros] == 0)
				zeros++;
			x += zeros;

			// Detect sequence of runs
			// NOTE: we don't stop the sequence for a single zero byte, because
			// that would be less efficient. So the runs finish when two zero
			// bytes are detected.
			int runs = 0;
			while (x+runs < pat_size && (cur_pat[x+runs] != 0 || x+runs+1 >= pat_size || cur_pat[x+runs+1] != 0))
				runs++;

			// Prepare the encoded zeros number (with the lowest bits that encode
			// part of runs), and the encoded runs (leftover).
			int runs_low = (runs > 7) ? 7 : runs;
			int enc_zeros = (zeros << 3) | runs_low;
			int enc_runs  = runs - runs_low;

			#define UINT32_NUM_BITS(n)   (32 - ((n) == 0 ? 32 : __builtin_clz(n)))
			#define CEIL_DIV(n, d)       (((n) + (d) - 1) / (d))

			// Varint-encode the zeros
			int nb = CEIL_DIV(UINT32_NUM_BITS(enc_zeros), 7) - 1;
			while (nb>0) {
				w8(out, (uint8_t)(0x80 | ((enc_zeros >> (nb*7)) & 0x7F)));
				nb--;
			}
			w8(out, (uint8_t)(enc_zeros & 0x7F));

			// Varint-encode the lefotver runs (if any)
			if (runs_low == 7) {			
				int nb = CEIL_DIV(UINT32_NUM_BITS(enc_runs), 7) - 1;
				while (nb>0) {
					w8(out, (uint8_t)(0x80 | ((enc_runs >> (nb*7)) & 0x7F)));
					nb--;
				}
				w8(out, (uint8_t)(enc_runs & 0x7F));
			}

			// Write the actual literal data
			wa(out, cur_pat+x, runs);
			x += runs;
		}

		placeholder_set_offset(out, ftell(out) - pos, "pattern_size_%d", i);
	}

	placeholder_set_offset(out, ftell(out) - patt_beginpos, "pattern_size");

	wa(out, "END!", 4);

	#undef _CHKSZ
	#undef _W8
	#undef _W16
	#undef _W32
	#undef _W64
	#undef _WA
	#undef W8
	#undef W16
	#undef W32
	#undef W64
	#undef WA
	#undef WB
	#undef WF
	#undef WALIGN
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

	// FIXME: force compression to 0 for now, as VADPCM doesn't support
	// loop points yet.
	flag_wav_compress = 0;

	FILE *xm = fopen(infn, "rb");
	if (!xm) fatal("cannot open: %s\n", infn);

	fseek(xm, 0, SEEK_END);
	int fsize = ftell(xm);
	fseek(xm, 0, SEEK_SET);

	char *xmdata = malloc(fsize);
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
				int8_t *data8 = malloc(s->length);
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
				sout = malloc(length);
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
					sout = malloc(loop_end + loop_length);
					length = loop_end+loop_length;
					// Copy waveform until loop end
					memcpy(sout, s->data8, loop_end);
					// Duplicate loop
					memmove(sout + loop_end, s->data8 + loop_end - loop_length, loop_length);
					loop_end += loop_length;
					loop_length *= 2;
				} else {				
					sout = malloc(loop_end);
					length = loop_end;
					// Copy waveform until loop end
					memcpy(sout, s->data8, loop_end);
				}
				break;
			case XM_PING_PONG_LOOP:
				length = loop_end + loop_length;
				sout = malloc(length);
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
	bool** used_samples = calloc(ctx->module.num_instruments, sizeof(bool*));
	for (int i=0; i<ctx->module.num_instruments; i++)
		if (ctx->module.instruments[i].num_samples > 0)
			used_samples[i] = calloc(ctx->module.instruments[i].num_samples, sizeof(bool));

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
					bool *used_samp_inst = used_samples[ch->instrument - ctx->module.instruments];
					int smp_idx = ch->sample - ch->instrument->samples;
					if (smp_idx >= 0 && smp_idx < ch->instrument->num_samples)
						used_samp_inst[smp_idx] = true;

					// Number of samples for this waveform at this playback frequency
					// (capped at the waveform length)
					int n = ceilf(ch->step * nsamples);
					if (n > ch->sample->length) {
						n = ch->sample->length;
					}

					// Convert samples to bytes
					if (ch->sample->bits == 16)
						n *= 2;

					// Take overread buffer into account
					n += MIXER_LOOP_OVERREAD;

					// Keep the maximum
					if (ch_buf[i] < n)
						ch_buf[i] = n;
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


	// Try reloading the just-created file. This is just a safety net to catch
	// serialization bugs and other mistakes immediately at conversion time.
	// It's not required if we can't trust our own code, but it's not a big
	// deal anyway, so better safe than sorry.
	xm_context_t *ctx2;
	out = fopen(outfn, "rb");
	if (!out) fatal("cannot open: %s", outfn);
	int ret = xm_context_load(&ctx2, out, 48000);
	if (ret != 0) fatal("internal error: loading just created module: %s (ret:%d)", outfn, ret);
	fclose(out);

	return 0;
}
