/**
 * @file sf64_synth.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Polyphonic SF64 synthesizer
 * @ingroup mixer
 */
#include "sf64_synth.h"
#include "sf64_internal.h"
#include "wav64.h"
#include "mixer.h"
#include "audio.h"
#include "debug.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

/** @brief Envelope phase of a synth voice. */
typedef enum {
	SF64_VOICE_OFF,      ///< Idle; mixer channel is free
	SF64_VOICE_ATTACK,   ///< Ramping to peak gain
	SF64_VOICE_DECAY,    ///< Ramping from peak to the sustain level
	SF64_VOICE_SUSTAIN,  ///< Holding the sustain level until note-off
	SF64_VOICE_RELEASE,  ///< Ramping to silence after note-off
} sf64_voice_phase_t;

/** @brief Per-MIDI-channel controllers and program */
typedef struct {
	uint16_t bank;              ///< MIDI bank select
	uint8_t program;            ///< MIDI program
	int16_t preset_index;       ///< Index into sf64_bank_t.presets, or -1
	uint8_t volume;             ///< CC7 (0–127)
	uint8_t expression;         ///< CC11 (0–127)
	uint8_t pan;                ///< CC10 (0–127, 64 = center)
	uint8_t sustain;            ///< CC64 sustain pedal (`>= 64` = down)
	uint16_t pitch_bend;        ///< 14-bit bend (center 8192)
	int16_t pitch_range_cents;  ///< Full bend span in cents (default 200)
} sf64_midi_channel_t;

/** @brief One voice bound to a mixer channel. */
typedef struct {
	sf64_voice_phase_t phase; ///< Current envelope phase
	int8_t midi_channel;      ///< MIDI channel that owns this voice, or -1
	int8_t key;               ///< MIDI key that started this voice, or -1
	int8_t velocity;          ///< Note-on velocity (for controller recalcs)
	int16_t preset_index;     ///< Preset used at note-on (for exclusive class)
	int region_index;         ///< Index into sf64_bank_t.regions
	uint32_t note_id;         ///< Note identity shared by layered voices; 0 if off
	int64_t deadline;         ///< Absolute sample time of the next phase change; INT64_MAX = none
	float lvol, rvol;         ///< Peak gain after attack (pre-sustain)
	bool sustain_loop;        ///< True if the region uses #SF64_LOOP_SUSTAIN
	bool key_released;        ///< Note-off received (may still be held by pedal)
	bool held_by_sustain;     ///< Sounding only because the sustain pedal is down
} sf64_voice_t;

/** @brief Opaque synthesizer state (see #sf64_synth_t). */
typedef struct sf64_synth_s {
	midi_target_t midi_target;    ///< Must be first; see #sf64_synth_midi_target
	sf64_bank_t *bank;            ///< Bank this synth plays from
	int first_channel;            ///< First mixer channel of the allocated range
	int num_channels;             ///< Number of mixer channels reserved for voices
	int64_t now;                  ///< Absolute sample time advanced by #sf64_synth_process
	uint32_t used_channel_mask;   ///< Bitmask of busy channels in the allocated range
	uint32_t next_note_id;        ///< Next note identity to assign (`>= 1`)
	sf64_midi_channel_t midi[SF64_MIDI_CHANNELS]; ///< Per-MIDI-channel state
	sf64_voice_t voices[MIXER_MAX_CHANNELS]; ///< Per-mixer-channel voice state
} sf64_synth_t;

static void voices_stop_all(sf64_synth_t *synth);

static void sf64_mt_note_on(midi_target_t *t, int ch, int key, int vel, int64_t now)
{
	sf64_synth_note_on((sf64_synth_t *)t, ch, key, vel);
}

static void sf64_mt_note_off(midi_target_t *t, int ch, int key, int vel, int64_t now)
{
	sf64_synth_note_off((sf64_synth_t *)t, ch, key);
}

