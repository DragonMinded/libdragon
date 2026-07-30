	####################################################################
	#
	# Libdragon RSP ucode for audio mixer
	#
	# Authors: Giovanni Bajo <giovannibajo@gmail.com>
	####################################################################
	#
	# Architecture (per-channel commands)
	# ===================================
	#
	# Mixing is driven by three RSPQ commands:
	#
	#   MIX_SETCHANNEL – record the VADPCM codebook/state pointers of one channel
	#                  into a per-channel DMEM table.
	#   MIX_CHANNEL  – resample one channel for up to MAX_SAMPLES_PER_ROUND
	#                  output ticks and accumulate into a stereo DMEM buffer
	#                  (ACCUM) in .bss. Optionally decode mono VADPCM frames
	#                  into a DMEM window first (CH_FLAGS_VADPCM). Stereo
	#                  VADPCM is two independent mono MIX_CHANNELs (L/R planes).
	#   MIX_FLUSH    – DMA ACCUM out to RDRAM and clear it.
	#
	# All MIX_CHANNEL + MIX_FLUSH of a round are enqueued in one highpri burst
	# of this overlay so ACCUM (in .bss) cannot be clobbered mid-round. XVOL_L/R
	# (one-tap volume filter state) and the per-channel VADPCM table live in
	# saved state, so they survive overlay swaps.
	#
	# Per-round channel parameters travel inline in the MIX_CHANNEL command; the
	# VADPCM pointers, which only change when a new waveform starts, are sent by
	# MIX_SETCHANNEL. Sample data comes from
	# either resident RDRAM or a CPU-side samplebuffer (no RSP compaction).
	# Loop wraps (pos -= loop_len, reload loop_state) are handled here when
	# the CPU pins a stable loop region (resident or loop-cached).
	#
	# Resampling is point-sampling with a fixed-point step (WAVEFORM_POS_FRAC_BITS).
	# PCM samples are fetched from RDRAM into a small DMEM cache; VADPCM frames
	# are DMA'd compressed, decoded into a DMEM window, then resampled the same
	# way. Staging goes into DMEM_SCRATCH and is mixed into ACCUM with the
	# current filtered volume. The volume filter runs once every 8 samples.
	#
	####################################################################

#include <rsp_queue.inc>
#include <emux.inc>

.set noreorder
.set at

# Maximum number of channels supported by this ucode.
#define MAX_CHANNELS              32

# Activate one-tap filter on volume changes, to smooth out sudden changes
# that can cause clicks.
#define VOLUME_FILTER             1

# Max output samples that fit in ACCUM (stereo int16). Must match mixer.c.
#define MAX_SAMPLES_PER_ROUND     512

# Extra ACCUM slots the mono resampler is allowed to overshoot into. Sized so
# that ACCUM+padding is a whole number of 64-byte clear blocks.
#define ACCUM_PAD_SAMPLES         16
#define ACCUM_CLEAR_BLOCKS        (((MAX_SAMPLES_PER_ROUND + ACCUM_PAD_SAMPLES) * 4) / 64)

# Number of fractional bits used in the fixed point numbers that specify
# the waveform position, step, length and loop length.
# NOTE: This must be the same of MIXER_FX64_FRAC in mixer.c.
#define WAVEFORM_POS_FRAC_BITS    12

# Waveform flags. Keep these in sync with mixer.c
#define CH_FLAGS_BPS_SHIFT        (3<<0)
#define CH_FLAGS_16BIT            (1<<2)
#define CH_FLAGS_STEREO           (1<<3)
#define CH_FLAGS_STEREO_SUB       (1<<4)
#define CH_FLAGS_VADPCM           (1<<5)
# Zero ACCUM before mixing (first MIX_CHANNEL of a round; ACCUM is in .bss).
#define CH_FLAGS_CLEAR_ACCUM      (1<<7)

# Resample staging (8 samples) for the stereo and VADPCM paths.
#define DMEM_SCRATCH_SIZE         16

# Sample DMA cache (must be <= MIXER_LOOP_OVERREAD in mixer.c).
#define SAMPLE_CACHE_SIZE         64

# In-mixer VADPCM: one decoded frame in DMEM (16 samples). Enough for
# point resampling within the frame; the next frame is decoded on demand.
#define VADPCM_WIN_FRAMES         1
#define VADPCM_WIN_SAMPLES        (VADPCM_WIN_FRAMES * 16)


	################################
	# Global register allocations
	################################

	#define v_zero        $v00
	#define v_shift       $v30
	#define v_const1      $v31

	#define k_0000        v_zero
	#define k_alpha       v_const1.e1
	#define k_1malpha     v_const1.e2
	# k_half:  0.5 as a 0.16 unsigned fraction. Multiplying by it with vmudl
	#          is a logical shift right by one (vsrl cannot be used here: it
	#          needs the RSPQ shift table in $v31, which holds VCONST_1).
	# k_one:   integer one. vmudh by it loads a vector into the accumulator
	#          unshifted, to start a vmadm chain.
	# k_eight: integer eight, one group of the mono resampler.
	#define k_half        v_const1.e3
	#define k_one         v_const1.e4
	#define k_eight       v_const1.e5


	.data

	RSPQ_BeginOverlayHeader
		RSPQ_DefineCommand MIX_CHANNEL,       32		# 0x0
		RSPQ_DefineCommand MIX_SETCHANNEL,    16		# 0x1
		RSPQ_DefineCommand MIX_FLUSH,          8		# 0x2
	RSPQ_EndOverlayHeader

############################################################################

	.align 4
VCONST_1:
	.half 0x7FFF
	.half 0xe076      #   (0.9837**8) fixed 0.16
	.half 0x1f8a      # 1-(0.9837**8) fixed 0.16
	.half 0x8000      # 0.5 fixed 0.16
	.half 1           # integer one
	.half 8           # integer eight

	# Lane -> output sample mapping used by the mono resampler. The eight
	# results come out interleaved this way so that the .q0/.q1 swizzles
	# feed the LRLRLRLR layout of ACCUM without any further shuffling.
	.align 4
PERM8:
	.half 0, 4, 1, 5, 2, 6, 3, 7

	.align 4
BANNER0:    .ascii "Dragon RSP Audio"
BANNER1:    .ascii " Coded by Rasky "

	# ================================================================
	# Overlay saved state – survives RSPQ overlay swaps.
	# Layout must match mixer_overlay_state_t in mixer.c.
	# ================================================================
	RSPQ_BeginSavedState
	# One-tap volume filter state, [left:16][right:16] per channel. Kept
	# interleaved so it can be splatted into the [L,R,L,R,...] vector the
	# mixing loops use with a single word load.
	.align 4
XVOL:                     .dcb.l MAX_CHANNELS
	# Per-channel VADPCM pointers, written by MIX_SETCHANNEL. They only
	# change when a new waveform is played, so the CPU sends them once
	# instead of inlining them in every MIX_CHANNEL.
	.align 4
CHTBL_CODEBOOK:           .dcb.l MAX_CHANNELS
CHTBL_STATE:              .dcb.l MAX_CHANNELS
CHTBL_LOOP_STATE:         .dcb.l MAX_CHANNELS
	RSPQ_EndSavedState

	.align 4
