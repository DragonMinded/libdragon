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

/** @brief Register mixer profile slots with #profile_init. */
void __mixer_profile_init(void);

#endif