static void sf64_mt_cc(midi_target_t *t, int ch, int cc, int value, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	switch (cc) {
	case 7:  sf64_synth_set_volume(s, ch, value); break;
	case 11: sf64_synth_set_expression(s, ch, value); break;
	case 10: sf64_synth_set_pan(s, ch, value); break;
	case 64: sf64_synth_set_sustain(s, ch, value); break;
	case 0:  s->midi[ch].bank = (uint16_t)value; break;
	}
}

static void sf64_mt_program(midi_target_t *t, int ch, int program, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	sf64_synth_set_program(s, ch, s->midi[ch].bank, program);
}

static void sf64_mt_pitch_bend(midi_target_t *t, int ch, int value, int64_t now)
{
	sf64_synth_set_pitch_bend((sf64_synth_t *)t, ch, value);
}

static void sf64_mt_reset(midi_target_t *t, int64_t now)
{
	sf64_synth_t *s = (sf64_synth_t *)t;
	voices_stop_all(s);
	s->now = now;
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

static const midi_target_ops_t sf64_midi_ops = {
	.note_on = sf64_mt_note_on,
	.note_off = sf64_mt_note_off,
	.control_change = sf64_mt_cc,
	.program_change = sf64_mt_program,
	.pitch_bend = sf64_mt_pitch_bend,
	.poly_pressure = NULL,
	.channel_pressure = NULL,
	.reset = sf64_mt_reset,
	.process = sf64_mt_process,
};

static void voice_stop(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	if (v->phase == SF64_VOICE_OFF)
		return;
	mixer_ch_stop(ch);
	v->phase = SF64_VOICE_OFF;
	v->deadline = INT64_MAX;
	v->midi_channel = -1;
	v->key = -1;
	v->velocity = 0;
	v->preset_index = -1;
	v->region_index = -1;
	v->note_id = 0;
	v->sustain_loop = false;
	v->key_released = false;
	v->held_by_sustain = false;
	synth->used_channel_mask &= ~(1u << ch);
}

static void voices_stop_all(sf64_synth_t *synth)
{
	if (synth->num_channels <= 0)
		return;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++)
		voice_stop(synth, ch);
}

static int alloc_channel(sf64_synth_t *synth)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (!(synth->used_channel_mask & (1u << ch))) {
			synth->used_channel_mask |= 1u << ch;
			return ch;
		}
	}
	return -1;
}

static int count_free_channels(sf64_synth_t *synth)
{
	int n = 0;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (!(synth->used_channel_mask & (1u << ch)))
			n++;
	}
	return n;
}

/** True while the voice is in a pre-release envelope phase. */
static bool voice_sounding(const sf64_voice_t *v)
{
	return v->phase == SF64_VOICE_ATTACK ||
		v->phase == SF64_VOICE_DECAY ||
		v->phase == SF64_VOICE_SUSTAIN;
}

/** True while the key is still down (note-off not yet received). */
static bool voice_key_down(const sf64_voice_t *v)
{
	return voice_sounding(v) && !v->key_released;
}

/** SF2 timecents -> output samples; <= -12000 is instant. */
static int timecents_to_samples(int16_t tc)
{
	if (tc <= -12000)
		return 0;
	int rate = audio_get_frequency();
	assert(rate > 0);
	float sec = powf(2.0f, tc / 1200.0f);
	int n = (int)lroundf(sec * (float)rate);
	return n > 0 ? n : 0;
}

static float sustain_factor(int16_t centibels)
{
	if (centibels <= 0) return 1.0f;
	if (centibels >= 1440) return 0.0f;
	return powf(10.0f, -centibels / 200.0f);
}

static void voice_peak_vols(sf64_synth_t *synth, int ch, float *lvol, float *rvol)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_midi_channel_t *mc = &synth->midi[v->midi_channel];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	float gain = (v->velocity / 127.0f)
		* (mc->volume / 127.0f)
		* (mc->expression / 127.0f)
		* powf(10.0f, -r->attenuation_cb / 200.0f);
	int pan_sf = r->pan + (int)lroundf((mc->pan - 64) * (500.0f / 64.0f));
	if (pan_sf < -500) pan_sf = -500;
	if (pan_sf > 500) pan_sf = 500;
	float pan = (pan_sf + 500) / 1000.0f;
	*lvol = gain * (1.0f - pan);
	*rvol = gain * pan;
}

