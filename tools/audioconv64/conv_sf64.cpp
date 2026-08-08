/*
    conv_sf64: convert SoundFont 2 files to SF64 format
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
/*
 * SF2 is loaded with a vendored TinySoundFont (MIT). We use its resolved
 * preset/region tables (not the public render API), copy them into SF64
 * structures, and embed each unique sample slice as a WAV64.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <cstdint>

#include "../common/binout.h"
#include "../common/polyfill.h"
#include "../common/utils.h"
#include "../common/assetcomp.h"
#include "../common/crc64.c"
#include "audioconv64.h"
#include "../../src/audio/sf64_internal.h"

#define TSF_IMPLEMENTATION
#include "tinysoundfont/tsf.h"

int flag_sf_compress = 1;

/** CRC-64 over PCM + playback variant params (rate/channels/loop). */
static uint64_t hash_sample_variant(const int16_t *pcm, int cnt, int rate, int ch,
	int loop_start, int loop_end)
{
	uint64_t h = 0;
	h = crc64(h, (const unsigned char *)pcm, (uint64_t)cnt * ch * sizeof(int16_t));
	h = crc64(h, (const unsigned char *)&rate, sizeof(rate));
	h = crc64(h, (const unsigned char *)&ch, sizeof(ch));
	h = crc64(h, (const unsigned char *)&loop_start, sizeof(loop_start));
	h = crc64(h, (const unsigned char *)&loop_end, sizeof(loop_end));
	return h;
}

static int16_t secs_to_timecents(float sec)
{
	if (sec <= 0.0f) return -12000;
	float tc = 1200.0f * log2f(sec);
	if (tc < -12000.0f) return -12000;
	if (tc > 8000.0f) return 8000;
	return (int16_t)lroundf(tc);
}

static int16_t gain_to_centibels(float gain)
{
	if (gain >= 1.0f) return 0;
	if (gain <= 0.0f) return 1440;
	float cb = -200.0f * log10f(gain);
	if (cb < 0.0f) return 0;
	if (cb > 1440.0f) return 1440;
	return (int16_t)lroundf(cb);
}

/** Hold/decay may still be in timecents when keynum scaling is active. */
static int16_t env_time_field(float v, float keynum_scale)
{
	if (keynum_scale != 0.0f)
		return (int16_t)lroundf(v);
	return secs_to_timecents(v);
}

static void copy_amp_env(sf64_envelope_t *dst, const struct tsf_envelope *src)
{
	dst->delay_timecents = secs_to_timecents(src->delay);
	dst->attack_timecents = secs_to_timecents(src->attack);
	dst->hold_timecents = env_time_field(src->hold, src->keynumToHold);
	dst->decay_timecents = env_time_field(src->decay, src->keynumToDecay);
	dst->sustain_centibels = gain_to_centibels(src->sustain);
	dst->release_timecents = secs_to_timecents(src->release);
	dst->keynum_to_hold = (int16_t)lroundf(src->keynumToHold);
	dst->keynum_to_decay = (int16_t)lroundf(src->keynumToDecay);
}

static bool env_used(const struct tsf_envelope *e)
{
	if (e->delay > 0 || e->attack > 0 || e->release > 0) return true;
	if (e->keynumToHold || e->keynumToDecay) return true;
	if (e->keynumToHold == 0 && e->hold > 0) return true;
	if (e->keynumToDecay == 0 && e->decay > 0) return true;
	return e->sustain < 0.999f;
}

static void warn_unsupported(const char *preset, int region, const char *feat, std::set<std::string> &seen)
{
	std::string key = std::string(feat) + "@" + preset;
	if (!seen.insert(key).second) return;
	fprintf(stderr, "WARNING: SF2 preset '%s' region %d: ignoring unsupported %s\n",
		preset, region, feat);
}

