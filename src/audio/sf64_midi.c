/**
 * @file sf64_midi.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief MIDI control plane for the SF64 synthesizer
 * @ingroup mixer
 *
 * Implements #midi_target_t callbacks, per-channel controllers, Channel Mode
 * Messages (CC120/121/123), RPN pitch-bend sensitivity, bank select, Native/GM1
 * mode, and GM1 System On. Voice lifecycle stays in sf64_synth.c.
 */
#include "sf64_synth_internal.h"
#include "debug.h"
#include <assert.h>
#include <limits.h>

static void midi_all_sound_off(sf64_synth_t *s, int midi_channel)
{
	if (s->num_channels <= 0)
		return;
	for (int ch = s->first_channel; ch < s->first_channel + s->num_channels; ch++) {
		if (s->voices[ch].midi_channel == midi_channel)
			voice_stop(s, ch);
	}
}

static void midi_all_notes_off(sf64_synth_t *s, int midi_channel)
{
	if (s->num_channels <= 0)
		return;
	bool pedal = s->midi[midi_channel].sustain >= 64;
	for (int ch = s->first_channel; ch < s->first_channel + s->num_channels; ch++) {
		sf64_voice_t *v = &s->voices[ch];
		if (v->midi_channel != midi_channel || !voice_key_down(v))
			continue;
		if (pedal) {
			v->key_released = true;
			v->held_by_sustain = true;
		} else {
			voice_enter_release(s, ch);
		}
	}
}

static void midi_rpn_apply_data_entry(sf64_synth_t *s, int midi_channel)
{
	sf64_midi_channel_t *mc = &s->midi[midi_channel];
	if (mc->rpn_msb != 0 || mc->rpn_lsb != 0)
		return;
	int cents = (int)mc->data_entry_msb * 100 + (int)mc->data_entry_lsb;
	if (cents > 12700) cents = 12700;
	mc->pitch_range_cents = (int16_t)cents;
	if (s->num_channels > 0)
		midi_apply_bend(s, midi_channel);
}

static void midi_channel_reset_program(sf64_synth_t *s, int midi_channel)
{
	sf64_midi_channel_t *mc = &s->midi[midi_channel];
	mc->bank = (midi_channel == SF64_DRUM_CHANNEL) ? SF64_DRUM_BANK : 0;
	mc->bank_msb = 0;
	mc->bank_lsb = 0;
	mc->program = 0;
	mc->preset_index = (int16_t)sf64_find_preset(s->bank, mc->bank, 0);
}

static void midi_channel_reset_controllers(sf64_synth_t *s, int midi_channel)
{
	sf64_midi_channel_t *mc = &s->midi[midi_channel];
	bool pedal = mc->sustain >= 64;
	mc->expression = 127;
	mc->pan = 64;
	mc->modulation = 0;
	mc->sustain = 0;
	mc->rpn_msb = 0x7F;
	mc->rpn_lsb = 0x7F;
	mc->data_entry_msb = 0;
	mc->data_entry_lsb = 0;
	mc->pitch_bend = 8192;
	mc->pitch_range_cents = 200;
	if (s->num_channels <= 0)
		return;
	if (pedal) {
		for (int ch = s->first_channel; ch < s->first_channel + s->num_channels; ch++) {
			sf64_voice_t *v = &s->voices[ch];
			if (v->midi_channel == midi_channel && v->held_by_sustain)
				voice_enter_release(s, ch);
		}
	}
	midi_apply_vol(s, midi_channel);
	midi_apply_bend(s, midi_channel);
}

static void midi_channel_reset_all(sf64_synth_t *s, int midi_channel)
{
	s->midi[midi_channel].volume = 127;
	midi_channel_reset_program(s, midi_channel);
	midi_channel_reset_controllers(s, midi_channel);
}

/** Reset every MIDI channel's program and controllers to defaults. */
void midi_channels_reset(sf64_synth_t *synth)
{
	for (int i = 0; i < SF64_MIDI_CHANNELS; i++)
		midi_channel_reset_all(synth, i);
}

static void sf64_mt_note_on(midi_target_t *t, int ch, int key, int vel, int64_t now)
{
	(void)now;
	sf64_synth_note_on((sf64_synth_t *)t, ch, key, vel);
}

static void sf64_mt_note_off(midi_target_t *t, int ch, int key, int vel, int64_t now)
{
	(void)vel; (void)now;
	sf64_synth_note_off((sf64_synth_t *)t, ch, key);
}

static void sf64_mt_cc(midi_target_t *t, int ch, int cc, int value, int64_t now)
{
	(void)now;
	sf64_synth_t *s = (sf64_synth_t *)t;
	sf64_midi_channel_t *mc = &s->midi[ch];
	switch (cc) {
	case 7:   sf64_synth_set_volume(s, ch, value); break;
	case 11:  sf64_synth_set_expression(s, ch, value); break;
	case 10:  sf64_synth_set_pan(s, ch, value); break;
	case 64:  sf64_synth_set_sustain(s, ch, value); break;
	case 0:
		mc->bank_msb = (uint8_t)value;
		// GM1: melodic presets stay on bank 0; drum channel stays on 128.
		if (s->mode == SF64_MODE_NATIVE)
			mc->bank = (uint16_t)value;
		break;
	case 32:
		mc->bank_lsb = (uint8_t)value;
		break;
	case 6:
		mc->data_entry_msb = (uint8_t)value;
		midi_rpn_apply_data_entry(s, ch);
		break;
	case 38:
		mc->data_entry_lsb = (uint8_t)value;
		midi_rpn_apply_data_entry(s, ch);
		break;
	case 100:
		mc->rpn_lsb = (uint8_t)value;
		break;
	case 101:
		mc->rpn_msb = (uint8_t)value;
		break;
	case 120: midi_all_sound_off(s, ch); break;
	case 121: midi_channel_reset_controllers(s, ch); break;
	case 123: midi_all_notes_off(s, ch); break;
	}
}

