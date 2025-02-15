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
#include "wav64_internal.h"
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
    OpusCustomMode *mode;           ///< Opus custom mode for this file
} wav64_opus_header_ext;

static void waveform_opus_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	wav64_opus_header_ext *ext = wav->st->ext;
    OpusCustomDecoder *dec = (OpusCustomDecoder*)wav->st->states;
    
	if (seeking) {
		if (wpos == 0) {
			lseek(wav->st->current_fd, wav->st->base_offset, SEEK_SET);
			opus_custom_decoder_ctl(dec, OPUS_RESET_STATE);
		} else {
			assertf(0, "seeking not support in wav64 with opus compression");
		}
	}

    // Allocate stack buffer for reading compressed data. Align it to cacheline
    // to avoid any false sharing.
    uint8_t alignas(16) buf[ext->max_cmp_frame_size + 1];
    int nframes = DIVIDE_CEIL(wlen, ext->frame_size);

    // Make space for the decoded samples. Call samplebuffer_append once as we
    // use RSP in background, and each call to the function might trigger a
    // memmove of internal samples.
    int16_t *out = samplebuffer_append(sbuf, ext->frame_size*nframes);

    for (int i=0; i<nframes; i++) {
        assert(wpos < wav->wave.len);

        // Read frame size
        uint16_t nb = 0;
        read(wav->st->current_fd, &nb, 2);
        assertf(nb <= ext->max_cmp_frame_size, "opus frame size too large: %08X (%ld)", nb, ext->max_cmp_frame_size);

        unsigned long aligned_frame_size = nb; 
        if (aligned_frame_size & 1) {
            aligned_frame_size += 1;
        }

        // Read frame
        data_cache_hit_writeback_invalidate(buf, aligned_frame_size);
        int size = read(wav->st->current_fd, buf, aligned_frame_size);
        assertf(size == aligned_frame_size, "opus read past end: %d", size);

        // Decode frame
        int err = opus_custom_decode(dec, buf, nb, out, ext->frame_size);
        assertf(err > 0, "opus decode error: %s", opus_strerror(err));
        assertf(err == ext->frame_size, "opus wrong frame size: %d (exp: %lx)", err, ext->frame_size);

        out += ext->frame_size * wav->wave.channels;
        wpos += ext->frame_size;
        wlen -= ext->frame_size;
    }

    if (wav->wave.loop_len && wpos >= wav->wave.len) {
        assert(wav->wave.loop_len == wav->wave.len);
        samplebuffer_undo(sbuf, wpos - wav->wave.len);
    }
}

static void waveform_opus_stop(void *ctx, samplebuffer_t *sbuf) {
	wav64_t *wav = (wav64_t*)sbuf->wave;

    // Inform wav64 that the channel has stopped
    int chidx = (int)ctx;
    assert(chidx >= 0 && chidx <= wav->st->nsimul);
    __wav64_channel_stopped(wav, chidx);
}

void wav64_opus_init(wav64_t *wav, int state_size) {
    rsp_opus_init();
    wav64_opus_header_ext *ext = wav->st->ext;

    int err = OPUS_OK;
    ext->mode = opus_custom_mode_create(wav->wave.frequency, ext->frame_size, &err);
    assertf(err == OPUS_OK, "%i", err);

    assertf(state_size >= opus_custom_decoder_get_size(ext->mode, wav->wave.channels), "wav64: opus state_size=%d calc_size=%d\n", state_size, opus_custom_decoder_get_size(ext->mode, wav->wave.channels));
    debugf("wav64: opus state_size=%d calc_size=%d\n", state_size, opus_custom_decoder_get_size(ext->mode, wav->wave.channels));

    OpusCustomDecoder *dec = (OpusCustomDecoder*)wav->st->states;
    assert(wav->st->nsimul == 1); 
    opus_custom_decoder_init(dec, ext->mode, wav->wave.channels);
    assert(err == OPUS_OK);

    wav->wave.read = waveform_opus_read;
    wav->wave.stop = waveform_opus_stop;
    wav->wave.ctx = 0;
}

void wav64_opus_close(wav64_t *wav) {
    wav64_opus_header_ext *ext = wav->st->ext;
    opus_custom_mode_destroy(ext->mode);
}

int wav64_opus_get_bitrate(wav64_t *wav) {
    wav64_opus_header_ext *ext = wav->st->ext;
    return ext->bitrate_bps;
}
