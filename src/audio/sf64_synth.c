/**
 * @file sf64_synth.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Voice engine for the SF64 synthesizer
 * @ingroup mixer
 *
 * Owns mixer-channel allocation, DAHDSR envelopes, exclusive class, and
 * #sf64_synth_process. The MIDI control plane lives in sf64_midi.c.
 */
#include "sf64_synth_internal.h"
#include "wav64.h"
#include "audio.h"
#include "debug.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>

void voice_stop(sf64_synth_t *synth, int ch)
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

void voices_stop_all(sf64_synth_t *synth)
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

/** Timecents with optional SF2 keynum scaling: base + scale*(60-key). */
static int env_scaled_samples(int16_t base_tc, int16_t keynum_scale, int key)
{
	int tc = base_tc;
	if (keynum_scale)
		tc += (int)keynum_scale * (60 - key);
	if (tc < -12000) tc = -12000;
	if (tc > 8000) tc = 8000;
	return timecents_to_samples((int16_t)tc);
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
	case SF64_VOICE_DELAY:
		mixer_ch_set_vol_ramp(ch, 0, 0, 0);
		break;
	case SF64_VOICE_ATTACK:
		mixer_ch_set_vol_ramp(ch, l, rvol, rem);
		break;
	case SF64_VOICE_HOLD:
		mixer_ch_set_vol_ramp(ch, l, rvol, 0);
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

void midi_apply_vol(sf64_synth_t *synth, int midi_channel)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (synth->voices[ch].phase != SF64_VOICE_OFF &&
			synth->voices[ch].midi_channel == midi_channel)
			voice_apply_vol(synth, ch);
	}
}

void midi_apply_bend(sf64_synth_t *synth, int midi_channel)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (synth->voices[ch].phase != SF64_VOICE_OFF &&
			synth->voices[ch].midi_channel == midi_channel)
			mixer_ch_set_freq(ch, voice_freq(synth, ch));
	}
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
	midi_channels_reset(synth);
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

void voice_enter_release(sf64_synth_t *synth, int ch)
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

/** Begin decay (or sustain) with phase boundary at absolute sample @p at. */
static void voice_enter_decay(sf64_synth_t *synth, int ch, int64_t at)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int decay = env_scaled_samples(r->amp_env.decay_timecents,
		r->amp_env.keynum_to_decay, v->key);
	float sf = sustain_factor(r->amp_env.sustain_centibels);

	if (decay > 0 && sf < 1.0f) {
		mixer_ch_set_vol_ramp(ch, v->lvol * sf, v->rvol * sf, decay);
		v->phase = SF64_VOICE_DECAY;
		v->deadline = at + decay;
	} else {
		if (sf < 1.0f)
			mixer_ch_set_vol_ramp(ch, v->lvol * sf, v->rvol * sf, 0);
		v->phase = SF64_VOICE_SUSTAIN;
		v->deadline = INT64_MAX;
	}
}

/** After attack: hold at peak, or go straight to decay. */
static void voice_enter_hold_or_decay(sf64_synth_t *synth, int ch, int64_t at)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int hold = env_scaled_samples(r->amp_env.hold_timecents,
		r->amp_env.keynum_to_hold, v->key);

	mixer_ch_set_vol_ramp(ch, v->lvol, v->rvol, 0);
	if (hold > 0) {
		v->phase = SF64_VOICE_HOLD;
		v->deadline = at + hold;
	} else {
		voice_enter_decay(synth, ch, at);
	}
}

/** Begin the attack ramp with phase boundary at absolute sample @p at. */
static void voice_enter_attack(sf64_synth_t *synth, int ch, int64_t at)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int attack = timecents_to_samples(r->amp_env.attack_timecents);

	if (attack > 0) {
		mixer_ch_set_vol_ramp(ch, v->lvol, v->rvol, attack);
		v->phase = SF64_VOICE_ATTACK;
		v->deadline = at + attack;
	} else {
		voice_enter_hold_or_decay(synth, ch, at);
	}
}