static void sf64_mt_program(midi_target_t *t, int ch, int program, int64_t now)
{
	(void)now;
	sf64_synth_t *s = (sf64_synth_t *)t;
	if (s->mode == SF64_MODE_GM1 && ch == SF64_DRUM_CHANNEL)
		return;
	int bank = (s->mode == SF64_MODE_GM1) ? 0 : (int)s->midi[ch].bank;
	sf64_synth_set_program(s, ch, bank, program);
}

static void sf64_mt_pitch_bend(midi_target_t *t, int ch, int value, int64_t now)
{
	(void)now;
	sf64_synth_set_pitch_bend((sf64_synth_t *)t, ch, value);
}

static void sf64_mt_reset(midi_target_t *t, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	voices_stop_all(s);
	midi_channels_reset(s);
	s->now = now;
}

static void sf64_mt_system_reset(midi_target_t *t, midi_system_t system, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	assert(system == MIDI_SYSTEM_GM1);
	s->mode = SF64_MODE_GM1;
	sf64_mt_reset(t, now);
}

static void sf64_mt_finish(midi_target_t *t, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	(void)now;
	for (int i = 0; i < SF64_MIDI_CHANNELS; i++)
		s->midi[i].sustain = 0;
	if (s->num_channels <= 0)
		return;
	for (int ch = s->first_channel; ch < s->first_channel + s->num_channels; ch++) {
		if (voice_sounding(&s->voices[ch]))
			voice_enter_release(s, ch);
	}
}

static int64_t sf64_mt_process(midi_target_t *t, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	int64_t elapsed = now - s->now;
	assertf(elapsed >= 0, "sf64 midi_target: clock went backwards");
	assertf(elapsed <= INT_MAX, "sf64 midi_target: process span too large");
	int next = sf64_synth_process(s, (int)elapsed);
	if (next < 0) return INT64_MAX;
	return s->now + next;
}

const midi_target_ops_t sf64_midi_ops = {
	.note_on = sf64_mt_note_on,
	.note_off = sf64_mt_note_off,
	.control_change = sf64_mt_cc,
	.program_change = sf64_mt_program,
	.pitch_bend = sf64_mt_pitch_bend,
	.poly_pressure = NULL,
	.channel_pressure = NULL,
	.finish = sf64_mt_finish,
	.reset = sf64_mt_reset,
	.system_reset = sf64_mt_system_reset,
	.process = sf64_mt_process,
};

midi_target_t *sf64_synth_midi_target(sf64_synth_t *synth)
{
	assert(synth);
	return &synth->midi_target;
}

void sf64_synth_set_mode(sf64_synth_t *synth, sf64_synth_mode_t mode)
{
	assert(synth);
	synth->mode = mode;
}

sf64_synth_mode_t sf64_synth_get_mode(sf64_synth_t *synth)
{
	assert(synth);
	return synth->mode;
}

bool sf64_synth_set_program(sf64_synth_t *synth, int midi_channel,
	int bank, int program)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	int idx = sf64_find_preset(synth->bank, bank, program);
	if (idx < 0)
		return false;
	sf64_midi_channel_t *mc = &synth->midi[midi_channel];
	mc->bank = (uint16_t)bank;
	mc->bank_msb = (bank > 127) ? 0 : (uint8_t)bank;
	mc->program = (uint8_t)program;
	mc->preset_index = (int16_t)idx;
	return true;
}

void sf64_synth_set_volume(sf64_synth_t *synth, int midi_channel, int volume)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	assert(volume >= 0 && volume <= 127);
	synth->midi[midi_channel].volume = (uint8_t)volume;
	if (synth->num_channels > 0)
		midi_apply_vol(synth, midi_channel);
}

void sf64_synth_mute_channel(sf64_synth_t *synth, int midi_channel, bool mute)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	synth->midi[midi_channel].muted = mute;
	if (mute)
		midi_all_sound_off(synth, midi_channel);
}

void sf64_synth_set_expression(sf64_synth_t *synth, int midi_channel, int expression)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	assert(expression >= 0 && expression <= 127);
	synth->midi[midi_channel].expression = (uint8_t)expression;
	if (synth->num_channels > 0)
		midi_apply_vol(synth, midi_channel);
}

void sf64_synth_set_pan(sf64_synth_t *synth, int midi_channel, int pan)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	assert(pan >= 0 && pan <= 127);
	synth->midi[midi_channel].pan = (uint8_t)pan;
	if (synth->num_channels > 0)
		midi_apply_vol(synth, midi_channel);
}

void sf64_synth_set_pitch_bend(sf64_synth_t *synth, int midi_channel, int pitch_bend)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	assert(pitch_bend >= 0 && pitch_bend <= 16383);
	synth->midi[midi_channel].pitch_bend = (uint16_t)pitch_bend;
	if (synth->num_channels > 0)
		midi_apply_bend(synth, midi_channel);
}

void sf64_synth_set_sustain(sf64_synth_t *synth, int midi_channel, int value)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	assert(value >= 0 && value <= 127);

	bool was_down = synth->midi[midi_channel].sustain >= 64;
	synth->midi[midi_channel].sustain = (uint8_t)value;
	bool down = value >= 64;
	if (down || !was_down || synth->num_channels <= 0)
		return;

	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->midi_channel == midi_channel && v->held_by_sustain)
			voice_enter_release(synth, ch);
	}
}
