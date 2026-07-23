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
#include "utils.h"
#include "debug.h"
#include <string.h>

/** @brief Set to 1 to activate debug logs */
#define MIXER_TRACE   0

#if MIXER_TRACE
/** @brief like debugf(), but writes only if #MIXER_TRACE is not 0 */
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
/** @brief like debugf(), but writes only if #MIXER_TRACE is not 0 */
#define tracef(fmt, ...)  ({ })
#endif

void samplebuffer_init(samplebuffer_t *buf, uint8_t* uncached_mem, int nbytes, int state_size) {
	memset(buf, 0, sizeof(samplebuffer_t));

	// Store the buffer pointer as uncached address. We don't want to access
	// it with CPU as we want to build samples with RSP, and all APIs assume
	// that content is committed to RDRAM (not cache).
	assertf(UncachedAddr(uncached_mem) == uncached_mem, 
		"specified buffer must be in the uncached segment.\nTry using malloc_uncached() to allocate it");
	buf->ptr_and_flags = (uint32_t)uncached_mem;
	assert((buf->ptr_and_flags & 7) == 0);
	buf->size = nbytes;

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

	int nbytes = buf->size << SAMPLES_BPS_SHIFT(buf);

	int bps = bits_per_sample == 8 ? 0 : (bits_per_sample == 16 ? 1 : 2);
	buf->ptr_and_flags = SAMPLES_PTR_MAKE(SAMPLES_PTR(buf), bps);
	buf->size = nbytes >> bps;
}

void samplebuffer_set_waveform(samplebuffer_t *buf, waveform_t *wave, WaveformRead read) {
	buf->wave = wave;
	buf->wv_read = read;
	buf->wave_uuid = wave->__uuid;
	assert(wave->state_size <= buf->state_size);
}

bool samplebuffer_is_inited(samplebuffer_t *buf)
{
	return SAMPLES_PTR(buf) != NULL;
}

void samplebuffer_close(samplebuffer_t *buf) {
	// A pending RSP compaction still references the memory we're about to free.
	if (buf->wmove_end > 0)
		__mixer_dirty_wait(buf->move_round);
	void *ptr = SAMPLES_PTR(buf);
	if (ptr)
		free_uncached(ptr);
	memset(buf, 0, sizeof(samplebuffer_t));
}

void* samplebuffer_get(samplebuffer_t *buf, int wpos, int *wlen) {
	// ROUNDUP8_BPS rounds up the specified number of samples
	// (given the bps shift) so that they span an exact multiple
	// of 8 bytes. This will be applied to the number of samples
	// requested to wv_read(), to make sure that we always
	// keep the sample buffer filled with a multiple of 8 bytes.
	// This is not strictly required because dma_read() can do wonders,
	// but it results in slightly faster DMA transfers and its almost free
	// to do here.
	/// @cond
	#define ROUNDUP8_BPS(nsamples, bps) \
		(((nsamples)+((8>>(bps))-1)) >> (3-(bps)) << (3-(bps)))
	/// @endcond

	int bps = SAMPLES_BPS_SHIFT(buf);

	tracef("samplebuffer_get: wpos=%x wlen=%x\n", wpos, *wlen);

	if (buf->widx == 0 || wpos < buf->wpos || wpos > buf->wpos+buf->widx) {
		// If the requested position is totally outside
		// the existing range (and not even consecutive),
		// Flush the buffer and decode from scratch.
		// Normally this is because of seeking, but might
		// also be just because we discarded all the contents
		// of the buffer, so be sure to set seeking only if
		// the position is different from what we expected.
		tracef("samplebuffer_get: flushing buffer: buf->widx=%x buf->wpos=%x buf->wnext=%x\n", buf->widx, buf->wpos, buf->wnext);
		bool seeking = wpos != buf->wnext;	
		samplebuffer_flush(buf);
		buf->wpos = wpos;
		// Avoid setting a position that is odd, because it would case a
		// 2-byte phase change in the sample buffer, which would make 
		// impossible to call dma_read.
		int len = *wlen;
		if ((buf->wpos << bps) & 1) {
			buf->wpos--; len++;
		}
		buf->wv_read(buf->wave->ctx, buf, buf->wpos, ROUNDUP8_BPS(len, bps), seeking);
		buf->wnext = buf->wpos + buf->widx;
	} else {
		// Record first sample that we still need to keep in the sample
		// buffer. This is important to do now because decoder_read might
		// push more samples than required into the buffer and force
		// to compact the buffer. We thus need to know which samples
		// are still required.
		buf->ridx = wpos - buf->wpos;

		// Part of the requested samples are already in the sample buffer.
		// Check how many we can reuse. For instance, if there's a waveform
		// loop, the whole loop might already be in the sample buffer, so
		// no further decoding is necessary.
		int reuse = buf->wpos + buf->widx - wpos;

		// If the existing samples are not enough, read the missing
		// through the callback.
		if (reuse < *wlen) {
			tracef("samplebuffer_get: read missing: reuse=%x wpos=%x wlen=%x\n", reuse, wpos, *wlen);
			assertf(wpos+reuse == buf->wnext, "wpos:%x reuse:%x buf->wnext:%x", wpos, reuse, buf->wnext);
			buf->wv_read(buf->wave->ctx, buf, wpos+reuse, ROUNDUP8_BPS(*wlen-reuse, bps), false);
			buf->wnext = buf->wpos + buf->widx;
		}
	}

	assertf(wpos >= buf->wpos && wpos < buf->wpos+buf->widx, 
		"samplebuffer_get: logic error\n"
		"wpos:%x buf->wpos:%x buf->widx:%x", wpos, buf->wpos, buf->widx);

	int idx = wpos - buf->wpos;

	// If the sample buffer contains less samples than requested,
	// report that by updating *wlen. This will cause cracks in the
	// audio as silence will be inserted by the mixer.
	int len = buf->widx - idx;
	if (len < *wlen)
		*wlen = len;

	return SAMPLES_PTR(buf) + (idx << SAMPLES_BPS_SHIFT(buf));
}

