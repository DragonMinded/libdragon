/**
 * @file test_sf64.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Standalone testrom for the SF64 bank loader and synth
 *
 * Loads the deterministic sf64test bank, checks preset/region/sample layout,
 * plays embedded waveforms through the real mixer, and exercises the
 * synthesizer through step 5 (pitch, matching, envelopes, polyphony, layers,
 * note identity, voice stealing, and the SF2 gain model).
 */
#include <libdragon.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/audio/sf64_internal.h"

#define SAMPLE_RATE  22050
#define CH           0
#define NCH          8
#define AMP          16000
#define SQ           16
#define A_LEN        4096
#define B_INTRO      1000
#define B_LOOP       1000
#define B_REL        1000
#define B_LEN        (B_INTRO + B_LOOP + B_REL)
#define C_LEN        4096
#define D_LEN        4096
#define VOL          0.5f

// Must match tests/assets/gen_sf64_bank.py (plus the trailing pad sample TSF adds).
static int16_t pcm_a(int i) {
	if (i < 0 || i >= A_LEN) return 0;
	return (int16_t)(-AMP + (int32_t)(2 * AMP) * i / A_LEN);
}
static int16_t pcm_b(int i) {
	if (i < 0 || i >= B_LEN) return 0;
	if (i < B_INTRO)
		return (int16_t)(-AMP + (int32_t)(2 * AMP) * i / B_INTRO);
	if (i < B_INTRO + B_LOOP)
		return ((i - B_INTRO) % SQ < SQ / 2) ? AMP : (int16_t)-AMP;
	int j = i - (B_INTRO + B_LOOP);
	return (int16_t)(AMP - (int32_t)(2 * AMP) * j / B_REL);
}
static int16_t pcm_c(int i) {
	if (i < 0 || i >= C_LEN) return 0;
	return (i % SQ < SQ / 2) ? (AMP / 2) : (int16_t)-(AMP / 2);
}
static int16_t pcm_d(int i) {
	if (i < 0 || i >= D_LEN) return 0;
	return (int16_t)(AMP - (int32_t)(2 * AMP) * (i % 64) / 64);
}

static int16_t *out;
static int out_cap;

static void mix(int nsamples)
{
	assert(nsamples * 2 <= out_cap);
	int16_t *p = out;
	int left = nsamples;
	while (left > 0) {
		int n = left < 1024 ? left : 1024;
		mixer_poll(p, n);
		p += n * 2;
		left -= n;
	}
}

static void silence(void)
{
	mixer_ch_stop(CH);
	mix(2048);
}

/** Stop mixer channels and clear synth voice state (re-binds the range). */
static void synth_silence(sf64_synth_t *synth, int nch)
{
	for (int i = 0; i < nch; i++)
		mixer_ch_stop(i);
	mix(2048);
	sf64_synth_set_channels(synth, 0, nch, MIXER_PRIORITY_MUSIC);
}

// Settle the volume ramp at step 0 so the playhead stays on @p start.
static void play_at(waveform_t *wave, int start)
{
	silence();
	mixer_ch_set_limits(CH, 16, SAMPLE_RATE, 0);
	mixer_ch_play(CH, wave);
	mixer_ch_set_vol(CH, VOL, VOL);
	mixer_ch_set_pos(CH, start);
	mixer_ch_set_freq(CH, 0);
	mix(2048);
	mixer_ch_set_freq(CH, SAMPLE_RATE);
}

// Hermite at 1:1 returns the second tap (source pos + 1). Volume is 0.5.
static bool check_pcm(const char *tag, int start, int nsamples,
	int16_t (*sample)(int), int loop_end, int loop_len)
{
	int nbad = 0, first = -1, maxdiff = 0, peak = 0;
	for (int i = 0; i < nsamples; i++) {
		int p = start + 1 + i;
		if (loop_len && p >= loop_end)
			p = loop_end - loop_len + (p - loop_end) % loop_len;
		int want = sample(p) / 2;
		int got = out[i * 2];
		int d = got - want; if (d < 0) d = -d;
		if (d > 64) {
			nbad++;
			if (first < 0) first = i;
			if (d > maxdiff) maxdiff = d;
		}
		int a = got < 0 ? -got : got;
		if (a > peak) peak = a;
	}
	if (nbad) {
		printf("FAILED %s: %d/%d off (max %d) from %d\n",
			tag, nbad, nsamples, maxdiff, first);
		for (int i = first; i < first + 6 && i < nsamples; i++) {
			int p = start + 1 + i;
			if (loop_len && p >= loop_end)
				p = loop_end - loop_len + (p - loop_end) % loop_len;
			printf("   [%d] play=%d got %d want %d\n",
				i, p, out[i * 2], sample(p) / 2);
		}
		return false;
	}
	if (peak < 2048) {
		printf("FAILED %s: silent (peak %d)\n", tag, peak);
		return false;
	}
	return true;
}

static bool test_bank_layout(sf64_bank_t *bank)
{
	if (bank->num_presets != 6 || bank->num_regions != 11 || bank->num_samples != 4) {
		printf("FAILED layout: presets=%d regions=%d samples=%d\n",
			bank->num_presets, bank->num_regions, bank->num_samples);
		return false;
	}
	if (sf64_preset_count(bank) != 6) {
		printf("FAILED preset_count\n");
		return false;
	}
	if (sf64_find_preset(bank, 0, 0) != 0 ||
		sf64_find_preset(bank, 0, 1) != 1 ||
		sf64_find_preset(bank, 0, 2) != 2 ||
		sf64_find_preset(bank, 0, 3) != 3 ||
		sf64_find_preset(bank, 0, 4) != 4 ||
		sf64_find_preset(bank, 128, 0) != 5 ||
		sf64_find_preset(bank, 0, 5) != -1) {
		printf("FAILED find_preset\n");
		return false;
	}
	int mb, prog;
	sf64_preset_id(bank, 1, &mb, &prog);
	if (mb != 0 || prog != 1 || strcmp(sf64_preset_name(bank, 0), "KeySplit") != 0 ||
		strcmp(sf64_preset_name(bank, 1), "VelSplit") != 0 ||
		strcmp(sf64_preset_name(bank, 3), "Layer2") != 0 ||
		strcmp(sf64_preset_name(bank, 4), "Layer3") != 0 ||
		strcmp(sf64_preset_name(bank, 5), "DrumKit") != 0) {
		printf("FAILED preset introspect: bank=%d prog=%d name0=%s name1=%s\n",
			mb, prog, sf64_preset_name(bank, 0), sf64_preset_name(bank, 1));
		return false;
	}

	sf64_preset_t *p0 = &bank->presets[0];
	sf64_preset_t *p1 = &bank->presets[1];
	sf64_preset_t *p2 = &bank->presets[2];
	sf64_preset_t *p3 = &bank->presets[3];
	sf64_preset_t *p4 = &bank->presets[4];
	sf64_preset_t *p5 = &bank->presets[5];
	if (p0->bank != 0 || p0->program != 0 || p0->num_regions != 2 ||
		p1->bank != 0 || p1->program != 1 || p1->num_regions != 2 ||
		p2->bank != 0 || p2->program != 2 || p2->num_regions != 1 ||
		p3->bank != 0 || p3->program != 3 || p3->num_regions != 2 ||
		p4->bank != 0 || p4->program != 4 || p4->num_regions != 3 ||
		p5->bank != 128 || p5->program != 0 || p5->num_regions != 1) {
		printf("FAILED preset headers\n");
		return false;
	}

	// Format counts must fit the on-disk uint16 fields.
	if (bank->num_presets > 65535 || bank->num_regions > 65535 ||
		bank->num_samples > 65535) {
		printf("FAILED layout: counts exceed uint16\n");
		return false;
	}

	// Preset 0: A key 0-71, C key 72-127
	sf64_region_t *r = &bank->regions[p0->first_region];
	if (r[0].key_min != 0 || r[0].key_max != 71 || r[0].root_key != 60 ||
		r[0].loop_mode != SF64_LOOP_NONE || r[0].sample_index != 0) {
		printf("FAILED preset0 region A\n");
		return false;
	}
	if (r[1].key_min != 72 || r[1].key_max != 127 || r[1].root_key != 72 ||
		r[1].loop_mode != SF64_LOOP_NONE || r[1].sample_index != 1) {
		printf("FAILED preset0 region C\n");
		return false;
	}

	// Preset 1: A vel 0-79, D vel 80-127 (D has 200 cB → gain 0.1).
	r = &bank->regions[p1->first_region];
	if (r[0].velocity_min != 0 || r[0].velocity_max != 79 ||
		r[0].sample_index != 0 ||
		r[1].velocity_min != 80 || r[1].velocity_max != 127 ||
		r[1].sample_index != 2 || r[1].gain < 0.09f || r[1].gain > 0.11f) {
		printf("FAILED preset1 velocity split (gainD=%f)\n", r[1].gain);
		return false;
	}

	// Dedup: sample A is shared by KeySplit / VelSplit / Layer2.
	if (bank->regions[p0->first_region].sample_index !=
		bank->regions[p1->first_region].sample_index ||
		bank->regions[p0->first_region].sample_index !=
		bank->regions[p3->first_region].sample_index) {
		printf("FAILED layout: sample A not deduplicated across presets\n");
		return false;
	}

	// Preset 2: sustain loop B, loop_end < len
	r = &bank->regions[p2->first_region];
	sf64_sample_t *sb = &bank->samples[r->sample_index];
	if (r->loop_mode != SF64_LOOP_SUSTAIN || r->root_key != 60 ||
		sb->loop_start != B_INTRO || sb->loop_end != B_INTRO + B_LOOP ||
		sb->loop_end >= sb->sample_end || sb->sample_rate != SAMPLE_RATE) {
		printf("FAILED preset2 sample B: mode=%d ls=%lu le=%lu end=%lu rate=%lu\n",
			r->loop_mode, (unsigned long)sb->loop_start, (unsigned long)sb->loop_end,
			(unsigned long)sb->sample_end, (unsigned long)sb->sample_rate);
		return false;
	}

	// Preset 3 Layer2 / 4 Layer3: overlapping full-range regions.
	r = &bank->regions[p3->first_region];
	if (r[0].sample_index == r[1].sample_index ||
		r[0].key_min != 0 || r[0].key_max != 127 ||
		r[1].key_min != 0 || r[1].key_max != 127) {
		printf("FAILED preset3 Layer2 regions\n");
		return false;
	}
	r = &bank->regions[p4->first_region];
	if (r[0].sample_index == r[1].sample_index ||
		r[1].sample_index == r[2].sample_index ||
		r[0].sample_index == r[2].sample_index) {
		printf("FAILED preset4 Layer3 regions\n");
		return false;
	}

	for (int i = 0; i < bank->num_samples; i++) {
		sf64_sample_t *s = &bank->samples[i];
		wav64_t *w = bank->waves[i];
		if (!w || w->wave.channels != 1 || w->wave.bits != 16 ||
			w->wave.frequency != SAMPLE_RATE ||
			(int)w->wave.len != (int)s->sample_end) {
			printf("FAILED sample %d waveform (len=%d meta=%lu)\n",
				i, w ? (int)w->wave.len : -1, (unsigned long)s->sample_end);
			return false;
		}
		if (s->loop_end && s->loop_end >= s->sample_end) {
			printf("FAILED sample %d: loop_end=%lu >= end=%lu\n",
				i, (unsigned long)s->loop_end, (unsigned long)s->sample_end);
			return false;
		}
	}
	return true;
}

static bool test_play_oneshot(sf64_bank_t *bank, int si, int16_t (*sample)(int), const char *tag)
{
	waveform_t *wave = &bank->waves[si]->wave;
	play_at(wave, 0);
	int pos = (int)mixer_ch_get_pos(CH);
	mix(1024);
	return check_pcm(tag, pos, 1024, sample, wave->len, 0);
}

static bool test_play_loop(sf64_bank_t *bank)
{
	// Sample B is last in the bank (index 3).
	int si = bank->regions[bank->presets[2].first_region].sample_index;
	waveform_t *wave = &bank->waves[si]->wave;
	int loop_end = wave->loop_end ? wave->loop_end : wave->len;
	int loop_len = wave->loop_len;

	if (loop_end != B_INTRO + B_LOOP || loop_len != B_LOOP) {
		printf("FAILED loop wave: end=%d len=%d (wave len=%d)\n",
			loop_end, loop_len, (int)wave->len);
		return false;
	}

	// Start inside the loop and verify wrapping.
	play_at(wave, B_INTRO + 8);
	int pos = (int)mixer_ch_get_pos(CH);
	mix(2048);
	if (!check_pcm("sample B loop", pos, 2048, pcm_b, loop_end, loop_len))
		return false;

	// Note-off: disable loop, drain through the release tail, then idle.
	silence();
	mixer_ch_play(CH, wave);
	mixer_ch_set_vol(CH, VOL, VOL);
	mixer_ch_set_pos(CH, B_INTRO + B_LOOP / 2);
	mixer_ch_set_freq(CH, 0);
	mix(2048);
	mixer_ch_set_freq(CH, SAMPLE_RATE);
	mixer_ch_set_loop(CH, false);
	pos = (int)mixer_ch_get_pos(CH);
	mix(B_REL + 512);
	if (!check_pcm("sample B release", pos, B_REL - 8, pcm_b, wave->len, 0))
		return false;
	mix(2048);
	if (mixer_ch_playing(CH)) {
		printf("FAILED sample B release: still playing at %d (len=%d)\n",
			(int)mixer_ch_get_pos(CH), (int)wave->len);
		return false;
	}
	return true;
}

