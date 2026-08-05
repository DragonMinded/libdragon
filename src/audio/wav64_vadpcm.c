/**
 * @file wav64_vadpcm.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include "wav64.h"
#include "wav64_internal.h"
#include "wav64_vadpcm_internal.h"
#include "n64sys.h"
#include "rspq.h"
#include "mixer.h"
#include "mixer_internal.h"
#include "samplebuffer.h"
#include "dma.h"
#include "utils.h"
#include "n64types.h"
#include "profile.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <alloca.h>


/**
 * @brief Find a VADPCM skip point index by sample offset.
 *
 * Skip points are stored sorted by sample offset (enforced by audioconv64).
 *
 * @param vhead     VADPCM header (contains skip_points)
 * @param wpos      Requested sample offset
 * @param nearest   If false, require an exact match (returns -1 if not found).
 *                  If true, return the index of the closest skip point.
 * @return          Index in vhead->skip_points, or -1 if no skip points / not found.
 */
 static int wav64_vadpcm_find_skippoint(wav64_header_vadpcm_t *vhead, int wpos, bool nearest)
 {
     if (vhead->num_skippoints <= 0 || !vhead->skip_points)
         return -1;
 
     // Find first index with offset >= wpos (lower_bound).
     int lo = 0, hi = vhead->num_skippoints; // [lo, hi)
     while (lo < hi) {
         int mid = (lo + hi) >> 1;
         int off = vhead->skip_points[mid].offset;
         if (off < wpos) lo = mid + 1;
         else hi = mid;
     }
 
     if (!nearest) {
         // Exact match required.
         if (lo < vhead->num_skippoints && vhead->skip_points[lo].offset == wpos)
             return lo;
         return -1;
     }
 
     // Nearest match (compare neighbors around insertion point).
     if (lo == 0) return 0;
     if (lo >= vhead->num_skippoints) return vhead->num_skippoints - 1;
 
     int off_hi = vhead->skip_points[lo].offset;
     int off_lo = vhead->skip_points[lo - 1].offset;
     int d_lo = wpos - off_lo;
     int d_hi = off_hi - wpos;
     return (d_hi < d_lo) ? lo : (lo - 1);
}

static void huffv_decompress(wav64_t *wav, wav64_state_vadpcm_t *vstate, uint8_t *dst, int len, uint8_t *scratch, int slen) {
	PROFILE_SCOPE(PS_VADPCM_HUFF) {
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;

    unsigned int bitpos = vstate->bitpos;
    // debugf("huffv_decompress: bitpos 0x%x.%d, seek to 0x%x\n", bitpos/8, bitpos&7, wav->st->base_offset +bitpos / 8);
    lseek(wav->st->current_fd, wav->st->base_offset + bitpos / 8, SEEK_SET);

    // Read the compressed data
    read(wav->st->current_fd, CachedAddr(scratch), slen);
    uint8_t *src = CachedAddr(scratch);

    // Decompress the data
    uint64_t buffer = 0;
    int buffer_bits = 0;

    if (bitpos & 7) {
        buffer = src[0];
        buffer_bits = 8 - (bitpos & 7);
        src++;
    }

    // The Huffman coder works on nibbles, so it only ever wraps classic 9-byte
    // frames: sub-nibble packing turns it off (see wav64_vadpcm_init).
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
    }

    vstate->bitpos = bitpos;
    assertf((void*)src <= CachedAddr(scratch) + slen, "invalid read past end: %p vs %p", src, scratch + slen);
    data_cache_hit_invalidate(CachedAddr(scratch), slen);
	}
}

/**
 * File-frame index of channel `ch`'s frame `frame` in a block-planar VADPCM
 * stream (`nframes` total frames per channel).
 */
static int vadpcm_file_index(int frame, int ch, int nframes, int channels) {
	if (channels == 1) return frame;
	int B = WAV64_VADPCM_BLOCK_FRAMES;
	int block = frame / B;
	int off = frame % B;
	int nblocks = (nframes + B - 1) / B;
	int bs = (block == nblocks - 1) ? (nframes - block * B) : B;
	return block * B * channels + ch * bs + off;
}

