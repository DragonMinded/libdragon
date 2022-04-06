	####################################################################
	#
	# Libdragon RSP ucode for audio mixer
	#
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
	# (that is, operating a frequency change using a linear interpolation),
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
	# Resampling means doing a linear interpolation on the index used to go
	# through the input sample, according to the resampling frequency. The
	# channel configuration already contains a fixed-point "step" value for
	# each channel, that is used as increment in the loop. For instance, if
	# the step is 2, it means that every other sample will be skipped, thus
	# achieving a 0.5x resampling (= playing a 88Khz sample on a 44Khz output).
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
		RSPQ_DefineCommand command_exec, 16
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

	vsll_data
	vsll8_data

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
	RSPQ_EndSavedState

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

	.text

	# Number of samples that will be processed in the current loop.
	#define num_samples     k1


command_exec:
	setup_vsll  v_shift
	setup_vsll8 v_shift8

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

	lw a2, CMD_ADDR(0x8, 0x10)
	sw a2, %lo(OUTPUT_RDRAM)

	# Load settings
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

	jal DMASettings
	li t2, DMA_OUT_ASYNC

	# Wait for the last out transfer to be finished
	jal_and_j DMAWaitIdle, RSPQ_Loop

	#undef samples_left
	#undef outptr


###############################################################
# DMASettings - Load/save the settings via DMA.
#
# Arguments:
#   t2:  DMA_* flag for DMAExec
###############################################################

	.func DMASettings
DMASettings:
	# Save settings
	lw s0, CMD_ADDR(0xC, 0x10)
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

#if 0 
	# TEST WITHOUT OVERREAD

	sub wv_addr_end, SAMPLE_CACHE_SIZE # do it once

	sub t0, wv_addr_end, s0
	bltz t0, start_dma
	addi t0, SAMPLE_CACHE_SIZE
	li t0, SAMPLE_CACHE_SIZE
start_dma:
	addi dma_cache_end, t0, %lo(DMEM_SAMPLE_CACHE)
	sll dma_cache_end, WAVEFORM_POS_FRAC_BITS
#endif



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
	vaddc v_out_l, v_mix_l, v_mix_l.q1;                lqv v_sample_0.e0, 0x00,s0
	vaddc v_out_r, v_mix_r, v_mix_r.q1;                lqv v_sample_1.e0, 0x10,s0
	  # 1 cycle stall here
	vaddc v_out_l, v_out_l, v_out_l.h2;                lqv v_sample_2.e0, 0x20,s0
	vaddc v_out_r, v_out_r, v_out_r.h2;                lqv v_sample_3.e0, 0x30,s0
	  # 1 cycle stall here
	vaddc v_out_l, v_out_l, v_out_l.e4;                bnez t0, Mix32Loop
	vaddc v_out_r, v_out_r, v_out_r.e4;

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
	vaddc v_out_l, v_mix_l, v_mix_l.q1;                addi t0, -1
	vaddc v_out_r, v_mix_r, v_mix_r.q1;                add s0, 32*2
	  # pipeline stall
	vaddc v_out_l, v_out_l, v_out_l.h2;                addi s4, 4
	vaddc v_out_r, v_out_r, v_out_r.h2;                lqv v_sample_0, 0,s0
	  # pipeline stall
	vaddc v_out_l, v_out_l, v_out_l.e4;                bnez t0, Mix8Loop
	vaddc v_out_r, v_out_r, v_out_r.e4;

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