static int peak_range(int start, int n)
{
	int peak = 0;
	for (int i = start * 2; i < (start + n) * 2; i++) {
		int a = out[i] < 0 ? -out[i] : out[i];
		if (a > peak) peak = a;
	}
	return peak;
}

// An envelope of several seconds asks the mixer for a ramp shallower than the
// one fx15 unit per block of four samples that is the least the ucode can
// walk. It has to be bounded by the volume each round ends on: bounded by the
// end of the whole ramp instead, it would run through every round many times
// too fast and be pulled back at the next one, an audible flutter.
static bool test_slow_ramp(sf64_bank_t *bank)
{
	// The loop of sample B is a square of constant amplitude, so the peak of
	// a window of output is the volume the ramp has reached over it.
	int si = bank->regions[bank->presets[2].first_region].sample_index;
	waveform_t *wave = &bank->waves[si]->wave;
	play_at(wave, B_INTRO + 8);

	const int SECS = 20, WIN = 64, BLK = 2048, NBLK = 4;
	mixer_ch_set_vol_ramp(CH, 0, 0, SECS * SAMPLE_RATE);

	int prev = -1;
	for (int b = 0; b < NBLK; b++) {
		mix(BLK);
		for (int i = 0; i < BLK / WIN; i++) {
			int peak = peak_range(i * WIN, WIN);
			// The envelope only sinks: any rise is the ramp snapping back.
			if (prev >= 0 && peak > prev + 4) {
				printf("FAILED slow ramp: peak %d -> %d at sample %d\n",
					prev, peak, b * BLK + i * WIN);
				silence();
				return false;
			}
			prev = peak;
		}
	}

	// And it sank by as much as it should have, give or take a round.
	float t = (float)(NBLK * BLK - WIN) / (float)(SECS * SAMPLE_RATE);
	int want = (int)(AMP * VOL * (1.0f - t));
	if (prev < want - 32 || prev > want + 8) {
		printf("FAILED slow ramp: peak %d after %d samples, want %d\n",
			prev, NBLK * BLK, want);
		silence();
		return false;
	}
	silence();
	return true;
}

// Expected playback frequency for a region / key (SF2 pitch formula).
static float expect_freq(const sf64_region_t *r, const sf64_sample_t *s, int key)
{
	float cents = (key - r->root_key) * (float)r->pitch_keytrack
		+ r->coarse_tune * 100.0f + r->fine_tune;
	return s->sample_rate * powf(2.0f, cents / 1200.0f);
}

// Advance of the playhead over @p nsamples at the current mixer frequency.
static bool check_freq(sf64_synth_t *synth, const char *tag, float want)
{
	(void)synth;
	double p0 = mixer_ch_get_pos(CH);
	mix(512);
	double got = (mixer_ch_get_pos(CH) - p0) * SAMPLE_RATE / 512.0;
	float err = got - want;
	if (err < 0) err = -err;
	if (err > want * 0.02f + 1.0f) {
		printf("FAILED %s: freq got %.1f want %.1f\n", tag, got, want);
		return false;
	}
	return true;
}

static bool test_synth_pitch(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));

	sf64_region_t *ra = &bank->regions[bank->presets[0].first_region];
	sf64_sample_t *sa = &bank->samples[ra->sample_index];

	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 60, 100)) {
		printf("FAILED pitch: note_on root\n");
		sf64_synth_close(synth);
		return false;
	}
	if (mixer_ch_playing_waveform(CH) != &bank->waves[ra->sample_index]->wave) {
		printf("FAILED pitch: wrong waveform at root\n");
		sf64_synth_close(synth);
		return false;
	}
	if (!check_freq(synth, "pitch root", expect_freq(ra, sa, 60))) {
		sf64_synth_close(synth);
		return false;
	}

	// −12 on region A (root 60); +12 on region C (root 72, key 84).
	sf64_region_t *rc = ra + 1;
	sf64_sample_t *sc = &bank->samples[rc->sample_index];
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 48, 100);
	if (!check_freq(synth, "pitch -12", expect_freq(ra, sa, 48))) {
		sf64_synth_close(synth);
		return false;
	}
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 84, 100);
	if (mixer_ch_playing_waveform(CH) != &bank->waves[rc->sample_index]->wave) {
		printf("FAILED pitch +12: wrong waveform\n");
		sf64_synth_close(synth);
		return false;
	}
	if (!check_freq(synth, "pitch +12", expect_freq(rc, sc, 84))) {
		sf64_synth_close(synth);
		return false;
	}
	// Expect ~2× the sample rate.
	if (fabsf(expect_freq(rc, sc, 84) - 2.0f * sc->sample_rate) > 1.0f) {
		printf("FAILED pitch +12: expect_freq not 2x rate\n");
		sf64_synth_close(synth);
		return false;
	}

	// Coarse / fine / keytrack by patching the live region, then restore.
	int8_t save_coarse = ra->coarse_tune;
	int16_t save_fine = ra->fine_tune;
	int16_t save_kt = ra->pitch_keytrack;
	ra->coarse_tune = 12;
	ra->fine_tune = 0;
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (!check_freq(synth, "pitch coarse+12", expect_freq(ra, sa, 60))) {
		ra->coarse_tune = save_coarse;
		sf64_synth_close(synth);
		return false;
	}
	ra->coarse_tune = 0;
	ra->fine_tune = 100; // +1 semitone
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (!check_freq(synth, "pitch fine+100", expect_freq(ra, sa, 60))) {
		ra->fine_tune = save_fine;
		sf64_synth_close(synth);
		return false;
	}
	ra->fine_tune = 0;
	ra->pitch_keytrack = 50; // half tracking
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 66, 100);
	if (!check_freq(synth, "pitch keytrack50", expect_freq(ra, sa, 66))) {
		ra->pitch_keytrack = save_kt;
		sf64_synth_close(synth);
		return false;
	}
	ra->coarse_tune = save_coarse;
	ra->fine_tune = save_fine;
	ra->pitch_keytrack = save_kt;

	sf64_synth_close(synth);
	return true;
}

static bool test_synth_key_split(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));

	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 60, 100) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[0].sample_index]->wave) {
		printf("FAILED key_split: low key\n");
		sf64_synth_close(synth);
		return false;
	}
	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 72, 100) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[1].sample_index]->wave) {
		printf("FAILED key_split: high key\n");
		sf64_synth_close(synth);
		return false;
	}
	// Boundary inclusivity: 71 → A, 72 → C.
	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 71, 100) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[0].sample_index]->wave) {
		printf("FAILED key_split: key 71 boundary\n");
		sf64_synth_close(synth);
		return false;
	}

	// Key outside every region: shrink both ranges, leave a hole at 60.
	uint8_t a_min = r[0].key_min, a_max = r[0].key_max;
	uint8_t c_min = r[1].key_min, c_max = r[1].key_max;
	r[0].key_min = 0;  r[0].key_max = 40;
	r[1].key_min = 80; r[1].key_max = 127;
	synth_silence(synth, 1);
	if (sf64_synth_note_on(synth, 0, 60, 100)) {
		printf("FAILED key_split: key outside ranges still played\n");
		r[0].key_min = a_min; r[0].key_max = a_max;
		r[1].key_min = c_min; r[1].key_max = c_max;
		sf64_synth_close(synth);
		return false;
	}
	if (mixer_ch_playing(CH)) {
		printf("FAILED key_split: channel active after unmatched note_on\n");
		r[0].key_min = a_min; r[0].key_max = a_max;
		r[1].key_min = c_min; r[1].key_max = c_max;
		sf64_synth_close(synth);
		return false;
	}
	r[0].key_min = a_min; r[0].key_max = a_max;
	r[1].key_min = c_min; r[1].key_max = c_max;

	sf64_synth_close(synth);
	return true;
}

