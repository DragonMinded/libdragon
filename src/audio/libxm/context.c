/* Author: Romain "Artefact2" Dalmaso <artefact2@gmail.com> */

/* This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */

#include "xm_internal.h"
#include <stdio.h>
#include <assert.h>


int xm_create_context(xm_context_t** ctxp, const char* moddata, uint32_t rate) {
	return xm_create_context_safe(ctxp, moddata, SIZE_MAX, rate);
}

int xm_create_context_safe(xm_context_t** ctxp, const char* moddata, size_t moddata_length, uint32_t rate) {
	size_t bytes_needed;
	char* mempool;
	xm_context_t* ctx;

	if(XM_DEFENSIVE) {
		int ret;
		if((ret = xm_check_sanity_preload(moddata, moddata_length))) {
			DEBUG("xm_check_sanity_preload() returned %i, module is not safe to load", ret);
			return 1;
		}
	}

	size_t mem_ctx, mem_pat, mem_sam;
	xm_get_memory_needed_for_context(moddata, moddata_length, &mem_ctx, &mem_pat, &mem_sam);
	bytes_needed = mem_ctx + mem_pat + mem_sam;
	mempool = malloc(bytes_needed);
	if(mempool == NULL && bytes_needed > 0) {
		/* malloc() failed, trouble ahead */
		DEBUG("call to malloc() failed, returned %p", (void*)mempool);
		return 2;
	}

	/* Initialize most of the fields to 0, 0.f, NULL or false depending on type */
	memset(mempool, 0, bytes_needed);

	ctx = (*ctxp = (xm_context_t*)mempool);
	mempool += sizeof(xm_context_t);

	ctx->rate = rate;
	mempool = xm_load_module(ctx, moddata, moddata_length, mempool, mem_ctx-sizeof(xm_context_t), mem_sam, mem_pat);

 	/* To load serialized data, we will need memory for all components. */
	ctx->ctx_size = mem_ctx+mem_sam+mem_pat;
	ctx->ctx_size_all_samples = mem_sam;
	ctx->ctx_size_all_patterns = mem_pat;

	/* Calculate the amount of memory required to stream patterns. This is
	   equal to the memory required to hold the largest pattern. */
	ctx->ctx_size_stream_pattern_buf = 0;
	for(int i = 0; i < ctx->module.num_patterns; ++i) {
		xm_pattern_t *p = &ctx->module.patterns[i];
		int pat_size = p->num_rows * ctx->module.num_channels * sizeof(xm_pattern_slot_t);
		if (ctx->ctx_size_stream_pattern_buf < pat_size)
			ctx->ctx_size_stream_pattern_buf = pat_size;
	}

	ctx->channels = (xm_channel_context_t*)mempool;
	mempool += ctx->module.num_channels * sizeof(xm_channel_context_t);

	ctx->tempo = ctx->module.tempo;
	ctx->bpm = ctx->module.bpm;

	ctx->global_volume = 1.f;
	ctx->amplification = .25f; /* XXX: some bad modules may still clip. Find out something better. */

#if XM_RAMPING
	ctx->volume_ramp = (1.f / 128.f);
#endif

	for(uint8_t i = 0; i < ctx->module.num_channels; ++i) {
		xm_channel_context_t* ch = ctx->channels + i;

		ch->ping = true;
		ch->vibrato_waveform = XM_SINE_WAVEFORM;
		ch->vibrato_waveform_retrigger = true;
		ch->tremolo_waveform = XM_SINE_WAVEFORM;
		ch->tremolo_waveform_retrigger = true;

		ch->volume = ch->volume_envelope_volume = ch->fadeout_volume = 1.0f;
		ch->panning = ch->panning_envelope_panning = .5f;
		ch->actual_volume[0] = .0f;
		ch->actual_volume[1] = .0f;
	}

	ctx->row_loop_count = (uint8_t*)mempool;
	mempool += ctx->module.length * MAX_NUM_ROWS * sizeof(uint8_t);

	if(XM_DEFENSIVE) {
		int ret;
		if((ret = xm_check_sanity_postload(ctx))) {
			DEBUG("xm_check_sanity_postload() returned %i, module is not safe to play", ret);
			xm_free_context(ctx);
			return 1;
		}
	}

	return 0;
}

#if !defined(N64) && !XM_STREAM_PATTERNS && !XM_STREAM_WAVEFORMS

