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
	# Mixing is driven by two RSPQ commands:
	#
	#   MIX_CHANNEL  – resample one channel for up to MAX_SAMPLES_PER_ROUND
	#                  output ticks and accumulate into a stereo DMEM buffer
	#                  (ACCUM) in .bss. Optionally decode mono VADPCM frames
	#                  into a DMEM window first (CH_FLAGS_VADPCM).
	#   MIX_FLUSH    – DMA ACCUM out to RDRAM, clear it, publish the round id.
	#
	# All MIX_CHANNEL + MIX_FLUSH of a round are enqueued in one highpri burst
	# of this overlay so ACCUM cannot be clobbered mid-round. XVOL_L/R (one-tap
	# volume filter state) live in saved state so volume ramps survive swaps.
	#
	# Channel parameters travel inline in the MIX_CHANNEL command – there is
	# no settings DMA ring. VADPCM channels also pass codebook/state/loop_state.
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
#define MAX_SAMPLES_PER_ROUND     256

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

# Scratch size for memmove bounce buffer / resample staging. Must be a
# multiple of 8 and large enough for useful memmove chunks.
#define DMEM_SCRATCH_SIZE         1536

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
		RSPQ_DefineCommand MIX_CHANNEL,       48		# 0x0
		RSPQ_DefineCommand VADPCM_Decompress, 16		# 0x1
		RSPQ_DefineCommand VADPCM_SetState,    8		# 0x2
		RSPQ_DefineCommand command_memmove,   12		# 0x3
		RSPQ_DefineCommand MIX_FLUSH,         12		# 0x4
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
	.align 4
COUNTER_RDRAM:            .long  0
	                      .long  0
	                      .long  0
	                      .long  0
	RSPQ_EndSavedState

	.align 4
IDENTITY:  .half  1<<11,1<<11,1<<11,1<<11, 1<<11,1<<11,1<<11,1<<11

	.bss

	# Stereo int16 accumulator for one mix round (LRLR...). Lives in .bss
	# (not saved state) so the DMEM budget still fits the VADPCM bssovl1
	# bank. All MIX_CHANNEL + MIX_FLUSH of a round are enqueued in one
	# highpri burst of this overlay, so no overlay switch can clobber it
	# mid-round. Cleared by the first MIX_CHANNEL (CH_FLAGS_CLEAR_ACCUM)
	# and again by MIX_FLUSH after DMA-out.
	.align 4
ACCUM:                    .dcb.w (MAX_SAMPLES_PER_ROUND * 2)

	# Temporary cache of samples fetched by DMA for resampling.
	.align 3
DMEM_SAMPLE_CACHE:        .dcb.b SAMPLE_CACHE_SIZE

	# Contiguous resample staging (8 samples) plus bounce buffer for
	# command_memmove.
	.align 4
DMEM_SCRATCH:             .dcb.b DMEM_SCRATCH_SIZE

	# In-mixer VADPCM staging (mono). Lives in .bss with ACCUM; not used by
	# the legacy VADPCM_Decompress path (which uses the bssovl1 bank).
	.align 4
MIX_VADPCM_CODEBOOK:      .space 256
	.align 4
MIX_VADPCM_STATE:         .space 16
	.align 4
MIX_VADPCM_STATE_BEFORE:  .space 16
	.align 3
MIX_VADPCM_COMP:          .space (VADPCM_WIN_FRAMES * 9 + 8)
	.align 4
MIX_VADPCM_PCM:           .space (VADPCM_WIN_SAMPLES * 2)

	# Scratch used to DMA the round id out to COUNTER_RDRAM.
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
# Nonzero: VADPCM_SaveState returns to VadpcmDecodeFrame instead of DMA-out.
CH_VADPCM_MIXRET:         .long  0
CH_VADPCM_RA:             .long  0
CH_ACC_PTR:               .long  0
# Volume-filter step countdown, preserved across frame decodes.
CH_K1:                    .half  0
                          .half  0


	.text 1