/**
 * @brief Fill samplebuffer(s) with plain (undecoded) VADPCM frames.
 *
 * wpos/wlen are in frames. Mono writes into `sbuf`. Stereo also writes the
 * right plane into `sbuf+1` (mixer allocates adjacent channel buffers).
 * Huffman is expanded to plain frames on the CPU.
 *
 * Stereo always fills through the end of the current file block so Huffman
 * can consume L-then-R contiguously and leave bitpos at the next block.
 */
static void waveform_vadpcm_read_compressed(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	PROFILE_SCOPE(PS_VADPCM_READ) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
	wav64_state_vadpcm_t *vstate = sbuf->state;
	assert(sbuf->state_size >= sizeof(wav64_state_vadpcm_t));
	int channels = wav->wave.channels;
	int nframes_total = WAV64_VADPCM_FILE_FRAMES(wav->wave.len);
	int fbytes = VADPCM_FRAME_BYTES(wav->st->vadpcm.bits);
	samplebuffer_t *sbuf_r = (channels == 2) ? sbuf + 1 : NULL;
	(void)ctx;

	if (seeking) {
		int sample_pos = wpos * 16;
		if (wpos == 0) {
			memset(vstate->state, 0, sizeof(vstate->state));
			vstate->bitpos = 0;
		} else {
			int idx = wav64_vadpcm_find_skippoint(vhead, sample_pos, false);
			assertf(idx >= 0, "wav64: %s: invalid VADPCM seeking point: 0x%x", wav->wave.name, sample_pos);
			vstate->bitpos = vhead->skip_points[idx].bitpos;
			memcpy(vstate->state, vhead->skip_states + idx * channels,
				sizeof(vstate->state[0]) * channels);
		}
		// The right plane has no reader of its own: it only ever receives what
		// this function appends, so its ring must follow the left one. A seek
		// that restarts the stream arrives with the left ring already flushed,
		// and the right one restarts at wpos as well. An overread past the end
		// also arrives as a seek (the decoder has to re-seed at the loop
		// point), but there the left ring keeps its live window and just
		// appends the loop-start frames after it: restarting the right ring
		// would leave the two planes at different stream positions.
		if (sbuf_r && sbuf->widx == 0) {
			samplebuffer_flush(sbuf_r);
			sbuf_r->wpos = wpos;
			sbuf_r->head = sbuf->head;
		}
	}

	if (wlen <= 0) goto done;

	while (wlen > 0) {
		int n;
		if (channels == 1) {
			n = MIN(wlen, SAMPLEBUFFER_MARGIN_UNITS);
		} else {
			// Fill through end of block (may exceed wlen; allowed by WaveformRead).
			int B = WAV64_VADPCM_BLOCK_FRAMES;
			int block_end = (wpos / B + 1) * B;
			if (block_end > nframes_total) block_end = nframes_total;
			n = block_end - wpos;
			assert(n > 0 && n <= B);
		}

		uint8_t *dest_l = samplebuffer_append(sbuf, n);
		uint8_t *dest_r = sbuf_r ? samplebuffer_append(sbuf_r, n) : NULL;

		if (vhead->flags & VADPCM_FLAG_HUFFMAN) {
			if (channels == 1) {
				int nbytes = n * fbytes;
				int scratch_size = ROUND_UP(nbytes * 2, 16);
				// data_cache_hit_invalidate requires a 16-byte aligned address;
				// alloca only guarantees the platform ABI alignment.
				uint8_t *scratch = (uint8_t *)ROUND_UP((uintptr_t)alloca(scratch_size + 15), 16);
				huffv_decompress(wav, vstate, dest_l, nbytes, scratch, scratch_size);
			} else {
				// From bitpos at L_wpos: stream is L_wpos..L_end, R_0..R_end of block.
				int B = WAV64_VADPCM_BLOCK_FRAMES;
				int block = wpos / B;
				int off = wpos % B;
				int nblocks = (nframes_total + B - 1) / B;
				int bs = (block == nblocks - 1) ? (nframes_total - block * B) : B;
				int n_l = bs - off;           // L frames still in this block
				int n_r = bs;                 // full R plane of this block
				assert(n_l == n);
				int span = n_l + n_r;
				int nbytes = span * fbytes;
				int scratch_size = ROUND_UP(nbytes * 2, 16);
				uint8_t *scratch = (uint8_t *)ROUND_UP((uintptr_t)alloca(scratch_size + 15), 16);
				uint8_t *spanbuf = (uint8_t *)ROUND_UP((uintptr_t)alloca(nbytes + 15), 16);
				huffv_decompress(wav, vstate, spanbuf, nbytes, scratch, scratch_size);
				memcpy(dest_l, spanbuf, n * fbytes);
				memcpy(dest_r, spanbuf + (n_l + off) * fbytes, n * fbytes);
			}
		} else {
			PROFILE_SCOPE(PS_VADPCM_IO) {
			for (int c = 0; c < channels; c++) {
				uint8_t *dest = (c == 0) ? dest_l : dest_r;
				samplebuffer_t *dstbuf = (c == 0) ? sbuf : sbuf_r;
				int fidx = vadpcm_file_index(wpos, c, nframes_total, channels);
				int nbytes = n * fbytes;
				// Serve the attack window from RDRAM: every note-on starts
				// here, and the PI would otherwise serialize a cold DMA per
				// channel on the same row.
				int attack_n = wav->st->attack_n;
				if (c == 0 && wav->st->attack && wpos < attack_n) {
					int ncache = MIN(n, attack_n - wpos);
					memcpy(dest, (uint8_t*)wav->st->attack + wpos * fbytes,
						ncache * fbytes);
					if (ncache == n)
						continue;
					dest += ncache * fbytes;
					fidx = vadpcm_file_index(wpos + ncache, c, nframes_total, channels);
					nbytes = (n - ncache) * fbytes;
				}
				uint32_t pi_addr = wav->st->rom_base + wav->st->base_offset + fidx * fbytes;
				if (wav->st->rom_base && !(((uint32_t)dest ^ pi_addr) & 1)) {
					// A prior chunk of this same WaveformRead may still be in
					// flight; wait before replacing the ticket.
					samplebuffer_dma_wait(dstbuf);
					dstbuf->dma_ticket = dma_read_async(dest, pi_addr, nbytes);
				} else {
					lseek(wav->st->current_fd, wav->st->base_offset + fidx * fbytes, SEEK_SET);
					int read_bytes = read(wav->st->current_fd, CachedAddr(dest), nbytes);
					assertf(nbytes == read_bytes, "invalid read past end: %d vs %d", nbytes, read_bytes);
					data_cache_hit_writeback_invalidate(CachedAddr(dest), nbytes);
				}
			}
			}
		}

		if (sbuf_r)
			sbuf_r->wnext = sbuf_r->wpos + sbuf_r->widx;

		wlen -= n;
		if (wlen < 0) wlen = 0;
		wpos += n;
	}
done: ;
	}
}