void xm_context_save(xm_context_t* ctx, FILE* out) {

	#undef _W64 // defined by mingw
	#define _CHKSZ(x,n) _Static_assert(sizeof(x) == n, "invalid type size");
	#define _W8(x)     ({ putc((x)&0xFF, out); })
	#define _W16(x)    ({ _W8 ((x)>>8);  _W8 (x); })
	#define _W32(x)    ({ _W16((x)>>16); _W16(x); })
	#define _W64(x)    ({ _W32((x)>>32); _W32(x); })
	#define _WA(x,n)   ({ fwrite(x, 1, n,out); })

	#define W8(x)     ({ _CHKSZ(x,1); _W8((uint8_t)(x)); })
	#define W16(x)    ({ _CHKSZ(x,2); _W16((uint16_t)(x)); })
	#define W32(x)    ({ _CHKSZ(x,4); _W32((uint32_t)(x)); })
	#define W64(x)    ({ _CHKSZ(x,8); _W64((uint64_t)(x)); })
	#define WA(x,n)   ({ _CHKSZ((x)[0],1); _WA((const uint8_t*)(x),n); })
	#define WB(x)     ({ bool *b = &x; _W8(*b); })
	#define WF(x)     ({ _CHKSZ(x,4); uint8_t *f = (uint8_t*)(&x); uint32_t fx = *(uint32_t*)f; _W32(fx); })
	#define WALIGN()  ({ while (ftell(out) % 8) _W8(0); })


	const uint8_t version = 5;
	WA("XM64", 4);
	W8(version);
	W32(ctx->ctx_size);
	W32(ctx->ctx_size_all_patterns);
	W32(ctx->ctx_size_all_samples);
	W32(ctx->ctx_size_stream_pattern_buf);
	for (int i=0; i<32; i++) W32(ctx->ctx_size_stream_sample_buf[i]);

	W16(ctx->module.tempo);
	W16(ctx->module.bpm);

#if XM_STRINGS
	WA(ctx->module.name, sizeof(ctx->module.name));
	WA(ctx->module.trackername, sizeof(ctx->module.trackername));
#else
	char name[MODULE_NAME_LENGTH+1] = {0}; char trackername[TRACKER_NAME_LENGTH+1] = {0};
	WA(name, sizeof(name)); WA(trackername, sizeof(trackername));
#endif
	W16(ctx->module.length);
	W16(ctx->module.restart_position);
	W16(ctx->module.num_channels);
	W16(ctx->module.num_patterns);
	W16(ctx->module.num_instruments);
	W32(ctx->module.frequency_type);
	WA(ctx->module.pattern_table, sizeof(ctx->module.pattern_table));

	int totsamples = 0;
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		totsamples += ins->num_samples;
	}
	uint32_t pat_off[ctx->module.num_patterns];
	uint32_t sam_off[totsamples];
	int sam_off_idx=0, pat_off_idx=0;

	for (int i=0;i<ctx->module.num_patterns;i++) {
		W16(ctx->module.patterns[i].num_rows);
		pat_off[pat_off_idx++] = ftell(out);
		W32(0); // will fill later (position)
		W16((uint16_t)0); // will fill later (size)
	}

	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
#if XM_STRINGS
		WA(ins->name, sizeof(ins->name));
#else
		char name[INSTRUMENT_NAME_LENGTH + 1] = {0};
		WA(name);
