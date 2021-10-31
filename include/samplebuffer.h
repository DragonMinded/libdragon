/**
 * @file samplebuffer.h
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
 * samplebuffer_t is a circular buffer of samples. It is used by the mixer
 * to store and cache the samples required for playback on each channel.
 * The mixer creates a sample buffer for each initialized channel. The size
 * of the buffers is calculated for optimal playback, and might grow depending
 * on channel usage (what waveforms are played on each channel).
 *
 * The mixer follows a "pull" architecture. During mixer_poll, it will call
 * samplebuffer_get() to extract samples from the buffer. If the required
 * samples are not available, the sample buffer will callback the waveform
 * decoder to produce more samples, through the WaveformRead API. The
 * waveform read function will push samples into the buffer via samplebuffer_append,
 * so that they become available for the mixer. The decoder can be configured
 * with samplebuffer_set_decoder.
 *
 * The current implementation of samplebuffer does not achieve full zero copy,
 * because when the buffer is full, it is flushed and samples that need to 
 * be preserved (that is, already in the buffer but not yet played back) are
 * copied back at the beginning of the buffer with the CPU. This limitation
 * exists because the RSP ucode (rsp_audio.S) isn't currently able to "wrap around"
 * in the sample buffer. In future, this limitation could be lifted to achieve
 * full zero copy.
 *
 * The sample buffer tries to always stay 8-byte aligned to simplify operations
 * of decoders that might need to use DMA transfers (either PI DMA or RSP DMA).
 * To guarantee this property, #WaveformRead must collaborate by decoding
 * the requested number of samples. If WaveformRead decodes a different
 * number of samples, the alignment might be lost. Moreover, it always guarantees
 * that the buffer has the same 2-byte phase of the waveforms (that is, odd
 * samples of the waveforms are stored at odd addresses in memory); this is
 * the minimal property required by #dma_read (libdragon's optimized PI DMA
 * transfer for unaligned addresses).
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
     * Size of the buffer (in samples).
     **/
    int size;

    /**
     * Absolute position in the waveform of the first sample
     * in the sample buffer (the sample at index 0). It keeps track of
     * which part of the waveform this sample buffer contains.
     */
    int wpos;

    /**
     * Write pointer in the sample buffer (expressed as index of samples).
     * Since sample buffers are always filled from index 0, it is also
     * the number of samples stored in the buffer.
     */
    int widx;

    /**
     * Read pointer in the sample buffer (expressed as index of samples).
     * It remembers which sample was last read. Assuming a forward
     * streaming, it is used by the sample buffer to discard unused samples
     * when not needed anymore.
     */
    int ridx;

    /**
     * wv_read is invoked by samplebuffer_get whenever more samples are
     * requested by the mixer. See #WaveformRead for more information.
     */
    WaveformRead wv_read;

    /**
     * wv_ctx is the opaque pointer to pass as context to decoder functions.
     */
    void *wv_ctx;
} samplebuffer_t;

/**
 * Initialize the sample buffer by binding it to the specified memory buffer.
 *
 * The sample buffer is guaranteed to be 8-bytes aligned, so the specified
 * memory buffer must follow this constraints. If the decoder respects 
 * the "wlen" argument passed to WaveformRead callback, the buffer returned
 * by samplbuffer_append will always be 8-byte aligned and thus suitable
 * for DMA transfers. Notice that it's responsibility of the client to flush
 * the cache if the DMA is used.
 * 
 * @param[in]   buf     Sample buffer
 * @param[in]   mem     Memory buffer to use. Must be 8-byte aligned.
 * @param[in]   size    Size of the memory buffer, in bytes.
 */
void samplebuffer_init(samplebuffer_t *buf, uint8_t *mem, int size);

/**
 * @brief Configure the bit width of the samples stored in the buffer.
 * 
 * Valid values for "bps" are 1, 2, or 4: 1 can be used for 8-bit mono samples,
 * 2 for either 8-bit interleaved stereo or 16-bit mono, and 4 for 16-bit
 * interleaved stereo.
 * 
 * @param[in]   buf     Sample buffer
 * @param[in]   bps     Bytes per sample.
 */
void samplebuffer_set_bps(samplebuffer_t *buf, int bps);