static void write_env(FILE *f, const sf64_envelope_t *e)
{
	w16(f, e->delay_timecents);
	w16(f, e->attack_timecents);
	w16(f, e->hold_timecents);
	w16(f, e->decay_timecents);
	w16(f, e->sustain_centibels);
	w16(f, e->release_timecents);
	w16(f, e->keynum_to_hold);
	w16(f, e->keynum_to_decay);
}

int sf_convert(const char *infn, const char *outfn)
{
	if (flag_wav_compress_vadpcm_huffman < 0)
		flag_wav_compress_vadpcm_huffman = 0;

	tsf *font = tsf_load_filename(infn);
	if (!font) fatal("ERROR: cannot load SF2: %s\n", infn);

	std::vector<sf64_preset_t> presets;
	std::vector<sf64_region_t> regions;
	std::vector<sf64_sample_t> samples;
	std::vector<std::string> names;
	std::map<uint64_t, int> sample_by_hash;
	std::set<std::string> warn_seen;
	int64_t raw_pcm_bytes = 0;
	int64_t unique_pcm_bytes = 0;
	int64_t embedded_wav = 0;

	FILE *out = fopen(outfn, "wb");
	if (!out) fatal("ERROR: cannot create: %s\n", outfn);
	placeholder_clear();

	wa(out, SF64_ID, 4);
	w8(out, SF64_VERSION);
	w8(out, 0); // flags
	w16_placeholderf(out, "num_presets");
	w16_placeholderf(out, "num_regions");
	w16_placeholderf(out, "num_samples");
	w32_placeholderf(out, "metadata_offset");
	w32_placeholderf(out, "metadata_size");
	w32_placeholderf(out, "sample_data_offset");

	walign(out, 2);
	placeholder_set(out, "sample_data_offset");

	for (int pi = 0; pi < font->presetNum; pi++) {
		struct tsf_preset *p = &font->presets[pi];
		sf64_preset_t sp = {};
		sp.bank = p->bank;
		sp.program = (uint8_t)p->preset;
		sp.first_region = (uint16_t)regions.size();
		sp.num_regions = 0;

		for (int ri = 0; ri < p->regionNum; ri++) {
			struct tsf_region *r = &p->regions[ri];

			if (env_used(&r->modenv))
				warn_unsupported(p->presetName, ri, "modulation envelope", warn_seen);
			if (r->initialFilterFc != 13500 || r->initialFilterQ != 0)
				warn_unsupported(p->presetName, ri, "filter", warn_seen);
			if (r->modEnvToPitch || r->modEnvToFilterFc)
				warn_unsupported(p->presetName, ri, "modEnvToPitch/FilterFc", warn_seen);
			if (r->modLfoToPitch || r->modLfoToFilterFc || r->modLfoToVolume || r->freqModLFO)
				warn_unsupported(p->presetName, ri, "modulation LFO", warn_seen);
			if (r->vibLfoToPitch || r->freqVibLFO)
				warn_unsupported(p->presetName, ri, "vibrato LFO", warn_seen);

			unsigned int start = r->offset;
			unsigned int end = r->end;
			if (end <= start) continue;
			int cnt = (int)(end - start);

			int loop_mode = SF64_LOOP_NONE;
			int loop_start = 0, loop_end = 0;
			if (r->loop_mode != TSF_LOOPMODE_NONE && r->loop_start < r->loop_end) {
				loop_mode = (r->loop_mode == TSF_LOOPMODE_SUSTAIN)
					? SF64_LOOP_SUSTAIN : SF64_LOOP_CONTINUOUS;
				// TSF stores loop_end inclusive; WAV64 wants exclusive.
				loop_start = (int)r->loop_start - (int)start;
				loop_end = (int)r->loop_end + 1 - (int)start;
				if (loop_start < 0) loop_start = 0;
				if (loop_end > cnt) loop_end = cnt;
				if (loop_end <= loop_start) {
					loop_mode = SF64_LOOP_NONE;
					loop_start = loop_end = 0;
				}
			}

			int16_t *pcm = (int16_t*)malloc(cnt * sizeof(int16_t));
			for (int i = 0; i < cnt; i++) {
				float s = font->fontSamples[start + i];
				if (s > 1.0f) s = 1.0f;
				else if (s < -1.0f) s = -1.0f;
				pcm[i] = (int16_t)lroundf(s * 32767.0f);
			}

			int loop_start_meta = loop_mode != SF64_LOOP_NONE ? loop_start : 0;
			int loop_end_meta = loop_mode != SF64_LOOP_NONE ? loop_end : 0;
			uint64_t hash = hash_sample_variant(pcm, cnt, (int)r->sample_rate, 1,
				loop_start_meta, loop_end_meta);
			raw_pcm_bytes += (int64_t)cnt * 2;

			int sample_index;
			auto it = sample_by_hash.find(hash);
			if (it != sample_by_hash.end()) {
				sample_index = it->second;
				free(pcm);
			} else {
				unique_pcm_bytes += (int64_t)cnt * 2;
				wav_data_t wav = {};
				wav.samples = pcm;
				wav.cnt = cnt;
				wav.channels = 1;
				wav.bitsPerSample = 16;
				wav.sampleRate = (int)r->sample_rate;
				wav.looping = loop_mode != SF64_LOOP_NONE;
				wav.loopOffset = loop_start;
				wav.loopEnd = loop_mode != SF64_LOOP_NONE ? loop_end : 0;

				walign(out, 2);
				uint32_t wav_off = ftell(out);
				char *wavfn = NULL;
				asprintf(&wavfn, "%s.%d.%d.wav64", outfn, pi, ri);
				if (!wav64_write(infn, wavfn, out, &wav, flag_sf_compress))
					fatal("ERROR: failure writing sample for %s\n", infn);
				free(wavfn);
				uint32_t wav_sz = ftell(out) - wav_off;
				embedded_wav += wav_sz;

				if (samples.size() >= 0xffffu)
					fatal("ERROR: too many unique samples (max 65535)\n");

				sf64_sample_t ss = {};
				ss.wav64_offset = wav_off;
				ss.wav64_size = wav_sz;
				ss.pcm_hash = hash;
				ss.sample_start = 0;
				// Keep the pre-padding logical length (wav64_write may pad cnt).
				ss.sample_end = (uint32_t)cnt;
				ss.loop_start = (uint32_t)loop_start_meta;
				ss.loop_end = (uint32_t)loop_end_meta;
				ss.sample_rate = (uint32_t)r->sample_rate;
				ss.channels = 1;
				sample_index = (int)samples.size();
				samples.push_back(ss);
				sample_by_hash[hash] = sample_index;
				free(wav.samples);
			}

			if (regions.size() >= 0xffffu)
				fatal("ERROR: too many regions (max 65535)\n");

			sf64_region_t sr = {};
			sr.sample_index = (uint16_t)sample_index;
			sr.key_min = r->lokey;
			sr.key_max = r->hikey;
			sr.velocity_min = r->lovel;
			sr.velocity_max = r->hivel;
			sr.loop_mode = (uint8_t)loop_mode;
			sr.exclusive_group = r->group > 255 ? 255 : (uint8_t)r->group;
			sr.root_key = (int8_t)r->pitch_keycenter;
			sr.coarse_tune = (int8_t)r->transpose;
			sr.fine_tune = (int16_t)r->tune;
			sr.pitch_keytrack = (int16_t)r->pitch_keytrack;
			// TinySoundFont applies InitialAttenuation with factor 0.01 instead
			// of the SF2 0.1 (cB→dB). Multiply by 100 to recover centibels.
			sr.attenuation_cb = (int16_t)lroundf(r->attenuation * 100.0f);
			if (sr.attenuation_cb < 0) sr.attenuation_cb = 0;
			if (sr.attenuation_cb > 1440) sr.attenuation_cb = 1440;
			sr.pan = (int16_t)lroundf(r->pan * 1000.0f);
			copy_amp_env(&sr.amp_env, &r->ampenv);
			regions.push_back(sr);
			sp.num_regions++;
		}

		if (sp.num_regions) {
			if (presets.size() >= 0xffffu)
				fatal("ERROR: too many presets (max 65535)\n");
			names.push_back(p->presetName);
			presets.push_back(sp);
		}
	}

	tsf_close(font);

	// Build uncompressed metadata: presets, regions, samples, then names.
	FILE *meta = tmpfile();
	assert(meta);

	uint32_t name_base = (uint32_t)(
		presets.size() * sizeof(sf64_preset_t) +
		regions.size() * sizeof(sf64_region_t) +
		samples.size() * sizeof(sf64_sample_t));
	uint32_t name_off = name_base;
	for (size_t i = 0; i < presets.size(); i++) {
		presets[i].name_offset = name_off;
		name_off += (uint32_t)names[i].size() + 1;
	}

	for (auto &p : presets) {
		w16(meta, p.bank);
		w8(meta, p.program);
		w8(meta, p.flags);
		w16(meta, p.first_region);
		w16(meta, p.num_regions);
		w32(meta, p.name_offset);
	}
	for (auto &r : regions) {
		w16(meta, r.sample_index);
		w8(meta, r.key_min);
		w8(meta, r.key_max);
		w8(meta, r.velocity_min);
		w8(meta, r.velocity_max);
		w8(meta, r.loop_mode);
		w8(meta, r.exclusive_group);
		w8(meta, r.root_key);
		w8(meta, r.coarse_tune);
		w16(meta, r.fine_tune);
		w16(meta, r.pitch_keytrack);
		w16(meta, r.attenuation_cb);
		w16(meta, r.pan);
		write_env(meta, &r.amp_env);
		w16(meta, r.reserved_flags);
		for (int i = 0; i < 4; i++) w16(meta, r.reserved[i]);
	}
	for (auto &s : samples) {
		w32(meta, s.wav64_offset);
		w32(meta, s.wav64_size);
		w64(meta, s.pcm_hash);
		w32(meta, s.sample_start);
		w32(meta, s.sample_end);
		w32(meta, s.loop_start);
		w32(meta, s.loop_end);
		w32(meta, s.sample_rate);
		w8(meta, s.channels);
		w8(meta, s.flags);
		w16(meta, s.reserved);
	}
	for (size_t i = 0; i < presets.size(); i++)
		wa(meta, names[i].c_str(), names[i].size() + 1);

	std::vector<uint8_t> metadata = slurp(meta);
	fclose(meta);

	walign(out, 2);
	placeholder_set(out, "metadata_offset");
	int meta_cmp = asset_compress_mem(metadata.data(), metadata.size(), out,
		DEFAULT_COMPRESSION, 0, NULL);
	placeholder_set_offset(out, meta_cmp, "metadata_size");

	placeholder_set_offset(out, presets.size(), "num_presets");
	placeholder_set_offset(out, regions.size(), "num_regions");
	placeholder_set_offset(out, samples.size(), "num_samples");
	placeholder_clear();
	fclose(out);

	if (flag_verbose) {
		fprintf(stderr, "Converting: %s => %s\n", infn, outfn);
		fprintf(stderr, "  SF2 presets selected:     %zu\n", presets.size());
		fprintf(stderr, "  resolved regions:         %zu\n", regions.size());
		fprintf(stderr, "  unique sample variants:   %zu\n", samples.size());
		fprintf(stderr, "  embedded WAV64 size:      %lld KiB\n", (long long)(embedded_wav / 1024));
		fprintf(stderr, "  deduplicated size saved:  %lld KiB\n",
			(long long)((raw_pcm_bytes - unique_pcm_bytes) / 1024));
		fprintf(stderr, "  unsupported generators:   %d\n", (int)warn_seen.size());
		fprintf(stderr, "  stereo voices:            0\n");
	}

	return 0;
}
