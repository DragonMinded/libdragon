##########################################################################
# RSPH264 - Common functions
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
##########################################################################
#
# This library contains the common code used by the RSPH264 overlays.
#
##########################################################################

.macro SpinWait len
    .set noat
    li $1,\len
1:  bnez $1,1b
    addi $1,-1
    .set at
.endm

# Asertion codes
#define ASSERT_DELTA_BUFFER_TOO_SMALL  0xAF00
#define ASSERT_CAVLC_BUFFER_TOO_SMALL  0xAF01

################################################################################
# DATA
################################################################################

    .data
#if 0
TQ_ADDR:    .long 0   # Task queue address in RDRAM
TQ_SKIP:    .long 0   # Skip task queue for a single command

    .align 3
OMXDL_FUNC: .long 0   # OpenMAX command to execute
OMXDL_ARGS: .long 0,0,0,0,0,0,0,0,0,0,0   # space for 11 arguments
#endif

    .align 4
BANNER0:    .ascii "RSPH264 by Rasky"
BANNER1:    .ascii "Mod: MB Decoder "
#RIDX_WSTAT: .byte 0x55,  0x56,  0x59,  0x5a,  0x65,  0x66,  0x69,  0x6a,  0x95,  0x96,  0x99,  0x9a,  0xa5,  0xa6,  0xa9,  0xaa

    #vsll_data   # define constants for VSLL pseudo-opcode
    #vsll8_data   # define constants for VSLL8 pseudo-opcode

    .align 4
    # Coefficients of FIR filter used in Luma interpolation.
    # Pre-shifted by 3 to help achieving the correct final precision
    # (see the comments in the interpolation functions).
LUMA_FIR_COEFFS:
    .half 1<<3       # [e8] = 1<<3
    .half -5<<3      # [e9] = -5<<3
    .half 20<<3      # [e10] = 20<<3
    .half 0
    .half 0
    .half 0x1        # [e13] = 1
    .half 0xFFFF     # [e14] = -1 (for negating)
    .half 0xFF       # [e15] = 255 (for clamping)

    .align 4
IQUANT_TABLE:
    #define IQV 0
    .byte 10<<IQV,13<<IQV,10<<IQV,13<<IQV,13<<IQV,16<<IQV,13<<IQV,16<<IQV,10<<IQV,13<<IQV,10<<IQV,13<<IQV,13<<IQV,16<<IQV,13<<IQV,16<<IQV
    .byte 11<<IQV,14<<IQV,11<<IQV,14<<IQV,14<<IQV,18<<IQV,14<<IQV,18<<IQV,11<<IQV,14<<IQV,11<<IQV,14<<IQV,14<<IQV,18<<IQV,14<<IQV,18<<IQV
    .byte 13<<IQV,16<<IQV,13<<IQV,16<<IQV,16<<IQV,20<<IQV,16<<IQV,20<<IQV,13<<IQV,16<<IQV,13<<IQV,16<<IQV,16<<IQV,20<<IQV,16<<IQV,20<<IQV
    .byte 14<<IQV,18<<IQV,14<<IQV,18<<IQV,18<<IQV,23<<IQV,18<<IQV,23<<IQV,14<<IQV,18<<IQV,14<<IQV,18<<IQV,18<<IQV,23<<IQV,18<<IQV,23<<IQV
    .byte 16<<IQV,20<<IQV,16<<IQV,20<<IQV,20<<IQV,25<<IQV,20<<IQV,25<<IQV,16<<IQV,20<<IQV,16<<IQV,20<<IQV,20<<IQV,25<<IQV,20<<IQV,25<<IQV
    .byte 18<<IQV,23<<IQV,18<<IQV,23<<IQV,23<<IQV,29<<IQV,23<<IQV,29<<IQV,18<<IQV,23<<IQV,18<<IQV,23<<IQV,23<<IQV,29<<IQV,23<<IQV,29<<IQV    
    #undef IQV

    .align 4
IDCT_COEEF:
    # Row coeffs
    #define IDVR  (14-8)
    #define I1    (1<<IDVR)
    #define I05   (1<<(IDVR-1))
    #define IM05  (-1<<(IDVR-1))
    #define IM1   (-1<<IDVR)
    .byte I1, I1, I1, I1,      I1, I1, I1, I1
    .byte I1, I05, IM05, IM1,  I1, I05, IM05, IM1
    .byte I1, IM1, IM1, I1,    I1, IM1, IM1, I1
    .byte I05, IM1, I1, IM05,  I05, IM1, I1, IM05
    #undef I1
    #undef I05
    #undef IM05
    #undef IM1
    #undef IDVR
    .half 0xFFFF, 0xFFFC, 0xFFFC, 0xFFFF,    0xFFFF, 0xFFFC, 0xFFFC, 0xFFFF
    .half 0xFFFC, 0xFFFF, 0xFFFF, 0xFFFC,    0xFFFC, 0xFFFF, 0xFFFF, 0xFFFC

    # Column coeffs
    #define IDVC  (9-8)
    #define I1    (1<<IDVC)
    #define I05   (1<<(IDVC-1))
    #define IM05  (-1<<(IDVC-1))
    #define IM1   (-1<<IDVC)
    .byte I1, I1, I1, I05,     I05, I1, IM1, IM1
    .byte I1, IM05, IM1, I1,  IM1, I1, IM05, I1
    #undef I1
    #undef I05
    #undef IM05
    #undef IM1
    #undef IDVC
    .half 0xFFFE, 0xFFFE, 0xFFFF, 0xFFFF,    0xFFFE, 0xFFFF, 0xFFFE, 0xFFFF

    .align 4
    # Coefficients used for LumaDC / ChromaDC IHT (Inverse Hadamard Transformatiom).
