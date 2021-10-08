#include "wav64internal.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;

int wav_convert(const char *infn, const char *outfn) {
	drwav wav;
	if (!drwav_init_file(&wav, infn, NULL)) {
		// Check if it's a RIFX file. This is a big-endian variant of WAV
		// that is not supported by dr_wav.
		FILE *f = fopen(infn, "rb");
		if (f) {
			char head[4] = {0};
			fread(head, 1, 4, f);
			fclose(f);
			if (memcmp(head, "RIFX", 4) == 0) {
				fprintf(stderr, "ERROR: %s: big-endian / Motorola WAV files are not supported.\n", infn);
				fprintf(stderr, "Convert them to little-endian / Intel format and run audiocovn64 again.\n");
				fprintf(stderr, "For instance, using sox:\n");
				fprintf(stderr, "    sox -L %s intel-%s\n", infn, infn);
				return 1;
			}
		}

		fprintf(stderr, "ERROR: %s: not a valid WAV file\n", infn);
		return 1;
	}

	// Decode the samples as 16bit big-endian. This will decode everything including
	// compressed formats so that we're able to read any kind of WAV file, though
	// it will end up as an uncompressed file.
	int16_t* samples = malloc(wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
	size_t cnt = drwav_read_pcm_frames_s16be(&wav, wav.totalPCMFrameCount, samples);
	if (cnt != wav.totalPCMFrameCount) {
		fprintf(stderr, "WARNING: %s: %llu frames found, but only %zu decoded\n", infn, wav.totalPCMFrameCount, cnt);
	}

	// Keep 8 bits file if original is 8 bit, otherwise expand to 16 bit.
	int nbits = wav.bitsPerSample == 8 ? 8 : 16;

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

	wav64_header_t head;
	memset(&head, 0, sizeof(wav64_header_t));

	memcpy(head.id, "WV64", 4);
	head.version = WAV64_FILE_VERSION;
	head.format = 0;
	head.channels = wav.channels;
	head.nbits = nbits;
	head.freq = HOST_TO_BE32(wav.sampleRate);
	head.len = HOST_TO_BE32(cnt);
	head.loop_len = HOST_TO_BE32(loop_len);
	head.start_offset = HOST_TO_BE32(sizeof(wav64_header_t));

	if (flag_verbose)
		fprintf(stderr, "Converting: %s => %s\n", infn, outfn);

	FILE *out = fopen(outfn, "wb");
	if (!out) {
		fprintf(stderr, "ERROR: %s: cannot create file\n", outfn);
		free(samples);
		drwav_uninit(&wav);
		return 1;
	}

	fwrite(&head, 1, sizeof(wav64_header_t), out);

	int16_t *sptr = samples;
	for (int i=0;i<cnt*wav.channels;i++) {
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

	fclose(out);
	free(samples);
	drwav_uninit(&wav);
	return 0;
}