void* samplebuffer_append(samplebuffer_t *buf, int wlen) {
	// If the requested number of samples doesn't fit the buffer, we
	// need to make space for it by discarding older samples.
	if (buf->widx + wlen > buf->size) {
		// Make space in the buffer by discarding everything up to the
		// ridx index, which is the first sample that we still need for playback.
		assertf(buf->widx >= buf->ridx,
			"samplebuffer_append: invalid consistency check\n"
			"widx:%x ridx:%x\n", buf->widx, buf->ridx);

		// Rollback ridx until it hit a 8-byte aligned position.
		// This preserves the guarantee that samplebuffer_append
		// will always return a 8-byte aligned pointer, which is
		// good for DMA purposes.
		int ridx = buf->ridx;
		while ((ridx << SAMPLES_BPS_SHIFT(buf)) & 7)
			ridx--;
		samplebuffer_discard(buf, buf->wpos+ridx);
	}

	assertf(((buf->wpos<< SAMPLES_BPS_SHIFT(buf)) % 2) == 0, "buf->wpos:%x", buf->wpos);

	// If there is still not space in the buffer, it means that the
	// buffer is too small for this append call. This is a logic error,
	// so better assert right away.
	// TODO: in principle, we could bubble this error up to the callers,
	// let them fill less samples than requested, and obtain some cracks
	// in the audio. Is it worth it?
	assertf(buf->widx + wlen <= buf->size,
		"samplebuffer_append: buffer too small\n"
		"ridx:%x widx:%x wlen:%x size:%x", buf->ridx, buf->widx, wlen, buf->size);

	// If an undo left a dirty tail that an in-flight RSP producer will still
	// write, wait for those producers before CPU-writing into the same region.
	if (buf->wdirty > buf->widx) {
		__mixer_dirty_wait(buf->dirty_round);
		buf->wdirty = 0;
	}

	// If an RSP-side compaction is pending, the CPU must not overwrite the
	// stale source region [wmove_start, wmove_end) before the RSP has read
	// it. Only gate when this append actually reaches into that region.
	if (buf->wmove_end > 0 && buf->widx + wlen > buf->wmove_start) {
		__mixer_dirty_wait(buf->move_round);
		buf->wmove_end = 0;
	}

	// Track the round that covers whatever producer will write the returned
	// area (e.g. an RSP VADPCM decode enqueued right after this call).
	buf->prod_round = __mixer_round_producer();

	void *data = SAMPLES_PTR(buf) + (buf->widx << SAMPLES_BPS_SHIFT(buf));
	buf->widx += wlen;
	return data;
}

void samplebuffer_undo(samplebuffer_t *buf, int wlen) {
	tracef("samplebuffer_truncate: wlen=%x\n", wlen);
	assertf(buf->widx >= wlen, "samplebuffer_append_undo: invalid wlen:%x widx:%x", wlen, buf->widx);
	buf->widx -= wlen;
	// Bytes above the new widx may still be written by an in-flight RSP
	// producer (e.g. VADPCM decode). Tag them so append waits if needed.
	buf->wdirty = MAX(buf->wdirty, buf->widx + wlen);
	buf->dirty_round = __mixer_round_producer();
}