IHT_DC_COEFF:
    # Row coeffs
    #define IDVR  (14-8)
    #define I1    (1<<IDVR)
    #define IM1   (-1<<IDVR)

    # IHT coefficients for rows transformation.
    # Luma and Chroma share the same table, though with different row
    # ordering.
    .byte I1, I1, I1, I1,     I1, I1, I1, I1
    .byte I1, IM1, I1, IM1,   I1, IM1, I1, IM1
    .byte I1, I1, IM1, IM1,   I1, I1, IM1, IM1
    .byte I1, IM1, IM1, I1,   I1, IM1, IM1, I1

    # IHT coefficients for column transformations
    # Only used by LumaDC.
    .byte I1, I1, I1, I1,    I1, I1, IM1, IM1
    .byte I1, IM1, IM1, I1,  IM1, I1, IM1, I1

    # Chroma DC masking (only keep 4 lanes).
    .half 0xFFFF,0xFFFF,0xFFFF,0xFFFF, 0,0,0,0
    #undef I1
    #undef IM1
    #undef IDVR

LUMA_DEQUANT_INDEX:
    .byte 0, 4, 64, 68, 8, 12, 72, 76, 128, 132, 192, 196, 136, 140, 200, 204
CHROMA_DEQUANT_INDEX:
    .byte 0, 4, 32, 36, 64, 68, 96, 100
DC_DEQUANT_INDEX:   # pre-scaled by 2
    .byte 0, 2, 8, 10, 4, 6, 12, 14, 16, 18, 24, 26, 20, 22, 28, 30

    # FIXME: The following symbols should really be in .bss. Unfortunately, 
    # overlays can define additional data symbols, and .bss starts
    # right at the end of .data. So basically if we defined them in BSS,
    # they'd get a different memory offset depending on the overlay, and
    # this means that we can't really have common code referring to them.
    # See if we can fix it in the future, maybe with a .bss2 section at
    # fixed address or something like that,

RSPQ_BeginSavedState
        .align 4
    #define PACKED_DELTA_MAX 840  // keep this in sync with rsph264.c
        # Buffer for delta coefficients
    PACKED_DELTA: .dcb.b PACKED_DELTA_MAX
    #define CAVLC_BUF_MAX    208  // keep this in sync with rsph264.c
    CAVLC_BUF: .dcb.b    CAVLC_BUF_MAX

    PACKED_DELTA_RDRAM: .long 0
    PACKED_DELTA_CUR:   .long 0
RSPQ_EndSavedState

CAVLC_BUF_RDRAM:    .long 0    # Original RDRAM pointer (start of buffer)
CAVLC_BUF_CUR:      .long 0    # Current pointer in DMEM << 3 + bitoffset

    .align 3
    # CALVC buffer descriptor. This is synchronized back to RDRAM
    # through CAVLCBufferUpdateRDRAM.
CAVLC_OUT_RDRAM:    .long 0    # Updated RDRAM pointer
CAVLC_OUT_BITOFF:   .long 0    # Updated bit offset

    # TCOEFF_TABLE contains TotalCoeff coefficients for all
    # luma/chroma blocks, with also room available for the neighbor
    # blocks (neighbor blocks totalCoeff is used during cavlc decoding).
    #
    # This is the layout:
    #
    #          Y             Cb       Cr
    #
    #     24 25 26 27      32 33     36 37
    #     -----------      -----     -- --
    #  28| 0  1  4  5   34|16 17  38|20 21
    #  29| 2  3  6  7   35|18 19  39|22 23
    #  30| 8  9 12 13
    #  31|10 11 14 15
    #
    .align 4
TCOEFF_TABLE: .dcb.b 40

    .align 4
LUMA_DC:            .half 0,0,0,0
                    .half 0,0,0,0
                    .half 0,0,0,0
                    .half 0,0,0,0
CHROMA_DC_0:        .half 0,0,0,0
CHROMA_DC_1:        .half 0,0,0,0

    # Empty space for building up vectors, used by many functions
        .align 4
V_0:    .quad 0,0
V_1:    .quad 0,0

#if 0
    # Task function table. Notice that we're using .half here for compactness, 
    # which is not supported by GAS/ELF, and it works with some bugs: we need to
    # add 16-bit of padding at the end, and there's an additional 16-bit at the start.

