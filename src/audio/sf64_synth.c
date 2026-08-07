/**
 * @file sf64_synth.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Monophonic SF64 synthesizer
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
#include <assert.h>

typedef enum {
	SF64_VOICE_OFF,
	SF64_VOICE_ATTACK,
	SF64_VOICE_SUSTAIN,
	SF64_VOICE_RELEASE,
} sf64_voice_phase_t;

typedef struct {
	sf64_voice_phase_t phase;
	int key;
	int region_index;
	int remaining;   ///< Output samples until next phase change (`< 0` = none)
	bool sustain_loop;
} sf64_voice_t;

struct sf64_synth_s {
	sf64_bank_t *bank;
	int mixer_channel;
	int preset_index;
	sf64_voice_t voice;
};

sf64_synth_t *sf64_synth_create(sf64_bank_t *bank)
{
	assert(bank);
	sf64_synth_t *synth = calloc(1, sizeof(*synth));
	assert(synth);
	synth->bank = bank;
	synth->mixer_channel = -1;
	synth->preset_index = -1;
	synth->voice.phase = SF64_VOICE_OFF;
	synth->voice.remaining = -1;
	synth->voice.key = -1;
	synth->voice.region_index = -1;
	return synth;
}

static void voice_stop(sf64_synth_t *synth)
{
	if (synth->voice.phase != SF64_VOICE_OFF)
		mixer_ch_stop(synth->mixer_channel);
	synth->voice.phase = SF64_VOICE_OFF;
	synth->voice.remaining = -1;
	synth->voice.key = -1;
	synth->voice.region_index = -1;
	synth->voice.sustain_loop = false;
}

void sf64_synth_close(sf64_synth_t *synth)
{
	assert(synth);
	voice_stop(synth);
	free(synth);
}

void sf64_synth_set_channel(sf64_synth_t *synth, int mixer_channel)
{
	assert(synth);
	assert(mixer_channel >= 0);
	voice_stop(synth);
	synth->mixer_channel = mixer_channel;
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

bool sf64_synth_note_on(sf64_synth_t *synth, int key, int velocity)
{
	assert(synth);
	assertf(synth->mixer_channel >= 0, "sf64_synth: call sf64_synth_set_channel first");
	if (velocity <= 0) {
		sf64_synth_note_off(synth, key);
		return false;
	}
	if (synth->preset_index < 0)
		return false;

	int ri = sf64_find_region(synth->bank, synth->preset_index, key, velocity);
	if (ri < 0)
		return false;

	sf64_region_t *r = &synth->bank->regions[ri];
	sf64_sample_t *s = &synth->bank->samples[r->sample_index];
	float cents = (key - r->root_key) * (float)r->pitch_keytrack
		+ r->coarse_tune * 100.0f + r->fine_tune;
	float freq = s->sample_rate * powf(2.0f, cents / 1200.0f);
	float lvol, rvol;
	region_gain_pan(r, velocity, &lvol, &rvol);
	int attack = timecents_to_samples(r->amp_env.attack_timecents);
	int ch = synth->mixer_channel;

	voice_stop(synth);
	// #mixer_ch_set_vol declicks; duration 0 is a true immediate mute.
	mixer_ch_set_vol_ramp(ch, 0, 0, 0);
	mixer_ch_play(ch, &synth->bank->waves[r->sample_index]->wave);
	mixer_ch_set_freq(ch, freq);
	mixer_ch_set_vol_ramp(ch, lvol, rvol, attack);

	synth->voice.key = key;
	synth->voice.region_index = ri;
	synth->voice.sustain_loop = (r->loop_mode == SF64_LOOP_SUSTAIN);
	if (attack > 0) {
		synth->voice.phase = SF64_VOICE_ATTACK;
		synth->voice.remaining = attack;
	} else {
		synth->voice.phase = SF64_VOICE_SUSTAIN;
		synth->voice.remaining = -1;
	}
	return true;
}

void sf64_synth_note_off(sf64_synth_t *synth, int key)
{
	assert(synth);
	if (synth->voice.phase == SF64_VOICE_OFF ||
		synth->voice.phase == SF64_VOICE_RELEASE ||
		synth->voice.key != key)
		return;

	sf64_region_t *r = &synth->bank->regions[synth->voice.region_index];
	int release = timecents_to_samples(r->amp_env.release_timecents);
	int ch = synth->mixer_channel;

	if (synth->voice.sustain_loop)
		mixer_ch_set_loop(ch, false);

	if (release <= 0) {
		voice_stop(synth);
		return;
	}

	mixer_ch_set_vol_ramp(ch, 0, 0, release);
	synth->voice.phase = SF64_VOICE_RELEASE;
	synth->voice.remaining = release;
}

int sf64_synth_process(sf64_synth_t *synth, int num_samples)
{
	assert(synth);
	assert(num_samples >= 0);

	if (synth->voice.phase == SF64_VOICE_OFF || synth->voice.remaining < 0)
		return -1;

	if (synth->voice.remaining > num_samples) {
		synth->voice.remaining -= num_samples;
		return synth->voice.remaining;
	}

	if (synth->voice.phase == SF64_VOICE_ATTACK) {
		synth->voice.phase = SF64_VOICE_SUSTAIN;
		synth->voice.remaining = -1;
		return -1;
	}

	if (synth->voice.phase == SF64_VOICE_RELEASE) {
		voice_stop(synth);
		return -1;
	}

	return -1;
}
