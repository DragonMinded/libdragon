#include "wav64.h"
#include "wav64_internal.h"
#include "wav64_vadpcm_internal.h"
#include "n64sys.h"
#include "rspq.h"
#include "mixer.h"
#include "mixer_internal.h"
#include "samplebuffer.h"
#include "utils.h"
#include "n64types.h"
#include <unistd.h>
#include <limits.h>
#include <string.h>


/** @brief Set to 1 to use the reference C decode for VADPCM */
#define VADPCM_REFERENCE_DECODER     0

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

// Copy the VADPCM state. If src is NULL, the state is cleared.
// This is basically a memcpy but performed by RSP, so it's in-order with the
// other RSP operations.
static inline void rsp_vadpcm_copystate(wav64_vadpcm_vector_t *dst, wav64_vadpcm_vector_t *src)
{
    rspq_write(__mixer_overlay_id, 0x2,
        PhysicalAddr(dst),
        PhysicalAddr(src));
}

#endif /* VADPCM_REFERENCE_DECODER */

__attribute__((noinline))
static void huffv_decompress(int nframe, wav64_t *wav, wav64_state_vadpcm_t *vstate, uint8_t *dst, int len, uint8_t *scratch, int slen) {
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;

    unsigned int bitpos = vstate->bitpos;
    // debugf("huffv_decompress: bitpos 0x%x.%d, seek to 0x%x\n", bitpos/8, bitpos&7, wav->st->base_offset +bitpos / 8);
    lseek(wav->st->current_fd, wav->st->base_offset + bitpos / 8, SEEK_SET);

    // Read the compressed data
    read(wav->st->current_fd, CachedAddr(scratch), slen);
    uint8_t *src = CachedAddr(scratch);

    uint64_t t0 = get_ticks_us();

    // Decompress the data
    uint64_t buffer = 0;
    int buffer_bits = 0;

    if (bitpos & 7) {
        buffer = src[0];
        buffer_bits = 8 - (bitpos & 7);
        src++;
    }

    assert(len % 9 == 0);
    for (int i = 0; i < len; i += 9) {
        wav64_vadpcm_hufftable_t *tbl = vhead->huff_tbl;

        for (int j=0; j<9; j++) {
            while (buffer_bits < 32) {
                buffer <<= 32;
                buffer |= *(u_uint32_t*)src;
                src += 4;
                assertf(src < scratch + slen, "invalid read past end: %p vs %p", src, scratch + slen);
                buffer_bits += 32;
            }
            
            uint8_t code1 = tbl->codes[(buffer >> (buffer_bits - 8)) & 0xFF];
            int len1 = code1 & 0xF;
            int val1 = code1 >> 4;
            assert(len1 <= 8);
            buffer_bits -= len1;
            bitpos += len1;
            if (j == 0) tbl++;

            uint8_t code2 = tbl->codes[(buffer >> (buffer_bits - 8)) & 0xFF];
            int len2 = code2 & 0xF;
            int val2 = code2 >> 4;
            assert(len2 <= 8);
            buffer_bits -= len2;
            bitpos += len2;
            if (j == 0) tbl++;
            
            *dst++ = (val1 << 4) | val2;
        }

        nframe++;
    }

    vstate->bitpos = bitpos;
    assertf((void*)src <= CachedAddr(scratch) + slen, "invalid read past end: %p vs %p", src, scratch + slen);
    data_cache_hit_invalidate(CachedAddr(scratch), slen);

    uint64_t t1 = get_ticks_us();
    // debugf("huffv_decompress: %d us / %d (%d ns/sample) (leftover: %d/%d, bitpos: 0x%x.%d)\n", 
    //     (int)(t1 - t0), len/9*16, (int)((t1 - t0) * 1000 / (len/9*16)),
    //     CachedAddr(scratch) + slen - (void*)src, slen,
    //     bitpos/8, bitpos&7);
}