/** @brief Seek VADPCM predictor state for a resident (preloaded) waveform. */
static void waveform_vadpcm_seek_state(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	(void)ctx; (void)wlen;
	if (!seeking) return;

	wav64_t *wav = (wav64_t*)sbuf->wave;
	wav64_state_vadpcm_t *vstate = sbuf->state;
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
	int channels = wav->wave.channels;
	int sample_pos = wpos * 16;
	if (wpos == 0) {
		memset(vstate->state, 0, sizeof(vstate->state[0]) * channels);
	} else {
		int idx = wav64_vadpcm_find_skippoint(vhead, sample_pos, false);
		assertf(idx >= 0, "wav64: %s: invalid VADPCM seeking point: 0x%x", wav->wave.name, sample_pos);
		memcpy(vstate->state, vhead->skip_states + idx * channels,
			sizeof(vstate->state[0]) * channels);
	}
}

static void waveform_vadpcm_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	if (wav->wave.mem)
		waveform_vadpcm_seek_state(ctx, sbuf, wpos, wlen, seeking);
	else
		waveform_vadpcm_read_compressed(ctx, sbuf, wpos, wlen, seeking);
}

/**
 * @brief Load the whole VADPCM body into a fully-planar plain-frame buffer.
 *
 * File layout is block-planar (and possibly Huffman); `dst` receives
 * [all L frames][all R frames] for direct MIX_CHANNEL addressing.
 */
