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
	# of this overlay so ACCUM (in .bss) cannot be clobbered mid-round. The
	# per-channel VADPCM table lives in saved state, so it survives overlay
	# swaps; nothing else has to.
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
	# Stereo VADPCM is two independent mono MIX_CHANNELs.
	#
	# Volume ramps
	# ============
	#
	# Each MIX_CHANNEL carries the volume the channel starts the round with,
	# the one its ramp ends on and the increment between the two, per block
	# of four output samples. The mixing loop walks the volume one output
	# sample at a time and clamps it to the two ends, so a ramp can land
	# anywhere inside a round; a channel with no ramp sends an increment of
	# zero and the clamp simply holds the volume. None of it costs a cycle:
	# it all fits vector slots the loop leaves empty. The CPU recomputes the
	# three values from absolute time at every round, so rounding of the
	# increment cannot accumulate into drift (see mixer_ch_set_vol_ramp).
	#
	####################################################################

#include <rsp_queue.inc>
#include <emux.inc>

.set noreorder
.set at

# Maximum number of channels supported by this ucode.
#define MAX_CHANNELS              32

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

# Assert codes reported by the RSP crash handler (debug builds only).
#define MIXER_ASSERT_VADPCM_AHEAD 0xA001

# Waveform flags. Keep these in sync with mixer.c
#define CH_FLAGS_BPS_SHIFT        (3<<0)
#define CH_FLAGS_16BIT            (1<<2)
#define CH_FLAGS_STEREO           (1<<3)
#define CH_FLAGS_STEREO_SUB       (1<<4)
#define CH_FLAGS_VADPCM           (1<<5)
# Decode from the state saved for the loop start rather than the one the last
# round left behind (see VadpcmSetup).
#define CH_FLAGS_VLOOP_STATE      (1<<6)
# Zero ACCUM before mixing (first MIX_CHANNEL of a round; ACCUM is in .bss).
#define CH_FLAGS_CLEAR_ACCUM      (1<<7)


# Sample DMA cache. A transfer may never end more than SAMPLE_CACHE_SIZE bytes
# past the end of the RDRAM window (that is #MIXER_LOOP_OVERREAD in mixer.c,
# the only part of the overread that is guaranteed to hold valid samples), but
# it may well be larger than that when the window is far from its end. The
# mono resampler exploits this to fetch SAMPLE_CACHE_SAMPLES samples at a
# time, whatever the PCM format: refilling costs about as much as mixing a
# cacheful, so it is worth spending most of the spare DMEM on the window.
#define SAMPLE_CACHE_SIZE         64
#define SAMPLE_CACHE_SAMPLES      160

# In-mixer VADPCM: one fill decodes as many frames of 16 samples as the
# window holds. It cannot hold more: their compressed form is staged in the
# tail of that same window (see VadpcmDecodeFrames).
#define VADPCM_WIN_FRAMES         (SAMPLE_CACHE_SAMPLES / 16)


	################################
	# Global register allocations
	################################

	# $v00 is zeroed once per command: it is the vs operand of the vor/vmrg
	# that build the splats of the mono resampler, and vs cannot be swizzled,
	# so no element of another vector can stand in for it. Nothing else in
	# this file may write to it, VADPCM decoder included (see v____), with
	# one exception: the mono mixing loop borrows it as the lower bound of
	# the volume clamp (see v_vlo), and hands it back at every exit.
	#define v_zero        $v00
	#define v_shift       $v30
	#define v_const1      $v31

	#define k_0000        v_zero
	# k_minusone: integer minus one. vmudh by it negates a vector with the
	#          same saturation a vsub from zero would have, which is what
	#          lets the mono loop keep its zero vector free (see v_vlo).
	# k_half:  0.5 as a 0.16 unsigned fraction. Multiplying by it with vmudl
	#          is a logical shift right by one (vsrl cannot be used here: it
	#          needs the RSPQ shift table in $v31, which holds VCONST_1).
	# k_one:   integer one. vmudh by it loads a vector into the accumulator
	#          unshifted, to start a vmadm chain.
	# k_eight: integer eight, one group of the mono resampler.
	#define k_minusone    v_const1.e1
	#define k_half        v_const1.e3
	#define k_one         v_const1.e4
	#define k_eight       v_const1.e5


	.data

	RSPQ_BeginOverlayHeader
		RSPQ_DefineCommand MIX_CHANNEL,       36		# 0x0
		RSPQ_DefineCommand MIX_SETCHANNEL,    16		# 0x1
		RSPQ_DefineCommand MIX_FLUSH,         16		# 0x2
		RSPQ_DefineCommand MIX_SETSTATE,      24		# 0x3
		RSPQ_DefineCommand MIX_COPY,          12		# 0x4
	RSPQ_EndOverlayHeader

############################################################################

	.align 4
VCONST_1:
	.half 0x7FFF
	.half -1          # integer minus one
	.half 0
	.half 0x8000      # 0.5 fixed 0.16
	.half 1           # integer one
	.half 8           # integer eight

	# Lane -> output sample mapping used by the mono resampler. The eight
	# results come out interleaved this way so that the .q0/.q1 swizzles
	# feed the LRLRLRLR layout of ACCUM without any further shuffling.
	.align 4
PERM8:
	.half 0, 4, 1, 5, 2, 6, 3, 7

	# Position of each ACCUM slot of a quad within the four samples the
	# ramp walks between two of its steps, as a 0.16 fraction. Multiplying
	# the step by it is what staggers a volume vector across its lanes.
	.align 4
VSTAGGER:
	.half 0, 0, 0x4000, 0x4000, 0x8000, 0x8000, 0xC000, 0xC000

	.align 4
BANNER0:    .ascii "Dragon RSP Audio"
BANNER1:    .ascii " Coded by Rasky "

	# ================================================================
	# Overlay saved state – survives RSPQ overlay swaps.
	# Layout must match mixer_overlay_state_t in mixer.c.
	# ================================================================
	RSPQ_BeginSavedState
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

	# Per-lane multipliers that bring each residual of a VADPCM frame to the
	# top of its 16-bit lane, one entry of four halfwords per residual width.
	#
	# A frame is a control byte followed by 16 residuals of `bits` bits each,
	# packed MSB first: residual i occupies payload bits [i*bits, (i+1)*bits).
	# The decoder reads the payload as four halfwords, one per group of four
	# consecutive residuals, and isolates a residual with two multiplies: one
	# that shifts it up to bit 15 of its lane (the low 16 bits of the product,
	# so everything above it drops out) and one that shifts it back down by
	# 16-bits, which sign extends it.
	#
	# Group j starts at payload byte (j*bits)>>1, at bit d = 4 if j*bits is
	# odd and 0 otherwise. Within the group, residual 4j+k is k*bits further
	# down. The .q0 swizzle feeds each halfword to a pair of lanes, which take
	# the even residual (multiplier K0) and the odd one (K0 << bits), so:
	#
	#   K0 = [2^0, 2^2b, 2^d, 2^(d+2b)] with b = bits
	#
	# The parity of j*bits has period 2 in j, so lanes 4 to 7 repeat the
	# entry: VadpcmDecodeFrames loads it with two ldv.
	#
	# The rest of an entry is what else depends on the width alone, so that
	# VadpcmSetup can load it instead of deriving it: 2^bits, the multiplier
	# of the sign extending shift, and the decomposition of the frame stride
	# (see CH_VMULSH). The two of those sit as one word, in the order of the
	# fields they are copied to.
	.align 4
