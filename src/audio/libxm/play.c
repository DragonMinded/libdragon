/* Author: Romain "Artefact2" Dalmaso <artefact2@gmail.com> */
/* Contributor: Daniel Oaks <daniel@danieloaks.net> */

/* This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */

#include "xm_internal.h"
#include <inttypes.h>

/* ----- Static functions ----- */

static float xm_waveform(xm_waveform_type_t, uint8_t);
static void xm_autovibrato(xm_context_t*, xm_channel_context_t*);
static void xm_vibrato(xm_context_t*, xm_channel_context_t*, uint8_t);
static void xm_tremolo(xm_context_t*, xm_channel_context_t*, uint8_t, uint16_t);
static void xm_arpeggio(xm_context_t*, xm_channel_context_t*, uint8_t, uint16_t);
static void xm_tone_portamento(xm_context_t*, xm_channel_context_t*);
static void xm_pitch_slide(xm_context_t*, xm_channel_context_t*, float);
static void xm_panning_slide(xm_channel_context_t*, uint8_t);
static void xm_volume_slide(xm_channel_context_t*, uint8_t);

static float xm_envelope_lerp(xm_envelope_point_t*, xm_envelope_point_t*, uint16_t);
static void xm_envelope_tick(xm_channel_context_t*, xm_envelope_t*, uint16_t*, float*);
static void xm_envelopes(xm_channel_context_t*);

static float xm_linear_period(float);
static float xm_linear_frequency(float);
static float xm_amiga_period(float);
static float xm_amiga_frequency(float);
static float xm_period(xm_context_t*, float);
static float xm_frequency(xm_context_t*, float, float, float);
static void xm_update_frequency(xm_context_t*, xm_channel_context_t*);

static void xm_handle_note_and_instrument(xm_context_t*, xm_channel_context_t*, xm_pattern_slot_t*);
static void xm_trigger_note(xm_context_t*, xm_channel_context_t*, unsigned int flags);
static void xm_cut_note(xm_channel_context_t*);
static void xm_key_off(xm_channel_context_t*);

static void xm_post_pattern_change(xm_context_t*);
static void xm_row(xm_context_t*);
static void xm_tick(xm_context_t*);

static float xm_sample_at(xm_sample_t*, size_t);
static float xm_next_of_sample(xm_channel_context_t*);
static void xm_sample(xm_context_t*, float*, float*);

/* ----- Other oddities ----- */

#define XM_TRIGGER_KEEP_VOLUME (1 << 0)
#define XM_TRIGGER_KEEP_PERIOD (1 << 1)
#define XM_TRIGGER_KEEP_SAMPLE_POSITION (1 << 2)
#define XM_TRIGGER_KEEP_ENVELOPE (1 << 3)

#define AMIGA_FREQ_SCALE   1024

static const uint32_t amiga_frequencies[] = {
	1712*AMIGA_FREQ_SCALE, 1616*AMIGA_FREQ_SCALE, 1525*AMIGA_FREQ_SCALE, 1440*AMIGA_FREQ_SCALE, /* C-2, C#2, D-2, D#2 */
	1357*AMIGA_FREQ_SCALE, 1281*AMIGA_FREQ_SCALE, 1209*AMIGA_FREQ_SCALE, 1141*AMIGA_FREQ_SCALE, /* E-2, F-2, F#2, G-2 */
	1077*AMIGA_FREQ_SCALE, 1017*AMIGA_FREQ_SCALE,  961*AMIGA_FREQ_SCALE,  907*AMIGA_FREQ_SCALE, /* G#2, A-2, A#2, B-2 */
	856*AMIGA_FREQ_SCALE,                                                                       /* C-3 */
};

static const float multi_retrig_add[] = {
	 0.f,  -1.f,  -2.f,  -4.f,  /* 0, 1, 2, 3 */
	-8.f, -16.f,   0.f,   0.f,  /* 4, 5, 6, 7 */
	 0.f,   1.f,   2.f,   4.f,  /* 8, 9, A, B */
	 8.f,  16.f,   0.f,   0.f   /* C, D, E, F */
};

static const float multi_retrig_multiply[] = {
	1.f,   1.f,  1.f,        1.f,  /* 0, 1, 2, 3 */
	1.f,   1.f,   .6666667f,  .5f, /* 4, 5, 6, 7 */
	1.f,   1.f,  1.f,        1.f,  /* 8, 9, A, B */
	1.f,   1.f,  1.5f,       2.f   /* C, D, E, F */
};

#define XM_CLAMP_UP1F(vol, limit) do {			\
		if((vol) > (limit)) (vol) = (limit);	\
	} while(0)
#define XM_CLAMP_UP(vol) XM_CLAMP_UP1F((vol), 1.f)

#define XM_CLAMP_DOWN1F(vol, limit) do {		\
		if((vol) < (limit)) (vol) = (limit);	\
	} while(0)
#define XM_CLAMP_DOWN(vol) XM_CLAMP_DOWN1F((vol), .0f)

#define XM_CLAMP2F(vol, up, down) do {			\
		if((vol) > (up)) (vol) = (up);			\
		else if((vol) < (down)) (vol) = (down); \
	} while(0)
#define XM_CLAMP(vol) XM_CLAMP2F((vol), 1.f, .0f)

#define XM_SLIDE_TOWARDS(val, goal, incr) do {		\
		if((val) > (goal)) {						\
			(val) -= (incr);						\
			XM_CLAMP_DOWN1F((val), (goal));			\
		} else if((val) < (goal)) {					\
			(val) += (incr);						\
			XM_CLAMP_UP1F((val), (goal));			\
		}											\
	} while(0)

#define XM_LERP(u, v, t) ((u) + (t) * ((v) - (u)))
#define XM_INVERSE_LERP(u, v, lerp) (((lerp) - (u)) / ((v) - (u)))

#define HAS_TONE_PORTAMENTO(s) ((s)->effect_type == 3 \
								 || (s)->effect_type == 5 \
								 || ((s)->volume_column >> 4) == 0xF)
#define HAS_ARPEGGIO(s) ((s)->effect_type == 0 \
						  && (s)->effect_param != 0)
#define HAS_VIBRATO(s) ((s)->effect_type == 4 \
						 || (s)->effect_type == 6 \
						 || ((s)->volume_column >> 4) == 0xB)