IDENTITY:  .half  1<<11,1<<11,1<<11,1<<11, 1<<11,1<<11,1<<11,1<<11

	.bss

	# Stereo int16 accumulator for one mix round (LRLR...). Lives in .bss
	# (not saved state) — kept out of the ACCUM bank
	# bank. All MIX_CHANNEL + MIX_FLUSH of a round are enqueued in one
	# highpri burst of this overlay, so no overlay switch can clobber it
	# mid-round. Cleared by the first MIX_CHANNEL (CH_FLAGS_CLEAR_ACCUM)
	# and again by MIX_FLUSH after DMA-out.
	# The mono resampler emits whole groups of 8 samples, so it can write up
	# to 7 samples past the end of the round. ACCUM_PAD absorbs them; it is
	# never DMA'd out, but it is cleared together with ACCUM so that the
	# accumulation never saturates on leftovers.
	.align 4
ACCUM:                    .dcb.w (MAX_SAMPLES_PER_ROUND * 2)
ACCUM_PAD:                .dcb.w (ACCUM_PAD_SAMPLES * 2)

	# Temporary cache of samples fetched by DMA for resampling. Each cache is
	# followed by a window of four zero samples: the mono resampler parks the
	# lanes it must discard there, so they interpolate to silence instead of
	# needing a separate mask (MonoLoop). Both caches are 16-byte aligned
	# because they are written/read with sqv/lqv.
	.align 4
DMEM_SAMPLE_CACHE:        .dcb.b SAMPLE_CACHE_SIZE
DMEM_SAMPLE_CACHE_ZERO:   .dcb.b 16

	# 8-bit samples widened to int16 after each DMA, so that one inner loop
	# serves both PCM formats.
	.align 4
DMEM_SAMPLE_CACHE16:      .dcb.b SAMPLE_CACHE_SIZE * 2
DMEM_SAMPLE_CACHE16_ZERO: .dcb.b 16

	# Per-lane tap addresses produced by the mono resampler, and the
	# scratch used to transpose the eight gathered 4-tap groups.
	.align 4
DMEM_ADDRS:               .dcb.b 16
	.align 4
DMEM_TRANSPOSE:           .dcb.b 128

	# Contiguous resample staging (stereo and VADPCM paths).
	.align 4
DMEM_SCRATCH:             .dcb.b DMEM_SCRATCH_SIZE

	# In-mixer VADPCM staging (mono codebook = 8 vectors = 128 bytes).
	# Stereo VADPCM uses two MIX_CHANNEL commands, each with its own half.
	.align 4
MIX_VADPCM_CODEBOOK:      .space 128
	.align 4
MIX_VADPCM_STATE:         .space 16
	.align 4
MIX_VADPCM_STATE_BEFORE:  .space 16
	.align 3
MIX_VADPCM_COMP:          .space (VADPCM_WIN_FRAMES * 9 + 8)
	.align 4
MIX_VADPCM_PCM:           .space (VADPCM_WIN_SAMPLES * 2)

	# Scratch for the VADPCM catch-up path (wanted frame index).
	.align 3
ROUND_SCRATCH:            .long  0
	                      .long  0

	# Stashed MIX_CHANNEL parameters (command buffer may be overwritten).
	.align 2
CH_POS:                   .long  0
CH_STEP:                  .long  0
CH_LEN:                   .long  0
CH_LOOP_LEN:              .long  0
CH_PTR:                   .long  0
CH_CODEBOOK:              .long  0
CH_STATE:                 .long  0
CH_LOOP_STATE:            .long  0
CH_FLAGS:                 .half  0
CH_IDX:                   .half  0
CH_LVOL:                  .half  0
CH_RVOL:                  .half  0
CH_NSAMPLES:              .half  0
CH_ACC_OFF:               .half  0
# Frame index currently in MIX_VADPCM_PCM; -1 = none.
CH_VCUR_FRAME:            .long  0
# Frame index that MIX_VADPCM_STATE is ready to decode next.
CH_VNEXT_FRAME:           .long  0
CH_VADPCM_RA:             .long  0
CH_ACC_PTR:               .long  0
# Volume-filter step countdown, preserved across frame decodes.
CH_K1:                    .half  0
# Per-sample step of the mono resampler, split the same way as the position:
# fraction as 0.16, plus whole samples.
CH_STEPF:                 .half  0
CH_STEPI:                 .half  0
                          .half  0


	.text 1

###############################################################
# MIX_CHANNEL – resample one channel and accumulate into ACCUM.
#
# Command words (32 bytes):
#   a0: [ovl+cmd:8][ch_idx:8][flags:8][pad:8]   (24-bit payload)
#   a1: [lvol:16][rvol:16]
#   a2: pos   (fixed point, WAVEFORM_POS_FRAC_BITS)
#   a3: step
#   +16: len
#   +20: loop_len
#   +24: ptr  (0 = silence: only ramp volume toward target)
#   +28: [acc_offset:16][nsamples:16]
#
# VADPCM codebook/state/loop_state come from the per-channel table filled by
# MIX_SETCHANNEL.
#
# Positions for CH_FLAGS_VADPCM are in *samples* (not bytes). ptr points to
# compressed 9-byte frames; frame i starts at ptr + i*9.
#
# Stereo owner (CH_FLAGS_STEREO): ptr points to interleaved LR samples;
# L is extracted and mixed with (lvol, rvol). The stereo-sub channel is
# a separate MIX_CHANNEL with CH_FLAGS_STEREO_SUB that extracts R.
###############################################################

	#define ticks          t3
	#define wv_pos         t4
	#define wv_step        t5
	#define wv_len         t6
	#define wv_loop_len    t7
	#define wv_addr        t8
	#define dma_cache_end  s7
	#define out_ptr        s5
	#define wv_pos_to_dmem s6
	#define wv_step_8x     t2
	#define is_stereo_sub  a0
	#define is_16bit       a1
	#define is_stereo      v0
	#define acc_ptr        s8
	#define ch_idx         k0

	# Volumes, as [L,R,L,R,L,R,L,R] to match the ACCUM layout. $v24/$v25
	# are outside the range the VADPCM decoder uses, so they survive a
	# frame decode untouched.
	#define v_xvol         $v24
	#define v_chvol        $v25

	# Mono resampler state, live across MonoLoop iterations:
	#   v_frac  fraction of the sample position, 0.16 unsigned
	#   v_idx   DMEM index of tap y0, in units of 2 bytes
	#   v_kmono broadcast scalars of the loop, see k_* below
	#define v_frac         $v26
	#define v_idx          $v27
	#define v_kmono        $v29
	# k_limit  first tap address that would read past the cache
	# k_zaddr  address of the window of four zero samples
	# k_step8f 8-sample step, fractional part
	# k_step8i 8-sample step, whole samples
	#define k_limit        v_kmono.e0
	#define k_zaddr        v_kmono.e1
	#define k_step8f       v_kmono.e2
	#define k_step8i       v_kmono.e3
	# Transient within one MonoLoop iteration.
	#define v_addr         $v01
	#define v_x2           $v02
	#define v_x05          $v03
	#define v_x205         $v04
	#define v_x3           $v05
	#define v_x305         $v06
	#define v_res          $v07
	#define v_y0n          $v09
	#define v_y1n          $v10
	#define v_y2n          $v11
	#define v_y3n          $v12
	#define v_cnt          $v13
	#define v_acc0         $v14
	#define v_acc1         $v15
	#define v_y0           $v16
	#define v_y1           $v17
	#define v_y2           $v18
	#define v_y3           $v19
	#define v_tmp          $v20

	#define addr_ptr       v0
	#define tr_ptr         a0
	#define bps            a1
	#define cache_half     a2
	#define frac_shift     a3
	#define mask           v1
	#define k_ff           s5

	.func MIX_CHANNEL
