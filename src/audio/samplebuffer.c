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

extern inline int samplebuffer_margin_units(int append_units);

void samplebuffer_dma_wait(samplebuffer_t *buf) {
	if (buf->dma_ticket) {
		dma_wait_finished(buf->dma_ticket);
		buf->dma_ticket = 0;
	}
}

/**
 * Copy exactly @p nbytes inside the sample area.
 *
 * A waveform that declares #waveform_t::rsp_written produces samples through
 * the RSP queue: the copy has to travel through the same queue so it lands
 * after them. Every other producer writes with the CPU or a PI transfer, so
 * the bytes are already in RDRAM and a memcpy is enough (after any PI DMA).
 */
static void samplebuffer_copy(samplebuffer_t *buf, uint8_t *dst, uint8_t *src, int nbytes) {
	if (buf->wave && buf->wave->rsp_written) {
		// The queued copy moves whole 8-byte blocks, and the bytes past
		// nbytes at the destination are not ours to change: stage them at the
		// source so that the tail of the copy rewrites them unchanged.
		int n8 = ROUND_UP(nbytes, 8);
		if (n8 > nbytes)
			memcpy(src + nbytes, dst + nbytes, n8 - nbytes);
		if (__mixer_rdram_copy(dst, src, n8))
			return;
	}
	samplebuffer_dma_wait(buf);
	memcpy(dst, src, nbytes);
}

/** Flush a pending spill from the margin back to the start of the ring. */
static void samplebuffer_unspill(samplebuffer_t *buf) {
	if (buf->spill <= 0)
		return;
	uint8_t *base = samplebuffer_base(buf);
	int nbytes = buf->spill * buf->unit_bytes;
	buf->spill = 0;
	// Exactly the spilled units: anything past them is a unit still live in
	// the ring (for VADPCM, a different 9-byte frame).
	samplebuffer_copy(buf, base, base + buf->size * buf->unit_bytes, nbytes);
}

void samplebuffer_init(samplebuffer_t *buf, uint8_t* uncached_mem, int nbytes, int state_size) {
	memset(buf, 0, sizeof(samplebuffer_t));

	assertf(UncachedAddr(uncached_mem) == uncached_mem,
		"specified buffer must be in the uncached segment.\nTry using malloc_uncached() to allocate it");
	buf->ptr_and_flags = (uint32_t)(uintptr_t)uncached_mem;
	assert((buf->ptr_and_flags & 7) == 0);
	// size is in units, so it stays 0 until the unit size is known.
	buf->capacity_bytes = nbytes;
	buf->margin_units = SAMPLEBUFFER_MARGIN_UNITS;

	if (state_size) {
		buf->state = uncached_mem + nbytes;
		buf->state_size = state_size;
	}

	buf->wnext = -1;
}

/**
 * Recompute the usable size in units, from the allocated bytes, the unit size
 * and the margin. The samplebuffer must be empty.
 */
