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

#include "mixer.h"

// Loops made by an odd number of bytes and shorter than this length are
// duplicated to prevent frequency changes during playback. See below for more
// information.
#define XM64_SHORT_ODD_LOOP_LENGTH  1024

// Bring libxm in
#include "../../src/audio/libxm/play.c"
#include "../../src/audio/libxm/context.c"
#include "../../src/audio/libxm/load.c"

int xm_convert(const char *infn, const char *outfn) {
	if (flag_verbose)
		fprintf(stderr, "Converting: %s => %s\n", infn, outfn);

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

	// Pre-process all waveforms:
	//   1) Ping-pong loops will be unrolled as regular forward
	//   2) Repeat initial data after loop end for MIXER_LOOP_OVERREAD bytes
	//      to speed up decoding in RSP.
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];

		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			int bps = s->bits / 8;

			uint32_t length = s->length * bps;
			uint32_t loop_length = s->loop_length * bps;
			uint32_t loop_end = s->loop_end * bps;

			uint8_t *sout, *out;
			switch (s->loop_type) {
			default:
				fatal("invalid loop type: %d\n", s->loop_type);
			case XM_NO_LOOP:
				sout = malloc(length + MIXER_LOOP_OVERREAD);
				memcpy(sout, s->data8, length);
				memset(sout+length, 0, MIXER_LOOP_OVERREAD);
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
					sout = malloc(loop_end + loop_length + MIXER_LOOP_OVERREAD);
					length = loop_end+loop_length;
					// Copy waveform until loop end
					memcpy(sout, s->data8, loop_end);
					// Duplicate loop
					memmove(sout + loop_end, s->data8 + loop_end - loop_length, loop_length);
					// Add overread
					memmove(sout + loop_end + loop_length, s->data8 + loop_end - loop_length, MIXER_LOOP_OVERREAD);
					loop_end += loop_length;
					loop_length *= 2;
				} else {				
					sout = malloc(loop_end + MIXER_LOOP_OVERREAD);
					length = loop_end;
					// Copy waveform until loop end
					memcpy(sout, s->data8, loop_end);
					// Add overread
					memmove(sout + loop_end, s->data8 + loop_end - loop_length, MIXER_LOOP_OVERREAD);
				}
				break;
			case XM_PING_PONG_LOOP:
				length = loop_end + loop_length;
				sout = malloc(length + MIXER_LOOP_OVERREAD);
				out = sout;

				memcpy(out, s->data8, loop_end);
				out += loop_end;
				for (int x=0;x<loop_length;x++)
					*out++ = s->data8[(loop_end-x-1) ^ (bps>>1)];
				memmove(out, s->data8 + loop_end - loop_length, MIXER_LOOP_OVERREAD);

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

	while (xm_get_loop_count(ctx) == 0) {
		xm_tick(ctx);

		// Number of samples that will be generated for this tick.
		int nsamples = ceilf(ctx->remaining_samples_in_tick);
		for(int i = 0; i < ctx->module.num_channels; ++i) {
			xm_channel_context_t *ch = &ctx->channels[i];

			if (ch->instrument && ch->sample) {
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

	FILE *out = fopen(outfn, "wb");
	if (!out) fatal("cannot create: %s", outfn);
	xm_context_save(ctx, out);
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