MIX_CHANNEL:
	# Stash command parameters into DMEM.
	# a0 bits[23:16]=ch_idx, bits[15:8]=flags (bits[31:24]=ovl+cmd).
	srl t0, a0, 16
	andi t0, 0xFF                  # ch_idx
	sh t0, %lo(CH_IDX)
	srl t0, a0, 8
	andi t0, 0xFF                  # flags
	sh t0, %lo(CH_FLAGS)
	srl t0, a1, 16                 # lvol
	sh t0, %lo(CH_LVOL)
	andi t0, a1, 0xFFFF            # rvol
	sh t0, %lo(CH_RVOL)
	sw a2, %lo(CH_POS)
	sw a3, %lo(CH_STEP)
	lw t0, CMD_ADDR(0x10, 0x20)
	sw t0, %lo(CH_LEN)
	lw t0, CMD_ADDR(0x14, 0x20)
	sw t0, %lo(CH_LOOP_LEN)
	lw t0, CMD_ADDR(0x18, 0x20)
	sw t0, %lo(CH_PTR)
	lw t0, CMD_ADDR(0x1C, 0x20)
	sh t0, %lo(CH_NSAMPLES)
	srl t0, 16
	sh t0, %lo(CH_ACC_OFF)
	# VADPCM pointers: fetch this channel's entry from the table.
	lh t1, %lo(CH_IDX)
	sll t1, 2
	lw t0, %lo(CHTBL_CODEBOOK)(t1)
	sw t0, %lo(CH_CODEBOOK)
	lw t0, %lo(CHTBL_STATE)(t1)
	sw t0, %lo(CH_STATE)
	lw t0, %lo(CHTBL_LOOP_STATE)(t1)
	sw t0, %lo(CH_LOOP_STATE)
	li t0, -1
	sw t0, %lo(CH_VCUR_FRAME)

	li t0, %lo(VCONST_1)
	lqv v_const1, 0,t0
	vxor v_zero, v_zero, v_zero

	# Optionally clear ACCUM (first channel of a round).
	lh t0, %lo(CH_FLAGS)
	andi t0, CH_FLAGS_CLEAR_ACCUM
	beqz t0, MIX_CHANNEL_NoClear
	li s4, %lo(ACCUM)
	li t0, ACCUM_CLEAR_BLOCKS - 1
1:	sqv v_zero, 0x00,s4
	sqv v_zero, 0x10,s4
	sqv v_zero, 0x20,s4
	sqv v_zero, 0x30,s4
	addi s4, 64
	bnez t0, 1b
	addi t0, -1

MIX_CHANNEL_NoClear:
	# Splat the current filtered volume and the target volume of this
	# channel into [L,R,L,R,L,R,L,R] vectors. Both are stored as adjacent
	# halfword pairs in DMEM, so one word replicated four times does it.
	li s0, %lo(DMEM_SCRATCH)
	lh ch_idx, %lo(CH_IDX)
	sll t0, ch_idx, 2
	lw t1, %lo(XVOL)(t0)
	lw t2, %lo(CH_LVOL)             # CH_RVOL is the next halfword
	sw t1, 0x00(s0)
	sw t1, 0x04(s0)
	sw t1, 0x08(s0)
	sw t1, 0x0C(s0)
	lqv v_xvol, 0x00,s0
	sw t2, 0x00(s0)
	sw t2, 0x04(s0)
	sw t2, 0x08(s0)
	sw t2, 0x0C(s0)
	lqv v_chvol, 0x00,s0

	lh ticks, %lo(CH_NSAMPLES)
	beqz ticks, MIX_CHANNEL_Done

	# Accumulator write pointer: ACCUM + acc_offset * sizeof(stereo int16)
	li acc_ptr, %lo(ACCUM)
	lh t0, %lo(CH_ACC_OFF)
	sll t0, 2
	add acc_ptr, t0

	lw wv_addr, %lo(CH_PTR)
	beqz wv_addr, MIX_CHANNEL_Silence

	lw wv_pos, %lo(CH_POS)
	lw wv_step, %lo(CH_STEP)
	lw wv_len, %lo(CH_LEN)
	lw wv_loop_len, %lo(CH_LOOP_LEN)

	lh t0, %lo(CH_FLAGS)
	andi is_16bit, t0, CH_FLAGS_16BIT
	andi is_stereo, t0, CH_FLAGS_STEREO
	andi is_stereo_sub, t0, CH_FLAGS_STEREO_SUB
	andi t1, t0, CH_FLAGS_VADPCM
	bnez t1, VadpcmSetup
	or t1, is_stereo, is_stereo_sub
	beqz t1, MonoSetup
	nop

	li dma_cache_end, %lo(DMEM_SAMPLE_CACHE+SAMPLE_CACHE_SIZE)
	sll dma_cache_end, WAVEFORM_POS_FRAC_BITS

WaveStart:
	bltu wv_pos, wv_len, WaveDmaFetch
	nop
	beqz wv_loop_len, MIX_CHANNEL_SilenceLeft
	nop
	j WaveStart
	sub wv_pos, wv_loop_len

WaveDmaFetch:
	srl s2, wv_pos, WAVEFORM_POS_FRAC_BITS
	add s0, s2, wv_addr
	li s4, %lo(DMEM_SAMPLE_CACHE)
	jal DMAIn
	li t0, DMA_SIZE(SAMPLE_CACHE_SIZE, 1)

	sub wv_pos_to_dmem, s2, s4
	sll wv_pos_to_dmem, WAVEFORM_POS_FRAC_BITS
	sll wv_step_8x, wv_step, 3

	# Stereo and stereo-sub share the interleaved fetch; sub extracts R.
	sub wv_pos, wv_pos_to_dmem
	bnez is_16bit, WaveLoop16_Stereo_8x
	nop
	j WaveLoop8StereoChecks
	nop

	############################################################
	# Stereo 8-bit (owner extracts L, sub extracts R)
	############################################################

WaveLoop8Stereo:
	beqz is_stereo_sub, 1f
	nop
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t0, 1(t0)
	j 2f
	add wv_pos, wv_step
1:	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t0, 0(t0)
	add wv_pos, wv_step
2:	sll t0, 8
	mtc2 t0, $v01.e0
	jal MixOne
	nop
	addi ticks, -1
	addi acc_ptr, 4