#define NOTE_IS_VALID(n) ((n) > 0 && (n) < 97)

/* ----- Function definitions ----- */

static float xm_waveform(xm_waveform_type_t waveform, uint8_t step) {
	static unsigned int next_rand = 24492;
	step %= 0x40;

	switch(waveform) {

	case XM_SINE_WAVEFORM:
		/* Why not use a table? For saving space, and because there's
		 * very very little actual performance gain. */
		return -sinf(2.f * 3.141592f * (float)step / (float)0x40);

	case XM_RAMP_DOWN_WAVEFORM:
		/* Ramp down: 1.0f when step = 0; -1.0f when step = 0x40 */
		return (float)(0x20 - step) / 0x20;

	case XM_SQUARE_WAVEFORM:
		/* Square with a 50% duty */
		return (step >= 0x20) ? 1.f : -1.f;

	case XM_RANDOM_WAVEFORM:
		/* Use the POSIX.1-2001 example, just to be deterministic
		 * across different machines */
		next_rand = next_rand * 1103515245 + 12345;
		return (float)((next_rand >> 16) & 0x7FFF) / (float)0x4000 - 1.f;

	case XM_RAMP_UP_WAVEFORM:
		/* Ramp up: -1.f when step = 0; 1.f when step = 0x40 */
		return (float)(step - 0x20) / 0x20;

	default:
		break;

	}

	return .0f;
}

static void xm_autovibrato(xm_context_t* ctx, xm_channel_context_t* ch) {
	if(ch->instrument == NULL || ch->instrument->vibrato_depth == 0){
		if (ch->autovibrato_note_offset){
			ch->autovibrato_note_offset = 0.f;
			xm_update_frequency(ctx, ch);
		}
		return;
	}
	xm_instrument_t* instr = ch->instrument;
	float sweep = 1.f;

	if(ch->autovibrato_ticks < instr->vibrato_sweep) {
		/* No idea if this is correct, but it sounds close enough… */
		sweep = XM_LERP(0.f, 1.f, (float)ch->autovibrato_ticks / (float)instr->vibrato_sweep);
	}

	unsigned int step = ((ch->autovibrato_ticks++) * instr->vibrato_rate) >> 2;
	ch->autovibrato_note_offset = .25f * xm_waveform(instr->vibrato_type, step)
		* (float)instr->vibrato_depth / (float)0xF * sweep;
	xm_update_frequency(ctx, ch);
}

static void xm_vibrato(xm_context_t* ctx, xm_channel_context_t* ch, uint8_t param) {
	ch->vibrato_ticks += (param >> 4);
	ch->vibrato_note_offset =
		-2.f
		* xm_waveform(ch->vibrato_waveform, ch->vibrato_ticks)
		* (float)(param & 0x0F) / (float)0xF;
	xm_update_frequency(ctx, ch);
}

static void xm_tremolo(xm_context_t* ctx, xm_channel_context_t* ch, uint8_t param, uint16_t pos) {
	unsigned int step = pos * (param >> 4);
	/* Not so sure about this, it sounds correct by ear compared with
	 * MilkyTracker, but it could come from other bugs */
	ch->tremolo_volume = -1.f * xm_waveform(ch->tremolo_waveform, step)
		* (float)(param & 0x0F) / (float)0xF;
}

static void xm_arpeggio(xm_context_t* ctx, xm_channel_context_t* ch, uint8_t param, uint16_t tick) {
	switch(tick % 3) {
	case 0:
		ch->arp_in_progress = false;
		ch->arp_note_offset = 0;
		break;
	case 2:
		ch->arp_in_progress = true;
		ch->arp_note_offset = param >> 4;
		break;
	case 1:
		ch->arp_in_progress = true;
		ch->arp_note_offset = param & 0x0F;
		break;
	}

	xm_update_frequency(ctx, ch);
}

static void xm_tone_portamento(xm_context_t* ctx, xm_channel_context_t* ch) {
	/* 3xx called without a note, wait until we get an actual
	 * target note. */
	if(ch->tone_portamento_target_period == 0.f) return;

	if(ch->period != ch->tone_portamento_target_period) {
		XM_SLIDE_TOWARDS(ch->period,
		                 ch->tone_portamento_target_period,
		                 (ctx->module.frequency_type == XM_LINEAR_FREQUENCIES ?
		                  4.f : 1.f) * ch->tone_portamento_param
		);
		xm_update_frequency(ctx, ch);
	}
}

static void xm_pitch_slide(xm_context_t* ctx, xm_channel_context_t* ch, float period_offset) {
	/* Don't ask about the 4.f coefficient. I found mention of it
	 * nowhere. Found by ear™. */
	if(ctx->module.frequency_type == XM_LINEAR_FREQUENCIES) {
		period_offset *= 4.f;
	}

	ch->period += period_offset;
	XM_CLAMP_DOWN(ch->period);
	/* XXX: upper bound of period ? */

	xm_update_frequency(ctx, ch);
}

static void xm_panning_slide(xm_channel_context_t* ch, uint8_t rawval) {
	float f;

	if((rawval & 0xF0) && (rawval & 0x0F)) {
		/* Illegal state */
		return;
	}

	if(rawval & 0xF0) {
		/* Slide right */
		f = (float)(rawval >> 4) / (float)0xFF;
		ch->panning += f;
		XM_CLAMP_UP(ch->panning);
	} else {
		/* Slide left */
		f = (float)(rawval & 0x0F) / (float)0xFF;
		ch->panning -= f;
		XM_CLAMP_DOWN(ch->panning);
	}
}

static void xm_volume_slide(xm_channel_context_t* ch, uint8_t rawval) {
	float f;

	if((rawval & 0xF0) && (rawval & 0x0F)) {
		/* Illegal state */
		return;
	}

	if(rawval & 0xF0) {
		/* Slide up */
		f = (float)(rawval >> 4) / (float)0x40;
		ch->volume += f;
		XM_CLAMP_UP(ch->volume);
	} else {
		/* Slide down */
		f = (float)(rawval & 0x0F) / (float)0x40;
		ch->volume -= f;
		XM_CLAMP_DOWN(ch->volume);
	}
}

static float xm_envelope_lerp(xm_envelope_point_t* restrict a, xm_envelope_point_t* restrict b, uint16_t pos) {
	/* Linear interpolation between two envelope points */
	if(pos <= a->frame) return a->value;
	else if(pos >= b->frame) return b->value;
	else {
		float p = (float)(pos - a->frame) / (float)(b->frame - a->frame);
		return a->value * (1 - p) + b->value * p;
	}
}

