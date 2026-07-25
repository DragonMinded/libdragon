/**
 * @file samplebuffer.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Sample buffer (ring FIFO)
 * @ingroup mixer
 */

#include "mixer.h"
#include "mixer_internal.h"
#include "samplebuffer.h"
#include "n64sys.h"
#include "n64types.h"
#include "dma.h"
#include "utils.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

/** @brief Set to 1 to activate debug logs */
#define MIXER_TRACE   0

#if MIXER_TRACE
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
#define tracef(fmt, ...)  ({ })
#endif

static inline uint8_t *samplebuffer_base(const samplebuffer_t *buf) {
	return SAMPLES_PTR(buf);
}

void samplebuffer_dma_wait(samplebuffer_t *buf) {
	if (buf->dma_ticket) {
		dma_wait_finished(buf->dma_ticket);
		buf->dma_ticket = 0;
	}
}

/** Commit a pending append that may have spilled into the tail margin. */
static void samplebuffer_commit(samplebuffer_t *buf) {
	if (buf->pending_len <= 0)
		return;
	int ub = samplebuffer_unit_bytes(buf);
	int ring = buf->size;
	int slot = buf->pending_slot;
	int len = buf->pending_len;
	// Mirror needs the DMA to have finished; otherwise leave the ticket for
	// the mixer to wait on before the RSP consumes the window.
	if (slot < SAMPLEBUFFER_MARGIN_UNITS) {
		samplebuffer_dma_wait(buf);
		int n = MIN(len, SAMPLEBUFFER_MARGIN_UNITS - slot);
		memcpy(samplebuffer_base(buf) + (ring + slot) * ub,
		       samplebuffer_base(buf) + slot * ub,
		       n * ub);
	} else if (slot + len > ring) {
		samplebuffer_dma_wait(buf);
		// Write crossed the ring end into the margin: mirror the overflow
		// back to the start (producer wrote into the margin area).
		int overflow = slot + len - ring;
		memcpy(samplebuffer_base(buf),
		       samplebuffer_base(buf) + ring * ub,
		       overflow * ub);
	}
	buf->pending_len = 0;
}

void samplebuffer_init(samplebuffer_t *buf, uint8_t* uncached_mem, int nbytes, int state_size) {
	memset(buf, 0, sizeof(samplebuffer_t));

	assertf(UncachedAddr(uncached_mem) == uncached_mem,
		"specified buffer must be in the uncached segment.\nTry using malloc_uncached() to allocate it");
	buf->ptr_and_flags = (uint32_t)uncached_mem;
	assert((buf->ptr_and_flags & 7) == 0);
	buf->size = nbytes;
	buf->capacity_bytes = nbytes;

	if (state_size) {
		buf->state = uncached_mem + nbytes;
		buf->state_size = state_size;
	}

	buf->wnext = -1;
}

void samplebuffer_set_bps(samplebuffer_t *buf, int bits_per_sample) {
	assert(bits_per_sample == 8 || bits_per_sample == 16 || bits_per_sample == 32);
	assertf(buf->widx == 0 && buf->ridx == 0 && buf->wpos == 0,
		"samplebuffer_set_bps can only be called on an empty samplebuffer");

	int bps = bits_per_sample == 8 ? 0 : (bits_per_sample == 16 ? 1 : 2);
	buf->unit_bytes = 0;
	buf->ptr_and_flags = SAMPLES_PTR_MAKE(SAMPLES_PTR(buf), bps);
	int ub = 1 << bps;
	int margin_bytes = SAMPLEBUFFER_MARGIN_UNITS * ub;
	int usable = buf->capacity_bytes - margin_bytes;
	assertf(usable > 0, "samplebuffer too small for ring margin");
	usable &= ~7;
	buf->size = usable >> bps;
	assert(buf->size > 0);
}

void samplebuffer_set_unit_bytes(samplebuffer_t *buf, int unit_bytes) {
	assert(unit_bytes > 0);
	assertf(buf->widx == 0 && buf->ridx == 0 && buf->wpos == 0,
		"samplebuffer_set_unit_bytes can only be called on an empty samplebuffer");

	buf->unit_bytes = (uint8_t)unit_bytes;
	buf->ptr_and_flags = SAMPLES_PTR_MAKE(SAMPLES_PTR(buf), 0);
	int margin_bytes = SAMPLEBUFFER_MARGIN_UNITS * unit_bytes;
	int usable = buf->capacity_bytes - margin_bytes;
	assertf(usable >= unit_bytes, "samplebuffer too small for ring margin");
	int ring = usable / unit_bytes;
	while (ring > 0 && ((ring * unit_bytes) & 7))
		ring--;
	assertf(ring >= SAMPLEBUFFER_MARGIN_UNITS, "samplebuffer ring too small");
	buf->size = ring;
}