###############################################################
# MIX_CHANNEL – resample one channel and accumulate into ACCUM.
#
# Command words (48 bytes):
#   a0: [ovl+cmd:8][ch_idx:8][flags:8][pad:8]   (24-bit payload)
#   a1: [lvol:16][rvol:16]
#   a2: pos   (fixed point, WAVEFORM_POS_FRAC_BITS)
#   a3: step
#   +16: len
#   +20: loop_len
#   +24: ptr  (0 = silence: only ramp volume toward target)
#   +28: [acc_offset:16][nsamples:16]
#   +32: codebook   (VADPCM; 0 for PCM)
#   +36: state      (VADPCM decoder state in RDRAM)
#   +40: loop_state (VADPCM state at loop start; 0 if none)
#   +44: pad
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
	lw t0, CMD_ADDR(0x10, 0x30)
	sw t0, %lo(CH_LEN)
	lw t0, CMD_ADDR(0x14, 0x30)
	sw t0, %lo(CH_LOOP_LEN)
	lw t0, CMD_ADDR(0x18, 0x30)
	sw t0, %lo(CH_PTR)
	lw t0, CMD_ADDR(0x1C, 0x30)
	sh t0, %lo(CH_NSAMPLES)
	srl t0, 16
	sh t0, %lo(CH_ACC_OFF)
	lw t0, CMD_ADDR(0x20, 0x30)
	sw t0, %lo(CH_CODEBOOK)
	lw t0, CMD_ADDR(0x24, 0x30)
	sw t0, %lo(CH_STATE)
	lw t0, CMD_ADDR(0x28, 0x30)
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
	li t0, DMA_SIZE(256, 1)

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
	# Ensure the current sample's frame is loaded.
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	srl t1, t0, 4
	lw t2, %lo(CH_VCUR_FRAME)
	bne t1, t2, VadpcmHavePos
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
# MIX_FLUSH – DMA ACCUM to RDRAM, clear ACCUM, publish round id.
#
# a0: [nsamples:16][pad:16]
# a1: output RDRAM pointer (physical)
# a2: round_id
###############################################################

	.func MIX_FLUSH
MIX_FLUSH:
	andi t4, a0, 0xFFFF            # nsamples
	move s1, a1                    # output rdram
	move k1, a2                    # round_id

	beqz t4, MIX_FLUSH_Publish
	nop

	# Handle 4-byte-but-not-8-byte aligned output: fetch the 8-byte DMA
	# transfer so the leading 4 bytes (before our buffer) are preserved,
	# then splice ACCUM starting at the misaligned offset.
	andi t1, s1, 7
	beqz t1, MIX_FLUSH_Dma

	move s0, s1
	li s4, %lo(DMEM_SCRATCH)
	jal DMAIn
	li t0, DMA_SIZE(8, 1)
	# s4 points at the first requested byte. Copy ACCUM over it.
	li t0, %lo(ACCUM)
	sll t2, t4, 2
1:	lw t3, 0(t0)
	sw t3, 0(s4)
	addi t0, 4
	addi s4, 4
	addi t2, -4
	bgtz t2, 1b
	nop
	li s4, %lo(DMEM_SCRATCH)
	move s0, s1
	sll t0, t4, 2
	addi t0, 3                     # +4 bytes of prefix - 1 for DMA_SIZE
	jal DMAOut
	nop
	j MIX_FLUSH_Clear
	nop

MIX_FLUSH_Dma:
	li s4, %lo(ACCUM)
	move s0, s1
	sll t0, t4, 2
	addi t0, -1
	jal DMAOut
	nop

MIX_FLUSH_Clear:
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

MIX_FLUSH_Publish:
	sw k1, %lo(ROUND_SCRATCH)
	li s4, %lo(ROUND_SCRATCH)
	lw s0, %lo(COUNTER_RDRAM)
	li t0, DMA_SIZE(8, 1)
	jal_and_j DMAOutAsync, RSPQ_Loop
	.endfunc


###############################################################
# command_memmove
#
# Backward move of bytes in RDRAM (dst < src). Chunks staged
# through DMEM_SCRATCH.
#
# a0: RDRAM destination
# a1: RDRAM source
# a2: byte count (multiple of 8)
###############################################################

	#define MEMMOVE_CHUNK  DMEM_SCRATCH_SIZE

	.func command_memmove
command_memmove:
	#if RSPQ_DEBUG
	andi t0, a0, 7
	assert_eq t0, 0, 0x8601
	andi t0, a1, 7
	assert_eq t0, 0, 0x8601
	andi t0, a2, 7
	assert_eq t0, 0, 0x8601
	#endif

MemmoveLoop:
	move t4, a2
	ble t4, MEMMOVE_CHUNK, 1f
	nop
	li t4, MEMMOVE_CHUNK
1:
	move s0, a1
	li s4, %lo(DMEM_SCRATCH)
	jal DMAIn
	addiu t0, t4, -1

	move s0, a0
	li s4, %lo(DMEM_SCRATCH)
	jal DMAOut
	addiu t0, t4, -1

	add a0, t4
	add a1, t4
	sub a2, t4
	bgtz a2, MemmoveLoop
	nop

	j RSPQ_Loop
	nop
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
# VADPCM decompressor
############################################################################

	########################################
	# VADPCM_Decompress
	#
	# Args:
	#   a0: pointer to input buffer
	#   a1: pointer to output buffer (+ numframes-1 in MSB)
	#   a2: pointer to state buffer  (+ bit 31 set if stereo)
	#   a3: pointer to codebook
	#
	########################################

