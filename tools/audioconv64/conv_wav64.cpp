#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <vector>
#include <algorithm>
#include "../../src/audio/wav64_internal.h"
#include "../common/binout.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "libvadpcm.h"
#include "libsamplerate.h"
#include "libopus.h"

#include "huff_vadpcm.c"
#include "conv_common.h"

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;
int flag_wav_compress = 1;
int flag_wav_compress_vadpcm_huffman = -1;
int flag_wav_compress_vadpcm_bits = 4;
int flag_wav_resample = 0;
bool flag_wav_mono = false;
const int OPUS_SAMPLE_RATE = 48000;

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
	int16_t* samples = (int16_t*)malloc(wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
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
				// If we have multiple loops, we just take the first one.
				drwav_smpl_loop* loop = &smpl->pLoops[0];
				// NOTE: the offset appears to be in samples, not bytes.
				// See also https://github.com/mackron/dr_libs/issues/267
				out->looping = true;
				out->loopOffset = loop->firstSampleByteOffset;
				if (out->cnt > loop->lastSampleByteOffset+1)
					out->cnt = loop->lastSampleByteOffset+1;

				switch (loop->type) {
				case 0: // standard forward loop
					if (flag_verbose)
						fprintf(stderr, "  found forward loop [start=%d end=%d cnt=%d]\n", loop->firstSampleByteOffset,
							loop->lastSampleByteOffset, out->cnt);
					break;
				case 1: { // ping-pong loop
					if (flag_verbose)
						fprintf(stderr, "  found ping-pong loop [start=%d end=%d cnt=%d]\n", loop->firstSampleByteOffset,
							loop->lastSampleByteOffset, out->cnt);
					// Unroll the ping-pong loop in the buffer.
					int last_offset = loop->lastSampleByteOffset / (wav.bitsPerSample / 8);
					int first_offset = loop->firstSampleByteOffset / (wav.bitsPerSample / 8);
					int loop_len = last_offset - first_offset + 1;
					int16_t* new_samples = (int16_t*)malloc((out->cnt + loop_len) * out->channels * sizeof(int16_t));
					memcpy(new_samples, samples, out->cnt * out->channels * sizeof(int16_t));
					for (int i=0; i<loop_len; i++) {
						for (int j=0; j<wav.channels; j++) {
							new_samples[out->cnt * wav.channels + i * wav.channels + j] = samples[(last_offset - i) * wav.channels + j];
						}
					}
					free(samples);
					out->samples = new_samples;
					out->cnt += loop_len;
					out->loopOffset = out->cnt - loop_len;
				}	break;
				default:
					fprintf(stderr, "WARNING: %s: loop type %d not supported\n", infn, loop->type);
					break;
				}
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
	int16_t* samples = (int16_t*)malloc(nframes * mp3.channels * sizeof(int16_t));
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

/**
 * @brief Write a WAV64 file, optionally compressing it.
 * 
 * @param infn 			Input file name (used only for diagnostics)
 * @param outfn 		Output file name (used only to create debug dump files if requested)
 * @param out 			Output file handle
 * @param wav 			Input WAV data (always pre-converted to 16 bit)
 * @param format 		Compression format. 0=none, 1=VADPCM, 3=opus.
 * @return true 		if the file was written successfully
 * @return false 		if any error occurred
 * 
 * After the call:
 *   wav->samples might have been reallocated to a different buffer, and the original one freed
 * 
 * Consider the function might have to change the wav->cnt and wav->loopOffset
 * values to make them compatible with the compression format (eg: padding, realigning).
 */
bool wav64_write(const char *infn, const char *outfn, FILE *out, wav_data_t* wav, int format)
{
	bool failed = false;
	int basepos = ftell(out);
	
	// Adjust loops for playback constraints
	int loop_len = wav->looping ? wav->cnt - wav->loopOffset : 0;
	if (loop_len < 0) {
		fprintf(stderr, "WARNING: %s: invalid looping offset: %d (size: %d)\n", infn, wav->loopOffset, wav->cnt);
		loop_len = 0;
	}

	switch (format) {
	case 0:
		if (loop_len&1 && wav->bitsPerSample ==8) {
			// Odd loop lengths are not supported for 8-bit waveforms because they would
			// change the 2-byte phase between ROM and RDRAM addresses during loop unrolling.
			// We shorten the loop by 1 sample which shouldn't matter.
			wav->loopOffset += 1;
			loop_len -= 1;
		}
		break;

	case 1: { // vadpcm 
		// We need the loop point to be aligned to the VADPCM frame size (16 samples).
		// This allows the VADPCM decoder to be simpler when looping, as it doesn't
		// have to decode and discard partial frames.
		// Moreover, we even force an alignment to *even* frames (32 samples) because
		// this gurantees the source ROM pointer is even, which means that direct DMA
		// will be performed during decoding, with no memcpy.
		// To do so, move forward the loop point until the next frame boundary,
		// and copy the skipped samples to the end of the buffer.
		enum { VADCPM_ALIGN = 32 };
		if (wav->looping && (wav->loopOffset % VADCPM_ALIGN) != 0) {
			int ncopy = VADCPM_ALIGN - (wav->loopOffset % VADCPM_ALIGN);
			
			wav->samples = (int16_t*)realloc(wav->samples, (wav->cnt + ncopy) * wav->channels * sizeof(int16_t));
			// Manually copy the samples to the end of the buffer, so that
			// we handle the case of a loop length smaller than the copy size.
			for (int i=0; i<ncopy * wav->channels; i++) {
				wav->samples[wav->cnt * wav->channels + i] = wav->samples[wav->loopOffset * wav->channels + i];
			}
			wav->cnt += ncopy;
			wav->loopOffset += ncopy;
			loop_len = wav->cnt - wav->loopOffset;
		}

		wav->bitsPerSample = 16; // VADPCM always uses 16-bit samples
	} 	break;

	case 3: // opus:
		wav->bitsPerSample = 16; // Opus always uses 16-bit samples
		break;
	}

	fwrite("WV64", 1, 4, out);
	w8(out, 4); 				 			// version
	w8(out, format);  						// format
	w8(out, wav->channels);					// channels
	w8(out, wav->bitsPerSample);			// bits
	w32(out, wav->sampleRate);				// frequency
	w32(out, wav->cnt);						// len
	w32(out, loop_len);						// loop_len
	w32_placeholderf(out, "%s/samples", outfn);		// offset where samples begin
	w32_placeholderf(out, "%s/state_size", outfn);    // size of per-mixer-channel state to allocate at runtime

	switch (format) {
	case 0: { // no compression
		// Uncompressed waveforms need to no state (0 bytes).
		placeholder_set_offset(out, 0, "%s/state_size", outfn);

		// Start of the samples data
		placeholder_set_offset(out, ftell(out)-basepos, "%s/samples", outfn);
		int16_t *sptr = wav->samples;
		for (int i=0;i<wav->cnt*wav->channels;i++) {
			// Byteswap *sptr
			int16_t v = *sptr;
			v = ((v & 0xFF00) >> 8) | ((v & 0x00FF) << 8);
			*sptr = v;
			// Write the sample as 16bit or 8bit. Since *sptr is 16-bit big-endian,
			// the 8bit representation is just the first byte (MSB). Notice
			// that WAV64 8bit is signed anyway.
			fwrite(sptr, 1, wav->bitsPerSample == 8 ? 1 : 2, out);
			sptr++;
		}
	} break;

	case 1: { // vadpcm
		// The state is 16+4+4 bytes per channel (see wav64_state_vadpcm_t), but the runtime code requires to
		// always allocate both channels even for mono files.
		placeholder_set_offset(out, 48, "%s/state_size", outfn);

		// We need cnt to be a multiple of kVADPCMFrameSampleCount (16) because
		// VADPCM are compressed using 16-sample frames.
		// In addition to that, our RSP decompressor at the moment only supports
		// multiples of 32 (for DMA alignment issues), so pad it to that.
		const int VADPCM_ALIGN = kVADPCMFrameSampleCount*2;
		int prepad_cnt = wav->cnt;
		if (wav->cnt % VADPCM_ALIGN) {
			int newcnt = (wav->cnt + VADPCM_ALIGN - 1) / VADPCM_ALIGN * VADPCM_ALIGN;
			wav->samples = (int16_t*)realloc(wav->samples, newcnt * wav->channels * sizeof(int16_t));
			memset(wav->samples + wav->cnt, 0, (newcnt - wav->cnt) * wav->channels * sizeof(int16_t));
			wav->cnt = newcnt;
		}

		enum { kPREDICTORS = 4 };

		assert(wav->cnt % kVADPCMFrameSampleCount == 0);
		int nframes = wav->cnt / kVADPCMFrameSampleCount;
		struct vadpcm_vector *codebook = (struct vadpcm_vector *)alloca(kPREDICTORS * kVADPCMEncodeOrder * wav->channels * sizeof(struct vadpcm_vector));
		struct vadpcm_params parms = { 
			.predictor_count = kPREDICTORS,
			.min_residual = -(1 << (flag_wav_compress_vadpcm_bits-1)),
			.max_residual = (1 << (flag_wav_compress_vadpcm_bits-1)) - 1
		};
		uint8_t *dest = (uint8_t*)malloc(nframes * kVADPCMFrameByteSize * wav->channels);
		
		if (flag_verbose)
			fprintf(stderr, "  compressing into VADPCM format (%d frames)\n", nframes);

		std::vector<int> skip_points = wav->skipPoints;
		if (wav->looping) skip_points.push_back(wav->loopOffset);
		std::sort(skip_points.begin(), skip_points.end());

		// Abort if skip points are out of bound
		for (int i=0; i<skip_points.size(); i++) {
			// Align skip points to *next* frame boundary
			skip_points[i] = (skip_points[i] + kVADPCMFrameSampleCount - 1) / kVADPCMFrameSampleCount * kVADPCMFrameSampleCount;

			if (skip_points[i] < 0 || skip_points[i] >= wav->cnt) {
				fprintf(stderr, "ERROR: %s: invalid skip point: %d\n", infn, skip_points[i]);
				failed = true;
				break;
			}
		}

		std::vector<int> skip_bitpos(skip_points.size(), 0);

		struct vadpcm_vector skip_state[skip_points.size()][2];
		memset(skip_state, 0, sizeof(skip_state));

		void *scratch = malloc(vadpcm_encode_scratch_size(nframes));
		int16_t *schan = (int16_t*)malloc(wav->cnt * sizeof(int16_t));
		for (int i=0; i<wav->channels; i++) {
			uint8_t *destchan = (uint8_t*)malloc(nframes * kVADPCMFrameByteSize);
			for (int j=0; j<wav->cnt; j++)
				schan[j] = wav->samples[i + j*wav->channels];
			vadpcm_encode(&parms, codebook + kPREDICTORS * kVADPCMEncodeOrder * i, nframes, destchan, schan, scratch);
			if (!skip_points.empty()) {
				for (int j=0; j<skip_points.size(); j++) {
					// Decode the whole buffer until the loop point to get the state
					// at the beginning of the loop.
					// FIXME: optimize by avoiding to redecode the whole buffer from scratch for each skip point
					vadpcm_decode(kPREDICTORS, kVADPCMEncodeOrder,
						codebook + kPREDICTORS * kVADPCMEncodeOrder * i,
						&skip_state[j][i], skip_points[j] / kVADPCMFrameSampleCount, schan, destchan);
				}
			}

			// Copy encoded samples to output buffer
			for (int j=0; j<nframes; j++)
				memcpy(dest + (i + wav->channels * j) * kVADPCMFrameByteSize, destchan + j * kVADPCMFrameByteSize, kVADPCMFrameByteSize);
			free(destchan);
		}
		free(schan);
		free(scratch);

		const int dest_size = nframes * kVADPCMFrameByteSize * wav->channels;
		const int maxcompbuflen = dest_size;
		uint8_t *compbuf = (uint8_t*)malloc(maxcompbuflen);
		uint8_t *ctxbuf = (uint8_t*)calloc(HUFF_CONTEXT_LEN, 1);
		int compbuflen = 0;
		if (flag_wav_compress_vadpcm_huffman) {
			compbuflen = huffv_compress(dest, dest_size, compbuf, maxcompbuflen, ctxbuf, HUFF_CONTEXT_LEN);

			if (flag_verbose)
				fprintf(stderr, "  huffman compressed %d bytes into %d bytes (ratio: %.1f%%)\n",
					nframes * kVADPCMFrameByteSize * wav->channels, compbuflen,
					100.0f * compbuflen / (nframes * kVADPCMFrameByteSize * wav->channels));

			// Try decompressing now, just to double check there are no bugs
			std::vector<uint8_t> scratch(dest_size);
			HuffLookup tbl[HUFF_CONTEXTS];
			huffv_decompress_init(ctxbuf, HUFF_CONTEXT_LEN, tbl);
			int bitpos = huffv_decompress(compbuf, compbuflen, tbl, &scratch[0], dest_size);
			assert((bitpos+7)/8 == compbuflen);
			assert(memcmp(&scratch[0], dest, dest_size) == 0);

			// Compute bit offset for each skip point
			for (int i=0; i<skip_points.size(); i++) {
				skip_bitpos[i] = huffv_decompress(compbuf, compbuflen, tbl,
					&scratch[0], skip_points[i] / 16 * 9 * wav->channels);
			}
		}

		uint8_t flags = 0;
		if (flag_wav_compress_vadpcm_huffman) flags |= (1<<0);

		const int CODEBOOK_SIZE = kPREDICTORS * kVADPCMEncodeOrder * wav->channels;
		struct vadpcm_vector state = {0};
		w8(out, kPREDICTORS);
		w8(out, kVADPCMEncodeOrder);
		w8(out, flags);
		w8(out, skip_points.size());
		w32(out, 0); // huff_tbl_ptr
		w32(out, skip_points.size() > 0 ? CODEBOOK_SIZE*16 : 0); // skip_points_ptr
		fwrite(ctxbuf, 1, HUFF_CONTEXT_LEN, out);					 // Huffman context
		w32(out, 0); // padding
		for (int i=0; i<CODEBOOK_SIZE; i++)    // codebook
			for (int j=0; j<8; j++)
				w16(out, codebook[i].v[j]);
		// Write the skip points
		for (int i=0; i<skip_points.size(); i++) {
			for (int k=0;k<2;k++) // always serialize two channels
				for (int j=0; j<8; j++)
					w16(out, skip_state[i][k].v[j]);
			w32(out, skip_bitpos[i]);
			w32(out, skip_points[i]);
		}

		// Start of samples data
		placeholder_set_offset(out, ftell(out)-basepos, "%s/samples", outfn);
		if (flag_wav_compress_vadpcm_huffman)
			fwrite(compbuf, 1, compbuflen, out);
		else
			fwrite(dest, 1, nframes * kVADPCMFrameByteSize * wav->channels, out);

		if (flag_debug) {
			char* wav2fn = changeext(outfn, ".vadpcm.wav");
			if (flag_verbose)
				fprintf(stderr, "  writing uncompressed file %s\n", wav2fn);
			
			int16_t *out_samples = (int16_t *)malloc(wav->cnt * wav->channels * sizeof(int16_t));
			int16_t *out_channel = (int16_t *)malloc(wav->cnt * sizeof(int16_t));
			for (int i=0;i<wav->channels;i++) {		
				uint8_t *in_channel = (uint8_t*)malloc(nframes * kVADPCMFrameByteSize);
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
				drwav_write_pcm_frames(&wav2, prepad_cnt, out_samples);
				drwav_uninit(&wav2);
			}

			free(out_samples);
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
		placeholder_set_offset(out, ftell(out)-basepos, "%s/samples", outfn);

		// Ask the size of the decoder state to the opus library. This is computed on x86-64
		// so it could be larger than on the N64, but it's a good approximation.
		// Add 16 because OpusDecoder has a 16-byte internal alingment, so we add
		// some margin. The value is asserted at runtime anyway.
		placeholder_set_offset(out, 16+opus_custom_decoder_get_size(custom_mode, wav->channels), "%s/state_size", outfn);

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
		wav->samples = (int16_t*)realloc(wav->samples, newcnt * wav->channels * sizeof(int16_t));
		memset(wav->samples + wav->cnt, 0, (newcnt - wav->cnt) * wav->channels * sizeof(int16_t));
		
		int max_nb = 0;
		int out_max_size = bitrate_bps/8; // overestimation
		uint8_t *out_buffer = (uint8_t*)malloc(out_max_size);
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
			int16_t *out_samples = (int16_t*)malloc(newcnt * wav->channels * sizeof(int16_t));
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

	// For wav64 conversion, activate huffman by default
	if (flag_wav_compress_vadpcm_huffman < 0) 
		flag_wav_compress_vadpcm_huffman = 1;

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

	int uncompressedSize;
	if (flag_verbose)
		fprintf(stderr, "  input: %d bits, %d Hz, %d channels\n", wav.bitsPerSample, wav.sampleRate, wav.channels);
	uncompressedSize = wav.cnt * wav.channels * wav.bitsPerSample / 8;

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
		int16_t *mono_samples = (int16_t*)malloc(wav.cnt * sizeof(int16_t));

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
		float *fsamples_in = (float*)malloc(wav.cnt * wav.channels * sizeof(float));
		src_short_to_float_array(wav.samples, fsamples_in, wav.cnt * wav.channels);

		// Allocate output buffer, estimating the size based on the ratio.
		// We add some margin because we are not sure of rounding errors.
		int newcnt = (int64_t)wav.cnt * wavResampleTo / wav.sampleRate + 16;
		float *fsamples_out = (float*)malloc(newcnt * wav.channels * sizeof(float));

		// Do the conversion
		SRC_DATA data{};
		data.data_in = fsamples_in;
		data.data_out = fsamples_out;
		data.input_frames = wav.cnt;
		data.output_frames = newcnt;
		data.src_ratio = (double)wavResampleTo / wav.sampleRate;

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
		wav.samples = (int16_t*)realloc(wav.samples, wav.cnt * wav.channels * sizeof(int16_t));
		src_float_to_short_array(fsamples_out, wav.samples, wav.cnt * wav.channels);

		free(fsamples_in);
		free(fsamples_out);

		// Update wav.sampleRate as it will be used later
		wav.sampleRate = wavResampleTo;

		// Update also the loop offset to the new sample rate
		wav.loopOffset = wav.loopOffset * wavResampleTo / wav.sampleRate;
	}

	FILE *out = fopen(outfn, "wb");
	if (!out) {
		fprintf(stderr, "ERROR: %s: cannot create file\n", outfn);
		free(wav.samples);
		return 1;
	}

	failed = !wav64_write(infn, outfn, out, &wav, flag_wav_compress);

	// Show a message with the final compression ratio between original file and output file
	if (flag_verbose)
		fprintf(stderr, "  uncompressed: %ld bytes, compressed: %ld bytes (ratio: %.1f%%)\n",
			(long)uncompressedSize, (long)ftell(out), (long)ftell(out) * 100.0 / uncompressedSize);			

	fclose(out);
	free(wav.samples);
	if (failed) {
		remove(outfn);
		return 1;
	}
	return 0;
}