VADPCM_K0:
	.half 1,  16,  1,   16;  .half 0,  4;  .half 2,  0    # 2 bits (stride 5)
	.half 1,  64, 16, 1024;  .half 0,  8;  .half 3, -1    # 3 bits (stride 7)
	.half 1, 256,  1,  256;  .half 0, 16;  .half 3,  0    # 4 bits (stride 9)

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
	# The tail of the window also stages the compressed frames of a VADPCM
	# fill, which are decoded into the window itself (see VadpcmDecodeFrames).
	#
	# The pipelined VADPCM decoder spills one stale vector 16 bytes below the
	# window before its first real store: that lands in the tail of ACCUM_PAD,
	# which is never DMA'd out.
	.align 4
DMEM_SAMPLE_CACHE:        .dcb.b SAMPLE_CACHE_SAMPLES * 2
DMEM_SAMPLE_CACHE_ZERO:   .dcb.b 16

	# Spill slot of the volume vector of the second half of a group. It lives
	# in $v30, which the VADPCM decoder needs as a shift table, so it is
	# saved here around a decode (the other one is in a register the decoder
	# leaves alone).
	.align 4
DMEM_VMID:                .dcb.b 16

	# Lower bound of the volume clamp, splatted the same way. It lives in
	# $v00, which the paths outside the mixing loop need back as the zero
	# vector, so it is reloaded from here on the way in (see MonoPrime).
	.align 4
DMEM_VLO:                 .dcb.b 16

	# Per-lane tap addresses produced by the mono resampler, and the
	# scratch used to transpose the eight gathered 4-tap groups.
	.align 4
DMEM_ADDRS:               .dcb.b 16
	.align 4
DMEM_TRANSPOSE:           .dcb.b 128

	# In-mixer VADPCM staging: 8 predictor vectors (128 bytes) plus the three
	# Hermite taps past a loop point (8 bytes with padding). Stereo VADPCM
	# uses two MIX_CHANNEL commands, each with its own half.
	.align 4
MIX_VADPCM_CODEBOOK:      .space 144
	.align 4
MIX_VADPCM_STATE:         .space 16
	.align 4
MIX_VADPCM_STATE_BEFORE:  .space 16

	# Frame index the current fill starts at.
	.align 2
ROUND_SCRATCH:            .long  0

	# Staging for the round marker MIX_FLUSH sends back to RDRAM, telling the
	# CPU which windows the RSP is done reading (see MIX_FLUSH). A DMA moves
	# 8 bytes at a time, so the id is stored twice rather than paired with
	# garbage.
	.align 3
ROUND_MARKER:             .long  0, 0

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
# Volume the ramp of this command is heading for, [left:16][right:16]. Together
# with CH_LVOL/CH_RVOL it gives the two ends of the ramp, which are the bounds
# the mixing loops clamp to. Equal to CH_LVOL/CH_RVOL when no ramp is running.
	.align 2
CH_TVOL:                  .long  0
# Volume the ramp advances by every four output samples, [left:16][right:16].
# Only the setup needs it: the loop keeps the step as the difference of its two
# volume vectors.
	.align 2
CH_DVOL:                  .long  0
# Output samples of the current round. Latched by every MIX_CHANNEL from
# a0's 9-bit nsamples/2 field; shared by every channel of the round.
# Not a scratch: the VADPCM path spills the live tick count elsewhere.
ROUND_NSAMPLES:           .half  0
# Scratch for the live tick count across a VADPCM decode, which clobbers the
# scalar registers that hold it.
CH_TICKS:                 .half  0
# First frame currently decoded into DMEM_SAMPLE_CACHE; -1 = none.
CH_VCUR_FRAME:            .long  0
# Frame index that MIX_VADPCM_STATE is ready to decode next.
CH_VNEXT_FRAME:           .long  0
CH_VADPCM_RA:             .long  0
# Frames of the current fill, and frames the next decode call will produce
# (they differ while the decoder is catching up to the start of the fill).
CH_VNFRAMES:              .half  0
CH_VDECN:                 .half  0
# k_limit of the current fill, computed before the decode clobbers it, and
# whether that fill stops short of the end of the waveform (in which case the
# limit is the end of the window instead of the end of the waveform).
CH_VLIMIT:                .half  0
CH_VATEND:                .half  0
# Per-sample step of the mono resampler, split the same way as the position:
# fraction as 0.16, plus whole samples.
CH_STEPF:                 .half  0
CH_STEPI:                 .half  0
# Decomposition of the frame stride (2*bits+1 bytes) that turns a frame index
# into a byte offset without a multiply:
#   n*stride = (n << CH_VMULSH) + ((n ^ neg) - neg)
# The two are adjacent and word aligned because they are copied together out
# of VADPCM_K0. CH_VBITS is the residual width itself (2, 3 or 4 bits).
	.align 2
CH_VMULSH:                .half  0
CH_VMULNEG:               .half  0
CH_VBITS:                 .half  0
# Frames the waveform covers, ceil(len/16). Every fill measures how many are
# left against it, and it cannot change within a command.
	.align 2
CH_VNFRAMES_TOT:          .long  0


	.text 1