#define VADPCM_ORDER_SHIFT    1
#define VADPCM_ORDER          (1<<VADPCM_ORDER_SHIFT)

	# VADPCM State and Codebook
#define VADPCM_CODEBOOK_SIZE  8

#define MAX_VADPCM_FRAMES     94

	.section .bssovl1

					# Codebook must be aligned to allow for xor trick
					# to switch channels
					.balign VADPCM_CODEBOOK_SIZE*16 
VADPCM_CODEBOOK:   	.space VADPCM_CODEBOOK_SIZE*16*2

					.align 4
VADPCM_STATE:    	.space 16*2   # state, left/right
VADPCM_BUFFER:      .space MAX_VADPCM_FRAMES*16*2		
VADPCM_BUFFER_END:	.space 8    # Extra 8 bytes for misalignment


	.text

#define scaling    			t8
#define predictor  			t7
#define nframes    			t6
#define dmem_state          s1
#define dmem_codebook       s2
#define dmem_output         s3
#define dmem_input          s5
#define buffer_in_size      v0
#define buffer_out_size     v1
#define stereo_toggle       k1
#define rdram_input		    a0
#define rdram_output	    a1
#define cb_xor              t2   // codebook toggle: 0 (mono) / VADPCM_CODEBOOK_SIZE*16 (stereo)
#define slot_xor            t3   // scratch-slot toggle: 0 (mono) / 16 (stereo)
#define slot_ptr            t4   // pointer into RSPQ_SCRATCH_MEM (2x 16-byte channel state slots)
#define db_base             t5   // stereo deinterleave: buffer base (saved before decode loop)
#define db_count            t9   // stereo deinterleave: number of L/R pairs

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

#define next_vres0 $v22   // upcoming frame's residuals, sign-extended: even lanes
#define next_vres1 $v23   // (next_vres0) and odd lanes (next_vres1), pre-scaling

#define vstate0 $v18
#define vstate1 $v19
#define vstate2 $v20
#define vstate3 $v21
#define vresult $v03   // current frame's decoded second sub-block (samples 8..15);
                       // also becomes the input state for the next same-channel frame

#define v____ 	$v29

	// Stereo deinterleave scratch: two banks of 4 registers that are dead by the
	// time the deinterleave runs. Bank A (v_il*) holds the pair being stored; bank
	// B (v_ib*) holds the next pair being computed (software-pipelined).
#define v_il0   $v06
#define v_il1   $v07
#define v_il2   $v08
#define v_il3   $v09
#define v_ib0   $v10
#define v_ib1   $v11
#define v_ib2   $v12
#define v_ib3   $v13

###############################################################
# VadpcmDecodeFrame – decode CH_VNEXT_FRAME into MIX_VADPCM_PCM.
# Reuses VADPCM_DecodeLoop / VADPCM_SaveState via CH_VADPCM_MIXRET.
# Clobbers: most SU/VU temps used by the VADPCM decompressor.
###############################################################
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
	li t0, 1
	sw t0, %lo(CH_VADPCM_MIXRET)
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

	.func VADPCM_Decompress
VADPCM_Decompress:
	# CH_VADPCM_MIXRET lives in .bss (not cleared between commands): make
	# sure VADPCM_SaveState takes the DMA-out path, not the in-mixer return.
	sw zero, %lo(CH_VADPCM_MIXRET)
	li dmem_codebook, %lo(VADPCM_CODEBOOK)
	li dmem_state, %lo(VADPCM_STATE)

	# Fetch the codebook
	move s4, dmem_codebook
	move s0, a3
	jal DMAInAsync
	li t0, DMA_SIZE(VADPCM_CODEBOOK_SIZE*16*2, 1)

	# Fetch the state
	move s4, dmem_state
	move s0, a2
	jal DMAInAsync
	li t0, DMA_SIZE(16*2, 1)

	jal VADPCM_LoadIdentity
	nop

	srl nframes, a1, 24
	bgez a2, 1f
	 addiu nframes, 1
	sll nframes, 1

1:	# Calculate offset of input buffer inside the output buffer
	li dmem_output, %lo(VADPCM_BUFFER)
	and rdram_input, 0x00FFFFFF
	and a1, 0x00FFFFFF
	sub t0, rdram_input, rdram_output
	add dmem_input, t0, dmem_output

	# Shift buffer by output misalignment
	andi t1, rdram_output, 7
	add dmem_output, t1
	add dmem_input, t1

	beqz t1, 1f
 	 sll buffer_out_size, nframes, 5		# buffer_out_size = nframes*32
	addiu buffer_out_size, 8