void samplebuffer_discard(samplebuffer_t *buf, int wpos) {
	// Compute the index of the first sample that will be preserved (and thus
	// will be moved to position 0 of the buffer).
	int idx = wpos - buf->wpos;
	if (idx <= 0)
		return;
	if (idx > buf->widx)
		idx = buf->widx;

	int bps = SAMPLES_BPS_SHIFT(buf);

	// Align the discard position down to 8 bytes. This preserves the 2-byte
	// phase of the waveform address (which helps waveform implementations
	// that want to use dma_read()), and additionally keeps the source of the
	// compaction move 8-byte aligned, as required by the RSP fast path below.
	idx &= ~((8 >> bps) - 1);
	if (idx == 0)
		return;

	tracef("samplebuffer_discard: wpos=%x idx:%x buf->wpos=%x buf->widx=%x\n", wpos, idx, buf->wpos, buf->widx);
	int kept_bytes = (buf->widx - idx) << bps;
	if (kept_bytes > 0) {		
		tracef("samplebuffer_discard: compacting buffer, moving 0x%x bytes\n", kept_bytes);

		uint8_t *src = SAMPLES_PTR(buf) + (idx << bps);
		uint8_t *dst = SAMPLES_PTR(buf);
		assert(((uint32_t)dst & 7) == 0);

		// Fast path: enqueue an RSP-side memmove in the highpri queue. Being
		// a regular queue command, it executes in-order after the in-flight
		// decode/mix commands that still reference the old buffer layout, so
		// no CPU wait is needed. It requires the kept size to be a multiple
		// of 8 bytes (RSP DMA granularity): we cannot round it up like the
		// CPU copy below does, because the bytes just after the kept region
		// belong to the append area that the CPU may write in the meantime.
		if ((kept_bytes & 7) == 0 && __mixer_memmove_async(dst, src, kept_bytes)) {
			// Record the stale source region (in physical buffer slots):
			// CPU appends must not overwrite it until the RSP executes the
			// move (see samplebuffer_append).
			int start = idx, end = buf->widx;
			if (buf->wmove_end > 0) {
				start = MIN(start, buf->wmove_start);
				end = MAX(end, buf->wmove_end);
			}
			buf->wmove_start = start;
			buf->wmove_end = end;
			buf->move_round = __mixer_round_producer();
		} else {
			// Slow path (kept size not a multiple of 8 bytes, which can
			// happen after samplebuffer_undo truncates the buffer): copy
			// with the CPU. Wait until nothing in-flight references the
			// buffer anymore: a pending compaction move, producers that may
			// still be writing the kept region, and mix rounds that read
			// the old layout.
			if (buf->wmove_end > 0) {
				__mixer_dirty_wait(buf->move_round);
				buf->wmove_end = 0;
			}
			__mixer_dirty_wait(buf->prod_round);
			__mixer_round_wait(buf->last_round);

			// Optimized copy of samples. We work on uncached memory directly
			// so that we don't need to flush, and use only 64-bits ops. We
			// round up to a multiple of 8 the amount of bytes: it doesn't
			// matter if we copy a bit more, because this copy is synchronous
			// (nothing else can be writing the buffer while it runs).
			// This has been benchmarked to be faster than memmove() + cache flush.
			int copy_bytes = ROUND_UP(kept_bytes, 8);
			u_uint64_t *src64 = (u_uint64_t*)src;
			uint64_t *dst64 = (uint64_t*)dst;
			for (int i=0;i<copy_bytes/8;i++)
				*dst64++ = *src64++;
		}
	}

	buf->wpos += idx;
	buf->widx -= idx;
	buf->ridx -= idx;
	if (buf->ridx < 0)
		buf->ridx = 0;
	if (buf->wdirty > 0) {
		buf->wdirty -= idx;
		if (buf->wdirty < 0)
			buf->wdirty = 0;
	}
}

void samplebuffer_flush(samplebuffer_t *buf) {
	buf->wpos = buf->widx = buf->ridx = 0;
	buf->wnext = -1;
	buf->wdirty = 0;
	// A pending RSP compaction still reads/writes the whole [0, wmove_end)
	// area: extend the protected region down to 0 so that the first append
	// after the flush waits for the move to complete.
	if (buf->wmove_end > 0)
		buf->wmove_start = 0;
}
