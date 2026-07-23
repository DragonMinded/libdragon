	####################################################################
	#
	# Libdragon RSP ucode for audio mixer
	#
	# Authors: Giovanni Bajo <giovannibajo@gmail.com>
	####################################################################

	##############################################################
	#
	# This ucode implements a mixer, for 32 channels of digital samples,
	# with per-channel resampling (mixing channels of different frequencies),
	# per-channel 16-bit volume and 16-bit panning control,
	# and full 16-bit stereo output with no loss of precision.
	#
	# The C code that drives this ucode is in mixer.c (mixer_poll).
	# The input for the ucode is the channel parameters, that describe
	# the playback configuration and where to find the actual samples
	# in RDRAM.
	#
	# The ucode fetches the samples from RDRAM via DMA, resample them 
	# (that is, operating a frequency change using point sampling),
	# apply volume/panning, and mix them together. The final output
	# samples are sent to RDRAM via DMA.
	#
	# Given the limited space in DMEM, this happens in loops where a small
	# amount of samples per each channel is processed (MAX_SAMPLES_PER_LOOP).
	#
	# RESAMPLING
	# **********
	#
	# Resampling is implemented by the function UpdateAndFetch. As the name
	# describes, this functions fetches samples from RDRAM via DMA into a
	# temporary buffer in DMEM (DMEM_SAMPLE_CACHE), and then resample them
	# into a buffer called CHANNEL_BUFFER, which holds the resampled samples
	# for all channels.
	#
	# Resampling means doing a point sampling on the input sample,
	# according to the resampling frequency. The channel configuration
	# already contains a fixed-point "step" value for each channel, that
	# is used as increment in the loop. For instance, if the step is 2, it
	# means that every other sample will be skipped, thus achieving a 0.5x
	# resampling (= playing a 88Khz sample on a 44Khz output).
	#
	# Resampling requires thus copying one sample at at time from DMEM_SAMPLE_CACHE
	# into CHANNEL_BUFFER, with a fixed-point increment. This is actually
	# quite slow and cannot be vectorized; it is by far the slowest part of
	# the whole ucode. To achieve reasonable performance, the inner resampling
	# loop has been painstakingly optimized by having specific version for 8-bit
	# and 16-bit input samples, and with manual loop unrolling to increase
	# performance. The final version takes 4,88 cycles/sample for 8-bit channels,
	# and 5,88 cycles/samples for 16-bit channels. TODO: handle also stereo
	# waveforms spanning 2 channels. This would allow the mixer to support
	# interleaved stereo waveforms.
	#
	# The DMEM_SAMPLE_CACHE area is a temporary 64-byte buffer that is used to
	# hold the original samples fetched via DMA (before resampling). Since the
	# ucode doesn't know how many samples will be needed (the exact number
	# depends on the resampling step and would require a division to be
	# calculated), the DMA always fills this area in full, so we define it
	# small-ish to avoid wasting too much time fetching useless samples. On
	# the other hand, in the rare cases in which there is a high resampling
	# step, multiple DMA transfers might be needed.
	#
	#
	# MIXER
	# *****
	#
	# The mixer is actually made of two cores: one that mixes up to 32 channels
	# and runs at 16 cycles/sample, and one that mixes up to 8 channels
	# and runs at 11 cycles/sample. The 8 channels core is only
	# 32% faster because the 32 channels core better exploit vector
	# instruction parallelism.
	#
	# The 8-channel mixer is automatically selected whenever no more than 8
	# channels are configured. TODO: this could be improved by actually looking
	# at channel status; a channel might be configured but be currently turned 
	# off or otherwise silent. 
	#
	# The mixer fetches the samples from CHANNEL_BUFFER, apply volume and
	# panning, mix them, and write the output stream in a buffer called
	# OUTPUT_AREA. The main loop will then DMA this buffer back to RDRAM.
	#
	# The mixer cores also implement a one-tap volume filter to
	# smooth out sudden volume changes to avoid clicks in the output.
	# This is common for instance with XM modules, and in fact most
	# XM players implement filters to achieve a similar effect.
	# To avoid taxing too much the core, the filter runs at 1/8th
	# of the output rate. With the filter turned on, the 32-channel
	# core runs at 18 cycles/samples, while the 8-channel core
	# runs at 11.5 cycles/samples. The filter can be turned off at compile
	# time (VOLUME_FILTER). TODO: make this a runtime option.
	#
	# Even with the filter overhead, the mixer is still very fast.
	# For 32 channel mixing at 44100Hz, it uses only the 1.65% of the
	# available frame time, while 8-channel mixing takes 1.10%. In
	# general, resampling takes much more time than mixing. Because of this,
	# the volume filter is on by default.
	#
	####################################################################
	#
	# Glossary:
	#    * Sample: a single 8-bit / 16-bit amplitude value
	#    * Waveform: a sequence of samples that can be played (eg: a WAV file)
	#
	####################################################################

#include <rsp_queue.inc>
#include <emux.inc>

.set noreorder
.set at