1:

	sll buffer_in_size, nframes, 3			# buffer_in_size = nframes*8
	andi t1, rdram_input, 7
	beqz t1, 1f	
	 add buffer_in_size, nframes             #  + nframes
	addiu buffer_in_size, 8
1:

	#ifndef NDEBUG
	# Check that dmem_input and RDRAM input have the same misalignment
	andi t3, dmem_input, 7
	andi t2, rdram_input, 7
	assert_eq t3, t2, 0x1234
	#endif

	# Fetch the input buffer. Calculate size as nframes*9+misalignemnt
	move s0, rdram_input
	addiu t0, buffer_in_size, -1
	jal DMAIn
	 move s4, dmem_input

	move stereo_toggle, a2
	lqv vstate1, 0x00,dmem_state
	lqv vstate3, 0x10,dmem_state
	addiu dmem_input, 1
	li t1, 1                          # loop-invariant: base for (1<<scaling)

	##################################################################

	# ---- VADPCM decode ----
	# VADPCM packs audio in 9-byte frames that each decode to 16 samples. A frame
	# is a control byte (high nibble = scaling shift, low nibble = predictor index
	# into the codebook) followed by 16 packed 4-bit residuals. The 16 samples are
	# produced as two sub-blocks of 8 (samples 0..7 then 8..15), each an order-2
	# linear prediction. Calling p0,p1 = codebook[2*index + 0,1] the two predictor
	# vectors and s[] the 8 samples of the PREVIOUS sub-block, sub-block sample j is
	#
	#   out[j] = p0[j]*s[6] + p1[j]*s[7]                 (state feedback, order 2)
	#          + (r[j] << 11)                            (this residual, Q11)
	#          + sum_{k<j} r[k] * v[j-1-k]               (residual convolution)
	#
	# where r[k] = ext4(residual_k) << scaling is the signed, scaled residual and
	# v = p1 is the predictor's second vector. Fixed point is Q11 throughout: the
	# codebook coefficients and the residual's own term (the "<< 11" identity) are
	# 1.0 = 2048, samples accumulate in Q11 in the 48-bit vector accumulator, and
	# the final step recovers the integer sample as clamp16(acc >> 11).
	#
	# This is evaluated as a matrix-vector product. pred1a..pred1h are the 8
	# columns of the residual->sample matrix (a lower-triangular Toeplitz matrix:
	# 2048 on the diagonal, v on the sub-diagonals); pred0,pred1 hold p0,p1 for the
	# state feedback. Even residuals live in vres0, odd residuals in vres1, already
	# multiplied by 2^scaling. A frame is decoded as: block 0 (samples 0..7) ->
	# combine 0 (extract+round) -> block 1 (samples 8..15) -> combine 1.
	#
	# The same loop decodes mono and stereo; they differ only in two parameters:
	#   cb_xor   - 0 (mono) / VADPCM_CODEBOOK_SIZE*16 (stereo). Stereo interleaves
	#              frames L0 R0 L1 R1 ...; XORing dmem_codebook by cb_xor each frame
	#              selects the L or R codebook for that frame.
	#   slot_xor - 0 (mono) / 16 (stereo). The per-channel input state lives in two
	#              16-byte slots of RSPQ_SCRATCH_MEM. Each frame reads its state from
	#              slot[cur^slot_xor] and writes its result to slot[cur]. For mono
	#              (xor 0) this is one slot carrying the running state; for stereo
	#              the two slots alternate, so each frame sees its own channel's
	#              previous frame (two frames back in the interleaved stream).
	# Stereo emits per-frame L/R blocks, so a deinterleave pass at the end turns
	# L0 R0 L1 R1 ... into sample-interleaved output.

	# ---- Priming for frame 0 (shared by both modes) ----
	# On entry the loop body expects the frame's residuals already scaled into
	# vres0/vres1 and its full convolution matrix already in pred0/pred1/pred1a..h.
	# This block establishes that for frame 0: read the control byte, sign-extend
	# the residuals, build vscale = 2^scaling and the predictor address, load the
	# matrix, and scale the residuals. (Frame 0 is the left channel for stereo, so
	# the base codebook address is correct.)
