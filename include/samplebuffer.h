/**
 * @file samplebuffer.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Sample buffer
 * @ingroup mixer
 */

#ifndef __LIBDRAGON_SAMPLEBUFFER_H
#define __LIBDRAGON_SAMPLEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @cond
typedef struct waveform_s waveform_t;
/// @endcond

/**
 * Pointer to the sample area (historically a tagged pointer; the low bits are
 * unused and the width of each unit lives in #samplebuffer_t::unit_bytes).
 */
typedef uint32_t sample_ptr_t;

/**
 * Extract the raw void* to the sample array from a samplebuffer.
 */
#define SAMPLES_PTR(buf)            ((void*)(uintptr_t)((buf)->ptr_and_flags))

/**
 * Maximum window (in units) that #samplebuffer_get may request across the ring
 * wrap. Matches one MIX_CHANNEL input span plus RSP sample-cache overread.
 * Also the default size of the mirrored tail (see #samplebuffer_t::margin_units).
 */
#define SAMPLEBUFFER_MARGIN_UNITS  128

/** @brief Units that #samplebuffer_prefetch keeps ready ahead of the mixer. */
#define SAMPLEBUFFER_PREFETCH_UNITS      SAMPLEBUFFER_MARGIN_UNITS

/**
 * @brief Size of the mirrored tail, in units, for the given append granularity.
 *
 * The caller that allocates the memory of a samplebuffer has to add this on
 * top of the usable size it wants (see #samplebuffer_t::margin_units).
 */
inline int samplebuffer_margin_units(int append_units)
{
    return append_units > SAMPLEBUFFER_MARGIN_UNITS
        ? append_units : SAMPLEBUFFER_MARGIN_UNITS;
}

/**
 * samplebuffer_t holds samples used by the mixer to feed the RSP.
 *
 * The mixer follows a "pull" architecture. During mixer_poll, it will call
 * samplebuffer_get() to extract samples from the buffer. If the required
 * samples are not available, the sample buffer will callback the waveform
 * decoder to produce more samples, through the WaveformRead API. The
 * waveform read function will push samples into the buffer via samplebuffer_append,
 * so that they become available for the mixer.
 *
 * Layout: `[0, size)` is the circular sample area; `[size, size+margin_units)`
 * is a contiguous spill/mirror region so that any append of up to
 * #margin_units units and any #samplebuffer_get window of up to
 * #SAMPLEBUFFER_MARGIN_UNITS units is always linear in physical memory
 * (required by RSP DMA). size * unit_bytes is kept a multiple of 8 so that
 * absolute waveform positions preserve their 2-byte phase in the samplebuffer
 * (dma_read / dfs compatibility).
 *
 * Physical cleanup has a single home: #samplebuffer_get mirrors the leading
 * units into the tail when a window straddles the wrap, and a pending spill
 * from a wrapping append is flushed back to the start before the next
 * get/append/flush/undo. The samplebuffer itself does not wait on the RSP;
 * that is the mixer's job (see mixer_poll_async).
 *
 * In general, the sample buffer assumes that the contained data is committed
 * to physical memory, not just CPU cache. It is responsibility of the client
 * to flush DMA cache (via data_cache_writeback) if samples are written
 * via CPU.
 */
typedef struct samplebuffer_s {
    /**
     * Pointer to the sample area (8-byte aligned, uncached).
     */
    sample_ptr_t ptr_and_flags;

    /**
     * Bytes per logical unit counted by wpos/widx/size.
     * PCM: 1, 2, or 4. Compressed frame stores: e.g. 9 for mono VADPCM.
     */
    uint8_t unit_bytes;

    /**
     * Usable size in units (excludes the tail margin).
     **/
    int size;

    /**
     * Granularity in units of the appends done by the producer, or 0 if
     * unknown: declared via #waveform_t::append_units. The mirrored tail is
     * sized to #margin_units = MAX(#SAMPLEBUFFER_MARGIN_UNITS, this), so a
     * single append of that size is always contiguous.
     */
    int append_units;

    /**
     * Size of the mirrored tail in units: MAX(#SAMPLEBUFFER_MARGIN_UNITS,
     * #append_units). Fixed for the life of a configuration.
     */
    int margin_units;

    /**
     * Absolute position in the waveform of the first sample
     * currently valid in the samplebuffer.
     */
    int wpos;

    /**
     * Number of valid units currently stored in the samplebuffer.
     */
    int widx;

    /**
     * Read cursor relative to wpos (first unit still needed for playback).
     */
    int ridx;

    /**
     * Next absolute position expected for a linear read (for seeking detection).
     */
    int wnext;

    /**
     * @brief Pointer to the state of the waveform decoder.
     */
    void *state;

    /**
     * @brief Allocated state size
     */
    int state_size;

    /**
     * Waveform being played back on this sample buffer.
     */
    waveform_t *wave;

    /**
     * wv_read is invoked by samplebuffer_get whenever more samples are
     * requested by the mixer. See #WaveformRead for more information.
     */
    WaveformRead wv_read;

    /**
     * Ticket of the most recent async PI DMA into this buffer (0 = none).
     * Producers set it when issuing dma_read_async; consumers wait via
     * #samplebuffer_dma_wait before touching the bytes or freeing memory.
     */
    uint64_t dma_ticket;

    /** Total bytes allocated for the sample area (usable size + margin). */
    int capacity_bytes;

    /**
     * Physical index (0 .. size-1) of #wpos in the circular sample area.
     * A unit at relative offset `rel` from wpos lives at
     * `(head + rel) % size`. Advances when the oldest units are discarded.
     */
    int head;

    /**
     * Units of the latest append that spilled past #size into the margin and
     * still need to be copied back to the start of the sample area. 0 means
     * nothing pending. Flushed by the next get/append/flush/undo.
     */
    int spill;
} samplebuffer_t;