static void samplebuffer_recalc_size(samplebuffer_t *buf) {
	int ub = buf->unit_bytes;
	assert(ub > 0);

	int margin = samplebuffer_margin_units(buf->append_units);
	buf->margin_units = margin;

	int usable = buf->capacity_bytes - margin * ub;
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

	// Changing the modulus invalidates the physical position of any window the
	// RSP may still be reading. A full highpri sync is the safe barrier; the
	// samplebuffer itself does not track rounds. The first configuration of a
	// buffer has handed out no window yet, so it never syncs.
	if (buf->size > 0 && n != buf->size)
		rspq_highpri_sync();

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

/** @brief Units left ahead that trigger a refill.
 *
 * A WaveformRead costs mostly per call, so refilling late (in bigger chunks)
 * is cheaper; refilling too late means the mixer has to fetch synchronously
 * and wait for the transfer. */
#define SAMPLEBUFFER_PREFETCH_LOW_UNITS  32

void* samplebuffer_get(samplebuffer_t *buf, int wpos, int *wlen) {
	void *ret = NULL;
	PROFILE_SCOPE(PS_SBUF_GET) {
	int ub = buf->unit_bytes;
	int ring = buf->size;
	int round_len = samplebuffer_align_len(buf, *wlen);

	tracef("samplebuffer_get: wpos=%x wlen=%x\n", wpos, *wlen);

	if (buf->widx == 0 || wpos < buf->wpos || wpos > buf->wpos + buf->widx) {
		tracef("samplebuffer_get: seek/flush wpos=%x (had %x+%x)\n", wpos, buf->wpos, buf->widx);
		assertf(buf->wv_read, "samplebuffer_get: no reader to seek to %x (had %x+%x)",
			wpos, buf->wpos, buf->widx);
		bool seeking = (buf->wnext < 0) || (wpos != buf->wnext);
		// Discard any pending spill with the flush: copying it back would
		// rewrite samples that this seek is about to throw away.
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
		samplebuffer_unspill(buf);
		buf->wnext = buf->wpos + buf->widx;
	} else {
		samplebuffer_unspill(buf);
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
			samplebuffer_unspill(buf);
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

	// Window straddles the wrap: mirror the leading units into the margin so
	// the RSP sees one linear DMA. wlen already includes the mixer's overread.
	int need = slot + *wlen - ring;
	if (need > 0) {
		assertf(need <= buf->margin_units,
			"samplebuffer_get: mirror %x > margin %x", need, buf->margin_units);
		uint8_t *base = samplebuffer_base(buf);
		samplebuffer_copy(buf, base + ring * ub, base, ROUND_UP(need * ub, 8));
	}

	ret = samplebuffer_base(buf) + slot * ub;
	}
	return ret;
}

void samplebuffer_prefetch(samplebuffer_t *buf, int wpos, int wlen) {
	if (!buf->wave || !buf->wv_read || !buf->wave->async_read ||
		buf->widx == 0 || buf->wnext < 0)
		return;
	samplebuffer_unspill(buf);

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

	// Leave the transfer in flight: samplebuffer_get will sync it.
	PROFILE_SCOPE(PS_SBUF_GET) {
		buf->wv_read(buf->wave->ctx, buf, buf->wnext, missing, false);
	}
	buf->wnext = buf->wpos + buf->widx;
}

void* samplebuffer_append(samplebuffer_t *buf, int wlen) {
	samplebuffer_unspill(buf);

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
	// Contiguous write: either fits before the usable end, or spills into the
	// margin (≤ margin_units).
	if (slot + wlen > ring) {
		assertf(wlen <= buf->margin_units,
			"samplebuffer_append: append of %x units wraps and does not fit the\n"
			"margin (slot:%x margin:%x): declare it in waveform_t::append_units",
			wlen, slot, buf->margin_units);
		buf->spill = slot + wlen - ring;
	}

	buf->widx += wlen;
	uint8_t *ret = samplebuffer_base(buf) + slot * ub;
	// An RSP producer writes through SP DMA, which ignores the low 3 bits of
	// the RDRAM address: an unaligned slot would silently land elsewhere.
	assertf(!buf->wave || !buf->wave->rsp_written || ((uintptr_t)ret & 7) == 0,
		"samplebuffer_append: unaligned slot %x for an RSP-written waveform", slot);
	return ret;
}

void samplebuffer_undo(samplebuffer_t *buf, int wlen) {
	samplebuffer_unspill(buf);
	tracef("samplebuffer_undo: wlen=%x\n", wlen);
	assertf(buf->widx >= wlen, "samplebuffer_undo: invalid wlen:%x widx:%x", wlen, buf->widx);
	buf->widx -= wlen;
	if (buf->wnext >= 0)
		buf->wnext = buf->wpos + buf->widx;
}

void samplebuffer_flush(samplebuffer_t *buf) {
	samplebuffer_dma_wait(buf);
	// Discard any pending spill: the samples are being thrown away, and
	// copying them back could overwrite a window a prior round still holds.
	buf->spill = 0;
	// Restart the ring where the last append left off, rather than rewinding it
	// to the top. Rounds already emitted read from the window that was live
	// here, and the RSP can still be behind them: the CPU is allowed to run
	// that far ahead (#mixer_poll_async). Writing forward leaves the refill
	// a whole turn of the ring away from those samples.
	if (buf->size > 0)
		buf->head = (buf->head + buf->widx) % buf->size;
	buf->wpos = buf->widx = buf->ridx = 0;
	buf->wnext = -1;
}