VADPCM_Priming:
	lbu t0, -1(dmem_input)
	lpv next_vres0, 0,dmem_input
	lpv next_vres1, 0,dmem_input
	srl scaling, t0, 4
	andi predictor, t0, 0xF
	sllv scaling, t1, scaling				
	mtc2 scaling, vscale.e0;					vsra8 next_vres0, next_vres0, 12
	sll predictor, VADPCM_ORDER_SHIFT + 4;      vsll  next_vres1, next_vres1, 4
	add predictor, dmem_codebook;				vsra8 next_vres1, next_vres1, 12			# next_vres1 = residual * 2^scaling
	
	# Load the whole convolution matrix for frame 0: pred0/pred1 are the two
	# predictor vectors p0,p1; pred1a.e1..pred1g.e7 fill the Toeplitz sub-diagonals
	# with v=p1 at increasing element offsets (the diagonal already holds the 2048
	# identity from init).
	lqv pred0.e0,  0x00,predictor
	lqv pred1.e0,  0x10,predictor
	lqv pred1a.e1, 0x10,predictor
	lqv pred1b.e2, 0x10,predictor
	lqv pred1c.e3, 0x10,predictor
	lqv pred1d.e4, 0x10,predictor
	lqv pred1e.e5, 0x10,predictor
	lqv pred1f.e6, 0x10,predictor
	lqv pred1g.e7, 0x10,predictor
	# Scale frame 0's residuals: vres0/vres1 = residual * 2^scaling.
	vmudh vres0, next_vres0, vscale.e0
	vmudh vres1, next_vres1, vscale.e0

	# Per-mode parameters and initial channel state. The loop carries each frame's
	# result in vresult and writes it to slot[cur] for the next same-channel frame
	# to read as input state. To make frame 0 read a valid initial state, vresult
	# is primed with the channel's incoming state (loaded earlier from dmem_state):
	# the left state for mono, the right state for stereo (frame 0 is left, so it
	# must read the left state from the other slot, seeded directly below).
	# db_base/db_count record the buffer base and L/R pair count for the stereo
	# deinterleave pass.
	li slot_ptr, %lo(RSPQ_SCRATCH_MEM)
	lqv vresult, 0x00,dmem_state      # mono: initial state = left  (delay slot, always runs)
	li slot_xor, 0
	bgez a2, VADPCM_DecodeLoop
	 li cb_xor, 0
	li slot_xor, 16
	li cb_xor, VADPCM_CODEBOOK_SIZE*16
	lqv vresult, 0x10,dmem_state       # stereo: initial state = right
	move db_base, dmem_output          # buffer base for the deinterleave
	srl db_count, nframes, 1           # number of L/R pairs
	sqv vstate1, 16,slot_ptr           # slot[1] = left initial state (read by frame 0)

	# ---- Decode loop: one frame (16 samples) per iteration ----
	# This loop runs at 35 cycles/frame (2.187 cycles/sample)
	.balign 8