static float voice_freq(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_midi_channel_t *mc = &synth->midi[v->midi_channel];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	sf64_sample_t *s = &synth->bank->samples[r->sample_index];
	float bend = (mc->pitch_bend - 8192) *
		(float)mc->pitch_range_cents / 8192.0f;
	float cents = (v->key - r->root_key) * (float)r->pitch_keytrack
		+ r->coarse_tune * 100.0f + r->fine_tune + bend;
	return s->sample_rate * powf(2.0f, cents / 1200.0f);
}

/** Reapply volume for the current envelope phase (controller change). */
static void voice_apply_vol(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	float l, rvol;
	voice_peak_vols(synth, ch, &l, &rvol);
	v->lvol = l;
	v->rvol = rvol;
	float sf = sustain_factor(r->amp_env.sustain_centibels);
	int rem = 0;
	if (v->deadline != INT64_MAX) {
		rem = (int)(v->deadline - synth->now);
		if (rem < 0) rem = 0;
	}

	switch (v->phase) {
	case SF64_VOICE_ATTACK:
		mixer_ch_set_vol_ramp(ch, l, rvol, rem);
		break;
	case SF64_VOICE_DECAY:
		mixer_ch_set_vol_ramp(ch, l * sf, rvol * sf, rem);
		break;
	case SF64_VOICE_SUSTAIN:
		mixer_ch_set_vol_ramp(ch, l * sf, rvol * sf, 0);
		break;
	case SF64_VOICE_RELEASE:
		mixer_ch_set_vol_ramp(ch, 0, 0, rem);
		break;
	default:
		break;
	}
}

static void midi_apply_vol(sf64_synth_t *synth, int midi_channel)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (synth->voices[ch].phase != SF64_VOICE_OFF &&
			synth->voices[ch].midi_channel == midi_channel)
			voice_apply_vol(synth, ch);
	}
}

static void midi_apply_bend(sf64_synth_t *synth, int midi_channel)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (synth->voices[ch].phase != SF64_VOICE_OFF &&
			synth->voices[ch].midi_channel == midi_channel)
			mixer_ch_set_freq(ch, voice_freq(synth, ch));
	}
}

midi_target_t *sf64_synth_midi_target(sf64_synth_t *synth)
{
	assert(synth);
	return &synth->midi_target;
}

sf64_synth_t *sf64_synth_create(sf64_bank_t *bank)
{
	assert(bank);
	sf64_synth_t *synth = calloc(1, sizeof(*synth));
	assert(synth);
	synth->midi_target.ops = &sf64_midi_ops;
	synth->bank = bank;
	synth->first_channel = -1;
	synth->num_channels = 0;
	synth->now = 0;
	synth->next_note_id = 1;
	int preset0 = sf64_find_preset(bank, 0, 0);
	for (int i = 0; i < SF64_MIDI_CHANNELS; i++) {
		sf64_midi_channel_t *mc = &synth->midi[i];
		mc->bank = 0;
		mc->program = 0;
		mc->preset_index = (int16_t)preset0;
		mc->volume = 127;
		mc->expression = 127;
		mc->pan = 64;
		mc->sustain = 0;
		mc->pitch_bend = 8192;
		mc->pitch_range_cents = 200;
	}
	for (int i = 0; i < MIXER_MAX_CHANNELS; i++) {
		synth->voices[i].phase = SF64_VOICE_OFF;
		synth->voices[i].deadline = INT64_MAX;
		synth->voices[i].midi_channel = -1;
		synth->voices[i].key = -1;
		synth->voices[i].preset_index = -1;
		synth->voices[i].region_index = -1;
	}
	return synth;
}

void sf64_synth_close(sf64_synth_t *synth)
{
	assert(synth);
	voices_stop_all(synth);
	free(synth);
}