static bool test_synth_vel_split(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 1));

	sf64_region_t *r = &bank->regions[bank->presets[1].first_region];
	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 60, 79) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[0].sample_index]->wave) {
		printf("FAILED vel_split: vel 79\n");
		sf64_synth_close(synth);
		return false;
	}
	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 60, 80) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[1].sample_index]->wave) {
		printf("FAILED vel_split: vel 80\n");
		sf64_synth_close(synth);
		return false;
	}
	// Upper bound inclusive.
	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 60, 127) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[1].sample_index]->wave) {
		printf("FAILED vel_split: vel 127\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_close(synth);
	return true;
}

static int16_t samples_to_timecents(int n)
{
	if (n <= 0) return -12000;
	return (int16_t)lroundf(1200.0f * log2f((float)n / (float)SAMPLE_RATE));
}

static int env_samples(int16_t tc)
{
	if (tc <= -12000) return 0;
	int n = (int)lroundf(powf(2.0f, tc / 1200.0f) * (float)SAMPLE_RATE);
	return n > 0 ? n : 0;
}

/** Match synth keynum scaling: base + scale*(60-key), then timecents→samples. */
static int env_scaled_samples_tc(int16_t base_tc, int16_t keynum_scale, int key)
{
	int tc = base_tc;
	if (keynum_scale)
		tc += (int)keynum_scale * (60 - key);
	if (tc < -12000) tc = -12000;
	if (tc > 8000) tc = 8000;
	return env_samples((int16_t)tc);
}

static int peak_n(int n)
{
	int peak = 0;
	for (int i = 0; i < n * 2; i++) {
		int a = out[i] < 0 ? -out[i] : out[i];
		if (a > peak) peak = a;
	}
	return peak;
}

static int count_playing(int nch)
{
	int n = 0;
	for (int i = 0; i < nch; i++)
		if (mixer_ch_playing(i)) n++;
	return n;
}

static bool wave_playing(int nch, waveform_t *w)
{
	for (int i = 0; i < nch; i++)
		if (mixer_ch_playing(i) && mixer_ch_playing_waveform(i) == w)
			return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////////
// Step 4 — polyphony and voice stealing
//////////////////////////////////////////////////////////////////////////////

/** Sequential alloc, free, reuse of the lowest free channel, range. */
static bool test_synth_allocator(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 3, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));

	synth_silence(synth, 3);
	if (!sf64_synth_note_on(synth, 0, 48, 100) ||
		!sf64_synth_note_on(synth, 0, 60, 100) ||
		!sf64_synth_note_on(synth, 0, 72, 100) ||
		!mixer_ch_playing(0) || !mixer_ch_playing(1) || !mixer_ch_playing(2)) {
		printf("FAILED alloc: sequential fill of ch0-2\n");
		sf64_synth_close(synth);
		return false;
	}

	// Free the middle channel; the next note must take the lowest free one.
	sf64_synth_note_off(synth, 0, 60);
	mix(64);
	if (mixer_ch_playing(1)) {
		printf("FAILED alloc: note-off did not free ch1\n");
		sf64_synth_close(synth);
		return false;
	}
	if (!sf64_synth_note_on(synth, 0, 64, 100) || !mixer_ch_playing(1) ||
		!mixer_ch_playing(0) || !mixer_ch_playing(2)) {
		printf("FAILED alloc: reuse did not pick the lowest free channel\n");
		sf64_synth_close(synth);
		return false;
	}

	// first_channel=1, num=2 → only ch1/ch2, never ch0 or ch3.
	synth_silence(synth, NCH);
	sf64_synth_set_channels(synth, 1, 2, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (mixer_ch_playing(0) || !mixer_ch_playing(1) || !mixer_ch_playing(2) ||
		mixer_ch_playing(3)) {
		printf("FAILED alloc: out-of-range channel used\n");
		sf64_synth_close(synth);
		return false;
	}
	// Saturated range: steals inside [1,2], still never touches ch0/ch3.
	if (!sf64_synth_note_on(synth, 0, 72, 100) || mixer_ch_playing(0) ||
		mixer_ch_playing(3) ||
		(int)mixer_ch_playing(1) + (int)mixer_ch_playing(2) != 2) {
		printf("FAILED alloc: steal escaped the reserved range\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

/** Chord, note-off of one key, release frees only at deadline, reuse after. */
static bool test_synth_polyphony(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 3, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));

	synth_silence(synth, 3);
	if (!sf64_synth_note_on(synth, 0, 48, 100) ||
		!sf64_synth_note_on(synth, 0, 60, 100) ||
		!sf64_synth_note_on(synth, 0, 72, 100)) {
		printf("FAILED poly: could not start three notes\n");
		sf64_synth_close(synth);
		return false;
	}
	if (!mixer_ch_playing(0) || !mixer_ch_playing(1) || !mixer_ch_playing(2)) {
		printf("FAILED poly: expected channels 0-2 playing\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_note_off(synth, 0, 60);
	mix(64);
	if (!mixer_ch_playing(0) || mixer_ch_playing(1) || !mixer_ch_playing(2)) {
		printf("FAILED poly: note-off 60 should free only ch1\n");
		sf64_synth_close(synth);
		return false;
	}

	// Release with non-zero envelope: channel freed only at the deadline.
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	int16_t save_rel = r->amp_env.release_timecents;
	r->amp_env.release_timecents = samples_to_timecents(400);
	int rel = env_samples(r->amp_env.release_timecents);
	synth_silence(synth, 3);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_off(synth, 0, 48);
	if (!mixer_ch_playing(0)) {
		printf("FAILED poly: release stopped immediately\n");
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	int mid = sf64_synth_process(synth, rel / 2);
	if (!mixer_ch_playing(0) || mid != rel - rel / 2) {
		printf("FAILED poly: release mid remaining %d\n", mid);
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_process(synth, mid);
	mix(64);
	if (mixer_ch_playing(0)) {
		printf("FAILED poly: channel not freed after release deadline\n");
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	if (!sf64_synth_note_on(synth, 0, 60, 100) || !mixer_ch_playing(0)) {
		printf("FAILED poly: could not reuse freed channel\n");
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	r->amp_env.release_timecents = save_rel;

	// note-off wrong key ignored; velocity 0 == note-off; repeated off ok.
	synth_silence(synth, 3);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 48);
	if (!mixer_ch_playing(0)) {
		printf("FAILED poly: wrong-key note-off stopped the note\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_note_on(synth, 0, 60, 0);
	mix(64);
	if (mixer_ch_playing(0)) {
		printf("FAILED poly: velocity 0 did not note-off\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_note_off(synth, 0, 60);
	sf64_synth_note_off(synth, 0, 60);

	sf64_synth_close(synth);
	return true;
}

/** Full pool: new note steals a voice; polyphony stays at the limit. */
static bool test_synth_saturation(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 3, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));

	synth_silence(synth, 3);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_on(synth, 0, 72, 100);
	mix(128);

	uint32_t id = sf64_synth_note_on(synth, 0, 64, 100);
	if (!id) {
		printf("FAILED sat: note rejected with full pool (stealing expected)\n");
		sf64_synth_close(synth);
		return false;
	}
	if (count_playing(3) != 3) {
		printf("FAILED sat: after steal playing=%d, want 3\n", count_playing(3));
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

/** Voice stealing: phase order, age tie-break, layers, exclusive, stress. */
static bool test_synth_voice_steal(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_region_t *r0 = &bank->regions[bank->presets[0].first_region];
	int16_t save_rel0 = r0[0].amp_env.release_timecents;
	int16_t save_rel1 = r0[1].amp_env.release_timecents;
	int16_t save_atk1 = r0[1].amp_env.attack_timecents;
	waveform_t *wc = &bank->waves[r0[1].sample_index]->wave;

	// Release is cheaper than sustain.
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	r0[0].amp_env.release_timecents = samples_to_timecents(2000);
	r0[1].amp_env.release_timecents = samples_to_timecents(2000);
	synth_silence(synth, 2);
	sf64_synth_note_on(synth, 0, 48, 100); // A
	sf64_synth_note_on(synth, 0, 72, 100); // C
	sf64_synth_note_off(synth, 0, 48);     // A → RELEASE
	if (!sf64_synth_note_on(synth, 0, 60, 100) || count_playing(2) != 2 ||
		!wave_playing(2, wc)) {
		printf("FAILED steal: release was not preferred over sustain\n");
		goto fail_steal;
	}

	// Sustain is cheaper than attack.
	r0[1].amp_env.attack_timecents = samples_to_timecents(2000);
	synth_silence(synth, 2);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100); // A sustain
	sf64_synth_note_on(synth, 0, 72, 100); // C attack
	if (!sf64_synth_note_on(synth, 0, 60, 100) || count_playing(2) != 2 ||
		!wave_playing(2, wc)) {
		printf("FAILED steal: sustain was not preferred over attack\n");
		goto fail_steal;
	}
	r0[1].amp_env.attack_timecents = save_atk1;

	// Same phase: oldest is stolen.
	r0[0].amp_env.release_timecents = save_rel0;
	r0[1].amp_env.release_timecents = save_rel1;
	synth_silence(synth, 2);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100);
	mix(512);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (!sf64_synth_note_on(synth, 0, 64, 100)) {
		printf("FAILED steal: oldest tie-break note rejected\n");
		goto fail_steal;
	}
	sf64_synth_note_off(synth, 0, 60);
	mix(64);
	if (count_playing(2) != 1) {
		printf("FAILED steal: expected oldest (48) stolen, playing=%d\n",
			count_playing(2));
		goto fail_steal;
	}

	// Layer3 needs 3 slots: steals a full pool of mono notes.
	sf64_synth_set_channels(synth, 0, 3, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 3);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_on(synth, 0, 65, 100);
	assert(sf64_synth_set_program(synth, 0, 0, 4)); // Layer3
	uint32_t lid = sf64_synth_note_on(synth, 0, 72, 100);
	if (!lid || count_playing(3) != 3) {
		printf("FAILED steal: Layer3 did not claim 3 slots (id=%lu playing=%d)\n",
			(unsigned long)lid, count_playing(3));
		goto fail_steal;
	}

	// Stealing a layered identity removes all of its layers.
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 3)); // Layer2
	synth_silence(synth, 2);
	sf64_synth_note_on(synth, 0, 60, 100); // fills both channels
	mix(64);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	// Mono note steals one layer channel; expansion must clear the other too.
	if (!sf64_synth_note_on(synth, 0, 64, 100) || count_playing(2) != 1) {
		printf("FAILED steal: layered victim not expanded (playing=%d)\n",
			count_playing(2));
		goto fail_steal;
	}

	// Exclusive victim is preferred (advertised at MIN).
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	uint8_t g0 = r0[0].exclusive_group, g1 = r0[1].exclusive_group;
	r0[0].exclusive_group = 1;
	r0[1].exclusive_group = 1;
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 2);
	sf64_synth_note_on(synth, 0, 48, 100); // A group 1
	sf64_synth_note_on(synth, 0, 60, 100); // A group 1
	if (!sf64_synth_note_on(synth, 0, 72, 100) || count_playing(2) != 1 ||
		!wave_playing(2, wc)) {
		printf("FAILED steal: exclusive victims not reclaimed (playing=%d)\n",
			count_playing(2));
		r0[0].exclusive_group = g0;
		r0[1].exclusive_group = g1;
		goto fail_steal;
	}
	r0[0].exclusive_group = g0;
	r0[1].exclusive_group = g1;

	// Stress: rapid note-on/off must not stick or over-allocate.
	sf64_synth_set_channels(synth, 0, 4, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 4);
	for (int i = 0; i < 2000; i++) {
		int key = 48 + (i % 24);
		sf64_synth_note_on(synth, 0, key, 100);
		if ((i & 3) == 3)
			sf64_synth_note_off(synth, 0, 48 + ((i - 3) % 24));
		if ((i & 15) == 15) {
			sf64_synth_process(synth, 64);
			mix(64);
		}
	}
	if (count_playing(4) > 4) {
		printf("FAILED steal: stress over-allocated (%d)\n", count_playing(4));
		goto fail_steal;
	}
	synth_silence(synth, 4);
	if (count_playing(4) != 0) {
		printf("FAILED steal: stress left stuck voices\n");
		goto fail_steal;
	}

	r0[0].amp_env.release_timecents = save_rel0;
	r0[1].amp_env.release_timecents = save_rel1;
	r0[1].amp_env.attack_timecents = save_atk1;
	sf64_synth_close(synth);
	return true;

fail_steal:
	r0[0].amp_env.release_timecents = save_rel0;
	r0[1].amp_env.release_timecents = save_rel1;
	r0[1].amp_env.attack_timecents = save_atk1;
	sf64_synth_close(synth);
	return false;
}

//////////////////////////////////////////////////////////////////////////////
// Step 5 — layers and note identity
//////////////////////////////////////////////////////////////////////////////

/** Matching regions start together; non-matching ones are ignored. */
static bool test_synth_layers(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 4, MIXER_PRIORITY_MUSIC);

	// Layer2: two full-range regions → two voices, distinct samples.
	assert(sf64_synth_set_program(synth, 0, 0, 3));
	sf64_region_t *r2 = &bank->regions[bank->presets[3].first_region];
	waveform_t *wa = &bank->waves[r2[0].sample_index]->wave;
	waveform_t *wc = &bank->waves[r2[1].sample_index]->wave;
	synth_silence(synth, 4);
	uint32_t id = sf64_synth_note_on(synth, 0, 60, 100);
	if (id == 0 || count_playing(4) != 2 ||
		!wave_playing(4, wa) || !wave_playing(4, wc)) {
		printf("FAILED layers: Layer2 did not start two distinct voices\n");
		sf64_synth_close(synth);
		return false;
	}

	// Layer3: three voices.
	assert(sf64_synth_set_program(synth, 0, 0, 4));
	sf64_region_t *r3 = &bank->regions[bank->presets[4].first_region];
	synth_silence(synth, 4);
	id = sf64_synth_note_on(synth, 0, 60, 100);
	if (id == 0 || count_playing(4) != 3 ||
		!wave_playing(4, &bank->waves[r3[0].sample_index]->wave) ||
		!wave_playing(4, &bank->waves[r3[1].sample_index]->wave) ||
		!wave_playing(4, &bank->waves[r3[2].sample_index]->wave)) {
		printf("FAILED layers: Layer3 did not start three voices\n");
		sf64_synth_close(synth);
		return false;
	}

	// KeySplit: key 60 matches only region A (C starts at 72).
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 4);
	id = sf64_synth_note_on(synth, 0, 60, 100);
	sf64_region_t *rk = &bank->regions[bank->presets[0].first_region];
	if (id == 0 || count_playing(4) != 1 ||
		!wave_playing(4, &bank->waves[rk[0].sample_index]->wave) ||
		wave_playing(4, &bank->waves[rk[1].sample_index]->wave)) {
		printf("FAILED layers: incompatible region was not ignored\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

/**
 * Layered note that needs more channels than the pool has is rejected with
 * no partial state. With room to steal, a layered note takes free + stolen.
 */
static bool test_synth_layer_atomic(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 3, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));

	synth_silence(synth, 3);
	// Occupy two channels; one free. Layer2 needs two → steals one.
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 60, 100);
	mix(64);
	assert(sf64_synth_set_program(synth, 0, 0, 3));
	if (!sf64_synth_note_on(synth, 0, 72, 100) || count_playing(3) != 3) {
		printf("FAILED atomic: layered note should steal to start (playing=%d)\n",
			count_playing(3));
		sf64_synth_close(synth);
		return false;
	}

	// Pool smaller than the layer count → reject, leave the mono note alone.
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 48, 100);
	mix(64);
	waveform_t *w0 = mixer_ch_playing_waveform(0);
	double p0 = mixer_ch_get_pos(0);
	assert(sf64_synth_set_program(synth, 0, 0, 3));
	if (sf64_synth_note_on(synth, 0, 72, 100) != 0) {
		printf("FAILED atomic: layered note started with pool size 1\n");
		sf64_synth_close(synth);
		return false;
	}
	if (mixer_ch_playing_waveform(0) != w0 || mixer_ch_get_pos(0) != p0) {
		printf("FAILED atomic: rejected layer disturbed the existing voice\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

/**
 * Stacked note-ons on the same key get distinct identities; note_off peels
 * the oldest identity (and all of its layers) first.
 */
static bool test_synth_note_identity(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 4, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 3)); // Layer2
	sf64_region_t *r = &bank->regions[bank->presets[3].first_region];
	waveform_t *wa = &bank->waves[r[0].sample_index]->wave;
	waveform_t *wc = &bank->waves[r[1].sample_index]->wave;

	synth_silence(synth, 4);
	uint32_t id1 = sf64_synth_note_on(synth, 0, 60, 100);
	uint32_t id2 = sf64_synth_note_on(synth, 0, 60, 100);
	if (id1 == 0 || id2 == 0 || id1 == id2 || count_playing(4) != 4) {
		printf("FAILED identity: stacked Layer2 note-ons (id1=%lu id2=%lu playing=%d)\n",
			(unsigned long)id1, (unsigned long)id2, count_playing(4));
		sf64_synth_close(synth);
		return false;
	}
	if (!wave_playing(4, wa) || !wave_playing(4, wc)) {
		printf("FAILED identity: layered samples missing after stack\n");
		sf64_synth_close(synth);
		return false;
	}

	// First note_off releases only the oldest identity (2 voices).
	sf64_synth_note_off(synth, 0, 60);
	mix(64);
	if (count_playing(4) != 2) {
		printf("FAILED identity: first note-off left %d voices, want 2\n",
			count_playing(4));
		sf64_synth_close(synth);
		return false;
	}
	if (!wave_playing(4, wa) || !wave_playing(4, wc)) {
		printf("FAILED identity: second identity lost a layer\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_note_off(synth, 0, 60);
	mix(64);
	if (count_playing(4) != 0) {
		printf("FAILED identity: second note-off left %d voices\n",
			count_playing(4));
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_note_off(synth, 0, 60); // no-op

	sf64_synth_close(synth);
	return true;
}

/** Independent attack/release deadlines across voices. */
static bool test_synth_deadlines(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r0 = &bank->regions[bank->presets[0].first_region];
	sf64_region_t *r1 = r0 + 1;
	int16_t save_atk0 = r0->amp_env.attack_timecents;
	int16_t save_atk1 = r1->amp_env.attack_timecents;
	int16_t save_rel0 = r0->amp_env.release_timecents;
	int16_t save_rel1 = r1->amp_env.release_timecents;

	// Two attacks with different deadlines → process returns the sooner one.
	r0->amp_env.attack_timecents = samples_to_timecents(800);
	r1->amp_env.attack_timecents = samples_to_timecents(200);
	int atk0 = env_samples(r0->amp_env.attack_timecents);
	int atk1 = env_samples(r1->amp_env.attack_timecents);
	synth_silence(synth, 2);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100); // region A
	sf64_synth_note_on(synth, 0, 72, 100); // region C
	int dl = sf64_synth_process(synth, 0);
	if (dl != atk1) {
		printf("FAILED deadlines: soonest attack %d want %d\n", dl, atk1);
		goto fail_dl;
	}
	// Several process(0) calls before the deadline leave it unchanged.
	if (sf64_synth_process(synth, 0) != atk1 ||
		sf64_synth_process(synth, 0) != atk1) {
		printf("FAILED deadlines: process(0) changed the pending attack\n");
		goto fail_dl;
	}
	dl = sf64_synth_process(synth, atk1 / 2);
	if (dl != atk1 - atk1 / 2) {
		printf("FAILED deadlines: mid-attack remaining %d want %d\n",
			dl, atk1 - atk1 / 2);
		goto fail_dl;
	}
	dl = sf64_synth_process(synth, dl);
	if (dl != atk0 - atk1) {
		printf("FAILED deadlines: after first attack remaining %d want %d\n",
			dl, atk0 - atk1);
		goto fail_dl;
	}
	sf64_synth_process(synth, dl);
	if (sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED deadlines: expected no deadline in dual sustain\n");
		goto fail_dl;
	}

	// Two equal releases; a late process past both deadlines frees both.
	r0->amp_env.attack_timecents = -12000;
	r1->amp_env.attack_timecents = -12000;
	r0->amp_env.release_timecents = samples_to_timecents(300);
	r1->amp_env.release_timecents = samples_to_timecents(300);
	int rel = env_samples(r0->amp_env.release_timecents);
	synth_silence(synth, 2);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 72, 100);
	sf64_synth_note_off(synth, 0, 48);
	sf64_synth_note_off(synth, 0, 72);
	dl = sf64_synth_process(synth, 0);
	if (dl != rel) {
		printf("FAILED deadlines: equal release remaining %d want %d\n", dl, rel);
		goto fail_dl;
	}
	sf64_synth_process(synth, rel + 50);
	mix(64);
	if (mixer_ch_playing(0) || mixer_ch_playing(1)) {
		printf("FAILED deadlines: late process did not free both channels\n");
		goto fail_dl;
	}

	r0->amp_env.attack_timecents = save_atk0;
	r1->amp_env.attack_timecents = save_atk1;
	r0->amp_env.release_timecents = save_rel0;
	r1->amp_env.release_timecents = save_rel1;
	sf64_synth_close(synth);
	return true;

fail_dl:
	r0->amp_env.attack_timecents = save_atk0;
	r1->amp_env.attack_timecents = save_atk1;
	r0->amp_env.release_timecents = save_rel0;
	r1->amp_env.release_timecents = save_rel1;
	sf64_synth_close(synth);
	return false;
}

static bool test_synth_preset(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);

	if (sf64_synth_set_program(synth, 0, 0, 99)) {
		printf("FAILED preset: missing program accepted\n");
		sf64_synth_close(synth);
		return false;
	}
	if (!sf64_synth_set_program(synth, 0, 0, 0)) {
		printf("FAILED preset: program 0 rejected\n");
		sf64_synth_close(synth);
		return false;
	}

	// Change preset while a note is sounding: the sounding wave stays,
	// a note-on on a different key uses the new preset on another channel.
	synth_silence(synth, 2);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_synth_note_on(synth, 0, 48, 100);
	waveform_t *before = mixer_ch_playing_waveform(CH);
	assert(sf64_synth_set_program(synth, 0, 0, 1));
	if (mixer_ch_playing_waveform(CH) != before) {
		printf("FAILED preset: change interrupted the sounding note\n");
		sf64_synth_close(synth);
		return false;
	}
	// High velocity on VelSplit → sample D on ch1 (key 60 ≠ 48, so no retrigger).
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_region_t *rd = &bank->regions[bank->presets[1].first_region + 1];
	if (mixer_ch_playing_waveform(1) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED preset: next note did not use new preset\n");
		sf64_synth_close(synth);
		return false;
	}
	if (mixer_ch_playing_waveform(CH) != before) {
		printf("FAILED preset: retriggered the previous key\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

//////////////////////////////////////////////////////////////////////////////
// Attack / release / sustain-loop (real mixer, virtual clock)
//////////////////////////////////////////////////////////////////////////////

static bool test_synth_attack(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	int16_t save_atk = r->amp_env.attack_timecents;

	// Zero attack → immediate sustain, no deadline.
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED attack0: expected no deadline\n");
		sf64_synth_close(synth);
		return false;
	}
	mix(128);
	if (peak_n(128) < 1024) {
		printf("FAILED attack0: silent after note_on\n");
		sf64_synth_close(synth);
		return false;
	}

	// Non-zero attack on sample C (constant amplitude square): early quieter
	// than post-attack, then sustain with matched L/R.
	sf64_region_t *rc = r + 1;
	int16_t save_atk_c = rc->amp_env.attack_timecents;
	rc->amp_env.attack_timecents = samples_to_timecents(512);
	int atk = env_samples(rc->amp_env.attack_timecents);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 100);
	int dl = sf64_synth_process(synth, 0);
	if (dl != atk) {
		printf("FAILED attack: remaining %d want %d\n", dl, atk);
		rc->amp_env.attack_timecents = save_atk_c;
		sf64_synth_close(synth);
		return false;
	}
	int mid = sf64_synth_process(synth, atk / 4);
	if (mid != atk - atk / 4) {
		printf("FAILED attack: remaining after %d is %d want %d\n",
			atk / 4, mid, atk - atk / 4);
		rc->amp_env.attack_timecents = save_atk_c;
		sf64_synth_close(synth);
		return false;
	}
	mix(32);
	int early = peak_n(32);
	mix((atk - 32 + 1) & ~1);
	sf64_synth_process(synth, mid); // → sustain
	if (sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED attack: not in sustain after deadline\n");
		rc->amp_env.attack_timecents = save_atk_c;
		sf64_synth_close(synth);
		return false;
	}
	mix(128);
	int late = peak_n(128);
	if (late < 1024 || early * 2 > late) {
		printf("FAILED attack: early peak %d late %d\n", early, late);
		rc->amp_env.attack_timecents = save_atk_c;
		sf64_synth_close(synth);
		return false;
	}
	int l = 0, rr = 0;
	for (int i = 0; i < 128; i++) {
		int a = out[i * 2] < 0 ? -out[i * 2] : out[i * 2];
		int b = out[i * 2 + 1] < 0 ? -out[i * 2 + 1] : out[i * 2 + 1];
		if (a > l) l = a;
		if (b > rr) rr = b;
	}
	int d = l - rr; if (d < 0) d = -d;
	if (d > 64) {
		printf("FAILED attack: L/R mismatch %d/%d\n", l, rr);
		rc->amp_env.attack_timecents = save_atk_c;
		sf64_synth_close(synth);
		return false;
	}

	r->amp_env.attack_timecents = save_atk;
	rc->amp_env.attack_timecents = save_atk_c;
	sf64_synth_close(synth);
	return true;
}

static bool test_synth_release(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	int16_t save_atk = r->amp_env.attack_timecents;
	int16_t save_rel = r->amp_env.release_timecents;

	// Zero release in sustain → immediate stop.
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 60);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED release0: still playing\n");
		sf64_synth_close(synth);
		return false;
	}

	// Note-off during attack: release ramp replaces attack.
	r->amp_env.attack_timecents = samples_to_timecents(800);
	r->amp_env.release_timecents = samples_to_timecents(400);
	int atk = env_samples(r->amp_env.attack_timecents);
	int rel = env_samples(r->amp_env.release_timecents);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	mix(64);
	sf64_synth_process(synth, 64); // still in attack
	sf64_synth_note_off(synth, 0, 60);
	int dl = sf64_synth_process(synth, 0);
	if (dl != rel) {
		printf("FAILED release-in-attack: remaining %d want %d\n", dl, rel);
		r->amp_env.attack_timecents = save_atk;
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	if (!mixer_ch_playing(CH)) {
		printf("FAILED release-in-attack: stopped too early\n");
		r->amp_env.attack_timecents = save_atk;
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	mix(rel);
	// Late / overdue process still stops.
	sf64_synth_process(synth, rel + 100);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED release-in-attack: late process did not stop\n");
		r->amp_env.attack_timecents = save_atk;
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}

	// Note-off in sustain with non-zero release.
	r->amp_env.attack_timecents = -12000;
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 60);
	dl = sf64_synth_process(synth, 0);
	if (dl != rel) {
		printf("FAILED release-sustain: remaining %d want %d\n", dl, rel);
		r->amp_env.attack_timecents = save_atk;
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	mix(rel / 2);
	dl = sf64_synth_process(synth, rel / 2);
	if (!mixer_ch_playing(CH) || dl != rel - rel / 2) {
		printf("FAILED release-sustain: early stop or remaining %d\n", dl);
		r->amp_env.attack_timecents = save_atk;
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_process(synth, dl);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED release-sustain: not stopped at deadline\n");
		r->amp_env.attack_timecents = save_atk;
		r->amp_env.release_timecents = save_rel;
		sf64_synth_close(synth);
		return false;
	}

	(void)atk;
	r->amp_env.attack_timecents = save_atk;
	r->amp_env.release_timecents = save_rel;
	sf64_synth_close(synth);
	return true;
}

static bool test_synth_loop_env(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);

	// Sustain loop (preset 2 / sample B): note-off disables loop, pos leaves the loop.
	assert(sf64_synth_set_program(synth, 0, 0, 2));
	sf64_region_t *r = &bank->regions[bank->presets[2].first_region];
	int16_t save_rel = r->amp_env.release_timecents;
	uint8_t save_mode = r->loop_mode;
	r->amp_env.release_timecents = samples_to_timecents(600);
	int rel = env_samples(r->amp_env.release_timecents);

	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	// Land inside the loop.
	mixer_ch_set_freq(CH, 0);
	mixer_ch_set_pos(CH, B_INTRO + 8);
	mix(2048);
	mixer_ch_set_freq(CH, SAMPLE_RATE);
	sf64_synth_note_off(synth, 0, 60);
	mix(256);
	double pos = mixer_ch_get_pos(CH);
	if (pos < B_INTRO + B_LOOP) {
		// May still be before loop_end early in the release; mix more.
		mix(B_LOOP);
		pos = mixer_ch_get_pos(CH);
	}
	if (pos < B_INTRO + B_LOOP) {
		printf("FAILED sustain-loop: pos %.1f still inside loop after note-off\n", pos);
		r->amp_env.release_timecents = save_rel;
		r->loop_mode = save_mode;
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_process(synth, rel);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED sustain-loop: not stopped after release\n");
		r->amp_env.release_timecents = save_rel;
		r->loop_mode = save_mode;
		sf64_synth_close(synth);
		return false;
	}

	// Continuous loop: note-off must NOT disable the loop (pos stays in range).
	r->loop_mode = SF64_LOOP_CONTINUOUS;
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	mixer_ch_set_freq(CH, 0);
	mixer_ch_set_pos(CH, B_INTRO + 8);
	mix(2048);
	mixer_ch_set_freq(CH, SAMPLE_RATE);
	sf64_synth_note_off(synth, 0, 60);
	mix(B_LOOP + 256);
	pos = mixer_ch_get_pos(CH);
	if (pos < B_INTRO || pos >= B_INTRO + B_LOOP) {
		printf("FAILED continuous-loop: pos %.1f left the loop after note-off\n", pos);
		r->amp_env.release_timecents = save_rel;
		r->loop_mode = save_mode;
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_process(synth, rel);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED continuous-loop: not stopped after release\n");
		r->amp_env.release_timecents = save_rel;
		r->loop_mode = save_mode;
		sf64_synth_close(synth);
		return false;
	}

	// No-loop region: note-off with release still works (no set_loop).
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *ra = &bank->regions[bank->presets[0].first_region];
	int16_t save_rel_a = ra->amp_env.release_timecents;
	ra->amp_env.release_timecents = samples_to_timecents(300);
	rel = env_samples(ra->amp_env.release_timecents);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 60);
	if (!mixer_ch_playing(CH)) {
		printf("FAILED oneshot-release: not playing during release\n");
		ra->amp_env.release_timecents = save_rel_a;
		r->amp_env.release_timecents = save_rel;
		r->loop_mode = save_mode;
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_process(synth, rel);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED oneshot-release: not stopped\n");
		ra->amp_env.release_timecents = save_rel_a;
		r->amp_env.release_timecents = save_rel;
		r->loop_mode = save_mode;
		sf64_synth_close(synth);
		return false;
	}

	ra->amp_env.release_timecents = save_rel_a;
	r->amp_env.release_timecents = save_rel;
	r->loop_mode = save_mode;
	sf64_synth_close(synth);
	return true;
}

//////////////////////////////////////////////////////////////////////////////
// Step 6 — 16 MIDI channels, controllers
//////////////////////////////////////////////////////////////////////////////

static void peaks_lr(int n, int *lpeak, int *rpeak)
{
	int l = 0, r = 0;
	for (int i = 0; i < n; i++) {
		int a = out[i * 2] < 0 ? -out[i * 2] : out[i * 2];
		int b = out[i * 2 + 1] < 0 ? -out[i * 2 + 1] : out[i * 2 + 1];
		if (a > l) l = a;
		if (b > r) r = b;
	}
	*lpeak = l;
	*rpeak = r;
}

static float expect_freq_bend(const sf64_region_t *r, const sf64_sample_t *s,
	int key, int pitch_bend, int range_cents)
{
	float bend = (pitch_bend - 8192) * (float)range_cents / 8192.0f;
	float cents = (key - r->root_key) * (float)r->pitch_keytrack
		+ r->coarse_tune * 100.0f + r->fine_tune + bend;
	return s->sample_rate * powf(2.0f, cents / 1200.0f);
}

/** Independent MIDI channels, programs, controllers, pitch bend. */
static bool test_synth_midi_channels(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 4, MIXER_PRIORITY_MUSIC);

	// Different programs on ch 0 / ch 1.
	assert(sf64_synth_set_program(synth, 0, 0, 0)); // KeySplit → A at 48
	assert(sf64_synth_set_program(synth, 1, 0, 1)); // VelSplit → D at vel 100
	synth_silence(synth, 4);
	if (!sf64_synth_note_on(synth, 0, 48, 100) ||
		!sf64_synth_note_on(synth, 1, 60, 100)) {
		printf("FAILED midi: could not start notes on two channels\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_region_t *ra = &bank->regions[bank->presets[0].first_region];
	sf64_region_t *rd = &bank->regions[bank->presets[1].first_region + 1];
	if (mixer_ch_playing_waveform(0) != &bank->waves[ra->sample_index]->wave ||
		mixer_ch_playing_waveform(1) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED midi: wrong presets on channels\n");
		sf64_synth_close(synth);
		return false;
	}

	// Program change on ch 0: sounding note keeps sample A; new note uses VelSplit.
	assert(sf64_synth_set_program(synth, 0, 0, 1));
	if (mixer_ch_playing_waveform(0) != &bank->waves[ra->sample_index]->wave) {
		printf("FAILED midi: program change interrupted sounding note\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_note_on(synth, 0, 60, 100);
	if (mixer_ch_playing_waveform(2) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED midi: new note after program change wrong sample\n");
		sf64_synth_close(synth);
		return false;
	}

	// note-off on ch 1 must not release ch 0's key 60 (different MIDI channel).
	sf64_synth_note_off(synth, 1, 60);
	mix(64);
	sf64_synth_process(synth, 64);
	if (!mixer_ch_playing(0) || !mixer_ch_playing(2) || mixer_ch_playing(1)) {
		printf("FAILED midi: note-off crossed MIDI channels\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

static bool test_synth_midi_controllers(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	assert(sf64_synth_set_program(synth, 1, 0, 0));

	// Volume follows the SF2 (x/127)² curve, not a linear CC/127: half CC7 is
	// a quarter of the peak on constant-amplitude sample C (key 72).
	sf64_synth_set_volume(synth, 0, 127);
	synth_silence(synth, 2);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(256);
	int full = peak_n(256);
	sf64_synth_set_volume(synth, 0, 64);
	mix(256);
	int quarter = peak_n(256);
	if (full < 1024 || quarter * 3 > full || quarter * 5 < full) {
		printf("FAILED midi vol: full %d vol64 %d (want ~4×)\n", full, quarter);
		sf64_synth_close(synth);
		return false;
	}

	// Expression on ch 0 must not change ch 1.
	synth_silence(synth, 2);
	sf64_synth_set_volume(synth, 0, 127);
	sf64_synth_set_expression(synth, 0, 127);
	sf64_synth_set_expression(synth, 1, 127);
	sf64_synth_note_on(synth, 0, 72, 127);
	sf64_synth_note_on(synth, 1, 72, 127);
	mix(128);
	// Isolate ch0, then drop expression.
	sf64_synth_note_off(synth, 1, 72);
	mix(64);
	sf64_synth_set_expression(synth, 0, 32);
	mix(256);
	int quiet = peak_n(256);
	if (quiet * 3 > full) {
		printf("FAILED midi expr: peak %d after expr 32 (full was %d)\n",
			quiet, full);
		sf64_synth_close(synth);
		return false;
	}

	// Pan hard left / hard right on sample C.
	synth_silence(synth, 2);
	sf64_synth_set_expression(synth, 0, 127);
	sf64_synth_set_pan(synth, 0, 0);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(256);
	int lp, rp;
	peaks_lr(256, &lp, &rp);
	if (lp < 1024 || rp * 4 > lp) {
		printf("FAILED midi pan left: L=%d R=%d\n", lp, rp);
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_set_pan(synth, 0, 127);
	mix(256);
	peaks_lr(256, &lp, &rp);
	if (rp < 1024 || lp * 4 > rp) {
		printf("FAILED midi pan right: L=%d R=%d\n", lp, rp);
		sf64_synth_close(synth);
		return false;
	}

	// Pitch bend min / center / max (±200 cents default).
	sf64_region_t *rc = &bank->regions[bank->presets[0].first_region + 1];
	sf64_sample_t *sc = &bank->samples[rc->sample_index];
	synth_silence(synth, 2);
	sf64_synth_set_pan(synth, 0, 64);
	sf64_synth_note_on(synth, 0, 72, 100);
	sf64_synth_set_pitch_bend(synth, 0, 8192);
	if (!check_freq(synth, "bend center",
			expect_freq_bend(rc, sc, 72, 8192, 200))) {
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_set_pitch_bend(synth, 0, 0);
	if (!check_freq(synth, "bend min",
			expect_freq_bend(rc, sc, 72, 0, 200))) {
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_set_pitch_bend(synth, 0, 16383);
	if (!check_freq(synth, "bend max",
			expect_freq_bend(rc, sc, 72, 16383, 200))) {
		sf64_synth_close(synth);
		return false;
	}

	// Bend on ch 0 must not move ch 1's frequency.
	synth_silence(synth, 2);
	sf64_synth_set_pitch_bend(synth, 0, 8192);
	sf64_synth_set_pitch_bend(synth, 1, 8192);
	sf64_synth_note_on(synth, 0, 72, 100);
	sf64_synth_note_on(synth, 1, 72, 100);
	sf64_synth_set_pitch_bend(synth, 0, 0);
	double p0 = mixer_ch_get_pos(1);
	mix(512);
	double got = (mixer_ch_get_pos(1) - p0) * SAMPLE_RATE / 512.0;
	float want = expect_freq(rc, sc, 72);
	float err = got - want;
	if (err < 0) err = -err;
	if (err > want * 0.02f + 1.0f) {
		printf("FAILED midi bend isolation: ch1 freq %.1f want %.1f\n",
			got, want);
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

//////////////////////////////////////////////////////////////////////////////
// Sustain pedal / exclusive class
//////////////////////////////////////////////////////////////////////////////

static int count_playing_n(int n)
{
	int c = 0;
	for (int i = 0; i < n; i++)
		if (mixer_ch_playing(i))
			c++;
	return c;
}

static bool test_synth_sustain_pedal(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 4, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	int16_t save_rel = r->amp_env.release_timecents;
	r->amp_env.release_timecents = samples_to_timecents(400);
	int rel = env_samples(r->amp_env.release_timecents);

	// Note-off without pedal → immediate release.
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 0);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 60);
	int dl = sf64_synth_process(synth, 0);
	if (dl != rel || !mixer_ch_playing(0)) {
		printf("FAILED pedal: note-off without pedal remaining %d\n", dl);
		goto fail_pedal;
	}
	sf64_synth_process(synth, rel);
	mix(64);
	if (mixer_ch_playing(0)) {
		printf("FAILED pedal: still playing after release\n");
		goto fail_pedal;
	}

	// Note-off with pedal → keep sounding; pedal up → release.
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 60);
	if (!mixer_ch_playing(0) || sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED pedal: note-off with pedal entered release\n");
		goto fail_pedal;
	}
	sf64_synth_set_sustain(synth, 0, 0);
	dl = sf64_synth_process(synth, 0);
	if (dl != rel || !mixer_ch_playing(0)) {
		printf("FAILED pedal: pedal-up remaining %d want %d\n", dl, rel);
		goto fail_pedal;
	}
	sf64_synth_process(synth, rel);
	mix(64);
	if (mixer_ch_playing(0)) {
		printf("FAILED pedal: not stopped after pedal-up release\n");
		goto fail_pedal;
	}

	// Multiple notes held by pedal; pedal up releases all.
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 48);
	sf64_synth_note_off(synth, 0, 60);
	if (count_playing_n(4) != 2) {
		printf("FAILED pedal: expected 2 held notes, got %d\n",
			count_playing_n(4));
		goto fail_pedal;
	}
	sf64_synth_set_sustain(synth, 0, 0);
	sf64_synth_process(synth, rel);
	mix(64);
	if (count_playing_n(4) != 0) {
		printf("FAILED pedal: multi hold not cleared\n");
		goto fail_pedal;
	}

	// Stacked same key: peel under pedal, then pedal-up releases remaining.
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_synth_note_off(synth, 0, 60); // first identity held by pedal
	if (count_playing_n(4) != 2) {
		printf("FAILED pedal: stack note-off dropped a voice\n");
		goto fail_pedal;
	}
	sf64_synth_note_off(synth, 0, 60); // second identity also held
	if (count_playing_n(4) != 2) {
		printf("FAILED pedal: second stack note-off dropped a voice\n");
		goto fail_pedal;
	}
	sf64_synth_set_sustain(synth, 0, 0);
	sf64_synth_process(synth, rel);
	mix(64);
	if (count_playing_n(4) != 0) {
		printf("FAILED pedal: stacked identities not released\n");
		goto fail_pedal;
	}

	// Sustain loop: pedal note-off must NOT leave the loop; pedal-up does.
	assert(sf64_synth_set_program(synth, 0, 0, 2));
	sf64_region_t *rb = &bank->regions[bank->presets[2].first_region];
	int16_t save_rel_b = rb->amp_env.release_timecents;
	uint8_t save_mode = rb->loop_mode;
	rb->amp_env.release_timecents = samples_to_timecents(600);
	int rel_b = env_samples(rb->amp_env.release_timecents);
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_note_on(synth, 0, 60, 100);
	mixer_ch_set_freq(0, 0);
	mixer_ch_set_pos(0, B_INTRO + 8);
	mix(2048);
	mixer_ch_set_freq(0, SAMPLE_RATE);
	sf64_synth_note_off(synth, 0, 60);
	mix(B_LOOP + 256);
	double pos = mixer_ch_get_pos(0);
	if (pos < B_INTRO || pos >= B_INTRO + B_LOOP) {
		printf("FAILED pedal: sustain loop left early (pos %.1f)\n", pos);
		rb->amp_env.release_timecents = save_rel_b;
		rb->loop_mode = save_mode;
		goto fail_pedal;
	}
	sf64_synth_set_sustain(synth, 0, 0);
	mix(B_LOOP);
	pos = mixer_ch_get_pos(0);
	if (pos < B_INTRO + B_LOOP) {
		printf("FAILED pedal: sustain loop still on after pedal-up (pos %.1f)\n",
			pos);
		rb->amp_env.release_timecents = save_rel_b;
		rb->loop_mode = save_mode;
		goto fail_pedal;
	}
	sf64_synth_process(synth, rel_b);
	mix(64);

	// Continuous loop: stays in loop through real release after pedal-up.
	rb->loop_mode = SF64_LOOP_CONTINUOUS;
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_note_on(synth, 0, 60, 100);
	mixer_ch_set_freq(0, 0);
	mixer_ch_set_pos(0, B_INTRO + 8);
	mix(2048);
	mixer_ch_set_freq(0, SAMPLE_RATE);
	sf64_synth_note_off(synth, 0, 60);
	sf64_synth_set_sustain(synth, 0, 0);
	mix(B_LOOP + 256);
	pos = mixer_ch_get_pos(0);
	if (pos < B_INTRO || pos >= B_INTRO + B_LOOP) {
		printf("FAILED pedal: continuous loop left during release (pos %.1f)\n",
			pos);
		rb->amp_env.release_timecents = save_rel_b;
		rb->loop_mode = save_mode;
		goto fail_pedal;
	}
	sf64_synth_process(synth, rel_b);
	mix(64);

	rb->amp_env.release_timecents = save_rel_b;
	rb->loop_mode = save_mode;
	r->amp_env.release_timecents = save_rel;
	sf64_synth_close(synth);
	return true;

fail_pedal:
	r->amp_env.release_timecents = save_rel;
	sf64_synth_close(synth);
	return false;
}

static bool test_synth_exclusive(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 6, MIXER_PRIORITY_MUSIC);
	sf64_region_t *r0 = &bank->regions[bank->presets[0].first_region];
	sf64_region_t *r1 = &bank->regions[bank->presets[1].first_region];
	uint8_t g0a = r0[0].exclusive_group, g0c = r0[1].exclusive_group;
	uint8_t g1a = r1[0].exclusive_group, g1d = r1[1].exclusive_group;

	// Same group + same preset → choke.
	r0[0].exclusive_group = 1;
	r0[1].exclusive_group = 1;
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 6);
	sf64_synth_note_on(synth, 0, 48, 100); // region A
	sf64_synth_note_on(synth, 0, 72, 100); // region C chokes A
	mix(64);
	waveform_t *wave_c = &bank->waves[r0[1].sample_index]->wave;
	if (count_playing_n(6) != 1) {
		printf("FAILED excl: bitmap after choke has %d voices\n",
			count_playing_n(6));
		goto fail_excl;
	}
	if (!wave_playing(6, wave_c)) {
		printf("FAILED excl: same group did not leave region C\n");
		goto fail_excl;
	}

	// Different groups → no choke.
	r0[0].exclusive_group = 1;
	r0[1].exclusive_group = 2;
	synth_silence(synth, 6);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 72, 100);
	if (!mixer_ch_playing(0) || !mixer_ch_playing(1)) {
		printf("FAILED excl: different groups choked\n");
		goto fail_excl;
	}

	// Same group, different presets → no choke.
	r0[0].exclusive_group = 1;
	r1[1].exclusive_group = 1; // VelSplit high-vel → D
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	assert(sf64_synth_set_program(synth, 1, 0, 1));
	synth_silence(synth, 6);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 1, 60, 100);
	if (!mixer_ch_playing(0) || !mixer_ch_playing(1)) {
		printf("FAILED excl: different presets choked\n");
		goto fail_excl;
	}

	// Multiple victims in the same group.
	r0[0].exclusive_group = 1;
	r0[1].exclusive_group = 1;
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 6);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 0, 60, 100); // also A (same region), both group 1
	// Second note-on of key 60 also matches region A only — both on group 1.
	// A third note at 72 chokes both.
	sf64_synth_note_on(synth, 0, 72, 100);
	mix(64);
	if (count_playing_n(6) != 1) {
		printf("FAILED excl: multi-victim left %d voices\n", count_playing_n(6));
		goto fail_excl;
	}
	if (!wave_playing(6, wave_c)) {
		printf("FAILED excl: multi-victim survivor is not region C\n");
		goto fail_excl;
	}

	// Same note-on layers sharing a group must not choke each other.
	sf64_region_t *rl = &bank->regions[bank->presets[3].first_region];
	uint8_t gl0 = rl[0].exclusive_group, gl1 = rl[1].exclusive_group;
	rl[0].exclusive_group = 5;
	rl[1].exclusive_group = 5;
	assert(sf64_synth_set_program(synth, 0, 0, 3));
	synth_silence(synth, 6);
	uint32_t id = sf64_synth_note_on(synth, 0, 60, 100);
	if (!id || count_playing_n(6) != 2) {
		printf("FAILED excl: layered same-group note-on playing=%d id=%lu\n",
			count_playing_n(6), (unsigned long)id);
		rl[0].exclusive_group = gl0;
		rl[1].exclusive_group = gl1;
		goto fail_excl;
	}
	rl[0].exclusive_group = gl0;
	rl[1].exclusive_group = gl1;

	// Rejected layered note-on must not choke victims (atomic with exclusive).
	// Pool of 1 with a group-1 mono voice: Layer2 needs 2 channels so the plan
	// fails, and the exclusive victim must still be sounding.
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	r0[0].exclusive_group = 1;
	r0[1].exclusive_group = 0;
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 48, 100);
	mix(64);
	waveform_t *w0 = mixer_ch_playing_waveform(0);
	assert(sf64_synth_set_program(synth, 0, 0, 3));
	rl[0].exclusive_group = 1;
	rl[1].exclusive_group = 0;
	if (sf64_synth_note_on(synth, 0, 61, 100) != 0) {
		printf("FAILED excl: layered note should have been rejected\n");
		rl[0].exclusive_group = gl0;
		rl[1].exclusive_group = gl1;
		goto fail_excl;
	}
	if (!mixer_ch_playing(0) || mixer_ch_playing_waveform(0) != w0) {
		printf("FAILED excl: rejected note-on choked a victim\n");
		rl[0].exclusive_group = gl0;
		rl[1].exclusive_group = gl1;
		goto fail_excl;
	}
	rl[0].exclusive_group = gl0;
	rl[1].exclusive_group = gl1;

	r0[0].exclusive_group = g0a;
	r0[1].exclusive_group = g0c;
	r1[0].exclusive_group = g1a;
	r1[1].exclusive_group = g1d;
	sf64_synth_close(synth);
	return true;

fail_excl:
	r0[0].exclusive_group = g0a;
	r0[1].exclusive_group = g0c;
	r1[0].exclusive_group = g1a;
	r1[1].exclusive_group = g1d;
	sf64_synth_close(synth);
	return false;
}

//////////////////////////////////////////////////////////////////////////////
// Envelope delay / hold / key scaling, multi-phase process, reclaim, MIDI
//////////////////////////////////////////////////////////////////////////////

static bool test_synth_delay_hold(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	sf64_envelope_t save = r->amp_env;

	// Delay: silent until deadline, then attack remaining.
	r->amp_env.delay_timecents = samples_to_timecents(256);
	r->amp_env.attack_timecents = samples_to_timecents(128);
	r->amp_env.hold_timecents = -12000;
	r->amp_env.decay_timecents = -12000;
	r->amp_env.sustain_gain = 1.0f;
	r->amp_env.keynum_to_hold = 0;
	r->amp_env.keynum_to_decay = 0;
	int delay = env_samples(r->amp_env.delay_timecents);
	int atk = env_samples(r->amp_env.attack_timecents);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	int dl = sf64_synth_process(synth, 0);
	if (dl != delay) {
		printf("FAILED delay: remaining %d want %d\n", dl, delay);
		goto fail_dh;
	}
	mix(64);
	if (peak_n(64) > 64) {
		printf("FAILED delay: audible during delay (peak %d)\n", peak_n(64));
		goto fail_dh;
	}
	dl = sf64_synth_process(synth, delay);
	if (dl != atk) {
		printf("FAILED delay→attack remaining %d want %d\n", dl, atk);
		goto fail_dh;
	}

	// Hold: after attack, peak holds before decay to silence.
	r->amp_env.delay_timecents = -12000;
	r->amp_env.attack_timecents = samples_to_timecents(64);
	r->amp_env.hold_timecents = samples_to_timecents(256);
	r->amp_env.decay_timecents = samples_to_timecents(64);
	r->amp_env.sustain_gain = 0.0f;
	atk = env_samples(r->amp_env.attack_timecents);
	int hold = env_samples(r->amp_env.hold_timecents);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 127);
	sf64_synth_process(synth, atk); // → hold
	dl = sf64_synth_process(synth, 0);
	if (dl != hold) {
		printf("FAILED hold: remaining %d want %d\n", dl, hold);
		goto fail_dh;
	}
	mix(64);
	int held = peak_n(64);
	if (held < 1024) {
		printf("FAILED hold: silent at peak (peak %d)\n", held);
		goto fail_dh;
	}
	sf64_synth_process(synth, hold); // → decay
	sf64_synth_process(synth, env_samples(r->amp_env.decay_timecents));
	mix(64);
	if (peak_n(64) > held / 8) {
		printf("FAILED hold: still loud after decay to silence\n");
		goto fail_dh;
	}

	// Keynum scaling shortens hold at keys above 60.
	r->amp_env.attack_timecents = -12000;
	r->amp_env.hold_timecents = samples_to_timecents(400);
	r->amp_env.keynum_to_hold = 100;
	r->amp_env.decay_timecents = -12000;
	r->amp_env.sustain_gain = 1.0f;
	int hold60 = env_scaled_samples_tc(r->amp_env.hold_timecents,
		r->amp_env.keynum_to_hold, 60);
	int hold72 = env_scaled_samples_tc(r->amp_env.hold_timecents,
		r->amp_env.keynum_to_hold, 72);
	if (hold72 >= hold60) {
		printf("FAILED keyscale: hold72=%d not shorter than hold60=%d\n",
			hold72, hold60);
		goto fail_dh;
	}
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	dl = sf64_synth_process(synth, 0);
	if (dl != hold60) {
		printf("FAILED keyscale: key60 hold %d want %d\n", dl, hold60);
		goto fail_dh;
	}
	// Lower key → longer hold (scale*(60-key) adds timecents).
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 48, 100);
	int hold48 = env_scaled_samples_tc(r->amp_env.hold_timecents,
		r->amp_env.keynum_to_hold, 48);
	dl = sf64_synth_process(synth, 0);
	if (dl != hold48 || hold48 <= hold60) {
		printf("FAILED keyscale: key48 hold %d want %d (key60=%d)\n",
			dl, hold48, hold60);
		goto fail_dh;
	}

	r->amp_env = save;
	sf64_synth_close(synth);
	return true;

fail_dh:
	r->amp_env = save;
	sf64_synth_close(synth);
	return false;
}

