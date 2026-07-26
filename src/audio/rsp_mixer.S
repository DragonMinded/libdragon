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

# Resample staging (8 samples) and volume spill during VADPCM decode
# (4 vectors).
#define DMEM_SCRATCH_SIZE         64

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
	#define v_shift8      $v29
	#define v_shift       $v30
	#define v_const1      $v31

	#define k_0000        v_zero
	#define k_8000        v_shift8.e0
	#define k_ffff        v_const1.e0
	#define k_alpha       v_const1.e1
	#define k_1malpha     v_const1.e2


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

	.align 4
BANNER0:    .ascii "Dragon RSP Audio"
BANNER1:    .ascii " Coded by Rasky "

	# ================================================================
	# Overlay saved state – survives RSPQ overlay swaps.
	# Layout must match mixer_overlay_state_t in mixer.c.
	# ================================================================
	RSPQ_BeginSavedState
	.align 4
XVOL_L:                   .dcb.w MAX_CHANNELS
XVOL_R:                   .dcb.w MAX_CHANNELS
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
	.align 4
ACCUM:                    .dcb.w (MAX_SAMPLES_PER_ROUND * 2)

	# Temporary cache of samples fetched by DMA for resampling.
	.align 3
DMEM_SAMPLE_CACHE:        .dcb.b SAMPLE_CACHE_SIZE

	# Contiguous resample staging, plus volume spill during VADPCM decode.
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
	li t0, (MAX_SAMPLES_PER_ROUND * 4) / 64 - 1
1:	sqv v_zero, 0x00,s4
	sqv v_zero, 0x10,s4
	sqv v_zero, 0x20,s4
	sqv v_zero, 0x30,s4
	addi s4, 64
	bnez t0, 1b
	addi t0, -1

MIX_CHANNEL_NoClear:
	# Load current filtered volumes for this channel into vector regs
	# (kept in $v13/$v14 for the duration of the command).
	lh ch_idx, %lo(CH_IDX)
	sll t0, ch_idx, 1
	lh t1, %lo(XVOL_L)(t0)
	lh t2, %lo(XVOL_R)(t0)
	mtc2 t1, $v13.e0               # v_xvol_l
	mtc2 t2, $v14.e0               # v_xvol_r
	vor $v13, v_zero, $v13.e0
	vor $v14, v_zero, $v14.e0

	# Target volumes
	lh t1, %lo(CH_LVOL)
	lh t2, %lo(CH_RVOL)
	mtc2 t1, $v15.e0               # v_chvol_l
	mtc2 t2, $v16.e0               # v_chvol_r
	vor $v15, v_zero, $v15.e0
	vor $v16, v_zero, $v16.e0

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

	# Route to the correct resampling path. Stereo and stereo-sub share the
	# interleaved fetch; sub extracts the R sample.
	bnez is_stereo_sub, WaveGoStereo
	sub wv_pos, wv_pos_to_dmem
	bnez is_stereo, WaveGoStereo
	nop
	bnez is_16bit, WaveLoop16_8x
	nop
	j WaveLoop8_8x
	nop

WaveGoStereo:
	bnez is_16bit, WaveLoop16_Stereo_8x
	nop
	j WaveLoop8StereoChecks
	nop

	############################################################
	# Mono 8-bit
	############################################################

WaveLoop8_8x:
	add t0, wv_pos, wv_step_8x
	bgt t0, dma_cache_end, WaveLoop8Checks
	li t0, 8
	blt ticks, t0, WaveLoop8Checks

	li out_ptr, %lo(DMEM_SCRATCH)
	li t1, 8
1:	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, 0(out_ptr)
	addi out_ptr, 1
	addi t1, -1
	bnez t1, 1b
	nop

	jal MixBatch8bit
	li t0, 8
	addi ticks, -8
	j WaveLoop8_8x
	addi acc_ptr, 8*4

WaveLoop8:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	# sign extend and mix one sample
	sll t0, 8
	mtc2 t0, $v01.e0
	jal MixOne
	nop
	addi ticks, -1
	addi acc_ptr, 4
WaveLoop8Checks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop8
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem

	############################################################
	# Mono 16-bit
	############################################################

WaveLoop16_8x:
	add t0, wv_pos, wv_step_8x
	bgt t0, dma_cache_end, WaveLoop16Checks
	li t0, 8
	blt ticks, t0, WaveLoop16Checks

	li out_ptr, %lo(DMEM_SCRATCH)
	li t1, 8
1:	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, 0(out_ptr)
	addi out_ptr, 2
	addi t1, -1
	bnez t1, 1b
	nop

	jal MixBatch16bit
	nop
	addi ticks, -8
	j WaveLoop16_8x
	addi acc_ptr, 8*4

WaveLoop16:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	mtc2 t0, $v01.e0
	jal MixOne
	nop
	addi ticks, -1
	addi acc_ptr, 4