VADPCM_DecodeLoop:
	# ---- block 0: samples 0..7 = matrix * residuals + state feedback ----
	# The VU accumulates the 8 residual terms (even residuals from vres0.e0..e3,
	# odd from vres1.e0..e3, each times its convolution-matrix column) and the two
	# state-feedback terms (pred0*s[6] + pred1*s[7], where s = vstate1 is the
	# previous sub-block of this channel). The accumulator is exact, so term order
	# is free. In parallel the SU advances the per-frame bookkeeping:
	#   vresult holds the previous frame's result; store it to its channel slot
	#   (where the next same-channel frame reads it) and to the output buffer,
	#   select this frame's slot, load this frame's input state into vstate1, then
	#   read the next frame's control byte and residual nibbles.
	vmudh v____,  pred1a, vres0.e0;    sqv vresult, 0,slot_ptr         # prev result -> this frame's channel slot
	vmadh v____,  pred1c, vres0.e1;    xor slot_ptr, slot_xor          # select this frame's channel slot
	vmadh v____,  pred1e, vres0.e2;    sqv vresult, -16,dmem_output    # prev result -> output (samples 8..15)
	vmadh v____,  pred1g, vres0.e3;    lqv vstate1, 0,slot_ptr         # input state = prev same-channel frame's samples 8..15
	vmadh v____,  pred1b, vres1.e0;    srv vresult, 0x00,dmem_output
	vmadh v____,  pred1d, vres1.e1;    addiu dmem_input, 9             # advance to next frame (9 bytes)
	vmadh v____,  pred1f, vres1.e2;    lbu t0, -1(dmem_input)          # next frame's control byte
	vmadh v____,  pred1h, vres1.e3;    lpv next_vres0, 0,dmem_input    # next frame's residual nibbles
	vmadh v____,  pred0,  vstate1.e6;  lpv next_vres1, 0,dmem_input
	vmadh v____,  pred1,  vstate1.e7;  addiu nframes, -1

	# ---- combine 0 -> vstate0 (samples 0..7) ----
	# Round the Q11 accumulator to integer samples: read the mid/high accumulator
	# words (acc >> 16) and multiply by 2^5 (vshift.e2 = K32), giving acc >> 11;
	# the vmadh saturates to signed 16-bit, i.e. clamp16. vstate0 = samples 0..7.
	# The SU decodes the next frame's header (scaling shift and predictor index ->
	# vscale and predictor address, toggling the codebook for stereo via cb_xor),
	# and unpacks its residuals: even nibbles into next_vres0 (sign-extend by >>12
	# after lpv put each nibble pair in the high byte), odd nibbles into next_vres1
	# (shift left 4 first, then sign-extend).
	vsar  vstatef, COP2_ACC_MD;        srl scaling, t0, 4              # scaling shift = control >> 4
	vsar  vstate0, COP2_ACC_HI;        andi predictor, t0, 0xF         # predictor index = control & 15
	vsra8 next_vres0, next_vres0, 12;  xor dmem_codebook, cb_xor       # select this channel's codebook
	vsll  next_vres1, next_vres1, 4;   sll predictor, VADPCM_ORDER_SHIFT + 4
	vmudn v____, vstatef, vshift.e2;   add predictor, dmem_codebook    # &codebook[2*index]
	vmadh vstate0, vstate0, vshift.e2; sllv scaling, t1, scaling       # vscale source = 1 << shift

	# ---- block 1: samples 8..15 = matrix * residuals + state feedback ----
	# Same product as block 0 but for the second sub-block: residuals from lanes
	# e4..e7 of vres0/vres1, and the state feedback now comes from vstate0.e6/e7
	# (samples 6,7 just produced by this frame's first sub-block). The SU builds
	# the next frame's matrix: mtc2 loads its vscale, and the lqv's fill the
	# Toeplitz sub-diagonals (v at offsets e1..e7) from its predictor address.
	vmudh v____,  pred1a, vres0.e4;    mtc2 scaling, vscale.e0         # next frame's vscale = 1 << shift
	vmadh v____,  pred1b, vres1.e4;    lqv pred1a.e1, 0x10,predictor
	vmadh v____,  pred1c, vres0.e5;    lqv pred1b.e2, 0x10,predictor
	vmadh v____,  pred1d, vres1.e5;    lqv pred1c.e3, 0x10,predictor
	vmadh v____,  pred1e, vres0.e6;    lqv pred1d.e4, 0x10,predictor
	vmadh v____,  pred1f, vres1.e6;    lqv pred1e.e5, 0x10,predictor
	vmadh v____,  pred1g, vres0.e7;    lqv pred1f.e6, 0x10,predictor
	vmadh v____,  pred1h, vres1.e7;    lqv pred1g.e7, 0x10,predictor
	vmadh v____,  pred0,  vstate0.e6
	vmadh v____,  pred1,  vstate0.e7

	# ---- combine 1 -> vresult (samples 8..15) ----
	# Round the accumulator to samples 8..15 exactly as combine 0 (acc >> 11 with
	# clamp16) into vresult. The SU writes this frame's first sub-block (vstate0,
	# samples 0..7) to the output buffer, loads the next frame's predictor vectors
	# pred0/pred1, scales the next frame's residuals into vres0/vres1, and advances
	# the output pointer by 32 bytes (16 samples).
	vsar  vstatef, COP2_ACC_MD;         sqv vstate0, 0x00,dmem_output
	vsar  vresult, COP2_ACC_HI;         srv vstate0, 0x10,dmem_output
	vsra8 next_vres1, next_vres1, 12;   lqv pred0.e0, 0x00,predictor
	vmudh vres0, next_vres0, vscale.e0; lqv pred1.e0, 0x10,predictor   # next frame's even residuals scaled
	vmudn v____, vstatef, vshift.e2;    addiu dmem_output, 32
	vmadh vresult, vresult, vshift.e2;  bgtz nframes, VADPCM_DecodeLoop
	 vmudh vres1, next_vres1, vscale.e0                                # next frame's odd residuals scaled

	# ---- End of decode loop ----

	# The loop writes each frame's result one iteration after producing it, so the
	# last frame's result is still only in vresult: write it to the output buffer
	# (samples 8..15) and to its channel slot. slot_ptr still selects that slot.
	sqv vresult, -16,dmem_output
	srv vresult, 0x00,dmem_output
	sqv vresult, 0,slot_ptr

	# The saved state for the next call is each channel's last sub-block. For mono
	# that is slot[0] (vstate1 currently holds unrelated data, so reload it).
	# vstate3 is unused for mono. Stereo first deinterleaves the per-frame L/R
	# blocks into sample order, then reloads both channels' final states.
	li slot_ptr, %lo(RSPQ_SCRATCH_MEM)
	bgez a2, VADPCM_SaveState
	 lqv vstate1, 0,slot_ptr           # mono: left final state = slot[0]

	# ---- Stereo deinterleave: per-frame L/R blocks -> sample-interleaved ----
	# The loop emitted each L/R pair as a 16-sample left block (L0..L15) followed
	# by a 16-sample right block (R0..R15), 64 bytes per pair. Rewrite each pair in
	# place to sample-interleaved order L0 R0 L1 R1 ... L15 R15.
	#
	# The duplicate swizzles .q0 = [0,0,2,2,4,4,6,6] and .q1 = [1,1,3,3,5,5,7,7]
	# gather the even/odd lanes of a vector. Merging L.q0 (even lanes) with R.q0
	# (odd lanes, via VCC = 0x55) yields the even-sample L/R pairs interleaved
	# [L0 R0 L2 R2 L4 R4 L6 R6]; .q1 yields the odd-sample pairs [L1 R1 L3 R3 ...].
	# Each lane-pair (e0,e2,e4,e6) is one finished [Lk Rk] output word; slv scatters
	# them to their interleaved byte offsets (even samples at 0,8,..; odd at 4,12,..).
	#
	# Software-pipelined: each iteration stores the previous pair (bank A, v_il*)
	# while loading and computing the current pair (bank B, v_ib*); bank B is then
	# copied into bank A for the next store. The stores read bank A and the loads
	# write the next 64-byte block, so an iteration's stores and the next pair's
	# compute are independent and the abundant VU work folds under the slv stores.
	li t0, 0x55
	ctc2 t0, COP2_CTRL_VCC              # vmrg mask: even lanes <- L, odd lanes <- R
	move dmem_output, db_base
	# Prologue: compute the first pair into bank A (v_il*).
	lqv vstate0, 0x00,dmem_output
	lrv vstate0, 0x10,dmem_output
	lqv vstate1, 0x10,dmem_output
	lrv vstate1, 0x20,dmem_output
	lqv vstate2, 0x20,dmem_output;     vcopy v_il0, vstate0.q0
	lrv vstate2, 0x30,dmem_output;     vcopy v_il1, vstate0.q1
	lqv vstate3, 0x30,dmem_output;     vcopy v_il2, vstate1.q0
	lrv vstate3, 0x40,dmem_output;     vcopy v_il3, vstate1.q1
	vmrg v_il0, v_il0, vstate2.q0                            # L0 R0 L2 R2 ..
	vmrg v_il1, v_il1, vstate2.q1                            # L1 R1 L3 R3 ..
	vmrg v_il2, v_il2, vstate3.q0                            # L8 R8 L10 R10 ..
	vmrg v_il3, v_il3, vstate3.q1                            # L9 R9 L11 R11 ..
	addiu db_count, -1
	blez db_count, VADPCM_DeintLast    # only one pair: store it directly
	 nop
	# This loop is 30 cycles per 8 stereo-samples / 16 samples = 1.875 cycles/sample
	.balign 8