static void xm_post_pattern_change(xm_context_t* ctx) {
	/* Loop if necessary */
	if(ctx->current_table_index >= ctx->module.length) {
		ctx->current_table_index = ctx->module.restart_position;
	}
}

static float xm_linear_period(float note) {
	return 7680.f - note * 64.f;
}

static float xm_linear_frequency(float period) {
	return 8363.f * powf(2.f, (4608.f - period) / 768.f);
}

static float xm_amiga_period(float note) {
	unsigned int intnote = note;
	uint8_t a = intnote % 12;
	int8_t octave = note / 12.f - 2;
	int32_t p1 = amiga_frequencies[a], p2 = amiga_frequencies[a + 1];

	if(octave > 0) {
		p1 >>= octave;
		p2 >>= octave;
	} else if(octave < 0) {
		p1 <<= (-octave);
		p2 <<= (-octave);
	}

	return XM_LERP(p1, p2, note - intnote) / AMIGA_FREQ_SCALE;
}

static float xm_amiga_frequency(float period) {
	if(period == .0f) return .0f;

	/* This is the PAL value. No reason to choose this one over the
	 * NTSC value. */
	return 7093789.2f / (period * 2.f);
}

static float xm_period(xm_context_t* ctx, float note) {
	switch(ctx->module.frequency_type) {
	case XM_LINEAR_FREQUENCIES:
		return xm_linear_period(note);
	case XM_AMIGA_FREQUENCIES:
		return xm_amiga_period(note);
	}
	return .0f;
}

static float xm_frequency(xm_context_t* ctx, float period, float note_offset, float period_offset) {
	uint8_t a;
	int8_t octave;
	float note;
	int32_t p1, p2;

	switch(ctx->module.frequency_type) {

	case XM_LINEAR_FREQUENCIES:
		return xm_linear_frequency(period - 64.f * note_offset - 16.f * period_offset);

	case XM_AMIGA_FREQUENCIES:
		if(note_offset == 0) {
			/* A chance to escape from insanity */
			return xm_amiga_frequency(period + 16.f * period_offset);
		}

		/* FIXME: this is very crappy at best */
		a = octave = 0;

		/* Find the octave of the current period */
		period *= AMIGA_FREQ_SCALE;
		if(period > amiga_frequencies[0]) {
			--octave;
			while(period > (amiga_frequencies[0] << (-octave))) --octave;
		} else if(period < amiga_frequencies[12]) {
			++octave;
			while(period < (amiga_frequencies[12] >> octave)) ++octave;
		}

		/* Find the smallest note closest to the current period */
		for(uint8_t i = 0; i < 12; ++i) {
			p1 = amiga_frequencies[i], p2 = amiga_frequencies[i + 1];

			if(octave > 0) {
				p1 >>= octave;
				p2 >>= octave;
			} else if(octave < 0) {
				p1 <<= (-octave);
				p2 <<= (-octave);
			}

			if(p2 <= period && period <= p1) {
				a = i;
				break;
			}
		}

		if(XM_DEBUG && (p1 < period || p2 > period)) {
			DEBUG("%" PRId32 " <= %f <= %" PRId32 " should hold but doesn't, this is a bug", p2, period, p1);
		}

		note = 12.f * (octave + 2) + a + XM_INVERSE_LERP(p1, p2, period);

		return xm_amiga_frequency(xm_amiga_period(note + note_offset) + 16.f * period_offset);

	}

	return .0f;
}

static void xm_update_frequency(xm_context_t* ctx, xm_channel_context_t* ch) {
	ch->frequency = xm_frequency(
		ctx, ch->period,
		ch->arp_note_offset,
		ch->vibrato_note_offset + ch->autovibrato_note_offset
	);
	ch->step = ch->frequency / ctx->rate;
}

