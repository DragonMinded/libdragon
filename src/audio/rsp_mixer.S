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
	# PCM and VADPCM resampling is 4-tap Catmull-Rom (Hermite) with a
	# fixed-point step (WAVEFORM_POS_FRAC_BITS). Stereo PCM is two MIX_CHANNELs
	# (owner=L, STEREO_SUB=R) that share the same interleaved cache and loop
	# body as mono; only the gather differs. VADPCM frames are DMA'd compressed,
	# decoded into DMEM_SAMPLE_CACHE, then Hermite-resampled by that same loop.
	# Stereo VADPCM is two independent mono MIX_CHANNELs. The volume filter
	# runs once every 8 samples.
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

# Sample DMA cache. A transfer may never end more than SAMPLE_CACHE_SIZE bytes
# past the end of the RDRAM window (that is #MIXER_LOOP_OVERREAD in mixer.c,
# the only part of the overread that is guaranteed to hold valid samples), but
# it may well be larger than that when the window is far from its end. The
# mono resampler exploits this to fetch SAMPLE_CACHE_SAMPLES samples at a
# time, whatever the PCM format: refilling costs about as much as mixing a
# cacheful, so it is worth spending most of the spare DMEM on the window.
#define SAMPLE_CACHE_SIZE         64
#define SAMPLE_CACHE_SAMPLES      128

# In-mixer VADPCM: decode up to this many frames into DMEM_SAMPLE_CACHE
# (16 samples each). 8 frames = SAMPLE_CACHE_SAMPLES.
#define VADPCM_WIN_FRAMES         8
#define VADPCM_WIN_SAMPLES        (VADPCM_WIN_FRAMES * 16)


	################################
	# Global register allocations
	################################

	# $v00 is zeroed once per command and must stay zero for the whole of it:
	# it is the vs operand of the vor/vsub that build the splats of the mono
	# resampler, and vs cannot be swizzled, so no element of another vector
	# can stand in for it. Nothing else in this file may write to it, VADPCM
	# decoder included (see v____).
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
	# ACCUM_PRE is the mirror image of that: entering the mono resampler
	# costs one pass over the group before the first real one. The pass only
	# adds zero, but it does read and write the slots, which on the very
	# first group are the ones right before ACCUM.
	.align 4
ACCUM_PRE:                .dcb.w (8 * 2)
ACCUM:                    .dcb.w (MAX_SAMPLES_PER_ROUND * 2)
ACCUM_PAD:                .dcb.w (ACCUM_PAD_SAMPLES * 2)

	# Temporary cache of samples fetched by DMA for resampling.
	#
	# The mono resampler sees it as a window of SAMPLE_CACHE_SAMPLES int16
	# samples: 8-bit waveforms are DMA'd into its upper half and widened in
	# place to fill it, so that one inner loop serves both PCM formats.
	# VADPCM decodes directly into this window (up to VADPCM_WIN_FRAMES).
	#
	# The window is followed by four zero samples: the mono resampler parks
	# the lanes it must discard there, so they interpolate to silence
	# instead of needing a separate mask (MonoLoop). It is 16-byte aligned
	# because it is written/read with sqv/lqv.
	#
	# The pipelined VADPCM decoder spills one stale vector 16 bytes below the
	# window before its first real store: that lands in the tail of ACCUM_PAD,
	# which is never DMA'd out.
	.align 4
DMEM_SAMPLE_CACHE:        .dcb.b SAMPLE_CACHE_SAMPLES * 2
DMEM_SAMPLE_CACHE_ZERO:   .dcb.b 16

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
	# Compressed frames of one fill, plus room for the 8-byte lookahead of
	# the decoder priming and for the DMA rounding up to 8 bytes.
	.align 3
