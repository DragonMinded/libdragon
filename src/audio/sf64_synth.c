/**
 * @file sf64_synth.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Voice engine for the SF64 synthesizer
 * @ingroup mixer
 *
 * Owns mixer-channel allocation, DAHDSR envelopes, exclusive class, and
 * #sf64_synth_process. The MIDI control plane lives in sf64_midi.c.
 *
 * A note-on can start several regions (SF2 layers). Those voices share one
 * note identity; note-off releases the oldest still-held identity for that
 * key on that MIDI channel. Allocation is atomic: #mixer_ch_alloc plans the
 * slots, and if fewer than the matching regions are available nothing starts.
 * Saturated pools steal existing voices (release before sustain before attack,
 * then quieter/older), expanding each victim to its full note identity.
 * Exclusive-class victims are advertised to the planner at
 * #MIXER_PRIORITY_MIN and choked only after the plan succeeds.
 *
 * Two mixer controls, kept independent so CC updates never rebuild envelopes:
 *   - Amp envelope → #mixer_ch_set_gain_ramp
 *   - Region × velocity × CC7 × CC11 × pan → #mixer_ch_set_vol
 * Amp attack is #mixer_ramp_linear (SF2's "convex in dB" intent: linear
 * amplitude). Decay and release use #mixer_ramp_exp. Hold/decay honor SF2
 * keynum scaling. Sustain-loop regions keep looping until release begins.
 *
 * When #sf64_region_t::mod_env_to_pitch is set, a second DAHDSR drives
 * #mixer_ch_set_freq_ramp. Mod attack is SF2 convex in level, then linear in
 * cents; later phases use #mixer_ramp_exp. Pitch bend re-targets the current
 * mod frequency without cancelling the remaining ramp duration.
 *
 * Velocity / CC7 / CC11 use the SF2.01 default `(x/127)²` gain curve. Region
 * attenuation is preconverted by audioconv64 so note-on never runs `powf`.
 *
 * #sf64_synth_process advances absolute sample time, drains every overdue
 * amp and mod envelope phase in one call, and reclaims mixer channels whose
 * one-shot samples finished early. Pass the same sample counts used with
 * #mixer_poll.
 */
#include "sf64_synth_internal.h"
#include "wav64.h"
#include "audio.h"
#include "debug.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>

/**
 * SF2.01 default modulators for note-on velocity, CC7 and CC11 → initial
 * attenuation (concave, decreasing, unipolar, amount 960 cB) collapse to
 * `(x/127)²` in linear gain.
 */
static float sf2_gain(int x)
{
	float t = x * (1.0f / 127.0f);
	return t * t;
}

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
	v->preset_index = -1;
	v->region_index = -1;
	v->note_id = 0;
	v->sustain_loop = false;
	v->key_released = false;
	v->held_by_sustain = false;
	v->mod_phase = SF64_VOICE_OFF;
	v->mod_deadline = INT64_MAX;
	v->mod_env_level = 0;
}

void voices_stop_all(sf64_synth_t *synth)
{
	if (synth->num_channels <= 0)
		return;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++)
		voice_stop(synth, ch);
}

/** Stealing priority for the current envelope phase (above #sf64_synth_s::priority). */
static int voice_priority(sf64_synth_t *synth, const sf64_voice_t *v)
{
	int base = synth->priority;
	switch (v->phase) {
	case SF64_VOICE_OFF:
		return MIXER_PRIORITY_MIN;
	case SF64_VOICE_RELEASE:
		return base + 1;
	case SF64_VOICE_SUSTAIN:
		return v->held_by_sustain ? base + 2 : base + 3;
	default:
		return base + 4; // DELAY / ATTACK / HOLD / DECAY
	}
}

/** Push phase (and exclusive-class) priorities into the mixer for this range. */
static void refresh_priorities(sf64_synth_t *synth, uint32_t exclusive_mask)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		int prio = (exclusive_mask & (1u << ch))
			? MIXER_PRIORITY_MIN
			: voice_priority(synth, &synth->voices[ch]);
		mixer_ch_set_priority(ch, prio);
	}
}

/** Stop every voice that shares @p note_id (layered note identity). */
static void voice_stop_note(sf64_synth_t *synth, uint32_t note_id)
{
	if (note_id == 0)
		return;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (synth->voices[ch].note_id == note_id)
			voice_stop(synth, ch);
	}
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

/** Refresh #sf64_voice_t::channel_gain from the owning MIDI channel. */
static void voice_refresh_channel_gain(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_midi_channel_t *mc = &synth->midi[v->midi_channel];
	v->channel_gain = sf2_gain(mc->volume) * sf2_gain(mc->expression);
}