static void xm_handle_note_and_instrument(xm_context_t* ctx, xm_channel_context_t* ch,
										  xm_pattern_slot_t* s) {
	if(s->instrument > 0) {
		if(HAS_TONE_PORTAMENTO(ch->current) && ch->instrument != NULL && ch->sample != NULL) {
			/* Tone portamento in effect, unclear stuff happens */
			xm_trigger_note(ctx, ch, XM_TRIGGER_KEEP_PERIOD | XM_TRIGGER_KEEP_SAMPLE_POSITION);
		} else if(s->note == 0 && ch->sample != NULL) {
			/* Ghost instrument, trigger note */
			/* Sample position is kept, but envelopes are reset */
			xm_trigger_note(ctx, ch, XM_TRIGGER_KEEP_SAMPLE_POSITION);
		} else if(s->instrument > ctx->module.num_instruments) {
			/* Invalid instrument, Cut current note */
			xm_cut_note(ch);
			ch->instrument = NULL;
			ch->sample = NULL;
		} else {
			ch->instrument = ctx->module.instruments + (s->instrument - 1);
		}
	}

	if(NOTE_IS_VALID(s->note)) {
		/* Yes, the real note number is s->note -1. Try finding
		 * THAT in any of the specs! :-) */

		xm_instrument_t* instr = ch->instrument;

		if(HAS_TONE_PORTAMENTO(ch->current) && instr != NULL && ch->sample != NULL) {
			/* Tone portamento in effect */
			ch->note = s->note + ch->sample->relative_note + ch->sample->finetune / 128.f - 1.f;
			ch->tone_portamento_target_period = xm_period(ctx, ch->note);
		} else if(instr == NULL || ch->instrument->num_samples == 0) {
			/* Bad instrument */
			xm_cut_note(ch);
		} else {
			if(instr->sample_of_notes[s->note - 1] < instr->num_samples) {
#if XM_RAMPING
				for(unsigned int z = 0; z < XM_SAMPLE_RAMPING_POINTS; ++z) {
					ch->end_of_previous_sample[z] = xm_next_of_sample(ch);
				}
				ch->frame_count = 0;
#endif
				ch->sample = instr->samples + instr->sample_of_notes[s->note - 1];
				ch->orig_note = ch->note = s->note + ch->sample->relative_note
					+ ch->sample->finetune / 128.f - 1.f;
				if(s->instrument > 0) {
					xm_trigger_note(ctx, ch, 0);
				} else {
					/* Ghost note: keep old volume */
					xm_trigger_note(ctx, ch, XM_TRIGGER_KEEP_VOLUME);
				}
			} else {
				/* Bad sample */
				xm_cut_note(ch);
			}
		}
	} else if(s->note == 97) {
		/* Key Off */
		xm_key_off(ch);
	}

	switch(s->volume_column >> 4) {

	case 0x5:
		if(s->volume_column > 0x50) break;
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
		/* Set volume */
		ch->volume = (float)(s->volume_column - 0x10) / (float)0x40;
		break;

	case 0x8: /* Fine volume slide down */
		xm_volume_slide(ch, s->volume_column & 0x0F);
		break;

	case 0x9: /* Fine volume slide up */
		xm_volume_slide(ch, s->volume_column << 4);
		break;

	case 0xA: /* Set vibrato speed */
		ch->vibrato_param = (ch->vibrato_param & 0x0F) | ((s->volume_column & 0x0F) << 4);
		break;

	case 0xC: /* Set panning */
		ch->panning = (float)(
			((s->volume_column & 0x0F) << 4) | (s->volume_column & 0x0F)
			) / (float)0xFF;
		break;

	case 0xF: /* Tone portamento */
		if(s->volume_column & 0x0F) {
			ch->tone_portamento_param = ((s->volume_column & 0x0F) << 4)
				| (s->volume_column & 0x0F);
		}
		break;

	default:
		break;

	}

	switch(s->effect_type) {

	case 1: /* 1xx: Portamento up */
		if(s->effect_param > 0) {
			ch->portamento_up_param = s->effect_param;
		}
		break;

	case 2: /* 2xx: Portamento down */
		if(s->effect_param > 0) {
			ch->portamento_down_param = s->effect_param;
		}
		break;

	case 3: /* 3xx: Tone portamento */
		if(s->effect_param > 0) {
			ch->tone_portamento_param = s->effect_param;
		}
		break;

	case 4: /* 4xy: Vibrato */
		if(s->effect_param & 0x0F) {
			/* Set vibrato depth */
			ch->vibrato_param = (ch->vibrato_param & 0xF0) | (s->effect_param & 0x0F);
		}
		if(s->effect_param >> 4) {
			/* Set vibrato speed */
			ch->vibrato_param = (s->effect_param & 0xF0) | (ch->vibrato_param & 0x0F);
		}
		break;

	case 5: /* 5xy: Tone portamento + Volume slide */
		if(s->effect_param > 0) {
			ch->volume_slide_param = s->effect_param;
		}
		break;

	case 6: /* 6xy: Vibrato + Volume slide */
		if(s->effect_param > 0) {
			ch->volume_slide_param = s->effect_param;
		}
		break;

	case 7: /* 7xy: Tremolo */
		if(s->effect_param & 0x0F) {
			/* Set tremolo depth */
			ch->tremolo_param = (ch->tremolo_param & 0xF0) | (s->effect_param & 0x0F);
		}
		if(s->effect_param >> 4) {
			/* Set tremolo speed */
			ch->tremolo_param = (s->effect_param & 0xF0) | (ch->tremolo_param & 0x0F);
		}
		break;

	case 8: /* 8xx: Set panning */
		ch->panning = (float)s->effect_param / (float)0xFF;
		break;

	case 9: /* 9xx: Sample offset */
		if(ch->sample != NULL && NOTE_IS_VALID(s->note)) {
			uint32_t final_offset = s->effect_param << (ch->sample->bits == 16 ? 7 : 8);
			if(final_offset >= ch->sample->length) {
				/* Pretend the sample dosen't loop and is done playing */
				ch->sample_position = -1;
				break;
			}
			ch->sample_position = final_offset;
		}
		break;

	case 0xA: /* Axy: Volume slide */
		if(s->effect_param > 0) {
			ch->volume_slide_param = s->effect_param;
		}
		break;

	case 0xB: /* Bxx: Position jump */
		if(s->effect_param < ctx->module.length) {
			ctx->position_jump = true;
			ctx->jump_dest = s->effect_param;
			ctx->jump_row = 0;
		}
		break;

	case 0xC: /* Cxx: Set volume */
		ch->volume = (float)((s->effect_param > 0x40)
							 ? 0x40 : s->effect_param) / (float)0x40;
		break;

	case 0xD: /* Dxx: Pattern break */
		/* Jump after playing this line */
		ctx->pattern_break = true;
		ctx->jump_row = (s->effect_param >> 4) * 10 + (s->effect_param & 0x0F);
		break;

	case 0xE: /* EXy: Extended command */
		switch(s->effect_param >> 4) {

		case 1: /* E1y: Fine portamento up */
			if(s->effect_param & 0x0F) {
				ch->fine_portamento_up_param = s->effect_param & 0x0F;
			}
			xm_pitch_slide(ctx, ch, -ch->fine_portamento_up_param);
			break;

		case 2: /* E2y: Fine portamento down */
			if(s->effect_param & 0x0F) {
				ch->fine_portamento_down_param = s->effect_param & 0x0F;
			}
			xm_pitch_slide(ctx, ch, ch->fine_portamento_down_param);
			break;

		case 4: /* E4y: Set vibrato control */
			ch->vibrato_waveform = s->effect_param & 3;
			ch->vibrato_waveform_retrigger = !((s->effect_param >> 2) & 1);
			break;

		case 5: /* E5y: Set finetune */
			if(NOTE_IS_VALID(ch->current->note) && ch->sample != NULL) {
				ch->note = ch->current->note + ch->sample->relative_note +
					(float)(((s->effect_param & 0x0F) - 8) << 4) / 128.f - 1.f;
				ch->period = xm_period(ctx, ch->note);
				xm_update_frequency(ctx, ch);
			}
			break;

		case 6: /* E6y: Pattern loop */
			if(s->effect_param & 0x0F) {
				if((s->effect_param & 0x0F) == ch->pattern_loop_count) {
					/* Loop is over */
					ch->pattern_loop_count = 0;
					break;
				}

				/* Jump to the beginning of the loop */
				ch->pattern_loop_count++;
				ctx->position_jump = true;
				ctx->jump_row = ch->pattern_loop_origin;
				ctx->jump_dest = ctx->current_table_index;
			} else {
				/* Set loop start point */
				ch->pattern_loop_origin = ctx->current_row;
				/* Replicate FT2 E60 bug */
				ctx->jump_row = ch->pattern_loop_origin;
			}
			break;

		case 7: /* E7y: Set tremolo control */
			ch->tremolo_waveform = s->effect_param & 3;
			ch->tremolo_waveform_retrigger = !((s->effect_param >> 2) & 1);
			break;

		case 0xA: /* EAy: Fine volume slide up */
			if(s->effect_param & 0x0F) {
				ch->fine_volume_slide_param = s->effect_param & 0x0F;
			}
			xm_volume_slide(ch, ch->fine_volume_slide_param << 4);
			break;

		case 0xB: /* EBy: Fine volume slide down */
			if(s->effect_param & 0x0F) {
				ch->fine_volume_slide_param = s->effect_param & 0x0F;
			}
			xm_volume_slide(ch, ch->fine_volume_slide_param);
			break;

		case 0xD: /* EDy: Note delay */
			/* XXX: figure this out better. EDx triggers
			 * the note even when there no note and no
			 * instrument. But ED0 acts like like a ghost
			 * note, EDx (x ≠ 0) does not. */
			if(s->note == 0 && s->instrument == 0) {
				unsigned int flags = XM_TRIGGER_KEEP_VOLUME;

				if(ch->current->effect_param & 0x0F) {
					ch->note = ch->orig_note;
					xm_trigger_note(ctx, ch, flags);
				} else {
					xm_trigger_note(
						ctx, ch,
						flags
						| XM_TRIGGER_KEEP_PERIOD
						| XM_TRIGGER_KEEP_SAMPLE_POSITION
						);
				}
			}
			break;

		case 0xE: /* EEy: Pattern delay */
			ctx->extra_ticks = (ch->current->effect_param & 0x0F) * ctx->tempo;
			break;

		default:
			break;

		}
		break;

	case 0xF: /* Fxx: Set tempo/BPM */
		if(s->effect_param > 0) {
			if(s->effect_param <= 0x1F) {
				ctx->tempo = s->effect_param;
			} else {
				ctx->bpm = s->effect_param;
			}
		}
		break;

	case 16: /* Gxx: Set global volume */
		ctx->global_volume = (float)((s->effect_param > 0x40)
									 ? 0x40 : s->effect_param) / (float)0x40;
		break;

	case 17: /* Hxy: Global volume slide */
		if(s->effect_param > 0) {
			ch->global_volume_slide_param = s->effect_param;
		}
		break;

	case 21: /* Lxx: Set envelope position */
		ch->volume_envelope_frame_count = s->effect_param;
		ch->panning_envelope_frame_count = s->effect_param;
		break;

	case 25: /* Pxy: Panning slide */
		if(s->effect_param > 0) {
			ch->panning_slide_param = s->effect_param;
		}
		break;

	case 27: /* Rxy: Multi retrig note */
		if(s->effect_param > 0) {
			if((s->effect_param >> 4) == 0) {
				/* Keep previous x value */
				ch->multi_retrig_param = (ch->multi_retrig_param & 0xF0) | (s->effect_param & 0x0F);
			} else {
				ch->multi_retrig_param = s->effect_param;
			}
		}
		break;

	case 29: /* Txy: Tremor */
		if(s->effect_param > 0) {
			/* Tremor x and y params do not appear to be separately
			 * kept in memory, unlike Rxy */
			ch->tremor_param = s->effect_param;
		}
		break;

	case 33: /* Xxy: Extra stuff */
		switch(s->effect_param >> 4) {

		case 1: /* X1y: Extra fine portamento up */
			if(s->effect_param & 0x0F) {
				ch->extra_fine_portamento_up_param = s->effect_param & 0x0F;
			}
			xm_pitch_slide(ctx, ch, -1.0f * ch->extra_fine_portamento_up_param);
			break;

		case 2: /* X2y: Extra fine portamento down */
			if(s->effect_param & 0x0F) {
				ch->extra_fine_portamento_down_param = s->effect_param & 0x0F;
			}
			xm_pitch_slide(ctx, ch, ch->extra_fine_portamento_down_param);
			break;

		default:
			break;

		}
		break;

	default:
		break;

	}
}