MIX_VADPCM_COMP:          .space (VADPCM_WIN_FRAMES * 9 + 24)

	# Frame index the current fill starts at.
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
# First frame currently decoded into DMEM_SAMPLE_CACHE; -1 = none.
CH_VCUR_FRAME:            .long  0
# Frame index that MIX_VADPCM_STATE is ready to decode next.
CH_VNEXT_FRAME:           .long  0
CH_VADPCM_RA:             .long  0
# Frames of the current fill, and frames the next decode call will produce
# (they differ while the decoder is catching up to the start of the fill).
CH_VNFRAMES:              .half  0
CH_VDECN:                 .half  0
# k_limit of the current fill, computed before the decode clobbers it.
CH_VLIMIT:                .half  0
# Per-sample step of the mono resampler, split the same way as the position:
# fraction as 0.16, plus whole samples.
CH_STEPF:                 .half  0
CH_STEPI:                 .half  0
                          .half  0
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
# compressed 9-byte frames; frame i starts at ptr + i*9. VADPCM is decoded
# into DMEM_SAMPLE_CACHE and Hermite-resampled like PCM.
#
# Stereo PCM owner (CH_FLAGS_STEREO): ptr points to interleaved LR samples;
# L is extracted and mixed with (lvol, rvol). The stereo-sub channel is
# a separate MIX_CHANNEL with CH_FLAGS_STEREO_SUB that extracts R.
# Stereo VADPCM is two mono MIX_CHANNELs (STEREO bit ignored by the ucode).
###############################################################

	#define ticks          t3
	#define wv_pos         t4
	#define wv_step        t5
	#define wv_len         t6
	#define wv_loop_len    t7
	#define wv_addr        t8
	#define wv_step_8x     t2
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

	# Registers of one MonoLoop iteration. The powers of x are the only
	# values that must survive from one iteration to the next (they are
	# computed for the group that the next iteration interpolates), so they
	# are kept away from $v08-$v23, which the gather and the transpose
	# rewrite wholesale.
	#
	# The taps sit in the transpose destination bank (v16-v23): after the
	# gather + transpose, y0..y3 of the plane being mixed are in v16..v19.
	# Negated taps live in v20..v23 (the half of the bank the 4-tap gather
	# leaves unused).
	#define v_x2           $v01
	#define v_x05          $v02
	#define v_x205         $v03
	#define v_x3           $v04
	#define v_x305         $v05
	#define v_res          $v06
	#define v_acc0         $v07
	#define v_acc1         $v28
	#define v_addr         $v07     /* dead by the time v_acc0 is loaded */
	#define v_tmp          $v15     /* vlt result, only VCC is of interest */
	#define v_y0           $v16
	#define v_y1           $v17
	#define v_y2           $v18
	#define v_y3           $v19
	#define v_y0n          $v20
	#define v_y1n          $v21
	#define v_y2n          $v22
	#define v_y3n          $v23
	#define v_cnt          $v20

	#define addr_ptr       v0
	#define tr_ptr         a0
	#define bps            a1
	#define cache_half     a2
	#define frac_shift     a3
	#define mask           v1        /* mask of the group being mixed */
	#define k_ff           s5
	#define partial        t9        /* < 0 once a group has been rejected */
	# stereo_shift lives in k1 (free on the PCM path; VADPCM uses it as the
	# volume-filter countdown). is16 / stereo_sub are reloaded from CH_FLAGS
	# at the few sites that need them — they must not alias wv_len / wv_addr.
	#define stereo_shift   k1        /* 0 = mono, 1 = stereo: v_idx unit is a
	                                    sample (2 B) or a frame (4 B) */

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
	andi t1, t0, CH_FLAGS_VADPCM
	bnez t1, VadpcmSetup
	nop
	# PCM mono and stereo share the Hermite path below.

	############################################################
	# PCM 8/16-bit mono and stereo: 4-tap Catmull-Rom (Hermite)
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
	# Stereo PCM is interleaved in the cache as [L,R,L,R,...]. The gather
	# loads the four taps of the plane this MIX_CHANNEL owns with lsv
	# (owner = L at +0/+4/+8/+12, STEREO_SUB = R at +2/+6/+10/+14), so the
	# transpose lands on the same y0..y3 layout the mono path uses. The
	# owner/sub volume split is unchanged.
	#
	# Lanes whose tap window runs past the end of the cache are parked on a
	# window of four zero samples: they interpolate to silence and, since
	# ACCUM is accumulated in place, leave it untouched. Being always the
	# last ones in output order, they are simply counted and replayed after
	# the next DMA.
	#
	# The loop is software pipelined over two stages: each iteration gathers
	# the taps of one group while interpolating and mixing the group gathered
	# by the previous one. That is what allows the 32 memory accesses of the
	# gather to run in the shadow of the 36 vector operations of the spline.
	# Consequently the powers of x and the mask of accepted lanes are
	# computed one iteration ahead of the group they belong to, and the loop
	# has to be entered through MonoPrime, which fills the first stage.
	############################################################

MonoSetup:
	# A sample (mono) or stereo frame is always stored as int16 in DMEM:
	# 8-bit waveforms are widened after each DMA so that a single inner
	# loop serves every PCM format.
	#   bps           log2(bytes per sample/frame) in the source waveform
	#   stereo_shift  0 mono / 1 stereo (halfwords per source unit)
	#   cache_half    DMEM address of the int16 cache, in units of 2 bytes
	#   frac_shift    left shift turning wv_pos into a 0.16 unit fraction
	lh t0, %lo(CH_FLAGS)
	andi bps, t0, CH_FLAGS_BPS_SHIFT
	# A VADPCM command decodes one plane into an int16 cache, so its units are
	# mono samples however the STEREO bit (which the owner still carries) reads.
	andi stereo_shift, t0, CH_FLAGS_STEREO
	andi t1, t0, CH_FLAGS_VADPCM
	srl t1, 2                                # VADPCM bit -> STEREO bit
	nor t1, t1, zero
	and stereo_shift, t1
	srl stereo_shift, 3                      # bit 3 → 0 or 1
	li cache_half, %lo(DMEM_SAMPLE_CACHE)
	li t1, %lo(DMEM_SAMPLE_CACHE_ZERO)
	li frac_shift, 4
	sub frac_shift, bps

	# The window of four zero samples (mono) / four zero frames (stereo)
	# the rejected lanes are parked on. It sits right after the cache, so
	# it also fails the "tap window inside the cache" test and a single
	# comparison covers both cases. 16 bytes is exactly one lqv.
	sw zero, 0(t1)
	sw zero, 4(t1)
	sw zero, 8(t1)
	sw zero, 12(t1)

	# Assemble the loop scalars in DMEM and splat them in one load. k_limit
	# depends on the size of each transfer, so it is filled in by
	# MonoDmaFetch before the loop ever runs.
	li s0, %lo(DMEM_SCRATCH)
	sh t1, 0x02(s0)                          # k_zaddr
	srl cache_half, 1                        # byte address -> DMEM index
	sll t2, wv_step, 3
	sllv t0, t2, frac_shift
	sh t0, 0x04(s0)                          # k_step8f
	srl t0, t2, WAVEFORM_POS_FRAC_BITS
	srlv t0, t0, bps                         # samples / frames (v_idx units)
	sh t0, 0x06(s0)                          # k_step8i
	# Same split for one sample/frame, needed to seed the eight lanes.
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
	# VADPCM refills by decoding into the sample cache, not by DMA of PCM.
	lh t0, %lo(CH_FLAGS)
	andi t0, CH_FLAGS_VADPCM
	bnez t0, VadpcmWaveStart
	nop
	bltu wv_pos, wv_len, MonoDmaFetch
	nop
	beqz wv_loop_len, MIX_CHANNEL_SilenceLeft
	nop
	j MonoWaveStart
	sub wv_pos, wv_loop_len