void wav64_vadpcm_preload(wav64_t *wav, void *dst)
{
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
	int channels = wav->wave.channels;
	// The file is read whole, padding included, but only the frames that carry
	// samples are laid out for the mixer.
	int file_frames = WAV64_VADPCM_FILE_FRAMES(wav->wave.len);
	int nframes = WAV64_VADPCM_FRAMES(wav->wave.len);
	int fbytes = VADPCM_FRAME_BYTES(wav->st->vadpcm.bits);
	int file_bytes = file_frames * fbytes * channels;
	uint8_t *scratch = malloc(ROUND_UP(file_bytes, 16));
	assertf(scratch, "Out of memory");

	lseek(wav->st->current_fd, wav->st->base_offset, SEEK_SET);
	if (vhead->flags & VADPCM_FLAG_HUFFMAN) {
		wav64_state_vadpcm_t st = {0};
		int scratch2_size = ROUND_UP(file_bytes * 2, 16);
		void *scratch2 = malloc(scratch2_size);
		assertf(scratch2, "Out of memory");
		huffv_decompress(wav, &st, scratch, file_bytes, scratch2, scratch2_size);
		free(scratch2);
	} else {
		int n = read(wav->st->current_fd, scratch, file_bytes);
		assertf(n == file_bytes, "wav64: short VADPCM preload read");
	}

	uint8_t *out = dst;
	for (int c = 0; c < channels; c++)
		for (int f = 0; f < nframes; f++)
			memcpy(out + (c * nframes + f) * fbytes,
				scratch + vadpcm_file_index(f, c, file_frames, channels) * fbytes, fbytes);
	free(scratch);
}