static void xm_trigger_note(xm_context_t* ctx, xm_channel_context_t* ch, unsigned int flags) {
	if(!(flags & XM_TRIGGER_KEEP_SAMPLE_POSITION)) {
		ch->sample_position = 0.f;
		ch->ping = true;
	}

	if(ch->sample != NULL) {
		if(!(flags & XM_TRIGGER_KEEP_VOLUME)) {
			ch->volume = ch->sample->volume;
		}

		ch->panning = ch->sample->panning;
	}

	if(!(flags & XM_TRIGGER_KEEP_ENVELOPE)) {
		ch->sustained = true;
		ch->fadeout_volume = ch->volume_envelope_volume = 1.0f;
		ch->panning_envelope_panning = .5f;
		ch->volume_envelope_frame_count = ch->panning_envelope_frame_count = 0;
	}
	ch->vibrato_note_offset = 0.f;
	ch->tremolo_volume = 0.f;
	ch->tremor_on = false;

	ch->autovibrato_ticks = 0;

	if(ch->vibrato_waveform_retrigger) {
		ch->vibrato_ticks = 0; /* XXX: should the waveform itself also
								* be reset to sine? */
	}
	if(ch->tremolo_waveform_retrigger) {
		ch->tremolo_ticks = 0;
	}

	if(!(flags & XM_TRIGGER_KEEP_PERIOD)) {
		ch->period = xm_period(ctx, ch->note);
		xm_update_frequency(ctx, ch);
	}

	ch->latest_trigger = ctx->generated_samples;
	if(ch->instrument != NULL) {
		ch->instrument->latest_trigger = ctx->generated_samples;
	}
	if(ch->sample != NULL) {
		ch->sample->latest_trigger = ctx->generated_samples;
	}
}

static void xm_cut_note(xm_channel_context_t* ch) {
	/* NB: this is not the same as Key Off */
	ch->volume = .0f;
}

static void xm_key_off(xm_channel_context_t* ch) {
	/* Key Off */
	ch->sustained = false;

	/* If no volume envelope is used, also cut the note */
	if(ch->instrument == NULL || !ch->instrument->volume_envelope.enabled) {
		xm_cut_note(ch);
	}
}