MonoDmaFetch:
	# Fetch as many source units as fill the int16 window (SAMPLE_CACHE_SAMPLES
	# halfwords: that is SAMPLE_CACHE_SAMPLES samples mono, or half that many
	# stereo frames), unless the transfer would end further than
	# SAMPLE_CACHE_SIZE bytes past the RDRAM window: beyond that point the
	# waveform is not guaranteed to continue.
	srl s2, wv_pos, WAVEFORM_POS_FRAC_BITS   # byte offset of the position
	srl s1, wv_len, WAVEFORM_POS_FRAC_BITS
	sub s1, s2                               # bytes left in the window,
	addi s1, SAMPLE_CACHE_SIZE               #   plus the guaranteed overread
	li t0, SAMPLE_CACHE_SAMPLES
	srlv t0, t0, stereo_shift                # samples / frames that fit
	sllv t0, t0, bps                         # ...as a transfer size
	sltu s3, s1, t0
	beqz s3, 1f
	nop
	move t0, s1
1:
	# The tap window of the last accepted lane must stay within the units
	# that were really fetched. v_idx is in samples (mono) or frames
	# (stereo); the byte address is that times 2<<stereo_shift.
	srlv t1, t0, bps                         # samples / frames in the cache
	sll t1, 1                                # → bytes if mono
	sllv t1, t1, stereo_shift                # → bytes if stereo (×4 per frame)
	sll s3, cache_half, 1                    # cache base, back to a byte address
	add t1, s3
	li s3, 3*2
	sllv s3, s3, stereo_shift                # y3 must stay inside the cache
	sub t1, s3
	mtc2 t1, k_limit

	# 8-bit waveforms are DMA'd into the upper half of the window and
	# widened downwards into the whole of it.
	add s0, s2, wv_addr
	li s4, %lo(DMEM_SAMPLE_CACHE)
	lh t1, %lo(CH_FLAGS)
	andi t1, CH_FLAGS_16BIT
	bnez t1, 1f
	andi s2, s0, 7                           # DMA rounds down to 8 bytes
	addi s4, SAMPLE_CACHE_SAMPLES            # = half of the window, in bytes
1:	jal DMAIn
	addi t0, -1                              # = DMA_SIZE(t0, 1)
	sll wv_step_8x, wv_step, 3               # DMAExec clobbers t2

	# Widen the 8-bit cache to int16: lpv shifts each byte into the high
	# half of a lane, which is exactly the <<8 scaling the mixer expects.
	# The first half is widened first, so that its source (which is the
	# second half of the destination) is read before being overwritten.
	lh t1, %lo(CH_FLAGS)
	andi t1, CH_FLAGS_16BIT
	bnez t1, MonoRefill
	li s1, %lo(DMEM_SAMPLE_CACHE+SAMPLE_CACHE_SAMPLES)
	li s3, %lo(DMEM_SAMPLE_CACHE)
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
	lpv $v08, 0x40,s1
	lpv $v09, 0x48,s1
	lpv $v10, 0x50,s1
	lpv $v11, 0x58,s1
	lpv $v12, 0x60,s1
	lpv $v13, 0x68,s1
	lpv $v14, 0x70,s1
	lpv $v15, 0x78,s1
	sqv $v08, 0x00,s1
	sqv $v09, 0x10,s1
	sqv $v10, 0x20,s1
	sqv $v11, 0x30,s1
	sqv $v12, 0x40,s1
	sqv $v13, 0x50,s1
	sqv $v14, 0x60,s1
	sqv $v15, 0x70,s1