static void wav64_vadpcm_init_huffman(wav64_t *wav) {
    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
    wav64_vadpcm_huffctx_t *ctx = vhead->huff_ctx;

    vhead->huff_tbl = malloc(sizeof(wav64_vadpcm_hufftable_t) * 3);
    assertf(vhead->huff_tbl, "Out of memory");
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
    _Static_assert((sizeof(wav64_state_vadpcm_t) % 16) == 0, "wav64: invalid state size for VADPCM");
    assertf(state_size == sizeof(wav64_state_vadpcm_t), 
        "wav64: invalid state size for VADPCM: %d/%d\n", state_size, sizeof(wav64_state_vadpcm_t));

    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;

    // Decode the skip pointer table pointer. The table is stored after the codebook,
    // and the exact byte offset is stored in the pointer itself to simplify initialization.
    if (vhead->num_skippoints > 0) {
        int tbl_off = (int)vhead->skip_points;
        int state_off = (int)vhead->skip_states;
        vhead->skip_points = (void*)vhead->codebook + tbl_off;
        vhead->skip_states = (void*)vhead->codebook + state_off;
        data_cache_hit_writeback(vhead->skip_points, sizeof(wav64_vadpcm_skippoint_t) * vhead->num_skippoints);
        data_cache_hit_writeback(vhead->skip_states, sizeof(wav64_vadpcm_vector_t) * vhead->num_skippoints * wav->wave.channels);
    }

    // Init huffman
    assertf(vhead->huff_tbl == NULL, "huff_tbl must be NULL before initialization");
    if (vhead->flags & VADPCM_FLAG_HUFFMAN) {
        wav64_vadpcm_init_huffman(wav);
    }

    wav->wave.start = NULL;
    wav->wave.format = WAVEFORM_FORMAT_VADPCM;
    wav->st->vadpcm.codebook = vhead->codebook;
    wav->st->vadpcm.bits = VADPCM_RESIDUAL_BITS(vhead->residual_bits);
    assertf(wav->st->vadpcm.bits >= 2 && wav->st->vadpcm.bits <= 4,
        "wav64: %s: invalid VADPCM residual width %d", wav->wave.name, wav->st->vadpcm.bits);
    // Huffman codes nibbles, so it cannot coexist with sub-nibble packing.
    assertf(wav->st->vadpcm.bits == 4 || !(vhead->flags & VADPCM_FLAG_HUFFMAN),
        "wav64: %s: %d-bit VADPCM cannot be huffman-compressed", wav->wave.name, wav->st->vadpcm.bits);
    wav->st->vadpcm.loop_state = NULL;
    if (wav->wave.loop_len > 0 && vhead->num_skippoints > 0) {
        int loop_start = wav->wave.len - wav->wave.loop_len;
        int idx = wav64_vadpcm_find_skippoint(vhead, loop_start, false);
        if (idx >= 0)
            wav->st->vadpcm.loop_state = vhead->skip_states + idx * wav->wave.channels;
    }
    wav->wave.codec = &wav->st->vadpcm;
    wav->wave.read = waveform_vadpcm_read;
    // Plain frames stream from ROM with async PI DMA. Huffman frames must be
    // decompressed by the CPU, and any other medium is read synchronously.
    // A full preload also leaves nothing to overlap: the body lives in RDRAM.
    wav->wave.async_read = wav->st->rom_base != 0 &&
        !(vhead->flags & VADPCM_FLAG_HUFFMAN) &&
        !(wav->st->flags & WAV64_FLAG_PRELOAD);

	// Attack cache: audioconv64 sizes #attack_frames for samples whose
	// note-ons would otherwise serialize cold PI DMAs. 0 means none.
	wav->st->attack = NULL;
	wav->st->attack_n = 0;
	if (wav->wave.async_read && wav->wave.channels == 1 && vhead->attack_frames) {
		int fbytes = VADPCM_FRAME_BYTES(wav->st->vadpcm.bits);
		int n = MIN(vhead->attack_frames, WAV64_VADPCM_FRAMES(wav->wave.len));
		int nbytes = ROUND_UP(n * fbytes, 16);
		void *buf = malloc_uncached(nbytes);
		assertf(buf, "wav64: %s: attack cache alloc failed (%d frames)", wav->wave.name, n);
		uint32_t pi = wav->st->rom_base + wav->st->base_offset;
		data_cache_hit_writeback_invalidate(CachedAddr(buf), nbytes);
		dma_read(CachedAddr(buf), pi, nbytes);
		wav->st->attack = UncachedAddr(buf);
		wav->st->attack_n = n;
	}
}

void wav64_vadpcm_close(wav64_t *wav)
{
    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
    if (vhead->huff_tbl) {
        free(vhead->huff_tbl);
        vhead->huff_tbl = NULL;
    }
	if (wav->st->attack) {
		free_uncached(wav->st->attack);
		wav->st->attack = NULL;
		wav->st->attack_n = 0;
	}
}

int wav64_vadpcm_get_bitrate(wav64_t *wav)
{
    return wav->wave.frequency * wav->wave.channels *
        (VADPCM_FRAME_BYTES(wav->st->vadpcm.bits) * 8) / 16;
}

int wav64_vadpcm_adjust_seek(wav64_t *wav, int wpos)
{
    wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->st->ext;
	int idx = wav64_vadpcm_find_skippoint(vhead, wpos, true);

    // If no skip points are available, VADPCM seeking is only supported to 0.
	if (idx < 0) return 0;

	return vhead->skip_points[idx].offset;
}