void samplebuffer_set_waveform(samplebuffer_t *buf, waveform_t *wave, WaveformRead read) {
	buf->wave = wave;
	buf->wv_read = read;
	assert(wave->state_size <= buf->state_size);
}

bool samplebuffer_is_inited(samplebuffer_t *buf)
{
	return SAMPLES_PTR(buf) != NULL;
}

void samplebuffer_close(samplebuffer_t *buf) {
	samplebuffer_dma_wait(buf);
	void *ptr = SAMPLES_PTR(buf);
	if (ptr)
		free_uncached(ptr);
	memset(buf, 0, sizeof(samplebuffer_t));
}

/** Physical slot for relative index `rel` (0 = wpos). */
static inline int samplebuffer_slot(const samplebuffer_t *buf, int rel) {
	return (buf->head + rel) % buf->size;
}

/** Move the live window [ridx, widx) to physical slot 0 (rare; large appends). */
static void samplebuffer_relocate_live(samplebuffer_t *buf) {
	int ub = samplebuffer_unit_bytes(buf);
	int ring = buf->size;
	int live = buf->widx - buf->ridx;
	uint8_t *base = samplebuffer_base(buf);

	if (live > 0) {
		samplebuffer_dma_wait(buf);
		// RSP may still be reading this window from a prior mix round.
		if (buf->last_round)
			__mixer_round_wait(buf->last_round);
		int src = samplebuffer_slot(buf, buf->ridx);
		if (src != 0) {
			int nbytes = live * ub;
			if (src + live <= ring) {
				memmove(base, base + src * ub, nbytes);
			} else {
				int first = ring - src;
				void *tmp = malloc(nbytes);
				assert(tmp);
				memcpy(tmp, base + src * ub, first * ub);
				memcpy((uint8_t*)tmp + first * ub, base, (live - first) * ub);
				memcpy(base, tmp, nbytes);
				free(tmp);
			}
		}
		// Keep margin mirror coherent for any live bytes in [0, MARGIN).
		if (live > 0 && live <= SAMPLEBUFFER_MARGIN_UNITS)
			memcpy(base + ring * ub, base, live * ub);
		else if (live > SAMPLEBUFFER_MARGIN_UNITS)
			memcpy(base + ring * ub, base, SAMPLEBUFFER_MARGIN_UNITS * ub);
	}

	buf->wpos += buf->ridx;
	buf->widx = live;
	buf->ridx = 0;
	buf->head = 0;
}