MonoRefill:
	# Seed the eight lanes: lane k plays output sample PERM8[k], so it
	# starts at wv_pos + PERM8[k]*step. The whole-sample carry of that
	# product comes out of the accumulator, hence the vmudn/vsar pair.
	li t1, %lo(PERM8)
	lh t0, %lo(CH_STEPF)
	lh s0, %lo(CH_STEPI)
	lqv $v16, 0x00,t1
	# v_idx is in samples (mono) or frames (stereo). The cache base in those
	# units is cache_half >> stereo_shift (halfwords vs halfwords/2).
	srlv s1, s2, bps                         # DMA misalignment -> v_idx units
	srlv s3, cache_half, stereo_shift
	mtc2 t0, $v21.e0
	add s1, s3
	sllv t0, wv_pos, frac_shift              # fraction of wv_pos, 0.16
	mtc2 s0, $v22.e0
	mtc2 s1, $v23.e0
	# vmudn reads vs unsigned, so the step fraction must be the one splatted
	# into a full vector: it is a 0.16 value and can have bit 15 set.
	vor   $v18, v_zero, $v21.e0
	mtc2 t0, $v20.e0
	vmudn v_frac, $v18, $v16                 # PERM8*step: fraction...
	vsar  $v17, COP2_ACC_MD                  # ...and whole samples
	vmudh v_idx, $v16, $v22.e0
	# The priming pass below interpolates a group of zero taps, so zero them
	# here, in the shadow of the seeding.
	vxor  $v19, $v19, $v19
	vxor  $v18, $v18, $v18
	vaddc v_frac, v_frac, $v20.e0
	vadd  v_idx, v_idx, $v17                 # + carry out of the fraction
	vxor  $v17, $v17, $v17
	vxor  $v16, $v16, $v16
	vadd  v_idx, v_idx, $v23.e0

MonoPrime:
	# Fill the first stage of the pipeline: gather the first group of the
	# new cache without mixing anything. This is the first half of the loop
	# body, minus the transpose read-back, and it is lent the group of ACCUM
	# slots that the loop advances past at its end. The zero taps make the
	# pass it makes over them add nothing.
	beqz  stereo_shift, 1f
	li    mask, 0xFF                         # nothing to give back yet
	vsll  v_addr, v_idx, 2                   # stereo: frames → bytes
	j     2f
	addi  acc_ptr, -8*4
1:	vsll  v_addr, v_idx, 1                   # mono: samples → bytes
	addi  acc_ptr, -8*4
2:	vlt   v_tmp,  v_addr, k_limit
	vmrg  v_addr, v_addr, k_zaddr
	j     MonoLoopGather
	sqv   v_addr, 0x00,addr_ptr

	.balign 8
MonoLoop:
	# Mono: ~50 cycles/iter (8 samples) → 6.3 cyc/sample including the
	# stereo_shift test. Stereo: a few more for the lsv gather; a stereo
	# waveform is two MIX_CHANNEL commands (owner=L, STEREO_SUB=R).
	#
	# v_idx is in samples (mono) or frames (stereo); byte address ×2 or ×4.
	beqz  stereo_shift, MonoLoopMono
	nop
	vsll  v_addr, v_idx, 2
	j     MonoLoopCommon
	nop

	.balign 8
MonoLoopMono:
	vsll  v_addr, v_idx, 1
MonoLoopCommon:
	# Read back the taps gathered by the previous iteration, one tap of all
	# eight output samples per register, while the DMEM addresses of the
	# group gathered by this one are computed. Lanes whose 4-tap window is
	# not fully inside the cache are parked on the zero window; VCC records
	# the survivors and is left untouched until the bottom of the body,
	# where it becomes the mask of the group the next iteration will mix.
	#
	# The eight ltv leave the SU no room for anything else, so this is where
	# the vector operations that depend on neither the taps nor the powers
	# of x are parked.
	                                         ltv $v16.e0, 0x00,tr_ptr
	                                         ltv $v16.e7, 0x10,tr_ptr
	                                         ltv $v16.e6, 0x20,tr_ptr
	                                         ltv $v16.e5, 0x30,tr_ptr
	vlt   v_tmp,  v_addr, k_limit;           ltv $v16.e4, 0x40,tr_ptr
	vmrg  v_addr, v_addr, k_zaddr;           ltv $v16.e3, 0x50,tr_ptr
#if VOLUME_FILTER
	vmudm v_xvol, v_xvol,  k_alpha;          ltv $v16.e2, 0x60,tr_ptr
	vmadm v_xvol, v_chvol, k_1malpha;        ltv $v16.e1, 0x70,tr_ptr
#else
	                                         ltv $v16.e2, 0x60,tr_ptr
	                                         ltv $v16.e1, 0x70,tr_ptr
#endif
	                                         add wv_pos, wv_step_8x
	                                         addi ticks, -8
	                                         sqv v_addr, 0x00,addr_ptr

