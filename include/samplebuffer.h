/**
 * @file samplebuffer.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Sample buffer (ring FIFO)
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
 * Tagged pointer to an array of samples. It contains both the void*
 * sample pointer, and byte-per-sample information (encoded as shift value).
 */
typedef uint32_t sample_ptr_t;

/**
 * SAMPLES_BPS_SHIFT extracts the byte-per-sample information from a sample_ptr_t.
 * Byte-per-sample is encoded as shift value, so the actual number of bits is
 * 1 << BPS. Valid shift values are 0, 1, 2 (which corresponds to 1, 2 or 4
 * bytes per sample).
 */
#define SAMPLES_BPS_SHIFT(buf)      ((buf)->ptr_and_flags & 3)

/**
 * SAMPLES_PTR extract the raw void* to the sample array. The size of array
 * is not encoded in the tagged pointer. Notice that it is implemented with a
 * XOR because on MIPS it's faster than using a reverse mask.
 */
#define SAMPLES_PTR(buf)            (void*)((buf)->ptr_and_flags ^ SAMPLES_BPS_SHIFT(buf))

/**
 * SAMPLES_PTR_MAKE create a tagged pointer, given a pointer to an array of
 * samples and a byte-per-sample value (encoded as shift value).
 */
#define SAMPLES_PTR_MAKE(ptr, bps)  ((sample_ptr_t)(ptr) | (bps))

/**
 * Maximum window (in units) that #samplebuffer_get / #samplebuffer_append may
 * request. Matches the ring tail margin: one MIX_CHANNEL input span plus
 * RSP sample-cache overread.
 */
#define SAMPLEBUFFER_MARGIN_UNITS  128

/**
 * samplebuffer_t is a ring FIFO of samples used by the mixer to feed the RSP.
 *
 * The mixer follows a "pull" architecture. During mixer_poll, it will call
 * samplebuffer_get() to extract samples from the buffer. If the required
 * samples are not available, the sample buffer will callback the waveform
 * decoder to produce more samples, through the WaveformRead API. The
 * waveform read function will push samples into the buffer via samplebuffer_append,
 * so that they become available for the mixer.
 *
 * Data is stored in a circular ring with a small tail margin so that any
 * window up to the margin size is always contiguous in physical memory
 * (required by RSP DMA). RING_SIZE * unit_bytes is kept a multiple of 8 so
 * that absolute waveform positions preserve their 2-byte phase in the ring
 * (dma_read / dfs compatibility).
 *
 * In general, the sample buffer assumes that the contained data is committed
 * to physical memory, not just CPU cache. It is responsibility of the client
 * to flush DMA cache (via data_cache_writeback) if samples are written
 * via CPU.
 */
typedef struct samplebuffer_s {
    /**
     * Tagged pointer to the actual buffer. Lower bits contain bit-per-shift.
     */
    sample_ptr_t ptr_and_flags;

    /**
     * Bytes per logical unit when not a power-of-two sample width.
     * 0 means "use 1 << SAMPLES_BPS_SHIFT(buf)" (PCM path). Non-zero is used
     * for compressed frame stores (e.g. 9 for mono VADPCM frames). In that
     * mode wpos/widx/size are counted in units (frames), not PCM samples.
     */
    uint8_t unit_bytes;

    /**
     * Ring size in units (excludes the tail margin).
     **/
    int size;

    /**
     * Absolute position in the waveform of the first sample
     * currently valid in the ring.
     */
    int wpos;

    /**
     * Number of valid units currently stored in the ring.
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
     * Highest mix round id whose RSP mix command still references bytes
     * in this buffer.
     */
    uint32_t last_round;

    /**
     * Ticket of the most recent async PI DMA into this buffer (0 = none).
     * Producers set it when issuing dma_read_async; consumers wait via
     * #samplebuffer_dma_wait before touching the bytes or freeing memory.
     */
    uint64_t dma_ticket;

    /** Total bytes allocated for the sample area (ring + margin). */
    int capacity_bytes;

    /** Physical slot of wpos within the ring. */
    int head;

    /** Pending append into the margin that needs a mirror copy-back. */
    int pending_slot;
    int pending_len;
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
 * RSP DMA across the ring wrap.
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
 * @brief Configure the bit width of the samples stored in the buffer.
 *
 * Valid values for "bps" are 1, 2, or 4: 1 can be used for 8-bit mono samples,
 * 2 for either 8-bit interleaved stereo or 16-bit mono, and 4 for 16-bit
 * interleaved stereo.
 *
 * Clears any previous #samplebuffer_set_unit_bytes setting.
 */
void samplebuffer_set_bps(samplebuffer_t *buf, int bps);

/**
 * @brief Configure a non-power-of-two unit size (compressed frames).
 *
 * After this call, wpos/widx/size are expressed in units of `unit_bytes`
 * each (e.g. VADPCM frames of 9 bytes). The buffer must be empty.
 */
void samplebuffer_set_unit_bytes(samplebuffer_t *buf, int unit_bytes);

/**
 * @brief Bytes per logical unit in the sample buffer.
 */
static inline int samplebuffer_unit_bytes(const samplebuffer_t *buf) {
	return buf->unit_bytes ? (int)buf->unit_bytes : (1 << SAMPLES_BPS_SHIFT(buf));
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
 * @brief Append samples into the buffer (zero-copy).
 *
 * Returns a contiguous uncached pointer where the caller must write `wlen`
 * units. The window size must not exceed the ring margin.
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