#define DECLARE_OMXDL_TABLE(...) \
    .half 0, \
          Task_BeginFrame - _start, \
          Task_EndFrame - _start, \
          Task_LoadOverlay - _start, \
          Task_SetPackedDeltaBuffer - _start, \
          Task_ResetPackedDeltaBuffer - _start, \
          Task_SetTotalCoeff - _start, \
          Task_ResetTotalCoeff - _start, \
          Task_WriteMacroblock - _start, \
          __VA_ARGS__, \
          0

        .align 3
OMXDL_TABLE:
DATA_OVERLAY_START:
#endif

################################################################################
# TEXT
################################################################################

    .text

#if 0
    .func AssertDeltaBufferTooSmall
AssertDeltaBufferTooSmall:
    li t0, ASSERT_DELTA_BUFFER_TOO_SMALL
    j AssertDeltaBufferTooSmall
    sw t0, %lo(TQ_SKIP)
    .endfunc

    .func AssertCAVLCBufferTooSmall
AssertCAVLCBufferTooSmall:
    li t0, ASSERT_CAVLC_BUFFER_TOO_SMALL
    j AssertCAVLCBufferTooSmall
    sw t0, %lo(TQ_SKIP)
    .endfunc
#endif

#if 0
####################################################################
# Task_WriteMacroblock
#
# Copy the reconstructed macroblock into the destination image
####################################################################
# ARGS:
#   0: SRC: Macroblock address Y/CB/CR (384 bytes)
#   1: DST_LUMA: Destination luma buffer 
#   2: DST_CB: Destination chroma buffer
#   3: DST_CR: Destination chroma buffer
#   4: DST_MB_WIDTH: Destination width in macroblocks

    #define src a0
    #define dst_luma a1
    #define dst_cb a2
    #define dst_cr a3
    #define dst_mb_width v0

    .func Task_WriteMacroblock
Task_WriteMacroblock:
    # Use end of DMEM as scratch buffer; this should be in BSS.
    li s4, 4096-(256+64+64)
    li t0, DMA_SIZE(256+64+64,1)
    jal DMAInAsync
    move s0, src

    lw dst_mb_width, %lo(OMXDL_ARGS+16)

    # Chroma blocks: 8x8 (pitch = dst_mb_width*8)
    sll t1, dst_mb_width, 3
    li t0, DMA_SIZE(8,8)

    addi s4, 256
    jal DMAOutAsync
    move s0, dst_cb

    addi s4, 64
    jal DMAOutAsync
    move s0, dst_cr

    # Luma block: 16x16 (pitch = dst_mb_width*16)
    # We do luma as last block as it's the bigger, so that we 
    # leave the function while it's in progress, and hopefully completes
    # in background without spin-waits.
    sub s4, 256+64
    sll t1, dst_mb_width, 4
    li t0, DMA_SIZE(16,16)
    move s0, dst_luma
    jal_and_j DMAOutAsync, RSPQ_Loop

    .endfunc

    #undef src
    #undef dst_luma
    #undef dst_cb
    #undef dst_cr
    #undef dst_mb_width
#endif

####################################################################
# COMMON RESIDUAL FUNCTIONS
#
# This section contains functions that are needed for residual
# calculations in both intra and inter prediction. We put them
# here so they are shared among the two overlays.
#
####################################################################

####################################################################
# Task_SetPackedDeltaBuffer
#
# Configure the packed buffer of delta coefficients that will be
# used for subsequent residual functions.
#
# Residual functions read delta coefficients from a packed buffer,
# that contain input for several functions called in specific order
# depending of the macro-block encoding type. This function reads
# the buffer and sets the internal pointer to the start.
# To unpack the 4x4 blocks, repeatedly call UnpackNextDeltaBlock.
#
# We copy the buffer into RSP once, and then process it. This not
# only saves DMA time as the whole buffer is DMA'd once, but also it
# does not create dependency between functions (which would otherwise
# need to communicate back the current pointer within the buffer). 
####################################################################
# ARGS:
#   0: SRC: RDRAM address of packed delta buffer

    .func Task_SetPackedDeltaBuffer
Task_SetPackedDeltaBuffer:
    move s0, a0
    # We don't know how long is the buffer; let's just DMA as much
    # as possible in our buffer, and assume it's enough.
    li t0, DMA_SIZE(PACKED_DELTA_MAX,1)
    jal DMAIn
    li s4, %lo(PACKED_DELTA)

    # Reflect the unalignment offset into the RDRAM pointer,
    # so that we're later able to provide a correct RDRAM
    # pointer back to the CPU (if requested).
    sub a0, s4
    addi a0, %lo(PACKED_DELTA)
    sw a0, %lo(PACKED_DELTA_RDRAM)
    j RSPQ_Loop
    sw s4, %lo(PACKED_DELTA_CUR)
    .endfunc

#if 0
####################################################################
# Task_ResetPackedDeltaBuffer
#
# Initialize delta buffer as empty (for writing into it via CALVC
# decoding).
####################################################################

    .func Task_ResetPackedDeltaBuffer
Task_ResetPackedDeltaBuffer:
    sw zero, %lo(PACKED_DELTA_RDRAM)
    li s4, %lo(PACKED_DELTA)
    j RSPQ_Loop
    sw s4, %lo(PACKED_DELTA_CUR)
    .endfunc
#endif

