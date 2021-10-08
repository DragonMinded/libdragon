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

#include <rsp.inc>

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
#define WAVERFORM_POS_FRAC_BITS  12


#define MAX_CHANNELS_VOFF  (MAX_CHANNELS*2/16)


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

	#define k_0000        v_zero,0
	#define k_8000        v_shift8,8


	.data

GLOBAL_VOLUME:            .long  0x2000
NUM_SAMPLES:              .half  0
NUM_CHANNELS:             .half  0
OUTPUT_RDRAM:             .long  0
STATE_RDRAM:               .long  0

	.align 4
CHANNEL_VOLUMES_L:        .dcb.w MAX_CHANNELS
CHANNEL_VOLUMES_R:        .dcb.w MAX_CHANNELS

	.align 4
	# FIXME: THIS COMMENT IS OUTDATED - PLEASE UPDATE IT
	#########################################################
	# 1. Waveform playing position. This is expressed as a 0.31 positive fixed point
	# number, to be interpreted as a [0-1] interval. So it needs to be multiplied
	# by the waveform length to get the actual waveform sample offset.
	# Low/high words are split for vectorization purposes.
	# 2. Waveform stepping per sample (fixed 0.31, like position).
	# 3. Waveform length, in number of samples. If the waveform is 16-bits, it
	# must be multiplied by 2 to find the actual byte offset.
	# 4. Waveform loop length
	# 5. RDRAM addresses of waveforms. Each waveform must have been fully loaded
	# in RDRAM before RSP can begin playing it. 
	# NOTE: the lowest bit of addresses is a flag: if 1, it means that the
	# waveform is made of 16-bit samples.
WAVEFORM_SETTINGS:        .dcb.l (6*MAX_CHANNELS)

	.align 4
XVOL_L:                   .dcb.w MAX_CHANNELS
XVOL_R:                   .dcb.w MAX_CHANNELS


	# Temporary cache of samples fetched by DMA. Notice that this must be
	# less or equal than MIXER_LOOP_OVERREAD (mixer.c), because the
	# RSP will over-read up to this amount of bytes after waveform's end.
	.align 3
	#define SAMPLE_CACHE_SIZE  64
DMEM_SAMPLE_CACHE:		  .dcb.b SAMPLE_CACHE_SIZE

	# Misc constants
	.align 4
VCONST_1:
	.half 0x7FFF
	.half 0xe076      #   (0.9837**8) fixed 0.16
	.half 0x1f8a      # 1-(0.9837**8) fixed 0.16

	#define k_ffff      v_const1,8
	#define k_alpha     v_const1,9
	#define k_1malpha   v_const1,10

	vsll_data
	vsll8_data

	.align 4
BANNER0:    .ascii "Dragon RSP Audio"
BANNER1:    .ascii " Coded by Rasky "

	.bss

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

	.globl _start
_start:
	setup_vsll  v_shift
	setup_vsll8 v_shift8

	vxor v_zero, v_zero, v_zero,0
	li t0, %lo(VCONST_1)
	lqv v_const1,0, 0,t0

#if VOLUME_FILTER
	# Read state from previous execution (actual channel volumes).
	# This is a state because we run the one-tap filter so their
	# value might be different from the requested one.
	jal DMAFilterState
	li t2, DMA_IN
#endif

	jal SetupMixer
	nop

MainLoop:
	# Fetch output RDRAM pointer and set t1=1 if it's not 8-byte aligned.
	# We expect the pointer to be 32-bit aligned (since it's a buffer of 16-bit
	# stereo samples), but it might not be 64-bit aligned.
	lw t1, %lo(OUTPUT_RDRAM)
	andi t1, 7
	slt t1, zero, t1

	lh t4, %lo(NUM_SAMPLES)
	beqz t4, End

	# The maximum number of samples that we want to generate
	# in a single loop is MAX_SAMPLES_PER_LOOP. Subtract 
	# 1 from this number if RDRAM buffer is not aligned so
	# that after this loop we're 8-byte aligned again.
	li k1, MAX_SAMPLES_PER_LOOP
	sub k1, t1

	# nsamples = MIN(nsamples, MAX_SAMPLES_PER_LOOP[-1])
	bgt t4, k1, CheckDMAAlignment
	nop
	move k1, t4
CheckDMAAlignment:
	# If the output buffer is not aligned, fetch one DMA line (8 bytes)
	# so that we preserve the 4 bytes that come before the buffer we were
	# given (which would be overwritten by the RSP DMA).
	beqz t1, DoLoop
	li gp, %lo(OUTPUT_AREA)

	lw s0, %lo(OUTPUT_RDRAM)
	li s4, %lo(OUTPUT_AREA)
	jal DMAIn
	li t0, DMA_SIZE(8,1)
	addi gp, 4