MonoLoopGather:
	# Negated taps: the coefficients are fed to the accumulator one unit    # Gather the taps of each of
	# at a time (vmadm saturates its output but not the accumulator), so    # the eight output samples of
	# the subtractions of the polynomial are done on the taps instead.      # the next group.
	#
	# Mono uses ldv (8 bytes = 4 contiguous taps). Stereo taps of one plane
	# are every other halfword, so four lsv per lane build the same e0..e3
	# layout; vector loads wrap inside a 16-byte DMEM line, and a lone lsv
	# of an aligned halfword never wraps.
	vsub  v_y0n, v_zero, v_y0;               lhu s0, 0x00(addr_ptr)
	vsub  v_y1n, v_zero, v_y1;               lhu s1, 0x02(addr_ptr)
	vsub  v_y2n, v_zero, v_y2;               lhu s2, 0x04(addr_ptr)
	vsub  v_y3n, v_zero, v_y3;               lhu s3, 0x06(addr_ptr)
	vmudh v_res, v_y1,  k_one;               lhu s4, 0x08(addr_ptr)     # y1
	vmadm v_res, v_y2,  v_x05;               lhu s6, 0x0A(addr_ptr)     # + x * (y2 - y0)/2
	vmadm v_res, v_y0n, v_x05;               lhu s7, 0x0C(addr_ptr)
	vmadm v_res, v_y0,  v_x2;                lhu t0, 0x0E(addr_ptr)     # + x^2 * (y0 - 5*y1/2
	bnez  stereo_shift, MonoGatherStereo
	nop
	vmadm v_res, v_y1n, v_x2;                ldv $v08, 0x00,s0          #          + 2*y2 - y3/2)
	vmadm v_res, v_y1n, v_x2;                ldv $v09, 0x00,s1
	vmadm v_res, v_y1n, v_x205;              ldv $v10, 0x00,s2
	vmadm v_res, v_y2,  v_x2;                ldv $v11, 0x00,s3
	vmadm v_res, v_y2,  v_x2;                ldv $v12, 0x00,s4
	vmadm v_res, v_y3n, v_x205;              ldv $v13, 0x00,s6
	vmadm v_res, v_y0n, v_x305;              ldv $v14, 0x00,s7          # + x^3 * (-y0/2 + 3*y1/2
	vmadm v_res, v_y1,  v_x3;                ldv $v15, 0x00,t0          #          - 3*y2/2 + y3/2)

	# ACCUM is accumulated in place. acc_ptr is only 4-byte aligned once a
	# partial group has been replayed, so the two quads are read and
	# written with the unaligned lqv/lrv and sqv/srv pairs. The stv can only
	# start four cycles after the last gather load anyway.
	vmadm v_res, v_y1,  v_x305;              lqv v_acc0, 0x00,acc_ptr
	vmadm v_res, v_y2n, v_x3;                lrv v_acc0, 0x10,acc_ptr
	vmadm v_res, v_y2n, v_x305;              lqv v_acc1, 0x10,acc_ptr
	vmadm v_res, v_y3,  v_x305;              lrv v_acc1, 0x20,acc_ptr

MonoGatherMix:

	# One test for both exits: the ticks left, forced negative if any lane
	# of the group just mixed was rejected. It is computed here, and not at
	# the bottom, because a store stalls when it lands exactly two cycles
	# after a load: these two are what keeps the stv block clear of the
	# vector loads above.
	vmudl v_x05,  v_frac, k_half;            sub partial, mask, k_ff    # powers of x of the group being
	vmudl v_x2,   v_frac, v_frac;            or s0, partial, ticks      #   gathered, for the next iteration

	# .q0/.q1 duplicate each result into the L and R slot of one ACCUM      # ...and spill them, so that they
	# entry, so the interleaved volume vector applies both gains at once.   # can be read back transposed.
	vmudh v_acc0, v_acc0, k_one;             stv $v08.e0, 0x00,tr_ptr
	vmacf v_acc0, v_xvol, v_res.q0;          stv $v08.e1, 0x10,tr_ptr
	vmudh v_acc1, v_acc1, k_one;             stv $v08.e2, 0x20,tr_ptr
	vmacf v_acc1, v_xvol, v_res.q1;          stv $v08.e3, 0x30,tr_ptr
	vmudl v_x305, v_x2,   v_x05;             stv $v08.e4, 0x40,tr_ptr   # x^3/2 = x^2 * (x/2)
	vmudl v_x3,   v_x2,   v_frac;            stv $v08.e5, 0x50,tr_ptr   # x^3
	vmudl v_x205, v_x2,   k_half;            stv $v08.e6, 0x60,tr_ptr   # x^2/2
	                                         stv $v08.e7, 0x70,tr_ptr
	                                         sqv v_acc0, 0x00,acc_ptr
	vaddc v_frac, v_frac, k_step8f;          srv v_acc0, 0x10,acc_ptr   # advance the eight positions:
	vadd  v_idx,  v_idx,  k_step8i;          sqv v_acc1, 0x10,acc_ptr   #   the carry crosses over
	                                         srv v_acc1, 0x20,acc_ptr

	# VCC still holds the verdict of the vlt at the top: read it only now
	# that "partial" has consumed the mask of the group just mixed. It is
	# also the one spot where the cfc2, which counts as both a load and a
	# store, has neither two cycles before nor after it.
	                                         cfc2 mask, COP2_CTRL_VCC
	bgtz s0, MonoLoop
	addi acc_ptr, 8*4
	bltz partial, MonoPartial
	nop
	j MonoExit
	nop

