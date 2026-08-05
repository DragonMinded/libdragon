/**
 * @file samplebuffer.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Sample buffer
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
#include "profile.h"
#include "rspq.h"
#include <stdlib.h>
#include <string.h>

/** @brief Set to 1 to activate debug logs */
#define MIXER_TRACE   0

///@cond
#if MIXER_TRACE
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
#define tracef(fmt, ...)  ({ })
#endif
///@endcond

static inline uint8_t *samplebuffer_base(const samplebuffer_t *buf) {
	return SAMPLES_PTR(buf);
}

/** Round a unit count so that count * unit_bytes is a multiple of 8. */
static int samplebuffer_align_len(const samplebuffer_t *buf, int len) {
	int ub = buf->unit_bytes;
	assert(ub > 0);
	if (ub & (ub - 1))
		return ROUND_UP(len, 8);
	// PCM: ub is 1, 2, or 4 → ROUND_UP(len << bps, 8) >> bps (no division).
	int bps = ub == 1 ? 0 : (ub == 2 ? 1 : 2);
	return ((len + ((8 >> bps) - 1)) >> (3 - bps)) << (3 - bps);
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
	int ub = buf->unit_bytes;
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
		// Write crossed the usable end into the margin: mirror the overflow
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
	buf->ptr_and_flags = (uint32_t)(uintptr_t)uncached_mem;
	assert((buf->ptr_and_flags & 7) == 0);
	buf->size = nbytes;
	buf->capacity_bytes = nbytes;

	if (state_size) {
		buf->state = uncached_mem + nbytes;
		buf->state_size = state_size;
	}

	buf->wnext = -1;
}

static int gcd(int a, int b) {
	while (b) { int t = a % b; a = b; b = t; }
	return a;
}

/**
 * Recompute the usable size in units, from the allocated bytes, the unit size
 * and the append granularity. The samplebuffer must be empty.
 */
static void samplebuffer_recalc_size(samplebuffer_t *buf) {
	int ub = buf->unit_bytes;
	assert(ub > 0);
	int usable = buf->capacity_bytes - SAMPLEBUFFER_MARGIN_UNITS * ub;
	assertf(usable >= ub, "samplebuffer too small for margin");

	// size * unit_bytes must be a multiple of 8, so that a waveform position
	// keeps its byte phase when it wraps around the samplebuffer.
	int n;
	if ((ub & (ub - 1)) == 0) {
		int bps = ub == 1 ? 0 : (ub == 2 ? 1 : 2);
		n = (usable & ~7) >> bps;
	} else {
		n = usable / ub;
		while (n > 0 && ((n * ub) & 7))
			n--;
	}
	assertf(n >= SAMPLEBUFFER_MARGIN_UNITS, "samplebuffer too small");

	// If the producer appends chunks that do not fit the tail margin, keep the
	// size a multiple of the chunk (and of the byte phase above): the write
	// cursor then wraps exactly at the end of the usable area, so a chunk is
	// never split by the wrap and never needs the live window to be relocated.
	// Smaller chunks always fit the margin, so they gain nothing from this.
	// Give up if it would not leave room for two chunks.
	int f = buf->append_units;
	if (f > SAMPLEBUFFER_MARGIN_UNITS) {
		int phase = 8 / gcd(ub, 8);
		int step = f / gcd(f, phase) * phase;
		if (n - n % step >= 2*f)
			n -= n % step;
	}

	buf->size = n;
	// The buffer is empty, but head is where the next stream restarts, and the
	// ring it indexes just changed size.
	buf->head %= n;
}

void samplebuffer_set_unit_bytes(samplebuffer_t *buf, int unit_bytes) {
	assert(unit_bytes > 0);
	assertf(buf->widx == 0 && buf->ridx == 0 && buf->wpos == 0,
		"samplebuffer_set_unit_bytes can only be called on an empty samplebuffer");

	buf->unit_bytes = (uint8_t)unit_bytes;
	samplebuffer_recalc_size(buf);
}

