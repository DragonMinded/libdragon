/**
 * @file wav64_opus.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
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
#include <stdlib.h>
#include <stdalign.h>
#include <stdbool.h>
#include "wav64.h"
#include "wav64_internal.h"
#include "wav64_opus_internal.h"
#include "samplebuffer.h"
#include "debug.h"
#include "dragonfs.h"
#include "dma.h"
#include "n64sys.h"
#include "rspq.h"
#include "utils.h"
#include <unistd.h>

#include "libopus_internal.h"

static wav64_opus_seekpoint_t *wav64_opus_find_seekpoint(wav64_opus_header_t *ext, int wpos) {
    assert(wpos >= 0);

    int lo = 0, hi = ext->num_seekpoints; // [lo, hi)
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        uint32_t off = ext->seekpoints[mid].sample_offset;
        if (off <= (uint32_t)wpos) lo = mid + 1;
        else hi = mid;
    }

    int idx = lo - 1;
    if (idx < 0) return NULL;
    return &ext->seekpoints[idx];
}

static void waveform_opus_start(void *ctx, samplebuffer_t *sbuf) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	wav64_opus_header_t *ext = wav->st->ext;

    OpusCustomDecoder *dec = (OpusCustomDecoder*)CachedAddr(sbuf->state);
    int err = opus_custom_decoder_init(dec, ext->mode, wav->wave.channels);
    assert(err == OPUS_OK);
}

static void waveform_opus_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)sbuf->wave;
	wav64_opus_header_t *ext = wav->st->ext;
    OpusCustomDecoder *dec = (OpusCustomDecoder*)CachedAddr(sbuf->state);
    int preroll_frames = 0;
    int intra_skip = 0;

    if (seeking) {
        if (wpos == 0) {
            lseek(wav->st->current_fd, wav->st->base_offset, SEEK_SET);
        } else {
            wav64_opus_seekpoint_t *sp = wav64_opus_find_seekpoint(ext, wpos);
            assertf(sp && sp->sample_offset == (uint32_t)wpos,
                "wav64: %s: invalid Opus seeking point: %d", wav->wave.name, wpos);

            // Seek directly to the preroll start frame and reset decoder state.
            lseek(wav->st->current_fd, wav->st->base_offset + sp->file_offset_preroll, SEEK_SET);

            // Clamp preroll (in case the target frame is at the beginning of the file)
            preroll_frames = MIN(ext->preroll_frames, wpos / ext->frame_size);

            // Round intra_skip to 8 bytes, because our RSP memmove requires 8-byte alignment
            intra_skip = ROUND_DOWN(sp->intra_skip, wav->wave.channels == 2 ? 2 : 4);
        }
        opus_custom_decoder_ctl(dec, OPUS_RESET_STATE);
    }

    // Allocate stack buffer for reading compressed data. Align it to cacheline
    // to avoid any false sharing.
    uint8_t alignas(16) buf[ext->max_cmp_frame_size + 1];
    int nframes = DIVIDE_CEIL(wlen + intra_skip, ext->frame_size);

    // Decode into one contiguous append (Opus frames are larger than the
    // default samplebuffer margin; declare append_units so the mirrored
    // tail covers a whole frame).
    int16_t *out = samplebuffer_append(sbuf, ext->frame_size*nframes);
    for (int i=0; i<nframes+preroll_frames; i++) {
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

        int frame_size = ext->frame_size;
        if (i == preroll_frames && intra_skip > 0) {
            rspq_highpri_begin();
            rsp_opus_memmove_bytes(out, out + intra_skip * wav->wave.channels, (ext->frame_size - intra_skip) * wav->wave.channels * sizeof(int16_t));
            rspq_highpri_end();
            samplebuffer_undo(sbuf, intra_skip);
            frame_size -= intra_skip;
        }
        if (i >= preroll_frames) {
            out += frame_size * wav->wave.channels;
            wpos += frame_size;
            wlen -= frame_size;
        }
    }

    if (wav->wave.loop_len && wpos >= wav->wave.len) {
        // Opus decodes whole frames, so a read ending at the waveform tail can
        // overshoot `len` by up to one frame. Trim the overshoot; the mixer
        // continues the loop via a separate seeking read at loop_start (see
        // mixer.c waveform_read). This is valid for both full loops
        // (loop_start == 0) and partial loops (loop_start > 0), so the only
        // invariant we still need to protect is that the overshoot stays
        // within a single frame (otherwise samplebuffer_undo would trim
        // samples from a previous read).
        assertf(wpos - wav->wave.len < (int)ext->frame_size,
                "opus loop tail overshoot too large: %d (frame_size %ld)",
                wpos - wav->wave.len, (long)ext->frame_size);
        // Round the trim down for the same reason intra_skip is rounded above:
        // the decoder writes through SP DMA, so the write cursor has to stay on
        // a boundary the RSP can write to (see #samplebuffer_align_units), here
        // for the loop overread the mixer appends right after this.
        samplebuffer_undo(sbuf, ROUND_DOWN(wpos - wav->wave.len, samplebuffer_align_units(sbuf)));
    }
}

void wav64_opus_init(wav64_t *wav, int state_size) {
    rsp_opus_init();
    wav64_opus_header_t *ext = wav->st->ext;

    int err = OPUS_OK;
    ext->mode = opus_custom_mode_create(wav->wave.frequency, ext->frame_size, &err);
    assertf(err == OPUS_OK, "%i", err);
    assertf(state_size >= opus_custom_decoder_get_size(ext->mode, wav->wave.channels), 
        "wav64: opus state_size=%d calc_size=%d\n", state_size, opus_custom_decoder_get_size(ext->mode, wav->wave.channels));

    wav->wave.read = waveform_opus_read;
    wav->wave.start = waveform_opus_start;
    wav->wave.append_units = ext->frame_size;
    wav->wave.rsp_written = true;
    wav->wave.loop_restart_only = true;
}

void wav64_opus_close(wav64_t *wav) {
    wav64_opus_header_t *ext = wav->st->ext;
    if (ext->mode) opus_custom_mode_destroy(ext->mode);
    ext->mode = NULL;
}

int wav64_opus_get_bitrate(wav64_t *wav) {
    wav64_opus_header_t *ext = wav->st->ext;
    return ext->bitrate_bps;
}

int wav64_opus_adjust_seek(wav64_t *wav, int wpos) {
    wav64_opus_header_t *ext = wav->st->ext;
    wav64_opus_seekpoint_t *sp = wav64_opus_find_seekpoint(ext, wpos);
    return sp ? (int)sp->sample_offset : 0;
}
