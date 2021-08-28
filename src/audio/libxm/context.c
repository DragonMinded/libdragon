/* Author: Romain "Artefact2" Dalmaso <artefact2@gmail.com> */

/* This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */

#include "xm_internal.h"

#define OFFSET(ptr) do {										\
		(ptr) = (void*)((intptr_t)(ptr) + (intptr_t)(*ctxp));	\
	} while(0)



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

	bytes_needed = xm_get_memory_needed_for_context(moddata, moddata_length);
	mempool = malloc(bytes_needed);
	if(mempool == NULL && bytes_needed > 0) {
		/* malloc() failed, trouble ahead */
		DEBUG("call to malloc() failed, returned %p", (void*)mempool);
		return 2;
	}

	/* Initialize most of the fields to 0, 0.f, NULL or false depending on type */
	memset(mempool, 0, bytes_needed);

	ctx = (*ctxp = (xm_context_t*)mempool);
	ctx->ctx_size = bytes_needed; /* Keep original requested size for xmconvert */
	mempool += sizeof(xm_context_t);

	ctx->rate = rate;
	mempool = xm_load_module(ctx, moddata, moddata_length, mempool);

	ctx->channels = (xm_channel_context_t*)mempool;
	mempool += ctx->module.num_channels * sizeof(xm_channel_context_t);

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

void xm_create_context_from_libxmize(xm_context_t** ctxp, char* libxmized, uint32_t rate) {
	size_t i, j;

	*ctxp = (void*)libxmized;

	/* Reverse steps of libxmize.c */
	OFFSET((*ctxp)->module.patterns);
	OFFSET((*ctxp)->module.instruments);
	OFFSET((*ctxp)->row_loop_count);
	OFFSET((*ctxp)->channels);

	for(i = 0; i < (*ctxp)->module.num_patterns; ++i) {
		OFFSET((*ctxp)->module.patterns[i].slots);
	}

	for(i = 0; i < (*ctxp)->module.num_instruments; ++i) {
		OFFSET((*ctxp)->module.instruments[i].samples);

		for(j = 0; j < (*ctxp)->module.instruments[i].num_samples; ++j) {
			OFFSET((*ctxp)->module.instruments[i].samples[j].data8);

			if(XM_LIBXMIZE_DELTA_SAMPLES) {
				if((*ctxp)->module.instruments[i].samples[j].length > 1) {
					if((*ctxp)->module.instruments[i].samples[j].bits == 8) {
						for(size_t k = 1; k < (*ctxp)->module.instruments[i].samples[j].length; ++k) {
							(*ctxp)->module.instruments[i].samples[j].data8[k] += (*ctxp)->module.instruments[i].samples[j].data8[k-1];
						}
					} else {
						for(size_t k = 1; k < (*ctxp)->module.instruments[i].samples[j].length; ++k) {
							(*ctxp)->module.instruments[i].samples[j].data16[k] += (*ctxp)->module.instruments[i].samples[j].data16[k-1];
						}
					}
				}
			}
		}
	}
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