#endif
		WA(ins->sample_of_notes, sizeof(ins->sample_of_notes));

		W8(ins->volume_envelope.num_points);
		for (int j=0;j<ins->volume_envelope.num_points;j++) {
			W16(ins->volume_envelope.points[j].frame);
			W16(ins->volume_envelope.points[j].value);
		}
		W8(ins->volume_envelope.sustain_point);
		W8(ins->volume_envelope.loop_start_point);
		W8(ins->volume_envelope.loop_end_point);
		WB(ins->volume_envelope.enabled);
		WB(ins->volume_envelope.sustain_enabled);
		WB(ins->volume_envelope.loop_enabled);

		W8(ins->panning_envelope.num_points);
		for (int j=0;j<ins->panning_envelope.num_points;j++) {
			W16(ins->panning_envelope.points[j].frame);
			W16(ins->panning_envelope.points[j].value);
		}
		W8(ins->panning_envelope.sustain_point);
		W8(ins->panning_envelope.loop_start_point);
		W8(ins->panning_envelope.loop_end_point);
		WB(ins->panning_envelope.enabled);
		WB(ins->panning_envelope.sustain_enabled);
		WB(ins->panning_envelope.loop_enabled);

		W32(ins->vibrato_type);
		W8(ins->vibrato_sweep);
		W8(ins->vibrato_depth);
		W8(ins->vibrato_rate);
		W16(ins->volume_fadeout);
		W64(ins->latest_trigger);

		W16(ins->num_samples);
		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			W8(s->bits);
			W32(s->length);
			W32(s->loop_start);
			W32(s->loop_length);
			W32(s->loop_end);
			WF(s->volume);
			W8(s->finetune);
			W32(s->loop_type);
			WF(s->panning);
			W8(s->relative_note);
			sam_off[sam_off_idx++] = ftell(out);
			W32(0); // will fill later
		}
	}


	pat_off_idx = 0;
	sam_off_idx = 0;

	WA("WAVE", 4);
	uint32_t wv_overred = XM_WAVEFORM_OVERREAD;
	W32(wv_overred);
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			WALIGN();

			uint32_t pos = ftell(out);
			fseek(out, sam_off[sam_off_idx++], SEEK_SET);
			W32(pos);
			fseek(out, pos, SEEK_SET);

			assert(s->bits == 8 || s->bits == 16);
			if (s->bits == 8)
				WA(s->data8, s->length+XM_WAVEFORM_OVERREAD);
			else {
				for (int k=0;k<s->length+XM_WAVEFORM_OVERREAD/2;k++)
					W16(s->data16[k]);
			}
		}
	}

	WA("PATT", 4);
	for (int i=0;i<ctx->module.num_patterns;i++) {
		WALIGN();

		uint32_t pos = ftell(out);

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
				W8((uint8_t)(0x80 | ((enc_zeros >> (nb*7)) & 0x7F)));
				nb--;
			}
			W8((uint8_t)(enc_zeros & 0x7F));

			// Varint-encode the lefotver runs (if any)
			if (runs_low == 7) {			
				int nb = CEIL_DIV(UINT32_NUM_BITS(enc_runs), 7) - 1;
				while (nb>0) {
					W8((uint8_t)(0x80 | ((enc_runs >> (nb*7)) & 0x7F)));
					nb--;
				}
				W8((uint8_t)(enc_runs & 0x7F));
			}

			// Write the actual literal data
			WA(cur_pat+x, runs);
			x += runs;
		}

		uint16_t size = ftell(out) - pos;

		fseek(out, pat_off[pat_off_idx++], SEEK_SET);
		W32(pos);
		W16(size);
		fseek(out, pos+size, SEEK_SET);
	}

	WA("END!", 4);

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

#endif

static uint32_t varint_get(uint8_t **pp) {
	int8_t *p = (int8_t*)*pp; // trick: get GCC to generate better code in the loop
	uint32_t x = 0;
	int8_t y;
	do {
		y = *p++;
		x <<= 7; x |= y & 0x7F;
	} while (y & 0x80);
	*pp = (uint8_t*)p;
	return x;
}

// Decompress a pattern that was compressed with our custom RLE algorithm (see above).
// Notice that in-place decompression is supported, by loading compressed data
// at the end of the decompression buffer, so that no additional memory is required.
int xm_context_decompress_pattern(uint8_t *in, int sz, xm_pattern_slot_t *pat) {
	uint8_t *in_end = in+sz;
	uint8_t *out = (uint8_t*)pat;
	bool direction = (out<=in);
	while (in < in_end) {
		int zeros = varint_get(&in);
		int runs = zeros & 7; if (runs == 7) runs += varint_get(&in);
		zeros >>= 3;
		memset(out, 0, zeros); out += zeros;
		memcpy(out, in, runs); out += runs; in += runs;
		assert((out<=in) == direction);  // check for overruns during in-place decompression
	}
	return out - (uint8_t*)pat;
}