DoLoop:
	# Update number of samples left
	sub t4, k1
	sh t4, %lo(NUM_SAMPLES)

	# Fetch the samples and do resampling
	jal UpdateAndFetch
	lhu k0, %lo(NUM_CHANNELS)

	# Mix the samples
	jal Mixer
	move s4, gp

	# Update the output pointer in RDRAM for next loop.
	sll t0, k1, 2
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

#if VOLUME_FILTER
	jal DMAFilterState
	li t2, DMA_OUT_ASYNC
#endif

	# Wait for the last out transfer to be finished
	jal DMAWaitIdle
	nop

	# Bye bye!
	break


###############################################################
# DMAFilterState - Load/save the volume filter state via DMA.
#
# Arguments:
#   t2:  DMA_* flag for DMAExec
###############################################################

	.func DMAFilterState
DMAFilterState:
	# Save volume filter state
	lw s0, %lo(STATE_RDRAM)
	li s4, %lo(XVOL_L)
	j DMAExec
	li t0, DMA_SIZE(MAX_CHANNELS*2*2, 1)
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
#  k1:  number of ticks to resample
#
###############################################################

	#define ticks          t3
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

	.func UpdateAndFetch
UpdateAndFetch:
	move ra2, ra

	li s5, %lo(CHANNEL_BUFFER)
	li t0, (MAX_SAMPLES_PER_LOOP * MAX_CHANNELS * 2) / 64 - 1
ClearLoop:
	sqv v_zero,0, 0,s5
	sqv v_zero,0, 1,s5
	sqv v_zero,0, 2,s5
	sqv v_zero,0, 3,s5
	addi s5, 64
	bnez t0, ClearLoop
	addi t0, -1

	li s1, %lo(WAVEFORM_SETTINGS)
	li nchan, 0

	li dma_cache_end, %lo(DMEM_SAMPLE_CACHE+SAMPLE_CACHE_SIZE)
	sll dma_cache_end, WAVERFORM_POS_FRAC_BITS

	# main update loop: will be done once per channel.
UpdateLoop:
	# Fetch waveform parameters. Notice that if the RDRAM
	# address is zero, no waveform was configured in this channel,
	# so we can skip directly to next channel.
	lw wv_addr,     16(s1)
	beqz wv_addr, WaveLoopEpilog
	lw wv_pos,       0(s1)
	lw wv_step,      4(s1)
	lw wv_len,       8(s1)
	lw wv_loop_len, 12(s1)

	# Prepare output pointer
	addi out_ptr, nchan, %lo(CHANNEL_BUFFER)
	add out_ptr, nchan
	move ticks, k1

	# Flags: isolate 16-bit flag (bit 0)
	lw t1, 20(s1)
	andi t1, 1
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
	srl s2, wv_pos, WAVERFORM_POS_FRAC_BITS
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
	sll dma_cache_end, WAVERFORM_POS_FRAC_BITS
#endif



	# Calculate an offset that converts wv_pos from a (fixed point) RDRAM
	# pointer into a (fixed point) DMEM pointer. This will be used because
	# the inner loop will use wv_pos adjusted to be a DMEM pointer
	# rather than a RDRAM pointer, for performance.
	sub wv_pos_to_dmem, s2, s4
	sll wv_pos_to_dmem, WAVERFORM_POS_FRAC_BITS

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
	bnez t1, WaveLoop16_8x
	sub wv_pos, wv_pos_to_dmem

