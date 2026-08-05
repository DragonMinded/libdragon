/**
 * @file mixer_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_MIXER_INTERNAL_H
#define LIBDRAGON_MIXER_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/** @brief RSPQ overlay ID assigned to the mixer ucode */
extern uint32_t __mixer_overlay_id;

/** @brief Profile slots for mixer / xm64 / vadpcm (see #__mixer_profile_init). */
enum {
	PS_MIXER = 0,       ///< mixer_try_play
	PS_XM_TICK,         ///< xm64 tick callback (sequencing + channel sync)
	PS_XM_GETPOS,       ///< xm64: sync sample_position from mixer
	PS_XM_LIBXM,        ///< xm64: xm_tick (libxm sequencer)
	PS_XM_SYNC,         ///< xm64: play/pos/freq/vol sync to mixer
	PS_MIXER_EXEC,      ///< mixer_exec
	PS_MIXER_PREP,      ///< update_loops + refresh_chtbl + round_length
	PS_MIXER_EMIT,      ///< mixer_emit_round (rspq write + fetch)
	PS_SBUF_GET,        ///< samplebuffer_get
	PS_VADPCM_READ,     ///< waveform_vadpcm_read_compressed
	PS_VADPCM_HUFF,     ///< huffv_decompress
	PS_VADPCM_IO,       ///< plain VADPCM DMA/read
	PS_MIXER_ADVANCE,   ///< mixer_advance
	PS_MIXER_SEEK,      ///< mixer_ch_seek
};

/**
 * Bytes of VADPCM codebook data per channel: eight predictor vectors
 * (128 bytes) followed by the first three samples decoded at the loop start
 * (plus one halfword of padding). Must match the WAV64 on-disk layout and the
 * mixer ucode DMA size.
 */
#define VADPCM_CODEBOOK_STRIDE  136

/**
 * @brief Size in bytes of a VADPCM frame with residuals of `bits` bits.
 *
 * One control byte plus 16 residuals: 9 bytes for the classic 4-bit encoding,
 * 7 for 3-bit, 5 for 2-bit. Frames always hold 16 samples.
 */
#define VADPCM_FRAME_BYTES(bits)  (2*(bits) + 1)

/** @brief Codec side-data for #WAVEFORM_FORMAT_VADPCM waveforms. */
typedef struct {
	/** Predictor codebook in RDRAM (must stay valid for the waveform lifetime). */
	void *codebook;
	/**
	 * Decoder state at the loop start (16-byte vector), or NULL if the
	 * waveform does not loop / loop state is unknown. Used by the RSP on
	 * cached-loop wraps.
	 */
	void *loop_state;
	/**
	 * Bits per residual in the bitstream: 2, 3 or 4. Framing, codebook, state
	 * and predictor order do not change with it, only the payload packing
	 * (see #VADPCM_GROUP_OFFSET) and thus #VADPCM_FRAME_BYTES.
	 */
	uint8_t bits;
} waveform_vadpcm_t;

/** @brief Register mixer profile slots with #profile_init. */
void __mixer_profile_init(void);

/**
 * @brief Rounds: how the CPU knows what the RSP has already read.
 *
 * The CPU queues mix rounds without waiting for them, so the input windows it
 * hands out stay live until the RSP gets to them. Each round carries an id
 * that MIX_FLUSH writes back to RDRAM once the round is over.
 * #mixer_poll_async waits for the round that closed the poll
 * MIXER_POLL_LOOKAHEAD polls ago before starting a new one, which is what
 * keeps samplebuffer refills from overwriting live windows. Ids start at 1,
 * so 0 means "no round".
 * @{
 */

/** @brief True if the RSP has finished the given round. */
bool __mixer_round_done(uint32_t id);

/** @brief Wait for the RSP to finish the given round. */
void __mixer_round_wait(uint32_t id);

/** @} */

/**
 * @brief Copy a block of RDRAM onto another one through the mixer ucode.
 *
 * What the RSP brings here is ordering: the copy runs where it is enqueued,
 * which is what mirroring data that an audio codec overlay has been asked to
 * write, and may not have written yet, requires (see #waveform_t::rsp_written).
 *
 * Both addresses and the length must be 8-byte aligned. Returns false if the
 * mixer is not running, leaving the copy to the caller.
 */
bool __mixer_rdram_copy(void *dst, void *src, int nbytes);

/**
 * @brief Output samples the CPU may mix without ever waiting for the RSP.
 *
 * The span sample buffers are sized for. Callers that size their own buffers
 * (xm64) have to use the same figure.
 */
int __mixer_inflight_samples(void);

#endif