int xm_context_load(xm_context_t** ctxp, FILE* in, uint32_t rate) {

	#define _CHKSZ(x,n) ({ _Static_assert(sizeof(x) == n, "invalid type size"); })
	#define _R8(x)     ({ uint8_t u8; fread(&u8, 1, 1, in); x=u8; })
	#define _R16(x)    ({ uint16_t lo2, hi2; _R8(hi2); _R8(lo2); x=(hi2<<8)|lo2; })
	#define _R32(x)    ({ uint32_t lo4, hi4; _R16(hi4); _R16(lo4); x=(hi4<<16)|lo4; })
	#define _R64(x)    ({ uint64_t lo8, hi8; _R32(hi8); _R32(lo8); x=(hi8<<32)|lo8; })
	#define _RA(x,n)   ({ fread(x, n, 1, in); })

	#define R8(x)     ({ _CHKSZ(x,1); _R8((x)); })
	#define R16(x)    ({ _CHKSZ(x,2); _R16((x)); })
	#define R32(x)    ({ _CHKSZ(x,4); _R32((x)); })
	#define R64(x)    ({ _CHKSZ(x,8); _R64((x)); })
	#define RA(x,n)   ({ _RA((uint8_t*)(x),n); })
	#define RB(x)     ({ bool *b = &x; _R8(*b); })
	#define RF(x)     ({ _CHKSZ(x,4); uint32_t fx; _R32(fx); uint8_t *f = (uint8_t*)(&fx); x=*(float*)f; })
	#define RALIGN()  ({ int a = ftell(in)%8; if (a) fseek(in, 8-a, SEEK_CUR); })

	#ifdef XM_STRINGS
	#define RS(x,n)   RA(x,n)
	#else
	#define RS(x,n)   fseek(in, n, SEEK_CUR)  // just skip the string
	#endif

	uint8_t version;
	char head[4];
	RA(head, 4);
	if (head[0] != 'X' || head[1] != 'M' || head[2] != '6' || head[3] != '4') {
		DEBUG("invalid header\n");
		return 1;
	}
	R8(version);
	if (version != 5) {
		DEBUG("invalid XM64 version %d\n", version);
		return 1;		
	}

	uint32_t ctx_size, ctx_size_all_samples, ctx_size_all_patterns, ctx_size_stream_pattern_buf, ctx_size_stream_sample_buf[32];

	R32(ctx_size);
	R32(ctx_size_all_patterns);
	R32(ctx_size_all_samples);
	R32(ctx_size_stream_pattern_buf);
	for (int i=0;i<32;i++) R32(ctx_size_stream_sample_buf[i]);

	uint32_t alloc_bytes = ctx_size;
	#if XM_STREAM_PATTERNS
	alloc_bytes -= ctx_size_all_patterns;
	alloc_bytes += ctx_size_stream_pattern_buf;
	#endif
	#if XM_STREAM_WAVEFORMS
	alloc_bytes -= ctx_size_all_samples;
	#endif

	char *mempool = malloc(alloc_bytes);
	char *mempool_end = mempool+alloc_bytes;
	memset(mempool, 0, alloc_bytes);

	xm_context_t *ctx = (xm_context_t*)mempool;
	mempool += sizeof(xm_context_t);

	*ctxp = ctx;
	ctx->ctx_size = ctx_size;
	ctx->ctx_size_all_samples = ctx_size_all_samples;
	ctx->ctx_size_all_patterns = ctx_size_all_patterns;
	ctx->ctx_size_stream_pattern_buf = ctx_size_stream_pattern_buf;
	for (int i=0;i<32;i++) ctx->ctx_size_stream_sample_buf[i] = ctx_size_stream_sample_buf[i];

#if XM_STREAM_WAVEFORMS || XM_STREAM_PATTERNS
	ctx->fh = in;   /* Save the file if we need to stream later */
#endif

	R16(ctx->module.tempo);
	R16(ctx->module.bpm);

	RS(ctx->module.name, sizeof(ctx->module.name));
	RS(ctx->module.trackername, sizeof(ctx->module.trackername));

	R16(ctx->module.length);
	R16(ctx->module.restart_position);
	R16(ctx->module.num_channels);
	R16(ctx->module.num_patterns);
	R16(ctx->module.num_instruments);
	R32(ctx->module.frequency_type);
	RA(ctx->module.pattern_table, sizeof(ctx->module.pattern_table));

	ctx->module.patterns = (xm_pattern_t*)mempool;
	mempool += sizeof(xm_pattern_t) * ctx->module.num_patterns;

	for (int i=0;i<ctx->module.num_patterns;i++) {
		R16(ctx->module.patterns[i].num_rows);
		R32(ctx->module.patterns[i].slots_offset);
		R16(ctx->module.patterns[i].slots_size);
	}

	ctx->module.instruments = (xm_instrument_t*)mempool;
	mempool += sizeof(xm_instrument_t) * ctx->module.num_instruments;

	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];
		RS(ins->name, sizeof(ins->name));

		RA(ins->sample_of_notes, sizeof(ins->sample_of_notes));

		R8(ins->volume_envelope.num_points);
		for (int j=0;j<ins->volume_envelope.num_points;j++) {
			R16(ins->volume_envelope.points[j].frame);
			R16(ins->volume_envelope.points[j].value);
		}
		R8(ins->volume_envelope.sustain_point);
		R8(ins->volume_envelope.loop_start_point);
		R8(ins->volume_envelope.loop_end_point);
		RB(ins->volume_envelope.enabled);
		RB(ins->volume_envelope.sustain_enabled);
		RB(ins->volume_envelope.loop_enabled);

		R8(ins->panning_envelope.num_points);
		for (int j=0;j<ins->panning_envelope.num_points;j++) {
			R16(ins->panning_envelope.points[j].frame);
			R16(ins->panning_envelope.points[j].value);
		}
		R8(ins->panning_envelope.sustain_point);
		R8(ins->panning_envelope.loop_start_point);
		R8(ins->panning_envelope.loop_end_point);
		RB(ins->panning_envelope.enabled);
		RB(ins->panning_envelope.sustain_enabled);
		RB(ins->panning_envelope.loop_enabled);

		R32(ins->vibrato_type);
		R8(ins->vibrato_sweep);
		R8(ins->vibrato_depth);
		R8(ins->vibrato_rate);
		R16(ins->volume_fadeout);
		R64(ins->latest_trigger);

		R16(ins->num_samples);
		ins->samples = (xm_sample_t*)mempool;
		mempool += sizeof(xm_sample_t) * ins->num_samples;
		if ((size_t)mempool & 7) mempool += 8 - ((size_t)mempool & 7);

		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];
			R8(s->bits);
			R32(s->length);
			R32(s->loop_start);
			R32(s->loop_length);
			R32(s->loop_end);
			RF(s->volume);
			R8(s->finetune);
			R32(s->loop_type);
			RF(s->panning);
			R8(s->relative_note);
			R32(s->data8_offset);
		}
	}

	RA(head, 4);
	if (head[0] != 'W' && head[1] != 'A' && head[2] != 'V' && head[3] != 'E') {
		DEBUG("invalid WAVE header\n");
		free(*ctxp);
		*ctxp = NULL;
		return 1;
	}

	uint32_t wv_overread;
	R32(wv_overread);
	if (wv_overread < XM_WAVEFORM_OVERREAD) {
		DEBUG("waveform overread too little (%d < %d)\n", (int)wv_overread, XM_WAVEFORM_OVERREAD);
		free(*ctxp);
		*ctxp = NULL;
		return 1;
	}