####################################################################
# UnpackNextDeltaBlock
#
# Unpack the next 4x4 residual coefficient block encoded in the
# delta buffer, previously configured by Task_SetPackedDeltaBuffer.
# The internal pointer is updated to the start of next block.
#
####################################################################
# ARGS:
#   None
#
# RESULT:
#   $v00: row 0 & 1 of 4x4 residual coefficient matrix
#   $v01: row 2 & 3 of 4x4 residual coefficient matrix
#
    .func UnpackNextDeltaBlock
UnpackNextDeltaBlock:
    lw s0, %lo(PACKED_DELTA_CUR)

    # Clear temporary buffer where we will build the matrix
    vxor $v00, $v00, $v00,0
    li s1, %lo(V_0)
    sqv $v00,0, 0*16,s1
    sqv $v00,0, 1*16,s1

UnpackLoop:
    lb t0, 0(s0)        # read control
    andi t2, t0, 0xF    # extract index
    sll t2, 1
    add t2, s1, t2      # calculate pointer into V_0/V_1
    andi t1, t0, 0x10   # check if 16-bit
    beqz t1, UnpackStore
    lb t3, 1(s0)        # load LSB (signed)
    lb t1, 2(s0)        # load MSB (signed) for 16-bit value
    sll t1, 8
    andi t3, 0xFF
    or t3, t1           # compose MSB/LSB into 16-bit word
    addi s0, 1
UnpackStore:
    sh t3, 0(t2)        # store into unpacked block
    andi t0, 0x20       # is this the last?
    beqz t0, UnpackLoop
    addi s0, 2
    sw s0, %lo(PACKED_DELTA_CUR)
    lqv $v00,0, 0*16,s1
    lqv $v01,0, 1*16,s1

UpdateDeltaBlockPointer:
    # HACK: update the RDRAM src pointer into the task argument
    # buffer: it is required by the CPU to know how
    # many bytes of the source buffer were consumed, in case it needs
    # to be further processed by the CPU itself.
    # This is not needed if 100% of residual decoding is made on
    # RSP, but we keep it to be able to enable/disable parts of RSP
    # decoding for debugging purposes.
    lw s0, %lo(PACKED_DELTA_CUR)
    li t0, %lo(PACKED_DELTA)+PACKED_DELTA_MAX
    assert_lt s0, t0, ASSERT_DELTA_BUFFER_TOO_SMALL
#if 0
    lw t0, %lo(PACKED_DELTA_RDRAM)
    li t1, %lo(PACKED_DELTA)
    sub t1, s0, t1
    add t0, t1
    jr ra
    sw t0, %lo(OMXDL_ARGS+0)
#endif
    jr ra
     nop
    .endfunc


####################################################################
# Task_ResetTotalCoeff
#
# Reset the totalCoeff table (fill with zeros)
####################################################################
#if 0
    .func Task_ResetTotalCoeff
Task_ResetTotalCoeff:
    vxor $v00, $v00, $v00,0
    li s4, %lo(TCOEFF_TABLE)
    sqv $v00,0, 0*16,s4
    sqv $v00,0, 1*16,s4
    jr ra
    spv $v00,0, 4*8,s4
    .endfunc
#endif

####################################################################
# Task_SetTotalCoeff
#
# Load the totalCoeff table from RDRAM via DMA
####################################################################

    .func Task_SetTotalCoeff
Task_SetTotalCoeff:
    move s0, a0
SetTotalCoeff:
    beqz s0, JrRa
    li t0, DMA_SIZE(27,1)
    j DMAIn
    li s4, %lo(TCOEFF_TABLE)
    .endfunc


####################################################################
# CalcDequantFactors
#
# Calculate the vectorial factors that can be used to run the
# dequantization steps required by Luma/Chroma residual calculations.
#
# Quantization can be applied using Dequant4x4.
#
####################################################################
# ARGS:
#   t0: quantization parameter
#
# OUTPUT:
#   $v29: scale factor (first lane, [e8]), can be used with vmudn to
#         apply scaling.
#   $v27/$v28: multiplication factor (8x8 block arranged in two vectors),
#              can be used with vmudn.
#

    .func CalcDequantFactors    
CalcDequantFactors:
    # Extract shift factor and set 1<<shift into a $v29[e8]; this
    # will be used as multiplication factor to shift.
    srl t1, t0, 8
    li t2, 1
    sllv t1, t2, t1
    mtc2 t1, $v29,0

    # Select the quantization table given QP, and load the multiplication
    # factors from it in v27/v28.
    li s2, %lo(IQUANT_TABLE)
    and t0, 0xFF
    sll t0, 4
    add s2, t0, s2
    luv $v27,0, 0*8,s2
    luv $v28,0, 1*8,s2
    vsrl $v27, $v27, 7
    jr ra
    vsrl $v28, $v28, 7
    .endfunc

####################################################################
# Dequant4x4
#
# Handle de-quantization of 4x4 pixel residual coefficients.
# Must be called after CalcDequantFactors
#
####################################################################
# ARGS:
#   $v00/$v01: 4x4 quantized coefficient matrix
#
# ARGS CALCULATED BY CalcDequantFactors:
#   $v29: scale factor (first lane, [e8])
#   $v27/$v28: multiplication factor (4x4 block arranged in two vectors)
#
    .func Dequant4x4