# Maximum number of channels supported by this ucode. You can't really increase
# this without modifying the code.
#define MAX_CHANNELS           32

# Activate one-tap filter on volume changes, to smooth out sudden changes
# that can cause clicks.
#define VOLUME_FILTER           1

# How many sample to process in a mixing loop. This can be tuned to use more/less
# DMEM. Obviously, more samples per loop is better for performance.
#define MAX_SAMPLES_PER_LOOP   32

# Number of fractional bits used in the fixed point numbers that specify
# the waveform position, step, length and loop length.
# NOTE: This must be the same of MIXER_FX32_FRAC in mixer.c.
#define WAVEFORM_POS_FRAC_BITS  12

# Waveform flags. Keep these in sync with mixer.c
#define CH_FLAGS_16BIT      (1<<2)
#define CH_FLAGS_STEREO     (1<<3)

#define MAX_CHANNELS_VOFF  (MAX_CHANNELS*2)


	################################
	# Global register allocations, valid in the whole ucode
	################################

	#define v_zero        $v00

	# Current volume left/right for each channel. This is the volume
	# that's being ramped up to the final volume.
	#define v_xvol_l_0    $v13
	#define v_xvol_r_0    $v14
	#define v_xvol_l_1    $v15
	#define v_xvol_r_1    $v16
	#define v_xvol_l_2    $v17
	#define v_xvol_r_2    $v18
	#define v_xvol_l_3    $v19
	#define v_xvol_r_3    $v20

	# Final volume left/right (basic volume * panning * global volume),
	# pre-multiplied by 1-alpha.
	#define v_chvol_l_0   $v21
	#define v_chvol_r_0   $v22
	#define v_chvol_l_1   $v23
	#define v_chvol_r_1   $v24
	#define v_chvol_l_2   $v25
	#define v_chvol_r_2   $v26
	#define v_chvol_l_3   $v27
	#define v_chvol_r_3   $v28

	# Shift registers
	#define v_shift8      $v29
	#define v_shift       $v30

	# Misc constants
	#define v_const1      $v31

	#define k_0000        v_zero
	#define k_8000        v_shift8.e0


	.data

	RSPQ_BeginOverlayHeader
		RSPQ_DefineCommand command_exec, 20				# 0x0
		RSPQ_DefineCommand VADPCM_Decompress, 16		# 0x1
		RSPQ_DefineCommand VADPCM_SetState, 8			# 0x2
		RSPQ_DefineCommand command_memmove, 12			# 0x3
	RSPQ_EndOverlayHeader

############################################################################

	# Misc constants
	.align 4
VCONST_1:
	.half 0x7FFF
	.half 0xe076      #   (0.9837**8) fixed 0.16
	.half 0x1f8a      # 1-(0.9837**8) fixed 0.16

	#define k_ffff      v_const1.e0
	#define k_alpha     v_const1.e1
	#define k_1malpha   v_const1.e2

	.align 4
BANNER0:    .ascii "Dragon RSP Audio"
BANNER1:    .ascii " Coded by Rasky "

	RSPQ_BeginSavedState
	# Current volume state for each channel. This might differ from CHANNEL_VOLUMES
	# when the volume filter is turned on: this is the actual current value that
	# is being interpolated to CHANNEL_VOLUMES, which is the requested target
	# volume to reach.
	.align 4
XVOL_L:                   .dcb.w MAX_CHANNELS
XVOL_R:                   .dcb.w MAX_CHANNELS
	# RDRAM address of the round completion counter (written by CPU at init).
	# Must stay in saved state so it survives overlay swaps.
	.align 4
COUNTER_RDRAM:            .long  0
	                      .long  0
	                      .long  0
	                      .long  0
	RSPQ_EndSavedState

	.align 4
IDENTITY:  .half  1<<11,1<<11,1<<11,1<<11, 1<<11,1<<11,1<<11,1<<11

	.bss

############################################################################
# UCODE INPUT DATA
############################################################################

# Output RDRAM buffer where to store mixed samples (16-bit, stereo)
OUTPUT_RDRAM:             .long  0
# Global volume of playback
GLOBAL_VOLUME:            .half  0
# Number of samples to resample/mix on each channel
NUM_SAMPLES:              .half  0
# Number of configured channels
NUM_CHANNELS:             .half  0

# Requested volumes for each channel. If VOLUME_FILTER is on, these are the
# values requested by the user, but the current value for each channel might
# be different (as the filter is running).
	.align 4
SETTINGS_START:
CHANNEL_VOLUMES_L:        .dcb.w MAX_CHANNELS
CHANNEL_VOLUMES_R:        .dcb.w MAX_CHANNELS

# Array of structures rsp_mixer_channel_s. See mixer.c. 6 words for each
# channel with the following content:
#
#   0: pos:      absolute waveform position (as fixed point, WAVEFORM_POS_FRAC_BITS)
#   1: step:     resampling increment of position for each output sample (fixed point)
#   2: len:      length of the waveform (fixed point)
#   3: loop_len: length of the loop from the end of the waveform (or 0 if no loop)
#   4: ptr:      pointer to the beginning of the waveform
#   5: flags:    channel flags (see CH_FLAGS_ macros in mixer.c)
#
	.align 4