/** Push region×velocity×CC7×CC11×pan into #mixer_ch_set_vol (not the envelope). */
static void voice_set_vol(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_midi_channel_t *mc = &synth->midi[v->midi_channel];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	float gain = v->base_gain * v->channel_gain;
	int pan_sf = r->pan + (int)lroundf((mc->pan - 64) * (500.0f / 64.0f));
	if (pan_sf < -500) pan_sf = -500;
	if (pan_sf > 500) pan_sf = 500;
	float pan = (pan_sf + 500) / 1000.0f;
	// Duration 0: immediate (no #mixer_ch_set_vol declick). Note-on and CC
	// updates must land before the next mixed sample or peak tests see the
	// tail of the previous level.
	mixer_ch_set_vol_ramp(ch, gain * (1.0f - pan), gain * pan, 0);
}

/** Playback Hz for @p mod_level in [0,1] (modEnvToPitch × level). */
static float voice_freq_at(sf64_synth_t *synth, int ch, float mod_level)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_midi_channel_t *mc = &synth->midi[v->midi_channel];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	sf64_sample_t *s = &synth->bank->samples[r->sample_index];
	float bend = (mc->pitch_bend - 8192) *
		(float)mc->pitch_range_cents / 8192.0f;
	float cents = (v->key - r->root_key) * (float)r->pitch_keytrack
		+ r->coarse_tune * 100.0f + r->fine_tune + bend
		+ r->mod_env_to_pitch * mod_level;
	return s->sample_rate * powf(2.0f, cents / 1200.0f);
}

/**
 * SF2.01 / FluidSynth unipolar convex on [0, 1] (see fluid_convex).
 * Used for envelope attacks
 */
static float sf64_convex(float u)
{
	if (u <= 0.0f) return 0.0f;
	if (u >= 1.0f) return 1.0f;
	float c = 1.0f - (-20.0f / 96.0f) * log10f(u * u);
	if (c < 0.0f) c = 0.0f;
	if (c > 1.0f) c = 1.0f;
	return c;
}

/** Mod-env attack → pitch: SF2 convex level, then linear cents. */
static float sf64_ramp_mod_attack(float start, float end, float u)
{
	return mixer_ramp_exp(start, end, sf64_convex(u));
}

/** Push mod-env target @p level into the mixer (immediate or curved ramp). */
static void voice_apply_mod_freq(sf64_synth_t *synth, int ch, float level,
	int duration, mixer_ramp_fn_t curve)
{
	sf64_voice_t *v = &synth->voices[ch];
	v->mod_env_level = level;
	float hz = voice_freq_at(synth, ch, level);
	if (duration > 0)
		mixer_ch_set_freq_ramp(ch, hz, duration, curve);
	else
		mixer_ch_set_freq(ch, hz);
}

/** Retarget frequency after pitch bend, preserving a running mod ramp. */
static void voice_retarget_freq(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	int rem = 0;
	mixer_ramp_fn_t curve = mixer_ramp_exp;
	if (v->mod_phase == SF64_VOICE_ATTACK || v->mod_phase == SF64_VOICE_DECAY ||
		v->mod_phase == SF64_VOICE_RELEASE) {
		int64_t left = v->mod_deadline - synth->now;
		if (left > 0)
			rem = (int)left;
		if (v->mod_phase == SF64_VOICE_ATTACK)
			curve = sf64_ramp_mod_attack;
	}
	voice_apply_mod_freq(synth, ch, v->mod_env_level, rem, curve);
}

/** Reapply #mixer_ch_set_vol after a MIDI volume/expression/pan change. */
static void voice_apply_vol(sf64_synth_t *synth, int ch)
{
	if (synth->voices[ch].phase == SF64_VOICE_OFF)
		return;
	voice_refresh_channel_gain(synth, ch);
	voice_set_vol(synth, ch);
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
			voice_retarget_freq(synth, ch);
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
		synth->voices[i].mod_phase = SF64_VOICE_OFF;
		synth->voices[i].deadline = INT64_MAX;
		synth->voices[i].mod_deadline = INT64_MAX;
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

void sf64_synth_set_channels(sf64_synth_t *synth, int first_channel,
	int num_channels, int priority)
{
	assert(synth);
	assert(first_channel >= 0);
	assert(num_channels >= 1);
	assert(first_channel + num_channels <= MIXER_MAX_CHANNELS);
	assertf(priority >= MIXER_PRIORITY_MIN && priority <= MIXER_PRIORITY_MAX - 4,
		"sf64_synth_set_channels: priority %d leaves no room for phase offsets",
		priority);
	voices_stop_all(synth);
	synth->first_channel = first_channel;
	synth->num_channels = num_channels;
	synth->priority = priority;
}

/** Begin mod-env decay (or sustain) at absolute sample @p at. */
static void voice_mod_enter_decay(sf64_synth_t *synth, int ch, int64_t at)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int decay = env_scaled_samples(r->mod_env.decay_timecents,
		r->mod_env.keynum_to_decay, v->key);
	float sus = r->mod_env.sustain_gain;
	// SF2/TSF: decay time is to zero; linear segment ends at sustain earlier.
	int dur = (int)lroundf((float)decay * (1.0f - sus));

	if (dur > 0 && sus < 1.0f) {
		voice_apply_mod_freq(synth, ch, sus, dur, mixer_ramp_exp);
		v->mod_phase = SF64_VOICE_DECAY;
		v->mod_deadline = at + dur;
	} else {
		voice_apply_mod_freq(synth, ch, sus, 0, mixer_ramp_exp);
		v->mod_phase = SF64_VOICE_SUSTAIN;
		v->mod_deadline = INT64_MAX;
	}
}