/**
 * Initialize the sample buffer by binding it to the specified memory buffer.
 *
 * The sample buffer is guaranteed to be 8-bytes aligned, so the specified
 * memory buffer must follow this constraint. Moreover, the buffer must be
 * in the uncached segment and not loaded in any CPU cacheline. It is
 * strongly advised to allocate the buffer via #malloc_uncached, that takes
 * care of these constraints.
 *
 * The memory buffer will be used for storing samples and for the state.
 * Part of the sample area is reserved as a tail margin for contiguous
 * RSP DMA across the wrap.
 *
 * @param[in]   buf              Sample buffer
 * @param[in]   uncached_mem     Memory buffer to use. Must be 8-byte aligned,
 *                               and in the uncached segment.
 * @param[in]   size             Size of the memory buffer portion to use for samples (in bytes)
 * @param[in]   state_size       Size of the memory buffer portion to use for the state (in bytes)
 */
void samplebuffer_init(samplebuffer_t *buf, uint8_t *uncached_mem, int size, int state_size);

/**
 * @brief Return true if the samplebuffer is initialized.
 */
bool samplebuffer_is_inited(samplebuffer_t *buf);

/**
 * @brief Configure the logical unit size (bytes per wpos/widx step).
 *
 * After this call, wpos/widx/size are expressed in units of `unit_bytes`
 * each (e.g. 2 for 16-bit mono PCM, 9 for mono VADPCM frames). The buffer
 * must be empty.
 */
void samplebuffer_set_unit_bytes(samplebuffer_t *buf, int unit_bytes);

/**
 * @brief Configure the bit width of PCM samples stored in the buffer.
 *
 * Valid values are 8, 16, or 32 (total bits per interleaved frame).
 * Wrapper for #samplebuffer_set_unit_bytes with `bps / 8`.
 */
static inline void samplebuffer_set_bps(samplebuffer_t *buf, int bps) {
	samplebuffer_set_unit_bytes(buf, bps / 8);
}

/**
 * Connect a waveform reader callback to this sample buffer.
 */
void samplebuffer_set_waveform(samplebuffer_t *buf, waveform_t *wave, WaveformRead read);

/**
 * @brief Get a pointer to specific set of samples in the buffer (zero-copy).
 *
 * "wpos" is the absolute waveform position of the first sample that the
 * caller needs access to. "wlen" is the number of requested samples.
 * wpos must be monotonic between seeks.
 */
void* samplebuffer_get(samplebuffer_t *buf, int wpos, int *wlen);

/**
 * @brief Start fetching samples that will be requested later (zero-wait).
 *
 * Fetches the part of [wpos, wpos+wlen) that the buffer does not hold yet,
 * leaving any PI DMA in flight: the next #samplebuffer_get finds the samples
 * already there and only has to wait for a transfer that ran meanwhile.
 *
 * This is best-effort: the call does nothing unless the waveform declares
 * #waveform_t::async_read (there would be nothing to overlap with), and
 * likewise if the window is already available, if it is not contiguous with
 * what the buffer holds (a seek is coming, so anything fetched now would be
 * discarded), or if the samplebuffer has no room for it.
 */
void samplebuffer_prefetch(samplebuffer_t *buf, int wpos, int wlen);

/**
 * @brief Append samples into the buffer (zero-copy).
 *
 * Returns a contiguous uncached pointer where the caller must write `wlen`
 * units. `wlen` must not exceed #margin_units when the write would wrap past
 * the usable end (declare large chunks in #waveform_t::append_units so the
 * margin covers them).
 */
void* samplebuffer_append(samplebuffer_t *buf, int wlen);

/**
 * @brief Remove the a specified number of samples from the tail of the buffer.
 */
void samplebuffer_undo(samplebuffer_t *buf, int wlen);

/**
 * Flush (reset) the sample buffer to empty status, discarding all samples.
 */
void samplebuffer_flush(samplebuffer_t *buf);

/**
 * Wait for any in-flight async PI DMA into this buffer to complete.
 */
void samplebuffer_dma_wait(samplebuffer_t *buf);

/**
 * Close the sample buffer.
 */
void samplebuffer_close(samplebuffer_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* __LIBDRAGON_SAMPLEBUFFER_H */