WaveLoop8StereoChecks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop8Stereo
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem

	############################################################
	# Stereo 16-bit (owner extracts L, sub extracts R)
	############################################################

WaveLoop16_Stereo_8x:
	add t0, wv_pos, wv_step_8x
	bgt t0, dma_cache_end, WaveLoop16StereoChecks
	li t0, 8
	blt ticks, t0, WaveLoop16StereoChecks

	li out_ptr, %lo(DMEM_SCRATCH)
	li t1, 8
WaveLoop16StereoUnroll:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	beqz is_stereo_sub, 1f
	nop
	lhu t0, 2(t0)                  # R sample
	j 2f
	add wv_pos, wv_step
1:	lhu t0, 0(t0)                  # L sample
	add wv_pos, wv_step
2:	sh t0, 0(out_ptr)
	addi out_ptr, 2
	addi t1, -1
	bnez t1, WaveLoop16StereoUnroll
	nop

	jal MixBatch16bit
	nop
	addi ticks, -8
	j WaveLoop16_Stereo_8x
	addi acc_ptr, 8*4

WaveLoop16Stereo:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	beqz is_stereo_sub, 1f
	nop
	lhu t0, 2(t0)
	j 2f
	add wv_pos, wv_step
1:	lhu t0, 0(t0)
	add wv_pos, wv_step
2:	mtc2 t0, $v01.e0
	jal MixOne
	nop
	addi ticks, -1
	addi acc_ptr, 4
WaveLoop16StereoChecks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop16Stereo
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem

	############################################################
	# Mono 8/16-bit: 4-tap Catmull-Rom (Hermite) resampling
	#
	# Eight output samples are produced per iteration, one per vector lane.
	# Lane k carries output sample PERM8[k] = [0,4,1,5,2,6,3,7], so that the
	# .q0/.q1 swizzles of the interpolated vector line up with the LRLRLRLR
	# layout of ACCUM without any further shuffling.
	#
	# The sample position is split across two vectors: v_frac holds the
	# fraction as 0.16 and v_idx the DMEM index of the first tap, in units
	# of 2 bytes. Advancing eight samples is then vaddc+vadd, because the
	# carry out of the fraction is exactly the carry into the sample index.
	#
	# With taps y0..y3 and x the fraction, the spline is evaluated in the
	# central interval, between y1 and y2:
	#
	#   out = y1 + x*(y2-y0)/2
	#            + x^2*(y0 - 5*y1/2 + 2*y2 - y3/2)
	#            + x^3*(-y0/2 + 3*y1/2 - 3*y2/2 + y3/2)
	#
	# so the output lags the nominal position by one sample. That keeps the
	# tap window at [pos, pos+3] and no sample before pos is ever needed —
	# which matters because the sample cache is DMA'd starting exactly at
	# pos, and for a streamed waveform the samples before it may no longer
	# be in RDRAM.
	#
	# Lanes whose tap window runs past the end of the cache are parked on a
	# window of four zero samples: they interpolate to silence and, since
	# ACCUM is accumulated in place, leave it untouched. Being always the
	# last ones in output order, they are simply counted and replayed after
	# the next DMA.
	#
	# The mono path reuses the register names of the stereo dispatch for its
	# own purposes (see the #defines above); is_16bit/is_stereo are dead here.
	############################################################

MonoSetup:
	# A sample is always 2 bytes in DMEM: 8-bit waveforms are widened after
	# each DMA so that a single inner loop serves both PCM formats.
	#   bps         0 for 8-bit source samples, 1 for 16-bit
	#   cache_half  DMEM address of the int16 cache, in units of 2 bytes
	#   frac_shift  left shift turning wv_pos into a 0.16 sample fraction
	lh t0, %lo(CH_FLAGS)
	andi bps, t0, CH_FLAGS_BPS_SHIFT
	li cache_half, %lo(DMEM_SAMPLE_CACHE16)
	li t1, %lo(DMEM_SAMPLE_CACHE16_ZERO)
	li frac_shift, 4
	li t0, SAMPLE_CACHE_SIZE
	beqz bps, 1f
	nop
	li cache_half, %lo(DMEM_SAMPLE_CACHE)
	li t1, %lo(DMEM_SAMPLE_CACHE_ZERO)
	li frac_shift, 3
	srl t0, 1
1:
	# The window of four zero samples the rejected lanes are parked on. It
	# sits right after the cache, so it also fails the "tap window inside
	# the cache" test and a single comparison covers both cases.
	sw zero, 0(t1)
	sw zero, 4(t1)

	# Assemble the loop scalars in DMEM and splat them in one load.
	li s0, %lo(DMEM_SCRATCH)
	sh t1, 0x02(s0)                          # k_zaddr
	sll t0, 1                                # cache size in bytes
	add t0, cache_half
	addi t0, -3*2                            # y3 must stay inside the cache
	sh t0, 0x00(s0)                          # k_limit
	srl cache_half, 1                        # byte address -> DMEM index
	sll t2, wv_step, 3
	sllv t0, t2, frac_shift
	sh t0, 0x04(s0)                          # k_step8f
	srl t0, t2, WAVEFORM_POS_FRAC_BITS
	srlv t0, t0, bps
	sh t0, 0x06(s0)                          # k_step8i
	# Same split for one sample, needed to seed the eight lanes.
	sllv t0, wv_step, frac_shift
	sh t0, %lo(CH_STEPF)
	srl t0, wv_step, WAVEFORM_POS_FRAC_BITS
	srlv t0, t0, bps
	sh t0, %lo(CH_STEPI)
	lqv v_kmono, 0x00,s0

	li addr_ptr, %lo(DMEM_ADDRS)
	li tr_ptr, %lo(DMEM_TRANSPOSE)
	li k_ff, 0xFF

MonoWaveStart:
	bltu wv_pos, wv_len, MonoDmaFetch
	nop
	beqz wv_loop_len, MIX_CHANNEL_SilenceLeft
	nop
	j MonoWaveStart
	sub wv_pos, wv_loop_len

MonoDmaFetch:
	srl s2, wv_pos, WAVEFORM_POS_FRAC_BITS
	add s0, s2, wv_addr
	li s4, %lo(DMEM_SAMPLE_CACHE)
	andi s2, s0, 7                           # DMA rounds down to 8 bytes
	jal DMAIn
	li t0, DMA_SIZE(SAMPLE_CACHE_SIZE, 1)
	sll wv_step_8x, wv_step, 3               # DMAExec clobbers t2

	# Widen the 8-bit cache to int16: lpv shifts each byte into the high
	# half of a lane, which is exactly the <<8 scaling the mixer expects.
	bnez bps, MonoRefill
	li s1, %lo(DMEM_SAMPLE_CACHE)
	li s3, %lo(DMEM_SAMPLE_CACHE16)
	lpv $v08, 0x00,s1
	lpv $v09, 0x08,s1
	lpv $v10, 0x10,s1
	lpv $v11, 0x18,s1
	lpv $v12, 0x20,s1
	lpv $v13, 0x28,s1
	lpv $v14, 0x30,s1
	lpv $v15, 0x38,s1
	sqv $v08, 0x00,s3
	sqv $v09, 0x10,s3
	sqv $v10, 0x20,s3
	sqv $v11, 0x30,s3
	sqv $v12, 0x40,s3
	sqv $v13, 0x50,s3
	sqv $v14, 0x60,s3
	sqv $v15, 0x70,s3