WAVEFORM_SETTINGS:        .dcb.l (6*MAX_CHANNELS)
SETTINGS_END:

	# Temporary cache of samples fetched by DMA. Notice that this must be
	# less or equal than MIXER_LOOP_OVERREAD (mixer.c), because the
	# RSP will over-read up to this amount of bytes after waveform's end.
	.align 3
	#define SAMPLE_CACHE_SIZE  64
DMEM_SAMPLE_CACHE:		  .dcb.b SAMPLE_CACHE_SIZE


	# CHANNEL_BUFFER holds the resampled samples for all the channels.
	# Samples of different channels are interleaved, so that they can
	# be mixed with vector instructions.
	.align 4  # for human visual debugging 
CHANNEL_BUFFER:  .dcb.w (MAX_SAMPLES_PER_LOOP * MAX_CHANNELS)

	# OUTPUT_AREA holds the final mixed stereo samples, that will be copied
	# to RDRAM via DMA.
	.align 4  # for human visual debugging, 3 would be sufficient (for DMA)
OUTPUT_AREA:     .dcb.w MAX_SAMPLES_PER_LOOP*2

	# Scratch used to DMA the round id out to COUNTER_RDRAM. 8-byte aligned
	# for RSP DMA; only the first word is meaningful.
	.align 3
ROUND_SCRATCH:            .long  0
	                      .long  0

	.text 1

	# Number of samples that will be processed in the current loop.
	#define num_samples     k1


command_exec:
	#define samples_left    t4
	#define outptr          s8

	vxor v_zero, v_zero, v_zero
	li t0, %lo(VCONST_1)
	lqv v_const1, 0,t0

	# Extract command parameters
	andi a0, 0xFFFF
	sh a0, %lo(GLOBAL_VOLUME)

	srl t1, a1, 16
	sh t1, %lo(NUM_SAMPLES)

	andi a1, 0xFFFF
	sh a1, %lo(NUM_CHANNELS)

	lw a2, CMD_ADDR(0x8, 0x14)
	sw a2, %lo(OUTPUT_RDRAM)

	# Load settings (input-only; positions are CPU-authoritative, no writeback)
	jal DMASettings
	li t2, DMA_IN

	jal SetupMixer
	nop

MainLoop:
	lh samples_left, %lo(NUM_SAMPLES)
	beqz samples_left, End

	# Fetch output RDRAM pointer and set t1=1 if it's not 8-byte aligned.
	# We expect the pointer to be 32-bit aligned (since it's a buffer of 16-bit
	# stereo samples), but it might not be 64-bit aligned.
	lw t1, %lo(OUTPUT_RDRAM)
	andi t1, 7
	slt t1, zero, t1

	# The maximum number of samples that we want to generate
	# in a single loop is MAX_SAMPLES_PER_LOOP. Subtract 
	# 1 from this number if RDRAM buffer is not aligned so
	# that after this loop we're 8-byte aligned again.
	li num_samples, MAX_SAMPLES_PER_LOOP
	sub num_samples, t1

	# num_samples = MIN(num_samples, MAX_SAMPLES_PER_LOOP[-1])
	bgt samples_left, num_samples, CheckDMAAlignment
	nop
	move num_samples, samples_left
CheckDMAAlignment:
	# If the output buffer is not aligned, fetch one DMA line (8 bytes)
	# so that we preserve the 4 bytes that come before the buffer we were
	# given (which would be overwritten by the RSP DMA).
	beqz t1, DoLoop
	li outptr, %lo(OUTPUT_AREA)

	lw s0, %lo(OUTPUT_RDRAM)
	li s4, %lo(OUTPUT_AREA)
	jal DMAIn
	li t0, DMA_SIZE(8,1)
	addi outptr, 4

DoLoop:
	# Update number of samples left, subtracting the number of samples
	# that will be calculated in this loop.
	sub samples_left, num_samples
	sh samples_left, %lo(NUM_SAMPLES)

	# Fetch the samples and do resampling
	jal UpdateAndFetch
	lhu k0, %lo(NUM_CHANNELS)

	# Mix the samples
	jal Mixer
	move s4, outptr

	# Update the output pointer in RDRAM for next loop.
	sll t0, num_samples, 2
	lw s0, %lo(OUTPUT_RDRAM)
	add s1, s0, t0
	sw s1, %lo(OUTPUT_RDRAM)

	# DMA the output buffer into RDRAM (s0 is the output pointer before update).
	# We can do the transfer in background because it will surely be finished
	# by the time we start filling the output area again.
	addi t0, -1
	jal DMAOutAsync
	li s4, %lo(OUTPUT_AREA)

	j MainLoop
	nop