static void waveform_vadpcm_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;

    // Access the per-channel state
    int chidx = (int)ctx;
    assert(chidx >= 0 && chidx <= wav->st->nsimul);
    wav64_state_vadpcm_t *vstate = &((wav64_state_vadpcm_t*)wav->st->states)[chidx];
	bool highpri = false;

	if (seeking) {
        rspq_highpri_begin();
		if (wpos == 0) {
            rsp_vadpcm_copystate(vstate->state, NULL);
			lseek(wav->st->current_fd, wav->st->base_offset, SEEK_SET);
            vstate->bitpos = 0;
		} else {
			assertf(wpos == wav->wave.len - wav->wave.loop_len,
				"wav64: seeking to %x not supported (%x %x)\n", wpos, wav->wave.len, wav->wave.loop_len);
            rsp_vadpcm_copystate(vstate->state, vhead->loop_state);
			lseek(wav->st->current_fd, (wav->wave.len - wav->wave.loop_len) / 16 * 9, SEEK_CUR);
            // vstate->bitpos = ???; FIXME: we need bitpos for the loop state
		}
        rspq_highpri_end();
	} else {
        assert((wpos % 16) == 0);

        // If not huffman compressed, seek here, once. Otherwise, the huffman
        // decompressor will seek as needed.
        if ((vhead->flags & VADPCM_FLAG_HUFFMAN) == 0)
            lseek(wav->st->current_fd, wav->st->base_offset + (wpos / 16) * 9, SEEK_SET);
    }

	// Round up wlen to 32 because our RSP decompressor only supports multiples
	// of 32 samples (2 frames) because of DMA alignment issues. audioconv64
	// makes sure files are padded to that length, so this is valid also at the
	// end of the file.
	wlen = ROUND_UP(wlen, 32);
	if (wlen == 0) return;

	// Maximum number of VADPCM frames that can be decompressed in a single
	// RSP call. Keep this in sync with rsp_mixer.S.
	enum { MAX_VADPCM_FRAMES = 94 };

	while (wlen > 0) {
		// Calculate number of frames to decompress in this iteration
		int max_vadpcm_frames = (wav->wave.channels == 1) ? MAX_VADPCM_FRAMES : MAX_VADPCM_FRAMES / 2;
		int nframes = MIN(wlen / 16, max_vadpcm_frames);

		// Acquire destination buffer from the sample buffer
		int16_t *dest = (int16_t*)samplebuffer_append(sbuf, nframes*16);

		// Calculate source pointer at the end of the destination buffer.
		// VADPCM decoding can be safely made in-place, so no auxillary buffer
		// is necessary.
		int src_bytes = 9 * nframes * wav->wave.channels;
		void *src = (void*)dest + ((nframes*16) << SAMPLES_BPS_SHIFT(sbuf)) - src_bytes;

		// Fetch compressed data
        if (vhead->flags & VADPCM_FLAG_HUFFMAN) {
            void *scratch = dest;
            int scratch_size = ROUND_UP(((void*)src - (void*)dest) / 2, 16);
            if ((uint32_t)scratch & 15)
                scratch += 16 - ((uint32_t)scratch & 15);

            // debugf("huffv_decompress: scratch:%p-%p src:%p-%p nframes:%d-%d\n", 
            //     scratch, scratch + scratch_size, src, src + src_bytes, wpos/16, wpos/16 + nframes -1);
            huffv_decompress(wpos/16, wav, vstate, src, src_bytes, scratch, scratch_size);
        } else {
            // FIXME: remove CachedAddr() when read() supports uncached addresses
            int read_bytes = read(wav->st->current_fd, CachedAddr(src), src_bytes);
            assertf(src_bytes == read_bytes, "invalid read past end: %d vs %d", src_bytes, read_bytes);
        }

		#if VADPCM_REFERENCE_DECODER
		if (wav->wave.channels == 1) {
			vadpcm_error err = vadpcm_decode(
				vhead->npredictors, vhead->order, vhead->codebook, vstate->state,
				nframes, dest, src);
			assertf(err == 0, "VADPCM decoding error: %d\n", err);
		} else {
			assert(wav->wave.channels == 2);
			int16_t uncomp[2][16];
			int16_t *dst = dest;

			for (int i=0; i<nframes; i++) {
				for (int j=0; j<2; j++) {
					vadpcm_error err = vadpcm_decode(
						vhead->npredictors, vhead->order, vhead->codebook + 8*j, &vstate->state[j],
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
		rsp_vadpcm_decompress(src, dest, wav->wave.channels==2, nframes, vstate->state, vhead->codebook);
		#endif

		wlen -= 16*nframes;
		wpos += 16*nframes;
	}

	if (highpri)
		rspq_highpri_end();

    if (wav->wave.loop_len && wpos >= wav->wave.len) {
        assert(wav->wave.loop_len == wav->wave.len);
        samplebuffer_undo(sbuf, wpos - wav->wave.len);
    }
}

static void waveform_vadpcm_stop(void *ctx, samplebuffer_t *sbuf) {
	wav64_t *wav = (wav64_t*)sbuf->wave;

    // Inform wav64 that the channel has stopped
    int chidx = (int)ctx;
    assert(chidx >= 0 && chidx <= wav->st->nsimul);
    __wav64_channel_stopped(wav, chidx);
}

static void wav64_vadpcm_init_huffman(wav64_t *wav) {
    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
    wav64_vadpcm_huffctx_t *ctx = vhead->huff_ctx;

    vhead->huff_tbl = malloc(sizeof(wav64_vadpcm_hufftable_t) * 3);
    memset(vhead->huff_tbl, 0, sizeof(wav64_vadpcm_hufftable_t) * 3);

    // Compute huffman tables
    for (int i = 0; i < 3; i++) {
        for (int j=0; j<16; j++) {
            int len = ctx[i].lengths[j/2] >> (4*(~j&1)) & 0xf;
            if (len == 0xF) continue;
            assert(len <= 8);
            assert((ctx[i].values[j] >> len) == 0);

            int shift = 8 - len;
            int code = ctx[i].values[j] << shift;
            uint8_t value = (j << 4) | len;
            for (int k=0; k<(1<<shift); k++) {
                assert(vhead->huff_tbl[i].codes[code+k] == 0);
                vhead->huff_tbl[i].codes[code+k] = value;
            }
        }

        for (int j=0; j<256; j++) {
            assert(vhead->huff_tbl[i].codes[j] != 0);
        }
    }
}

void wav64_vadpcm_init(wav64_t *wav, int state_size)
{
    assertf(state_size == sizeof(wav64_state_vadpcm_t), 
        "wav64: invalid state size for VADPCM: %d/%d\n", state_size, sizeof(wav64_state_vadpcm_t));

    // Set wave callback functions
    wav->wave.read = waveform_vadpcm_read;
    wav->wave.stop = waveform_vadpcm_stop;

    // Flush cached state; it will be manipulated by RSP only
    wav64_state_vadpcm_t *vstate = (wav64_state_vadpcm_t*)wav->st->states;
    data_cache_hit_writeback_invalidate(vstate, wav->st->nsimul * sizeof(wav64_state_vadpcm_t));

    // This should never happen as audioconv64 handles this.
    assertf(wav->wave.loop_len == 0 || wav->wave.loop_len % 16 == 0, 
        "wav64: invalid loop length for VADPCM: %d\n", wav->wave.loop_len);

    // Init huffman
    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
    if (vhead->flags & VADPCM_FLAG_HUFFMAN) {
        wav64_vadpcm_init_huffman(wav);
    }
}

void wav64_vadpcm_close(wav64_t *wav)
{
    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
    if (vhead->huff_tbl) {
        free(vhead->huff_tbl);
        vhead->huff_tbl = NULL;
    }
}

int wav64_vadpcm_get_bitrate(wav64_t *wav)
{
    return wav->wave.frequency * wav->wave.channels * 72 / 16;
}