static bool test_synth_multiphase(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	sf64_envelope_t save = r->amp_env;

	r->amp_env.delay_timecents = samples_to_timecents(100);
	r->amp_env.attack_timecents = samples_to_timecents(100);
	r->amp_env.hold_timecents = samples_to_timecents(100);
	r->amp_env.decay_timecents = -12000;
	r->amp_env.sustain_gain = 1.0f;
	r->amp_env.keynum_to_hold = 0;
	r->amp_env.keynum_to_decay = 0;
	int delay = env_samples(r->amp_env.delay_timecents);
	int atk = env_samples(r->amp_env.attack_timecents);
	int hold = env_samples(r->amp_env.hold_timecents);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 100);
	// One process spanning delay+attack+hold must land in sustain.
	sf64_synth_process(synth, delay + atk + hold);
	if (sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED multiphase: still pending after full envelope span\n");
		r->amp_env = save;
		sf64_synth_close(synth);
		return false;
	}
	mix(64);
	if (peak_n(64) < 1024) {
		printf("FAILED multiphase: silent in sustain after catch-up\n");
		r->amp_env = save;
		sf64_synth_close(synth);
		return false;
	}

	r->amp_env = save;
	sf64_synth_close(synth);
	return true;
}

static bool test_synth_oneshot_reclaim(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	sf64_envelope_t save = r->amp_env;
	r->amp_env.delay_timecents = -12000;
	r->amp_env.attack_timecents = -12000;
	r->amp_env.hold_timecents = -12000;
	r->amp_env.decay_timecents = -12000;
	r->amp_env.release_timecents = -12000;

	synth_silence(synth, 1);
	if (!sf64_synth_note_on(synth, 0, 60, 100)) {
		printf("FAILED oneshot reclaim: first note_on failed\n");
		goto fail_os;
	}
	// Play past the oneshot length at 1:1 (root key 60).
	mix(A_LEN + 512);
	if (mixer_ch_playing(0)) {
		printf("FAILED oneshot reclaim: mixer still playing after sample end\n");
		goto fail_os;
	}
	// Mixer channel is free but the voice is not yet reclaimed: note_on steals
	// it (same path as an occupied slot with phase != OFF). Same key so the
	// oneshot still finishes in A_LEN samples at 1:1.
	if (!sf64_synth_note_on(synth, 0, 60, 100) || !mixer_ch_playing(0)) {
		printf("FAILED oneshot reclaim: note_on did not reuse finished voice\n");
		goto fail_os;
	}
	// process() also reclaims finished voices when no note_on intervenes.
	mix(A_LEN + 512);
	if (mixer_ch_playing(0)) {
		printf("FAILED oneshot reclaim: second oneshot still playing\n");
		goto fail_os;
	}
	sf64_synth_process(synth, 0);
	if (sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED oneshot reclaim: process left a pending deadline\n");
		goto fail_os;
	}

	r->amp_env = save;
	sf64_synth_close(synth);
	return true;

fail_os:
	r->amp_env = save;
	sf64_synth_close(synth);
	return false;
}