MonoGatherStereo:
	# Build y0..y3 of this plane into e0..e3 of each gather register, matching
	# the mono ldv layout. STEREO_SUB starts one halfword later (the R sample).
	lh    t1, %lo(CH_FLAGS)
	andi  t1, CH_FLAGS_STEREO_SUB
	beqz  t1, 1f
	nop
	addi  s0, 2
	addi  s1, 2
	addi  s2, 2
	addi  s3, 2
	addi  s4, 2
	addi  s6, 2
	addi  s7, 2
	addi  t0, 2
1:	vmadm v_res, v_y1n, v_x2;                lsv $v08.e0, 0x00,s0
	vmadm v_res, v_y1n, v_x2;                lsv $v08.e1, 0x04,s0
	vmadm v_res, v_y1n, v_x205;              lsv $v08.e2, 0x08,s0
	vmadm v_res, v_y2,  v_x2;                lsv $v08.e3, 0x0C,s0
	vmadm v_res, v_y2,  v_x2;                lsv $v09.e0, 0x00,s1
	vmadm v_res, v_y3n, v_x205;              lsv $v09.e1, 0x04,s1
	vmadm v_res, v_y0n, v_x305;              lsv $v09.e2, 0x08,s1
	vmadm v_res, v_y1,  v_x3;                lsv $v09.e3, 0x0C,s1
	vmadm v_res, v_y1,  v_x305;              lsv $v10.e0, 0x00,s2
	vmadm v_res, v_y2n, v_x3;                lsv $v10.e1, 0x04,s2
	vmadm v_res, v_y2n, v_x305;              lsv $v10.e2, 0x08,s2
	vmadm v_res, v_y3,  v_x305;              lsv $v10.e3, 0x0C,s2
	                                         lsv $v11.e0, 0x00,s3
	                                         lsv $v11.e1, 0x04,s3
	                                         lsv $v11.e2, 0x08,s3
	                                         lsv $v11.e3, 0x0C,s3
	                                         lsv $v12.e0, 0x00,s4
	                                         lsv $v12.e1, 0x04,s4
	                                         lsv $v12.e2, 0x08,s4
	                                         lsv $v12.e3, 0x0C,s4
	                                         lsv $v13.e0, 0x00,s6
	                                         lsv $v13.e1, 0x04,s6
	                                         lsv $v13.e2, 0x08,s6
	                                         lsv $v13.e3, 0x0C,s6
	                                         lsv $v14.e0, 0x00,s7
	                                         lsv $v14.e1, 0x04,s7
	                                         lsv $v14.e2, 0x08,s7
	                                         lsv $v14.e3, 0x0C,s7
	                                         lsv $v15.e0, 0x00,t0
	                                         lsv $v15.e1, 0x04,t0
	                                         lsv $v15.e2, 0x08,t0
	                                         lsv $v15.e3, 0x0C,t0
	                                         lqv v_acc0, 0x00,acc_ptr
	                                         lrv v_acc0, 0x10,acc_ptr
	                                         lqv v_acc1, 0x10,acc_ptr
	j     MonoGatherMix
	                                         lrv v_acc1, 0x20,acc_ptr

MonoPartial:
	# Some lanes were parked: they contributed nothing to ACCUM, so give
	# their ticks, ACCUM slots and positions back and replay them once the
	# cache has been refilled. Their mask is what the loop turned into
	# "partial", and VCC has since moved on to the group being gathered.
	addi t0, partial, 0xFF
	ctc2 t0, COP2_CTRL_VCC
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
	# fall through

MonoExit:
	# VADPCM must write decoder state back before finishing.
	lh t0, %lo(CH_FLAGS)
	andi t0, CH_FLAGS_VADPCM
	bnez t0, VadpcmEpilog
	nop
	j MIX_CHANNEL_Done
	nop

############################################################
# VADPCM path: decode into DMEM_SAMPLE_CACHE, then Hermite (MonoLoop).
# Positions are in samples (not bytes). STEREO on the wire is ignored —
# stereo VADPCM is two independent mono MIX_CHANNELs.
############################################################

VadpcmSetup:
	# DMA codebook once per command.
	lw s0, %lo(CH_CODEBOOK)
	li s4, %lo(MIX_VADPCM_CODEBOOK)
	jal DMAInAsync
	li t0, DMA_SIZE(128, 1)

	# DMA decoder state. next_frame = floor(pos/16); the cache is empty.
	lw s0, %lo(CH_STATE)
	li s4, %lo(MIX_VADPCM_STATE)
	jal DMAIn
	li t0, DMA_SIZE(16, 1)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t0, 4
	sw t0, %lo(CH_VNEXT_FRAME)
	li t0, -1
	j MonoSetup                              # bps = 0, stereo_shift = 0
	sw t0, %lo(CH_VCUR_FRAME)

