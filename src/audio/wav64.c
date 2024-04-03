/**
 * @file wav64.c
 * @brief Support for WAV64 audio files
 * @ingroup mixer
 */

#include "wav64.h"
#include "wav64internal.h"
#include "wav64_opus_internal.h"
#include "mixer.h"
#include "mixer_internal.h"
#include "dragonfs.h"
#include "n64sys.h"
#include "dma.h"
#include "samplebuffer.h"
#include "debug.h"
#include "utils.h"
#include "rspq.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdalign.h>

/** @brief Set to 1 to use the reference C decode for VADPCM */
#define VADPCM_REFERENCE_DECODER     0

/** ID of a standard WAV file */
#define WAV_RIFF_ID   "RIFF"
/** ID of a WAVX file (big-endian WAV) */
#define WAV_RIFX_ID   "RIFX"

/** @brief Profile of DMA usage by WAV64, used for debugging purposes. */
int64_t __wav64_profile_dma = 0;

#if VADPCM_REFERENCE_DECODER
/** @brief VADPCM decoding errors */
typedef enum {
    // No error (success). Equal to 0.
    kVADPCMErrNone,

    // Invalid data.
    kVADPCMErrInvalidData,

    // Predictor order is too large.
    kVADPCMErrLargeOrder,

    // Predictor count is too large.
    kVADPCMErrLargePredictorCount,

    // Data uses an unsupported / unknown version of VADPCM.
    kVADPCMErrUnknownVersion,

    // Invalid encoding parameters.
    kVADPCMErrInvalidParams,
} vadpcm_error;

// Extend the sign bit of a 4-bit integer.
static int vadpcm_ext4(int x) {
    return x > 7 ? x - 16 : x;
}

// Clamp an integer to a 16-bit range.
static int vadpcm_clamp16(int x) {
    if (x < -0x8000 || 0x7fff < x) {
        return (x >> (sizeof(int) * CHAR_BIT - 1)) ^ 0x7fff;
    }
    return x;
}

__attribute__((unused))
static vadpcm_error vadpcm_decode(int predictor_count, int order,
                           const wav64_vadpcm_vector_t *restrict codebook,
                           wav64_vadpcm_vector_t *restrict state,
                           size_t frame_count, int16_t *restrict dest,
                           const void *restrict src) {
    const uint8_t *sptr = src;
    for (size_t frame = 0; frame < frame_count; frame++) {
        const uint8_t *fin = sptr + 9 * frame;

        // Control byte: scaling & predictor index.
        int control = fin[0];
        int scaling = control >> 4;
        int predictor_index = control & 15;
        if (predictor_index >= predictor_count) {
            return kVADPCMErrInvalidData;
        }
        const wav64_vadpcm_vector_t *predictor =
            codebook + order * predictor_index;

        // Decode each of the two vectors within the frame.
        for (int vector = 0; vector < 2; vector++) {
            int32_t accumulator[8];
            for (int i = 0; i < 8; i++) {
                accumulator[i] = 0;
            }

            // Accumulate the part of the predictor from the previous block.
            for (int k = 0; k < order; k++) {
                int sample = state->v[8 - order + k];
                for (int i = 0; i < 8; i++) {
                    accumulator[i] += sample * predictor[k].v[i];
                }
            }

            // Decode the ADPCM residual.
            int residuals[8];
            for (int i = 0; i < 4; i++) {
                int byte = fin[1 + 4 * vector + i];
                residuals[2 * i] = vadpcm_ext4(byte >> 4);
                residuals[2 * i + 1] = vadpcm_ext4(byte & 15);
            }

            // Accumulate the residual and predicted values.
            const wav64_vadpcm_vector_t *v = &predictor[order - 1];
            for (int k = 0; k < 8; k++) {
                int residual = residuals[k] << scaling;
                accumulator[k] += residual << 11;
                for (int i = 0; i < 7 - k; i++) {
                    accumulator[k + 1 + i] += residual * v->v[i];
                }
            }

            // Discard fractional part and clamp to 16-bit range.
            for (int i = 0; i < 8; i++) {
                int sample = vadpcm_clamp16(accumulator[i] >> 11);
                dest[16 * frame + 8 * vector + i] = sample;
                state->v[i] = sample;
            }
        }
    }
    return 0;
}
#else

static inline void rsp_vadpcm_decompress(void *input, int16_t *output, bool stereo, int nframes, 
	wav64_vadpcm_vector_t *state, wav64_vadpcm_vector_t *codebook)
{
	assert(nframes > 0 && nframes <= 256);
	rspq_write(__mixer_overlay_id, 0x1,
		PhysicalAddr(input), 
		PhysicalAddr(output) | (nframes-1) << 24,
		PhysicalAddr(state)  | (stereo ? 1 : 0) << 31,
		PhysicalAddr(codebook));
}