MonoRefill:
	# Seed the eight lanes: lane k plays output sample PERM8[k], so it
	# starts at wv_pos + PERM8[k]*step. The whole-sample carry of that
	# product comes out of the accumulator, hence the vmudn/vsar pair.
	li t1, %lo(PERM8)
	lqv $v16, 0x00,t1
	lh t0, %lo(CH_STEPF)
	mtc2 t0, $v21.e0
	lh t0, %lo(CH_STEPI)
	mtc2 t0, $v22.e0
	srlv t0, s2, bps                         # DMA misalignment -> index of
	add t0, cache_half                       #   wv_pos inside the cache
	mtc2 t0, $v23.e0
	sllv t0, wv_pos, frac_shift              # fraction of wv_pos, 0.16
	mtc2 t0, $v20.e0
	# vmudn reads vs unsigned, so the step fraction must be the one splatted
	# into a full vector: it is a 0.16 value and can have bit 15 set.
	vor   $v18, v_zero, $v21.e0
	vmudn v_frac, $v18, $v16                 # PERM8*step: fraction...
	vsar  $v17, COP2_ACC_MD                  # ...and whole samples
	vmudh v_idx, $v16, $v22.e0
	vaddc v_frac, v_frac, $v20.e0
	vadd  v_idx, v_idx, $v17                 # + carry out of the fraction
	vadd  v_idx, v_idx, $v23.e0

	.balign 8
MonoLoop:
	# Reject the lanes whose 4-tap window is not fully inside the cache,
	# parking them on the zero window; VCC records the survivors.
	vsll  v_addr, v_idx,  1                  # tap index -> DMEM address
	vmudl v_x2,   v_frac, v_frac             # x^2
	vmudl v_x05,  v_frac, k_half             # x/2
	vlt   v_tmp,  v_addr, k_limit
	vmudl v_x3,   v_x2,   v_frac             # x^3
	vmrg  v_addr, v_addr, k_zaddr
	vmudl v_x205, v_x2,   k_half             # x^2/2
	vaddc v_frac, v_frac, k_step8f           # advance the eight positions:
	vadd  v_idx,  v_idx,  k_step8i           #   the carry crosses over
	vmudl v_x305, v_x3,   k_half             # x^3/2
	cfc2 mask, COP2_CTRL_VCC
	sqv v_addr, 0x00,addr_ptr

	# Gather the four taps of each output sample, then transpose the eight
	# 4-tap groups into four vectors holding one tap of all eight samples.
	lhu s0, 0x00(addr_ptr)
	lhu s1, 0x02(addr_ptr)
	lhu s2, 0x04(addr_ptr)
	lhu s3, 0x06(addr_ptr)
	lhu s4, 0x08(addr_ptr)
	lhu s6, 0x0A(addr_ptr)
	lhu s7, 0x0C(addr_ptr)
	lhu t0, 0x0E(addr_ptr)
	ldv $v08, 0x00,s0
	ldv $v09, 0x00,s1
	ldv $v10, 0x00,s2
	ldv $v11, 0x00,s3
	ldv $v12, 0x00,s4
	ldv $v13, 0x00,s6
	ldv $v14, 0x00,s7
	ldv $v15, 0x00,t0
	stv $v08.e0, 0x00,tr_ptr
	stv $v08.e1, 0x10,tr_ptr
	stv $v08.e2, 0x20,tr_ptr
	stv $v08.e3, 0x30,tr_ptr
	stv $v08.e4, 0x40,tr_ptr
	stv $v08.e5, 0x50,tr_ptr
	stv $v08.e6, 0x60,tr_ptr
	stv $v08.e7, 0x70,tr_ptr
	ltv $v16.e0, 0x00,tr_ptr
	ltv $v16.e7, 0x10,tr_ptr
	ltv $v16.e6, 0x20,tr_ptr
	ltv $v16.e5, 0x30,tr_ptr
	ltv $v16.e4, 0x40,tr_ptr
	ltv $v16.e3, 0x50,tr_ptr
	ltv $v16.e2, 0x60,tr_ptr
	ltv $v16.e1, 0x70,tr_ptr

	# ACCUM is accumulated in place. acc_ptr is only 4-byte aligned once a
	# partial group has been replayed, so the two quads are read and
	# written with the unaligned lqv/lrv and sqv/srv pairs.
	lqv v_acc0, 0x00,acc_ptr
	lrv v_acc0, 0x10,acc_ptr
	lqv v_acc1, 0x10,acc_ptr
	lrv v_acc1, 0x20,acc_ptr

	# Negated taps: the coefficients are fed to the accumulator one unit at
	# a time (vmadm saturates its output but not the accumulator), so the
	# subtractions of the polynomial are done on the taps instead.
	vsub v_y0n, v_zero, v_y0
	vsub v_y1n, v_zero, v_y1
	vsub v_y2n, v_zero, v_y2
	vsub v_y3n, v_zero, v_y3

	vmudh v_res, v_y1,  k_one                # y1
	vmadm v_res, v_y2,  v_x05                # + x  * (y2 - y0)/2
	vmadm v_res, v_y0n, v_x05
	vmadm v_res, v_y0,  v_x2                 # + x^2 * (y0 - 5*y1/2
	vmadm v_res, v_y1n, v_x2                 #          + 2*y2 - y3/2)
	vmadm v_res, v_y1n, v_x2
	vmadm v_res, v_y1n, v_x205
	vmadm v_res, v_y2,  v_x2
	vmadm v_res, v_y2,  v_x2
	vmadm v_res, v_y3n, v_x205
	vmadm v_res, v_y0n, v_x305               # + x^3 * (-y0/2 + 3*y1/2
	vmadm v_res, v_y1,  v_x3                 #          - 3*y2/2 + y3/2)
	vmadm v_res, v_y1,  v_x305
	vmadm v_res, v_y2n, v_x3
	vmadm v_res, v_y2n, v_x305
	vmadm v_res, v_y3,  v_x305

	# .q0/.q1 duplicate each result into the L and R slot of one ACCUM
	# entry, so the interleaved volume vector applies both gains at once.
	vmudh v_acc0, v_acc0, k_one
	vmacf v_acc0, v_xvol, v_res.q0
	vmudh v_acc1, v_acc1, k_one
	vmacf v_acc1, v_xvol, v_res.q1
#if VOLUME_FILTER
	vmudm v_xvol, v_xvol,  k_alpha
	vmadm v_xvol, v_chvol, k_1malpha
