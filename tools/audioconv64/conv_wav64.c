#include "../../src/audio/wav64internal.h"

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

#include "../../src/audio/libopus.c"

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;
int flag_wav_compress = 1;
int flag_wav_resample = 0;
bool flag_wav_mono = false;
const int OPUS_SAMPLE_RATE = 48000;

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
		const char *compr[4] = { "raw", "vadpcm", "raw", "opus" };
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

	int wavOriginalSampleRate = wav.sampleRate;

	// When compressing with opus, we need to resample to 32 Khz. Whatever value
	// was selected by the user, we force it to 32 Khz.
	if (flag_wav_compress == 3) {
		if (flag_verbose)
			fprintf(stderr, "  opus only supports %d kHz, forcing resample\n", OPUS_SAMPLE_RATE/1000);

		// If the user asked to resample to a certain sample rate, keep that in
		// mind for later when we will calculate th opus output bitrate.
		// Basically --wav-resample becomes a way to tune the bitrate,
		// but resampling is always done to OPUS_SAMPLE_RATE.
		if (flag_wav_resample)
			wavOriginalSampleRate = flag_wav_resample;
		flag_wav_resample = OPUS_SAMPLE_RATE;
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
	// Compressed waveforms always expand to 16 (both vadpcm and opus only supports 16 bits)
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

		// Amount of data that can be overread by the player.
		const int OVERREAD_BYTES = 64;
		if (loop_len == 0) {
			for (int i=0;i<OVERREAD_BYTES;i++)
				fputc(0, out);
		} else {
			int idx = cnt - loop_len;
			int nb = 0;
			while (nb < OVERREAD_BYTES) {
				int16_t *sptr = wav.samples + idx*wav.channels;
				for (int ch=0;ch<wav.channels;ch++) {
					nb += fwrite(sptr, 1, nbits==8 ? 1 : 2, out);
					sptr++;
				}
				idx++;
				if (idx == cnt)
					idx -= loop_len;
			}
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

	case 3: { // opus
		// Frame size: for now this is hardcoded to frames of 20ms, which is the
		// maximum support by celt and also the best for quality.
		// 48 Khz => 960 samples
		// 32 Khz => 640 samples
		const int FRAMES_PER_SECOND = 50;
		int frame_size = wav.sampleRate / FRAMES_PER_SECOND;
		int err = OPUS_OK;

		OpusCustomMode *custom_mode = opus_custom_mode_create(
			wav.sampleRate, frame_size, &err);
		if (err != OPUS_OK) {
			fprintf(stderr, "ERROR: %s: cannot create opus custom mode: %s\n", infn, opus_strerror(err));
			failed = true; goto end;
		}

		OpusCustomEncoder *enc = opus_custom_encoder_create(
				custom_mode, wav.channels, &err);
		if (err != OPUS_OK) {
			opus_custom_mode_destroy(custom_mode);
			fprintf(stderr, "ERROR: %s: cannot create opus encoder: %s\n", infn, opus_strerror(err));
			failed = true; goto end;
		}

		// Automatic bitrate calculation for "good quality". This is the same
		// algorithm libopus selects when setting OPUS_AUTO bitrate.
		int bitrate_bps = 60*FRAMES_PER_SECOND + wavOriginalSampleRate * wav.channels;
		if (flag_verbose)
			fprintf(stderr, "  opus bitrate: %d bps\n", bitrate_bps);

		// Write extended header
		w32(out, frame_size);
		uint32_t max_cmp_size_pos = w32_placeholder(out);  // max compressed frame size
		w32(out, bitrate_bps);
		w32_at(out, wstart_offset, ftell(out));

		// Configure opus encoder. We use VBR as it provides the best
		// compression/quality balance and we don't have specific constraints
		// there. We select the maximum algorithmic complexity to get the best quality.
		opus_custom_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_bps));
		opus_custom_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_AUTO));
		opus_custom_encoder_ctl(enc, OPUS_SET_VBR(1));
		opus_custom_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(0));
		opus_custom_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
		opus_custom_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
		opus_custom_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(OPUS_AUTO));
		opus_custom_encoder_ctl(enc, OPUS_SET_DTX(0));
		opus_custom_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(0));
		opus_custom_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));

		// Pad input samples with zeros, rounding to frame size
		int newcnt = (cnt + frame_size - 1) / frame_size * frame_size;
		wav.samples = realloc(wav.samples, newcnt * wav.channels * sizeof(int16_t));
		memset(wav.samples + cnt, 0, (newcnt - cnt) * wav.channels * sizeof(int16_t));
		
		int max_nb = 0;
		int out_max_size = bitrate_bps/8; // overestimation
		uint8_t *out_buffer = malloc(out_max_size);
		for (int i=0; i<newcnt; i+=frame_size) {
			int nb = opus_custom_encode(enc, wav.samples + i*wav.channels, frame_size, out_buffer, out_max_size);
			if (nb < 0) {
				fprintf(stderr, "ERROR: %s: opus encoding failed: %s\n", infn, opus_strerror(nb));
				failed = true;
				break;
			}

			w16(out, nb);
			fwrite(out_buffer, 1, nb, out);
			if (nb > max_nb)
				max_nb = nb;
			walign(out, 2);	// make sure frames are 2-byte aligned
		}

		w32_at(out, max_cmp_size_pos, max_nb); // write maxixum compressed frame size
		
		free(out_buffer);
		opus_custom_encoder_destroy(enc);

		if (flag_debug) {
			fclose(out);

			char* wav2fn = changeext(outfn, ".opus.wav");
			if (flag_verbose)
				fprintf(stderr, "  writing uncompressed file %s\n", wav2fn);

			out = fopen(outfn, "rb");
			fseek(out, 36, SEEK_SET);
			OpusCustomDecoder *dec = opus_custom_decoder_create(
					custom_mode, wav.channels, &err);
			if (err != OPUS_OK) {
				opus_custom_mode_destroy(custom_mode);
				fprintf(stderr, "ERROR: %s: cannot create opus decoder: %s\n", infn, opus_strerror(err));
				free(wav2fn);
				failed = true; goto end;
			}

			// Decode the whole file to check for errors
			int16_t *out_samples = malloc(newcnt * wav.channels * sizeof(int16_t));
			int outcnt = 0;
			for (int i=0; i<newcnt; i+=frame_size) {
				int nb = fgetc(out) << 8;
				nb |= fgetc(out);
				if (nb < 0) {
					fprintf(stderr, "ERROR: %s: opus decoding failed: %s\n", infn, opus_strerror(nb));
					failed = true;
					break;
				}

				uint8_t in_samples[nb];
				fread(in_samples, 1, nb, out);
				if (nb & 1) fgetc(out); // align to 2-byte boundary

				int ret = opus_custom_decode(dec, in_samples, nb, out_samples + outcnt*wav.channels, frame_size);
				if (ret < 0) {
					fprintf(stderr, "ERROR: %s: opus decoding failed: %s\n", infn, opus_strerror(ret));
					failed = true;
					break;
				}
				outcnt += frame_size;
			}

			// Save decoded samples into WAV file
			if (!failed) {
				drwav_data_format fmt = {
					.container = drwav_container_riff,
					.format = DR_WAVE_FORMAT_PCM,
					.channels = wav.channels,
					.sampleRate = wav.sampleRate,
					.bitsPerSample = 16,
				};
				drwav wav2;
				if (!drwav_init_file_write(&wav2, wav2fn, &fmt, NULL)) {
					fprintf(stderr, "ERROR: %s: cannot create WAV file\n", outfn);
					failed = true;
				} else {
					drwav_write_pcm_frames(&wav2, outcnt, out_samples);
					drwav_uninit(&wav2);
				}
			}
			
			free(wav2fn);
		}

		opus_custom_mode_destroy(custom_mode);
	} break;
	}

end:
	fclose(out);
	free(wav.samples);
	if (failed) {
		remove(outfn);
		return 1;
	}
	return 0;
}