Dequant4x4:
    # Apply multiplication factors to delta coefficients
    vmudn $v00, $v00, $v27,0
    vmudn $v01, $v01, $v28,0

    # Apply scaling to delta coefficients
    vmudn $v00, $v00, $v29,8
    jr ra
    vmudn $v01, $v01, $v29,8
    .endfunc



####################################################################
# TransformResidual4x4IDCT
#
# Handle IDCT transform of 4x4 pixel luma residual
# deltas, and merge them into the 4x4 predicted pixel buffer.
#
####################################################################
# ARGS:
#       s4:  address of predicted pixel buffer in DMEM
#       t4:  destination buffer width
#  v00/v01:  unpacked delta coefficients: 4x4 block of 16-bit values
#            (calculated by UnpackNextDeltaBlock)

    #define v_res_row01         $v00   // residual coefficients (rows 0 & 1)
    #define v_res_row23         $v01   // residual coefficients (rows 2 & 3)

    #define v_a_row0            $v08   // A (row 0, duplicated) 
    #define v_a_row1            $v09   // A (row 1, duplicated)
    #define v_a_row2            $v10   // A (row 2, duplicated)
    #define v_a_row3            $v11   // A (row 3, duplicated)
    #define v_a_row1_05mask     $v02   // A (row 1, 0.5 mask)
    #define v_a_row3_05mask     $v03   // A (row 3, 0.5 mask)
    #define v_at_row01          $v12   // A' (row 0 & 1)
    #define v_at_row23          $v13   // A' (row 2 & 3)
    #define v_at_05mask         $v14   // A' (0.5 mask)

    #define v_pix_row0          $v20   // Predicted pixels (row 0)
    #define v_pix_row1          $v21   // Predicted pixels (row 1)
    #define v_pix_row2          $v22   // Predicted pixels (row 2)
    #define v_pix_row3          $v23   // Predicted pixels (row 3)
    #define v_pix_row01         $v24   // Predicted pixels (row 0 & 1)
    #define v_pix_row23         $v25   // Predicted pixels (row 2 & 3)

    #define dst_buffer          s4
    #define dst_buffer_width    t4

    .func TransformResidual4x4IDCT
