/**
 * @file wav64_opus.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief Support for opus-compressed WAV64 files
 * 
 * Opus notes
 * ----------
 * This section details how the Opus format is used in wav64. Opus is made
 * by a mix of two different coders: CELT and SILK. CELT is used for larger
 * frames and is more apt for music, while SILK is used for smaller frames
 * and is more apt for speech. Our N64 implementation only uses CELT. In 
 * fact, the whole Opus layer (which is a framing layer) is not used at all.
 * 
 * A WAV64 file compressed with Opus contains a sequence of raw CELT frames.
 * Since CELT requires framing (that is, the length of the compressed frame
 * must be known in advance), a very simple framing is used: each frame is
 * preceded by a 16-bit integer that contains the compressed length of the
 * frame itself. Moreover, frames are forced to be 2-byte aligned, so that
 * they're easier to read them via DMA.
 * 
 * At the API level, we use the opus_custom API which is a CELT-only API
 * that allows to implement custom "modes". A "mode" is the configuration
 * of the codec, in terms of sample rate and frame size. Standard CELT only
 * supports 48kHz with frames of some specific length (from 2.5ms to 60ms
 * in various steps). For N64, we want to flexibility of experimenting with
 * different sample rates and frame sizes. For instance, currently the
 * implementation defaults to 32 Khz and 20ms frames (640 samples per frame),
 * which seems a good compromise between quality and performance.
 */

#include <stdint.h>
#include <assert.h>
#include <malloc.h>
#include <stdalign.h>
#include "wav64.h"
#include "wav64internal.h"
#include "samplebuffer.h"
#include "debug.h"
#include "dragonfs.h"
#include "dma.h"
#include "n64sys.h"
#include "rspq.h"
#include "utils.h"
#include <unistd.h>

#include "libopus_internal.h"

/// @brief Wav64 Opus header extension
typedef struct {
    uint32_t frame_size;            ///< Size of an audioframe in samples
    uint32_t max_cmp_frame_size;    ///< Maximum compressed frame size in bytes
    uint32_t bitrate_bps;           ///< Bitrate in bits per second
} wav64_opus_header_ext;

/// @brief Wav64 Opus state
typedef struct {
    wav64_opus_header_ext xhead;    ///< Opus header extension
    OpusCustomMode *mode;           ///< Opus custom mode for this file
    OpusCustomDecoder *dec;         ///< Opus decoder for this file
} wav64_opus_state;

static void waveform_opus_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	wav64_opus_state *st = (wav64_opus_state*)wav->ext;

	if (seeking) {
		if (wpos == 0) {
			lseek(wav->current_fd, wav->base_offset, SEEK_SET);
			opus_custom_decoder_ctl(st->dec, OPUS_RESET_STATE);
		} else {
			assertf(0, "seeking not support in wav64 with opus compression");
		}
	}

    // Allocate stack buffer for reading compressed data. Align it to cacheline
    // to avoid any false sharing.
    uint8_t alignas(16) buf[st->xhead.max_cmp_frame_size + 1];
    int nframes = DIVIDE_CEIL(wlen, st->xhead.frame_size);

    // Make space for the decoded samples. Call samplebuffer_append once as we
    // use RSP in background, and each call to the function might trigger a
    // memmove of internal samples.
    int16_t *out = samplebuffer_append(sbuf, st->xhead.frame_size*nframes);

    for (int i=0; i<nframes; i++) {
        if (wpos >= wav->wave.len) {
            // End of file. This request can happen because of the RSP mixer overread.
            // FIXME: maybe the mixer should handle this case?
            memset(out, 0, st->xhead.frame_size * wav->wave.channels * sizeof(int16_t));
        } else {
            // Read frame size
            uint16_t nb = 0;
            read(wav->current_fd, &nb, 2);
            assertf(nb <= st->xhead.max_cmp_frame_size, "opus frame size too large: %08X (%ld)", nb, st->xhead.max_cmp_frame_size);

            unsigned long aligned_frame_size = nb; 
            if (aligned_frame_size & 1) {
                aligned_frame_size += 1;
            }

            // Read frame
            data_cache_hit_writeback_invalidate(buf, aligned_frame_size);
            read(wav->current_fd, buf, aligned_frame_size);

            // Decode frame
            int err = opus_custom_decode(st->dec, buf, nb, out, st->xhead.frame_size);
            assertf(err > 0, "opus decode error: %s", opus_strerror(err));
            assertf(err == st->xhead.frame_size, "opus wrong frame size: %d (exp: %lx)", err, st->xhead.frame_size);
        }

        out += st->xhead.frame_size * wav->wave.channels;
        wpos += st->xhead.frame_size;
        wlen -= st->xhead.frame_size;
    }

    if (wav->wave.loop_len && wpos >= wav->wave.len) {
        assert(wav->wave.loop_len == wav->wave.len);
        samplebuffer_undo(sbuf, wpos - wav->wave.len);
    }
}

void wav64_opus_init(wav64_t *wav, int fh) {
    wav64_opus_header_ext xhead;
    read(fh, &xhead, sizeof(xhead));
    debugf("opus header: frame_size=%ld, max_cmp_frame_size=%ld, bitrate_bps=%ld\n", xhead.frame_size, xhead.max_cmp_frame_size, xhead.bitrate_bps);
    debugf("frequency: %f\n", wav->wave.frequency);

    rsp_opus_init();

    int err = OPUS_OK;
    OpusCustomMode *custom_mode = opus_custom_mode_create(wav->wave.frequency, xhead.frame_size, &err);
    assertf(err == OPUS_OK, "%i", err);
    OpusCustomDecoder *dec = opus_custom_decoder_create(custom_mode, wav->wave.channels, &err);
    assert(err == OPUS_OK);

    // FIXME: try to avoid one allocation by allocating the decoder in the same malloc
    wav64_opus_state *state = malloc(sizeof(wav64_opus_state));
    state->mode = custom_mode;
    state->dec = dec;
    state->xhead = xhead;
 
    wav->ext = state;
    wav->wave.read = waveform_opus_read;
    wav->wave.ctx = wav;
}

void wav64_opus_close(wav64_t *wav) {
 	wav64_opus_state *st = (wav64_opus_state*)wav->ext;

    opus_custom_decoder_destroy(st->dec);
    opus_custom_mode_destroy(st->mode);
    free(st);
}

int wav64_opus_get_bitrate(wav64_t *wav) {
    wav64_opus_state *st = (wav64_opus_state*)wav->ext;
    return st->xhead.bitrate_bps;
}