#if !XM_STREAM_WAVEFORMS
	for (int i=0;i<ctx->module.num_instruments;i++) {
		xm_instrument_t *ins = &ctx->module.instruments[i];

		for (int j=0;j<ins->num_samples;j++) {
			xm_sample_t *s = &ins->samples[j];

			fseek(in, s->data8_offset, SEEK_SET);

			s->data8 = (int8_t*)mempool;
			mempool += s->length * (s->bits / 8) + XM_WAVEFORM_OVERREAD;
			if ((size_t)mempool & 7) mempool += 8 - ((size_t)mempool & 7);

			if (s->bits == 8)
				RA(s->data8, s->length+XM_WAVEFORM_OVERREAD);
			else {
				RA(s->data8, s->length*2+XM_WAVEFORM_OVERREAD);
				#if BYTE_ORDER == LITTLE_ENDIAN
				for (int k=0;k<s->length+XM_WAVEFORM_OVERREAD/2;k++)
					s->data16[k] = __builtin_bswap16(s->data16[k]);
				#endif
			}
		}
	}

	// This is actually not guaranteed by the file format, but since the
	// save function laids out waveforms in order, after reading the last one
	// we should have arrived on the pattern magic string.
	RA(head, 4);
	if (head[0] != 'P' || head[1] != 'A' || head[2] != 'T' || head[3] != 'T') {
		DEBUG("invalid PATT header\n");
		free(*ctxp);
		*ctxp = NULL;
		return 1;
	}
#endif