void sf64_synth_set_channels(sf64_synth_t *synth, int first_channel, int num_channels)
{
	assert(synth);
	assert(first_channel >= 0);
	assert(num_channels >= 1);
	assert(first_channel + num_channels <= MIXER_MAX_CHANNELS);
	voices_stop_all(synth);
	synth->first_channel = first_channel;
	synth->num_channels = num_channels;
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

/** Begin the release envelope (and leave a sustain loop if present). */
static void voice_enter_release(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	sf64_sample_t *s = &synth->bank->samples[r->sample_index];
	int release = timecents_to_samples(r->amp_env.release_timecents);

	v->key_released = true;
	v->held_by_sustain = false;

	// Exit sustain loops so the SF2 release tail can play. Tiny leading
	// loops are the "release sample" idiom (silence until note-off):
	// without a low-pass filter, leaving them reveals a pitched body that
	// sounds like a second note (e.g. GeneralUser Clavinet_rel). Keep
	// looping silence for those until filters exist.
	if (v->sustain_loop && s->loop_end > s->loop_start &&
		s->loop_end - s->loop_start >= 128)
		mixer_ch_set_loop(ch, false);

	if (release <= 0) {
		voice_stop(synth, ch);
		return;
	}

	mixer_ch_set_vol_ramp(ch, 0, 0, release);
	v->phase = SF64_VOICE_RELEASE;
	v->deadline = synth->now + release;
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

	// Pedal up: voices held only by the pedal enter real release.
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->midi_channel == midi_channel && v->held_by_sustain)
			voice_enter_release(synth, ch);
	}
}

/** After attack: decay to the sustain level, or enter sustain immediately. */
static void voice_enter_decay(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int decay = timecents_to_samples(r->amp_env.decay_timecents);
	float sf = sustain_factor(r->amp_env.sustain_centibels);

	if (decay > 0 && sf < 1.0f) {
		mixer_ch_set_vol_ramp(ch, v->lvol * sf, v->rvol * sf, decay);
		v->phase = SF64_VOICE_DECAY;
		v->deadline = synth->now + decay;
	} else {
		if (sf < 1.0f)
			mixer_ch_set_vol_ramp(ch, v->lvol * sf, v->rvol * sf, 0);
		v->phase = SF64_VOICE_SUSTAIN;
		v->deadline = INT64_MAX;
	}
}

/** Start a held voice on an already-reserved channel. */
static void voice_start(sf64_synth_t *synth, int ch, int midi_channel,
	int preset_index, int ri, int key, int velocity, uint32_t note_id)
{
	sf64_region_t *r = &synth->bank->regions[ri];
	int attack = timecents_to_samples(r->amp_env.attack_timecents);
	sf64_voice_t *v = &synth->voices[ch];

	v->midi_channel = (int8_t)midi_channel;
	v->key = (int8_t)key;
	v->velocity = (int8_t)velocity;
	v->preset_index = (int16_t)preset_index;
	v->region_index = ri;
	v->note_id = note_id;
	v->sustain_loop = (r->loop_mode == SF64_LOOP_SUSTAIN);
	v->key_released = false;
	v->held_by_sustain = false;
	voice_peak_vols(synth, ch, &v->lvol, &v->rvol);

	// #mixer_ch_set_vol declicks; duration 0 is a true immediate mute.
	mixer_ch_set_vol_ramp(ch, 0, 0, 0);
	mixer_ch_play(ch, &synth->bank->waves[r->sample_index]->wave);
	mixer_ch_set_freq(ch, voice_freq(synth, ch));
	mixer_ch_set_vol_ramp(ch, v->lvol, v->rvol, attack);

	if (attack > 0) {
		v->phase = SF64_VOICE_ATTACK;
		v->deadline = synth->now + attack;
	} else {
		voice_enter_decay(synth, ch);
	}
}

/** Stop active voices that share preset + exclusive class with @p matches. */
static void choke_exclusive(sf64_synth_t *synth, int preset_index,
	const int *matches, int nmatch)
{
	for (int i = 0; i < nmatch; i++) {
		uint8_t group = synth->bank->regions[matches[i]].exclusive_group;
		if (group == 0)
			continue;
		for (int ch = synth->first_channel;
			 ch < synth->first_channel + synth->num_channels; ch++) {
			sf64_voice_t *v = &synth->voices[ch];
			if (v->phase == SF64_VOICE_OFF || v->preset_index != preset_index)
				continue;
			if (synth->bank->regions[v->region_index].exclusive_group == group)
				voice_stop(synth, ch);
		}
	}
}