###############################################################
# MIX_CHANNEL – resample one channel and accumulate into ACCUM.
#
# Command words (32 bytes):
#   a0: [ovl+cmd:8][ch:5][flags:8][nsamples/2:9]
#   a1: [lvol:16][rvol:16]        volume at the start of the round
#   a2: pos   (fixed point, WAVEFORM_POS_FRAC_BITS)
#   a3: step
#   +16: len
#   +20: loop_len
#   +24: ptr  (0 = nothing to mix)
#   +28: [dlvol:16][drvol:16]     volume ramp, per block of 4 samples
#   +32: [tlvol:16][trvol:16]     volume the ramp ends on (= lvol/rvol if none)
#
# nsamples is the same for every channel of a round and is always even
# (see mixer_round_length), so every MIX_CHANNEL latches nsamples/2 from a0
# into ROUND_NSAMPLES (9 bits: 0..256 covers MIXER_MAX_SAMPLES_PER_ROUND).
# Accumulator writes always start at ACCUM: the old per-command acc_offset
# was invariably 0.
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

	# Volumes, as [L,R,L,R,L,R,L,R] to match the ACCUM layout.
	#
	# The ramp moves once per output sample, so the eight samples a group
	# emits are mixed with eight different volumes. Two vectors carry them:
	# v_xvol the four samples ACCUM's first quad holds, v_xvol4 the four of
	# the second one, four samples further along the ramp. Both are built
	# already staggered (lane k of v_xvol is k quarters of a half-group step
	# above the volume the round starts at) and both advance by a whole
	# group at a time, which is what the loop needs the step for.
	#
	# The step itself is not kept anywhere: it is the difference between the
	# two vectors, which the loop subtracts back out of them. That is what
	# leaves a register for the second vector, and it is also what makes the
	# landing come out right, since the difference of two clamped vectors is
	# what is left of the step and goes to zero as the ramp lands.
	#
	# The ramp is a slope and nothing else: what stops it on the value the
	# CPU asked for is the clamp to [v_vlo, v_vhi], the two ends of the ramp
	# sorted, which is also why a round never has to be cut short at the
	# point a ramp lands. With no ramp running the two bounds coincide and
	# the clamp simply holds the volume.
	#
	# $v24/$v25 are outside the range the VADPCM decoder uses, so they
	# survive a frame decode untouched. v_xvol4 does not: the decoder needs
	# $v30 as a shift table, so it is spilled to DMEM_VMID around a decode.
	# v_vlo borrows the zero vector, which every path outside the mixing
	# loop wants back, so it is reloaded from DMEM_VLO on the way in.
	#define v_xvol         $v24
	#define v_xvol4        v_shift
	#define v_vhi          $v25
	#define v_vlo          v_zero

	# Mono resampler state, live across MonoLoop iterations:
	#   v_frac  fraction of the sample position, 0.16 unsigned
	#   v_idx   DMEM index of tap y0, in units of 2 bytes
	#   v_kmono broadcast scalars of the loop, see k_* below
	#define v_frac         $v26
	#define v_idx          $v27
	#define v_kmono        $v29
	# k_limit   first tap address that would read past the cache
	# k_zaddr   address of the window of four zero samples
	# k_step8f  8-sample step, fractional part
	# k_step8i  8-sample step, whole samples
	# k_addrmul bytes per v_idx unit: 2 per sample (mono), 4 per frame (stereo)
	# k_subofs  byte bias of the plane being mixed: 2 on the right channel of
	#           an interleaved stereo waveform, whose samples start one
	#           halfword later, and 0 everywhere else
	# (e4 carries the VADPCM sign-extension multiplier, see kext.)
	#define k_limit        v_kmono.e0
	#define k_zaddr        v_kmono.e1
	#define k_step8f       v_kmono.e2
	#define k_step8i       v_kmono.e3
	#define k_addrmul      v_kmono.e5
	#define k_subofs       v_kmono.e6

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
	# Half-group step of the ramp, the difference of the two volume vectors.
	# The loop rebuilds it every iteration in the register y0 is read out of,
	# which the transpose only rewrites at the top of the next one; the paths
	# around the loop rebuild it in one of the free gather registers.
	#define v_diff         $v16
	#define v_diffc        $v20

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
	# a0: bits[23:19]=ch_idx, bits[18:11]=flags, bits[8:0]=nsamples/2
	srl t0, a0, 19
	andi t0, 0x1F                  # ch_idx
	sh t0, %lo(CH_IDX)
	srl t0, a0, 11
	andi t0, 0xFF                  # flags
	sh t0, %lo(CH_FLAGS)
	srl t0, a1, 16                 # lvol
	sh t0, %lo(CH_LVOL)
	andi t0, a1, 0xFFFF            # rvol
	sh t0, %lo(CH_RVOL)
	sw a2, %lo(CH_POS)
	sw a3, %lo(CH_STEP)
	lw t0, CMD_ADDR(0x10, 0x24)
	sw t0, %lo(CH_LEN)
	lw t0, CMD_ADDR(0x14, 0x24)
	sw t0, %lo(CH_LOOP_LEN)
	lw t0, CMD_ADDR(0x18, 0x24)
	sw t0, %lo(CH_PTR)
	lw t0, CMD_ADDR(0x20, 0x24)
	sw t0, %lo(CH_TVOL)
	lw t0, CMD_ADDR(0x1C, 0x24)
	sw t0, %lo(CH_DVOL)
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

	# Latch nsamples from a0. Same for every channel of a round; nsamples/2
	# needs 9 bits because MIXER_MAX_SAMPLES_PER_ROUND is 512.
	andi t1, a0, 0x1FF
	sll t1, 1
	sh t1, %lo(ROUND_NSAMPLES)

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
	# Splat the volume this round starts at, the one its ramp ends on and
	# the step between two of its points into [L,R,L,R,L,R,L,R] vectors.
	# All three are stored as adjacent halfword pairs in DMEM, so one word
	# replicated four times does each of them.
	li s0, %lo(RSPQ_SCRATCH_MEM)
	lw t1, %lo(CH_LVOL)             # CH_RVOL is the next halfword
	lw t2, %lo(CH_TVOL)
	sw t1, 0x00(s0)
	sw t1, 0x04(s0)
	sw t1, 0x08(s0)
	sw t1, 0x0C(s0)
	lqv v_xvol, 0x00,s0
	sw t2, 0x00(s0)
	sw t2, 0x04(s0)
	sw t2, 0x08(s0)
	sw t2, 0x0C(s0)
	lqv v_vhi, 0x00,s0
	lw t1, %lo(CH_DVOL)
	sw t1, 0x00(s0)
	sw t1, 0x04(s0)
	sw t1, 0x08(s0)
	sw t1, 0x0C(s0)
	lqv v_diffc, 0x00,s0

	lh ticks, %lo(ROUND_NSAMPLES)
	beqz ticks, MIX_CHANNEL_Done

	# Sort the two ends of the ramp into the bounds the mixing loop clamps
	# to. v_vlo lands in the zero vector, so it is also stashed: every path
	# that leaves the loop wants the zero back and reloads it from there.
	# They are the ends of the whole ramp, so they are the same for every
	# lane: what the stagger below adds is a fraction of one step, and no
	# lane of the round can start further along than the ramp ends.
	vlt v_vlo, v_xvol, v_vhi
	vge v_vhi, v_xvol, v_vhi
	li s0, %lo(DMEM_VLO)
	sqv v_vlo, 0x00,s0

	# Stagger the two volume vectors: the four samples of a quad are a
	# quarter of a step apart, and the second quad is a whole step past the
	# first one.
	li s0, %lo(VSTAGGER)
	lqv $v08, 0x00,s0
	vmudm $v08, v_diffc, $v08
	vadd v_xvol, v_xvol, $v08
	vadd v_xvol4, v_xvol, v_diffc

	li acc_ptr, %lo(ACCUM)

	lw wv_addr, %lo(CH_PTR)
	beqz wv_addr, MIX_CHANNEL_Done

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
	# mono samples however the STEREO / STEREO_SUB bits (which the owner of a
	# stereo pair still carries) read.
	andi t1, t0, CH_FLAGS_VADPCM
	srl t1, 2                                # VADPCM bit -> STEREO bit
	sll t2, t1, 1                            #            -> STEREO_SUB bit
	or t1, t2
	nor t1, t1, zero
	and t0, t1
	andi stereo_shift, t0, CH_FLAGS_STEREO
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
	li s0, %lo(RSPQ_SCRATCH_MEM)
	sh t1, 0x02(s0)                          # k_zaddr
	# The resampler turns each lane of v_idx into a byte address with a single
	# multiply, and biases it to the plane it is mixing with a single or, so
	# that neither depends on a test inside the loop.
	li t1, 2
	sllv t1, t1, stereo_shift
	sh t1, 0x0A(s0)                          # k_addrmul
	andi t0, CH_FLAGS_STEREO_SUB
	srl t0, 3                                # bit 4 → 2 bytes
	sh t0, 0x0C(s0)                          # k_subofs
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
	# SAMPLE_CACHE_SAMPLES=160 → 80 source bytes per half, so 8+2 lpv/sqv.
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
	sqv $v08, 0x80,s3
	sqv $v09, 0x90,s3
	lpv $v08, 0x50,s1
	lpv $v09, 0x58,s1
	lpv $v10, 0x60,s1
	lpv $v11, 0x68,s1
	lpv $v12, 0x70,s1
	lpv $v13, 0x78,s1
	lpv $v14, 0x80,s1
	lpv $v15, 0x88,s1
	sqv $v08, 0x00,s1
	sqv $v09, 0x10,s1
	sqv $v10, 0x20,s1
	sqv $v11, 0x30,s1
	sqv $v12, 0x40,s1
	sqv $v13, 0x50,s1
	sqv $v14, 0x60,s1
	sqv $v15, 0x70,s1
	lpv $v08, 0x90,s1
	lpv $v09, 0x98,s1
	sqv $v08, 0x80,s1
	sqv $v09, 0x90,s1