VADPCM_Deinterleave:
	# Store bank A (current pair) while loading + computing the next pair (bank B).
	slv v_il0.e0, 0x00,dmem_output
	lqv vstate0,  0x40,dmem_output
	slv v_il1.e0, 0x04,dmem_output
	lrv vstate0,  0x50,dmem_output
	slv v_il0.e2, 0x08,dmem_output
	lqv vstate1,  0x50,dmem_output
	slv v_il1.e2, 0x0C,dmem_output
	lrv vstate1,  0x60,dmem_output
	slv v_il0.e4, 0x10,dmem_output
	lqv vstate2,  0x60,dmem_output
	slv v_il1.e4, 0x14,dmem_output
	lrv vstate2,  0x70,dmem_output
	slv v_il0.e6, 0x18,dmem_output
	lqv vstate3,  0x70,dmem_output
	slv v_il1.e6, 0x1C,dmem_output
	lrv vstate3,  0x80,dmem_output
	slv v_il2.e0, 0x20,dmem_output;    vcopy v_ib0, vstate0.q0
	slv v_il3.e0, 0x24,dmem_output;    vcopy v_ib1, vstate0.q1
	slv v_il2.e2, 0x28,dmem_output;    vcopy v_ib2, vstate1.q0
	slv v_il3.e2, 0x2C,dmem_output;    vcopy v_ib3, vstate1.q1
	slv v_il2.e4, 0x30,dmem_output;    vmrg v_ib0, v_ib0, vstate2.q0
	slv v_il3.e4, 0x34,dmem_output;    vmrg v_ib1, v_ib1, vstate2.q1
	slv v_il2.e6, 0x38,dmem_output;    vmrg v_ib2, v_ib2, vstate3.q0
	slv v_il3.e6, 0x3C,dmem_output;    vmrg v_ib3, v_ib3, vstate3.q1
	vcopy v_il0, v_ib0;                addiu dmem_output, 64
	vcopy v_il1, v_ib1;                addiu db_count, -1
	vcopy v_il2, v_ib2;                bgtz db_count, VADPCM_Deinterleave
	 vcopy v_il3, v_ib3
