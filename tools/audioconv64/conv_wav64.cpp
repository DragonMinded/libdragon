/*
    conv_wav64: convert WAV files to WAV64 format
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <vector>
#include <array>
#include <algorithm>
#include <time.h>
#include <unordered_set>
#include "../../src/audio/wav64_internal.h"
#include "../common/binout.h"

#define DR_WAV_IMPLEMENTATION
#include "../common/dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "../common/dr_mp3.h"

#include "libvadpcm.h"
#include "libsamplerate.h"
#include "libopus.h"
#include "libulc.h"

#include "huff_vadpcm.c"
#include "conv_common.h"

bool flag_wav_looping = false;
int flag_wav_looping_offset = 0;
int flag_wav_compress = 1;
int flag_wav_compress_vadpcm_huffman = -1;
int flag_wav_compress_vadpcm_bits = 4;
enum ulc_mode_t { ULC_MODE_VBR, ULC_MODE_ABR, ULC_MODE_CBR };
ulc_mode_t flag_wav_compress_ulc_mode = ULC_MODE_VBR;
float flag_wav_compress_ulc_bitrate = 64.0f;
float flag_wav_compress_ulc_quality = 50.0f;
int flag_wav_resample = 0;
double flag_wav_seek_interval_sec = 0.0;
const char *flag_wav_seek_file = NULL;
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
				out->looping = true;
				out->loopOffset = loop->firstSampleOffset;
				if (out->cnt > loop->lastSampleOffset+1)
					out->cnt = loop->lastSampleOffset+1;

				switch (loop->type) {
				case 0: // standard forward loop
					if (flag_verbose)
						fprintf(stderr, "  found forward loop [start=%d end=%d cnt=%d]\n", loop->firstSampleOffset,
							loop->lastSampleOffset, out->cnt);
					break;
				case 1: { // ping-pong loop
					if (flag_verbose)
						fprintf(stderr, "  found ping-pong loop [start=%d end=%d cnt=%d]\n", loop->firstSampleOffset,
							loop->lastSampleOffset, out->cnt);
					// Unroll the ping-pong loop in the buffer.
					int last_offset = loop->lastSampleOffset;
					int first_offset = loop->firstSampleOffset;
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

		// If we find cue points, interpret them as "seek points" (skip points) used during compression.
		// This is more semantically correct than abusing SMPL loops for markers.
		if (wav.pMetadata[i].type == drwav_metadata_type_cue) {
			drwav_cue *cue = &wav.pMetadata[i].data.cue;
			if (cue->cuePointCount > 0 && cue->pCuePoints) {
				for (drwav_uint32 j = 0; j < cue->cuePointCount; j++) {
					drwav_uint32 off = cue->pCuePoints[j].sampleOffset;
					if (off > 0) out->skipPoints.push_back((int)off);
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

static int64_t now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void resample_progress_print(int64_t bytes_done, int64_t bytes_total, int64_t *last_print_ms, double *last_pct) {
	if (!flag_verbose) return;
	int64_t now = now_ms();
	// Print at most once every 2 seconds, but always print the final 100%.
	const bool is_done = (bytes_total > 0 && bytes_done >= bytes_total);
	if (!is_done && *last_print_ms && (now - *last_print_ms) < 1000) return;
	double pct = (bytes_total > 0) ? (double)bytes_done * 100.0 / (double)bytes_total : 0.0;
	if (pct > 100.0) pct = 100.0;
	*last_print_ms = now;
	*last_pct = pct;
	fprintf(stderr, "Resampling: %lld bytes (%.1f%%)\n", (long long)bytes_done, pct);
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

	case 2: { // ulc
		wav->bitsPerSample = 16; // ULC always uses 16-bit source samples

		// Keep the logical waveform length on an 8-byte boundary so that full-file
		// loops always append decoded blocks at an aligned RDRAM address. ULC pads
		// the encoded stream to whole 1024-frame blocks independently.
		const int frame_bytes = wav->channels * sizeof(int16_t);
		int frame_align = 1;
		while ((frame_align * frame_bytes) & 7) {
			frame_align++;
		}

		// Move an embedded loop start to the next aligned frame. Preserve the
		// complete loop by appending the skipped prefix to its end, effectively
		// rotating it just like the VADPCM alignment adjustment above.
		if (wav->looping && (wav->loopOffset % frame_align) != 0) {
			const int ncopy = frame_align - (wav->loopOffset % frame_align);
			wav->samples = (int16_t*)realloc(wav->samples, (wav->cnt + ncopy) * wav->channels * sizeof(int16_t));
			for (int i = 0; i < ncopy * wav->channels; i++) {
				wav->samples[wav->cnt * wav->channels + i] = wav->samples[wav->loopOffset * wav->channels + i];
			}
			wav->cnt += ncopy;
			wav->loopOffset += ncopy;
		}

		wav->cnt -= wav->cnt % frame_align;
		loop_len = wav->looping ? wav->cnt - wav->loopOffset : 0;
	} break;

	case 3: // opus:
		wav->bitsPerSample = 16; // Opus always uses 16-bit source samples
		break;
	}

	fwrite("WV64", 1, 4, out);
	w8(out, 9); 				 			// version
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
		// Sub-nibble residuals are packed natively in the bitstream, which
		// leaves nothing for Huffman to exploit: it works on nibbles and both
		// schemes squeeze the same redundancy.
		if (flag_wav_compress_vadpcm_bits < 4 && flag_wav_compress_vadpcm_huffman) {
			if (flag_verbose)
				fprintf(stderr, "  %d-bit residuals are packed natively: disabling huffman\n",
					flag_wav_compress_vadpcm_bits);
			flag_wav_compress_vadpcm_huffman = 0;
		}

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
		std::vector<std::array<vadpcm_vector, 2>> skip_state(skip_points.size());
		// First three samples decoded at the loop start, stored after each
		// channel's codebook so the mixer can Hermite across the loop point.
		std::array<std::array<int16_t, 3>, 2> loop_head = {};
		int loop_start_aligned = -1;
		if (wav->looping) {
			loop_start_aligned = (wav->loopOffset + kVADPCMFrameSampleCount - 1)
				/ kVADPCMFrameSampleCount * kVADPCMFrameSampleCount;
		}

		int16_t *schan = (int16_t*)malloc(wav->cnt * sizeof(int16_t));
		for (int i=0; i<wav->channels; i++) {
			uint8_t *destchan = (uint8_t*)malloc(nframes * kVADPCMFrameByteSize);
			for (int j=0; j<wav->cnt; j++)
				schan[j] = wav->samples[i + j*wav->channels];
			vadpcm_error err = vadpcm_encode(&parms, codebook + kPREDICTORS * kVADPCMEncodeOrder * i, nframes, destchan, schan, NULL);
			if (err != kVADPCMErrNone) {
				fprintf(stderr, "ERROR: %s: VADPCM encoding failed: %s\n", infn, vadpcm_error_name(err));
				failed = true;
				free(destchan);
				break;
			}
			if (!skip_points.empty()) {
				// Compute decoder states at all skip points in a single forward pass.
				// We need the state at the *beginning* of the target VADPCM frame N,
				// which corresponds to the state after decoding frames [0..N).
				struct vadpcm_vector st = {0};
				int cur_frame = 0;
				for (int j=0; j<(int)skip_points.size(); j++) {
					const int target_frame = skip_points[j] / kVADPCMFrameSampleCount;
					const int run_frames = target_frame - cur_frame;
					if (run_frames > 0) {
						vadpcm_error err = vadpcm_decode(kPREDICTORS, kVADPCMEncodeOrder,
							codebook + kPREDICTORS * kVADPCMEncodeOrder * i,
							&st, (size_t)run_frames, schan,
							destchan + (size_t)cur_frame * kVADPCMFrameByteSize);
						assert(err == kVADPCMErrNone);
					}

					skip_state[j][i] = st;
					// The loop taps are the first three samples of the frame the
					// loop resumes on: decode that one frame on a copy of the
					// state so the forward pass is not disturbed.
					if (skip_points[j] == loop_start_aligned && target_frame < nframes) {
						struct vadpcm_vector st_head = st;
						int16_t head[kVADPCMFrameSampleCount];
						vadpcm_error herr = vadpcm_decode(kPREDICTORS, kVADPCMEncodeOrder,
							codebook + kPREDICTORS * kVADPCMEncodeOrder * i,
							&st_head, 1, head,
							destchan + (size_t)target_frame * kVADPCMFrameByteSize);
						assert(herr == kVADPCMErrNone);
						loop_head[i][0] = head[0];
						loop_head[i][1] = head[1];
						loop_head[i][2] = head[2];
					}
					cur_frame = target_frame;
				}
			}

			// Block-planar layout: for each block of B frames, all L then all R
			// (mono unchanged). One ring fill maps to one file block.
			const int B = 128;
			for (int j=0; j<nframes; j++) {
				int dstj;
				if (wav->channels == 1) {
					dstj = j;
				} else {
					int block = j / B;
					int off = j % B;
					int nblocks = (nframes + B - 1) / B;
					int bs = (block == nblocks - 1) ? (nframes - block * B) : B;
					dstj = block * B * wav->channels + i * bs + off;
				}
				memcpy(dest + dstj * kVADPCMFrameByteSize, destchan + j * kVADPCMFrameByteSize, kVADPCMFrameByteSize);
			}
			free(destchan);
		}
		free(schan);
		if (failed) {
			free(dest);
			break;
		}

		if (!skip_points.empty() && flag_verbose)
			fprintf(stderr, "  state generated for %zu seek points\n", skip_points.size());

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
			std::vector<int> bitpos_stats;
			if (!skip_points.empty())
				bitpos_stats.resize(dest_size / kVADPCMFrameByteSize + 1);

			int bitpos = huffv_decompress(compbuf, compbuflen, tbl, &scratch[0], dest_size,
				bitpos_stats.empty() ? NULL : bitpos_stats.data());
			assert((bitpos+7)/8 == compbuflen);
			assert(memcmp(&scratch[0], dest, dest_size) == 0);

			// Compute bit offset for each skip point (O(1) lookup from full decode stats).
			// Bitpos is the start of channel-0's frame F in the block-planar stream,
			// so seeking can resume Huffman decoding from that point.
			const int B = 128;
			for (int i=0; i<(int)skip_points.size(); i++) {
				int F = skip_points[i] / kVADPCMFrameSampleCount;
				int idx;
				if (wav->channels == 1) {
					idx = F;
				} else {
					int block = F / B;
					int off = F % B;
					idx = block * B * wav->channels + off; // channel 0 within block
				}
				assert(idx >= 0);
				assert(idx < (int)bitpos_stats.size());
				skip_bitpos[i] = bitpos_stats[idx];
			}
		}

		uint8_t flags = 0;
		if (flag_wav_compress_vadpcm_huffman) flags |= (1<<0);
		if (wav->resident) flags |= (1<<1); // VADPCM_FLAG_RESIDENT

		// Per channel: 8 predictor vectors (128 bytes) + 3 loop taps + pad.
		const int CODEBOOK_STRIDE = kPREDICTORS * kVADPCMEncodeOrder * 16 + 8;
		const int codebook_bytes = CODEBOOK_STRIDE * wav->channels;
		struct vadpcm_vector state = {0};
		w8(out, kPREDICTORS);
		w8(out, kVADPCMEncodeOrder);
		w16(out, flags);
		w16(out, skip_points.size());
		w8(out, flag_wav_compress_vadpcm_bits);
		w8(out, wav->attack_frames);
		w32(out, 0); // huff_tbl_ptr
		w32(out, skip_points.size() > 0 ? codebook_bytes : 0); // skip_points_ptr
		w32(out, skip_points.size() > 0 ? codebook_bytes + (int)skip_points.size()*8 : 0); // skip_states_ptr
		fwrite(ctxbuf, 1, HUFF_CONTEXT_LEN, out);					 // Huffman context
		w32(out, 0); // padding
		for (int ch=0; ch<wav->channels; ch++) {
			struct vadpcm_vector *cb = codebook + kPREDICTORS * kVADPCMEncodeOrder * ch;
			for (int i=0; i<kPREDICTORS * kVADPCMEncodeOrder; i++)
				for (int j=0; j<8; j++)
					w16(out, cb[i].v[j]);
			for (int j=0; j<3; j++)
				w16(out, loop_head[ch][j]);
			w16(out, 0); // padding
		}
		// Write the skip points
		for (int i=0; i<skip_points.size(); i++) {
			w32(out, skip_points[i]);
			w32(out, skip_bitpos[i]);
		}
		for (int i=0; i<skip_points.size(); i++) {
			for (int k=0;k<wav->channels;k++) {
				for (int j=0; j<8; j++)
					w16(out, skip_state[i][k].v[j]);
			}
		}

		// Start of samples data
		placeholder_set_offset(out, ftell(out)-basepos, "%s/samples", outfn);
		if (flag_wav_compress_vadpcm_huffman) {
			fwrite(compbuf, 1, compbuflen, out);
		} else if (flag_wav_compress_vadpcm_bits < 4) {
			const int total_frames = nframes * wav->channels;
			uint8_t *packed = (uint8_t*)malloc(total_frames * vadpcm_frame_bytes(flag_wav_compress_vadpcm_bits));
			int packed_size = vadpcm_pack_frames(packed, dest, total_frames, flag_wav_compress_vadpcm_bits);
			if (flag_verbose)
				fprintf(stderr, "  packed %d bytes into %d bytes (ratio: %.1f%%)\n",
					dest_size, packed_size, 100.0f * packed_size / dest_size);
			fwrite(packed, 1, packed_size, out);
			free(packed);
		} else {
			fwrite(dest, 1, nframes * kVADPCMFrameByteSize * wav->channels, out);
		}

		if (flag_debug) {
			char* wav2fn = changeext(outfn, ".vadpcm.wav");
			if (flag_verbose)
				fprintf(stderr, "  writing uncompressed file %s\n", wav2fn);
			
			int16_t *out_samples = (int16_t *)malloc(wav->cnt * wav->channels * sizeof(int16_t));
			int16_t *out_channel = (int16_t *)malloc(wav->cnt * sizeof(int16_t));
			const int B = 128;
			for (int i=0;i<wav->channels;i++) {		
				uint8_t *in_channel = (uint8_t*)malloc(nframes * kVADPCMFrameByteSize);
				for (int j=0;j<nframes;j++) {
					int srcj;
					if (wav->channels == 1) {
						srcj = j;
					} else {
						int block = j / B;
						int off = j % B;
						int nblocks = (nframes + B - 1) / B;
						int bs = (block == nblocks - 1) ? (nframes - block * B) : B;
						srcj = block * B * wav->channels + i * bs + off;
					}
					memcpy(in_channel + j * kVADPCMFrameByteSize, dest + srcj * kVADPCMFrameByteSize, kVADPCMFrameByteSize);
				}

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

	case 2: { // ulc
		const int block_size = 1024;
		const int blocks_len = (wav->cnt + block_size - 1) / block_size + 2;
		struct ULC_EncoderState_t enc = {};
		enc.RateHz = wav->sampleRate;
		enc.nChan = wav->channels;
		enc.BlockSize = block_size;
		if (ULC_EncoderState_Init(&enc) <= 0) {
			fprintf(stderr, "ERROR: %s: cannot initialize ULC encoder\n", infn);
			failed = true;
			break;
		}

		std::vector<float> block(block_size * wav->channels, 0.0f);
		auto load_block = [&](int block_idx) {
			std::fill(block.begin(), block.end(), 0.0f);
			const int first = block_idx * block_size;
			for (int ch = 0; ch < wav->channels; ch++)
				for (int i = 0; i < block_size && first + i < wav->cnt; i++)
					block[i * wav->channels + ch] = wav->samples[(first + i) * wav->channels + ch] / 32768.0f;
		};

		float avg_complexity = 0.0f;
		if (flag_wav_compress_ulc_mode == ULC_MODE_ABR) {
			double complexity_sum = 0.0;
			for (int i = 0; i < blocks_len; i++) {
				load_block(i);
				ULC_EncodeBlock_VBR(&enc, block.data(), NULL, flag_wav_compress_ulc_quality);
				complexity_sum += enc.BlockComplexity;
			}
			avg_complexity = (float)(complexity_sum / blocks_len);
			if (avg_complexity <= 0.0f) avg_complexity = ULC_COEF_EPS;
			ULC_EncoderState_Destroy(&enc);
			enc = {};
			enc.RateHz = wav->sampleRate;
			enc.nChan = wav->channels;
			enc.BlockSize = block_size;
			if (ULC_EncoderState_Init(&enc) <= 0) {
				fprintf(stderr, "ERROR: %s: cannot initialize ULC ABR encoder\n", infn);
				failed = true;
				break;
			}
		}

		w16(out, block_size);
		w16_placeholderf(out, "%s/ulc_max_block_size", outfn);
		w32(out, blocks_len);
		w32_placeholderf(out, "%s/ulc_bitrate", outfn);
		w32_placeholderf(out, "%s/ulc_seek_table", outfn); // reserved: seek table offset

		// Collect the first-block-relative offset of every eighth block. The table
		// itself is written after the compressed stream.
		const int seek_interval_blocks = 8;
		const int seek_table_len = (blocks_len + seek_interval_blocks - 1) / seek_interval_blocks;
		const int samples_start = ftell(out);
		placeholder_set_offset(out, samples_start-basepos, "%s/samples", outfn);
		std::vector<uint32_t> seek_offsets;
		seek_offsets.reserve(seek_table_len);

		// Fixed 32-bit target state, alignment slack, two temporary blocks per
		// channel, and the persistent per-channel lap state. The two banks retain
		// coefficients for both queued preroll blocks. Normal mono transforms
		// overwrite coefficients directly in the output samplebuffer; normal
		// stereo reuses the first bank as planar mid/side staging.
		const int decoder_state_size = 24 /*sizeof(ulc_state_t)*/ + 63 +
			sizeof(int16_t) * 2 * wav->channels * block_size +
			sizeof(int16_t) * wav->channels * (block_size / 2);
		placeholder_set_offset(out, decoder_state_size, "%s/state_size", outfn);

		uint64_t total_bytes = 0;
		int max_block_size = 0;
		std::vector<std::vector<uint8_t>> debug_blocks;
		if (flag_debug)
			debug_blocks.reserve(blocks_len);
		for (int i = 0; i < blocks_len; i++) {
			if (i % seek_interval_blocks == 0)
				seek_offsets.push_back(ftell(out)-samples_start);

			load_block(i);
			int size_bits = 0;
			const void *encoded;
			switch (flag_wav_compress_ulc_mode) {
			case ULC_MODE_VBR:
				encoded = ULC_EncodeBlock_VBR(&enc, block.data(), &size_bits, flag_wav_compress_ulc_quality);
				break;
			case ULC_MODE_ABR:
				encoded = ULC_EncodeBlock_ABR(&enc, block.data(), &size_bits, flag_wav_compress_ulc_bitrate, avg_complexity);
				break;
			default:
				encoded = ULC_EncodeBlock_CBR(&enc, block.data(), &size_bits, flag_wav_compress_ulc_bitrate);
				break;
			}
			const int size_bytes = (size_bits + 7) / 8;
			fwrite(encoded, 1, size_bytes, out);
			if (flag_debug)
				debug_blocks.emplace_back((const uint8_t *)encoded, (const uint8_t *)encoded + size_bytes);
			total_bytes += size_bytes;
			max_block_size = std::max(max_block_size, size_bytes);
		}
		assert((int)seek_offsets.size() == seek_table_len);
		placeholder_set_offset(out, ftell(out)-samples_start, "%s/ulc_seek_table", outfn);
		for (uint32_t offset : seek_offsets)
			w32(out, offset);

		const int actual_bitrate = (int)llround(total_bytes * 8.0 * wav->sampleRate / ((double)blocks_len * block_size));
		placeholder_set_offset(out, max_block_size, "%s/ulc_max_block_size", outfn);
		placeholder_set_offset(out, actual_bitrate, "%s/ulc_bitrate", outfn);
		if (flag_verbose)
			fprintf(stderr, "  ULC: %d blocks, %.2f kbps (%s)\n", blocks_len, actual_bitrate / 1000.0,
				flag_wav_compress_ulc_mode == ULC_MODE_ABR ? "ABR" :
				flag_wav_compress_ulc_mode == ULC_MODE_CBR ? "CBR" : "VBR");

		if (flag_debug) {
			char* wav2fn = changeext(outfn, ".ulc.wav");
			if (flag_verbose)
				fprintf(stderr, "  writing uncompressed file %s\n", wav2fn);

			struct ULC_DecoderState_t dec = {};
			dec.nChan = wav->channels;
			dec.BlockSize = block_size;
			if (ULC_DecoderState_Init(&dec) <= 0) {
				fprintf(stderr, "ERROR: %s: cannot initialize ULC decoder\n", infn);
				free(wav2fn);
				failed = true;
			} else {
				int out_len = block_size * blocks_len;
				int out_pos = 0;
				std::vector<int16_t> out_samples(out_len * wav->channels);
				std::vector<float> decode_buffer(block_size * wav->channels);

				for (int i = 0; i < blocks_len; i++) {
					int bits = ULC_DecodeBlock(&dec, decode_buffer.data(), debug_blocks[i].data());
					if (bits <= 0) {
						fprintf(stderr, "ERROR: %s: ULC decoding failed at block %d\n", infn, i);
						failed = true;
						break;
					}

					for (int j = 0; j < block_size * wav->channels; j++) {
						float v = decode_buffer[j] * 32768.0f;
						if (v > 32767.0f) {
							v = 32767.0f;
						}
						if (v < -32768.0f) {
							v = -32768.0f;
						}
						out_samples[out_pos++] = v;
					}
				}

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
						drwav_write_pcm_frames(&wav2, out_len, out_samples.data());
						drwav_uninit(&wav2);
					}
				}

				ULC_DecoderState_Destroy(&dec);
				free(wav2fn);
			}
		}
		ULC_EncoderState_Destroy(&enc);
	} break;

	case 3: { // opus
		// Number of preroll frames to decode/discard after seeking, to allow
		// the decoder to warm up and start producing valid output.
		const uint16_t PREROLL_FRAMES = 2;

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

		// Collect seek points (cue points) from the input file. No extra CLI options.
		std::vector<int> seek_points = wav->skipPoints;
		if (wav->looping) seek_points.push_back(wav->loopOffset);
		std::sort(seek_points.begin(), seek_points.end());

		// Write extended header
		w32(out, frame_size);
		w32_placeholderf(out, "%s/max_cmp_frame_size", outfn);  // max compressed frame size
		w32(out, bitrate_bps);
		w32(out, 0);				// custom mode pointer at runtime
		w16(out, PREROLL_FRAMES);
		w16(out, (uint16_t)seek_points.size());
		w32(out, 0);                // reserved / padding

		// Write seek table with placeholders for file offsets (relative to samples start)
		std::unordered_set<int> missing_frame_offsets;
		for (size_t si = 0; si < seek_points.size(); si++) {
			const int sp = seek_points[si];
			const int frame_idx = sp / frame_size;
			const int intra_skip = sp - frame_idx * frame_size;
			int pre_idx = frame_idx - (int)PREROLL_FRAMES;
			if (pre_idx < 0) pre_idx = 0;

			w32(out, sp);
			w32_placeholderf(out, "%s/frame_offset/%d", outfn, pre_idx); // file_offset_preroll (patched later)
			w16(out, intra_skip);
			w16(out, 0); // padding
			missing_frame_offsets.insert(pre_idx);
		}

		// Start of samples data
		long samples_start = ftell(out);
		placeholder_set_offset(out, samples_start-basepos, "%s/samples", outfn);

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

		// Compress frames and write them to the output file. While doing so,
		// we patch the file offsets of the seek points to allow seeking to
		// the correct frame.
		int frame_idx = 0;
		for (int i=0; i<newcnt; i+=frame_size) {
			if (missing_frame_offsets.find(frame_idx) != missing_frame_offsets.end()) {
				uint32_t cur_off = (uint32_t)(ftell(out) - samples_start);
				placeholder_set_offset(out, cur_off, "%s/frame_offset/%d", outfn, frame_idx);
				missing_frame_offsets.erase(missing_frame_offsets.find(frame_idx));
			}

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
			frame_idx++;
		}

		// Save the maximum compressed frame size to the placeholder.
		placeholder_set_offset(out, max_nb, "%s/max_cmp_frame_size", outfn);
		
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

				std::vector<uint8_t> in_samples(nb);
				fread(&in_samples[0], 1, nb, out);
				if (nb & 1) fgetc(out); // align to 2-byte boundary

				int ret = opus_custom_decode(dec, &in_samples[0], nb, out_samples + outcnt*wav->channels, frame_size);
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
		const char *compr[5] = { "raw", "vadpcm", "raw", "opus", "ulc" };
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
	uncompressedSize = (int64_t)wav.cnt * wav.channels * wav.bitsPerSample / 8;

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

	// Load or generate seek points as requested. Do this now before optional
	// resampleing, so that frame indices are interpreted using the original
	// sample rate.
	if (flag_wav_seek_file || flag_wav_seek_interval_sec > 0.0) {
		std::vector<int> points;

		// From explicit list (file-based --wav-seek)
		if (flag_wav_seek_file) {
			points = load_seek_frames_file(flag_wav_seek_file, (double)wav.sampleRate);
		}

		// From periodic interval (seconds)
		if (flag_wav_seek_interval_sec > 0.0) {
			int step = (int)llround(flag_wav_seek_interval_sec * (double)wav.sampleRate);
			if (step <= 0) step = 1;
			for (int sp = step; sp < wav.cnt; sp += step)
				points.push_back(sp);
		}

		// Append, then normalize (remove <=0 and out-of-range, sort+unique).
		for (int sp : points) {
			if (sp > 0 && sp < wav.cnt)
				wav.skipPoints.push_back(sp);
		}
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

		// Don't use best quality for files longer than 15 seconds. It is
		// extremely slow and it's not worth the time.
		int converter = SRC_SINC_BEST_QUALITY;
		if (wav.cnt > 15 * wav.sampleRate) {
			if (flag_verbose)
				fprintf(stderr, "  using medium quality resampling for long files\n");
			converter = SRC_SINC_MEDIUM_QUALITY;
		}

		// Do the conversion in chunks so we can report progress.
		int err = 0;
		SRC_STATE *st = src_new(converter, wav.channels, &err);
		if (!st || err != 0) {
			fprintf(stderr, "ERROR: %s: resampling init failed: %s\n", infn, src_strerror(err));
			free(fsamples_in);
			free(fsamples_out);
			free(wav.samples);
			return 1;
		}

		const int64_t total_in_frames = wav.cnt;
		const int64_t total_in_bytes = (int64_t)wav.cnt * wav.channels * sizeof(int16_t);
		int64_t in_pos = 0;
		int64_t out_pos = 0;
		int64_t last_print_ms = 0;
		double last_pct = -1.0;

		while (in_pos < total_in_frames) {
			const int64_t remaining_in = total_in_frames - in_pos;
			const int64_t chunk_in = remaining_in > 4096 ? 4096 : remaining_in;

			SRC_DATA d{};
			d.data_in = fsamples_in + in_pos * wav.channels;
			d.input_frames = (long)chunk_in;
			d.data_out = fsamples_out + out_pos * wav.channels;
			d.output_frames = (long)(newcnt - out_pos);
			d.src_ratio = (double)wavResampleTo / (double)wav.sampleRate;
			d.end_of_input = (in_pos + chunk_in == total_in_frames) ? 1 : 0;

			err = src_process(st, &d);
			if (err != 0) break;

			in_pos += d.input_frames_used;
			out_pos += d.output_frames_gen;

			resample_progress_print(in_pos * wav.channels * (int64_t)sizeof(int16_t), total_in_bytes, &last_print_ms, &last_pct);
			if (d.input_frames_used == 0 && d.output_frames_gen == 0) break;
		}

		// Flush.
		for (;;) {
			SRC_DATA d{};
			d.data_in = NULL;
			d.input_frames = 0;
			d.data_out = fsamples_out + out_pos * wav.channels;
			d.output_frames = (long)(newcnt - out_pos);
			d.src_ratio = (double)wavResampleTo / (double)wav.sampleRate;
			d.end_of_input = 1;
			err = src_process(st, &d);
			if (err != 0) break;
			out_pos += d.output_frames_gen;
			if (d.output_frames_gen == 0) break;
		}

		src_delete(st);
		if (err == 0) resample_progress_print(total_in_bytes, total_in_bytes, &last_print_ms, &last_pct);

		if (err != 0) {
			fprintf(stderr, "ERROR: %s: resampling failed: %s\n", infn, src_strerror(err));
			free(fsamples_in);
			free(fsamples_out);
			free(wav.samples);
			return 1;
		}

		// Extract the number of samples generated, and convert back to 16-bit
		wav.cnt = (int)out_pos;
		wav.samples = (int16_t*)realloc(wav.samples, wav.cnt * wav.channels * sizeof(int16_t));
		src_float_to_short_array(fsamples_out, wav.samples, wav.cnt * wav.channels);

		free(fsamples_in);
		free(fsamples_out);

		// Update loop/seek points to the new sample rate
		wav.loopOffset = (int)((int64_t)wav.loopOffset * wavResampleTo / wav.sampleRate);
		for (size_t i = 0; i < wav.skipPoints.size(); i++) {
			wav.skipPoints[i] = (int)((int64_t)wav.skipPoints[i] * wavResampleTo / wav.sampleRate);
		}

		// Update wav.sampleRate as it will be used later
		wav.sampleRate = wavResampleTo;
	}

	// Normalize seek points array
	std::sort(wav.skipPoints.begin(), wav.skipPoints.end());
	wav.skipPoints.erase(std::unique(wav.skipPoints.begin(), wav.skipPoints.end()), wav.skipPoints.end());

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