End:
	# Store the current channel volume in DMEM (permanent state)
	jal EndMixer
	nop

	# Publish the round id to RDRAM. Settings are no longer written back:
	# the CPU advances channel positions itself. Completing this DMA is the
	# signal that every command enqueued before this mix (VADPCM/Opus
	# decodes included) has finished, because the highpri queue is FIFO.
	lw t0, CMD_ADDR(0x10, 0x14)
	sw t0, %lo(ROUND_SCRATCH)
	li s4, %lo(ROUND_SCRATCH)
	lw s0, %lo(COUNTER_RDRAM)
	li t0, DMA_SIZE(8, 1)
	jal_and_j DMAOutAsync, RSPQ_Loop

	#undef samples_left
	#undef outptr


###############################################################
# command_memmove
#
# Backward move of bytes in RDRAM (dst < src), enqueued by
# samplebuffer_discard to compact a sample buffer. Being a regular
# queue command, it executes in-order with the decode/mix commands
# that reference the buffer, so the CPU does not need to wait for
# them before compacting.
#
# dst/src must be 8-byte aligned and len must be a multiple of 8
# (the CPU falls back to a synchronous copy otherwise): unlike
# OPUS_memmove, we cannot copy a few extra bytes, because the bytes
# just after the moved region belong to the append area that the
# CPU may be writing concurrently.
#
# Chunks are staged through CHANNEL_BUFFER, which is scratch space
# only used within a single command_exec.
#
# Input:
#   a0: 0..23: RDRAM destination pointer
#   a1: 0..23: RDRAM source pointer
#   a2:        number of bytes to move
###############################################################

	#define MEMMOVE_CHUNK  (MAX_SAMPLES_PER_LOOP * MAX_CHANNELS * 2)

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
	li s4, %lo(CHANNEL_BUFFER)
	jal DMAIn
	addiu t0, t4, -1

	move s0, a0
	li s4, %lo(CHANNEL_BUFFER)
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


###############################################################
# DMASettings - Load the settings via DMA.
#
# Arguments:
#   t2:  DMA_* flag for DMAExec (always DMA_IN for command_exec)
###############################################################

	.func DMASettings
DMASettings:
	lw s0, CMD_ADDR(0xC, 0x14)
	li s4, %lo(SETTINGS_START)
	j DMAExec
	li t0, DMA_SIZE((SETTINGS_END - SETTINGS_START), 1)
	.endfunc



###############################################################
# UpdateAndFetch - Resampling loop.
#
# This function goes through all active channels, fetch
# the samples of current waveforms via DMA, and apply the
# required resampling (frequency change) for a specified
# number of ticks (output samples).
#
# All channels parameters are fetched from WAVEFORM_SETTINGS.
#
# Arguments:
#
#  k0:  number of active channels
#
# Global state:
#
#  num_samples:  number of ticks to resample
#
###############################################################

	#define ticks          t3
	#define waveform_ptr   s1
	#define wv_pos         t4
	#define wv_step        t5
	#define wv_len         t6
	#define wv_loop_len    t7
	#define wv_addr        t8
	#define wv_dma_addr    v0
	#define nchan          v1
	#define dma_cache_end  s7
	#define out_ptr        s5
	#define wv_pos_to_dmem s6
	#define wv_step_8x     t2
	#define is_stereo      a0
	#define is_16bit       a1

	.func UpdateAndFetch
UpdateAndFetch:
	move ra2, ra

	li out_ptr, %lo(CHANNEL_BUFFER)
	li t0, (MAX_SAMPLES_PER_LOOP * MAX_CHANNELS * 2) / 64 - 1
ClearLoop:
	sqv v_zero, 0x00,out_ptr
	sqv v_zero, 0x10,out_ptr
	sqv v_zero, 0x20,out_ptr
	sqv v_zero, 0x30,out_ptr
	addi out_ptr, 64
	bnez t0, ClearLoop
	addi t0, -1

	li waveform_ptr, %lo(WAVEFORM_SETTINGS)
	li nchan, 0

	li dma_cache_end, %lo(DMEM_SAMPLE_CACHE+SAMPLE_CACHE_SIZE)
	sll dma_cache_end, WAVEFORM_POS_FRAC_BITS

	# main update loop: will be done once per channel.
UpdateLoop:
	# Fetch waveform parameters. Notice that if the RDRAM
	# address is zero, no waveform was configured in this channel,
	# so we can skip directly to next channel.
	lw wv_addr,     16(waveform_ptr)
	beqz wv_addr, WaveLoopEpilog2
	lw wv_pos,       0(waveform_ptr)
	lw wv_step,      4(waveform_ptr)
	lw wv_len,       8(waveform_ptr)
	lw wv_loop_len, 12(waveform_ptr)

	# Prepare output pointer
	addi out_ptr, nchan, %lo(CHANNEL_BUFFER)
	add out_ptr, nchan
	move ticks, num_samples

	# Flags: isolate 16-bit flag (bit 2) and stereo flags (bit 3)
	lw t0, 20(waveform_ptr)
	andi is_stereo, t0, CH_FLAGS_STEREO
	andi is_16bit, t0, CH_FLAGS_16BIT