VADPCM_DeintLast:
	# Epilogue: store the last computed pair (bank A).
	slv v_il0.e0, 0x00,dmem_output
	slv v_il1.e0, 0x04,dmem_output
	slv v_il0.e2, 0x08,dmem_output
	slv v_il1.e2, 0x0C,dmem_output
	slv v_il0.e4, 0x10,dmem_output
	slv v_il1.e4, 0x14,dmem_output
	slv v_il0.e6, 0x18,dmem_output
	slv v_il1.e6, 0x1C,dmem_output
	slv v_il2.e0, 0x20,dmem_output
	slv v_il3.e0, 0x24,dmem_output
	slv v_il2.e2, 0x28,dmem_output
	slv v_il3.e2, 0x2C,dmem_output
	slv v_il2.e4, 0x30,dmem_output
	slv v_il3.e4, 0x34,dmem_output
	slv v_il2.e6, 0x38,dmem_output
	slv v_il3.e6, 0x3C,dmem_output

	# Reload the final per-channel states (the deinterleave reused vstate1/3).
	# Left frames write slot[1], right frames write slot[0], so each slot now holds
	# its channel's last decoded sub-block.
	li slot_ptr, %lo(RSPQ_SCRATCH_MEM)
	lqv vstate1, 16,slot_ptr           # slot[1] = left  final state
	lqv vstate3, 0,slot_ptr            # slot[0] = right final state

VADPCM_SaveState:
	##################################################################

	# Save back state
	sqv vstate1, 0x00,dmem_state

	# In-mixer path: keep state in DMEM and return to VadpcmDecodeFrame.
	# MIX_VADPCM_STATE is only 16 bytes (mono) — do not store vstate3.
	lw t0, %lo(CH_VADPCM_MIXRET)
	beqz t0, 1f
	sw zero, %lo(CH_VADPCM_MIXRET)
	lw t0, %lo(CH_VNEXT_FRAME)
	addi t0, 1
	sw t0, %lo(CH_VNEXT_FRAME)
	lw ra, %lo(CH_VADPCM_RA)
	jr ra
	nop
1:
	sqv vstate3, 0x10,dmem_state
	move s4, dmem_state
	move s0, a2
	jal DMAOutAsync
	li t0, DMA_SIZE(32, 1)

	# Check if the output buffer is misaligned
	and t0, rdram_output, 7
    beqz t0, VADPM_OutputDma
    nop

    # Handle unaligned output buffer: we need to fetch and merge
    # existing samples on misaligned addresses. Use dmem_state
	# as scratch buffer
    move s0, rdram_output
    jal DMAIn
    move s4, dmem_state

    li s0, %lo(VADPCM_BUFFER)
	move s4, dmem_state
VADPCM_OutputMergeLoop:
    lhu t1, 0(s4)
    addiu s4, 2
    addiu t0, -2
    sh t1, 0(s0)
    bnez t0, VADPCM_OutputMergeLoop
    addiu s0, 2

VADPM_OutputDma:
	# Save back output buffer
	li s4, %lo(VADPCM_BUFFER)
	move s0, rdram_output
	addiu t0, buffer_out_size, -1
	jal_and_j DMAOutAsync, RSPQ_Loop

	.endfunc

	#undef v_out_l
	#undef v_out_r
	#undef v_sample_0
	#undef v_sample_1
	#undef v_sample_2
	#undef v_sample_3
	#undef v_mix_l
	#undef v_mix_r
	#undef vscale
	#undef vstatef
	#undef pred0
	#undef pred1
	#undef pred1a
	#undef pred1b
	#undef pred1c
	#undef pred1d
	#undef pred1e
	#undef pred1f
	#undef pred1g
	#undef pred1h
	#undef vres0
	#undef vres1
	#undef vstate0
	#undef vstate1
	#undef vstate2
	#undef vstate3
	#undef v____
	#undef scaling
	#undef predictor
	#undef nframes
	#undef dmem_state
	#undef dmem_codebook
	#undef dmem_output
	#undef dmem_input
	#undef buffer_in_size
	#undef buffer_out_size
	#undef stereo_toggle
	#undef rdram_input
	#undef rdram_output


	########################################
	# VADPCM_SetState
	#
	# Args:
	#   a0: pointer to RDRAM state (destination)
	#   a1: pointer to RDRAM buffer with source state
    #       or 0 if state must be cleared
	########################################

	.func VADPCM_SetState
VADPCM_SetState:
	li s4, %lo(VADPCM_STATE)
	sqv vzero, 0x00,s4
	sqv vzero, 0x10,s4
	beqz a1, VADPCM_SetState_Write
	 li t0, DMA_SIZE(16*2, 1)
	jal DMAIn
	 move s0, a1

VADPCM_SetState_Write:
	move s0, a0
	jal_and_j DMAOut, RSPQ_Loop

	.endfunc