MonoRefill:
	# Seed the eight lanes: lane k plays output sample PERM8[k], so it
	# starts at wv_pos + PERM8[k]*step. The whole-sample carry of that
	# product comes out of the accumulator, hence the vmudn/vsar pair.
	#
	# The splat below needs the zero vector as its vs, so take $v00 back
	# from the volume clamp; MonoPrime reloads the bound before the loop.
	vxor v_zero, v_zero, v_zero
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
	#
	# It also runs the tail of the body, which is where the ramp advances,
	# so the group it advances by has to be given back first: a pair one
	# group behind is what the tail turns back into the pair it was handed.
	# Only the first vector is rewound, the second one is rebuilt from it,
	# because the two ends of the ramp are a step apart and that step is all
	# the loop has to go on. This is the one place the ramp leaves the range
	# it spans, so the rewind can saturate: a round that starts at a volume
	# less than a group below the top of the fx15 range (a fade-out from
	# full volume) begins a fraction of a group in instead. Rebuilding the
	# second vector is what keeps the step exact when that happens; the
	# starting volume is off by whatever was clipped, which is a hair of the
	# ramp unless it is steep enough to cover the range in a few groups. Only
	# the round a ramp starts in can be in that position — every later one
	# starts from a volume the ramp has already passed through.
	#
	# It is also the one way into the loop, so this is where the lower bound
	# of the volume clamp gets its register back from the zero vector.
	li    t0, %lo(DMEM_VLO)
	vsub  v_diffc, v_xvol4, v_xvol
	lqv   v_vlo, 0x00,t0
	vsub  v_xvol,  v_xvol,  v_diffc
	vsub  v_xvol,  v_xvol,  v_diffc
	vadd  v_xvol4, v_xvol,  v_diffc
	vmudn v_addr, v_idx,  k_addrmul          # v_idx units → byte address
	li    mask, 0xFF                         # nothing to give back yet
	addi  acc_ptr, -8*4
	vor   v_addr, v_addr, k_subofs           # → the plane being mixed
	vlt   v_tmp,  v_addr, k_limit
	vmrg  v_addr, v_addr, k_zaddr
	j     MonoLoopGather
	sqv   v_addr, 0x00,addr_ptr

	.balign 8
MonoLoop:
	# Mono: ~50 cycles per group of eight output samples, that is 6.25 cycles
	# per sample. Stereo: a few more for the lsv gather; a stereo waveform is
	# two MIX_CHANNEL commands (owner=L, STEREO_SUB=R).
	#
	# The byte addresses of the group being gathered were multiplied out of
	# v_idx at the bottom of the previous iteration and biased there to the
	# plane this command owns (an or, because a stereo frame is four bytes,
	# so the halfword bit is always clear).
	#
	# Read back the taps gathered by the previous iteration, one tap of all
	# eight output samples per register, while the DMEM addresses of the
	# group gathered by this one are computed. Lanes whose 4-tap window is
	# not fully inside the cache are parked on the zero window; VCC records
	# the survivors and is left untouched until the bottom of the body,
	# where it becomes the mask of the group the next iteration will mix.
	#
	# The eight ltv leave the SU no room for anything else, so this is where
	# the vector operations that depend on neither the taps nor the powers
	# of x are parked: the whole volume clamp. The two volume vectors were
	# advanced by one group at the bottom of the previous iteration, and
	# pinning them to the ends of the ramp is what is left before the vmacf
	# reads them. It has to happen here and nowhere else, because a compare
	# writes VCC and the vlt below owns it all the way down to the cfc2.
	# The clamps also clear VCO, which nothing between here and the vaddc at
	# the bottom of the body relies on.
	vge   v_xvol,  v_xvol,  v_vlo;           ltv $v16.e0, 0x00,tr_ptr
	vlt   v_xvol,  v_xvol,  v_vhi;           ltv $v16.e7, 0x10,tr_ptr
	vge   v_xvol4, v_xvol4, v_vlo;           ltv $v16.e6, 0x20,tr_ptr
	vlt   v_xvol4, v_xvol4, v_vhi;           ltv $v16.e5, 0x30,tr_ptr
	vlt   v_tmp,  v_addr, k_limit;           ltv $v16.e4, 0x40,tr_ptr
	vmrg  v_addr, v_addr, k_zaddr;           ltv $v16.e3, 0x50,tr_ptr
	                                         ltv $v16.e2, 0x60,tr_ptr
	                                         ltv $v16.e1, 0x70,tr_ptr
	                                         add wv_pos, wv_step_8x
	                                         addi ticks, -8
	                                         sqv v_addr, 0x00,addr_ptr

MonoLoopGather:
	# Negated taps: the coefficients are fed to the accumulator one unit    # Gather the taps of each of
	# at a time (vmadm saturates its output but not the accumulator), so    # the eight output samples of
	# the subtractions of the polynomial are done on the taps instead.      # the next group.
	# They are multiplies by minus one, and not subtractions from zero,
	# because the zero vector is on loan to the volume clamp for the whole
	# of the loop (see v_vlo). Both saturate the same way.
	#
	# Mono uses ldv (8 bytes = 4 contiguous taps). Stereo taps of one plane
	# are every other halfword, so four lsv per lane build the same e0..e3
	# layout; vector loads wrap inside a 16-byte DMEM line, and a lone lsv
	# of an aligned halfword never wraps. The test between the two rides
	# one slot earlier than it reads, so that the last address load fills
	# its delay slot: both gathers need that address anyway.
	vmudh v_y0n, v_y0, k_minusone;           lhu s0, 0x00(addr_ptr)
	vmudh v_y1n, v_y1, k_minusone;           lhu s1, 0x02(addr_ptr)
	vmudh v_y2n, v_y2, k_minusone;           lhu s2, 0x04(addr_ptr)
	vmudh v_y3n, v_y3, k_minusone;           lhu s3, 0x06(addr_ptr)
	vmudh v_res, v_y1,  k_one;               lhu s4, 0x08(addr_ptr)     # y1
	vmadm v_res, v_y2,  v_x05;               lhu s6, 0x0A(addr_ptr)     # + x * (y2 - y0)/2
	vmadm v_res, v_y0n, v_x05;               lhu s7, 0x0C(addr_ptr)
	vmadm v_res, v_y0,  v_x2;                bnez stereo_shift, MonoGatherStereo
	                                         lhu t0, 0x0E(addr_ptr)     # + x^2 * (y0 - 5*y1/2
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
	# Each quad has its own vector, four samples of ramp apart.
	vmudh v_acc0, v_acc0, k_one;             stv $v08.e0, 0x00,tr_ptr
	vmacf v_acc0, v_xvol,  v_res.q0;         stv $v08.e1, 0x10,tr_ptr
	vmudh v_acc1, v_acc1, k_one;             stv $v08.e2, 0x20,tr_ptr
	vmacf v_acc1, v_xvol4, v_res.q1;         stv $v08.e3, 0x30,tr_ptr
	vmudl v_x305, v_x2,   v_x05;             stv $v08.e4, 0x40,tr_ptr   # x^3/2 = x^2 * (x/2)
	vmudl v_x3,   v_x2,   v_frac;            stv $v08.e5, 0x50,tr_ptr   # x^3
	vaddc v_frac, v_frac, k_step8f;          stv $v08.e6, 0x60,tr_ptr   # advance the eight positions:
	vadd  v_idx,  v_idx,  k_step8i;          stv $v08.e7, 0x70,tr_ptr   #   the carry crosses over
	vmudl v_x205, v_x2,   k_half;            sqv v_acc0, 0x00,acc_ptr   # x^2/2
	# The volume of the group the next iteration mixes: one group further
	# along the ramp, which is two of the half-group steps the two vectors
	# are apart by. Walking them over each other that way is what keeps the
	# step out of a register of its own, and it costs nothing here, where
	# the SU is busy draining ACCUM. The clamp cannot follow them, because
	# the vlt/vmrg pair at the top owns VCC from there down to the cfc2
	# below: it runs at the top of the next iteration instead.
	#
	# y0 has been read out of v_diff for the last time by now, and the
	# transpose only writes it again at the top.
	vsub  v_diff,  v_xvol4, v_xvol;          srv v_acc0, 0x10,acc_ptr
	vadd  v_xvol,  v_xvol4, v_diff;          sqv v_acc1, 0x10,acc_ptr
	# Byte addresses of the group the next iteration gathers. v_addr shares
	# its register with v_acc0, which the stores above have just drained.
	vmudn v_addr, v_idx,  k_addrmul;         srv v_acc1, 0x20,acc_ptr

	# VCC still holds the verdict of the vlt at the top: read it only now
	# that "partial" has consumed the mask of the group just mixed. It is
	# also the one spot where the cfc2, which counts as both a load and a
	# store, has neither two cycles before nor after it.
	vadd  v_xvol4, v_xvol,  v_diff;          cfc2 mask, COP2_CTRL_VCC
	vor   v_addr, v_addr, k_subofs;          bgtz s0, MonoLoop
	                                         addi acc_ptr, 8*4
	bltz partial, MonoPartial
	nop
	j MonoExit
	nop

	.balign 8