#endif /* VADPCM_REFERENCE_DECODER */

void raw_waveform_read(samplebuffer_t *sbuf, int base_rom_addr, int wpos, int wlen, int bps) {
	uint32_t rom_addr = base_rom_addr + (wpos << bps);
	uint8_t* ram_addr = (uint8_t*)samplebuffer_append(sbuf, wlen);
	int bytes = wlen << bps;

	uint32_t t0 = TICKS_READ();
	// Run the DMA transfer. We rely on libdragon's PI DMA function which works
	// also for misaligned addresses and odd lengths.
	// The mixer/samplebuffer guarantees that ROM/RAM addresses are always
	// on the same 2-byte phase, as the only requirement of dma_read.
	dma_read(ram_addr, rom_addr, bytes);
	__wav64_profile_dma += TICKS_READ() - t0;
}

static void waveform_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	int bps = (wav->wave.bits == 8 ? 0 : 1) + (wav->wave.channels == 2 ? 1 : 0);
	raw_waveform_read(sbuf, wav->rom_addr, wpos, wlen, bps);
}

static void waveform_vadpcm_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->ext;

	if (seeking) {
		if (wpos == 0) {
			memset(&vhead->state, 0, sizeof(vhead->state));
			vhead->current_rom_addr = wav->rom_addr;
		} else {
			assertf(wpos == wav->wave.len - wav->wave.loop_len,
				"wav64: seeking to %x not supported (%x %x)\n", wpos, wav->wave.len, wav->wave.loop_len);
			memcpy(&vhead->state, &vhead->loop_state, sizeof(vhead->state));
			vhead->current_rom_addr = wav->rom_addr + (wav->wave.len - wav->wave.loop_len) / 16 * 9;
		}
	}

	wlen = ROUND_UP(wlen, 32);
	if (wlen == 0) return;

	bool highpri = false;
	while (wlen > 0) {
		int nframes = wlen / 16;
		// Most of the code here would be ready to loop over multiple blocks of
		// 256 frames, but the problem is that we don't doublebuffer the RDRAM
		// buffers, so the RSP doesn't get to process the data in time. This
		// would require CPU-spinning here. Since it's a very rare case, just
		// block it for now.
		assert(nframes <= 256);
		nframes = MIN(nframes, 256);

		// Acquire destination buffer from the sample buffer
		int16_t *dest = (int16_t*)samplebuffer_append(sbuf, nframes*16);

		// Calculate source pointer at the end of the destination buffer.
		// VADPCM decoding can be safely made in-place, so no auxillary buffer
		// is necessary.
		int src_bytes = 9 * nframes * wav->wave.channels;
		void *src = (void*)dest + ((nframes*16) << SAMPLES_BPS_SHIFT(sbuf)) - src_bytes;

		// Fetch compressed data
		dma_read(src, vhead->current_rom_addr, src_bytes);
		vhead->current_rom_addr += src_bytes;

		#if VADPCM_REFERENCE_DECODER
		if (wav->wave.channels == 1) {
			vadpcm_error err = vadpcm_decode(
				vhead->npredictors, vhead->order, vhead->codebook, vhead->state,
				nframes, dest, src);
			assertf(err == 0, "VADPCM decoding error: %d\n", err);
		} else {
			assert(wav->wave.channels == 2);
			int16_t uncomp[2][16];
			int16_t *dst = dest;

			for (int i=0; i<nframes; i++) {
				for (int j=0; j<2; j++) {
					vadpcm_error err = vadpcm_decode(
						vhead->npredictors, vhead->order, vhead->codebook + 8*j, &vhead->state[j],
						1, uncomp[j], src);
					assertf(err == 0, "VADPCM decoding error: %d\n", err);
					src += 9;
				}
				for (int j=0; j<16; j++) {
					*dst++ = uncomp[0][j];
					*dst++ = uncomp[1][j];
				}
			}
		}
		#else
		// Switch to highpri as late as possible
		if (!highpri) {
			rspq_highpri_begin();
			highpri = true;
		}
		rsp_vadpcm_decompress(src, dest, wav->wave.channels==2, nframes, vhead->state, vhead->codebook);
		#endif

		wlen -= 16*nframes;
	}

	if (highpri)
		rspq_highpri_end();
}