WaveStart:
	# Check if we reached end of sample.
	bltu wv_pos, wv_len, WaveDmaFetch
	nop

	# End of sample. Check if the waveform loops
	beqz wv_loop_len, WaveLoopEpilog
	nop

	# Apply loop to current position and loop again (until wv_pos < wv_len).
	# We need to iterate in case during a single UpdateLoop we moved wv_pos
	# more than wv_loop_len bytes, so subtracting just once is not sufficient.
	j WaveStart
	sub wv_pos, wv_loop_len

WaveDmaFetch:
	# Fetch SAMPLE_CACHE_SIZE bytes of the current waveform
	# into DMEM_SAMPLE_CACHE, from the current position.
	# Notice that DMA is 8-byte aligned, and DMAIn will adjust
	# s4 to point to the actual byte in DMEM containing the
	# first requested sample.
	srl s2, wv_pos, WAVEFORM_POS_FRAC_BITS
	add s0, s2, wv_addr
	li s4, %lo(DMEM_SAMPLE_CACHE)
	jal DMAIn
	li t0, DMA_SIZE(SAMPLE_CACHE_SIZE, 1)

	# Calculate an offset that converts wv_pos from a (fixed point) RDRAM
	# pointer into a (fixed point) DMEM pointer. This will be used because
	# the inner loop will use wv_pos adjusted to be a DMEM pointer
	# rather than a RDRAM pointer, for performance.
	sub wv_pos_to_dmem, s2, s4
	sll wv_pos_to_dmem, WAVEFORM_POS_FRAC_BITS

	# Compute wv_step_8x
	sll wv_step_8x, wv_step, 3

	############################################################
	#       His Royal Majesty The Resampling Loop.
	############################################################
	# This is where 80% of the RSP time is spent. Any clock
	# cycle counts!
	#
	# We need to go through the samples fetched in DMEM and
	# resample them into the output buffer (aka: copying them
	# going through with specified step).
	#
	# For performance, this loop assumes that all samples in
	# DMEM are valid (that is, they were not fetched more than 
	# waveform length). To make sure this is always true,
	# the C code has prepared all samples with an overread
	# buffer at the end (see XM_WAVEFORM_OVERREAD).
	#
	# The loop is available in four different versions:
	# 8-bit and 16-bits, and 1x and 8x (unrolled). The unrolled
	# version automatically fallbacks to the 1x version when
	# required.
	############################################################

	# Adjust wv_pos to become a DMEM pointer, and then 
	# jump to the 8-bit or 16-bit resampling loop.
	bnez is_stereo, WaveLoopStereo
	sub wv_pos, wv_pos_to_dmem

	############################################################
	#       Mono
	############################################################

	bnez is_16bit, WaveLoop16_8x

	############################################################
	#       Mono - 8 bit
	############################################################