MonoGatherStereo:
	# Build y0..y3 of this plane into e0..e3 of each gather register, matching
	# the mono ldv layout. The addresses already point at the plane this
	# command owns, since k_subofs biased the whole vector of them.
	vmadm v_res, v_y1n, v_x2;                lsv $v08.e0, 0x00,s0
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
	#
	# Part of the ramp is given back with them. The loop moves the volume by
	# a whole group per iteration, so a group that only emitted some of its
	# samples has moved it further than those samples paid for, and the
	# replay is about to move it again for the rest of them. Handing back
	# the fraction that belongs to the replayed lanes is what keeps the
	# slope the one the CPU computed, wherever the edge of the cache happens
	# to cut a group. MonoPrime gives back a whole group on top of that, for
	# its own pass over the tail of the body.
	#
	# The step is the difference between the two volume vectors, so a lane
	# the clamp has already landed owes nothing without anything having to
	# ask: what it would hand back is zero. It is kept aside so that the
	# second vector can be rebuilt from the first one below rather than
	# rewound with it, which is what keeps the step exact if the rewind
	# saturates (see MonoPrime). The gather registers are free here: the
	# group they hold has been spilled and is about to be dropped.
	vsub  $v09, v_xvol4, v_xvol
	#
	# The vmrg below wants the zero vector, which the loop has been using as
	# the lower bound of the volume clamp: MonoPrime reloads it.
	vxor  v_zero, v_zero, v_zero
	addi t0, partial, 0xFF
	ctc2 t0, COP2_CTRL_VCC
	vmrg  v_cnt, v_zero, k_one               # 1 for each rejected lane
	vadd  v_cnt, v_cnt, v_cnt.q1
	vadd  v_cnt, v_cnt, v_cnt.h2
	vadd  v_cnt, v_cnt, v_cnt.e4
	mfc2  t0, v_cnt.e0
	# A step covers four samples, so the share of it is count/4: taken as
	# count/16 and doubled twice, because a whole group can be rejected and
	# count/4 would not fit a 0.16 multiplier.
	sll t1, t0, 12
	mtc2 t1, $v08.e0
	add ticks, t0
	sll t1, t0, 2
	sub acc_ptr, t1
	vmudm $v10, $v09, $v08.e0
	vadd  $v10, $v10, $v10
	vadd  $v10, $v10, $v10
	vsub  v_xvol,  v_xvol, $v10
	vadd  v_xvol4, v_xvol, $v09
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
	# DMA codebook once per command (predictors + Hermite loop taps). The
	# codebook is 8-byte aligned, so MIX_SETCHANNEL packs the residual width
	# of the stream into the two low bits of its pointer, as bits-2.
	lw s0, %lo(CH_CODEBOOK)
	andi t9, s0, 3
	sub s0, t9
	addi t9, 2                               # residual width: 2, 3 or 4
	li s4, %lo(MIX_VADPCM_CODEBOOK)
	sh t9, %lo(CH_VBITS)
	jal DMAInAsync
	li t0, DMA_SIZE(136, 1)

	# DMA decoder state. next_frame = floor(pos/16); the cache is empty.
	#
	# A round can end exactly on the loop point, and then it is the CPU that
	# wraps the position, leaving behind the state of the end of the waveform
	# instead of the one of the loop. The CPU marks the command that starts on
	# the frame the loop starts on, which decodes from the state saved for it —
	# also the state the waveform reaches that frame with, so it holds for a
	# channel that is still playing its attack.
	lw s0, %lo(CH_STATE)
	lh t0, %lo(CH_FLAGS)
	andi t0, CH_FLAGS_VLOOP_STATE
	beqz t0, 1f
	li s4, %lo(MIX_VADPCM_STATE)
	lw s0, %lo(CH_LOOP_STATE)
1:	jal DMAIn
	li t0, DMA_SIZE(16, 1)

	# Residual unpacking constants of this stream. Everything that depends on
	# the width alone is in VADPCM_K0: the decoder indexes it for the per-lane
	# multipliers, and the rest is copied out of the same entry here.
	#
	# 2^bits is the multiplier of the sign extending shift. It reaches the
	# decoder in v_kmono.e4, through the scratch MonoSetup splats the loop
	# scalars from (which only writes lanes 0 to 3). The stride decomposition
	# lands in the field pair it is stored as (see CH_VMULSH).
	sll t0, t9, 4
	addi t0, %lo(VADPCM_K0 - 32)             # entry of this width: (bits-2)*16
	li s1, %lo(RSPQ_SCRATCH_MEM)
	lh t1, 0x0A(t0)
	lw t2, 0x0C(t0)
	sh t1, 0x08(s1)
	sw t2, %lo(CH_VMULSH)

	# Frames the waveform covers: hoisted here because every fill needs it and
	# it only depends on the length, which the command fixes.
	srl t0, wv_len, WAVEFORM_POS_FRAC_BITS
	addi t0, 15
	srl t0, 4
	sw t0, %lo(CH_VNFRAMES_TOT)

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
	lw t0, %lo(CH_VNFRAMES_TOT)              # frames covered by the waveform
	sub t0, t1
	blez t0, VadpcmEpilog
	li t2, VADPCM_WIN_FRAMES
	# Only lanes with all four taps inside the decoded window are accepted;
	# the others are replayed after the next fill. The window that reaches the
	# end of the waveform stops one sample past the last one instead: taps
	# beyond it are filled in after the decode, and the lanes that would start
	# there are given back to VadpcmWaveStart, which wraps the loop (or ends
	# the waveform) before they are replayed.
	slt t9, t2, t0                           # more frames than the window holds
	bnez t9, 1f
	nop
	# The taps written past the last sample have to fit in the cache: on the
	# rare window that is full and whose last frame is barely padded they do
	# not, so keep the window-end limit and let the next fill, which starts
	# further on, be the one that reaches the end.
	lw s0, %lo(ROUND_SCRATCH)                # start_frame
	srl s1, wv_len, WAVEFORM_POS_FRAC_BITS
	sll s0, 4
	sub s1, s0                               # samples up to the end
	addi s0, s1, 3
	slti s0, s0, SAMPLE_CACHE_SAMPLES + 1
	beqz s0, 1f
	move t2, t0                              # nframes = frames left
	sll s1, 1
	addi t1, s1, %lo(DMEM_SAMPLE_CACHE)      # limit: the end of the waveform
	j 2f
	sh zero, %lo(CH_VATEND)