static void xm_row(xm_context_t* ctx) {
	if(ctx->position_jump) {
		ctx->current_table_index = ctx->jump_dest;
		ctx->current_row = ctx->jump_row;
		ctx->position_jump = false;
		ctx->pattern_break = false;
		ctx->jump_row = 0;
		xm_post_pattern_change(ctx);
	} else if(ctx->pattern_break) {
		ctx->current_table_index++;
		ctx->current_row = ctx->jump_row;
		ctx->pattern_break = false;
		ctx->jump_row = 0;
		xm_post_pattern_change(ctx);
	}

	xm_pattern_t* cur = ctx->module.patterns + ctx->module.pattern_table[ctx->current_table_index];
	bool in_a_loop = false;

	/* Read notes… */
	for(uint8_t i = 0; i < ctx->module.num_channels; ++i) {
		xm_pattern_slot_t* s = cur->slots + ctx->current_row * ctx->module.num_channels + i;
		xm_channel_context_t* ch = ctx->channels + i;

		ch->current = s;

		if(s->effect_type != 0xE || s->effect_param >> 4 != 0xD) {
			xm_handle_note_and_instrument(ctx, ch, s);
		} else {
			ch->note_delay_param = s->effect_param & 0x0F;
		}

		if(!in_a_loop && ch->pattern_loop_count > 0) {
			in_a_loop = true;
		}
	}

	if(!in_a_loop) {
		/* No E6y loop is in effect (or we are in the first pass) */
		ctx->loop_count = (ctx->row_loop_count[MAX_NUM_ROWS * ctx->current_table_index + ctx->current_row]++);
	}

	ctx->current_row++; /* Since this is an uint8, this line can
						 * increment from 255 to 0, in which case it
						 * is still necessary to go the next
						 * pattern. */
	if(!ctx->position_jump && !ctx->pattern_break &&
	   (ctx->current_row >= cur->num_rows || ctx->current_row == 0)) {
		ctx->current_table_index++;
		ctx->current_row = ctx->jump_row; /* This will be 0 most of
										   * the time, except when E60
										   * is used */
		ctx->jump_row = 0;
		xm_post_pattern_change(ctx);
	}
}

static void xm_envelope_tick(xm_channel_context_t* ch,
							 xm_envelope_t* env,
							 uint16_t* counter,
							 float* outval) {
	if(env->num_points < 2) {
		/* Don't really know what to do… */
		if(env->num_points == 1) {
			/* XXX I am pulling this out of my ass */
			*outval = (float)env->points[0].value / (float)0x40;
			if(*outval > 1) {
				*outval = 1;
			}
		}

		return;
	} else {
		uint8_t j;

		if(env->loop_enabled) {
			uint16_t loop_start = env->points[env->loop_start_point].frame;
			uint16_t loop_end = env->points[env->loop_end_point].frame;
			uint16_t loop_length = loop_end - loop_start;

			if(*counter >= loop_end) {
				*counter -= loop_length;
			}
		}

		for(j = 0; j < (env->num_points - 2); ++j) {
			if(env->points[j].frame <= *counter &&
			   env->points[j+1].frame >= *counter) {
				break;
			}
		}

		*outval = xm_envelope_lerp(env->points + j, env->points + j + 1, *counter) / (float)0x40;

		/* Make sure it is safe to increment frame count */
		if(!ch->sustained || !env->sustain_enabled ||
		   *counter != env->points[env->sustain_point].frame) {
			(*counter)++;
		}
	}
}

static void xm_envelopes(xm_channel_context_t* ch) {
	if(ch->instrument != NULL) {
		if(ch->instrument->volume_envelope.enabled) {
			if(!ch->sustained) {
				ch->fadeout_volume -= (float)ch->instrument->volume_fadeout / 32768.f;
				XM_CLAMP_DOWN(ch->fadeout_volume);
			}

			xm_envelope_tick(ch,
							 &(ch->instrument->volume_envelope),
							 &(ch->volume_envelope_frame_count),
							 &(ch->volume_envelope_volume));
		}

		if(ch->instrument->panning_envelope.enabled) {
			xm_envelope_tick(ch,
							 &(ch->instrument->panning_envelope),
							 &(ch->panning_envelope_frame_count),
							 &(ch->panning_envelope_panning));
		}
	}
}

