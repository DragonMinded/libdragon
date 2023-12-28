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

#include "libopus.c"

typedef struct {
    uint32_t frame_size;
    uint32_t max_cmp_frame_size;
    uint32_t bitrate_bps;
} wav64_opus_header_ext;

typedef struct {
    wav64_opus_header_ext xhead;
    uint32_t current_rom_addr;
    OpusCustomMode *mode;
    OpusCustomDecoder *dec;
} wav64_opus_state;

static uint16_t io_read16(uint32_t addr) {
    uint32_t value = io_read(addr & ~3);
    if (!(addr & 2))
        value >>= 16;
    return value;
}


static void waveform_opus_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	wav64_opus_state *st = (wav64_opus_state*)wav->ext;

	if (seeking) {
		if (wpos == 0) {
			st->current_rom_addr = wav->rom_addr;
            opus_custom_decoder_ctl(st->dec, OPUS_RESET_STATE);
		} else {
            assertf(0, "seeking not support in wav64 with opus compression");
		}
	}

    // Allocate stack buffer for reading compressed data. Align it to cacheline
    // to avoid any false sharing.
    uint8_t buf[st->xhead.max_cmp_frame_size] alignas(16);

    // rspq_highpri_begin();
    while (wlen > 0) {
        // Read frame size
        int nb = io_read16(st->current_rom_addr);
        assertf(nb <= st->xhead.max_cmp_frame_size, "opus frame size too large: %d (%ld)", nb, st->xhead.max_cmp_frame_size);

        // Read frame
        data_cache_hit_writeback_invalidate(buf, nb);
        dma_read(buf, st->current_rom_addr + 2, nb);

        // Update ROM pointer after current frame
        st->current_rom_addr += nb + 2;
        if (st->current_rom_addr & 1)
            st->current_rom_addr += 1;

        // Decode frame
        int16_t *out = samplebuffer_append(sbuf, st->xhead.frame_size);

        int err = opus_custom_decode(st->dec, buf, nb, out, st->xhead.frame_size);
        assertf(err == st->xhead.frame_size, "opus decode error: %s", opus_strerror(err));

        wpos += st->xhead.frame_size;
        wlen -= st->xhead.frame_size;
        if (wpos > wav->wave.len) {
            // The last audio frame is padded with zeros. Truncate it automatically
            // to the right length.
            samplebuffer_undo(sbuf, wpos - wav->wave.len);
        }
    }
    // rspq_highpri_end();
}

void wav64_opus_init(wav64_t *wav, int fh) {
    wav64_opus_header_ext xhead; 
    dfs_read(&xhead, sizeof(xhead), 1, fh);
    debugf("opus header: frame_size=%ld, max_cmp_frame_size=%ld, bitrate_bps=%ld\n", xhead.frame_size, xhead.max_cmp_frame_size, xhead.bitrate_bps);
    debugf("frequency: %f\n", wav->wave.frequency);

    int err = OPUS_OK;
    OpusCustomMode *custom_mode = opus_custom_mode_create(wav->wave.frequency, xhead.frame_size, &err);
    assert(err == OPUS_OK);
    OpusCustomDecoder *dec = opus_custom_decoder_create(custom_mode, wav->wave.channels, &err);
    assert(err == OPUS_OK);

    // FIXME: try to avoid one allocation by allocating the decoder in the same malloc
    wav64_opus_state *state = malloc(sizeof(wav64_opus_state));
    state->current_rom_addr = 0;
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