/** Start a held voice on an already-reserved channel. */
static void voice_start(sf64_synth_t *synth, int ch, int midi_channel,
	int preset_index, int ri, int key, int velocity, uint32_t note_id)
{
	sf64_region_t *r = &synth->bank->regions[ri];
	int delay = timecents_to_samples(r->amp_env.delay_timecents);
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

	if (delay > 0) {
		v->phase = SF64_VOICE_DELAY;
		v->deadline = synth->now + delay;
	} else {
		voice_enter_attack(synth, ch, synth->now);
	}
}

/** True if @p group is among the exclusive classes of @p matches. */
static bool match_has_group(sf64_bank_t *bank, const int *matches, int nmatch,
	uint8_t group)
{
	if (group == 0)
		return false;
	for (int i = 0; i < nmatch; i++)
		if (bank->regions[matches[i]].exclusive_group == group)
			return true;
	return false;
}

/** Count voices that #choke_exclusive would stop (without mutating). */
static int count_exclusive_victims(sf64_synth_t *synth, int preset_index,
	const int *matches, int nmatch)
{
	int n = 0;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->phase == SF64_VOICE_OFF || v->preset_index != preset_index)
			continue;
		uint8_t g = synth->bank->regions[v->region_index].exclusive_group;
		if (match_has_group(synth->bank, matches, nmatch, g))
			n++;
	}
	return n;
}

/** Stop active voices that share preset + exclusive class with @p matches. */
static void choke_exclusive(sf64_synth_t *synth, int preset_index,
	const int *matches, int nmatch)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->phase == SF64_VOICE_OFF || v->preset_index != preset_index)
			continue;
		uint8_t g = synth->bank->regions[v->region_index].exclusive_group;
		if (match_has_group(synth->bank, matches, nmatch, g))
			voice_stop(synth, ch);
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
			if (nmatch >= MIXER_MAX_CHANNELS)
				return 0;
			matches[nmatch++] = ri;
		}
	}
	if (nmatch == 0)
		return 0;

	// Exclusive victims are reclaimable for this note-on, but only choke them
	// once allocation is known to succeed (all-or-nothing).
	int free = count_free_channels(synth)
		+ count_exclusive_victims(synth, preset_index, matches, nmatch);
	if (free < nmatch)
		return 0;

	choke_exclusive(synth, preset_index, matches, nmatch);

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

/** Drain all envelope phases whose deadline is at or before @p now. */
static void voice_advance_overdue(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	while (v->phase != SF64_VOICE_OFF && v->deadline <= synth->now) {
		int64_t at = v->deadline;
		switch (v->phase) {
		case SF64_VOICE_DELAY:
			voice_enter_attack(synth, ch, at);
			break;
		case SF64_VOICE_ATTACK:
			voice_enter_hold_or_decay(synth, ch, at);
			break;
		case SF64_VOICE_HOLD:
			voice_enter_decay(synth, ch, at);
			break;
		case SF64_VOICE_DECAY: {
			sf64_region_t *r = &synth->bank->regions[v->region_index];
			// Sustain at silence: reclaim the channel (common for piano).
			if (sustain_factor(r->amp_env.sustain_centibels) <= 0.0f) {
				voice_stop(synth, ch);
				return;
			}
			v->phase = SF64_VOICE_SUSTAIN;
			v->deadline = INT64_MAX;
			break;
		}
		case SF64_VOICE_RELEASE:
			voice_stop(synth, ch);
			return;
		default:
			return;
		}
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
		// One-shot sample finished before the envelope: free the channel.
		if (!mixer_ch_playing(ch)) {
			voice_stop(synth, ch);
			continue;
		}
		voice_advance_overdue(synth, ch);
		if (v->phase == SF64_VOICE_OFF)
			continue;
		if (v->deadline < next)
			next = v->deadline;
	}

	if (next == INT64_MAX)
		return -1;
	int64_t delta = next - synth->now;
	return delta > 0 ? (int)delta : 0;
}
