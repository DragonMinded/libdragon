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
#include <assert.h>

typedef enum {
	SF64_VOICE_OFF,
	SF64_VOICE_ATTACK,
	SF64_VOICE_DECAY,
	SF64_VOICE_SUSTAIN,
	SF64_VOICE_RELEASE,
} sf64_voice_phase_t;

typedef struct {
	sf64_voice_phase_t phase;
	int key;
	int region_index;
	int64_t deadline;   ///< Absolute sample time; INT64_MAX = none
	float lvol, rvol;   ///< Peak gain after attack (pre-sustain)
	bool sustain_loop;
} sf64_voice_t;

struct sf64_synth_s {
	sf64_bank_t *bank;
	int first_channel;
	int num_channels;
	int preset_index;
	int64_t now;
	uint32_t used_channel_mask;
	sf64_voice_t voices[MIXER_MAX_CHANNELS];
};

static void voice_stop(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	if (v->phase == SF64_VOICE_OFF)
		return;
	mixer_ch_stop(ch);
	v->phase = SF64_VOICE_OFF;
	v->deadline = INT64_MAX;
	v->key = -1;
	v->region_index = -1;
	v->sustain_loop = false;
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

static void voices_stop_key(sf64_synth_t *synth, int key)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (synth->voices[ch].phase != SF64_VOICE_OFF &&
			synth->voices[ch].key == key)
			voice_stop(synth, ch);
	}
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

sf64_synth_t *sf64_synth_create(sf64_bank_t *bank)
{
	assert(bank);
	sf64_synth_t *synth = calloc(1, sizeof(*synth));
	assert(synth);
	synth->bank = bank;
	synth->first_channel = -1;
	synth->num_channels = 0;
	synth->preset_index = -1;
	synth->now = 0;
	for (int i = 0; i < MIXER_MAX_CHANNELS; i++) {
		synth->voices[i].phase = SF64_VOICE_OFF;
		synth->voices[i].deadline = INT64_MAX;
		synth->voices[i].key = -1;
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

bool sf64_synth_set_preset(sf64_synth_t *synth, int midi_bank, int program)
{
	assert(synth);
	int idx = sf64_find_preset(synth->bank, midi_bank, program);
	if (idx < 0)
		return false;
	synth->preset_index = idx;
	return true;
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

static void region_gain_pan(const sf64_region_t *r, int velocity,
	float *lvol, float *rvol)
{
	float gain = (velocity / 127.0f) * powf(10.0f, -r->attenuation_cb / 200.0f);
	float pan = (r->pan + 500) / 1000.0f;
	if (pan < 0) pan = 0;
	if (pan > 1) pan = 1;
	*lvol = gain * (1.0f - pan);
	*rvol = gain * pan;
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

bool sf64_synth_note_on(sf64_synth_t *synth, int key, int velocity)
{
	assert(synth);
	assertf(synth->num_channels > 0, "sf64_synth: call sf64_synth_set_channels first");
	if (velocity <= 0) {
		sf64_synth_note_off(synth, key);
		return false;
	}
	if (synth->preset_index < 0)
		return false;

	int ri = sf64_find_region(synth->bank, synth->preset_index, key, velocity);
	if (ri < 0)
		return false;

	// Retrigger: drop any previous voice on this key so presses do not stack.
	voices_stop_key(synth, key);

	int ch = alloc_channel(synth);
	if (ch < 0)
		return false;

	sf64_region_t *r = &synth->bank->regions[ri];
	sf64_sample_t *s = &synth->bank->samples[r->sample_index];
	float cents = (key - r->root_key) * (float)r->pitch_keytrack
		+ r->coarse_tune * 100.0f + r->fine_tune;
	float freq = s->sample_rate * powf(2.0f, cents / 1200.0f);
	float lvol, rvol;
	region_gain_pan(r, velocity, &lvol, &rvol);
	int attack = timecents_to_samples(r->amp_env.attack_timecents);
	sf64_voice_t *v = &synth->voices[ch];

	// #mixer_ch_set_vol declicks; duration 0 is a true immediate mute.
	mixer_ch_set_vol_ramp(ch, 0, 0, 0);
	mixer_ch_play(ch, &synth->bank->waves[r->sample_index]->wave);
	mixer_ch_set_freq(ch, freq);
	mixer_ch_set_vol_ramp(ch, lvol, rvol, attack);

	v->key = key;
	v->region_index = ri;
	v->lvol = lvol;
	v->rvol = rvol;
	v->sustain_loop = (r->loop_mode == SF64_LOOP_SUSTAIN);
	if (attack > 0) {
		v->phase = SF64_VOICE_ATTACK;
		v->deadline = synth->now + attack;
	} else {
		voice_enter_decay(synth, ch);
	}
	return true;
}

void sf64_synth_note_off(sf64_synth_t *synth, int key)
{
	assert(synth);
	if (synth->num_channels <= 0)
		return;

	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->phase == SF64_VOICE_OFF ||
			v->phase == SF64_VOICE_RELEASE ||
			v->key != key)
			continue;

		sf64_region_t *r = &synth->bank->regions[v->region_index];
		int release = timecents_to_samples(r->amp_env.release_timecents);

		if (v->sustain_loop)
			mixer_ch_set_loop(ch, false);

		if (release <= 0) {
			voice_stop(synth, ch);
			continue;
		}

		mixer_ch_set_vol_ramp(ch, 0, 0, release);
		v->phase = SF64_VOICE_RELEASE;
		v->deadline = synth->now + release;
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