/** After mod attack: hold at peak, or go straight to decay. */
static void voice_mod_enter_hold_or_decay(sf64_synth_t *synth, int ch, int64_t at)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int hold = env_scaled_samples(r->mod_env.hold_timecents,
		r->mod_env.keynum_to_hold, v->key);

	voice_apply_mod_freq(synth, ch, 1.0f, 0, mixer_ramp_exp);
	if (hold > 0) {
		v->mod_phase = SF64_VOICE_HOLD;
		v->mod_deadline = at + hold;
	} else {
		voice_mod_enter_decay(synth, ch, at);
	}
}

/** Begin the mod-env attack at absolute sample @p at. */
static void voice_mod_enter_attack(sf64_synth_t *synth, int ch, int64_t at)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int attack = timecents_to_samples(r->mod_env.attack_timecents);

	if (attack > 0) {
		voice_apply_mod_freq(synth, ch, 1.0f, attack, sf64_ramp_mod_attack);
		v->mod_phase = SF64_VOICE_ATTACK;
		v->mod_deadline = at + attack;
	} else {
		voice_mod_enter_hold_or_decay(synth, ch, at);
	}
}

/** Start the mod envelope, or pin base pitch when modEnvToPitch is unused. */
static void voice_mod_start(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];

	if (r->mod_env_to_pitch == 0) {
		v->mod_phase = SF64_VOICE_OFF;
		v->mod_deadline = INT64_MAX;
		v->mod_env_level = 0;
		mixer_ch_set_freq(ch, voice_freq_at(synth, ch, 0));
		return;
	}

	int delay = timecents_to_samples(r->mod_env.delay_timecents);
	if (delay > 0) {
		voice_apply_mod_freq(synth, ch, 0, 0, mixer_ramp_exp);
		v->mod_phase = SF64_VOICE_DELAY;
		v->mod_deadline = synth->now + delay;
	} else {
		voice_mod_enter_attack(synth, ch, synth->now);
	}
}

/** Note-off for the mod envelope (independent of amp release length). */
static void voice_mod_enter_release(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	if (v->mod_phase == SF64_VOICE_OFF || v->mod_phase == SF64_VOICE_RELEASE)
		return;

	sf64_region_t *r = &synth->bank->regions[v->region_index];
	int release = timecents_to_samples(r->mod_env.release_timecents);
	if (release <= 0) {
		voice_apply_mod_freq(synth, ch, 0, 0, mixer_ramp_exp);
		v->mod_phase = SF64_VOICE_OFF;
		v->mod_deadline = INT64_MAX;
		return;
	}
	voice_apply_mod_freq(synth, ch, 0, release, mixer_ramp_exp);
	v->mod_phase = SF64_VOICE_RELEASE;
	v->mod_deadline = synth->now + release;
}

/** Drain overdue mod-envelope phases. */
static void voice_mod_advance_overdue(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	while (v->mod_phase != SF64_VOICE_OFF && v->mod_deadline <= synth->now) {
		int64_t at = v->mod_deadline;
		switch (v->mod_phase) {
		case SF64_VOICE_DELAY:
			voice_mod_enter_attack(synth, ch, at);
			break;
		case SF64_VOICE_ATTACK:
			voice_mod_enter_hold_or_decay(synth, ch, at);
			break;
		case SF64_VOICE_HOLD:
			voice_mod_enter_decay(synth, ch, at);
			break;
		case SF64_VOICE_DECAY: {
			sf64_region_t *r = &synth->bank->regions[v->region_index];
			voice_apply_mod_freq(synth, ch, r->mod_env.sustain_gain, 0, mixer_ramp_exp);
			v->mod_phase = SF64_VOICE_SUSTAIN;
			v->mod_deadline = INT64_MAX;
			break;
		}
		case SF64_VOICE_RELEASE:
			voice_apply_mod_freq(synth, ch, 0, 0, mixer_ramp_exp);
			v->mod_phase = SF64_VOICE_OFF;
			v->mod_deadline = INT64_MAX;
			return;
		default:
			return;
		}
	}
}