1:	li t9, 1
	sll t1, t2, 5                            # nframes * 16 samples * 2 bytes
	addi t1, %lo(DMEM_SAMPLE_CACHE) - 3*2    # limit: three taps before its end
	sh t9, %lo(CH_VATEND)
2:	sh t2, %lo(CH_VNFRAMES)
	sh t1, %lo(CH_VLIMIT)
	sh ticks, %lo(CH_TICKS)
	sw wv_pos, %lo(CH_POS)

	# Bring MIX_VADPCM_STATE, which decodes CH_VNEXT_FRAME, to start_frame.
	# Every fill but the first starts a few frames back, because the taps of
	# the last samples of the previous window were rejected and replayed;
	# rewinding is free, since the state of a frame is just the eight samples
	# before it and those are still in the cache.
	#
	# It can only ever be a rewind: the resampler stops at the end of the
	# decoded window (k_limit), so the frame the next fill starts on is never
	# past the one the decoder stopped at.
VadpcmAlignState:
	lw t2, %lo(CH_VNEXT_FRAME)
	lw t1, %lo(ROUND_SCRATCH)
	beq t2, t1, VadpcmAligned
	lw t0, %lo(CH_VCUR_FRAME)
	assert_lt t1, t2, MIXER_ASSERT_VADPCM_AHEAD
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
	sh t0, %lo(CH_VDECN)
	# The decoder wants the register the second volume vector lives in.
	li t0, %lo(DMEM_VMID)
	jal VadpcmDecodeFrames
	sqv v_xvol4, 0x00,t0

	# Hermite reads three taps past the sample it interpolates, and past the
	# last sample of the waveform those are the first three of the loop. They
	# ride in with the codebook DMA (MIX_VADPCM_CODEBOOK+128); without them a
	# loop point would be interpolated towards the padding of the last frame.
	#
	# A loop state is only recorded for a waveform that loops, so it says the
	# taps are meaningful even when the loop is one the CPU wraps by itself
	# (which reaches the ucode as loop_len = 0).
	lh t9, %lo(CH_VATEND)
	bnez t9, VadpcmTapsDone
	lh s0, %lo(CH_VLIMIT)                    # one sample past the last one
	lw t3, %lo(CH_LOOP_STATE)
	beqz t3, VadpcmTapsHold
	li s1, %lo(MIX_VADPCM_CODEBOOK+128)
	lh t0, 0x00(s1)
	lh t1, 0x02(s1)
	lh t2, 0x04(s1)
	sh t0, 0x00(s0)
	sh t1, 0x02(s0)
	j VadpcmTapsDone
	sh t2, 0x04(s0)

VadpcmTapsHold:
	# Nothing to interpolate towards: a waveform that does not loop. Hold the
	# last sample, which at least does not decay towards silence.
	lh t0, -2(s0)
	sh t0, 0x00(s0)
	sh t0, 0x02(s0)
	sh t0, 0x04(s0)
VadpcmTapsDone:

	# The decoder clobbers ticks/pos/len/loop_len/addr, k_ff, s2, $v01-$v23
	# and $v30/$v31 (which it needs as the shift tables of vsra8).
	lh ticks, %lo(CH_TICKS)
	lw wv_pos, %lo(CH_POS)
	lw wv_len, %lo(CH_LEN)
	lw wv_loop_len, %lo(CH_LOOP_LEN)
	lw wv_addr, %lo(CH_PTR)
	li t0, %lo(VCONST_1)
	lqv v_const1, 0,t0
	li t0, %lo(DMEM_VMID)
	lqv v_xvol4, 0,t0
	lh t1, %lo(CH_VLIMIT)
	mtc2 t1, k_limit

	srl s2, wv_pos, WAVEFORM_POS_FRAC_BITS
	andi s2, 15                              # offset of wv_pos in the cache
	sll wv_step_8x, wv_step, 3
	j MonoRefill
	li k_ff, 0xFF

VadpcmEpilog:
	# The resampler mixes in whole groups of eight and can run up to seven
	# samples past the end of the round (ACCUM_PAD absorbs them). The next
	# command resumes from the position the CPU keeps, which is the one the
	# round really ended on: rewind to it, or the state saved below would be
	# the one of a later frame and the decode would restart from the wrong
	# samples.
	bgez ticks, 3f
	nop
1:	addi ticks, 1
	bltz ticks, 1b
	sub wv_pos, wv_step

3:	# The state that decodes frame F is just the eight samples before it, so
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

MIX_CHANNEL_SilenceLeft:
	# A channel that runs out of data mixes nothing for the rest of the
	# round, and there is no state left to carry across it: the volume of
	# every round is computed from scratch by the CPU.

MIX_CHANNEL_Done:
	# Hand $v00 back to the next command as the zero vector: the mixing
	# loop borrows it for the lower bound of the volume clamp, and not
	# every path out of it goes through a reload.
	vxor v_zero, v_zero, v_zero
	j RSPQ_Loop
	nop
	.endfunc


###############################################################
# MIX_FLUSH – DMA ACCUM to RDRAM, clear ACCUM, mark the round as done.
#
# a0: [nsamples:16][pad:16]
# a1: output RDRAM pointer (physical, 8-byte aligned)
# a2: round id
# a3: RDRAM pointer where the id of the last round done is published
#
# The DMA engine ignores the low 3 bits of both addresses, so ACCUM can only
# be sent as-is to an 8-byte aligned pointer. The CPU guarantees that: the
# output buffer is aligned and every round mixes an even number of stereo
# samples (see mixer_round_length).
#
# The marker is what lets the CPU know how far behind the RSP is. Every
# MIX_CHANNEL of the round has read its input window by the time we get here,
# and transfers run in order, so a CPU that sees the id knows those windows
# are free and can refill them (see samplebuffer_append).
###############################################################

	.func MIX_FLUSH
MIX_FLUSH:
	andi t4, a0, 0xFFFF            # nsamples
	beqz t4, MIX_FLUSH_Mark
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

MIX_FLUSH_Mark:
	sw a2, %lo(ROUND_MARKER) + 0
	sw a2, %lo(ROUND_MARKER) + 4
	move s0, a3
	li s4, %lo(ROUND_MARKER)
	jal DMAOut
	li t0, DMA_SIZE(8, 1)

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