VadpcmWaveStart:
	bltu wv_pos, wv_len, VadpcmFillCache
	nop
	beqz wv_loop_len, VadpcmEpilog
	sub wv_pos, wv_loop_len
	# Cached-loop wrap: reload loop-start state and invalidate the window.
	lw s0, %lo(CH_LOOP_STATE)
	beqz s0, 1f
	li s4, %lo(MIX_VADPCM_STATE)
	jal DMAIn
	li t0, DMA_SIZE(16, 1)
1:	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t0, 4
	sw t0, %lo(CH_VNEXT_FRAME)
	li t0, -1
	j VadpcmWaveStart
	sw t0, %lo(CH_VCUR_FRAME)

VadpcmFillCache:
	# Decode min(VADPCM_WIN_FRAMES, frames left) frames of 16 samples into the
	# sample cache, then let MonoLoop resample them like 16-bit mono PCM.
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t1, t0, 4                            # start_frame
	sw t1, %lo(ROUND_SCRATCH)
	srl t0, wv_len, WAVEFORM_POS_FRAC_BITS
	addi t0, 15
	srl t0, 4                                # frames covered by the waveform
	sub t0, t1
	blez t0, VadpcmEpilog
	li t2, VADPCM_WIN_FRAMES
	# Only lanes with all four taps inside the decoded window are accepted;
	# the others are replayed after the next fill. The window that reaches the
	# end of the waveform stops one sample past the last one instead: taps
	# beyond it are filled in after the decode, and the lanes that would start
	# there are given back to VadpcmWaveStart, which wraps the loop (or ends
	# the waveform) before they are replayed.
	slt t9, t2, t0
	bnez t9, 1f
	li t1, -3*2
	move t2, t0
	li t1, 0
1:	sh t2, %lo(CH_VNFRAMES)
	sll t0, t2, 5                            # nframes * 16 samples * 2 bytes
	addi t0, %lo(DMEM_SAMPLE_CACHE)
	bnez t9, 2f
	add t1, t0
	lw t1, %lo(ROUND_SCRATCH)                # start_frame
	srl t0, wv_len, WAVEFORM_POS_FRAC_BITS
	sll t1, 4
	sub t0, t1                               # last sample, from the window start
	sll t0, 1
	addi t1, t0, %lo(DMEM_SAMPLE_CACHE)
2:	sh t1, %lo(CH_VLIMIT)
	sh ticks, %lo(CH_NSAMPLES)
	sw wv_pos, %lo(CH_POS)

	# Bring MIX_VADPCM_STATE, which decodes CH_VNEXT_FRAME, to start_frame.
	# Every fill but the first starts a few frames back, because the taps of
	# the last samples of the previous window were rejected and replayed;
	# rewinding is free, since the state of a frame is just the eight samples
	# before it and those are still in the cache. Decoding forward is only
	# needed when the position jumped past the window.
VadpcmAlignState:
	lw t2, %lo(CH_VNEXT_FRAME)
	lw t1, %lo(ROUND_SCRATCH)
	beq t2, t1, VadpcmAligned
	lw t0, %lo(CH_VCUR_FRAME)
	slt t9, t1, t2
	beqz t9, VadpcmCatchUp
	sub t9, t1, t0                           # frames of the window to keep
	li s0, %lo(MIX_VADPCM_STATE_BEFORE)
	beqz t9, 1f
	sll t9, 5
	addi t9, %lo(DMEM_SAMPLE_CACHE)
	addi s0, t9, -16
1:	li s1, %lo(MIX_VADPCM_STATE)
	lqv $v01, 0,s0
	sqv $v01, 0,s1
	j VadpcmAligned
	sw t1, %lo(CH_VNEXT_FRAME)

VadpcmCatchUp:
	li t0, 1
	sh t0, %lo(CH_VDECN)
1:	lw t2, %lo(CH_VNEXT_FRAME)
	lw t1, %lo(ROUND_SCRATCH)
	beq t2, t1, VadpcmAligned
	nop
	jal VadpcmDecodeFrames
	nop
	j 1b
	nop

VadpcmAligned:
	# Snapshot the state that decodes start_frame: the epilog needs it if the
	# command ends before a single frame has been consumed.
	li s0, %lo(MIX_VADPCM_STATE)
	li s1, %lo(MIX_VADPCM_STATE_BEFORE)
	lqv $v01, 0,s0
	sqv $v01, 0,s1
	lw t1, %lo(ROUND_SCRATCH)
	lh t0, %lo(CH_VNFRAMES)
	sw t1, %lo(CH_VCUR_FRAME)
	jal VadpcmDecodeFrames
	sh t0, %lo(CH_VDECN)

	# The decoder clobbers ticks/pos/len/loop_len/addr, k_ff, s2, $v01-$v23
	# and $v31 (which it needs as the shift table of vsra8).
	lh ticks, %lo(CH_NSAMPLES)
	lw wv_pos, %lo(CH_POS)
	lw wv_len, %lo(CH_LEN)
	lw wv_loop_len, %lo(CH_LOOP_LEN)
	lw wv_addr, %lo(CH_PTR)
	li t0, %lo(VCONST_1)
	lqv v_const1, 0,t0
	lh t1, %lo(CH_VLIMIT)
	mtc2 t1, k_limit

	# Hermite reads three taps past the sample it interpolates, and past the
	# last sample of the waveform there is only the padding of its last frame:
	# hold the last sample instead, so that a loop point does not decay
	# towards silence on the few samples that straddle it.
	lw t0, %lo(CH_VCUR_FRAME)
	lh t1, %lo(CH_VNFRAMES)
	srl s0, wv_len, WAVEFORM_POS_FRAC_BITS
	sll t0, 4
	sll t1, 4                                # samples in the window
	sub s0, t0                               # last sample, from the window start
	sltu s1, t1, s0
	bnez s1, 1f                              # end of the waveform is further on
	sll s0, 1
	addi s0, %lo(DMEM_SAMPLE_CACHE)
	lh t0, -2(s0)
	sh t0, 0(s0)
	sh t0, 2(s0)
	sh t0, 4(s0)