WaveLoop8_8x:
	add t0, wv_pos, wv_step_8x                 # check if pos+step*8 is still in the DMEM cache
	bge t0, dma_cache_end, WaveLoop8Checks
	li t0, 8                                   # check if we need to process at least 8 samples
	blt ticks, t0, WaveLoop8Checks

	# Process 8 samples
	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS    # Get raw DMEM pointer (integer part of wv_pos)
	lbu t0, 0(t0)                              # Fetch the sample
	add wv_pos, wv_step                        # Update wv_pos (load cycle delay)
	sb t0, (0*MAX_CHANNELS*2+0)(out_ptr)       # Store the sample

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (1*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (2*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (3*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (4*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (5*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (6*MAX_CHANNELS*2+0)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS
	lbu t0, 0(t0)
	add wv_pos, wv_step
	sb t0, (7*MAX_CHANNELS*2+0)(out_ptr)

	# Update output pointer and ticks counter, and loop.
	add out_ptr, 8*MAX_CHANNELS*2
	j WaveLoop8_8x
	addi ticks, -8

WaveLoop8:
	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS        # Get the raw DMEM pointer
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

WaveLoop16_8x:
	add t0, wv_pos, wv_step_8x
	bge t0, dma_cache_end, WaveLoop16Checks
	li t0, 8
	blt ticks, t0, WaveLoop16Checks

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (0*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (1*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (2*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (3*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (4*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (5*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (6*MAX_CHANNELS*2)(out_ptr)

	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
	sll t0, 1
	lhu t0, 0(t0)
	add wv_pos, wv_step
	sh t0, (7*MAX_CHANNELS*2)(out_ptr)

	add out_ptr, 8*MAX_CHANNELS*2
	j WaveLoop16_8x
	addi ticks, -8

WaveLoop16:
	srl t0, wv_pos, WAVERFORM_POS_FRAC_BITS+1
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

WaveBeforeEpilog:
	add wv_pos, wv_pos_to_dmem

WaveLoopEpilog:
	sw wv_pos, 0(s1)   # store updated wv_pos in DMEM

	addi nchan, 1
	bne nchan, k0, UpdateLoop
	addi s1, 6*4

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
	lw t0, %lo(GLOBAL_VOLUME)
	mtc2 t0, v_glvol,0
	vor v_glvol, v_zero, v_glvol,8

	li s0, %lo(CHANNEL_VOLUMES_L)
	li s1, %lo(XVOL_L)

	# Load channel volumes (left / right)
	lqv v_chvol_l_0,0,     0*MAX_CHANNELS_VOFF+0,s0
	lqv v_chvol_l_1,0,     0*MAX_CHANNELS_VOFF+1,s0
	lqv v_chvol_l_2,0,     0*MAX_CHANNELS_VOFF+2,s0
	lqv v_chvol_l_3,0,     0*MAX_CHANNELS_VOFF+3,s0

	lqv v_chvol_r_0,0,     1*MAX_CHANNELS_VOFF+0,s0
	lqv v_chvol_r_1,0,     1*MAX_CHANNELS_VOFF+1,s0
	lqv v_chvol_r_2,0,     1*MAX_CHANNELS_VOFF+2,s0
	lqv v_chvol_r_3,0,     1*MAX_CHANNELS_VOFF+3,s0

	# Apply global volume to obtain the final volume for each channel
	vmudl v_chvol_l_0, v_chvol_l_0, v_glvol,0
	vmudl v_chvol_r_0, v_chvol_r_0, v_glvol,0
	vmudl v_chvol_l_1, v_chvol_l_1, v_glvol,0
	vmudl v_chvol_r_1, v_chvol_r_1, v_glvol,0
	vmudl v_chvol_l_2, v_chvol_l_2, v_glvol,0
	vmudl v_chvol_r_2, v_chvol_r_2, v_glvol,0
	vmudl v_chvol_l_3, v_chvol_l_3, v_glvol,0
	vmudl v_chvol_r_3, v_chvol_r_3, v_glvol,0

#if VOLUME_FILTER
	# Load actual volumes levels
	lqv v_xvol_l_0,0,      0*MAX_CHANNELS_VOFF+0,s1
	lqv v_xvol_l_1,0,      0*MAX_CHANNELS_VOFF+1,s1
	lqv v_xvol_l_2,0,      0*MAX_CHANNELS_VOFF+2,s1
	lqv v_xvol_l_3,0,      0*MAX_CHANNELS_VOFF+3,s1

	lqv v_xvol_r_0,0,      1*MAX_CHANNELS_VOFF+0,s1
	lqv v_xvol_r_1,0,      1*MAX_CHANNELS_VOFF+1,s1
	lqv v_xvol_r_2,0,      1*MAX_CHANNELS_VOFF+2,s1
	lqv v_xvol_r_3,0,      1*MAX_CHANNELS_VOFF+3,s1
#else
	vor v_xvol_l_0, v_chvol_l_0, v_zero,0
	vor v_xvol_l_1, v_chvol_l_1, v_zero,0
	vor v_xvol_l_2, v_chvol_l_2, v_zero,0
	vor v_xvol_l_3, v_chvol_l_3, v_zero,0

	vor v_xvol_r_0, v_chvol_r_0, v_zero,0
	vor v_xvol_r_1, v_chvol_r_1, v_zero,0
	vor v_xvol_r_2, v_chvol_r_2, v_zero,0
	vor v_xvol_r_3, v_chvol_r_3, v_zero,0
#endif

	jr ra
	nop
	.endfunc

	#undef v_glvol 


	.func EndMixer
EndMixer:
	li s1, %lo(XVOL_L)

	sqv v_xvol_l_0,0,      0*MAX_CHANNELS_VOFF+0,s1
	sqv v_xvol_l_1,0,      0*MAX_CHANNELS_VOFF+1,s1
	sqv v_xvol_l_2,0,      0*MAX_CHANNELS_VOFF+2,s1
	sqv v_xvol_l_3,0,      0*MAX_CHANNELS_VOFF+3,s1

	sqv v_xvol_r_0,0,      1*MAX_CHANNELS_VOFF+0,s1
	sqv v_xvol_r_1,0,      1*MAX_CHANNELS_VOFF+1,s1
	sqv v_xvol_r_2,0,      1*MAX_CHANNELS_VOFF+2,s1
	sqv v_xvol_r_3,0,      1*MAX_CHANNELS_VOFF+3,s1

	jr ra
	nop

	.endfunc


##############################################################
# Mixer
#
# Arguments:
#    s4:  buffer into which the mixed samples will be stored
#    k1:  num samples
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
	move t1, k1

	# Load initial samples
	lqv v_sample_0,0, 0,s0
	lqv v_sample_1,0, 1,s0
	lqv v_sample_2,0, 2,s0
	lqv v_sample_3,0, 3,s0

	# For optimal pipelining, output is stored at the beginning of the loop. To avoid
	# corrupting memory, load the output register with whatever is there now.
	lsv v_out_l,0, -2,s4
	blt k0, 8, Mix8Start    # Optimized mixing loop for <= 8 channels
	lsv v_out_r,0, -1,s4

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
	vmulf v_mix_l, v_sample_0, v_xvol_l_0,0;                   
	vmacf v_mix_l, v_sample_1, v_xvol_l_1,0;           # Store previous loop's output         
	vmacf v_mix_l, v_sample_2, v_xvol_l_2,0;           ssv v_out_l,0, -2,s4
	vmacf v_mix_l, v_sample_3, v_xvol_l_3,0;           ssv v_out_r,0, -1,s4
	# right channel:                                   # Updated counters
	vmulf v_mix_r, v_sample_0, v_xvol_r_0,0;           add s0, 32*2
	vmacf v_mix_r, v_sample_1, v_xvol_r_1,0;           addi t0, -1
	vmacf v_mix_r, v_sample_2, v_xvol_r_2,0;           addi s4, 4
	vmacf v_mix_r, v_sample_3, v_xvol_r_3,0;

	# Mix all lanes together into the first lane       # Load next loop's samples
	vaddc v_out_l, v_mix_l, v_mix_l,3;                 lqv v_sample_0,0, 0,s0
	vaddc v_out_r, v_mix_r, v_mix_r,3;                 lqv v_sample_1,0, 1,s0
	  # 1 cycle stall here
	vaddc v_out_l, v_out_l, v_out_l,6;                 lqv v_sample_2,0, 2,s0
	vaddc v_out_r, v_out_r, v_out_r,6;                 lqv v_sample_3,0, 3,s0
	  # 1 cycle stall here
	vaddc v_out_l, v_out_l, v_out_l,12;                bnez t0, Mix32Loop
	vaddc v_out_r, v_out_r, v_out_r,12;                

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
	ssv v_out_l,0, -2,s4
	jr ra
	ssv v_out_r,0, -1,s4


Mix8Start:
	blt t1, 8, Mix32Loop
	move t0, t1
	li t0, 8

	############################################################################
	#             VU                                          SU               #
	############################################################################
	.align 3
Mix8Loop:
	vmulf v_mix_l, v_sample_0, v_xvol_l_0,0;           ssv v_out_l,0, -2,s4
	vmulf v_mix_r, v_sample_0, v_xvol_r_0,0;           ssv v_out_r,0, -1,s4
	  # pipeline stall
	vaddc v_out_l, v_mix_l, v_mix_l,3;                 addi t0, -1
	vaddc v_out_r, v_mix_r, v_mix_r,3;                 add s0, 32*2
	  # pipeline stall
	vaddc v_out_l, v_out_l, v_out_l,6;                 addi s4, 4
	vaddc v_out_r, v_out_r, v_out_r,6;                 lqv v_sample_0,0, 0,s0
	  # pipeline stall
	vaddc v_out_l, v_out_l, v_out_l,12;                bnez t0, Mix8Loop
	vaddc v_out_r, v_out_r, v_out_r,12;

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

	ssv v_out_l,0, -2,s4
	jr ra
	ssv v_out_r,0, -1,s4
	.endfunc

# Bring in RSP DMA library
#include <rsp_dma.inc>