#endif
	sqv v_acc0, 0x00,acc_ptr
	srv v_acc0, 0x10,acc_ptr
	sqv v_acc1, 0x10,acc_ptr
	srv v_acc1, 0x20,acc_ptr

	addi acc_ptr, 8*4
	addi ticks, -8
	bne mask, k_ff, MonoPartial
	add wv_pos, wv_step_8x
	bgtz ticks, MonoLoop
	nop
	j MIX_CHANNEL_Done
	nop

MonoPartial:
	# Some lanes were parked: they contributed nothing to ACCUM, so give
	# their ticks, ACCUM slots and positions back and replay them once the
	# cache has been refilled.
	vmrg  v_cnt, v_zero, k_one               # 1 for each rejected lane
	vadd  v_cnt, v_cnt, v_cnt.q1
	vadd  v_cnt, v_cnt, v_cnt.h2
	vadd  v_cnt, v_cnt, v_cnt.e4
	mfc2  t0, v_cnt.e0
	add ticks, t0
	sll t1, t0, 2
	sub acc_ptr, t1
	beqz t0, 2f
	nop
1:	addi t0, -1
	bnez t0, 1b
	sub wv_pos, wv_step
2:	bgtz ticks, MonoWaveStart
	nop
	j MIX_CHANNEL_Done
	nop

############################################################
# Mono VADPCM path: decode one frame at a time into MIX_VADPCM_PCM,
# then point-resample like 16-bit PCM. State is written back at epilog.
############################################################

VadpcmSetup:
	# DMA codebook once per command.
	lw s0, %lo(CH_CODEBOOK)
	li s4, %lo(MIX_VADPCM_CODEBOOK)
	jal DMAInAsync
	li t0, DMA_SIZE(128, 1)

	# DMA decoder state. next_frame = floor(pos/16).
	lw s0, %lo(CH_STATE)
	li s4, %lo(MIX_VADPCM_STATE)
	jal DMAIn
	li t0, DMA_SIZE(16, 1)

	lw t0, %lo(CH_POS)
	srl t0, WAVEFORM_POS_FRAC_BITS
	srl t0, 4
	sw t0, %lo(CH_VNEXT_FRAME)
	li t0, -1
	sw t0, %lo(CH_VCUR_FRAME)
	li k1, 8                       # volume-filter step countdown (samples)
	# fall through

VadpcmWaveStart:
	bltu wv_pos, wv_len, VadpcmHavePos
	nop
	beqz wv_loop_len, VadpcmEpilog
	sub wv_pos, wv_loop_len
	# Cached-loop wrap: reload loop-start state and invalidate window.
	lw s0, %lo(CH_LOOP_STATE)
	beqz s0, 1f
	li s4, %lo(MIX_VADPCM_STATE)
	jal DMAIn
	li t0, DMA_SIZE(16, 1)
1:	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t0, 4
	sw t0, %lo(CH_VNEXT_FRAME)
	li t0, -1
	sw t0, %lo(CH_VCUR_FRAME)
	j VadpcmWaveStart
	nop

VadpcmHavePos:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS   # sample
	srl t1, t0, 4                            # frame needed
	lw t2, %lo(CH_VCUR_FRAME)
	beq t1, t2, VadpcmResample
	sw t1, %lo(ROUND_SCRATCH)                # wanted frame
	# A frame decode may clobber k1 (the volumes live in $v24/$v25, which
	# the decoder does not touch).
	sh k1, %lo(CH_K1)
VadpcmCatchUp:
	lw t2, %lo(CH_VNEXT_FRAME)
	lw t1, %lo(ROUND_SCRATCH)
	beq t2, t1, VadpcmDecodeCur
	nop
	slt t0, t1, t2
	bnez t0, VadpcmDecodeCur
	nop
	# Decode-discard one frame (large step skipped past it)
	sh ticks, %lo(CH_NSAMPLES)
	sw wv_pos, %lo(CH_POS)
	sw acc_ptr, %lo(CH_ACC_PTR)
	jal VadpcmDecodeFrame
	nop
	lh ticks, %lo(CH_NSAMPLES)
	lw wv_pos, %lo(CH_POS)
	lw acc_ptr, %lo(CH_ACC_PTR)
	lw wv_step, %lo(CH_STEP)
	lw wv_len, %lo(CH_LEN)
	lw wv_loop_len, %lo(CH_LOOP_LEN)
	j VadpcmCatchUp
	nop

VadpcmDecodeCur:
	# Snapshot state before decoding the frame we will resample from.
	li s0, %lo(MIX_VADPCM_STATE)
	li s1, %lo(MIX_VADPCM_STATE_BEFORE)
	lqv $v01, 0,s0
	sqv $v01, 0,s1
	sh ticks, %lo(CH_NSAMPLES)
	sw wv_pos, %lo(CH_POS)
	sw acc_ptr, %lo(CH_ACC_PTR)
	jal VadpcmDecodeFrame
	nop
	lh ticks, %lo(CH_NSAMPLES)
	lw wv_pos, %lo(CH_POS)
	lw acc_ptr, %lo(CH_ACC_PTR)
	lw wv_step, %lo(CH_STEP)
	lw wv_len, %lo(CH_LEN)
	lw wv_loop_len, %lo(CH_LOOP_LEN)
	lw t1, %lo(CH_VNEXT_FRAME)
	addi t1, -1                              # frame just decoded
	sw t1, %lo(CH_VCUR_FRAME)
	# Decode clobbers $v31 (vsra8 uses vshift8): reload mixer constants.
	li t0, %lo(VCONST_1)
	lqv v_const1, 0,t0
	lh k1, %lo(CH_K1)                      # resume volume-filter countdown
	# fall through to resample

VadpcmResample:
	blez ticks, VadpcmEpilog
	nop
	# Ensure the current sample's frame is loaded. Go through VadpcmWaveStart
	# so that a position that just ran past the end wraps the loop (a short
	# loop can be crossed several times within one round).
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t1, t0, 4
	lw t2, %lo(CH_VCUR_FRAME)
	bne t1, t2, VadpcmWaveStart
	nop

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	andi t0, 15
	sll t0, 1
	li s1, %lo(MIX_VADPCM_PCM)
	add t0, s1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	mtc2 t0, $v01.e0
	jal MixOne
	nop
#if VOLUME_FILTER
	# MixOne does not advance the volume filter. Apply the same 8-sample
	# coefficient the PCM MixBatch path uses, once every 8 output samples.
	addi k1, -1
	bgtz k1, 1f
	nop
	li k1, 8
	vmudm v_xvol, v_xvol,  k_alpha
	vmadm v_xvol, v_chvol, k_1malpha
1:
#endif
	addi ticks, -1
	j VadpcmResample
	addi acc_ptr, 4

VadpcmEpilog:
	# Write back state for frame at wv_pos. STATE_BEFORE = ready for
	# CH_VCUR_FRAME; STATE = ready for CH_VNEXT_FRAME.
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t0, 4
	lw t1, %lo(CH_VCUR_FRAME)
	li s4, %lo(MIX_VADPCM_STATE)
	bne t0, t1, 1f
	nop
	li s4, %lo(MIX_VADPCM_STATE_BEFORE)
1:	lw s0, %lo(CH_STATE)
	jal DMAOut
	li t0, DMA_SIZE(16, 1)
	blez ticks, MIX_CHANNEL_Done
	nop
	j MIX_CHANNEL_Silence
	nop