/**
 * Connect a waveform reader callback to this sample buffer. The waveform
 * will be use to produce samples whenever they are required by the mixer
 * as playback progresses.
 *
 * "read" is the main decoding function, that is invoked to produce a specified
 * number of samples. Normally, the function is invoked by #samplebuffer_get,
 * whenever the mixer requests more samples. See #WaveformRead for more
 * information.
 * 
 * @param[in]       buf     Sample buffer
 * @param[in]       read    Waveform reading function, that produces samples.
 * @param[in]       ctx     Opaque context that will be passed to the read
 *                          function.
 */
void samplebuffer_set_waveform(samplebuffer_t *buf, WaveformRead read, void *ctx);

/**
 * @brief Get a pointer to specific set of samples in the buffer (zero-copy).
 * 
 * "wpos" is the absolute waveform position of the first sample that the
 * caller needs access to. "wlen" is the number of requested samples.
 *
 * The function returns a pointer within the sample buffer where the samples
 * should be read, and optionally changes "wlen" with the maximum number of
 * samples that can be read. "wlen" is always less or equal to the requested value.
 *
 * If the samples are available in the buffer, they will be returned immediately.
 * Otherwise, if the samplebuffer has a sample decoder registered via
 * samplebuffer_set_decoder, the decoder "read" function is called once to
 * produce the samples.
 *
 * If "wlen" is changed with a value less than "wlen", it means that
 * not all samples were available in the buffer and it was not possible to
 * generate more, so the caller should not loop calling this function, but
 * rather use what was obtained and possibly pad with silence.
 *
 * @param[in]       buf     Sample buffer
 * @param[in]       wpos    Absolute waveform position of the first samples to
 *                          return.
 * @param[in,out]   wlen    Number of samples to return. After return, it is
 *                          modified with the actual number of samples that
 *                          have been returned.
 * @return                  Pointer to samples.
 */
void* samplebuffer_get(samplebuffer_t *buf, int wpos, int *wlen);

/**
 * @brief Append samples into the buffer (zero-copy). 
 *
 * "wlen" is the number of samples that the caller will append.
 *
 * The function returns a pointer within the sample buffer where the samples
 * should be written. Notice that since audio samples are normally processed
 * via DMA/RSP, it is responsibility of the caller to actually force a cache
 * writeback (with #data_cache_hit_writeback) in case the samples are written
 * using CPU. In other words, this function expects samples to be written to
 * physical memory, not just CPU cache.
 *
 * The function is meant only to "append" samples, as in add samples that are
 * consecutive within the waveform to the ones already stored in the sample
 * buffer. This is necessary because #samplebuffer_t can only store a single
 * range of samples of the waveform; there is no way to hold two disjoint
 * ranges.
 *
 * For instance, if the sample buffer currently contains 50 samples
 * starting from position 100 in the waverform, the next call to
 * samplebuffer_append will append samples starting at 150.
 *
 * If required, samplebuffer_append will discard older samples to make space
 * for the new ones, through #samplebuffer_discard. It will only discard samples
 * that come before the "wpos" specified in the last #samplebuffer_get call, so
 * to make sure that nothing required for playback is discarded. If there is
 * not enough space in the buffer, it will assert.
 * 
 * @param[in]   buf     Sample buffer
 * @param[in]   wlen    Number of samples to append.
 * @return              Pointer to the area where new samples can be written.
 */
void* samplebuffer_append(samplebuffer_t *buf, int wlen);

/**
 * Discard all samples from the buffer that come before a specified
 * absolute waveform position.
 * 
 * This function can be used to discard samples that are not needed anymore
 * in the sample buffer. "wpos" specifies the absolute position of the first
 * sample that should be kept: all samples that come before will be discarded.
 * This function will silently do nothing if there are no samples to discard.
 * 
 * @param[in]   buf     Sample buffer
 * @param[in]   wpos    Absolute waveform position of the first sample that
 *                      must be kept.
 */
void samplebuffer_discard(samplebuffer_t *buf, int wpos);

/**
 * Flush (reset) the sample buffer to empty status, discarding all samples.
 * 
 * @param[in]   buf     Sample buffer.
 */
void samplebuffer_flush(samplebuffer_t *buf);

/**
 * Close the sample buffer.
 * 
 * After calling close, the sample buffer must be initialized again before
 * using it.
 * 
 * @param[in]   buf     Sample buffer.
 */
void samplebuffer_close(samplebuffer_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* __LIBDRAGON_SAMPLEBUFFER_H */