static void xm_tick(xm_context_t* ctx) {
	if(ctx->current_tick == 0) {
		xm_row(ctx);
	}

	for(uint8_t i = 0; i < ctx->module.num_channels; ++i) {
		xm_channel_context_t* ch = ctx->channels + i;

		xm_envelopes(ch);
		xm_autovibrato(ctx, ch);

		if(ch->arp_in_progress && !HAS_ARPEGGIO(ch->current)) {
			ch->arp_in_progress = false;
			ch->arp_note_offset = 0;
			xm_update_frequency(ctx, ch);
		}
		if(ch->vibrato_in_progress && !HAS_VIBRATO(ch->current)) {
			ch->vibrato_in_progress = false;
			ch->vibrato_note_offset = 0.f;
			xm_update_frequency(ctx, ch);
		}

		switch(ch->current->volume_column >> 4) {

		case 0x6: /* Volume slide down */
			if(ctx->current_tick == 0) break;
			xm_volume_slide(ch, ch->current->volume_column & 0x0F);
			break;

		case 0x7: /* Volume slide up */
			if(ctx->current_tick == 0) break;
			xm_volume_slide(ch, ch->current->volume_column << 4);
			break;

		case 0xB: /* Vibrato */
			if(ctx->current_tick == 0) break;
			ch->vibrato_in_progress = false;
			xm_vibrato(ctx, ch, ch->vibrato_param);
			break;

		case 0xD: /* Panning slide left */
			if(ctx->current_tick == 0) break;
			xm_panning_slide(ch, ch->current->volume_column & 0x0F);
			break;

		case 0xE: /* Panning slide right */
			if(ctx->current_tick == 0) break;
			xm_panning_slide(ch, ch->current->volume_column << 4);
			break;

		case 0xF: /* Tone portamento */
			if(ctx->current_tick == 0) break;
			xm_tone_portamento(ctx, ch);
			break;

		default:
			break;

		}

		switch(ch->current->effect_type) {

		case 0: /* 0xy: Arpeggio */
			if(ch->current->effect_param > 0) {
				char arp_offset = ctx->tempo % 3;
				switch(arp_offset) {
				case 2: /* 0 -> x -> 0 -> y -> x -> … */
					if(ctx->current_tick == 1) {
						ch->arp_in_progress = true;
						ch->arp_note_offset = ch->current->effect_param >> 4;
						xm_update_frequency(ctx, ch);
						break;
					}
					/* No break here, this is intended */
				case 1: /* 0 -> 0 -> y -> x -> … */
					if(ctx->current_tick == 0) {
						ch->arp_in_progress = false;
						ch->arp_note_offset = 0;
						xm_update_frequency(ctx, ch);
						break;
					}
					/* No break here, this is intended */
				case 0: /* 0 -> y -> x -> … */
					xm_arpeggio(ctx, ch, ch->current->effect_param, ctx->current_tick - arp_offset);
				default:
					break;
				}
			}
			break;

		case 1: /* 1xx: Portamento up */
			if(ctx->current_tick == 0) break;
			xm_pitch_slide(ctx, ch, -ch->portamento_up_param);
			break;

		case 2: /* 2xx: Portamento down */
			if(ctx->current_tick == 0) break;
			xm_pitch_slide(ctx, ch, ch->portamento_down_param);
			break;

		case 3: /* 3xx: Tone portamento */
			if(ctx->current_tick == 0) break;
			xm_tone_portamento(ctx, ch);
			break;

		case 4: /* 4xy: Vibrato */
			if(ctx->current_tick == 0) break;
			ch->vibrato_in_progress = true;
			xm_vibrato(ctx, ch, ch->vibrato_param);
			break;

		case 5: /* 5xy: Tone portamento + Volume slide */
			if(ctx->current_tick == 0) break;
			xm_tone_portamento(ctx, ch);
			xm_volume_slide(ch, ch->volume_slide_param);
			break;

		case 6: /* 6xy: Vibrato + Volume slide */
			if(ctx->current_tick == 0) break;
			ch->vibrato_in_progress = true;
			xm_vibrato(ctx, ch, ch->vibrato_param);
			xm_volume_slide(ch, ch->volume_slide_param);
			break;

		case 7: /* 7xy: Tremolo */
			if(ctx->current_tick == 0) break;
			xm_tremolo(ctx, ch, ch->tremolo_param, ch->tremolo_ticks++);
			break;

		case 0xA: /* Axy: Volume slide */
			if(ctx->current_tick == 0) break;
			xm_volume_slide(ch, ch->volume_slide_param);
			break;

		case 0xE: /* EXy: Extended command */
			switch(ch->current->effect_param >> 4) {

			case 0x9: /* E9y: Retrigger note */
				if(ctx->current_tick != 0 && ch->current->effect_param & 0x0F) {
					if(!(ctx->current_tick % (ch->current->effect_param & 0x0F))) {
						xm_trigger_note(ctx, ch, XM_TRIGGER_KEEP_VOLUME);
						xm_envelopes(ch);
					}
				}
				break;

			case 0xC: /* ECy: Note cut */
				if((ch->current->effect_param & 0x0F) == ctx->current_tick) {
				    xm_cut_note(ch);
				}
				break;

			case 0xD: /* EDy: Note delay */
				if(ch->note_delay_param == ctx->current_tick) {
					xm_handle_note_and_instrument(ctx, ch, ch->current);
					xm_envelopes(ch);
				}
				break;

			default:
				break;

			}
			break;

		case 17: /* Hxy: Global volume slide */
			if(ctx->current_tick == 0) break;
			if((ch->global_volume_slide_param & 0xF0) &&
			   (ch->global_volume_slide_param & 0x0F)) {
				/* Illegal state */
				break;
			}
			if(ch->global_volume_slide_param & 0xF0) {
				/* Global slide up */
				float f = (float)(ch->global_volume_slide_param >> 4) / (float)0x40;
				ctx->global_volume += f;
				XM_CLAMP_UP(ctx->global_volume);
			} else {
				/* Global slide down */
				float f = (float)(ch->global_volume_slide_param & 0x0F) / (float)0x40;
				ctx->global_volume -= f;
				XM_CLAMP_DOWN(ctx->global_volume);
			}
			break;

		case 20: /* Kxx: Key off */
			/* Most documentations will tell you the parameter has no
			 * use. Don't be fooled. */
			if(ctx->current_tick == ch->current->effect_param) {
				xm_key_off(ch);
			}
			break;

		case 25: /* Pxy: Panning slide */
			if(ctx->current_tick == 0) break;
			xm_panning_slide(ch, ch->panning_slide_param);
			break;

		case 27: /* Rxy: Multi retrig note */
			if(ctx->current_tick == 0) break;
			if(((ch->multi_retrig_param) & 0x0F) == 0) break;
			if((ctx->current_tick % (ch->multi_retrig_param & 0x0F)) == 0) {
				xm_trigger_note(ctx, ch, XM_TRIGGER_KEEP_VOLUME | XM_TRIGGER_KEEP_ENVELOPE);

				/* Rxy doesn't affect volume if there's a command in the volume
				   column, or if the instrument has a volume envelope. */
				if (!ch->current->volume_column && !ch->instrument->volume_envelope.enabled){
					float v = ch->volume * multi_retrig_multiply[ch->multi_retrig_param >> 4]
						+ multi_retrig_add[ch->multi_retrig_param >> 4] / (float)0x40;
					XM_CLAMP(v);
					ch->volume = v;
				}
			}
			break;

		case 29: /* Txy: Tremor */
			if(ctx->current_tick == 0) break;
			ch->tremor_on = (
				(ctx->current_tick - 1) % ((ch->tremor_param >> 4) + (ch->tremor_param & 0x0F) + 2)
				>
				(ch->tremor_param >> 4)
			);
			break;

		default:
			break;

		}

		float panning, volume;

		panning = ch->panning +
			(ch->panning_envelope_panning - .5f) * (.5f - fabsf(ch->panning - .5f)) * 2.0f;

		if(ch->tremor_on) {
		        volume = .0f;
		} else {
			volume = ch->volume + ch->tremolo_volume;
			XM_CLAMP(volume);
			volume *= ch->fadeout_volume * ch->volume_envelope_volume;
		}

#if XM_RAMPING
		/* See https://modarchive.org/forums/index.php?topic=3517.0
		 * and https://github.com/Artefact2/libxm/pull/16 */
		ch->target_volume[0] = volume * sqrt(1.f - panning);
		ch->target_volume[1] = volume * sqrt(panning);
#else
		ch->actual_volume[0] = volume * sqrt(1.f - panning);
		ch->actual_volume[1] = volume * sqrt(panning);
#endif
	}

	ctx->current_tick++;
	if(ctx->current_tick >= ctx->tempo + ctx->extra_ticks) {
		ctx->current_tick = 0;
		ctx->extra_ticks = 0;
	}

	/* FT2 manual says number of ticks / second = BPM * 0.4 */
	ctx->remaining_samples_in_tick += (float)ctx->rate / ((float)ctx->bpm * 0.4f);
}