void* samplebuffer_get(samplebuffer_t *buf, int wpos, int *wlen) {
	samplebuffer_commit(buf);

	int ub = samplebuffer_unit_bytes(buf);
	int ring = buf->size;
	int round_len = *wlen;
	if (buf->unit_bytes) {
		round_len = ROUND_UP(round_len, 8);
	} else {
		int bps = SAMPLES_BPS_SHIFT(buf);
		round_len = ((round_len)+((8>>(bps))-1)) >> (3-(bps)) << (3-(bps));
	}

	tracef("samplebuffer_get: wpos=%x wlen=%x\n", wpos, *wlen);

	if (buf->widx == 0 || wpos < buf->wpos || wpos > buf->wpos + buf->widx) {
		tracef("samplebuffer_get: seek/flush wpos=%x (had %x+%x)\n", wpos, buf->wpos, buf->widx);
		bool seeking = (buf->wnext < 0) || (wpos != buf->wnext);
		samplebuffer_flush(buf);
		buf->wpos = wpos;
		buf->head = 0;
		int len = round_len;
		if (!buf->unit_bytes && (buf->wpos * ub) & 1) {
			buf->wpos--; len++;
		}
		buf->wv_read(buf->wave->ctx, buf, buf->wpos, len, seeking);
		samplebuffer_commit(buf);
		buf->wnext = buf->wpos + buf->widx;
	} else {
		buf->ridx = wpos - buf->wpos;
		int reuse = buf->wpos + buf->widx - wpos;
		if (reuse < *wlen) {
			tracef("samplebuffer_get: read missing: reuse=%x wpos=%x wlen=%x\n", reuse, wpos, *wlen);
			assertf(wpos+reuse == buf->wnext, "wpos:%x reuse:%x buf->wnext:%x", wpos, reuse, buf->wnext);
			int missing = *wlen - reuse;
			if (buf->unit_bytes)
				missing = ROUND_UP(missing, 8);
			else {
				int bps = SAMPLES_BPS_SHIFT(buf);
				missing = ((missing)+((8>>(bps))-1)) >> (3-(bps)) << (3-(bps));
			}
			buf->wv_read(buf->wave->ctx, buf, wpos+reuse, missing, false);
			samplebuffer_commit(buf);
			buf->wnext = buf->wpos + buf->widx;
		}
	}

	assertf(wpos >= buf->wpos && wpos < buf->wpos+buf->widx,
		"samplebuffer_get: logic error\n"
		"wpos:%x buf->wpos:%x buf->widx:%x", wpos, buf->wpos, buf->widx);

	int rel = wpos - buf->wpos;
	buf->ridx = rel;
	int len = buf->widx - rel;
	if (len < *wlen)
		*wlen = len;

	int slot = samplebuffer_slot(buf, rel);
	assertf(*wlen <= SAMPLEBUFFER_MARGIN_UNITS || slot + *wlen <= ring,
		"samplebuffer_get: window %x not contiguous (slot=%x ring=%x)", *wlen, slot, ring);
	return samplebuffer_base(buf) + slot * ub;
}

void* samplebuffer_append(samplebuffer_t *buf, int wlen) {
	samplebuffer_commit(buf);

	int ub = samplebuffer_unit_bytes(buf);
	int ring = buf->size;
	assertf(buf->unit_bytes || ((buf->wpos * ub) % 2) == 0, "buf->wpos:%x", buf->wpos);

	// Make room by discarding the oldest units past the read cursor.
	while (buf->widx + wlen > ring) {
		int drop = buf->widx + wlen - ring;
		assertf(drop <= buf->ridx,
			"samplebuffer_append: buffer too small\n"
			"ridx:%x widx:%x wlen:%x size:%x", buf->ridx, buf->widx, wlen, ring);
		buf->wpos += drop;
		buf->widx -= drop;
		buf->ridx -= drop;
		buf->head = (buf->head + drop) % ring;
	}

	int slot = samplebuffer_slot(buf, buf->widx);
	// Contiguous write: fit before ring end, or spill into the margin (≤MARGIN).
	// Codecs that append full frames larger than the margin (Opus, YM) may need
	// a one-shot relocate of the live window to physical 0 so the write is linear.
	if (slot + wlen > ring && wlen > SAMPLEBUFFER_MARGIN_UNITS) {
		assertf(buf->widx - buf->ridx + wlen <= ring,
			"samplebuffer_append: buffer too small\n"
			"live:%x wlen:%x size:%x", buf->widx - buf->ridx, wlen, ring);
		samplebuffer_relocate_live(buf);
		slot = buf->widx;
		assert(slot + wlen <= ring);
	} else if (slot + wlen > ring) {
		assertf(wlen <= SAMPLEBUFFER_MARGIN_UNITS,
			"samplebuffer_append: large append %x wraps ring (slot=%x)", wlen, slot);
	}

	buf->pending_slot = slot;
	buf->pending_len = wlen;
	buf->widx += wlen;
	return samplebuffer_base(buf) + slot * ub;
}

void samplebuffer_undo(samplebuffer_t *buf, int wlen) {
	samplebuffer_commit(buf);
	tracef("samplebuffer_undo: wlen=%x\n", wlen);
	assertf(buf->widx >= wlen, "samplebuffer_undo: invalid wlen:%x widx:%x", wlen, buf->widx);
	buf->widx -= wlen;
	if (buf->wnext >= 0)
		buf->wnext = buf->wpos + buf->widx;
}

void samplebuffer_flush(samplebuffer_t *buf) {
	samplebuffer_commit(buf);
	samplebuffer_dma_wait(buf);
	buf->wpos = buf->widx = buf->ridx = 0;
	buf->head = 0;
	buf->wnext = -1;
	buf->pending_len = 0;
}