WaveLoop8_8x:
	add t0, wv_pos, wv_step_8x                 # check if pos+step*8 is still in the DMEM cache
	bgt t0, dma_cache_end, WaveLoop8Checks
	li t0, 8                                   # check if we need to process at least 8 samples
	blt ticks, t0, WaveLoop8Checks

	# Process 8 samples
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS    # Get raw DMEM pointer (integer part of wv_pos)
	lbu t0, 0(t0)                              # Fetch the sample
	add wv_pos, wv_step                        # Update wv_pos (load cycle delay)
	sb t0, (0*MAX_CHANNELS*2+0)(out_ptr)       # Store the sample

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (1*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (2*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (3*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (4*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (5*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (6*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (7*MAX_CHANNELS*2+0)(out_ptr)

	# Update output pointer and ticks counter, and loop.
	add out_ptr, 8*MAX_CHANNELS*2
	j WaveLoop8_8x
	addi ticks, -8

WaveLoop8:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS        # Get the raw DMEM pointer
	lbu t0, 0(t0)                                  # Fetch the 8-bit sample
	add wv_pos, wv_step                            # Update the pointer using the resampling step 
	sb t0, 0(out_ptr)                              # Store to output as 16-bit
	addi out_ptr, MAX_CHANNELS*2                   # Update output pointer
	addi ticks, -1
WaveLoop8Checks:
	blez ticks, WaveBeforeEpilog                   # Check if we're finished
	slt t0, wv_pos, dma_cache_end                  # Loop if we've not reached the end of DMEM buffer
	bnez t0, WaveLoop8
	nop
	j WaveStart                                    # End of buffer: fetch some more samples
	add wv_pos, wv_pos_to_dmem

	############################################################
	#       Mono - 16 bit
	############################################################

WaveLoop16_8x:
	add t0, wv_pos, wv_step_8x
	bgt t0, dma_cache_end, WaveLoop16Checks
	li t0, 8
	blt ticks, t0, WaveLoop16Checks

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (0*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (1*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (2*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (3*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (4*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (5*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (6*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (7*MAX_CHANNELS*2)(out_ptr)

	add out_ptr, 8*MAX_CHANNELS*2
	j WaveLoop16_8x
	addi ticks, -8

WaveLoop16:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, 0(out_ptr)
	addi out_ptr, MAX_CHANNELS*2
	addi ticks, -1
WaveLoop16Checks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop16
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem


	############################################################
	#       Stereo
	############################################################

WaveLoopStereo:
	bnez is_16bit, WaveLoop16_Stereo_8x

	############################################################
	#       Stereo - 8 bit
	############################################################

WaveLoop8_Stereo_8x:
	add t0, wv_pos, wv_step_8x
	bgt t0, dma_cache_end, WaveLoop8StereoChecks
	li t0, 8
	blt ticks, t0, WaveLoop8StereoChecks

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (0*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (0*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (1*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (1*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (2*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (2*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (3*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (3*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (4*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (4*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (5*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (5*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (6*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (6*MAX_CHANNELS*2+2)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, (7*MAX_CHANNELS*2+0)(out_ptr)
	sb t0, (7*MAX_CHANNELS*2+2)(out_ptr)

	add out_ptr, 8*MAX_CHANNELS*2
	j WaveLoop8_Stereo_8x
	addi ticks, -8

WaveLoop8Stereo:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+1
	sll t0, 1
	lbu t1, 0(t0)
	lbu t0, 1(t0)
	add wv_pos, wv_step
	sb t1, 0(out_ptr)
	sb t0, 2(out_ptr)

	addi out_ptr, MAX_CHANNELS*2
	addi ticks, -1
WaveLoop8StereoChecks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop8Stereo
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem


	############################################################
	#       Stereo - 16 bit
	############################################################

WaveLoop16_Stereo_8x:
	add t0, wv_pos, wv_step_8x
	bgt t0, dma_cache_end, WaveLoop16StereoChecks
	li t0, 8
	blt ticks, t0, WaveLoop16StereoChecks

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (0*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (1*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (2*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (3*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (4*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (5*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (6*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, (7*MAX_CHANNELS*2+0)(out_ptr)

	add out_ptr, 8*MAX_CHANNELS*2
	j WaveLoop16_Stereo_8x
	addi ticks, -8

WaveLoop16Stereo:
	srl t0, wv_pos, WAVEFORM_POS_FRAC_BITS+2
	sll t0, 2
	lw t0, 0(t0)
	add wv_pos, wv_step
	sw t0, 0(out_ptr)

	addi out_ptr, MAX_CHANNELS*2
	addi ticks, -1
WaveLoop16StereoChecks:
	blez ticks, WaveBeforeEpilog
	slt t0, wv_pos, dma_cache_end
	bnez t0, WaveLoop16Stereo
	nop
	j WaveStart
	add wv_pos, wv_pos_to_dmem

WaveBeforeEpilog:
	add wv_pos, wv_pos_to_dmem

WaveLoopEpilog:
	beqz is_stereo, WaveLoopEpilog2
	sw wv_pos, 0(waveform_ptr)   # store updated wv_pos in DMEM

	# For stereo, we need to skip 1 channel
	addi nchan, 1
	addi waveform_ptr, 6*4

WaveLoopEpilog2:
	addi nchan, 1
	bne nchan, k0, UpdateLoop
	addi waveform_ptr, 6*4

	jr ra2
	nop
	.endfunc



##############################################################
# SetupMixer: load left/right volumes for each channel
# into the global registers (v_chvol_l/r), so that they're 
# available for mixing.
##############################################################

	#define v_glvol       $v01

	.func SetupMixer
SetupMixer:
	# Load global volume (into all lanes)
	lh t0, %lo(GLOBAL_VOLUME)
	mtc2 t0, v_glvol.e0
	vor v_glvol, v_zero, v_glvol.e0

	li s0, %lo(CHANNEL_VOLUMES_L)
	li s1, %lo(XVOL_L)

	# Load channel volumes (left / right)
	lqv v_chvol_l_0,     0*MAX_CHANNELS_VOFF+0x00,s0
	lqv v_chvol_l_1,     0*MAX_CHANNELS_VOFF+0x10,s0
	lqv v_chvol_l_2,     0*MAX_CHANNELS_VOFF+0x20,s0
	lqv v_chvol_l_3,     0*MAX_CHANNELS_VOFF+0x30,s0

	lqv v_chvol_r_0,     1*MAX_CHANNELS_VOFF+0x00,s0
	lqv v_chvol_r_1,     1*MAX_CHANNELS_VOFF+0x10,s0
	lqv v_chvol_r_2,     1*MAX_CHANNELS_VOFF+0x20,s0
	lqv v_chvol_r_3,     1*MAX_CHANNELS_VOFF+0x30,s0

	# Apply global volume to obtain the final volume for each channel
	vmudl v_chvol_l_0, v_chvol_l_0, v_glvol
	vmudl v_chvol_r_0, v_chvol_r_0, v_glvol
	vmudl v_chvol_l_1, v_chvol_l_1, v_glvol
	vmudl v_chvol_r_1, v_chvol_r_1, v_glvol
	vmudl v_chvol_l_2, v_chvol_l_2, v_glvol
	vmudl v_chvol_r_2, v_chvol_r_2, v_glvol
	vmudl v_chvol_l_3, v_chvol_l_3, v_glvol
	vmudl v_chvol_r_3, v_chvol_r_3, v_glvol

#if VOLUME_FILTER
	# Load actual volumes levels
	lqv v_xvol_l_0,      0*MAX_CHANNELS_VOFF+0x00,s1
	lqv v_xvol_l_1,      0*MAX_CHANNELS_VOFF+0x10,s1
	lqv v_xvol_l_2,      0*MAX_CHANNELS_VOFF+0x20,s1
	lqv v_xvol_l_3,      0*MAX_CHANNELS_VOFF+0x30,s1

	lqv v_xvol_r_0,      1*MAX_CHANNELS_VOFF+0x00,s1
	lqv v_xvol_r_1,      1*MAX_CHANNELS_VOFF+0x10,s1
	lqv v_xvol_r_2,      1*MAX_CHANNELS_VOFF+0x20,s1
	lqv v_xvol_r_3,      1*MAX_CHANNELS_VOFF+0x30,s1
#else
	vor v_xvol_l_0, v_chvol_l_0, v_zero
	vor v_xvol_l_1, v_chvol_l_1, v_zero
	vor v_xvol_l_2, v_chvol_l_2, v_zero
	vor v_xvol_l_3, v_chvol_l_3, v_zero

	vor v_xvol_r_0, v_chvol_r_0, v_zero
	vor v_xvol_r_1, v_chvol_r_1, v_zero
	vor v_xvol_r_2, v_chvol_r_2, v_zero
	vor v_xvol_r_3, v_chvol_r_3, v_zero
#endif

	jr ra
	nop
	.endfunc

	#undef v_glvol 


	.func EndMixer
EndMixer:
	li s1, %lo(XVOL_L)

	sqv v_xvol_l_0,      0*MAX_CHANNELS_VOFF+0x00,s1
	sqv v_xvol_l_1,      0*MAX_CHANNELS_VOFF+0x10,s1
	sqv v_xvol_l_2,      0*MAX_CHANNELS_VOFF+0x20,s1
	sqv v_xvol_l_3,      0*MAX_CHANNELS_VOFF+0x30,s1

	sqv v_xvol_r_0,      1*MAX_CHANNELS_VOFF+0x00,s1
	sqv v_xvol_r_1,      1*MAX_CHANNELS_VOFF+0x10,s1
	sqv v_xvol_r_2,      1*MAX_CHANNELS_VOFF+0x20,s1
	sqv v_xvol_r_3,      1*MAX_CHANNELS_VOFF+0x30,s1

	jr ra
	nop

	.endfunc


##############################################################
# Mixer
#
# Arguments:
#    s4:  buffer into which the mixed samples will be stored
#
# Global state:
#    num_samples:  number of samples to mix
#
##############################################################

	#define v_out_l       $v01
	#define v_out_r       $v02
	#define v_sample_0    $v03
	#define v_sample_1    $v04
	#define v_sample_2    $v05
	#define v_sample_3    $v06
	#define v_mix_l       $v07
	#define v_mix_r       $v08

	.func Mixer
Mixer:
	# Load samples
	li s0, %lo(CHANNEL_BUFFER)
	move t1, num_samples

	# Load initial samples
	lqv v_sample_0, 0x00,s0
	lqv v_sample_1, 0x10,s0
	lqv v_sample_2, 0x20,s0
	lqv v_sample_3, 0x30,s0

	# For optimal pipelining, output is stored at the beginning of the loop. To avoid
	# corrupting memory, load the output register with whatever is there now.
	lsv v_out_l.e0, -4,s4
	ble k0, 8, Mix8Start    # Optimized mixing loop for <= 8 channels
	lsv v_out_r.e0, -2,s4

Mix32Start:
	blt t1, 8, Mix32Loop
	move t0, t1
	li t0, 8

	############################################################################
	#             VU                                          SU               #
	############################################################################
	.align 3
Mix32Loop:
	# Apply volume/panning to each channel sample.
	# left channel:
	vmulf v_mix_l, v_sample_0, v_xvol_l_0;
	vmacf v_mix_l, v_sample_1, v_xvol_l_1;             # Store previous loop's output
	vmacf v_mix_l, v_sample_2, v_xvol_l_2;             ssv v_out_l.e0, -4,s4
	vmacf v_mix_l, v_sample_3, v_xvol_l_3;             ssv v_out_r.e0, -2,s4
	# right channel:                                   # Updated counters
	vmulf v_mix_r, v_sample_0, v_xvol_r_0;             add s0, 32*2
	vmacf v_mix_r, v_sample_1, v_xvol_r_1;             addi t0, -1
	vmacf v_mix_r, v_sample_2, v_xvol_r_2;             addi s4, 4
	vmacf v_mix_r, v_sample_3, v_xvol_r_3;

	# Mix all lanes together into the first lane       # Load next loop's samples
	vadd v_out_l, v_mix_l, v_mix_l.q1;                 lqv v_sample_0.e0, 0x00,s0
	vadd v_out_r, v_mix_r, v_mix_r.q1;                 lqv v_sample_1.e0, 0x10,s0
	  # 1 cycle stall here
	vadd v_out_l, v_out_l, v_out_l.h2;                 lqv v_sample_2.e0, 0x20,s0
	vadd v_out_r, v_out_r, v_out_r.h2;                 lqv v_sample_3.e0, 0x30,s0
	  # 1 cycle stall here
	vadd v_out_l, v_out_l, v_out_l.e4;                 bnez t0, Mix32Loop
	vadd v_out_r, v_out_r, v_out_r.e4;

#if VOLUME_FILTER
	# Apply volume ramp
	vmudm v_xvol_l_0, v_xvol_l_0, k_alpha
	vmadm v_xvol_l_0, v_chvol_l_0, k_1malpha

	vmudm v_xvol_l_1, v_xvol_l_1, k_alpha
	vmadm v_xvol_l_1, v_chvol_l_1, k_1malpha

	vmudm v_xvol_l_2, v_xvol_l_2, k_alpha
	vmadm v_xvol_l_2, v_chvol_l_2, k_1malpha

	vmudm v_xvol_l_3, v_xvol_l_3, k_alpha
	vmadm v_xvol_l_3, v_chvol_l_3, k_1malpha

	vmudm v_xvol_r_0, v_xvol_r_0, k_alpha
	vmadm v_xvol_r_0, v_chvol_r_0, k_1malpha

	vmudm v_xvol_r_1, v_xvol_r_1, k_alpha
	vmadm v_xvol_r_1, v_chvol_r_1, k_1malpha

	vmudm v_xvol_r_2, v_xvol_r_2, k_alpha              # Next iteration
	vmadm v_xvol_r_2, v_chvol_r_2, k_1malpha;          addi t1, -8

	vmudm v_xvol_r_3, v_xvol_r_3, k_alpha;             bgtz t1, Mix32Start
	vmadm v_xvol_r_3, v_chvol_r_3, k_1malpha           
#else
	addi t1, -8
	bgtz t1, Mix32Start
	nop
#endif

	# Store last loop's output and exit
	ssv v_out_l.e0, -4,s4
	jr ra
	ssv v_out_r.e0, -2,s4


Mix8Start:
	blt t1, 8, Mix8Loop
	move t0, t1
	li t0, 8

	############################################################################
	#             VU                                          SU               #
	############################################################################
	.align 3
Mix8Loop:
	vmulf v_mix_l, v_sample_0, v_xvol_l_0;             ssv v_out_l.e0, -4,s4
	vmulf v_mix_r, v_sample_0, v_xvol_r_0;             ssv v_out_r.e0, -2,s4
	  # pipeline stall
	vadd v_out_l, v_mix_l, v_mix_l.q1;                 addi t0, -1
	vadd v_out_r, v_mix_r, v_mix_r.q1;                 add s0, 32*2
	  # pipeline stall
	vadd v_out_l, v_out_l, v_out_l.h2;                 addi s4, 4
	vadd v_out_r, v_out_r, v_out_r.h2;                 lqv v_sample_0, 0,s0
	  # pipeline stall
	vadd v_out_l, v_out_l, v_out_l.e4;                 bnez t0, Mix8Loop
	vadd v_out_r, v_out_r, v_out_r.e4;

#if VOLUME_FILTER
	# Apply volume ramp
	vmudm v_xvol_l_0, v_xvol_l_0, k_alpha
	vmadm v_xvol_l_0, v_chvol_l_0, k_1malpha;          addi t1, -8

	vmudm v_xvol_r_0, v_xvol_r_0, k_alpha;             bgtz t1, Mix8Start
	vmadm v_xvol_r_0, v_chvol_r_0, k_1malpha
#else
	addi t1, -8
	bgtz t1, Mix8Start
	nop 
#endif

	ssv v_out_l.e0, -4,s4
	jr ra
	ssv v_out_r.e0, -2,s4
	.endfunc


	#undef v_zero       
	#undef v_xvol_l_0   
	#undef v_xvol_r_0   
	#undef v_xvol_l_1   
	#undef v_xvol_r_1   
	#undef v_xvol_l_2   
	#undef v_xvol_r_2   
	#undef v_xvol_l_3   
	#undef v_xvol_r_3   
	#undef v_chvol_l_0  
	#undef v_chvol_r_0  
	#undef v_chvol_l_1  
	#undef v_chvol_r_1  
	#undef v_chvol_l_2  
	#undef v_chvol_r_2  
	#undef v_chvol_l_3  
	#undef v_chvol_r_3  
	#undef v_shift8     
	#undef v_shift      
	#undef v_const1
	#undef k_0000
	#undef k_8000


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

	.func VADPCM_Decompress
VADPCM_Decompress:
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

	# Create an identity matrix in pred1a-pred1h, zeroing the whole
	# matrix, and then loading the identity in the diagonal
	vxor pred1a, pred1a
	vxor pred1b, pred1b
	vxor pred1c, pred1c
	vxor pred1d, pred1d
	vxor pred1e, pred1e
	vxor pred1f, pred1f
	vxor pred1g, pred1g
	vxor pred1h, pred1h
	li s0, %lo(IDENTITY)
	ltv pred1a.e0,    0,s0

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