#if !XM_STREAM_PATTERNS
	for (int i=0;i<ctx->module.num_patterns;i++) {
		xm_pattern_t *p = &ctx->module.patterns[i];

		int cmp_size = p->slots_size;
		int dec_size = sizeof(xm_pattern_slot_t) * p->num_rows * ctx->module.num_channels;

		fseek(in, p->slots_offset, SEEK_SET);

		assert(((size_t)mempool & 7) == 0);
		p->slots = (xm_pattern_slot_t*)mempool;
		mempool += sizeof(xm_pattern_slot_t) * ctx->module.num_channels * p->num_rows;
		if ((size_t)mempool & 7) mempool += 8 - ((size_t)mempool & 7);

		uint8_t *cmp_data = (uint8_t*)p->slots + dec_size - cmp_size;
		RA(cmp_data, cmp_size);

		// Decompress the slots
		int sz = xm_context_decompress_pattern(cmp_data, cmp_size, p->slots);
		assert(sz == dec_size);
	}

	// This is actually not guaranteed by the file format, but since the
	// save function laids out patterns in order, after reading the last one
	// we should have arrived on the end magic string.
	RA(head, 4);
	if (head[0] != 'E' || head[1] != 'N' || head[2] != 'D' || head[3] != '!') {
		DEBUG("invalid END header (%x)\n", (unsigned)ftell(in));
		free(*ctxp);
		*ctxp = NULL;
		return 1;
	}
#else
	ctx->slot_buffer_index = -1;
	ctx->slot_buffer = (xm_pattern_slot_t*)mempool;
	mempool += ctx->ctx_size_stream_pattern_buf;
	if ((size_t)mempool & 7) mempool += 8 - ((size_t)mempool & 7);
#endif

	ctx->rate = rate;

	ctx->channels = (xm_channel_context_t*)mempool;
	mempool += ctx->module.num_channels * sizeof(xm_channel_context_t);

	ctx->tempo = ctx->module.tempo;
	ctx->bpm = ctx->module.bpm;

	ctx->global_volume = 1.f;
	ctx->amplification = .25f; /* XXX: some bad modules may still clip. Find out something better. */

#if XM_RAMPING
	ctx->volume_ramp = (1.f / 128.f);
#endif

	for(uint8_t i = 0; i < ctx->module.num_channels; ++i) {
		xm_channel_context_t* ch = ctx->channels + i;

		ch->ping = true;
		ch->vibrato_waveform = XM_SINE_WAVEFORM;
		ch->vibrato_waveform_retrigger = true;
		ch->tremolo_waveform = XM_SINE_WAVEFORM;
		ch->tremolo_waveform_retrigger = true;

		ch->volume = ch->volume_envelope_volume = ch->fadeout_volume = 1.0f;
		ch->panning = ch->panning_envelope_panning = .5f;
		ch->actual_volume[0] = .0f;
		ch->actual_volume[1] = .0f;
	}

	ctx->row_loop_count = (uint8_t*)mempool;
	mempool += ctx->module.length * MAX_NUM_ROWS * sizeof(uint8_t);

	if (mempool != mempool_end) {
		// If we used more than declared, then it's a bug. We can't continue
		// or we'd be doing a buffer overflow. Just abort.
		if (mempool > mempool_end) {
			DEBUG("invalid mempool size allocated (diff: %d)\n", (unsigned)(mempool-mempool_end));
			free(*ctxp);
			*ctxp = NULL;
			return 2;
		}

		// FIXME: currently, it's normal to use less memory than declared.
		// Memory consumption is calculated with the audioconv tool on x86
		// (normally, 64bit), where data structures defined in xm_internal.h
		// might have a different size (especially, pointers). This could be
		// fixed by carefully defined padding etc. and make sure that all
		// structures have the same size.
		// Anyway, allocating 100-200 bytes more isn't going to hurt for now.
	}

	return 0;
}

void xm_free_context(xm_context_t* context) {
	free(context);
}

void xm_set_max_loop_count(xm_context_t* context, uint8_t loopcnt) {
	context->max_loop_count = loopcnt;
}

uint8_t xm_get_loop_count(xm_context_t* context) {
	return context->loop_count;
}



void xm_seek(xm_context_t* ctx, uint8_t pot, uint8_t row, uint16_t tick) {
	ctx->current_table_index = pot;
	ctx->current_row = row;
	ctx->current_tick = tick;
	ctx->remaining_samples_in_tick = 0;
}