WaveBeforeEpilog:
	# If ticks remain (past end of non-looping sample), treat as silence
	# so volume still ramps.
MIX_CHANNEL_SilenceLeft:
	blez ticks, MIX_CHANNEL_Done
	nop
	# fall through to silence ramp for remaining ticks

	############################################################
	# Silence path: ramp volume toward target, advance acc_ptr
	############################################################
MIX_CHANNEL_Silence:
SilenceLoop:
	blez ticks, MIX_CHANNEL_Done
	li t0, 8
	blt ticks, t0, 1f
	nop
	li t0, 8
1:
#if VOLUME_FILTER
	# xvol = xvol*alpha + chvol*(1-alpha)
	vmudm v_xvol, v_xvol,  k_alpha
	vmadm v_xvol, v_chvol, k_1malpha
#endif
	sub ticks, t0
	sll t1, t0, 2                  # each sample = 4 bytes in ACCUM
	j SilenceLoop
	add acc_ptr, t1

MIX_CHANNEL_Done:
	# Store updated xvol for this channel
	lh ch_idx, %lo(CH_IDX)
	sll t0, ch_idx, 2
	mfc2 t1, v_xvol.e0
	mfc2 t2, v_xvol.e1
	sll t1, 16
	andi t2, 0xFFFF
	or t1, t2
	sw t1, %lo(XVOL)(t0)
	j RSPQ_Loop
	nop
	.endfunc


###############################################################
# MixBatch16bit – mix 8 mono int16 samples from DMEM_SCRATCH
# into ACCUM at acc_ptr, then update the volume filter.
#
# DMEM_SCRATCH holds: s0 s1 s2 s3 s4 s5 s6 s7 (halfwords)
# v_xvol / v_chvol = filtered / target volume, as [L,R,L,R,...]
###############################################################

	.func MixBatch16bit
MixBatch16bit:
	move ra2, ra
	li s0, %lo(DMEM_SCRATCH)
	lqv $v01, 0,s0                 # 8 samples
	lqv $v06, 0x00,acc_ptr
	lqv $v07, 0x10,acc_ptr

	# .q0/.q1 duplicate each sample into the L and R slot of one ACCUM
	# entry, so the interleaved volume vector applies both gains at once.
	vmudh $v06, $v06, k_one
	vmacf $v06, v_xvol, $v01.q0
	vmudh $v07, $v07, k_one
	vmacf $v07, v_xvol, $v01.q1
#if VOLUME_FILTER
	vmudm v_xvol, v_xvol,  k_alpha
	vmadm v_xvol, v_chvol, k_1malpha
#endif
	sqv $v06, 0x00,acc_ptr
	sqv $v07, 0x10,acc_ptr
	jr ra2
	nop
	.endfunc


###############################################################
# MixOne – mix a single sample in $v01.e0 into ACCUM at acc_ptr.
# Used for the <8-sample tail of a resampling batch. Does NOT
# update the volume filter (the next full batch or silence ramp
# will); tails are rare and short.
###############################################################

	.func MixOne
MixOne:
	lsv $v04.e0, 0,acc_ptr         # lane 0 = left, lane 1 = right
	lsv $v04.e1, 2,acc_ptr
	nop
	nop
	vmudh $v04, $v04, k_one
	vmacf $v04, v_xvol, $v01.e0
	ssv $v04.e0, 0,acc_ptr
	jr ra
	ssv $v04.e1, 2,acc_ptr
	.endfunc


###############################################################
# MIX_FLUSH – DMA ACCUM to RDRAM and clear ACCUM.
#
# a0: [nsamples:16][pad:16]
# a1: output RDRAM pointer (physical, 8-byte aligned)
#
# The DMA engine ignores the low 3 bits of both addresses, so ACCUM can only
# be sent as-is to an 8-byte aligned pointer. The CPU guarantees that: the
# output buffer is aligned and every round mixes an even number of stereo
# samples (see mixer_round_length).
###############################################################

	.func MIX_FLUSH
MIX_FLUSH:
	andi t4, a0, 0xFFFF            # nsamples
	beqz t4, RSPQ_Loop
	move s0, a1                    # output rdram

	sll t0, t4, 2
	addi t0, -1
	jal DMAOut
	li s4, %lo(ACCUM)

	li s4, %lo(ACCUM)
	li t0, ACCUM_CLEAR_BLOCKS - 1
	vxor v_zero, v_zero, v_zero
1:	sqv v_zero, 0x00,s4
	sqv v_zero, 0x10,s4
	sqv v_zero, 0x20,s4
	sqv v_zero, 0x30,s4
	addi s4, 64
	bnez t0, 1b
	addi t0, -1

	j RSPQ_Loop
	nop
	.endfunc


###############################################################
# MIX_SETCHANNEL – record the VADPCM pointers of one channel.
#
# Command words (16 bytes):
#   a0: [ovl+cmd:8][ch_idx:8][pad:16]
#   a1: codebook
#   a2: state       (VADPCM decoder state in RDRAM)
#   a3: loop_state  (VADPCM state at loop start; 0 if none)
###############################################################

	.func MIX_SETCHANNEL
MIX_SETCHANNEL:
	srl t0, a0, 16
	andi t0, 0xFF
	sll t0, 2
	sw a1, %lo(CHTBL_CODEBOOK)(t0)
	sw a2, %lo(CHTBL_STATE)(t0)
	j RSPQ_Loop
	sw a3, %lo(CHTBL_LOOP_STATE)(t0)
	.endfunc

	#undef ticks
	#undef wv_pos
	#undef wv_step
	#undef wv_len
	#undef wv_loop_len
	#undef wv_addr
	#undef dma_cache_end
	#undef out_ptr
	#undef wv_pos_to_dmem
	#undef wv_step_8x
	#undef is_stereo_sub
	#undef is_16bit
	#undef is_stereo
	#undef acc_ptr
	#undef ch_idx



############################################################################
# Mono VADPCM decode (MIX_CHANNEL / VadpcmDecodeFrame)
############################################################################

#define VADPCM_ORDER_SHIFT    1
#define VADPCM_ORDER          (1<<VADPCM_ORDER_SHIFT)

#define scaling    			t8
#define predictor  			t7
#define nframes    			t6
#define dmem_state          s1
#define dmem_codebook       s2
#define dmem_output         s3
#define dmem_input          s5
#define cb_xor              t2
#define slot_xor            t3
#define slot_ptr            t4

#define vscale  $v01
#define vstatef $v02
#define pred0   $v06
#define pred1   $v07
#define pred1a  $v08
#define pred1b  $v09
#define pred1c  $v10
#define pred1d  $v11
#define pred1e  $v12
#define pred1f  $v13
#define pred1g  $v14
#define pred1h  $v15
#define vres0    $v16
#define vres1    $v17
#define next_vres0 $v22
#define next_vres1 $v23
#define vstate0 $v18
#define vstate1 $v19
#define vstate2 $v20
#define vstate3 $v21
#define vresult $v03
#define v____   $v00

	.func VadpcmDecodeFrame