static float xm_sample_at(xm_sample_t* sample, size_t k) {
	return sample->bits == 8 ? (sample->data8[k] / 128.f) : (sample->data16[k] / 32768.f);
}

static float xm_next_of_sample(xm_channel_context_t* ch) {
	if(ch->instrument == NULL || ch->sample == NULL || ch->sample_position < 0) {
#if XM_RAMPING
		if(ch->frame_count < XM_SAMPLE_RAMPING_POINTS) {
			return XM_LERP(ch->end_of_previous_sample[ch->frame_count], .0f,
			               (float)ch->frame_count / (float)XM_SAMPLE_RAMPING_POINTS);
		}
#endif
		return .0f;
	}
	if(ch->sample->length == 0) {
		return .0f;
	}

	float u, v, t;
	uint32_t a, b;
	a = (uint32_t)ch->sample_position; /* This cast is fine,
										* sample_position will not
										* go above integer
										* ranges */
	if(XM_LINEAR_INTERPOLATION) {
		b = a + 1;
		t = ch->sample_position - a; /* Cheaper than fmodf(., 1.f) */
	}
	u = xm_sample_at(ch->sample, a);

	switch(ch->sample->loop_type) {

	case XM_NO_LOOP:
		if(XM_LINEAR_INTERPOLATION) {
			v = (b < ch->sample->length) ? xm_sample_at(ch->sample, b) : .0f;
		}
		ch->sample_position += ch->step;
		if(ch->sample_position >= ch->sample->length) {
			ch->sample_position = -1;
		}
		break;

	case XM_FORWARD_LOOP:
		if(XM_LINEAR_INTERPOLATION) {
			v = xm_sample_at(
				ch->sample,
				(b == ch->sample->loop_end) ? ch->sample->loop_start : b
				);
		}
		ch->sample_position += ch->step;
		while(ch->sample_position >= ch->sample->loop_end) {
			ch->sample_position -= ch->sample->loop_length;
		}
		break;

	case XM_PING_PONG_LOOP:
		if(ch->ping) {
			ch->sample_position += ch->step;
		} else {
			ch->sample_position -= ch->step;
		}
		/* XXX: this may not work for very tight ping-pong loops
		 * (ie switches direction more than once per sample */
		if(ch->ping) {
			if(XM_LINEAR_INTERPOLATION) {
				v = xm_sample_at(ch->sample, (b >= ch->sample->loop_end) ? a : b);
			}
			if(ch->sample_position >= ch->sample->loop_end) {
				ch->ping = false;
				ch->sample_position = (ch->sample->loop_end << 1) - ch->sample_position;
			}
			/* sanity checking */
			if(ch->sample_position >= ch->sample->length) {
				ch->ping = false;
				ch->sample_position -= ch->sample->length - 1;
			}
		} else {
			if(XM_LINEAR_INTERPOLATION) {
				v = u;
				u = xm_sample_at(
					ch->sample,
					(b == 1 || b - 2 <= ch->sample->loop_start) ? a : (b - 2)
					);
			}
			if(ch->sample_position <= ch->sample->loop_start) {
				ch->ping = true;
				ch->sample_position = (ch->sample->loop_start << 1) - ch->sample_position;
			}
			/* sanity checking */
			if(ch->sample_position <= .0f) {
				ch->ping = true;
				ch->sample_position = .0f;
			}
		}
		break;

	default:
		v = .0f;
		break;
	}

	float endval = (XM_LINEAR_INTERPOLATION ? XM_LERP(u, v, t) : u);

#if XM_RAMPING
	if(ch->frame_count < XM_SAMPLE_RAMPING_POINTS) {
		/* Smoothly transition between old and new sample. */
		return XM_LERP(ch->end_of_previous_sample[ch->frame_count], endval,
		               (float)ch->frame_count / (float)XM_SAMPLE_RAMPING_POINTS);
	}
#endif

	return endval;
}

static void xm_sample(xm_context_t* ctx, float* left, float* right) {
	if(ctx->remaining_samples_in_tick <= 0) {
		xm_tick(ctx);
	}
	ctx->remaining_samples_in_tick--;

	*left = 0.f;
	*right = 0.f;

	if(ctx->max_loop_count > 0 && ctx->loop_count >= ctx->max_loop_count) {
		return;
	}

	for(uint8_t i = 0; i < ctx->module.num_channels; ++i) {
		xm_channel_context_t* ch = ctx->channels + i;

		if(ch->instrument == NULL || ch->sample == NULL || ch->sample_position < 0) {
			continue;
		}

		const float fval = xm_next_of_sample(ch);

		if(!ch->muted && !ch->instrument->muted) {
			*left += fval * ch->actual_volume[0];
			*right += fval * ch->actual_volume[1];
		}

#if XM_RAMPING
		ch->frame_count++;
		XM_SLIDE_TOWARDS(ch->actual_volume[0], ch->target_volume[0], ctx->volume_ramp);
		XM_SLIDE_TOWARDS(ch->actual_volume[1], ch->target_volume[1], ctx->volume_ramp);
#endif
	}

	const float fgvol = ctx->global_volume * ctx->amplification;
	*left *= fgvol;
	*right *= fgvol;

	if(XM_DEBUG) {
		if(fabs(*left) > 1 || fabs(*right) > 1) {
			DEBUG("clipping frame: %f %f, this is a bad module or a libxm bug", *left, *right);
		}
	}
}

void xm_generate_samples(xm_context_t* ctx, float* output, size_t numsamples) {
	ctx->generated_samples += numsamples;

	for(size_t i = 0; i < numsamples; i++) {
		xm_sample(ctx, output + (2 * i), output + (2 * i + 1));
	}
}