void wav64_open(wav64_t *wav, const char *fn) {
	memset(wav, 0, sizeof(*wav));

	// Currently, we only support streaming WAVs from DFS (ROMs). Any other
	// filesystem is unsupported.
	// For backward compatibility, we also silently accept a non-prefixed path.
	if (strstr(fn, ":/")) {
		assertf(strncmp(fn, "rom:/", 5) == 0, "Cannot open %s: wav64 only supports files in ROM (rom:/)", fn);
		fn += 5;
	}

	int fh = dfs_open(fn);
	assertf(fh >= 0, "error opening file %s: %s (%d)\n", fn, dfs_strerror(fh), fh);

	wav64_header_t head = {0};
	dfs_read(&head, 1, sizeof(head), fh);
	if (memcmp(head.id, WAV64_ID, 4) != 0) {
		assertf(memcmp(head.id, WAV_RIFF_ID, 4) != 0 && memcmp(head.id, WAV_RIFX_ID, 4) != 0,
			"wav64 %s: use audioconv64 to convert to wav64 format", fn);
		assertf(0, "wav64 %s: invalid ID: %02x%02x%02x%02x\n",
			fn, head.id[0], head.id[1], head.id[2], head.id[3]);
	}
	assertf(head.version == WAV64_FILE_VERSION, "wav64 %s: invalid version: %02x\n",
		fn, head.version);

	wav->wave.name = fn;
	wav->wave.channels = head.channels;
	wav->wave.bits = head.nbits;
	wav->wave.frequency = head.freq;
	wav->wave.len = head.len;
	wav->wave.loop_len = head.loop_len; 
	wav->rom_addr = dfs_rom_addr(fn) + head.start_offset;
	wav->format = head.format;

	switch (head.format) {
	case WAV64_FORMAT_RAW:
		wav->wave.read = waveform_read;
		wav->wave.ctx = wav;
		break;

	case WAV64_FORMAT_VADPCM: {
		wav64_header_vadpcm_t vhead = {0};
		dfs_read(&vhead, 1, sizeof(vhead), fh);

		int codebook_size = vhead.npredictors * vhead.order * head.channels * sizeof(wav64_vadpcm_vector_t);

		void *ext = malloc_uncached(sizeof(vhead) + codebook_size);
		memcpy(ext, &vhead, sizeof(vhead));
		dfs_read(ext + sizeof(vhead), 1, codebook_size, fh);
		wav->ext = ext;
		wav->wave.read = waveform_vadpcm_read;
		wav->wave.ctx = wav;
		assertf(head.loop_len == 0 || head.loop_len % 16 == 0, 
			"wav64 %s: invalid loop length: %ld\n", fn, head.loop_len);
	}	break;

	case WAV64_FORMAT_OPUS: {
		wav64_opus_init(wav, fh);
	}	break;
	
	default:
		assertf(0, "wav64 %s: invalid format: %02x\n", fn, head.format);
	}

	dfs_close(fh);
	debugf("wav64 %s: %d-bit %.1fHz %dch %d samples (loop: %d)\n",
		fn, wav->wave.bits, wav->wave.frequency, wav->wave.channels, wav->wave.len, wav->wave.loop_len);
}

void wav64_play(wav64_t *wav, int ch)
{
	// Update the context pointer, so that we try to catch cases where the
	// wav64_t instance was moved.
	wav->wave.ctx = wav;
    mixer_ch_play(ch, &wav->wave);
}

void wav64_set_loop(wav64_t *wav, bool loop) {
	wav->wave.loop_len = loop ? wav->wave.len : 0;

	// Odd loop lengths are not supported for 8-bit waveforms because they would
	// change the 2-byte phase between ROM and RDRAM addresses during loop unrolling.
	// We shorten the loop by 1 sample which shouldn't matter.
	// Notice that audioconv64 does the same during conversion.
	if (wav->wave.bits == 8 && wav->wave.loop_len & 1)
		wav->wave.loop_len -= 1;
}

int wav64_get_bitrate(wav64_t *wav) {
	if (wav->ext) {
		switch (wav->format) {
		case WAV64_FORMAT_VADPCM:
			return wav->wave.frequency * wav->wave.channels * 72 / 16;
		case WAV64_FORMAT_OPUS:
			return wav64_opus_get_bitrate(wav);
		}
	}
	return wav->wave.frequency * wav->wave.channels * wav->wave.bits;
}

void wav64_close(wav64_t *wav)
{
	if (wav->ext) {
		switch (wav->format) {
		case WAV64_FORMAT_VADPCM:
			free_uncached(wav->ext);
			break;
		case WAV64_FORMAT_OPUS:
			wav64_opus_close(wav);
			break;
		}
		wav->ext = NULL;
	}
}