bool xm_mute_channel(xm_context_t* ctx, uint16_t channel, bool mute) {
	bool old = ctx->channels[channel - 1].muted;
	ctx->channels[channel - 1].muted = mute;
	return old;
}

bool xm_mute_instrument(xm_context_t* ctx, uint16_t instr, bool mute) {
	bool old = ctx->module.instruments[instr - 1].muted;
	ctx->module.instruments[instr - 1].muted = mute;
	return old;
}



#if XM_STRINGS
const char* xm_get_module_name(xm_context_t* ctx) {
	return ctx->module.name;
}

const char* xm_get_tracker_name(xm_context_t* ctx) {
	return ctx->module.trackername;
}
#else
const char* xm_get_module_name(xm_context_t* ctx) {
	return NULL;
}

const char* xm_get_tracker_name(xm_context_t* ctx) {
	return NULL;
}
#endif



uint16_t xm_get_number_of_channels(xm_context_t* ctx) {
	return ctx->module.num_channels;
}

uint16_t xm_get_module_length(xm_context_t* ctx) {
	return ctx->module.length;
}

uint16_t xm_get_number_of_patterns(xm_context_t* ctx) {
	return ctx->module.num_patterns;
}

uint16_t xm_get_number_of_rows(xm_context_t* ctx, uint16_t pattern) {
	return ctx->module.patterns[pattern].num_rows;
}

uint16_t xm_get_number_of_instruments(xm_context_t* ctx) {
	return ctx->module.num_instruments;
}

uint16_t xm_get_number_of_samples(xm_context_t* ctx, uint16_t instrument) {
	return ctx->module.instruments[instrument - 1].num_samples;
}

void* xm_get_sample_waveform(xm_context_t* ctx, uint16_t i, uint16_t s, size_t* size, uint8_t* bits) {
	*size = ctx->module.instruments[i - 1].samples[s].length;
	*bits = ctx->module.instruments[i - 1].samples[s].bits;
	return ctx->module.instruments[i - 1].samples[s].data8;
}



void xm_get_playing_speed(xm_context_t* ctx, uint16_t* bpm, uint16_t* tempo) {
	if(bpm) *bpm = ctx->bpm;
	if(tempo) *tempo = ctx->tempo;
}

void xm_get_position(xm_context_t* ctx, uint8_t* pattern_index, uint8_t* pattern, uint8_t* row, uint64_t* samples) {
	if(pattern_index) *pattern_index = ctx->current_table_index;
	if(pattern) *pattern = ctx->module.pattern_table[ctx->current_table_index];
	if(row) *row = ctx->current_row;
	if(samples) *samples = ctx->generated_samples;
}

uint64_t xm_get_latest_trigger_of_instrument(xm_context_t* ctx, uint16_t instr) {
	return ctx->module.instruments[instr - 1].latest_trigger;
}

uint64_t xm_get_latest_trigger_of_sample(xm_context_t* ctx, uint16_t instr, uint16_t sample) {
	return ctx->module.instruments[instr - 1].samples[sample].latest_trigger;
}

uint64_t xm_get_latest_trigger_of_channel(xm_context_t* ctx, uint16_t chn) {
	return ctx->channels[chn - 1].latest_trigger;
}

bool xm_is_channel_active(xm_context_t* ctx, uint16_t chn) {
	xm_channel_context_t* ch = ctx->channels + (chn - 1);
	return ch->instrument != NULL && ch->sample != NULL && ch->sample_position >= 0;
}

float xm_get_frequency_of_channel(xm_context_t* ctx, uint16_t chn) {
	return ctx->channels[chn - 1].frequency;
}

float xm_get_volume_of_channel(xm_context_t* ctx, uint16_t chn) {
	return ctx->channels[chn - 1].volume * ctx->global_volume;
}

float xm_get_panning_of_channel(xm_context_t* ctx, uint16_t chn) {
	return ctx->channels[chn - 1].panning;
}

uint16_t xm_get_instrument_of_channel(xm_context_t* ctx, uint16_t chn) {
	xm_channel_context_t* ch = ctx->channels + (chn - 1);
	if(ch->instrument == NULL) return 0;
	return 1 + (ch->instrument - ctx->module.instruments);
}

void xm_set_effect_callback(xm_context_t *ctx, xm_effect_callback_t cb, void *cbctx) {
	ctx->effect_callback = cb;
	ctx->effect_callback_ctx = cbctx;
}