VadpcmDecodeFrame:
	sw ra, %lo(CH_VADPCM_RA)

	# MIX_CHANNEL keeps volume-filter constants in $v31 (v_const1), but
	# vsra8 needs the RSPQ shift table in vshift8 ($v31). Restore it the
	# same way RSPQ_Loop does before each command; caller reloads VCONST_1.
	lpv vshift8, 0x00,zero
	vsrl vshift, vshift8, 8

	# RDRAM address of compressed frame: ptr + next_frame*9
	lw t0, %lo(CH_VNEXT_FRAME)
	sll t1, t0, 3
	add t1, t0
	lw s0, %lo(CH_PTR)
	add s0, t1
	andi t9, s0, 7                        # misalignment (survives DMAIn)
	sub s0, t9
	li s4, %lo(MIX_VADPCM_COMP)
	jal DMAIn
	li t0, DMA_SIZE(9+8, 1)

	li dmem_codebook, %lo(MIX_VADPCM_CODEBOOK)
	li dmem_state, %lo(MIX_VADPCM_STATE)
	li dmem_output, %lo(MIX_VADPCM_PCM)
	li dmem_input, %lo(MIX_VADPCM_COMP)
	add dmem_input, t9
	addiu dmem_input, 1

	jal VADPCM_LoadIdentity
	nop

	li nframes, 1
	li a2, 0                               # mono path in SaveState
	lqv vstate1, 0x00,dmem_state
	li t1, 1
	j VADPCM_Priming
	nop
	.endfunc

	.func VADPCM_LoadIdentity
VADPCM_LoadIdentity:
	vxor pred1a, pred1a
	vxor pred1b, pred1b
	vxor pred1c, pred1c
	vxor pred1d, pred1d
	vxor pred1e, pred1e
	vxor pred1f, pred1f
	vxor pred1g, pred1g
	vxor pred1h, pred1h
	li s0, %lo(IDENTITY)
	jr ra
	ltv pred1a.e0, 0,s0
	.endfunc

VADPCM_Priming:
	lbu t0, -1(dmem_input)
	lpv next_vres0, 0,dmem_input
	lpv next_vres1, 0,dmem_input
	srl scaling, t0, 4
	andi predictor, t0, 0xF
	sllv scaling, t1, scaling
	mtc2 scaling, vscale.e0;					vsra8 next_vres0, next_vres0, 12
	sll predictor, VADPCM_ORDER_SHIFT + 4;      vsll  next_vres1, next_vres1, 4
	add predictor, dmem_codebook;				vsra8 next_vres1, next_vres1, 12

	lqv pred0.e0,  0x00,predictor
	lqv pred1.e0,  0x10,predictor
	lqv pred1a.e1, 0x10,predictor
	lqv pred1b.e2, 0x10,predictor
	lqv pred1c.e3, 0x10,predictor
	lqv pred1d.e4, 0x10,predictor
	lqv pred1e.e5, 0x10,predictor
	lqv pred1f.e6, 0x10,predictor
	lqv pred1g.e7, 0x10,predictor
	vmudh vres0, next_vres0, vscale.e0
	vmudh vres1, next_vres1, vscale.e0

	li slot_ptr, %lo(RSPQ_SCRATCH_MEM)
	lqv vresult, 0x00,dmem_state
	li slot_xor, 0
	li cb_xor, 0

	.balign 8
VADPCM_DecodeLoop:
	vmudh v____,  pred1a, vres0.e0;    sqv vresult, 0,slot_ptr
	vmadh v____,  pred1c, vres0.e1;    xor slot_ptr, slot_xor
	vmadh v____,  pred1e, vres0.e2;    sqv vresult, -16,dmem_output
	vmadh v____,  pred1g, vres0.e3;    lqv vstate1, 0,slot_ptr
	vmadh v____,  pred1b, vres1.e0;    srv vresult, 0x00,dmem_output
	vmadh v____,  pred1d, vres1.e1;    addiu dmem_input, 9
	vmadh v____,  pred1f, vres1.e2;    lbu t0, -1(dmem_input)
	vmadh v____,  pred1h, vres1.e3;    lpv next_vres0, 0,dmem_input
	vmadh v____,  pred0,  vstate1.e6;  lpv next_vres1, 0,dmem_input
	vmadh v____,  pred1,  vstate1.e7;  addiu nframes, -1

	vsar  vstatef, COP2_ACC_MD;        srl scaling, t0, 4
	vsar  vstate0, COP2_ACC_HI;        andi predictor, t0, 0xF
	vsra8 next_vres0, next_vres0, 12;  xor dmem_codebook, cb_xor
	vsll  next_vres1, next_vres1, 4;   sll predictor, VADPCM_ORDER_SHIFT + 4
	vmudn v____, vstatef, vshift.e2;   add predictor, dmem_codebook
	vmadh vstate0, vstate0, vshift.e2; sllv scaling, t1, scaling

	vmudh v____,  pred1a, vres0.e4;    mtc2 scaling, vscale.e0
	vmadh v____,  pred1b, vres1.e4;    lqv pred1a.e1, 0x10,predictor
	vmadh v____,  pred1c, vres0.e5;    lqv pred1b.e2, 0x10,predictor
	vmadh v____,  pred1d, vres1.e5;    lqv pred1c.e3, 0x10,predictor
	vmadh v____,  pred1e, vres0.e6;    lqv pred1d.e4, 0x10,predictor
	vmadh v____,  pred1f, vres1.e6;    lqv pred1e.e5, 0x10,predictor
	vmadh v____,  pred1g, vres0.e7;    lqv pred1f.e6, 0x10,predictor
	vmadh v____,  pred1h, vres1.e7;    lqv pred1g.e7, 0x10,predictor
	vmadh v____,  pred0,  vstate0.e6
	vmadh v____,  pred1,  vstate0.e7

	vsar  vstatef, COP2_ACC_MD;         sqv vstate0, 0x00,dmem_output
	vsar  vresult, COP2_ACC_HI;         srv vstate0, 0x10,dmem_output
	vsra8 next_vres1, next_vres1, 12;   lqv pred0.e0, 0x00,predictor
	vmudh vres0, next_vres0, vscale.e0; lqv pred1.e0, 0x10,predictor
	vmudn v____, vstatef, vshift.e2;    addiu dmem_output, 32
	vmadh vresult, vresult, vshift.e2;  bgtz nframes, VADPCM_DecodeLoop
	 vmudh vres1, next_vres1, vscale.e0

	sqv vresult, -16,dmem_output
	srv vresult, 0x00,dmem_output
	sqv vresult, 0,slot_ptr

	li slot_ptr, %lo(RSPQ_SCRATCH_MEM)
	lqv vstate1, 0,slot_ptr

VADPCM_SaveState:
	sqv vstate1, 0x00,dmem_state
	lw t0, %lo(CH_VNEXT_FRAME)
	addi t0, 1
	sw t0, %lo(CH_VNEXT_FRAME)
	lw ra, %lo(CH_VADPCM_RA)
	jr ra
	nop
