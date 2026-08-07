/**
 * @file sf64_synth.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief SF64 synthesizer
 * @ingroup mixer
 */
#include "sf64_synth.h"
#include "sf64_internal.h"
#include "wav64.h"
#include "mixer.h"
#include "debug.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief SF64 synthesizer.
 *
 * A synthesizer binds an #sf64_bank_t to the mixer and plays
 * notes by matching the selected preset's regions against key and velocity.
 */
typedef struct sf64_synth_s {
	sf64_bank_t *bank;			///< Borrowed bank
	int mixer_channel;			///< Mixer channel to use for playback
	int preset_index;			///< Preset index 
	int current_key;			///< Current key
	bool playing;				///< True if a note is playing
} sf64_synth_t;

sf64_synth_t *sf64_synth_create(sf64_bank_t *bank)
{
	assert(bank);
	sf64_synth_t *synth = calloc(1, sizeof(*synth));
	assert(synth);
	synth->bank = bank;
	synth->mixer_channel = -1;
	synth->preset_index = -1;
	synth->current_key = -1;
	return synth;
}

void sf64_synth_close(sf64_synth_t *synth)
{
	assert(synth);
	if (synth->playing)
		mixer_ch_stop(synth->mixer_channel);
	free(synth);
}

void sf64_synth_set_channel(sf64_synth_t *synth, int mixer_channel)
{
	assert(synth);
	assert(mixer_channel >= 0);
	if (synth->playing) {
		mixer_ch_stop(synth->mixer_channel);
		synth->playing = false;
		synth->current_key = -1;
	}
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
	float gain = (velocity / 127.0f) * powf(10.0f, -r->attenuation_cb / 200.0f);
	float pan = (r->pan + 500) / 1000.0f;
	if (pan < 0) pan = 0;
	if (pan > 1) pan = 1;

	if (synth->playing)
		mixer_ch_stop(synth->mixer_channel);
	mixer_ch_set_vol(synth->mixer_channel, gain * (1.0f - pan), gain * pan);
	mixer_ch_play(synth->mixer_channel, &synth->bank->waves[r->sample_index]->wave);
	mixer_ch_set_freq(synth->mixer_channel, freq);

	synth->playing = true;
	synth->current_key = key;
	return true;
}

void sf64_synth_note_off(sf64_synth_t *synth, int key)
{
	assert(synth);
	if (!synth->playing || synth->current_key != key)
		return;
	mixer_ch_stop(synth->mixer_channel);
	synth->playing = false;
	synth->current_key = -1;
}