TransformResidual4x4IDCT:
    li t0,%lo(IDCT_COEEF)

    # Load IDCT coefficients (see below)
    lpv v_a_row0,0, 0*8,t0
    lpv v_a_row1,0, 1*8,t0
    lpv v_a_row2,0, 2*8,t0
    lpv v_a_row3,0, 3*8,t0
    lqv v_a_row1_05mask,0, 2*16,t0
    lqv v_a_row3_05mask,0, 3*16,t0
    lpv v_at_row01,0, 8*8,t0
    lpv v_at_row23,0, 9*8,t0
    lqv v_at_05mask,0, 5*16,t0
    li t1,%lo(V_0)

    # Load a constant vector that contains 255 to be used later for clamping.
    li t0,LUMA_FIR_COEFFS
    lqv $v15,0, 0,t0

    # We will need to add the residual to the predicted block.
    # Remember that the block is 4x4, while we have DMA'd a
    # 8x4 block. Also, our residual is kept in only 2 registers (with
    # two rows in each register), so we can't easily process 8 pixels
    # at a time
    #
    # Let's load the predicted pixels into vector registers, but
    # disalign the pixels of even rows so that we can later merge them
    # into a single register containing two rows.
    #
    # We have two possible initial layouts, depending on dst_unaling, but in 
    # both cases it's sufficient to move the pointer to the second row
    # 4 pixels backwards:
    # 
    # dst_unalign = 0:
    #    XXXX....YYYY....
    #    s4  s3
    #
    # dst_unalign = 4:
    #    ....XXXX....YYYY
    #        s4  s3
    #
    # In both cases, merging the first half of the register loaded at s4,
    # and the second half of the register loaded at s3, will produce the
    # correct vector holding two rows (XXXXYYYY).
    add s3, s4, dst_buffer_width
    addi s3, -4

    luv v_pix_row0,0, 0,s4
    luv v_pix_row1,0, 0,s3
    sll t0, dst_buffer_width, 1
    add s2, s4, t0
    add s1, s3, t0
    luv v_pix_row2,0, 0,s2
    luv v_pix_row3,0, 0,s1

    # Prepare a merge mask for merging 4 calculated pixels
    # into a vector register
    li t2, 0x0F
    ctc2 t2, COP2_CTRL_VCC

    # Merge each two consecutive 4-pixel lines into a single register
    vmrg v_pix_row01, v_pix_row0, v_pix_row1,0
    vmrg v_pix_row23, v_pix_row2, v_pix_row3,0

    # 4x4 IDCT core transformation 
    # ****************************
    #
    # Our coefficients matrix is in v00/v01 (16 values total).
    # Let's give names to the lanes: a0-a7 for the values in v00,
    # and b0-b7 for the values in v01. This is the coefficient
    # matrix:
    #
    #       ┌─                ─┐
    #       │  a0  a1  a2  a3  │
    #  Y =  │  a4  a5  a6  a7  │
    #       │  b0  b1  b2  b3  │
    #       │  b4  b5  b6  b7  │
    #       └─                ─┘
    #
    # The standard transform matrix for a 4x4 DCT (as implemented
    # e.g. by Matlab) is the following:
    #
    #       ┌─                           ─┐
    #       │   0.5    0.5    0.5    0.5  │
    #  A =  │  0.653  0.271 -0.271 -0.653 │
    #       │   0.5   -0.5    -0.5   0.5  │
    #       │  0.271 -0.653  0.653 -0.271 │
    #       └─                           ─┘
    #
    # which is used to obtain the matrix of samples X:
    #
    #    X = A' × Y × A
    #
    # (where A' is the transposed matrix).
    #
    # The H.264 standard mandates a specific integer approximation
    # because it is necessary that all encoders/decoders produce
    # the same pixels. This is the approximated matrix (which is
    # also scaled by 2 compared to the above values):
    #
    #      ┌─                       ─┐ 
    #      │  1.0   1.0   1.0   1.0  │ 
    #  A = │  1.0   0.5  -0.5  -1.0  │ 
    #      │  1.0  -1.0  -1.0   1.0  │ 
    #      │  0.5  -1.0   1.0  -0.5  │ 
    #      └─                       ─┘ 
    #
    # In our implementation, we will first implement Y x A (row transformation),
    # and then do Y' x (YxA) (column transformation).
    #
    # There is a LSB precision issue in using 0.5: the H264 standard
    # describes the operation as ">>1", which means that the LSB is
    # truncated away; if we simply multiply by 0.5, there is a risk that
    # the LSB accumulates during the fixed point operations, and affects the result.
    # To simulate a real ">>1", we need to mask out the LSB before 
    # multiplying by 0.5.
    #
    # Matrix multiplication on RSP
    # ****************************
    #
    # Given that we have two rows in each vector register,
    # we can manage to do the multiplication with two MACs
    # sequences to calculate the matrix product. 
    #
    # We use the RSP "scalar half" access to each
    # register ("Xh" in SGI syntax, "element" 4-7 in opcode),
    # so that we replicate each cell four times to multiply it
    # in parallel with each column of coefficients (which is
    # already duplicated in DMEM). For instance:
    #
    #                    ┌─                                      ─┐
    # v0[0h] = $v00,4 =  │ a0   a0   a0   a0   a4   a4   a4   a4  │
    #                    └─                                      ─┘
    #                                        x
    #                    ┌─                                      ─┐
    #          $v08   =  │ 1.0  1.0  1.0  1.0  1.0  1.0  1.0  1.0 │
    #                    └─                                      ─┘
    #
    #                                        +
    #
    #                    ┌─                                      ─┐
    # v0[1h] = $v00,5 =  │ a1   a1   a1   a1   a5   a5   a5   a5  │
    #                    └─                                      ─┘
    #                                        X
    #                    ┌─                                      ─┐
    #          $v09   =  │ 1.0  0.5 -0.5  1.0  1.0  0.5 -0.5  1.0 │
    #                    └─                                      ─┘
    #
    # This computes exactly the partial result of the first two
    # row of the resulting matrix (after multiplication with the
    # first two rows of each column):
    #
    #    ┌─                                 ─┐
    #    │ a0+a1   a0+a1*.5  a0-a1*.5  a0+a1 │
    #    │ a4+a5   a4+a5*.5  a4-a5*.5  a4+a5 │
    #    │  ...      ...       ...      ...  │
    #    │  ...      ...       ...      ...  │
    #    └─                                 ─┘
    #
    # By adding the following two MACs ($v00,6 * $v10 + $v00,7 * $v11), 
    # we complete the calculation of the first two rows of the result.
    #
    # We then repeat the same sequence on $v01 to calculate the second
    # two rows.
    #
    # For the precision, we choose to use VMULF and to encode the coefficients
    # in the range [-1, 1] in 2.14 fixed point (see IDCT_COEFF definition), and
    # thus we need to pre-shift the input coefficients by 1 to achieve the
    # correct result.

    vsll v_res_row01, v_res_row01, 1
    vand $v18, v_a_row1_05mask, v_res_row01,5   # Mask out LSB where will be multiplied by 0.5
    vand $v19, v_a_row3_05mask, v_res_row01,7   # Mask out LSB where will be multiplied by 0.5

    vmulf $v04, v_a_row0, v_res_row01,4
    vmacf $v04, v_a_row1, $v18,0
    vmacf $v04, v_a_row2, v_res_row01,6
    vmacf $v04, v_a_row3, $v19,0

    vsll v_res_row23, v_res_row23, 1
    vand $v18, v_a_row1_05mask, v_res_row23,5   # Mask out LSB where will be multiplied by 0.5
    vand $v19, v_a_row3_05mask, v_res_row23,7   # Mask out LSB where will be multiplied by 0.5

    vmulf $v06, v_a_row0, v_res_row23,4
    vmacf $v06, v_a_row1, $v18,0
    vmacf $v06, v_a_row2, v_res_row23,6
    vmacf $v06, v_a_row3, $v19,0

    # Matrix pre-multiplication on RSP
    # ********************************
    #
    # To calculate A' x (YxA), we use a somewhat similar approach. This time,
    # the transposed transform matrix A' ($v12 & $v13) will use the RSP
    # "scalar half" access, while the coefficients matrix (YxA, as resulted
    # by previous operations in $v04/$v06) needs to be duplicated horizontally:
    #
    #                     ┌─                                      ─┐
    # v12[0h] = $v12,4 =  │ 1.0  1.0  1.0  1.0  0.5  0.5  0.5  0.5 │
    #                     └─                                      ─┘
    #                                        x
    #                     ┌─                                      ─┐
    #           vXX    =  │ a0   a1   a2   a3   a0   a1   a2   a3  │
    #                     └─                                      ─┘
    #
    #  Given that we currently have:
    #
    #                     ┌─                                      ─┐
    #           v04    =  │ a0   a1   a2   a3   a4   a5   a6   a7  │
    #                     └─                                      ─┘
    #                     ┌─                                      ─┐
    #           v06    =  │ b0   b1   b2   b3   b4   b5   b6   b7  │
    #                     └─                                      ─┘
    #
    # How do we build all the 4 required vXX above, with each matrix row
    # duplicated two times into each vector? We use a simple trick: first,
    # we use a unaligned store to memory with SQV that rotates vectors to
    # create a 4-position rotated versions of v04 and v06 in v05 and v07
    # respectively:
    #
    #                     ┌─                                      ─┐
    #           v05    =  │ a4   a5   a6   a7   a0   a1   a2   a3  │
    #                     └─                                      ─┘
    #                     ┌─                                      ─┐
    #           v07    =  │ b4   b5   b6   b7   b0   b1   b2   b3  │
    #                     └─                                      ─┘
    #
    # And now, remembering that we are doing sequences of multiplication and
    # adds vertically on each lane, we don't need to exactly build vXX: if you
    # look at v04-v07, each column contains exactly the required terms, though
    # not in the original order; it is thus sufficient to swap the terms of A'
    # accordingly, so that each term gets multiplied to the right coefficient.
    #

    # Create rotated vectors
    sqv $v04,8, 0*16,t1
    sqv $v06,8, 1*16,t1
    lqv $v05,0, 0*16,t1
    lqv $v07,0, 1*16,t1

    # In addition to IDCT, we also take the opportunity of this MAC
    # sequence to add the predicted pixels (destination buffer) to do the
    # final blending. We need to do this in the middle
    # of the MAC sequence (rather than after it) because we
    # want the last instruction to be a VMACU, the only one
    # in the RSP instruction set that clamps to 0, so that
    # we get that clamping step for free.
    # Obviously we need to shift the pixel data to the required
    # precision: since the pixel data was loaded with LUV as <<7,
    # we need to shift it back >>7; we can't use the VSRL macro-opcode
    # because it generates a VMUDM and we need a VMADM to be part
    # of the MAC sequence; so we just call VMADM manually.

    vand $v18, $v04, v_at_05mask,6    # Mask out LSB on second half (will be * 0.5)
    vand $v19, $v07, v_at_05mask,5    # Mask out LSB on first half (will be * 0.5)

    vmulf v_res_row01, $v18, v_at_row01,4
    vmacf v_res_row01, $v05, v_at_row01,5
    vmacf v_res_row01, $v06, v_at_row01,6
    vmadm v_res_row01, v_pix_row01, vshift8,14   # Add predicted pixel data (after >>7)
    vmacu v_res_row01, $v19, v_at_row01,7     # Complete MAC & clamp to 0

    vand $v18, $v05, v_at_05mask,5    # Mask out LSB on second half (will be * 0.5)
    vand $v19, $v06, v_at_05mask,6    # Mask out LSB on first half (will be * 0.5)

    vmulf v_res_row23, $v04, v_at_row23,4
    vmacf v_res_row23, $v18, v_at_row23,5
    vmacf v_res_row23, $v19, v_at_row23,6
    vmadm v_res_row23, v_pix_row23, vshift8,14   # Add predicted pixel data (after >>7)
    vmacu v_res_row23, $v07, v_at_row23,7     # Complete MAC & clamp to 0

    # Clamp pixel values to 255
    # NOTE: we use vcl instead of vch because it doesn't change VCC (that we need for vmrg).
    # vcl works almost by "chance", because VCO has been cleared by a previous vmrg.
    # If it stops working, it's better to switch to vch, and reload VCC before the next vmrg.
    vcl v_res_row01, v_res_row01, $v15,15
    vcl v_res_row23, v_res_row23, $v15,15

    # Shift them back with <<7
    vsll v_res_row01, v_res_row01, 7
    vsll v_res_row23, v_res_row23, 7

    # Merge the calculated values into the original register,
    # preserving pixels that are not within our 4x4 area.
    vmrg v_pix_row0, v_res_row01, v_pix_row0,0
    vmrg v_pix_row1, v_pix_row1,  v_res_row01,0
    vmrg v_pix_row2, v_res_row23, v_pix_row2,0
    vmrg v_pix_row3, v_pix_row3,  v_res_row23,0

    # Write back
    suv v_pix_row2,0, 0,s2
    suv v_pix_row3,0, 0,s1
    suv v_pix_row0,0, 0,s4
    jr ra
    suv v_pix_row1,0, 0,s3
    .endfunc

    #undef v_res_row01      
    #undef v_res_row23      
    #undef v_a_row0         
    #undef v_a_row1         
    #undef v_a_row2         
    #undef v_a_row3         
    #undef v_a_row1_05mask  
    #undef v_a_row3_05mask  
    #undef v_at_row01       
    #undef v_at_row23       
    #undef v_at_05mask      
    #undef v_pix_row0       
    #undef v_pix_row1       
    #undef v_pix_row2       
    #undef v_pix_row3       
    #undef v_pix_row01      
    #undef v_pix_row23      