uint32_t sf64_synth_note_on(sf64_synth_t *synth, int midi_channel,
	int key, int velocity)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	assertf(synth->num_channels > 0, "sf64_synth: call sf64_synth_set_channels first");
	if (velocity <= 0) {
		sf64_synth_note_off(synth, midi_channel, key);
		return 0;
	}

	int preset_index = synth->midi[midi_channel].preset_index;
	if (preset_index < 0)
		return 0;

	sf64_preset_t *p = &synth->bank->presets[preset_index];
	int matches[MIXER_MAX_CHANNELS];
	int nmatch = 0;
	for (int i = 0; i < p->num_regions; i++) {
		int ri = p->first_region + i;
		sf64_region_t *r = &synth->bank->regions[ri];
		if (key >= r->key_min && key <= r->key_max &&
			velocity >= r->velocity_min && velocity <= r->velocity_max) {
			assert(nmatch < MIXER_MAX_CHANNELS);
			matches[nmatch++] = ri;
		}
	}
	if (nmatch == 0)
		return 0;

	// Free exclusive-class victims before checking allocation so a choked
	// channel can be reused by this note-on. Done before any of the new
	// layers start, so siblings in the same note-on do not choke each other.
	choke_exclusive(synth, preset_index, matches, nmatch);

	if (count_free_channels(synth) < nmatch)
		return 0;

	uint32_t note_id = synth->next_note_id++;
	if (synth->next_note_id == 0)
		synth->next_note_id = 1;

	for (int i = 0; i < nmatch; i++) {
		int ch = alloc_channel(synth);
		assert(ch >= 0);
		voice_start(synth, ch, midi_channel, preset_index,
			matches[i], key, velocity, note_id);
	}
	return note_id;
}

void sf64_synth_note_off(sf64_synth_t *synth, int midi_channel, int key)
{
	assert(synth);
	assert(midi_channel >= 0 && midi_channel < SF64_MIDI_CHANNELS);
	if (synth->num_channels <= 0)
		return;

	// Oldest identity that still has the key down on this MIDI channel.
	uint32_t oldest = 0;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (!voice_key_down(v) || v->midi_channel != midi_channel || v->key != key)
			continue;
		if (oldest == 0 || v->note_id < oldest)
			oldest = v->note_id;
	}
	if (oldest == 0)
		return;

	bool pedal = synth->midi[midi_channel].sustain >= 64;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (!voice_key_down(v) || v->midi_channel != midi_channel ||
			v->key != key || v->note_id != oldest)
			continue;

		if (pedal) {
			// Sustain loop stays on until the pedal releases.
			v->key_released = true;
			v->held_by_sustain = true;
			continue;
		}
		voice_enter_release(synth, ch);
	}
}

int sf64_synth_process(sf64_synth_t *synth, int num_samples)
{
	assert(synth);
	assert(num_samples >= 0);

	synth->now += num_samples;

	int64_t next = INT64_MAX;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->phase == SF64_VOICE_OFF)
			continue;
		if (v->deadline <= synth->now) {
			if (v->phase == SF64_VOICE_ATTACK) {
				voice_enter_decay(synth, ch);
			} else if (v->phase == SF64_VOICE_DECAY) {
				sf64_region_t *r = &synth->bank->regions[v->region_index];
				// Sustain at silence: reclaim the channel (common for piano).
				if (sustain_factor(r->amp_env.sustain_centibels) <= 0.0f) {
					voice_stop(synth, ch);
					continue;
				}
				v->phase = SF64_VOICE_SUSTAIN;
				v->deadline = INT64_MAX;
			} else if (v->phase == SF64_VOICE_RELEASE) {
				voice_stop(synth, ch);
				continue;
			}
		}
		if (v->deadline < next)
			next = v->deadline;
	}

	if (next == INT64_MAX)
		return -1;
	int64_t delta = next - synth->now;
	return delta > 0 ? (int)delta : 0;
}