void voice_enter_release(sf64_synth_t *synth, int ch)
{
	sf64_voice_t *v = &synth->voices[ch];
	sf64_region_t *r = &synth->bank->regions[v->region_index];
	sf64_sample_t *s = &synth->bank->samples[r->sample_index];
	int release = timecents_to_samples(r->amp_env.release_timecents);

	v->key_released = true;
	v->held_by_sustain = false;
	voice_mod_enter_release(synth, ch);

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

	v->envelope_gain = 0;
	mixer_ch_set_gain_ramp(ch, 0, release, mixer_ramp_exp);
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
	float sus = r->amp_env.sustain_gain;

	v->envelope_gain = sus;
	if (decay > 0 && sus < 1.0f) {
		mixer_ch_set_gain_ramp(ch, sus, decay, mixer_ramp_exp);
		v->phase = SF64_VOICE_DECAY;
		v->deadline = at + decay;
	} else {
		mixer_ch_set_gain(ch, sus);
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

	v->envelope_gain = 1.0f;
	mixer_ch_set_gain(ch, 1.0f);
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
		v->envelope_gain = 1.0f;
		mixer_ch_set_gain_ramp(ch, 1.0f, attack, mixer_ramp_linear);
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
	v->preset_index = (int16_t)preset_index;
	v->region_index = ri;
	v->note_id = note_id;
	v->sustain_loop = (r->loop_mode == SF64_LOOP_SUSTAIN);
	v->key_released = false;
	v->held_by_sustain = false;
	v->base_gain = r->gain * sf2_gain(velocity);
	voice_refresh_channel_gain(synth, ch);
	v->envelope_gain = 0;

	mixer_ch_set_gain(ch, 0);
	voice_set_vol(synth, ch);
	mixer_ch_play(ch, &synth->bank->waves[r->sample_index]->wave);
	voice_mod_start(synth, ch);

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

/** Bitmask of voices that share preset + exclusive class with @p matches. */
static uint32_t exclusive_victim_mask(sf64_synth_t *synth, int preset_index,
	const int *matches, int nmatch)
{
	uint32_t mask = 0;
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		sf64_voice_t *v = &synth->voices[ch];
		if (v->phase == SF64_VOICE_OFF || v->preset_index != preset_index)
			continue;
		uint8_t g = synth->bank->regions[v->region_index].exclusive_group;
		if (match_has_group(synth->bank, matches, nmatch, g))
			mask |= 1u << ch;
	}
	return mask;
}

/** Stop every voice listed in @p mask (absolute channel bits). */
static void choke_exclusive_mask(sf64_synth_t *synth, uint32_t mask)
{
	for (int ch = synth->first_channel;
		 ch < synth->first_channel + synth->num_channels; ch++) {
		if (mask & (1u << ch))
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

	// Advertise exclusive victims as free to the planner; choke only after
	// the plan succeeds so a rejected note-on leaves them untouched.
	uint32_t excl = exclusive_victim_mask(synth, preset_index, matches, nmatch);
	refresh_priorities(synth, excl);

	int plan[MIXER_MAX_CHANNELS];
	int nplan = mixer_ch_alloc(synth->first_channel, synth->num_channels,
		nmatch, false, synth->priority + 4, NULL, plan);
	if (nplan < nmatch)
		return 0;

	choke_exclusive_mask(synth, excl);

	// Steal: expand each still-occupied planned channel to its note identity
	// so a layered victim is never cut in half.
	for (int i = 0; i < nmatch; i++) {
		sf64_voice_t *v = &synth->voices[plan[i]];
		if (v->phase != SF64_VOICE_OFF)
			voice_stop_note(synth, v->note_id);
	}

	uint32_t note_id = synth->next_note_id++;
	if (synth->next_note_id == 0)
		synth->next_note_id = 1;

	for (int i = 0; i < nmatch; i++) {
		voice_start(synth, plan[i], midi_channel, preset_index,
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
			if (r->amp_env.sustain_gain <= 0.0f) {
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
		voice_mod_advance_overdue(synth, ch);
		if (v->deadline < next)
			next = v->deadline;
		if (v->mod_deadline < next)
			next = v->mod_deadline;
	}

	if (next == INT64_MAX)
		return -1;
	int64_t delta = next - synth->now;
	return delta > 0 ? (int)delta : 0;
}