####################################################################
# TransformDequantLumaDC
#
# Reconstruct a 4x4 Luma DC block from a packed block of coefficients. 
# Do a 4x4 inverse Hadamard transformation, followed by a quantization
# step.
#
# This is exposed as OMX_TransformDequantLumaDC.
####################################################################
# ARGS:
#   $v00/$v01: 4x4 coefficient matrix
#   t1: qp scale factor
#   FIXME: used to be gp
#   s4: address of destination buffer in DMEM
#

    .func TransformDequantLumaDC
TransformDequantLumaDC:
    # Apply a IHT 4X4 transformation. This is quite similar to 
    # TransformResidual4x4IDCT, but there are a few differences:
    # first, the coefficient matrix is different. Second, the matrix
    # does not contain 0.5 / -0.5 values which require special care
    # in TransformResidual4x4IDCT (see the various vand opcodes).

    # So we don't try to reuse the code for now, but just redo the
    # double matrix multiplication.
    vsll $v00, $v00, 1
    vsll $v01, $v01, 1

    # Load coefficients
    li t0, %lo(IHT_DC_COEFF)
    lpv $v08,0, 0*8,t0
    lpv $v09,0, 2*8,t0
    lpv $v10,0, 3*8,t0
    lpv $v11,0, 1*8,t0

    lpv $v12,0, 4*8,t0
    lpv $v13,0, 5*8,t0

    vmulf $v02, $v08, $v00,4
    vmacf $v02, $v09, $v00,5
    vmacf $v02, $v10, $v00,6
    vmacf $v02, $v11, $v00,7

    vmulf $v04, $v08, $v01,4
    vmacf $v04, $v09, $v01,5
    vmacf $v04, $v10, $v01,6
    vmacf $v04, $v11, $v01,7

    vsll $v02, $v02, 1
    vsll $v04, $v04, 1

    li t0, %lo(V_0)
    sqv $v02,8, 0*16,t0
    sqv $v04,8, 1*16,t0
    lqv $v03,0, 0*16,t0
    lqv $v05,0, 1*16,t0

    vmulf $v00, $v02, $v12,4
    vmacf $v00, $v03, $v12,5
    vmacf $v00, $v04, $v12,6
    vmacf $v00, $v05, $v12,7

    vmulf $v01, $v02, $v13,4
    vmacf $v01, $v03, $v13,5
    vmacf $v01, $v04, $v13,6
    vmacf $v01, $v05, $v13,7

    # Dequantize using only first multiplication factor ($v27[e8]) calculated
    # by CalcDequantFactors. 
    vmudn $v00, $v00, $v27,8
    vmudn $v01, $v01, $v27,8

    # Now we need to apply scale. The scale factor in QP is used after being
    # subtracted by 2, so for instance QP=5 -> <<3; QP=1 -> <<-1 -> >>1.
    # In case of right shift, a rounding is also necessary.
    # So we can't just use the scale factor calculated by CalcDequantFactors.
    srl t0, t1, 8
    beqz t0, LumaDCScaleDown2    # >>2
    addi t0, -1
    beqz t0, LumaDCScaleDown1    # >>1
    nop

    # Shift the dequant scale factor itself by 2, so that we can then apply
    # to our coefficients.
    vsrl $v06, $v29, 2
    vmudn $v00, $v00, $v06,8
    j LumaDCEnd
    vmudn $v01, $v01, $v06,8

LumaDCScaleDown1:
    # Right-shift by 1, after adding 1 (from shift register vshift[e15])
    vaddc $v00, $v00, vshift,15
    vaddc $v01, $v01, vshift,15
    vsra $v00, $v00, 1
    j LumaDCEnd
    vsra $v01, $v01, 1

LumaDCScaleDown2:
    # Right-shift by 2, after adding 2 (from shift register vshift[e14])
    vaddc $v00, $v00, vshift,14
    vaddc $v01, $v01, vshift,14
    vsra $v00, $v00, 2
    vsra $v01, $v01, 2

LumaDCEnd:
    sqv $v00,0, 0*16,s4
    jr ra
    sqv $v01,0, 1*16,s4

    .endfunc

#if 0
TEXT_OVERLAY_START:
#endif
