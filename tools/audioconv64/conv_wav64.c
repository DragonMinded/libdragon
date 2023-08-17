#include "wav64internal.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "vadpcm/vadpcm.h"
#include "vadpcm/encode.c"
#include "vadpcm/error.c"

#include "../common/binout.h"

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;
int flag_wav_compress = 0;

int wav_convert(const char *infn, const char *outfn) {
	drwav wav;
	if (!drwav_init_file(&wav, infn, NULL)) {
		fprintf(stderr, "ERROR: %s: not a valid WAV/RIFF/AIFF file\n", infn);
		return 1;
	}

	if (flag_verbose)
		fprintf(stderr, "Converting: %s => %s (%d bits, %d Hz, %d channels, %s)\n", infn, outfn,
			wav.bitsPerSample, wav.sampleRate, wav.channels, flag_wav_compress ? "vadpcm" : "raw");

	// Decode the samples as 16bit big-endian. This will decode everything including
	// compressed formats so that we're able to read any kind of WAV file, though
	// it will end up as an uncompressed file.
	int16_t* samples = malloc(wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
	size_t cnt = drwav_read_pcm_frames_s16le(&wav, wav.totalPCMFrameCount, samples);
	if (cnt != wav.totalPCMFrameCount) {
		fprintf(stderr, "WARNING: %s: %llu frames found, but only %zu decoded\n", infn, wav.totalPCMFrameCount, cnt);
	}

	// Keep 8 bits file if original is 8 bit, otherwise expand to 16 bit.
	// Compressed waveforms always expand to 16
	int nbits = wav.bitsPerSample == 8 ? 8 : 16;
	if (flag_wav_compress == 1)
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
		free(samples);
		drwav_uninit(&wav);
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
		int16_t *sptr = samples;
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
				int16_t *sptr = samples + idx*wav.channels;
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
			samples = realloc(samples, newcnt * wav.channels * sizeof(int16_t));
			memset(samples + cnt, 0, (newcnt - cnt) * wav.channels * sizeof(int16_t));
			cnt = newcnt;
		}

		enum { kPREDICTORS = 4 };

		assert(cnt % kVADPCMFrameSampleCount == 0);
		int nframes = cnt / kVADPCMFrameSampleCount;
		void *scratch = malloc(vadpcm_encode_scratch_size(nframes));
		struct vadpcm_vector *codebook = alloca(kPREDICTORS * kVADPCMEncodeOrder * wav.channels * sizeof(struct vadpcm_vector));
		struct vadpcm_params parms = { .predictor_count = kPREDICTORS };
		void *dest = malloc(nframes * kVADPCMFrameByteSize * wav.channels);
		
		int16_t *schan = malloc(cnt * sizeof(int16_t));
		uint8_t *destchan = dest;
		for (int i=0; i<wav.channels; i++) {
			for (int j=0; j<cnt; j++)
				schan[j] = samples[i + j*wav.channels];
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
	free(samples);
	drwav_uninit(&wav);
	return 0;
}