static bool test_synth_drum_channel(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	sf64_region_t *rd = &bank->regions[bank->presets[5].first_region];
	sf64_region_t *ra = &bank->regions[bank->presets[0].first_region];

	// Channel 9 defaults to bank 128 / DrumKit without Bank Select.
	synth_silence(synth, 2);
	if (!sf64_synth_note_on(synth, 9, 60, 100)) {
		printf("FAILED drum: ch9 note_on with default bank\n");
		sf64_synth_close(synth);
		return false;
	}
	if (mixer_ch_playing_waveform(0) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED drum: ch9 did not play DrumKit sample\n");
		sf64_synth_close(synth);
		return false;
	}

	// Melodic ch0 still defaults to bank 0 / KeySplit.
	if (!sf64_synth_note_on(synth, 0, 48, 100)) {
		printf("FAILED drum: ch0 note_on failed\n");
		sf64_synth_close(synth);
		return false;
	}
	if (mixer_ch_playing_waveform(1) != &bank->waves[ra->sample_index]->wave) {
		printf("FAILED drum: ch0 did not play KeySplit\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

static bool test_synth_midi_reset(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	midi_target_t *mt = sf64_synth_midi_target(synth);
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	int16_t save_rel = r->amp_env.release_timecents;
	r->amp_env.release_timecents = samples_to_timecents(200);
	int rel = env_samples(r->amp_env.release_timecents);

	assert(sf64_synth_set_program(synth, 0, 0, 3)); // Layer2
	sf64_synth_set_volume(synth, 0, 16);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_set_pan(synth, 0, 0);
	synth_silence(synth, 2);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (count_playing_n(2) != 2) {
		printf("FAILED midi reset: Layer2 did not start two voices\n");
		goto fail_mr;
	}

	mt->ops->reset(mt, 0);
	mix(64);
	if (count_playing_n(2) != 0) {
		printf("FAILED midi reset: voices still playing\n");
		goto fail_mr;
	}

	// Controllers / program restored: KeySplit, full volume, no sustain.
	synth_silence(synth, 2);
	if (!sf64_synth_note_on(synth, 0, 48, 127) || count_playing_n(2) != 1) {
		printf("FAILED midi reset: expected single KeySplit voice\n");
		goto fail_mr;
	}
	mix(128);
	int full = peak_n(128);
	if (full < 1024) {
		printf("FAILED midi reset: volume not restored (peak %d)\n", full);
		goto fail_mr;
	}
	sf64_synth_note_off(synth, 0, 48);
	int dl = sf64_synth_process(synth, 0);
	if (dl != rel) {
		printf("FAILED midi reset: sustain still held (remaining %d)\n", dl);
		goto fail_mr;
	}

	// Drum channel bank restored to 128.
	synth_silence(synth, 2);
	sf64_synth_set_program(synth, 9, 0, 0); // force melodic
	mt->ops->reset(mt, 0);
	sf64_region_t *rd = &bank->regions[bank->presets[5].first_region];
	if (!sf64_synth_note_on(synth, 9, 60, 100) ||
		mixer_ch_playing_waveform(0) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED midi reset: ch9 bank not restored to 128\n");
		goto fail_mr;
	}

	r->amp_env.release_timecents = save_rel;
	sf64_synth_close(synth);
	return true;

fail_mr:
	r->amp_env.release_timecents = save_rel;
	sf64_synth_close(synth);
	return false;
}

/** CC120 / CC121 / CC123 Channel Mode Messages. */
static bool test_synth_channel_mode(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 4, MIXER_PRIORITY_MUSIC);
	midi_target_t *mt = sf64_synth_midi_target(synth);
	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	sf64_region_t *rc = &bank->regions[bank->presets[0].first_region + 1];
	sf64_sample_t *sc = &bank->samples[rc->sample_index];
	sf64_envelope_t save0 = r->amp_env;
	sf64_envelope_t save_c = rc->amp_env;

	r->amp_env.attack_timecents = samples_to_timecents(800);
	r->amp_env.release_timecents = samples_to_timecents(800);
	rc->amp_env.release_timecents = samples_to_timecents(800);
	int rel = env_samples(r->amp_env.release_timecents);

	assert(sf64_synth_set_program(synth, 0, 0, 0));
	assert(sf64_synth_set_program(synth, 1, 0, 0));

	// CC120 during attack: hard cut, no release tail.
	synth_silence(synth, 4);
	sf64_synth_note_on(synth, 0, 48, 100);
	assert(mixer_ch_playing(0));
	mt->ops->control_change(mt, 0, 120, 0, 0);
	mix(64);
	if (mixer_ch_playing(0)) {
		printf("FAILED CC120 attack: voice still playing\n");
		goto fail_cm;
	}

	// CC120 during release: also hard cut.
	synth_silence(synth, 4);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_off(synth, 0, 48);
	assert(sf64_synth_process(synth, 0) == rel);
	assert(mixer_ch_playing(0));
	mt->ops->control_change(mt, 0, 120, 0, 0);
	mix(64);
	if (mixer_ch_playing(0)) {
		printf("FAILED CC120 release: voice still playing\n");
		goto fail_cm;
	}

	// CC123 sustain OFF → release (still playing mid-release).
	synth_silence(synth, 4);
	sf64_synth_note_on(synth, 0, 48, 100);
	mt->ops->control_change(mt, 0, 123, 0, 0);
	int dl = sf64_synth_process(synth, 0);
	if (dl != rel || !mixer_ch_playing(0)) {
		printf("FAILED CC123: expected release remaining %d playing=%d\n",
			dl, mixer_ch_playing(0));
		goto fail_cm;
	}

	// CC123 sustain ON → held by pedal through attack into sustain.
	synth_silence(synth, 4);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_note_on(synth, 0, 48, 100);
	mt->ops->control_change(mt, 0, 123, 0, 0);
	int atk = env_samples(r->amp_env.attack_timecents);
	sf64_synth_process(synth, atk);
	mix(64);
	if (!mixer_ch_playing(0) || sf64_synth_process(synth, 0) >= 0) {
		printf("FAILED CC123 pedal: expected held at sustain (playing=%d dl=%d)\n",
			mixer_ch_playing(0), sf64_synth_process(synth, 0));
		goto fail_cm;
	}
	sf64_synth_set_sustain(synth, 0, 0);
	if (sf64_synth_process(synth, 0) != rel) {
		printf("FAILED CC123 pedal: pedal-up did not release\n");
		goto fail_cm;
	}

	// CC121: pitch bend restored; volume preserved; pedal released.
	synth_silence(synth, 4);
	sf64_synth_set_volume(synth, 0, 32);
	sf64_synth_set_pan(synth, 0, 0);
	sf64_synth_set_expression(synth, 0, 32);
	sf64_synth_set_sustain(synth, 0, 127);
	sf64_synth_set_pitch_bend(synth, 0, 0);
	sf64_synth_note_on(synth, 0, 72, 100);
	sf64_synth_note_off(synth, 0, 72); // held by pedal
	assert(mixer_ch_playing(0));
	if (!check_freq(synth, "CC121 pre-bend",
			expect_freq_bend(rc, sc, 72, 0, 200)))
		goto fail_cm;
	mt->ops->control_change(mt, 0, 121, 0, 0);
	if (!check_freq(synth, "CC121 bend center",
			expect_freq_bend(rc, sc, 72, 8192, 200)))
		goto fail_cm;
	// Pedal cleared → release; volume still 32 (quiet vs full).
	dl = sf64_synth_process(synth, 0);
	if (dl != rel) {
		printf("FAILED CC121: pedal not cleared (dl=%d)\n", dl);
		goto fail_cm;
	}
	sf64_synth_process(synth, rel);
	mix(64);
	synth_silence(synth, 4);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(256);
	int quiet = peak_n(256);
	sf64_synth_set_volume(synth, 0, 127);
	mix(256);
	int loud = peak_n(256);
	if (quiet * 2 > loud) {
		printf("FAILED CC121: volume was reset (quiet=%d loud=%d)\n",
			quiet, loud);
		goto fail_cm;
	}

	// CC121 on ch0 does not affect ch1 bend.
	synth_silence(synth, 4);
	sf64_synth_set_pitch_bend(synth, 0, 0);
	sf64_synth_set_pitch_bend(synth, 1, 0);
	sf64_synth_note_on(synth, 0, 72, 100);
	sf64_synth_note_on(synth, 1, 72, 100);
	mt->ops->control_change(mt, 0, 121, 0, 0);
	double p0 = mixer_ch_get_pos(1);
	mix(512);
	double got = (mixer_ch_get_pos(1) - p0) * SAMPLE_RATE / 512.0;
	float want = expect_freq_bend(rc, sc, 72, 0, 200);
	float err = got - want;
	if (err < 0) err = -err;
	if (err > want * 0.02f + 1.0f) {
		printf("FAILED CC121 isolation: ch1 freq %.1f want %.1f\n", got, want);
		goto fail_cm;
	}

	// Layered note identity: CC120 / CC123 affect both layers.
	synth_silence(synth, 4);
	assert(sf64_synth_set_program(synth, 0, 0, 3)); // Layer2
	sf64_region_t *rl = &bank->regions[bank->presets[3].first_region];
	sf64_envelope_t save_l0 = rl[0].amp_env;
	sf64_envelope_t save_l1 = rl[1].amp_env;
	rl[0].amp_env.release_timecents = samples_to_timecents(800);
	rl[1].amp_env.release_timecents = samples_to_timecents(800);
	sf64_synth_note_on(synth, 0, 60, 100);
	if (count_playing_n(4) != 2) {
		printf("FAILED channel mode layer: setup playing=%d\n",
			count_playing_n(4));
		rl[0].amp_env = save_l0;
		rl[1].amp_env = save_l1;
		goto fail_cm;
	}
	mt->ops->control_change(mt, 0, 123, 0, 0);
	if (sf64_synth_process(synth, 0) != rel || count_playing_n(4) != 2) {
		printf("FAILED CC123 layer: playing=%d\n", count_playing_n(4));
		rl[0].amp_env = save_l0;
		rl[1].amp_env = save_l1;
		goto fail_cm;
	}
	mt->ops->control_change(mt, 0, 120, 0, 0);
	mix(64);
	if (count_playing_n(4) != 0) {
		printf("FAILED CC120 layer: playing=%d\n", count_playing_n(4));
		rl[0].amp_env = save_l0;
		rl[1].amp_env = save_l1;
		goto fail_cm;
	}
	rl[0].amp_env = save_l0;
	rl[1].amp_env = save_l1;

	r->amp_env = save0;
	rc->amp_env = save_c;
	sf64_synth_close(synth);
	return true;

fail_cm:
	r->amp_env = save0;
	rc->amp_env = save_c;
	sf64_synth_close(synth);
	return false;
}

/** GM1 mode: ch10 ignores Program Change / Bank Select; melodic PC uses bank 0. */
static bool test_synth_gm_mode(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 2, MIXER_PRIORITY_MUSIC);
	midi_target_t *mt = sf64_synth_midi_target(synth);
	sf64_region_t *rd = &bank->regions[bank->presets[5].first_region];
	sf64_region_t *ra = &bank->regions[bank->presets[0].first_region];

	if (sf64_synth_get_mode(synth) != SF64_MODE_NATIVE) {
		printf("FAILED gm mode: create is not NATIVE\n");
		sf64_synth_close(synth);
		return false;
	}

	// Defaults after create/reset: ch0 program 0, ch9 percussion.
	synth_silence(synth, 2);
	sf64_synth_note_on(synth, 0, 48, 100);
	sf64_synth_note_on(synth, 9, 60, 100);
	if (mixer_ch_playing_waveform(0) != &bank->waves[ra->sample_index]->wave ||
		mixer_ch_playing_waveform(1) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED gm mode: default programs wrong\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_set_mode(synth, SF64_MODE_GM1);
	if (sf64_synth_get_mode(synth) != SF64_MODE_GM1) {
		printf("FAILED gm mode: set_mode\n");
		sf64_synth_close(synth);
		return false;
	}

	// Program Change on ch10 ignored; bank select must not make it melodic.
	synth_silence(synth, 2);
	mt->ops->control_change(mt, 9, 0, 0, 0);
	mt->ops->program_change(mt, 9, 0, 0);
	sf64_synth_note_on(synth, 9, 60, 100);
	if (mixer_ch_playing_waveform(0) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED gm mode: ch10 became melodic\n");
		sf64_synth_close(synth);
		return false;
	}

	// Melodic Program Change still works (bank 0 regardless of CC0).
	synth_silence(synth, 2);
	mt->ops->control_change(mt, 0, 0, 5, 0);
	mt->ops->program_change(mt, 0, 1, 0); // VelSplit
	sf64_synth_note_on(synth, 0, 60, 100);
	sf64_region_t *rvel = &bank->regions[bank->presets[1].first_region + 1];
	if (mixer_ch_playing_waveform(0) != &bank->waves[rvel->sample_index]->wave) {
		printf("FAILED gm mode: melodic PC ignored bank incorrectly\n");
		sf64_synth_close(synth);
		return false;
	}

	// system_reset → GM1 + hard silence + default programs.
	synth_silence(synth, 2);
	sf64_synth_set_mode(synth, SF64_MODE_NATIVE);
	assert(sf64_synth_set_program(synth, 9, 0, 0)); // force melodic
	sf64_synth_note_on(synth, 9, 48, 100);
	assert(mixer_ch_playing(0));
	mt->ops->system_reset(mt, MIDI_SYSTEM_GM1, 0);
	mix(64);
	if (sf64_synth_get_mode(synth) != SF64_MODE_GM1 || mixer_ch_playing(0)) {
		printf("FAILED gm mode: system_reset mode/silence\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_note_on(synth, 9, 60, 100);
	if (mixer_ch_playing_waveform(0) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED gm mode: system_reset did not restore drums\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

/** RPN 0,0 pitch bend sensitivity; CC121 restores default range. */
static bool test_synth_rpn(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	midi_target_t *mt = sf64_synth_midi_target(synth);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *rc = &bank->regions[bank->presets[0].first_region + 1];
	sf64_sample_t *sc = &bank->samples[rc->sample_index];

	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 100);
	mt->ops->control_change(mt, 0, 101, 0, 0); // RPN MSB
	mt->ops->control_change(mt, 0, 100, 0, 0); // RPN LSB
	mt->ops->control_change(mt, 0, 6, 12, 0);  // 12 semitones
	sf64_synth_set_pitch_bend(synth, 0, 16383);
	if (!check_freq(synth, "rpn +12",
			expect_freq_bend(rc, sc, 72, 16383, 1200))) {
		sf64_synth_close(synth);
		return false;
	}

	mt->ops->control_change(mt, 0, 121, 0, 0);
	if (!check_freq(synth, "rpn CC121 range",
			expect_freq_bend(rc, sc, 72, 8192, 200))) {
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_set_pitch_bend(synth, 0, 16383);
	if (!check_freq(synth, "rpn after CC121",
			expect_freq_bend(rc, sc, 72, 16383, 200))) {
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

static bool test_synth_attenuation(sf64_bank_t *bank)
{
	// Converter bakes 200 cB → linear gain 0.1 (not ~20 from the old 10× TSF factor).
	sf64_region_t *rd = &bank->regions[bank->presets[1].first_region + 1];
	if (rd->gain < 0.09f || rd->gain > 0.11f) {
		printf("FAILED attenuation: region D gain=%f want ~0.1\n",
			rd->gain);
		return false;
	}

	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 1));
	float save = rd->gain;

	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 127);
	mix(256);
	int quiet = peak_n(256);
	rd->gain = 1.0f;
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 60, 127);
	mix(256);
	int loud = peak_n(256);
	rd->gain = save;

	// 200 cB = 20 dB → amplitude ≈ 10× (band for mixer / hermite).
	if (loud < 1024 || quiet * 5 > loud || quiet * 20 < loud) {
		printf("FAILED attenuation: quiet %d loud %d (want ~10×)\n",
			quiet, loud);
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

/** Step 5: SF2 velocity/volume/expression/sustain gain model (no linear /127). */
static bool test_synth_gain_model(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channels(synth, 0, 1, MIXER_PRIORITY_MUSIC);
	assert(sf64_synth_set_program(synth, 0, 0, 0));
	sf64_region_t *rc = &bank->regions[bank->presets[0].first_region + 1];
	sf64_envelope_t save = rc->amp_env;
	float save_gain = rc->gain;

	// Constant-amplitude sample C (key 72), zero attack, full sustain.
	rc->amp_env.delay_timecents = -12000;
	rc->amp_env.attack_timecents = -12000;
	rc->amp_env.hold_timecents = -12000;
	rc->amp_env.decay_timecents = -12000;
	rc->amp_env.sustain_gain = 1.0f;
	rc->gain = 1.0f;
	sf64_synth_set_volume(synth, 0, 127);
	sf64_synth_set_expression(synth, 0, 127);

	// Velocity follows (x/127)²: 96 is 1.75× down, 64 a full 4× (a linear
	// velocity/127 would only halve it).
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(256);
	int v127 = peak_n(256);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 96);
	mix(256);
	int v96 = peak_n(256);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 64);
	mix(256);
	int v64 = peak_n(256);
	if (v127 < 1024 || v127 * 2 < v96 * 3 || v127 * 10 > v96 * 21) {
		printf("FAILED gain vel: 127=%d 96=%d (want ~1.75×)\n", v127, v96);
		goto fail_gain;
	}
	if (v64 * 3 > v127 || v64 * 5 < v127) {
		printf("FAILED gain vel: 127=%d 64=%d (want ~4×)\n", v127, v64);
		goto fail_gain;
	}

	// Region attenuation halves amplitude when gain = 0.5.
	rc->gain = 0.5f;
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(256);
	int half_attn = peak_n(256);
	rc->gain = 1.0f;
	if (half_attn * 2 < v127 * 3 / 4 || half_attn * 2 > v127 * 5 / 4) {
		printf("FAILED gain attn: half %d full %d\n", half_attn, v127);
		goto fail_gain;
	}

	// Sustain at half peak: after instant decay, peak ≈ half.
	rc->amp_env.sustain_gain = 0.5f;
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(256);
	int sus = peak_n(256);
	rc->amp_env.sustain_gain = 1.0f;
	if (sus * 2 < v127 * 3 / 4 || sus * 2 > v127 * 5 / 4) {
		printf("FAILED gain sustain: sus %d full %d\n", sus, v127);
		goto fail_gain;
	}

	// Expression shares the SF2 curve with volume: 64 → a quarter.
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 127);
	mix(128);
	sf64_synth_set_expression(synth, 0, 64);
	mix(256);
	int expr = peak_n(256);
	sf64_synth_set_expression(synth, 0, 127);
	if (expr * 3 > v127 || expr * 5 < v127) {
		printf("FAILED gain expr: full %d expr64 %d (want ~4×)\n", v127, expr);
		goto fail_gain;
	}

	// Combined: velocity 64 × volume 64 → a quarter of a quarter.
	sf64_synth_set_volume(synth, 0, 64);
	synth_silence(synth, 1);
	sf64_synth_note_on(synth, 0, 72, 64);
	mix(256);
	int combo = peak_n(256);
	sf64_synth_set_volume(synth, 0, 127);
	if (combo * 12 > v127 || combo * 22 < v127) {
		printf("FAILED gain combo: full %d vel64×vol64 %d (want ~16×)\n",
			v127, combo);
		goto fail_gain;
	}

	rc->amp_env = save;
	rc->gain = save_gain;
	sf64_synth_close(synth);
	return true;

fail_gain:
	rc->amp_env = save;
	rc->gain = save_gain;
	sf64_synth_close(synth);
	return false;
}

int main(void)
{
	debug_init_emulog();
	debug_init_usblog();
	emux_ioctl_fast();
	console_init();

	printf("SF64 bank / synth tests\n\n");

	assert(dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS);
	audio_init(SAMPLE_RATE, 4);
	mixer_init(NCH);
	for (int i = 0; i < NCH; i++)
		mixer_ch_set_limits(i, 16, SAMPLE_RATE * 4, 0);

	out_cap = 8192 * 2;
	out = malloc_uncached(out_cap * sizeof(int16_t));

	sf64_bank_t *bank = sf64_load("rom:/sf64test.sf64");
	assert(bank);

	int total = 0, failed = 0;

	total++; if (!test_bank_layout(bank)) failed++;
	total++; if (!test_play_oneshot(bank, 0, pcm_a, "sample A")) failed++;
	total++; if (!test_play_oneshot(bank, 1, pcm_c, "sample C")) failed++;
	total++; if (!test_play_oneshot(bank, 2, pcm_d, "sample D")) failed++;
	total++; if (!test_play_loop(bank)) failed++;
	total++; if (!test_slow_ramp(bank)) failed++;

	printf("synth\n");
	fflush(stdout);
	// Oneshoot helpers pin the channel to SAMPLE_RATE; pitch tests need headroom.
	for (int i = 0; i < NCH; i++)
		mixer_ch_set_limits(i, 16, SAMPLE_RATE * 4, 0);
	total++; if (!test_synth_pitch(bank)) failed++;
	total++; if (!test_synth_key_split(bank)) failed++;
	total++; if (!test_synth_vel_split(bank)) failed++;
	total++; if (!test_synth_allocator(bank)) failed++;
	total++; if (!test_synth_polyphony(bank)) failed++;
	total++; if (!test_synth_saturation(bank)) failed++;
	total++; if (!test_synth_voice_steal(bank)) failed++;
	total++; if (!test_synth_deadlines(bank)) failed++;
	total++; if (!test_synth_preset(bank)) failed++;
	total++; if (!test_synth_layers(bank)) failed++;
	total++; if (!test_synth_layer_atomic(bank)) failed++;
	total++; if (!test_synth_note_identity(bank)) failed++;

	printf("envelopes\n");
	fflush(stdout);
	total++; if (!test_synth_attack(bank)) failed++;
	total++; if (!test_synth_release(bank)) failed++;
	total++; if (!test_synth_loop_env(bank)) failed++;

	printf("midi channels\n");
	fflush(stdout);
	total++; if (!test_synth_midi_channels(bank)) failed++;
	total++; if (!test_synth_midi_controllers(bank)) failed++;

	printf("sustain / exclusive\n");
	fflush(stdout);
	total++; if (!test_synth_sustain_pedal(bank)) failed++;
	total++; if (!test_synth_exclusive(bank)) failed++;

	printf("tests for fixed bugs\n");
	fflush(stdout);
	total++; if (!test_synth_delay_hold(bank)) failed++;
	total++; if (!test_synth_multiphase(bank)) failed++;
	total++; if (!test_synth_oneshot_reclaim(bank)) failed++;
	total++; if (!test_synth_drum_channel(bank)) failed++;
	total++; if (!test_synth_midi_reset(bank)) failed++;
	total++; if (!test_synth_channel_mode(bank)) failed++;
	total++; if (!test_synth_gm_mode(bank)) failed++;
	total++; if (!test_synth_rpn(bank)) failed++;
	total++; if (!test_synth_attenuation(bank)) failed++;
	total++; if (!test_synth_gain_model(bank)) failed++;

	sf64_close(bank);

	printf("\n%d/%d tests failed\n", failed, total);
	if (failed == 0)
		printf("SUCCESS\n");
	fflush(stdout);
	emux_ioctl_exit();
	return failed ? 1 : 0;
}