###############################################################
# MIX_SETSTATE – seed the VADPCM decoder state of one channel.
#
# Command words (24 bytes):
#   a0: [ovl+cmd:8][pad:24]
#   a1: state  (the buffer MIX_SETCHANNEL registered for the channel)
#   a2: state word 0
#   a3: state word 1
#   +16: state word 2
#   +20: state word 3
#
# The CPU re-seeds the decoder whenever it moves a channel somewhere the
# stream does not reach on its own: a seek, or the wrap of a loop too large to
# be pinned in the samplebuffer. Writing that state to RDRAM directly would
# race with the epilog of every MIX_CHANNEL, which saves its own state to the
# same buffer whenever the RSP gets to it — several rounds behind the CPU, and
# at a different moment for each plane of a stereo pair. Carrying it through
# the queue instead puts it after every round already enqueued and before the
# one that has to decode from it.
###############################################################

	.func MIX_SETSTATE
MIX_SETSTATE:
	addi t0, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 0x10
	li s4, %lo(RSPQ_SCRATCH_MEM)
	lqv $v01, 0x00,t0
	lrv $v01, 0x10,t0
	move s0, a1
	li t0, DMA_SIZE(16, 1)
	j DMAOut
	sqv $v01, 0x00,s4
	.endfunc


###############################################################
# MIX_COPY – copy a block of RDRAM onto another one.
#
# Command words (12 bytes):
#   a0: [ovl+cmd:8][pad:8][nbytes:16]
#   a1: destination (physical, 8-byte aligned)
#   a2: source      (physical, 8-byte aligned)
#
# The samplebuffers use it to refresh the copy of their first samples that
# sits past the end of the ring (see samplebuffer_commit), when those samples
# are written by the RSP itself (any codec that decodes with an overlay): the
# copy has to travel through the queue to land after them.
#
# nbytes may not exceed the sample cache this stages the copy through, which
# is scratch to every other command.
###############################################################

	.func MIX_COPY
MIX_COPY:
	andi t0, a0, 0xFFFF
	move s0, a2
	addi t0, -1
	jal DMAIn
	li s4, %lo(DMEM_SAMPLE_CACHE)

	andi t0, a0, 0xFFFF
	move s0, a1
	addi t0, -1
	jal DMAOut
	li s4, %lo(DMEM_SAMPLE_CACHE)

	j RSPQ_Loop
	nop
	.endfunc

	#undef ticks
	#undef wv_pos
	#undef wv_step
	#undef wv_len
	#undef wv_loop_len
	#undef wv_addr
	#undef wv_step_8x
	#undef acc_ptr
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
# One base pointer per payload window of the frame being decoded (see the
# unpack in VADPCM_DecodeLoop). in0 also carries the control byte, at -1.
#define in0                 s5
#define in1                 t9
#define in2                 s0
#define in3                 s4
#define vstride             t2
# Value in3 reaches once the last frame of the fill has been read, which is
# what ends the loop (no separate frame counter).
#define in_end              t3
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
# across the decode. $v04 is one the decoder does not otherwise use, and the
# mixer only keeps powers of x there, which it recomputes every group.
#define v____   $v04
# The four payload halfwords of a frame, one per lane pair.
#define vsrc    $v05
# Unpack multipliers, in the two registers MonoRefill rebuilds from scratch
# after every fill (v_frac and v_idx), plus 2^bits in a lane of v_kmono.
#define vk0     $v26
#define vk1     $v27
#define kext    v_kmono.e4

	# Decode CH_VDECN frames starting at CH_VNEXT_FRAME into
	# DMEM_SAMPLE_CACHE. Advances CH_VNEXT_FRAME by CH_VDECN.
	# Clobbers t2, t3, t4, t6, t7, t8, t9, s0-s5 and $v01-$v23, $v26, $v27,
	# $v30, $v31. $v00 is deliberately left alone, see v____.
	.func VadpcmDecodeFrames
VadpcmDecodeFrames:
	sw ra, %lo(CH_VADPCM_RA)

	# MIX_CHANNEL keeps its constants in $v31 (v_const1) and one of its two
	# volume vectors in $v30 (v_xvol4), but the decoder needs the RSPQ shift
	# tables there. Restore them the same way RSPQ_Loop does before each
	# command; the caller saves the volume and reloads both.
	lpv vshift8, 0x00,zero
	vsrl vshift, vshift8, 8

	# Unpack multipliers of this stream, the entry of VADPCM_K0 for its
	# residual width. They are reloaded on every call because the registers
	# they live in are two MonoRefill rebuilds after each fill. K1 serves the
	# odd residual of each lane pair, which sits exactly `bits` bits further
	# into the halfword.
	lh t3, %lo(CH_VBITS)
	lw t0, %lo(CH_VNEXT_FRAME)
	lh t1, %lo(CH_VMULSH)
	sll s0, t3, 4
	addi s0, %lo(VADPCM_K0 - 32)          # entry of this width: (bits-2)*16
	ldv vk0.e0, 0x00,s0
	ldv vk0.e4, 0x00,s0
	lh t2, %lo(CH_VMULNEG)
	lw s0, %lo(CH_PTR)
	vmudn vk1, vk0, kext

	# RDRAM address of the first compressed frame: ptr + next_frame*stride,
	# with the stride multiply decomposed as in VadpcmSetup.
	sllv s1, t0, t1
	xor s2, t0, t2
	sub s2, t2
	add s1, s2
	add s0, s1
	andi t9, s0, 7                        # misalignment (survives DMAIn)
	sub s0, t9
	lh nframes, %lo(CH_VDECN)
	# nframes*stride bytes, plus the 8-byte lookahead and the misalignment
	# that DMAIn folds into s4.
	sllv t0, nframes, t1
	xor s1, nframes, t2
	sub s1, t2
	add t0, s1
	move in_end, t0                       # kept across the DMA for the loop test
	addi t0, 16
	# The compressed frames are staged in the tail of the very window they
	# decode into, which costs no DMEM of its own: parked so that the fill
	# ends where the window does, the decoder overtakes them just fast enough
	# never to reach one it has not read yet. Storing the samples of frame k
	# ends 32(k+1) bytes into the window, while the frames still to be read
	# start at least (nframes-k-2)*stride bytes before its end: a frame is 32
	# bytes of samples against 9 of residuals at its widest, so the samples
	# stay behind for as long as there are frames left, whatever the width
	# and however short the fill (a full one is exactly the window).
	addi s1, t0, 7
	andi s1, 0xFFF8                       # what the DMA writes, rounding included
	li s4, %lo(DMEM_SAMPLE_CACHE_ZERO)
	sub s4, s1
	jal DMAIn
	addi t0, -1                            # = DMA_SIZE(t0, 1)

	# The eight registers of the predictor matrix keep the identity on their
	# diagonal, which ltv writes here: every frame only rewrites the column
	# the codebook provides (see the priming below). s0 is free of the DMA
	# address only at this point.
	#
	# The rest builds the payload window of each group of four residuals:
	# group j starts at byte (j*bits)>>1 of the payload, that is at 0,
	# bits>>1, bits and bits + (bits>>1).
	li s0, %lo(IDENTITY)
	vxor pred1a, pred1a;                  li dmem_codebook, %lo(MIX_VADPCM_CODEBOOK)
	vxor pred1b, pred1b;                  li dmem_state, %lo(MIX_VADPCM_STATE)
	vxor pred1c, pred1c;                  li dmem_output, %lo(DMEM_SAMPLE_CACHE)
	vxor pred1d, pred1d;                  lh t0, %lo(CH_VBITS)
	vxor pred1e, pred1e;                  add in0, s4, t9
	vxor pred1f, pred1f;                  addiu in0, 1      # past the control byte
	vxor pred1g, pred1g;                  srl t1, t0, 1
	vxor pred1h, pred1h;                  add in1, in0, t1
	ltv pred1a.e0, 0,s0
	add in2, in0, t0                      # in2 is s0: only free once ltv is done
	add in3, in2, t1
	add in_end, in3                       # in3 after the last frame of the fill
	sll vstride, t0, 1
	addiu vstride, 1                      # frame stride: 2*bits + 1

	lqv vstate1, 0x00,dmem_state
	li t1, 1

	# Residual unpacking, shared by the priming below and the decode loop.
	#
	# The 16 residuals of a frame are read as four halfwords, one per group of
	# four consecutive residuals, and .q0 hands each halfword to the lane pair
	# that holds the group: lanes 2j and 2j+1 of vres0 take residuals 4j and
	# 4j+2, the same lanes of vres1 take 4j+1 and 4j+3.
	#
	# A lane isolates its residual in two steps. The vmudn keeps the low 16
	# bits of halfword * K, with K the power of two that brings the top of the
	# residual to bit 15: everything before the residual falls off the top.
	# The vmudm then multiplies by 2^bits and keeps bits 31..16 of the result,
	# which is an arithmetic shift right by 16-bits: it drops everything after
	# the residual and replicates its sign. Both leave a value in the 15.0
	# range that the frame scale then shifts up.
	#
	# In:  in0..in3, one per group; vk0 (even residuals), vk1 (odd ones)
	# Out: next_vres0 (even residuals), next_vres1 (odd ones), sign extended

