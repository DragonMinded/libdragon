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
	ctx->fd = fileno(in);   /* Save the file if we need to stream later */
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

	R8(ctx->external_samples);

#if XM_STREAM_PATTERNS
	if ((size_t)mempool & 15) mempool += 16 - ((size_t)mempool & 15);
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