WaveLoop16Checks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop16
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem

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
	# A frame decode clobbers $v13-$v16 (filtered/target volumes) and may
	# clobber k1. Spill them to DMEM_SCRATCH (unused in the VADPCM path) so
	# the post-decode reload in VadpcmDecodeCur resumes the volume filter
	# from the current value instead of the command-start one (which would
	# stall the filter convergence).
	li t0, %lo(DMEM_SCRATCH)
	sqv $v13, 0x00,t0
	sqv $v14, 0x10,t0
	sqv $v15, 0x20,t0
	sqv $v16, 0x30,t0
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
	# Decode clobbers $v31 (vsra8 uses vshift8). Reload mixer constants
	# and unspill the volumes saved in VadpcmHavePos before resampling.
	li t0, %lo(VCONST_1)
	lqv v_const1, 0,t0
	li t0, %lo(DMEM_SCRATCH)
	lqv $v13, 0x00,t0
	lqv $v14, 0x10,t0
	lqv $v15, 0x20,t0
	lqv $v16, 0x30,t0
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
	vmudm $v13, $v13, k_alpha
	vmadm $v13, $v15, k_1malpha
	vmudm $v14, $v14, k_alpha
	vmadm $v14, $v16, k_1malpha
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
	vmudm $v13, $v13, k_alpha
	vmadm $v13, $v15, k_1malpha
	vmudm $v14, $v14, k_alpha
	vmadm $v14, $v16, k_1malpha
#endif
	sub ticks, t0
	sll t1, t0, 2                  # each sample = 4 bytes in ACCUM
	j SilenceLoop
	add acc_ptr, t1

MIX_CHANNEL_Done:
	# Store updated xvol for this channel
	lh ch_idx, %lo(CH_IDX)
	sll t0, ch_idx, 1
	mfc2 t1, $v13.e0
	mfc2 t2, $v14.e0
	sh t1, %lo(XVOL_L)(t0)
	sh t2, %lo(XVOL_R)(t0)
	j RSPQ_Loop
	nop
	.endfunc


###############################################################
# MixBatch16bit – mix 8 mono int16 samples from DMEM_SCRATCH
# into ACCUM at acc_ptr, then update the volume filter.
#
# DMEM_SCRATCH holds: s0 s1 s2 s3 s4 s5 s6 s7 (halfwords)
# $v13/$v14 = xvol_l / xvol_r (broadcast)
# $v15/$v16 = chvol_l / chvol_r (broadcast)
###############################################################

	.func MixBatch16bit
MixBatch16bit:
	move ra2, ra
	li s0, %lo(DMEM_SCRATCH)
	lqv $v01, 0,s0                 # 8 samples

	# Apply volumes
	vmulf $v02, $v01, $v13         # left  = sample * xvol_l
	vmulf $v03, $v01, $v14         # right = sample * xvol_r

	# Interleave L/R into two vectors matching ACCUM layout and add.
	# Build L0 R0 L1 R1 L2 R2 L3 R3 in $v04 via even/odd merge.
	li t0, 0x5555
	ctc2 t0, COP2_CTRL_VCC
	vcopy $v04, $v02.q0            # L0 L0 L2 L2 L4 L4 L6 L6
	vcopy $v05, $v02.q1            # L1 L1 L3 L3 L5 L5 L7 L7
	vmrg $v04, $v04, $v03.q0       # L0 R0 L2 R2 L4 R4 L6 R6
	vmrg $v05, $v05, $v03.q1       # L1 R1 L3 R3 L5 R5 L7 R7

	# $v04 has even pairs, $v05 odd pairs – store via slv like VADPCM deint,
	# but we need A0 B0 A1 B1 order. Rebuild with a temp buffer instead
	# (clearer and not too expensive for 8 samples).
	ssv $v02.e0,  0,s0
	ssv $v03.e0,  2,s0
	ssv $v02.e1,  4,s0
	ssv $v03.e1,  6,s0
	ssv $v02.e2,  8,s0
	ssv $v03.e2, 10,s0
	ssv $v02.e3, 12,s0
	ssv $v03.e3, 14,s0
	ssv $v02.e4, 16,s0
	ssv $v03.e4, 18,s0
	ssv $v02.e5, 20,s0
	ssv $v03.e5, 22,s0
	ssv $v02.e6, 24,s0
	ssv $v03.e6, 26,s0
	ssv $v02.e7, 28,s0
	ssv $v03.e7, 30,s0

	lqv $v04, 0x00,s0
	lqv $v05, 0x10,s0
	lqv $v06, 0x00,acc_ptr
	lqv $v07, 0x10,acc_ptr
	vadd $v06, $v06, $v04
	vadd $v07, $v07, $v05
	sqv $v06, 0x00,acc_ptr
	sqv $v07, 0x10,acc_ptr

#if VOLUME_FILTER
	vmudm $v13, $v13, k_alpha
	vmadm $v13, $v15, k_1malpha
	vmudm $v14, $v14, k_alpha
	vmadm $v14, $v16, k_1malpha
#endif
	jr ra2
	nop
	.endfunc


###############################################################
# MixBatch8bit – promote 8 signed bytes in DMEM_SCRATCH[0..7]
# to (sample<<8) halfwords, then mix via MixBatch16bit.
###############################################################

	.func MixBatch8bit
MixBatch8bit:
	move t9, ra
	li s0, %lo(DMEM_SCRATCH)
	# lpv places each byte into bits 15..8 of a lane (= sample<<8),
	# matching the legacy mixer which stored 8-bit samples as the high
	# byte of a cleared halfword.
	lpv $v01, 0,s0
	sqv $v01, 0,s0
	jal MixBatch16bit
	nop
	jr t9
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
	vmulf $v02, $v01, $v13
	vmulf $v03, $v01, $v14
	lsv $v04.e0, 0,acc_ptr
	lsv $v05.e0, 2,acc_ptr
	vadd $v04, $v04, $v02
	vadd $v05, $v05, $v03
	ssv $v04.e0, 0,acc_ptr
	jr ra
	ssv $v05.e0, 2,acc_ptr
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
	li t0, (MAX_SAMPLES_PER_ROUND * 4) / 64 - 1
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