VADPCM_Priming:
	lbu t0, -1(in0)
	lsv vsrc.e0, 0x00,in0
	lsv vsrc.e2, 0x00,in1
	lsv vsrc.e4, 0x00,in2
	lsv vsrc.e6, 0x00,in3
	srl scaling, t0, 4
	andi predictor, t0, 0xF
	sllv scaling, t1, scaling
	sll predictor, VADPCM_ORDER_SHIFT + 4
	add predictor, dmem_codebook
	mtc2 scaling, vscale.e0;                    vmudn next_vres0, vk0, vsrc.q0
	lqv pred0.e0,  0x00,predictor;              vmudn next_vres1, vk1, vsrc.q0
	lqv pred1.e0,  0x10,predictor
	lqv pred1a.e1, 0x10,predictor
	lqv pred1b.e2, 0x10,predictor;              vmudm next_vres0, next_vres0, kext
	lqv pred1c.e3, 0x10,predictor;              vmudm next_vres1, next_vres1, kext
	lqv pred1d.e4, 0x10,predictor
	lqv pred1e.e5, 0x10,predictor
	lqv pred1f.e6, 0x10,predictor
	lqv pred1g.e7, 0x10,predictor
	# vres1 of this first frame is computed by the top of the loop.
	vmudh vres0, next_vres0, vscale.e0
	lqv vresult, 0x00,dmem_state

	# The loop advances in0..in2 at its bottom and in3 at its top, so that
	# each of them has the whole body between the advance and the load it
	# feeds. Leave the first three on the frame the loop reads first.
	addu in0, vstride
	addu in1, vstride
	addu in2, vstride

	li slot_ptr, %lo(RSPQ_SCRATCH_MEM)
	vnop

	.balign 8
VADPCM_DecodeLoop:
	# One iteration decodes one frame, that is 16 samples, in ~37 cycles:
	# 2.3 cycles per sample, against the 8.3 that the resampler above spends
	# on each output sample it then interpolates from them.
	vmudh vres1, next_vres1, vscale.e0; addu in3, vstride
	vmudh v____,  pred1a, vres0.e0;    lsv vsrc.e0, 0x00,in0
	vmadh v____,  pred1c, vres0.e1;    sqv vresult, 0,slot_ptr
	vmadh v____,  pred1e, vres0.e2;    lsv vsrc.e2, 0x00,in1
	vmadh v____,  pred1g, vres0.e3;    sqv vresult, -16,dmem_output
	vmadh v____,  pred1b, vres1.e0;    lqv vstate1, 0,slot_ptr
	vmadh v____,  pred1d, vres1.e1;    srv vresult, 0x00,dmem_output
	vmadh v____,  pred1f, vres1.e2;    lsv vsrc.e4, 0x00,in2
	vmadh v____,  pred1h, vres1.e3;    lbu t0, -1(in0)
	vmadh v____,  pred0,  vstate1.e6;  lsv vsrc.e6, 0x00,in3
	vmadh v____,  pred1,  vstate1.e7

	vsar  vstatef, COP2_ACC_MD;        srl scaling, t0, 4
	vsar  vstate0, COP2_ACC_HI;        andi predictor, t0, 0xF
	vmudn next_vres0, vk0, vsrc.q0;    sllv scaling, t1, scaling
	vmudn next_vres1, vk1, vsrc.q0;    sll predictor, VADPCM_ORDER_SHIFT + 4
	vmudn v____, vstatef, vshift.e2;   add predictor, dmem_codebook
	vmadh vstate0, vstate0, vshift.e2; mtc2 scaling, vscale.e0

	vmudh v____,  pred1a, vres0.e4;    addu in0, vstride
	vmadh v____,  pred1b, vres1.e4;    lqv pred1a.e1, 0x10,predictor
	vmadh v____,  pred1c, vres0.e5;    lqv pred1b.e2, 0x10,predictor
	vmadh v____,  pred1d, vres1.e5;    lqv pred1c.e3, 0x10,predictor
	vmadh v____,  pred1e, vres0.e6;    lqv pred1d.e4, 0x10,predictor
	vmadh v____,  pred1f, vres1.e6;    lqv pred1e.e5, 0x10,predictor
	vmadh v____,  pred1g, vres0.e7;    lqv pred1f.e6, 0x10,predictor
	vmadh v____,  pred1h, vres1.e7;    lqv pred1g.e7, 0x10,predictor
	vmadh v____,  pred0,  vstate0.e6;  addu in1, vstride
	vmadh v____,  pred1,  vstate0.e7;  addu in2, vstride

	vsar  vstatef, COP2_ACC_MD;         sqv vstate0, 0x00,dmem_output
	vsar  vresult, COP2_ACC_HI;         srv vstate0, 0x10,dmem_output
	vmudm next_vres0, next_vres0, kext; lqv pred0.e0, 0x00,predictor
	vmudm next_vres1, next_vres1, kext; lqv pred1.e0, 0x10,predictor
	vmudn v____, vstatef, vshift.e2;    addiu dmem_output, 32
	vmadh vresult, vresult, vshift.e2;  bne in3, in_end, VADPCM_DecodeLoop
	 vmudh vres0, next_vres0, vscale.e0

	sqv vresult, -16,dmem_output
	srv vresult, 0x00,dmem_output
	sqv vresult, 0,slot_ptr
	lqv vstate1, 0,slot_ptr

	lw t0, %lo(CH_VNEXT_FRAME)
	lh t1, %lo(CH_VDECN)
	lw ra, %lo(CH_VADPCM_RA)
	add t0, t1
	sqv vstate1, 0x00,dmem_state
	jr ra
	sw t0, %lo(CH_VNEXT_FRAME)
	.endfunc
