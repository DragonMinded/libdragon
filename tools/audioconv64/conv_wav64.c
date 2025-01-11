#include "../../src/audio/wav64_internal.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "vadpcm/vadpcm.h"
#include "vadpcm/encode.c"
#include "vadpcm/error.c"

#include "../common/binout.c"
#include "../common/binout.h"
#include "../common/polyfill.h"

#define ENABLE_SINC_BEST_CONVERTER
#define PACKAGE "libsamplerate"
#define VERSION "0.1.9"
#include "libsamplerate/samplerate.h"
#include "libsamplerate/samplerate.c"
#include "libsamplerate/src_sinc.c"
#include "libsamplerate/src_zoh.c"
#include "libsamplerate/src_linear.c"
#undef PACKAGE
#undef VERSION
#undef MIN
#undef MAX

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;
int flag_wav_compress = 1;
int flag_wav_resample = 0;
bool flag_wav_mono = false;

typedef struct {
	int16_t *samples;
	int channels;
	int bitsPerSample;
	int sampleRate;
} wav_data_t;

static size_t read_wav(const char *infn, wav_data_t *out)
{
	drwav wav;
	if (!drwav_init_file(&wav, infn, NULL)) {
		fprintf(stderr, "ERROR: %s: not a valid WAV/RIFF/AIFF file\n", infn);
		return 0;
	}

	// Decode the samples as 16bit little-endian. This will decode everything including
	// compressed formats so that we're able to read any kind of WAV file, though
	// it will end up as an uncompressed file.
	int16_t* samples = malloc(wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
	size_t cnt = drwav_read_pcm_frames_s16le(&wav, wav.totalPCMFrameCount, samples);
	if (cnt != wav.totalPCMFrameCount) {
		fprintf(stderr, "WARNING: %s: %d frames found, but only %zu decoded\n", infn, (int)wav.totalPCMFrameCount, cnt);
	}

	out->samples = samples;
	out->channels = wav.channels;
	out->bitsPerSample = wav.bitsPerSample;
	out->sampleRate = wav.sampleRate;
	drwav_uninit(&wav);
	return cnt;
}

static size_t read_mp3(const char *infn, wav_data_t *out)
{
	drmp3 mp3;
	if (!drmp3_init_file(&mp3, infn, NULL)) {
		fprintf(stderr, "ERROR: %s: not a valid MP3 file\n", infn);
		return 0;
	}

	uint64_t nframes = drmp3_get_pcm_frame_count(&mp3);
	int16_t* samples = malloc(nframes * mp3.channels * sizeof(int16_t));
	size_t cnt = drmp3_read_pcm_frames_s16(&mp3, nframes, samples);
	if (cnt != nframes) {
		fprintf(stderr, "WARNING: %s: %d frames found, but only %zu decoded\n", infn, (int)nframes, cnt);
	}

	out->samples = samples;
	out->channels = mp3.channels;
	out->bitsPerSample = 16;
	out->sampleRate = mp3.sampleRate;
	drmp3_uninit(&mp3);
	return cnt;
}

int wav_convert(const char *infn, const char *outfn) {
	if (flag_verbose) {
		const char *compr[3] = { "raw", "vadpcm", "raw" };
		fprintf(stderr, "Converting: %s => %s (%s)\n", infn, outfn, compr[flag_wav_compress]);
	}

	bool failed = false;
	wav_data_t wav; size_t cnt;

	// Read the input file
	if (strcasestr(infn, ".mp3"))
		cnt = read_mp3(infn, &wav);
	else
		cnt = read_wav(infn, &wav);
	if (cnt == 0) {
		return 1;
	}

	if (flag_verbose)
		fprintf(stderr, "  input: %d bits, %d Hz, %d channels\n", wav.bitsPerSample, wav.sampleRate, wav.channels);

	// Check if the user requested conversion to mono
	if (flag_wav_mono && wav.channels == 2) {
		if (flag_verbose)
			fprintf(stderr, "  converting to mono\n");

		// Allocate a new buffer for the mono samples
		int16_t *mono_samples = malloc(cnt * sizeof(int16_t));

		// Convert to mono
		int16_t *sptr = wav.samples;
		int16_t *dptr = mono_samples;
		for (int i=0;i<cnt;i++) {
			int32_t v = *sptr + *(sptr+1);
			v /= 2;
			*dptr = v;
			sptr += 2;
			dptr++;
		}

		// Replace the samples buffer with the mono one
		free(wav.samples);
		wav.samples = mono_samples;
		wav.channels = 1;
	}

	// Do sample rate conversion if requested
	if (flag_wav_resample && wav.sampleRate != flag_wav_resample) {
		if (flag_verbose)
			fprintf(stderr, "  resampling to %d Hz\n", flag_wav_resample);

		// Convert input samples to float
		float *fsamples_in = malloc(cnt * wav.channels * sizeof(float));
		src_short_to_float_array(wav.samples, fsamples_in, cnt * wav.channels);

		// Allocate output buffer, estimating the size based on the ratio.
		// We add some margin because we are not sure of rounding errors.
		int newcnt = cnt * flag_wav_resample / wav.sampleRate + 16;
		float *fsamples_out = malloc(newcnt * wav.channels * sizeof(float));

		// Do the conversion
		SRC_DATA data = {
			.data_in = fsamples_in,
			.input_frames = cnt,
			.data_out = fsamples_out,
			.output_frames = newcnt,
			.src_ratio = (double)flag_wav_resample / wav.sampleRate,
		};
		int err = src_simple(&data, SRC_SINC_BEST_QUALITY, wav.channels);
		if (err != 0) {
			fprintf(stderr, "ERROR: %s: resampling failed: %s\n", infn, src_strerror(err));
			free(fsamples_in);
			free(fsamples_out);
			free(wav.samples);
			return 1;
		}

		// Extract the number of samples generated, and convert back to 16-bit
		cnt = data.output_frames_gen;
		wav.samples = realloc(wav.samples, cnt * wav.channels * sizeof(int16_t));
		src_float_to_short_array(fsamples_out, wav.samples, cnt * wav.channels);

		free(fsamples_in);
		free(fsamples_out);

		// Update wav.sampleRate as it will be used later
		wav.sampleRate = flag_wav_resample;

		// Update also the loop offset to the new sample rate
		flag_wav_looping_offset = flag_wav_looping_offset * flag_wav_resample / wav.sampleRate;
	}

	// Keep 8 bits file if original is 8 bit, otherwise expand to 16 bit.
	// Compressed waveforms always expand to 16 bit.
	int nbits = wav.bitsPerSample == 8 ? 8 : 16;
	if (flag_wav_compress != 0)
		nbits = 16;

	int loop_len = flag_wav_looping ? cnt - flag_wav_looping_offset : 0;
	if (loop_len < 0) {
		fprintf(stderr, "WARNING: %s: invalid looping offset: %d (size: %zu)\n", infn, flag_wav_looping_offset, cnt);
		loop_len = 0;
	}
	if (loop_len&1 && nbits==8) {
		// Odd loop lengths are not supported for 8-bit waveforms because they would
		// change the 2-byte phase between ROM and RDRAM addresses during loop unrolling.
		// We shorten the loop by 1 sample which shouldn't matter.
		fprintf(stderr, "WARNING: %s: invalid looping size: %d\n", infn, loop_len);
		loop_len -= 1;
	}

	FILE *out = fopen(outfn, "wb");
	if (!out) {
		fprintf(stderr, "ERROR: %s: cannot create file\n", outfn);
		free(wav.samples);
		return 1;
	}

	char id[4] = "WV64";
	fwrite(id, 1, 4, out);
	w8(out, WAV64_FILE_VERSION);
	w8(out, flag_wav_compress);
	w8(out, wav.channels);
	w8(out, nbits);
	w32(out, wav.sampleRate);
	w32(out, cnt);
	w32(out, loop_len);
	int wstart_offset = w32_placeholder(out); // start_offset (to be filled later)

	switch (flag_wav_compress) {
	case 0: { // no compression
		w32_at(out, wstart_offset, ftell(out));
		int16_t *sptr = wav.samples;
		for (int i=0;i<cnt*wav.channels;i++) {
			// Byteswap *sptr
			int16_t v = *sptr;
			v = ((v & 0xFF00) >> 8) | ((v & 0x00FF) << 8);
			*sptr = v;
			// Write the sample as 16bit or 8bit. Since *sptr is 16-bit big-endian,
			// the 8bit representation is just the first byte (MSB). Notice
			// that WAV64 8bit is signed anyway.
			fwrite(sptr, 1, nbits == 8 ? 1 : 2, out);
			sptr++;
		}
	} break;

	case 1: { // vadpcm
		if (cnt % kVADPCMFrameSampleCount) {
			int newcnt = (cnt + kVADPCMFrameSampleCount - 1) / kVADPCMFrameSampleCount * kVADPCMFrameSampleCount;
			wav.samples = realloc(wav.samples, newcnt * wav.channels * sizeof(int16_t));
			memset(wav.samples + cnt, 0, (newcnt - cnt) * wav.channels * sizeof(int16_t));
			cnt = newcnt;
		}

		enum { kPREDICTORS = 4 };

		assert(cnt % kVADPCMFrameSampleCount == 0);
		int nframes = cnt / kVADPCMFrameSampleCount;
		void *scratch = malloc(vadpcm_encode_scratch_size(nframes));
		struct vadpcm_vector *codebook = alloca(kPREDICTORS * kVADPCMEncodeOrder * wav.channels * sizeof(struct vadpcm_vector));
		struct vadpcm_params parms = { .predictor_count = kPREDICTORS };
		void *dest = malloc(nframes * kVADPCMFrameByteSize * wav.channels);
		
		if (flag_verbose)
			fprintf(stderr, "  compressing into VADPCM format (%d frames)\n", nframes);

		int16_t *schan = malloc(cnt * sizeof(int16_t));
		uint8_t *destchan = dest;
		for (int i=0; i<wav.channels; i++) {
			for (int j=0; j<cnt; j++)
				schan[j] = wav.samples[i + j*wav.channels];
			vadpcm_error err = vadpcm_encode(&parms, codebook + kPREDICTORS * kVADPCMEncodeOrder * i, nframes, destchan, schan, scratch);
			if (err != 0) {
				fprintf(stderr, "VADPCM encoding error: %s\n", vadpcm_error_name(err));
				return 1;
			}
			destchan += nframes * kVADPCMFrameByteSize;
		}

		struct vadpcm_vector state = {0};
		w8(out, kPREDICTORS);
		w8(out, kVADPCMEncodeOrder);
		w16(out, 0); // padding
		w32(out, 0); // padding
		fwrite(&state, 1, sizeof(struct vadpcm_vector), out);   // TBC: loop_state[0]
		fwrite(&state, 1, sizeof(struct vadpcm_vector), out);   // TBC: loop_state[1]
		fwrite(&state, 1, sizeof(struct vadpcm_vector), out);   // state
		fwrite(&state, 1, sizeof(struct vadpcm_vector), out);   // state
		for (int i=0; i<kPREDICTORS * kVADPCMEncodeOrder * wav.channels; i++)    // codebook
			for (int j=0; j<8; j++)
				w16(out, codebook[i].v[j]);
		w32_at(out, wstart_offset, ftell(out));
		for (int i=0;i<nframes;i++) {
			for (int j=0;j<wav.channels;j++)
				fwrite(dest + (j * nframes + i) * kVADPCMFrameByteSize, 1, kVADPCMFrameByteSize, out);
		}
		free(dest);
		free(scratch);
	} break;

	}

	fclose(out);
	free(wav.samples);
	if (failed) {
		remove(outfn);
		return 1;
	}
	return 0;
}
