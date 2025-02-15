#include "../../src/audio/wav64_internal.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "vadpcm/vadpcm.h"
#include "vadpcm/encode.c"
#include "vadpcm/decode.c"
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

#include "huff_vadpcm.c"

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;
int flag_wav_compress = 1;
int flag_wav_resample = 0;
bool flag_wav_mono = false;
const int OPUS_SAMPLE_RATE = 48000;

typedef struct {
	int16_t *samples;			// Samples (always 16-bit signed)
	int cnt;					// Number of audio frames
	int channels;				// Number of channels
	int bitsPerSample;
	int sampleRate;
	bool looping;
	int loopOffset;
} wav_data_t;

static bool read_wav(const char *infn, wav_data_t *out)
{
	drwav wav;
	if (!drwav_init_file_with_metadata(&wav, infn, 0, NULL)) {
		fprintf(stderr, "ERROR: %s: not a valid WAV/RIFF/AIFF file\n", infn);
		return false;
	}

	// Decode the samples as 16bit little-endian. This will decode everything including
	// compressed formats so that we're able to read any kind of WAV file, though
	// it will end up as an uncompressed file.
	int16_t* samples = malloc(wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
	out->cnt = drwav_read_pcm_frames_s16le(&wav, wav.totalPCMFrameCount, samples);
	if (out->cnt != wav.totalPCMFrameCount) {
		fprintf(stderr, "WARNING: %s: %d frames found, but only %d decoded\n", infn, (int)wav.totalPCMFrameCount, out->cnt);
	}

	out->samples = samples;
	out->channels = wav.channels;
	out->bitsPerSample = wav.bitsPerSample;
	out->sampleRate = wav.sampleRate;

	// Check if we find smpl metadata, and if so, extract the loop points.
	for (int i=0; i<wav.metadataCount; i++) {
		if (wav.pMetadata[i].type == drwav_metadata_type_smpl) {
			drwav_smpl* smpl = &wav.pMetadata[i].data.smpl;
			if (smpl->sampleLoopCount > 0) {
				if (flag_verbose)
					fprintf(stderr, "  found %d loop points [start=%d end=%d cnt=%d]\n", smpl->sampleLoopCount,
						smpl->pLoops[0].firstSampleByteOffset, smpl->pLoops[0].lastSampleByteOffset, out->cnt);

				// If we have multiple loops, we just take the first one.
				drwav_smpl_loop* loop = &smpl->pLoops[0];
				if (loop->type != 0) {
					fprintf(stderr, "WARNING: %s: loop type %d not supported\n", infn, loop->type);
					break;
				}
				// NOTE: the offset appears to be in samples, not bytes.
				// See also https://github.com/mackron/dr_libs/issues/267
				out->looping = true;
				out->loopOffset = loop->firstSampleByteOffset;
				if (out->cnt > loop->lastSampleByteOffset+1)
					out->cnt = loop->lastSampleByteOffset+1;
				break;
			}
		}
	}

	drwav_uninit(&wav);
	return true;
}

static size_t read_mp3(const char *infn, wav_data_t *out)
{
	drmp3 mp3;
	if (!drmp3_init_file(&mp3, infn, NULL)) {
		fprintf(stderr, "ERROR: %s: not a valid MP3 file\n", infn);
		return false;
	}

	uint64_t nframes = drmp3_get_pcm_frame_count(&mp3);
	int16_t* samples = malloc(nframes * mp3.channels * sizeof(int16_t));
	out->cnt = drmp3_read_pcm_frames_s16(&mp3, nframes, samples);
	if (out->cnt != nframes) {
		fprintf(stderr, "WARNING: %s: %d frames found, but only %d decoded\n", infn, (int)nframes, out->cnt);
	}

	out->samples = samples;
	out->channels = mp3.channels;
	out->bitsPerSample = 16;
	out->sampleRate = mp3.sampleRate;
	drmp3_uninit(&mp3);
	return true;
}

bool wav64_write(const char *infn, const char *outfn, FILE *out, wav_data_t* wav, size_t loop_len, int nbits, int format)
{
	bool failed = false;

	char id[4] = "WV64";
	fwrite(id, 1, 4, out);
	w8(out, 3); 				 			// version
	w8(out, format);  						// format
	w8(out, wav->channels);					// channels
	w8(out, nbits);							// bits
	w32(out, wav->sampleRate);				// frequency
	w32(out, wav->cnt);						// len
	w32(out, loop_len);						// loop_len
	w32_placeholderf(out, "samples");		// offset where samples begin
	w32_placeholderf(out, "state_size");    // size of per-mixer-channel state to allocate at runtime

	switch (format) {
	case 0: { // no compression
		// Uncompressed waveforms need to no state (0 bytes).
		placeholder_set_offset(out, 0, "state_size");

		// Start of the samples data
		placeholder_set(out, "samples");
		int16_t *sptr = wav->samples;
		for (int i=0;i<wav->cnt*wav->channels;i++) {
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
		bool huffman = true;

		// The state is 16 bytes per channel, but the runtime code requires to
		// always allocate both channels even for mono files.
		placeholder_set_offset(out, 48, "state_size");

		// We need cnt to be a multiple of kVADPCMFrameSampleCount (16) because
		// VADPCM are compressed using 16-sample frames.
		// In addition to that, our RSP decompressor at the moment only supports
		// multiples of 32 (for DMA alignment issues), so pad it to that.
		const int VADPCM_ALIGN = kVADPCMFrameSampleCount*2;
		if (wav->cnt % VADPCM_ALIGN) {
			int newcnt = (wav->cnt + VADPCM_ALIGN - 1) / VADPCM_ALIGN * VADPCM_ALIGN;
			wav->samples = realloc(wav->samples, newcnt * wav->channels * sizeof(int16_t));
			memset(wav->samples + wav->cnt, 0, (newcnt - wav->cnt) * wav->channels * sizeof(int16_t));
			wav->cnt = newcnt;
		}

		enum { kPREDICTORS = 4 };

		assert(wav->cnt % kVADPCMFrameSampleCount == 0);
		int nframes = wav->cnt / kVADPCMFrameSampleCount;
		void *scratch = malloc(vadpcm_encode_scratch_size(nframes));
		struct vadpcm_vector *codebook = alloca(kPREDICTORS * kVADPCMEncodeOrder * wav->channels * sizeof(struct vadpcm_vector));
		struct vadpcm_params parms = { .predictor_count = kPREDICTORS };
		void *dest = malloc(nframes * kVADPCMFrameByteSize * wav->channels);
		
		if (flag_verbose)
			fprintf(stderr, "  compressing into VADPCM format (%d frames)\n", nframes);

		int16_t *schan = malloc(wav->cnt * sizeof(int16_t));
		for (int i=0; i<wav->channels; i++) {
			uint8_t *destchan = malloc(nframes * kVADPCMFrameByteSize);
			for (int j=0; j<wav->cnt; j++)
				schan[j] = wav->samples[i + j*wav->channels];
			vadpcm_error err = vadpcm_encode(&parms, codebook + kPREDICTORS * kVADPCMEncodeOrder * i, nframes, destchan, schan, scratch);
			if (err != 0) {
				fprintf(stderr, "VADPCM encoding error: %s\n", vadpcm_error_name(err));
				return 1;
			}
			for (int j=0; j<nframes; j++)
				memcpy(dest + (i + wav->channels * j) * kVADPCMFrameByteSize, destchan + j * kVADPCMFrameByteSize, kVADPCMFrameByteSize);
			free(destchan);
		}
		free(scratch);

		const int maxcompbuflen = nframes * kVADPCMFrameByteSize * wav->channels;
		uint8_t *compbuf = malloc(maxcompbuflen);
		uint8_t *ctxbuf = calloc(HUFF_CONTEXT_LEN, 1);
		int compbuflen = 0;
		if (huffman) 
			compbuflen = huffv_compress(dest, nframes * kVADPCMFrameByteSize * wav->channels, compbuf, maxcompbuflen, ctxbuf, HUFF_CONTEXT_LEN);

		if (flag_verbose)
			fprintf(stderr, "  huffman compressed %d bytes into %d bytes (ratio: %.1f)\n", 
				nframes * kVADPCMFrameByteSize * wav->channels, compbuflen,
				100.0f * compbuflen / (nframes * kVADPCMFrameByteSize * wav->channels));

		uint8_t flags = 0;
		if (huffman) flags |= (1<<0);

		struct vadpcm_vector state = {0};
		w8(out, kPREDICTORS);
		w8(out, kVADPCMEncodeOrder);
		w8(out, flags);
		w8(out, 0);  // padding
		w32(out, 0); // padding
		fwrite(&state, 1, sizeof(struct vadpcm_vector), out);   // TBC: loop_state[0]
		fwrite(&state, 1, sizeof(struct vadpcm_vector), out);   // TBC: loop_state[1]
		fwrite(ctxbuf, 1, HUFF_CONTEXT_LEN, out);				// Huffman context
		for (int i=0; i<kPREDICTORS * kVADPCMEncodeOrder * wav->channels; i++)    // codebook
			for (int j=0; j<8; j++)
				w16(out, codebook[i].v[j]);

		// Start of samples data
		placeholder_set(out, "samples");
		if (huffman)
			fwrite(compbuf, 1, compbuflen, out);
		else
			fwrite(dest, 1, nframes * kVADPCMFrameByteSize * wav->channels, out);

		if (flag_debug) {
			char* wav2fn = changeext(outfn, ".vadpcm.wav");
			if (flag_verbose)
				fprintf(stderr, "  writing uncompressed file %s\n", wav2fn);
			
			int16_t *out_samples = malloc(wav->cnt * wav->channels * sizeof(int16_t));
			int16_t *out_channel = malloc(wav->cnt * sizeof(int16_t));
			for (int i=0;i<wav->channels;i++) {		
				uint8_t *in_channel = malloc(nframes * kVADPCMFrameByteSize);
				for (int j=0;j<nframes;j++)
					memcpy(in_channel + j * kVADPCMFrameByteSize, dest + (i + wav->channels * j) * kVADPCMFrameByteSize, kVADPCMFrameByteSize);

				memset(&state, 0, sizeof(state));
				vadpcm_decode(kPREDICTORS, kVADPCMEncodeOrder,
					codebook + kPREDICTORS * kVADPCMEncodeOrder * i,
					&state, nframes, out_channel, in_channel);
				for (int j=0;j<wav->cnt;j++)
					out_samples[i + j*wav->channels] = out_channel[j];
				free(in_channel);
			}
			free(out_channel);

			drwav_data_format fmt = {
				.container = drwav_container_riff,
				.format = DR_WAVE_FORMAT_PCM,
				.channels = wav->channels,
				.sampleRate = wav->sampleRate,
				.bitsPerSample = 16,
			};
			drwav wav2;
			if (!drwav_init_file_write(&wav2, wav2fn, &fmt, NULL)) {
				fprintf(stderr, "ERROR: %s: cannot create WAV file\n", outfn);
				failed = true;
			} else {
				drwav_write_pcm_frames(&wav2, wav->cnt, out_samples);
				drwav_uninit(&wav2);
			}
		}

		free(dest);
		free(compbuf);
		free(ctxbuf);
	} break;

	case 3: { // opus
		// Frame size: for now this is hardcoded to frames of 20ms, which is the
		// maximum support by celt and also the best for quality.
		// 48 Khz => 960 samples
		// 32 Khz => 640 samples
		const int FRAMES_PER_SECOND = 50;
		int frame_size = wav->sampleRate / FRAMES_PER_SECOND;
		int err = OPUS_OK;

		OpusCustomMode *custom_mode = opus_custom_mode_create(
			wav->sampleRate, frame_size, &err);
		if (err != OPUS_OK) {
			fprintf(stderr, "ERROR: %s: cannot create opus custom mode: %s\n", infn, opus_strerror(err));
			failed = true; goto end;
		}

		OpusCustomEncoder *enc = opus_custom_encoder_create(
				custom_mode, wav->channels, &err);
		if (err != OPUS_OK) {
			opus_custom_mode_destroy(custom_mode);
			fprintf(stderr, "ERROR: %s: cannot create opus encoder: %s\n", infn, opus_strerror(err));
			failed = true; goto end;
		}

		// Automatic bitrate calculation for "good quality". This is the same
		// algorithm libopus selects when setting OPUS_AUTO bitrate.
		int bitrate_bps = 60*FRAMES_PER_SECOND + flag_wav_resample * wav->channels;
		if (flag_verbose)
			fprintf(stderr, "  opus bitrate: %d bps\n", bitrate_bps);

		// Write extended header
		w32(out, frame_size);
		uint32_t max_cmp_size_pos = w32_placeholder(out);  // max compressed frame size
		w32(out, bitrate_bps);
		w32(out, 0);				// custom mode pointer at runtime
		placeholder_set(out, "samples");

		// Ask the size of the decoder state to the opus library. This is computed on x86-64
		// so it could be larger than on the N64, but it's a good approximation.
		// Add 16 because OpusDecoder has a 16-byte internal alingment, so we add
		// some margin. The value is asserted at runtime anyway.
		placeholder_set_offset(out, 16+opus_custom_decoder_get_size(custom_mode, wav->channels), "state_size");

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
		int newcnt = (wav->cnt + frame_size - 1) / frame_size * frame_size;
		wav->samples = realloc(wav->samples, newcnt * wav->channels * sizeof(int16_t));
		memset(wav->samples + wav->cnt, 0, (newcnt - wav->cnt) * wav->channels * sizeof(int16_t));
		
		int max_nb = 0;
		int out_max_size = bitrate_bps/8; // overestimation
		uint8_t *out_buffer = malloc(out_max_size);
		for (int i=0; i<newcnt; i+=frame_size) {
			int nb = opus_custom_encode(enc, wav->samples + i*wav->channels, frame_size, out_buffer, out_max_size);
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
			fseek(out, 20, SEEK_SET);
			int start_offset = 0;
			start_offset |= fgetc(out) << 24;
			start_offset |= fgetc(out) << 16;
			start_offset |= fgetc(out) << 8;
			start_offset |= fgetc(out);
			fseek(out, start_offset, SEEK_SET);
			OpusCustomDecoder *dec = opus_custom_decoder_create(
					custom_mode, wav->channels, &err);
			if (err != OPUS_OK) {
				opus_custom_mode_destroy(custom_mode);
				fprintf(stderr, "ERROR: %s: cannot create opus decoder: %s\n", infn, opus_strerror(err));
				free(wav2fn);
				failed = true; goto end;
			}

			// Decode the whole file to check for errors
			int16_t *out_samples = malloc(newcnt * wav->channels * sizeof(int16_t));
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

				int ret = opus_custom_decode(dec, in_samples, nb, out_samples + outcnt*wav->channels, frame_size);
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
					.channels = wav->channels,
					.sampleRate = wav->sampleRate,
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
	return !failed;
}



int wav_convert(const char *infn, const char *outfn) {
	if (flag_verbose) {
		const char *compr[4] = { "raw", "vadpcm", "raw", "opus" };
		fprintf(stderr, "Converting: %s => %s (%s)\n", infn, outfn, compr[flag_wav_compress]);
	}

	bool failed = false;
	wav_data_t wav = {0};

	// Read the input file
	bool loaded;
	if (strcasestr(infn, ".mp3"))
		loaded = read_mp3(infn, &wav);
	else
		loaded = read_wav(infn, &wav);
	if (!loaded) {
		return 1;
	}

	if (flag_verbose)
		fprintf(stderr, "  input: %d bits, %d Hz, %d channels\n", wav.bitsPerSample, wav.sampleRate, wav.channels);

	// Apply command line flags if not provided by WAV itself
	if (flag_wav_looping_offset > 0 && wav.loopOffset == 0)
		wav.loopOffset = flag_wav_looping_offset;
	if (flag_wav_looping && !wav.looping)
		wav.looping = true;

	// Check if the user requested conversion to mono
	if (flag_wav_mono && wav.channels == 2) {
		if (flag_verbose)
			fprintf(stderr, "  converting to mono\n");

		// Allocate a new buffer for the mono samples
		int16_t *mono_samples = malloc(wav.cnt * sizeof(int16_t));

		// Convert to mono
		int16_t *sptr = wav.samples;
		int16_t *dptr = mono_samples;
		for (int i=0;i<wav.cnt;i++) {
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

	int wavResampleTo = flag_wav_resample;

	// When compressing with opus, we need to resample to 32 Khz. Whatever value
	// was selected by the user, we force it to 32 Khz.
	if (flag_wav_compress == 3) {
		if (flag_verbose)
			fprintf(stderr, "  opus only supports %d kHz, forcing resample\n", OPUS_SAMPLE_RATE/1000);

		// For Opus, input files must always be 48 Khz (OPUS_SAMPLE_RATE).
		// We will check the real flag_wav_resample later as a way to tune the
		// bitrate.
		wavResampleTo = OPUS_SAMPLE_RATE;
		if (!flag_wav_resample)
			flag_wav_resample = wav.sampleRate;
	}

	// Do sample rate conversion if requested
	if (wavResampleTo && wav.sampleRate != wavResampleTo) {
		if (flag_verbose)
			fprintf(stderr, "  resampling to %d Hz\n", wavResampleTo);

		// Convert input samples to float
		float *fsamples_in = malloc(wav.cnt * wav.channels * sizeof(float));
		src_short_to_float_array(wav.samples, fsamples_in, wav.cnt * wav.channels);

		// Allocate output buffer, estimating the size based on the ratio.
		// We add some margin because we are not sure of rounding errors.
		int newcnt = wav.cnt * wavResampleTo / wav.sampleRate + 16;
		float *fsamples_out = malloc(newcnt * wav.channels * sizeof(float));

		// Do the conversion
		SRC_DATA data = {
			.data_in = fsamples_in,
			.input_frames = wav.cnt,
			.data_out = fsamples_out,
			.output_frames = newcnt,
			.src_ratio = (double)wavResampleTo / wav.sampleRate,
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
		wav.cnt = data.output_frames_gen;
		wav.samples = realloc(wav.samples, wav.cnt * wav.channels * sizeof(int16_t));
		src_float_to_short_array(fsamples_out, wav.samples, wav.cnt * wav.channels);

		free(fsamples_in);
		free(fsamples_out);

		// Update wav.sampleRate as it will be used later
		wav.sampleRate = wavResampleTo;

		// Update also the loop offset to the new sample rate
		wav.loopOffset = wav.loopOffset * wavResampleTo / wav.sampleRate;
	}

	// Keep 8 bits file if original is 8 bit, otherwise expand to 16 bit.
	// Compressed waveforms always expand to 16 (both vadpcm and opus only supports 16 bits)
	int nbits = wav.bitsPerSample == 8 ? 8 : 16;
	if (flag_wav_compress != 0)
		nbits = 16;

	int loop_len = wav.looping ? wav.cnt - wav.loopOffset : 0;
	if (loop_len < 0) {
		fprintf(stderr, "WARNING: %s: invalid looping offset: %d (size: %d)\n", infn, wav.loopOffset, wav.cnt);
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

	failed = !wav64_write(infn, outfn, out, &wav, loop_len, nbits, flag_wav_compress);

	fclose(out);
	free(wav.samples);
	if (failed) {
		remove(outfn);
		return 1;
	}
	return 0;
}