1:	srl s2, wv_pos, WAVEFORM_POS_FRAC_BITS
	andi s2, 15                              # offset of wv_pos in the cache
	sll wv_step_8x, wv_step, 3
	j MonoRefill
	li k_ff, 0xFF

VadpcmEpilog:
	# The state that decodes frame F is just the eight samples before it, so
	# once the cache holds F it can be written back straight out of it. Only
	# the two ends need the snapshots: nothing decoded yet (STATE, as loaded)
	# or nothing consumed yet (STATE_BEFORE, taken at CH_VCUR_FRAME).
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t0, 4                                # end_frame
	lw t1, %lo(CH_VCUR_FRAME)
	li s4, %lo(MIX_VADPCM_STATE)
	bltz t1, 1f
	sub t0, t1                               # frames consumed
	blez t0, 2f
	lw t2, %lo(CH_VNEXT_FRAME)
	sub t2, t1                               # frames decoded
	slt t9, t2, t0
	bnez t9, 1f
	sll t0, 5
	addi t0, %lo(DMEM_SAMPLE_CACHE)
	j 1f
	addi s4, t0, -16                         # eight samples before end_frame
2:	li s4, %lo(MIX_VADPCM_STATE_BEFORE)
1:	lw s0, %lo(CH_STATE)
	jal DMAOut
	li t0, DMA_SIZE(16, 1)
	blez ticks, MIX_CHANNEL_Done
	nop
	j MIX_CHANNEL_Silence
	nop

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
	#undef wv_step_8x
	#undef acc_ptr
	#undef ch_idx
	#undef stereo_shift



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
# Destination of the vmudh/vmadh whose result is only wanted in the
# accumulator. It must not be $v00: the mixer needs that one to stay zero
# across the decode. $v04/$v05 are the two the decoder does not otherwise use,
# and the mixer only keeps powers of x there, which it recomputes every group.
#define v____   $v04

	# Decode CH_VDECN frames starting at CH_VNEXT_FRAME into
	# DMEM_SAMPLE_CACHE. Advances CH_VNEXT_FRAME by CH_VDECN.
	# Clobbers t2, t3, t4, t6, t7, t8, t9, s0-s5 and $v01-$v23, $v31 ($v30 is
	# rewritten with the shift table it already held). $v00 is deliberately
	# left alone, see v____.
	.func VadpcmDecodeFrames
VadpcmDecodeFrames:
	sw ra, %lo(CH_VADPCM_RA)

	# MIX_CHANNEL keeps volume-filter constants in $v31 (v_const1), but
	# vsra8 needs the RSPQ shift table in vshift8 ($v31). Restore it the
	# same way RSPQ_Loop does before each command; caller reloads VCONST_1.
	lpv vshift8, 0x00,zero
	vsrl vshift, vshift8, 8

	# RDRAM address of first compressed frame: ptr + next_frame*9
	lw t0, %lo(CH_VNEXT_FRAME)
	sll t1, t0, 3
	add t1, t0
	lw s0, %lo(CH_PTR)
	add s0, t1
	andi t9, s0, 7                        # misalignment (survives DMAIn)
	sub s0, t9
	lh nframes, %lo(CH_VDECN)
	# nframes*9 bytes, plus the 8-byte lookahead and the misalignment that
	# DMAIn folds into s4.
	sll t0, nframes, 3
	add t0, nframes
	addi t0, 16
	li s4, %lo(MIX_VADPCM_COMP)
	jal DMAIn
	addi t0, -1                            # = DMA_SIZE(t0, 1)

	li dmem_codebook, %lo(MIX_VADPCM_CODEBOOK)
	li dmem_state, %lo(MIX_VADPCM_STATE)
	li dmem_output, %lo(DMEM_SAMPLE_CACHE)
	li dmem_input, %lo(MIX_VADPCM_COMP)
	add dmem_input, t9
	addiu dmem_input, 1

	jal VADPCM_LoadIdentity
	nop

	lh nframes, %lo(CH_VDECN)
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
	lh t1, %lo(CH_VDECN)
	add t0, t1
	sw t0, %lo(CH_VNEXT_FRAME)
	lw ra, %lo(CH_VADPCM_RA)
	jr ra
	nop
