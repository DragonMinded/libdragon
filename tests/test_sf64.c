/**
 * @file test_sf64.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Standalone testrom for the SF64 bank loader and synth
 *
 * Loads the deterministic sf64test bank, checks preset/region/sample layout,
 * plays embedded waveforms through the real mixer, and exercises the
 * monophonic synthesizer (pitch, key/velocity matching, note replace).
 */
#include <libdragon.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/audio/sf64_internal.h"

#define SAMPLE_RATE  22050
#define CH           0
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
	if (bank->num_presets != 3 || bank->num_regions != 5 || bank->num_samples != 4) {
		printf("FAILED layout: presets=%d regions=%d samples=%d\n",
			bank->num_presets, bank->num_regions, bank->num_samples);
		return false;
	}
	if (sf64_preset_count(bank) != 3) {
		printf("FAILED preset_count\n");
		return false;
	}
	if (sf64_find_preset(bank, 0, 0) != 0 ||
		sf64_find_preset(bank, 0, 1) != 1 ||
		sf64_find_preset(bank, 0, 2) != 2 ||
		sf64_find_preset(bank, 0, 3) != -1) {
		printf("FAILED find_preset\n");
		return false;
	}
	int mb, prog;
	sf64_preset_id(bank, 1, &mb, &prog);
	if (mb != 0 || prog != 1 || strcmp(sf64_preset_name(bank, 0), "KeySplit") != 0 ||
		strcmp(sf64_preset_name(bank, 1), "VelSplit") != 0) {
		printf("FAILED preset introspect: bank=%d prog=%d name0=%s name1=%s\n",
			mb, prog, sf64_preset_name(bank, 0), sf64_preset_name(bank, 1));
		return false;
	}

	sf64_preset_t *p0 = &bank->presets[0];
	sf64_preset_t *p1 = &bank->presets[1];
	sf64_preset_t *p2 = &bank->presets[2];
	if (p0->bank != 0 || p0->program != 0 || p0->num_regions != 2 ||
		p1->bank != 0 || p1->program != 1 || p1->num_regions != 2 ||
		p2->bank != 0 || p2->program != 2 || p2->num_regions != 1) {
		printf("FAILED preset headers\n");
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

	// Preset 1: A vel 0-79, D vel 80-127
	r = &bank->regions[p1->first_region];
	if (r[0].velocity_min != 0 || r[0].velocity_max != 79 ||
		r[0].sample_index != 0 ||
		r[1].velocity_min != 80 || r[1].velocity_max != 127 ||
		r[1].sample_index != 2) {
		printf("FAILED preset1 velocity split\n");
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
	sf64_synth_set_channel(synth, CH);
	assert(sf64_synth_set_preset(synth, 0, 0));

	sf64_region_t *ra = &bank->regions[bank->presets[0].first_region];
	sf64_sample_t *sa = &bank->samples[ra->sample_index];

	silence();
	if (!sf64_synth_note_on(synth, 60, 100)) {
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
	silence();
	sf64_synth_note_on(synth, 48, 100);
	if (!check_freq(synth, "pitch -12", expect_freq(ra, sa, 48))) {
		sf64_synth_close(synth);
		return false;
	}
	silence();
	sf64_synth_note_on(synth, 84, 100);
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
	silence();
	sf64_synth_note_on(synth, 60, 100);
	if (!check_freq(synth, "pitch coarse+12", expect_freq(ra, sa, 60))) {
		ra->coarse_tune = save_coarse;
		sf64_synth_close(synth);
		return false;
	}
	ra->coarse_tune = 0;
	ra->fine_tune = 100; // +1 semitone
	silence();
	sf64_synth_note_on(synth, 60, 100);
	if (!check_freq(synth, "pitch fine+100", expect_freq(ra, sa, 60))) {
		ra->fine_tune = save_fine;
		sf64_synth_close(synth);
		return false;
	}
	ra->fine_tune = 0;
	ra->pitch_keytrack = 50; // half tracking
	silence();
	sf64_synth_note_on(synth, 66, 100);
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
	sf64_synth_set_channel(synth, CH);
	assert(sf64_synth_set_preset(synth, 0, 0));

	sf64_region_t *r = &bank->regions[bank->presets[0].first_region];
	silence();
	if (!sf64_synth_note_on(synth, 60, 100) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[0].sample_index]->wave) {
		printf("FAILED key_split: low key\n");
		sf64_synth_close(synth);
		return false;
	}
	silence();
	if (!sf64_synth_note_on(synth, 72, 100) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[1].sample_index]->wave) {
		printf("FAILED key_split: high key\n");
		sf64_synth_close(synth);
		return false;
	}
	// Boundary inclusivity: 71 → A, 72 → C.
	silence();
	if (!sf64_synth_note_on(synth, 71, 100) ||
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
	silence();
	if (sf64_synth_note_on(synth, 60, 100)) {
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
	sf64_synth_set_channel(synth, CH);
	assert(sf64_synth_set_preset(synth, 0, 1));

	sf64_region_t *r = &bank->regions[bank->presets[1].first_region];
	silence();
	if (!sf64_synth_note_on(synth, 60, 79) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[0].sample_index]->wave) {
		printf("FAILED vel_split: vel 79\n");
		sf64_synth_close(synth);
		return false;
	}
	silence();
	if (!sf64_synth_note_on(synth, 60, 80) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[1].sample_index]->wave) {
		printf("FAILED vel_split: vel 80\n");
		sf64_synth_close(synth);
		return false;
	}
	// Upper bound inclusive.
	silence();
	if (!sf64_synth_note_on(synth, 60, 127) ||
		mixer_ch_playing_waveform(CH) != &bank->waves[r[1].sample_index]->wave) {
		printf("FAILED vel_split: vel 127\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_close(synth);
	return true;
}

static bool test_synth_mono_replace(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channel(synth, CH);
	assert(sf64_synth_set_preset(synth, 0, 0));

	silence();
	sf64_synth_note_on(synth, 48, 100);
	mix(256);
	double pos_a = mixer_ch_get_pos(CH);
	sf64_synth_note_on(synth, 60, 100);
	double pos_b = mixer_ch_get_pos(CH);
	if (pos_b >= pos_a && pos_b > 64) {
		// A restart must rewind; a continued A would keep advancing.
		printf("FAILED mono: note-on did not restart (pos %.1f -> %.1f)\n",
			pos_a, pos_b);
		sf64_synth_close(synth);
		return false;
	}
	if (!mixer_ch_playing(CH)) {
		printf("FAILED mono: second note not playing\n");
		sf64_synth_close(synth);
		return false;
	}

	// note-off wrong key ignored; matching key stops.
	sf64_synth_note_off(synth, 48);
	if (!mixer_ch_playing(CH)) {
		printf("FAILED mono: wrong-key note-off stopped the note\n");
		sf64_synth_close(synth);
		return false;
	}
	sf64_synth_note_off(synth, 60);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED mono: note-off did not stop\n");
		sf64_synth_close(synth);
		return false;
	}

	// Stop while already idle, and a second identical note-off, must stay quiet.
	sf64_synth_note_off(synth, 60);
	sf64_synth_note_off(synth, 60);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED mono: repeated note-off left channel playing\n");
		sf64_synth_close(synth);
		return false;
	}

	// velocity 0 == note-off
	sf64_synth_note_on(synth, 60, 100);
	sf64_synth_note_on(synth, 60, 0);
	mix(64);
	if (mixer_ch_playing(CH)) {
		printf("FAILED mono: velocity 0 did not note-off\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
}

static bool test_synth_preset(sf64_bank_t *bank)
{
	sf64_synth_t *synth = sf64_synth_create(bank);
	sf64_synth_set_channel(synth, CH);

	if (sf64_synth_set_preset(synth, 0, 99)) {
		printf("FAILED preset: missing program accepted\n");
		sf64_synth_close(synth);
		return false;
	}
	if (!sf64_synth_set_preset(synth, 0, 0)) {
		printf("FAILED preset: program 0 rejected\n");
		sf64_synth_close(synth);
		return false;
	}

	// Change preset while a note is sounding: the sounding wave stays,
	// the next note-on uses the new preset.
	silence();
	sf64_synth_note_on(synth, 60, 100);
	waveform_t *before = mixer_ch_playing_waveform(CH);
	assert(sf64_synth_set_preset(synth, 0, 1));
	if (mixer_ch_playing_waveform(CH) != before) {
		printf("FAILED preset: change interrupted the sounding note\n");
		sf64_synth_close(synth);
		return false;
	}
	// High velocity on VelSplit → sample D, not A.
	sf64_synth_note_on(synth, 60, 100);
	sf64_region_t *rd = &bank->regions[bank->presets[1].first_region + 1];
	if (mixer_ch_playing_waveform(CH) != &bank->waves[rd->sample_index]->wave) {
		printf("FAILED preset: next note did not use new preset\n");
		sf64_synth_close(synth);
		return false;
	}

	sf64_synth_close(synth);
	return true;
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
	mixer_init(4);
	mixer_ch_set_limits(CH, 16, SAMPLE_RATE * 4, 0);

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

	printf("synth\n");
	fflush(stdout);
	// Oneshoot helpers pin the channel to SAMPLE_RATE; pitch tests need headroom.
	mixer_ch_set_limits(CH, 16, SAMPLE_RATE * 4, 0);
	total++; if (!test_synth_pitch(bank)) failed++;
	total++; if (!test_synth_key_split(bank)) failed++;
	total++; if (!test_synth_vel_split(bank)) failed++;
	total++; if (!test_synth_mono_replace(bank)) failed++;
	total++; if (!test_synth_preset(bank)) failed++;

	sf64_close(bank);

	printf("\n%d/%d tests failed\n", failed, total);
	if (failed == 0)
		printf("SUCCESS\n");
	fflush(stdout);
	emux_ioctl_exit();
	return failed ? 1 : 0;
}