void samplebuffer_set_waveform(samplebuffer_t *buf, waveform_t *wave, WaveformRead read) {
	buf->wave = wave;
	buf->wv_read = read;
	assert(wave->state_size <= buf->state_size);

	buf->append_units = wave->append_units;
	if (buf->unit_bytes && buf->widx == 0 && buf->ridx == 0)
		samplebuffer_recalc_size(buf);
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

/**
 * Move the live window [ridx, widx) to the start of the sample area, so that a
 * following append of wlen units is contiguous (rare; only large appends).
 *
 * With a known append granularity, the window is placed so that the write
 * cursor lands on a multiple of it: as the usable size is a multiple as well,
 * from here on appends wrap exactly at the end of the samplebuffer and no
 * further relocation is needed, until an undo or a seek breaks the phase again.
 */
static void samplebuffer_relocate_live(samplebuffer_t *buf, int wlen) {
	int ub = buf->unit_bytes;
	int ring = buf->size;
	int live = buf->widx - buf->ridx;
	uint8_t *base = samplebuffer_base(buf);

	int dst = 0;
	int f = buf->append_units;
	if (f > SAMPLEBUFFER_MARGIN_UNITS && ring % f == 0 &&
		ROUND_UP(live, f) + wlen <= ring)
		dst = ROUND_UP(live, f) - live;

	if (live > 0) {
		samplebuffer_dma_wait(buf);
		// RSP may still be reading this window from a prior mix round. With
		// #waveform_t::append_units this path is rare (seek / undo that breaks
		// the write phase), so a full highpri sync is acceptable.
		rspq_highpri_sync();
		int src = samplebuffer_slot(buf, buf->ridx);
		if (src != dst) {
			int nbytes = live * ub;
			if (src + live <= ring) {
				memmove(base + dst * ub, base + src * ub, nbytes);
			} else {
				int first = ring - src;
				void *tmp = malloc(nbytes);
				assert(tmp);
				memcpy(tmp, base + src * ub, first * ub);
				memcpy((uint8_t*)tmp + first * ub, base, (live - first) * ub);
				memcpy(base + dst * ub, tmp, nbytes);
				free(tmp);
			}
		}
		// Keep the margin mirror coherent with the new start of the sample area.
		memcpy(base + ring * ub, base, SAMPLEBUFFER_MARGIN_UNITS * ub);
	} else if (buf->head != dst) {
		// Nothing live to move, but the write cursor still jumps backwards over
		// a region that a round emitted before the last flush can be reading:
		// this is the one case where restarting forward is not an option.
		rspq_highpri_sync();
	}

	buf->wpos += buf->ridx;
	buf->widx = live;
	buf->ridx = 0;
	buf->head = dst;
}

/** @brief Units that #samplebuffer_prefetch keeps ready ahead of the mixer. */
#define SAMPLEBUFFER_PREFETCH_UNITS      SAMPLEBUFFER_MARGIN_UNITS

/** @brief Units left ahead that trigger a refill.
 *
 * A WaveformRead costs mostly per call, so refilling late (in bigger chunks)
 * is cheaper; refilling too late means the mixer has to fetch synchronously
 * and wait for the transfer. */
#define SAMPLEBUFFER_PREFETCH_LOW_UNITS  32

void* samplebuffer_get(samplebuffer_t *buf, int wpos, int *wlen) {
	void *ret = NULL;
	PROFILE_SCOPE(PS_SBUF_GET) {
	samplebuffer_commit(buf);

	int ub = buf->unit_bytes;
	int ring = buf->size;
	int round_len = samplebuffer_align_len(buf, *wlen);

	tracef("samplebuffer_get: wpos=%x wlen=%x\n", wpos, *wlen);

	if (buf->widx == 0 || wpos < buf->wpos || wpos > buf->wpos + buf->widx) {
		tracef("samplebuffer_get: seek/flush wpos=%x (had %x+%x)\n", wpos, buf->wpos, buf->widx);
		assertf(buf->wv_read, "samplebuffer_get: no reader to seek to %x (had %x+%x)",
			wpos, buf->wpos, buf->widx);
		bool seeking = (buf->wnext < 0) || (wpos != buf->wnext);
		samplebuffer_flush(buf);
		buf->wpos = wpos;
		int len = round_len;
		// 8-bit PCM: dma_read only accepts a source and a destination of equal
		// parity, so the ring slot the stream restarts on has to keep the phase
		// of its waveform position (see samplebuffer_append).
		if (ub == 1 && ((buf->head - buf->wpos) & 1)) {
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
			assertf(buf->wv_read, "samplebuffer_get: no reader to extend to %x", wpos+reuse);
			assertf(wpos+reuse == buf->wnext, "wpos:%x reuse:%x buf->wnext:%x", wpos, reuse, buf->wnext);
			int missing = samplebuffer_align_len(buf, *wlen - reuse);
			// Free space without discarding past the live read cursor.
			missing = MIN(missing, ring - (buf->widx - buf->ridx));
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
		"samplebuffer_get: window %x not contiguous (slot=%x size=%x)", *wlen, slot, ring);
	ret = samplebuffer_base(buf) + slot * ub;
	}
	return ret;
}

void samplebuffer_prefetch(samplebuffer_t *buf, int wpos, int wlen) {
	if (!buf->wave || !buf->wv_read || !buf->wave->async_read ||
		buf->widx == 0 || buf->wnext < 0)
		return;
	samplebuffer_commit(buf);

	// Only extend the current stream: on a seek the samplebuffer is flushed,
	// so anything fetched now would be thrown away.
	if (wpos < buf->wpos || wpos > buf->wnext)
		return;
	// Fill back up to the watermark, so that one transfer covers several rounds.
	int ahead = buf->wnext - wpos;
	int missing = MAX(SAMPLEBUFFER_PREFETCH_UNITS, wlen) - ahead;
	if (missing <= 0 ||
		(ahead > SAMPLEBUFFER_PREFETCH_LOW_UNITS && ahead >= wlen))
		return;
	missing = samplebuffer_align_len(buf, missing);
	// Never ask for more than a margin: readers split bigger requests into
	// several appends, and since a buffer tracks a single in-flight transfer,
	// the second append would have to wait for the first one here.
	missing = MIN(missing, SAMPLEBUFFER_MARGIN_UNITS);
	// Never fetch into space that a live window still needs.
	missing = MIN(missing, buf->size - (buf->widx - buf->ridx));
	if (missing <= 0)
		return;

	// Leave the transfer in flight: samplebuffer_get will commit and sync it.
	PROFILE_SCOPE(PS_SBUF_GET) {
		buf->wv_read(buf->wave->ctx, buf, buf->wnext, missing, false);
	}
	buf->wnext = buf->wpos + buf->widx;
}

void* samplebuffer_append(samplebuffer_t *buf, int wlen) {
	samplebuffer_commit(buf);

	// Learn the granularity of the large appends, for producers that do not
	// declare it (or declare a divisor of the chunk they really use: one that
	// fits the margin cannot save any relocate, so the chunk we are seeing is
	// a better guess). The size can only be changed while the samplebuffer is
	// empty, as it is the modulus of all the physical positions.
	if (wlen > SAMPLEBUFFER_MARGIN_UNITS) {
		int f = buf->append_units > SAMPLEBUFFER_MARGIN_UNITS
			? gcd(buf->append_units, wlen) : wlen;
		if (f != buf->append_units) {
			buf->append_units = f;
			if (buf->widx == 0 && buf->ridx == 0)
				samplebuffer_recalc_size(buf);
		}
	}

	int ub = buf->unit_bytes;
	int ring = buf->size;
	// PCM: the ring slot and the waveform position must keep the same byte
	// phase, as dma_read only accepts source and destination of equal parity
	// (see raw_waveform_read). head and wpos move together on a discard, so
	// the phase is preserved even when both become odd; a seek resets head
	// and has to fix wpos up (see samplebuffer_get).
	assertf((ub & (ub - 1)) != 0 || (((buf->head - buf->wpos) * ub) & 1) == 0,
		"buf->wpos:%x buf->head:%x", buf->wpos, buf->head);

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
	// Contiguous write: fit before the usable end, or spill into the margin
	// (≤MARGIN). Codecs that append full frames larger than the margin (Opus,
	// YM) may need a relocate of the live window so that the write is linear.
	if (slot + wlen > ring && wlen > SAMPLEBUFFER_MARGIN_UNITS) {
		assertf(buf->widx - buf->ridx + wlen <= ring,
			"samplebuffer_append: buffer too small\n"
			"live:%x wlen:%x size:%x", buf->widx - buf->ridx, wlen, ring);
		samplebuffer_relocate_live(buf, wlen);
		slot = samplebuffer_slot(buf, buf->widx);
		assert(slot + wlen <= ring);
	} else if (slot + wlen > ring) {
		assertf(wlen <= SAMPLEBUFFER_MARGIN_UNITS,
			"samplebuffer_append: large append %x wraps (slot=%x)", wlen, slot);
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
	// Restart the ring where the last append left off, rather than rewinding it
	// to the top. Rounds already emitted read from the window that was live
	// here, and the RSP can still be behind them: the CPU is allowed to run
	// that far ahead (#mixer_poll_async, and the events served in the middle of
	// a poll). Refilling from the top would rewrite those samples under it,
	// while writing forward leaves the refill the same distance from them that
	// every append relies on — a whole turn of the ring.
	if (buf->size > 0)
		buf->head = (buf->head + buf->widx) % buf->size;
	buf->wpos = buf->widx = buf->ridx = 0;
	buf->wnext = -1;
	buf->pending_len = 0;
}
