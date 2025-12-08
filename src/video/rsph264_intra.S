############################################################
# RSPH264 - Intra-prediction functions
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
############################################################
#include <rsp_queue.inc>

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand OMX_IntraPredictLuma4x4,              32
        RSPQ_DefineCommand OMX_IntraPredictLuma16x16,            32
        RSPQ_DefineCommand OMX_IntraPredictChroma8x8,            32
        # RSPQ_DefineCommand Task_ProcessLumaIntra4x4Residual,     40
        # RSPQ_DefineCommand Task_ProcessLumaIntra16x16Residual,   32
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand RSPQCmd_Noop,                        4
        RSPQ_DefineCommand Task_SetPackedDeltaBuffer,           4
    RSPQ_EndOverlayHeader

#if 0
    # NOTE: this must be at the start of the .data overlay section
    # so that it concatenates to the main table.
DECLARE_OMXDL_TABLE(
    OMX_IntraPredictLuma4x4 - _start,
    OMX_IntraPredictLuma16x16 - _start,
    OMX_IntraPredictChroma8x8 - _start,
    Task_ProcessLumaIntra4x4Residual - _start,
    Task_ProcessLumaIntra16x16Residual - _start
)
#endif

#include "rsph264_common.inc"

    .align 4
    .ascii "OVL: Intra-pred "

INTRALUMA4_TABLE:
                 .half IL4X4_VER - _start
                 .half IL4X4_HOR - _start
                 .half IL4X4_DC - _start
                 .half IL4X4_DIAG_DL - _start
                 .half IL4X4_DIAG_DR_VR_HD - _start
                 .half IL4X4_DIAG_DR_VR_HD - _start
                 .half IL4X4_DIAG_DR_VR_HD - _start
                 .half IL4X4_VL - _start
                 .half IL4X4_HU - _start

INTRALUMA16_TABLE:
                .half IL16x16_VER - _start
                .half IL16x16_HOR - _start
                .half IL16x16_DC - _start
                .half IL16x16_PLANE - _start

INTRACHROMA8_TABLE:
                .half IC8x8_DC - _start
                .half IC8x8_HOR - _start
                .half IC8x8_VER - _start
                .half IC8x8_PLANE - _start

    .align 4
INTRALUMA4_COEFF:
    .half 1<<9  # Used by IL4X4_DC / IC8X8_DC (note: it MUST stay in the first lane!)
    .half 1<<6  # Used by IL4X4_DIAG_*, for 3-tap filter
    .half 2<<6  # Used by IL4X4_DIAG_*, for 3-tap filter
    .half 1<<7  # Used by IL4X4_FINISH_VR/HD, for 2-tap filter
    .half 0
    .half 128
    .half 2<<7  # Rounding, used by IL4X4_DC
    .half 0     # Used to zero accumulator by IL4X4_DC

INTRALUMA16_COEFF1:   # Used by IL16x16_PLANE
    .half 1
    .half 2
    .half 3
    .half 4
    .half 5
    .half 6
    .half 7
    .half 8

    #define ILSH  10
INTRALUMA16_COEFF2:   # Used by IL16x16_PLANE
    .half (-7<<ILSH)
    .half (-6<<ILSH)
    .half (-5<<ILSH)
    .half (-4<<ILSH)
    .half (-3<<ILSH)
    .half (-2<<ILSH)
    .half (-1<<ILSH)
    .half (0<<ILSH)

    .half (1<<ILSH)
    .half (2<<ILSH)
    .half (3<<ILSH)
    .half (4<<ILSH)
    .half (5<<ILSH)
    .half (6<<ILSH)
    .half (7<<ILSH)
    .half (8<<ILSH)
INTRALUMA16_COEFF3:   # Used by IL16x16_PLANE
    .half 255
    .half 5<<9
    .half -7
    .half 1<<ILSH
    .half 0
    .half 0
    .half 0
    .half 0
    #undef ILSH

    .align 4
    #define ILSH  10
INTRACHROMA_COEFF1:   # Used by IC8x8_PLANE
    .half 4
    .half 3
    .half 2
    .half 1
    .half 1
    .half 2
    .half 3
    .half 4
INTRACHROMA_COEFF2:   # Used by IC8x8_PLANE
    .half (-3<<ILSH)
    .half (-2<<ILSH)
    .half (-1<<ILSH)
    .half (0<<ILSH)
    .half (1<<ILSH)
    .half (2<<ILSH)
    .half (3<<ILSH)
    .half (4<<ILSH)
INTRACHROMA_COEFF3:   # Used by IC8x8_PLANE
    .half 255
    .half 17<<10
    .half -3
    .half 1<<ILSH
    .half 0
    .half 0
    .half 0
    .half 0
    #undef ILSH

INTRADC_UPPER_MASK: .byte 0, 1, 2, 1
INTRADC_LEFT_MASK:  .byte 0, 1, 2, 2

    #define INTRA4RES_MBINPUT_WIDTH 40
    #define INTRA4RES_INITIAL_OFFSET (8 + INTRA4RES_MBINPUT_WIDTH)
    #define IOFF(x,y) ((x) + (y)*INTRA4RES_MBINPUT_WIDTH + INTRA4RES_INITIAL_OFFSET)
INTRA4RES_OFFSET:
    .half IOFF(0,0), IOFF(4,0), IOFF(0,4), IOFF(4,4)
    .half IOFF(8,0), IOFF(12,0), IOFF(8,4), IOFF(12,4)
    .half IOFF(0,8), IOFF(4,8), IOFF(0,12), IOFF(4,12)
    .half IOFF(8,8), IOFF(12,8), IOFF(8,12), IOFF(12,12)
    #undef IOFF

    # Table to quickly convert ProcessLumaIntra4x4Residual 
    # availability bits (bit 0, 1, 2, 3) into OMX
    # availability bits (bit 0, 1, 5, 6).
INTRA4RES_AVAILABILITY:
    .byte (0<<5)+0, (0<<5)+1, (0<<5)+2, (0<<5)+3
    .byte (1<<5)+0, (1<<5)+1, (1<<5)+2, (1<<5)+3
    .byte (2<<5)+0, (2<<5)+1, (2<<5)+2, (2<<5)+3
    .byte (3<<5)+0, (3<<5)+1, (3<<5)+2, (3<<5)+3

OMXDL_ARGS: # FIXME *************************
    .bss

    # Macroblock input buffer. A macroblock is max 16x16,
    # but we need to read more lines/pixels because of
    # over-reading by interpolation and because of DMA
    # alignment constraints, so space for 32x21 pixels
    # is required.
    .align 8   # 4 is sufficient, but 8 visually helps debugging
MB_INPUT:   .dcb.b 40*17  # MAX(40*17,32*21)

    # Maximum macroblock output. We fill this in without
    # over-reading (and RDRAM buffer is always aligned),
    # so we don't need more than 16x16
    .align 8   # 4 is sufficient, but 8 visually helps debugging
MB_OUTPUT:  .dcb.b 16*16

    # Scratch memory for intermediate computations
    .align 8   # 4 is sufficient, but 8 visually helps debugging
MB_SCRATCH: .dcb.b 32*21


    .text

####################################################################
#
#  INTRA PREDICTION: CHROMA & LUMA
#
####################################################################

    # Registers shared by all intra-prediction
    #define srcbuf_l  a0
    #define srcbuf_u  a1 
    #define srcbuf_ul a2
    #define dstbuf    a3

    #define availability   fp
    #define src_dma_width  k0
    #define dst_dma_width  k1
    
    #define v_zero  $v25
    #define v_coeff $v26

    .equ OMX_VC_UPPER, 1      
    .equ OMX_VC_LEFT, 2       
    .equ OMX_VC_CENTER, 4
    .equ OMX_VC_RIGHT, 8
    .equ OMX_VC_LOWER, 16
    .equ OMX_VC_UPPER_LEFT, 32
    .equ OMX_VC_UPPER_RIGHT, 64
    .equ OMX_VC_LOWER_LEFT, 128
    .equ OMX_VC_LOWER_RIGHT, 256

    .func IntraMB16InOut
IntraMB16InOut:
    j IntraMBInOutInner
    li t0, DMA_SIZE(16,16)
    .endfunc

    .func IntraMBInOut
IntraMBInOut:
    li t0, DMA_SIZE(8,8)
IntraMBInOutInner:
    move t1, dst_dma_width
    move s0, dstbuf
    j DMAExec
    li s4, %lo(MB_OUTPUT)
    .endfunc

    .func IntraSrcIn
IntraSrcIn:
    li s4, %lo(MB_SCRATCH)
    j DMAIn
    move t1, src_dma_width   # this is only required for L
    .endfunc

##########################################################

    # Registers shared by Luma4x4 and Chroma8x8

    #  $v08: UL U0 U1 U2 U3 U4 U5 U6
    #  $v09: U0 U1 U2 U3 U4 U5 U6 U7
    #  $v10: L0 xx xx xx xx xx xx xx
    #  $v11: L1 xx xx xx xx xx xx xx
    #  $v12: L2 xx xx xx xx xx xx xx
    #  $v13: L3 xx xx xx xx xx xx xx
    #define v_ul_u  $v08
    #define v_u     $v09
    #define v_l0    $v10
    #define v_l1    $v11
    #define v_l2    $v12
    #define v_l3    $v13

#########################################################
# IntraDC4x4: Calculate Intra DC prediction of a 4x4 block.
#
# This is used by both Luma4x4 and Chroma8x8.
#
#########################################################
#
# Input:
#    v_u: upper 4 pixels U0-U3 (in lanes 0-3)
#    v_l0: left pixel L0 (lane 0) 
#    v_l1: left pixel L1 (lane 0) 
#    v_l2: left pixel L2 (lane 0) 
#    v_l3: left pixel L3 (lane 0) 
#
# Output:
#    $v00: predicted value (lane 0)
#
# Requires v_coeff to be loaded with INTRALUMA4_COEFF
#
    .func IntraDC4x4
IntraDC4x4:
    # Calculate average of L0-L4/U0-U4, but only
    # available pixels. This function works even if
    # no pixels are available (think top-left macroblock),
    # in which case a fixed value is returned (128).

    # Clear accumulator
    vmudm $v00, v_zero, v_zero,0

    andi t0, availability, OMX_VC_LEFT
    beqz t0, IDCTestUpper
    li t1, 2

    # Calculate L0+L1+L2+L3+2, result into $v00[e8]
    # v_coeff[e8] contains (1<<9), so that multiplying by LN<<7 we get
    # LN<<16. VMADM does >>16, so the end result is accumulating the sum
    # into $v00, fully unshifted.
    # Notice that we care only about the first lane ([e8]), and we ignore 
    # the other lanes.
    vmadm $v00, v_coeff, v_l0,0
    vmadm $v00, v_coeff, v_l1,0
    vmadm $v00, v_coeff, v_l2,0
    vmadm $v00, v_coeff, v_l3,0
    vmadm $v00, v_coeff, v_coeff,14  # +2
    addi t1, -1

IDCTestUpper:
    andi t0, availability, OMX_VC_UPPER
    beqz t0, IDCCheck
    nop

    # Calculate U0+U1+U2+U3+2, result into $v00[e8]
    vmadm $v00, v_coeff, v_u,8
    vmadm $v00, v_coeff, v_u,9
    vmadm $v00, v_coeff, v_u,10
    vmadm $v00, v_coeff, v_u,11
    vmadm $v00, v_coeff, v_coeff,14  # +2
    addi t1, -1

IDCCheck:
    # To calculate the average, we should divide by either 4 (>>2) or 8 (>>3),
    # depending on the number of pixels we added. After that, we want to shift
    # back the pixel into the higher part (<<7) to be used by LUV/SUV. So in
    # total, we should either <<5 or <<4.
    beqz t1, IDCFinal
    vsll $v00, $v00,4   # <<4
    addi t1, -1
    beqz t1, IDCFinal
    vsll $v00, $v00,1   # <<4 <<1 = <<5

    # Zero elements were added: use 128 as final prediction
    li t2, 128<<7
    mtc2 t2, $v00,0

IDCFinal:
    jr ra
    nop
    .endfunc


####################################################################
# OMX_IntraPredictLuma4x4
#
# Intra-prediction of a 4x4 luma macroblock partition.
####################################################################
# ARGS:
#   0: srcbuf_l: pointer to first pixel in left column (-1,0)
#   1: srcbuf_u: pointer to first pixel in top column (0,-1)
#   2: srcbuf_ul: pointer to pixel in top-left corner (-1,-1)
#   3: dstbuf: pointer to destination buffer
#   4: src_dma_width: pitch of the source buffers
#   5: dst_dma_width: pitch of the destination buffers
#   6: mode: prediction mode
#   7: availability: bitmask of availability of neighbor macroblocks

    .func OMX_IntraPredictLuma4x4
OMX_IntraPredictLuma4x4:    
    lw src_dma_width, CMD_ADDR(16, 32)
    lw dst_dma_width, CMD_ADDR(20, 32)
    lw availability, CMD_ADDR(28, 32)

    # Read UL into $v00.
    # We only care about the first pixel.
    move s0, srcbuf_ul
    jal IntraSrcIn
    li t0, DMA_SIZE(8,1)
    luv $v00,0, 0,s4

    # Read U. This is a sequence of 8 bytes, so request
    # 16 bytes because of unalignment.
    move s0, srcbuf_u
    jal IntraSrcIn
    li t0, DMA_SIZE(16,1)
    luv $v01,0, 0,s4

    # Now read L. We need a 8x4 DMA transfer
    move s0, srcbuf_l
    jal IntraSrcIn
    li t0, DMA_SIZE(8,4)

    # Prepare arguments for IL4x4 call. S3 must point to the left buffer,
    # and t3 must be the pitch of the buffer.
    move s3, s4
    li t3, 8

    # Copy 8x8 macroblock partition into DMEM (MB_OUTPUT).
    jal IntraMBInOut
    li t2, DMA_IN   # direction: RDRAM -> DMEM

    # Run intra-prediction algorithm
    lw t2, CMD_ADDR(24, 32)    # mode
    jal IL4X4
    li t4, 8   # destination pitch

IntraLuma4Finish:
    # Copy 8x8 macroblock partition back into RDRAM
    li t2, DMA_OUT   # direction: DMEM -> RDRAM
    jal_and_j IntraMBInOut, RSPQ_Loop
    .endfunc

####################################################################
# IL4X4
#
# Intra-prediction of Luma 4x4 block: inner function.
####################################################################
# ARGS:
#  $v00: upper-left pixel (only first lane will be used)
#  $v01: top 8 pixels
#  t2: mode
#  s3: left buffer
#  t3: pitch of the left buffer
#  s4: destination buffer
#  t4: pitch of the destination buffer
#  fp (availability macro: neighbor availability bitmask
#
# OUTPUT:
#  $v00-$v03: predicted pixels (first 4 lanes in each vector)
#  Predicted pixels are also stored at s4
#
    .func IL4X4
IL4X4:
    # Set v_zero to 0
    vxor v_zero, v_zero, v_zero,0

    # Load L0-L4
    luv v_l0,0, 0,s3
    add s3, t3
    luv v_l1,0, 0,s3
    add s3, t3
    luv v_l2,0, 0,s3
    add s3, t3
    luv v_l3,0, 0,s3

    # Create a mask to preserve the other 4 pixels in each row.
    li t0, 0xF0
    ctc2 t0, COP2_CTRL_VCC

    # Prepare v_u. If upper right MB doesn't exist, overfill last valid pixel
    andi t0, availability, OMX_VC_UPPER_RIGHT
    bnez t0, NoOverfill
    vor v_u, v_zero, $v01,0

    vor $v01, v_zero, v_u,11
    vmrg v_u, $v01, v_u,0

NoOverfill:
    # Store (potentially overfilled) v_u into temporary buffer V_0.
    li t0, %lo(V_0)
    suv v_u,0, 0,t0

    # Load v_ul_u, also recovering the first pixel that was read before.
    addi t0, -1
    luv v_ul_u,0, 0,t0
    vmov v_ul_u,0, $v00,0

    # S4 is the buffer where pixels must be predicted (MB_OUPUT).
    # Point S0 to the scratch buffer, that can be used by prediction functions 
    li s0, %lo(MB_SCRATCH)

    # Jump to the function implementing the requested type of prediction
    sll t0, t2, 1
    lhu t0, %lo(INTRALUMA4_TABLE)(t0)
    li t1, %lo(INTRALUMA4_COEFF)
    lqv v_coeff,0, 0,t1
    .endfunc

    .func IL4X4_VER
IL4X4_VER:
    # Vertical intra-prediction:
    # Fill output pixels with U0-U4 repeated in each row
    vor $v00, v_zero, v_u,0
    vor $v01, v_zero, v_u,0
    vor $v02, v_zero, v_u,0
    j IL4X4_FINISH
    vor $v03, v_zero, v_u,0
    .endfunc

    .func IL4X4_HOR
IL4X4_HOR:
    # Horizontal intra-prediction:
    # Fill output pixels with L0-L4 repeated in each column
    vor $v00, v_zero, v_l0,8
    vor $v01, v_zero, v_l1,8
    vor $v02, v_zero, v_l2,8
    j IL4X4_FINISH
    vor $v03, v_zero, v_l3,8
    .endfunc

    .func IL4X4_DC
IL4X4_DC:
    move sp, ra
    jal IntraDC4x4
    nop
    move ra, sp
    vor $v00, v_zero, $v00,8
    vor $v01, v_zero, $v00,8
    vor $v02, v_zero, $v00,8
    j IL4X4_FINISH
    vor $v03, v_zero, $v00,8
    .endfunc
   

    .func IL4X4_DIAG_DL
IL4X4_DIAG_DL:
    suv v_u,0, 0,s0
    luv $v01,15, 0,s0
    luv $v02,14, 0,s0

    # We now have:
    #  V_U:   U0 U1 U2 U3 U4 U5 U6 U7
    # $v01:   U1 U2 U3 U4 U5 U6 U7 U0
    # $v02:   U2 U3 U4 U5 U6 U7 U0 U1
    #
    # We don't care to adjust the last lane, but we need to replicate U7 in the 6th 
    # lane of $v02 becuase the filter behaves like with overfilling, and we need
    # the 6th lane to hold the correct result
    vmov $v02,14, $v02,13

    # Calculate 3-tap filter and shift pixel result <<7 (into higher part, aligned
    # for LUV/SUV).
    vmulf $v00, v_u, v_coeff,9
    vmacf $v00, $v01, v_coeff,10
    vmacf $v00, $v02, v_coeff,9
    vsll $v00, $v00, 7

    # Rotate lines as expected for output (in diagonal)
    suv $v00,0, 0,s0
    luv $v01,15, 0,s0
    luv $v02,14, 0,s0
    j IL4X4_FINISH
    luv $v03,13, 0,s0
    .endfunc


    .func IL4X4_DIAG_DR_VR_HD
IL4X4_DIAG_DR_VR_HD:
    vor $v04, v_zero, v_ul_u,0
    vmov $v04,4, v_l3,8
    vmov $v04,5, v_l2,8
    vmov $v04,6, v_l1,8
    vmov $v04,7, v_l0,8
    suv $v04,0, 0*8,s0
    suv $v04,0, 1*8,s0
    luv $v00,2, 0*8,s0
    luv $v01,3, 0*8,s0
    luv $v02,4, 0*8,s0

    # We now have:
    #    $v00:   L1 L0 UL U0 U1 U2 L3 L2
    #    $v01:   L2 L1 L0 UL U0 U1 U2 L3
    #    $v02:   L3 L2 L1 L0 UL U0 U1 U2
    #
    # Correct $v00 6th-lane, putting U3 instead of L3. We don't care about lane 7.
    vmov $v00,14, v_ul_u,12

    # Calculate 3-tap filter and shift pixel result <<7 (into higher part, aligned
    # for LUV/SUV).
    vmulf $v04, $v00, v_coeff,9
    vmacf $v04, $v01, v_coeff,10
    beq t2, 6, IL4X4_FINISH_HD
    vmacf $v04, $v02, v_coeff,9
    vsll $v04, $v04, 7

    suv $v04,0, 0*8,s0
    beq t2, 5, IL4X4_FINISH_VR
    suv $v04,0, 1*8,s0

    # Rotate lines as expected for output (in diagonal)
    luv $v00,5, 0,s0
    luv $v01,6, 0,s0
    luv $v02,7, 0,s0
    j IL4X4_FINISH
    luv $v03,8, 0,s0

IL4X4_FINISH_VR:
    luv $v01,5, 0,s0
    luv $v03,6, 0,s0
    vmov $v03,8, $v04,9

    vmulf $v00, v_ul_u, v_coeff,11
    vmacf $v00, v_u, v_coeff,11
    vsll $v00, $v00, 7

    suv $v00,0, 0,s0
    luv $v02,1, 0,s0
    j IL4X4_FINISH
    vmov $v02,8, $v04,10
    
IL4X4_FINISH_HD:
    # $v04 contains the (unshifted) result of the 3-tap filter, so the pixel
    # value is in the LSB.
    vmov $v05,8, v_l3,8
    vmov $v05,9, v_l2,8
    vmov $v05,10, v_l1,8
    vmov $v05,11, v_l0,8
    vmov $v05,12, v_ul_u,8
    suv $v05,0, 0,s0
    luv $v06,15, 0,s0

    # $v05 = L3 L2 L1 L0 UL
    # $v06 = L2 L1 L0 UL
    # $v07 = 2-tap filters of the above
    vmulf $v07, $v05, v_coeff,11
    vmacf $v07, $v06, v_coeff,11
    vsll8 $v07, $v07, 8

    # Interleave $v04 and $v07 and write them to scratch buffer
    vor $v07, $v04, $v07,0
    sqv $v07,0, 0,s0

    luv $v03,0, 0,s0
    luv $v02,14, 0,s0
    luv $v01,12, 0,s0
    luv $v00,10, 0,s0

    # Adjust last two pixels of v00 which are different
    vsll $v04, $v04, 7
    vmov $v00,10, $v04,12
    j IL4X4_FINISH
    vmov $v00,11, $v04,13
    .endfunc

    .func IL4X4_VL
IL4X4_VL:
    suv v_u,0, 0,s0
    luv $v04,15, 0,s0
    luv $v05,14, 0,s0
    vmulf $v00, v_u, v_coeff,11
    vmacf $v00, $v04, v_coeff,11

    vmulf $v01, v_u, v_coeff,9
    vmacf $v01, $v04, v_coeff,10
    vmacf $v01, $v05, v_coeff,9

    vsll $v00, $v00, 7
    vsll $v01, $v01, 7

    suv $v00,0, 0,s0
    luv $v02,15, 0,s0

    suv $v01,0, 0,s0
    j IL4X4_FINISH
    luv $v03,15, 0,s0
    .endfunc

    .func IL4X4_HU
IL4X4_HU:
    # Create:
    #   $v04 = L0 L1 L2 L3 L3 L3 L3 L3
    vmov $v04,8, v_l3,8
    vor $v04, v_zero, $v04,8
    vmov $v04,8, v_l0,8
    vmov $v04,9, v_l1,8
    vmov $v04,10, v_l2,8

    #   $v04 = L0 L1 L2 L3 L3 L3 L3 L3
    #   $v05 = L1 L2 L3 L3 L3 L3 L3 L0
    #   $v06 = L2 L3 L3 L3 L3 L3 L0 L1
    # (obviously, we only care about the first 4 lanes)
    suv $v04,0, 0,s0
    luv $v05,15, 0,s0
    luv $v06,14, 0,s0

    # 2-tap filter
    vmulf $v07, $v04, v_coeff,11
    vmacf $v07, $v05, v_coeff,11

    # 3-tap filter
    vmulf $v04, $v04, v_coeff,9
    vmacf $v04, $v05, v_coeff,10
    vmacf $v04, $v06, v_coeff,9
    
    # Interleave 2-tap and 3-tap
    vsll8 $v07, $v07, 8
    vor $v07, $v07, $v04,0

    # Compose destination registers as expected
    sqv $v07,0, 0,s0
    luv $v00,0, 0,s0
    luv $v01,14, 0,s0
    luv $v02,12, 0,s0
    j IL4X4_FINISH
    luv $v03,10, 0,s0
    .endfunc

    .func IL4X4_FINISH
IL4X4_FINISH:
    # We have now calculated the 4x4 block into the first
    # 4 lanes of $v00-$v03. We need to put it into the 8x8
    # block at s4, preserving the other pixels. Use VMRG,
    # considering that the mask has been already calculated.

    # Calculate beginning of each line using pitch in t4
    add s3, s4, t4
    add s2, s3, t4
    add s1, s2, t4

    luv $v16,0, 0,s4
    luv $v17,0, 0,s3
    luv $v18,0, 0,s2
    luv $v19,0, 0,s1

    vmrg $v16, $v16, $v00,0
    vmrg $v17, $v17, $v01,0
    vmrg $v18, $v18, $v02,0
    vmrg $v19, $v19, $v03,0

    suv $v16,0, 0,s4
    suv $v17,0, 0,s3
    suv $v18,0, 0,s2
    jr ra
    suv $v19,0, 0,s1
    .endfunc


####################################################################
# OMX_IntraPredictChroma8x8
#
# Intra-prediction of a 8x8 chroma macroblock
####################################################################
# ARGS:
#   0: srcbuf_l: pointer to first pixel in left column (-1,0)
#   1: srcbuf_u: pointer to first pixel in top column (0,-1)
#   2: srcbuf_ul: pointer to pixel in top-left corner (-1,-1)
#   3: dstbuf: pointer to destination buffer
#   4: src_dma_width: pitch of the source buffers
#   5: dst_dma_width: pitch of the destination buffers
#   6: mode: prediction mode
#   7: availability: bitmask of availability of neighbor macroblocks

    .func OMX_IntraPredictChroma8x8
OMX_IntraPredictChroma8x8:
    lw src_dma_width, CMD_ADDR(16, 32)
    lw dst_dma_width, CMD_ADDR(20, 32)
    lw availability, CMD_ADDR(28, 32)

    # $v14: U4 U5 U6 U7 -- -- -- -- 
    #define v_u4    $v14
    #define dmem_l  s3

    vxor v_zero, v_zero, v_zero,0

    # Load UL byte into $v00, will be used later to compose v_ul_u. We load
    # 8 pixels but we only care about first one.
    move s0, srcbuf_ul
    jal IntraSrcIn
    li t0, DMA_SIZE(8,1)
    luv $v00,0, 0,s4

    # Read U. This is a sequence of 8 ALIGNED bytes
    move s0, srcbuf_u
    jal IntraSrcIn
    li t0, DMA_SIZE(8,1)
    luv v_u,0, 0,s4
    addi s4, 4
    luv v_u4,0, 0,s4
    addi s4, -5
    luv v_ul_u,0, 0,s4
    vmov v_ul_u,8, $v00,8

    # Read L. This will be a 8x8 block where the first column
    # is the L vector
    move s0, srcbuf_l
    jal IntraSrcIn
    li t0, DMA_SIZE(8,8)
    move dmem_l, s4

    # Dispatch
    lw t0, %lo(OMXDL_ARGS+24)
    sll t2, t0, 1
    lhu t2, %lo(INTRACHROMA8_TABLE)(t2)
    jr t2
    li s4, %lo(MB_OUTPUT)


IntraChroma8End:
    # Copy 8x8 macroblock back into RDRAM
    li t2, DMA_OUT   # direction: DMEM -> RDRAM
    jal_and_j IntraMBInOut, RSPQ_Loop
    .endfunc

    .func IC8x8_DC
IC8x8_DC:
    # Load coefficients needed by IntraDC4x4
    li t0, %lo(INTRALUMA4_COEFF)
    lqv v_coeff,0, 0,t0

    li t0, 0x0F
    ctc2 t0, COP2_CTRL_VCC

    andi t8, availability, 3   # Keep only OMX_VC_UPPER | OMX_VC_LEFT
    vor $v04, v_zero, v_u,0    # Save v_u into $v04

    # Load first 4 left pixels, as we're going to process
    # the first two 4x4 upper blocks first
    luv v_l0,0, 0*8,dmem_l
    luv v_l1,0, 1*8,dmem_l
    luv v_l2,0, 2*8,dmem_l
    luv v_l3,0, 3*8,dmem_l

    # Call IntraDC on upper-left 4x4 block. Save result into $v01
    jal IntraDC4x4
    nop
    vor $v01, v_zero, $v00,8

    # Call IntraDC on upper-right 4x4 block. We need to use
    # v_u4 instead of v_u, and we must change the availability mask
    # to only use the upper pixels if available
    lbu availability, %lo(INTRADC_UPPER_MASK)(t8)
    jal IntraDC4x4
    vor v_u, v_zero, v_u4,0

    # Merge upper-left and upper-right result into $v01
    vmrg $v01, $v01, $v00,8

    # Now we're moving to lower 4x4 blocks. Load the last 4 left
    # pixels.
    luv v_l0,0, 4*8,dmem_l
    luv v_l1,0, 5*8,dmem_l
    luv v_l2,0, 6*8,dmem_l
    luv v_l3,0, 7*8,dmem_l

    # Call IntraDC on lower-left 4x4 block. Restore original
    # v_u, and change the availability mask to only use the left
    # pixels if available. Save result into $v02.
    lbu availability, %lo(INTRADC_LEFT_MASK)(t8)
    jal IntraDC4x4
    vor v_u, v_zero, $v04,0
    vor $v02, v_zero, $v00,8

    # Call IntraDC on lower-right 4x4 block. Restore original
    # availability, and use v_u4.
    move availability, t8
    jal IntraDC4x4
    vor v_u, v_zero, v_u4,0

    # Merge lower-left and lower-right result into $v02
    vmrg $v02, $v02, $v00,8

    # Store the result
    suv $v01,0, 0*8,s4
    suv $v01,0, 1*8,s4
    suv $v01,0, 2*8,s4
    suv $v01,0, 3*8,s4
    suv $v02,0, 4*8,s4
    suv $v02,0, 5*8,s4
    suv $v02,0, 6*8,s4
    j IntraChroma8End
    suv $v02,0, 7*8,s4
    .endfunc

    .func IC8x8_HOR
IC8x8_HOR:
    addi t0, s4, 64-16
IC8x8_HOR_StoreLoop:
    lpv $v00,0, 0*8,dmem_l
    vor $v00, v_zero, $v00,8
    lpv $v01,0, 1*8,dmem_l
    vor $v01, v_zero, $v01,8
    addi dmem_l, 16
    spv $v00,0, 0*8,s4
    spv $v01,0, 1*8,s4
    bne t0, s4, IC8x8_HOR_StoreLoop
    addi s4, 16
    j IntraChroma8End
    nop
    .endfunc

    .func IC8x8_VER
IC8x8_VER:
    suv v_u,0, 0*8,s4
    suv v_u,0, 1*8,s4
    suv v_u,0, 2*8,s4
    suv v_u,0, 3*8,s4
    suv v_u,0, 4*8,s4
    suv v_u,0, 5*8,s4
    suv v_u,0, 6*8,s4
    j IntraChroma8End
    suv v_u,0, 7*8,s4
    .endfunc

    .func IC8x8_PLANE
IC8x8_PLANE:
    #define v_coeff_hv   $v16
    #define v_coeff_int  $v17
    #define v_coeff_misc $v18

    li t0, %lo(INTRACHROMA_COEFF1)
    lqv v_coeff_hv,0,   0*16,t0
    lqv v_coeff_int,0,  1*16,t0
    lqv v_coeff_misc,0, 2*16,t0

    #  HV0 = $v02 = U7 U6 U5 U4   L4 L5 L6 L7
    #  HV1 = $v01 = UL U0 U1 U2   L2 L1 L0 UL
    #  V_COEFF_HV = 4  3  2  1    1   2  3  4

    vor $v01, v_zero, v_ul_u,0
    luv $v03,0, 0*8,dmem_l  # L0
    luv $v04,0, 1*8,dmem_l  # L1
    luv $v05,0, 2*8,dmem_l  # L2
    vmov $v01,15, v_ul_u,8 
    vmov $v01,14, $v03,8
    vmov $v01,13, $v04,8
    vmov $v01,12, $v05,8

    vmov $v02,8, v_u,15
    vmov $v02,9, v_u,14
    vmov $v02,10, v_u,13
    vmov $v02,11, v_u,12

    luv $v03,0, 4*8,dmem_l  # L4
    luv $v04,0, 5*8,dmem_l  # L5
    luv $v05,0, 6*8,dmem_l  # L6
    luv $v06,0, 7*8,dmem_l  # L7
    vmov $v02,12, $v03,8
    vmov $v02,13, $v04,8
    vmov $v02,14, $v05,8
    vmov $v02,15, $v06,8

    # Compute (HV0-HV1)*coeff 
    vsubc $v00, $v02, $v01,0
    vsra $v00, $v00, 7
    vmudh $v00, $v00, $v16,0

    # Compute sum of first 4-lanes and second 2-lanes
    vaddc $v00, $v00, $v00,3
    vaddc $v00, $v00, $v00,6

    # Compute (17*H/V + 32)>>6. We use vmulf to have the +32 for free.
    vmulf $v06, $v00, v_coeff_misc,9

    # Compute 16*(U7+L7). Notice that we have U7 in v_u[e15]
    # and L7 in $v02[15]. Put the result in $v01[e15]
    vsrl $v02, $v02, 3
    vsrl $v01, v_u, 3
    vaddc $v01, $v01, $v02,0

    # Now we have
    #   a = $v01[e15]
    #   b = $v06[e8]
    #   c = $v06[e12]
    #
    # all unshifted

    # Compute -3*c
    vmudh $v07, $v06, v_coeff_misc,10 

    # Compute $v07 = A + -3*C. This is the base addend which
    # is constant across all pixels in the first line.
    # We'll update it adding C every line.
    vaddc $v07, $v07, $v01,15
    vor $v07, v_zero, $v07,12   # copy into all lanes

    # Main loop. Generate two lines at a time to lower the overhead
    li t1, 4
LoopPlane8:
    # Compute (a + b*(x-3) + c*(y-3) + 16)>>5
    # where x is the lane number (0..7)
    vmulf $v00, v_coeff_int, $v06,8
    vmacu $v00, $v07, v_coeff_misc,11

    # Update the constant addend: $v07 += c
    vaddc $v07, $v07, $v06,12

    # Compute the same on next line
    vmulf $v01, v_coeff_int, $v06,8
    vmacu $v01, $v07, v_coeff_misc,11

    # Update the constant addend: $v07 += c
    vaddc $v07, $v07, $v06,12

    vch $v00, $v00, v_coeff_misc,8
    vch $v01, $v01, v_coeff_misc,8

    vsll $v00, $v00, 7
    vsll $v01, $v01, 7

    suv $v00,0, 0*8,s4
    suv $v01,0, 1*8,s4
    addi s4, 16

    bnez t1, LoopPlane8
    addi t1, -1

    #undef v_coeff_misc
    #undef v_coeff_int
    #undef v_coeff_hv

    j IntraChroma8End
    nop
    .endfunc

    #undef v_u4
    #undef dmem_l

####################################################################

    # Registers shared by Luma4x4 and Chroma8x8
    #undef v_ul_u 
    #undef v_u    
    #undef v_l0   
    #undef v_l1   
    #undef v_l2   
    #undef v_l3   

####################################################################
####################################################################

####################################################################
# OMX_IntraPredictLuma16x16
#
# Intra-prediction of a 16x16 luma macroblock partition.
####################################################################
# ARGS:
#   0: srcbuf_l: pointer to first pixel in left column (-1,0)
#   1: srcbuf_u: pointer to first pixel in top column (0,-1)
#   2: srcbuf_ul: pointer to pixel in top-left corner (-1,-1)
#   3: dstbuf: pointer to destination buffer
#   4: src_dma_width: pitch of the source buffers
#   5: dst_dma_width: pitch of the destination buffers
#   6: mode: prediction mode
#   7: availability: bitmask of availability of neighbor macroblocks

    .func OMX_IntraPredictLuma16x16
OMX_IntraPredictLuma16x16:    
    lw src_dma_width, CMD_ADDR(16, 32)
    lw dst_dma_width, CMD_ADDR(20, 32)
    lw availability, CMD_ADDR(28, 32)

    jal IL16x16
    lw t3, CMD_ADDR(24, 32)   # mode

    # Copy 16x16 macroblock back into RDRAM
    li t2, DMA_OUT   # direction: DMEM -> RDRAM
    jal_and_j IntraMB16InOut, RSPQ_Loop
    .endfunc


####################################################################
# IL16x16
#
# Inner function for Intra-prediction of a 16x16 luma macroblock partition.
####################################################################
# ARGS:
#   a0: srcbuf_l: pointer to first pixel in left column (-1,0)
#   a1: srcbuf_u: pointer to first pixel in top column (0,-1)
#   a2: srcbuf_ul: pointer to pixel in top-left corner (-1,-1)
#   k0: src_dma_width: pointer to destination buffer
#   t3: prediction mode
#
    .func IL16x16
IL16x16:
    #define v_u0    $v08
    #define v_u1    $v09
    #define v_ul    $v11
    #define v_l0    $v12
    #define v_l1    $v13
    #define dmem_l  s3

    vxor v_zero, v_zero, v_zero,0

    # We save return address in sp to be able to call subroutines from this
    # function.
    move sp, ra

    .macro IL16x16Return
    jr sp; nop
    .endm

    # Load UL byte into the corresponding register. We load
    # 8 pixels but we only care about first one.
    move s0, srcbuf_ul
    jal IntraSrcIn
    li t0, DMA_SIZE(8,1)
    luv v_ul,0, 0*8,s4

    # Read U. This is a sequence of 16 ALIGNED bytes
    move s0, srcbuf_u
    jal IntraSrcIn
    li t0, DMA_SIZE(16,1)
    luv v_u0,0, 0*8,s4
    luv v_u1,0, 1*8,s4

    # Read L. This will be a 8x16 block where the first column
    # is the L vector
    move s0, srcbuf_l
    jal IntraSrcIn
    li t0, DMA_SIZE(8,16)
    move dmem_l, s4

    # Prepare the vectors V_L0/V_L1 containing the first column that
    # was copied by DMA, that is 1 byte every 8. We use LFV to load
    # one every 4 bytes, then store into scratch memory, and then
    # again LHV to load one every 2 bytes.
    li t0, %lo(V_0)
    lfv v_l0,0, 0*16,dmem_l
    lfv v_l0,8, 1*16,dmem_l
    lfv v_l1,0, 2*16,dmem_l
    lfv v_l1,8, 3*16,dmem_l
    suv v_l0,0, 0*8,t0
    suv v_l1,0, 1*8,t0

    lfv v_l0,0, 4*16,dmem_l
    lfv v_l0,8, 5*16,dmem_l
    lfv v_l1,0, 6*16,dmem_l
    lfv v_l1,8, 7*16,dmem_l
    suv v_l0,0, 2*8,t0
    suv v_l1,0, 3*8,t0

    lhv v_l0,0, 0*16,t0
    lhv v_l1,0, 1*16,t0

    sll t3, t3, 1
    lhu t3, %lo(INTRALUMA16_TABLE)(t3)
    jr t3
    li s4, %lo(MB_OUTPUT)
    .endfunc


    .func IL16x16_VER
IL16x16_VER:
    addi t0, s4, 256-32
IL16x16_VER_StoreLoop:
    suv v_u0,0, 0*8,s4
    suv v_u1,0, 1*8,s4
    suv v_u0,0, 2*8,s4
    suv v_u1,0, 3*8,s4
    bne t0, s4, IL16x16_VER_StoreLoop
    addi s4, 32
    IL16x16Return
    .endfunc

    .func IL16x16_HOR
IL16x16_HOR:
    addi t0, s4, 256-32
IL16x16_HOR_StoreLoop:
    lpv $v00,0, 0*8,dmem_l
    vor $v00, v_zero, $v00,8
    lpv $v01,0, 1*8,dmem_l
    vor $v01, v_zero, $v01,8
    addi dmem_l, 8
    spv $v00,0, 0*8,s4
    spv $v00,0, 1*8,s4
    spv $v01,0, 2*8,s4
    spv $v01,0, 3*8,s4
    bne t0, s4, IL16x16_HOR_StoreLoop
    addi s4, 16
    IL16x16Return
    .endfunc

    .func IL16x16_DC
IL16x16_DC:
    li t3, 2
    andi t0, availability, OMX_VC_UPPER
    beqz t0, IDC16TestLeft
    li t2, 0

    vaddc $v00, v_u0, v_u1,0
    jal VAddLanes
    vsrl $v00, $v00, 7
    mfc2 t2, $v00,0
    addi t2, 8
    addi t3, -1

IDC16TestLeft:
    andi t0, availability, OMX_VC_LEFT
    beqz t0, IDC16Calc

    vaddc $v00, v_l0, v_l1,0
    jal VAddLanes
    vsrl $v00, $v00, 7
    mfc2 t1, $v00,0
    add t2, t1
    addi t2, 8
    addi t3, -1

IDC16Calc:
    beqz t3, IDC16Store
    sll t2, 2
    addi t3, -1
    beqz t3, IDC16Store
    sll t2, 1
    li t2, 128<<7
IDC16Store:
    mtc2 t2, $v00, 0
    vor v_u0, v_zero, $v00,8
    vor v_u1, v_zero, $v00,8
    j IL16x16_VER
    nop
    .endfunc

    .func IL16x16_PLANE
IL16x16_PLANE:
    #define v_coeff_hv    $v16
    #define v_coeff_int1  $v17
    #define v_coeff_int2  $v18
    #define v_coeff_misc  $v19 
    #define plane_c       t6

    li t0, %lo(INTRALUMA16_COEFF1)
    lqv v_coeff_hv,0,   0*16,t0
    lqv v_coeff_int1,0, 1*16,t0
    lqv v_coeff_int2,0, 2*16,t0
    lqv v_coeff_misc,0, 3*16,t0

    # Compute the horizontal filter H into $v06[e9]
    vor $v01, v_zero, v_u0,0
    jal PlaneAccum
    vor $v02, v_zero, v_u1,0
    vmov $v06,9, $v00,8

    # Compute the vertical filter V into $v06[e10]
    vor $v01, v_zero, v_l0,0
    jal PlaneAccum
    vor $v02, v_zero, v_l1,0
    vmov $v06,10, $v00,8

    # Compute (5*H/V+32)>>6. We use vmulf to get the +32
    # for free.
    # FIXME: The maximum value we can get for H/V is 0x1BE4,
    # so check whether this overflows.
    vmulf $v06, $v06, v_coeff_misc,9

    # Compute 16*(U15+L15), into $v01[e15]
    vsrl $v01, v_u1, 3
    vsrl $v02, v_l1, 3
    vaddc $v01, $v01, $v02,0

    # Now we have:
    #   $v01[e15] = a
    #   $v06[e9] = b
    #   $v06[e10] = c
    #
    # all unshifted

    # Compute -7*c
    vmudh $v07, $v06, v_coeff_misc,10 

    # Compute $v07 = A + -7*C. This is the base addend which
    # is constant across all pixels in the first line.
    # We'll update it adding C every line.
    vaddc $v07, $v07, $v01,15
    vor $v07, v_zero, $v07,10   # copy into all lanes

    li t1, 15
LoopPlane:
    # Compute (a + b*(x-7) + c*(y-7) + 16)>>5
    # where x is the lane number (0..15), split between
    # $v00 and $v01
    vmulf $v00, v_coeff_int1, $v06,9
    vmacu $v00, $v07, v_coeff_misc,11

    vmulf $v01, v_coeff_int2, $v06,9
    vmacu $v01, $v07, v_coeff_misc,11

    vch $v00, $v00, v_coeff_misc,8
    vch $v01, $v01, v_coeff_misc,8

    vsll $v00, $v00, 7
    vsll $v01, $v01, 7

    # Update the constant addend: $v07 += c
    vaddc $v07, $v07, $v06,10

    suv $v00,0, 0*8,s4
    suv $v01,0, 1*8,s4
    addi s4, 16

    bnez t1, LoopPlane
    addi t1, -1

    IL16x16Return
    .endfunc

    .func PlaneAccum
PlaneAccum:
    # Given:
    #   $v01: U0 U1 U2  U3  U4  U5  U6  U7
    #   $v02: U8 U9 U10 U11 U12 U13 U14 U15
    #
    # We need to calculate:
    #  8 * (U15 - UL) +
    #  7 * (U14 - U0) +
    #  6 * (U13 - U1) + 
    #  5 * (U12 - U2) +
    #  4 * (U11 - U3) +
    #  3 * (U10 - U4) +
    #  2 * (U9 - U5) +
    #  1 * (U8 - U6)
    #
    # (Notice the presence of UL, and U7 being ignored).
    #
    # So the first problem is that we need to reverse most of $v01
    # to be able to subtract it from $v02. Unfortunately, 
    # there is no fast way of doing it, but moving each lane
    # one at a time.
    vmov $v00,8,  $v01,14
    vmov $v00,9,  $v01,13
    vmov $v00,10, $v01,12
    vmov $v00,11, $v01,11
    vmov $v00,12, $v01,10
    vmov $v00,13, $v01,9
    vmov $v00,14, $v01,8
    vmov $v00,15, v_ul,8

    # Do the subtraction, and multiply by the coefficient
    # vector (8, 7, .., 1).
    vsubc $v00, $v02, $v00,0
    vsra $v00, $v00, 7
    vmudh $v00, $v00, v_coeff_hv,0

    # FALLTRHOUGH!
    .endfunc

    .func VAddLanes
VAddLanes:
    # Sum all lanes together into $v00[e8]
    vaddc $v00, $v00, $v00,3
    vaddc $v00, $v00, $v00,6
    jr ra
    vaddc $v00, $v00, $v00,12
    .endfunc

    #undef v_coeff_hv   
    #undef v_coeff_int1 
    #undef v_coeff_int2 
    #undef v_coeff_misc 
    #undef plane_c      

    #undef v_u0
    #undef v_u1
    #undef v_zero
    #undef v_ul
    #undef v_l0
    #undef v_l1
    #undef dmem_l

####################################################################

    #undef v_zero 
    #undef v_coeff

    #undef srcbuf_l 
    #undef srcbuf_u  
    #undef srcbuf_ul
    #undef dstbuf   

    #undef src_dma_width 
    #undef dst_dma_width 

####################################################################
####################################################################
#if 0

###################################################################
# Task_ProcessLumaIntra4x4Residual
#
####################################################################
# ARGS:
#    0: src frame (used only for neighbor pixels)
#    1: dst buffer (where the destination macroblock will be written)
#    2: src_dma_width
#    3: dst_dma_width
#    4: modeAvail bits (0-3)
#    5: modeAvail bits (4-7)
#    6: modeAvail bits (8-11)
#    7: modeAvail bits (12-15)
#    8: qp
#    9: totalCoeff pointer (can be NULL)

    #define src a0
    #define dst a1
    #define src_dma_width a2
    #define dst_dma_width a3

    #define npart t8

    .func Task_ProcessLumaIntra4x4Residual
Task_ProcessLumaIntra4x4Residual:
    # Load TotalCoeff table (if asked to do so). Otherwise,
    # it's already loaded correctly and we can just use it.
    jal SetTotalCoeff
    lw s0, %lo(OMXDL_ARGS+36)

LoadBlocks:
    # We will treat MB_INPUT as a 33x17 buffer, where the first line
    # and column come from the source frame (neighbour macroblocks), and 
    # the final macroblock will be predicted at (1,1)-(17,17).

    # DMA from source frame pixel (-1,-1) into MB_INPUT. We only need
    # the first line and column, but it's faster to simply bring the whole
    # 33x17 in (aka 40x17 after unalignment adjustment).
    sub s0, src, src_dma_width
    addi s0, -1
    li s4, %lo(MB_INPUT)
    li t0, DMA_SIZE(INTRA4RES_MBINPUT_WIDTH, 17)
    jal DMAIn
    move t1, src_dma_width

    # Calculate dequantization factors in v29 / v30 / v31.
    jal CalcDequantFactors
    lw t0, %lo(OMXDL_ARGS+32)

    li npart, 0
LumaBlockLoop:
    # Calculate starting position of the 4x4 block within MB_INPUT.
    # This will be our output buffer (in s4)
    sll t0, npart, 1
    lhu t0, %lo(INTRA4RES_OFFSET)(t0)
    addi s4, t0, %lo(MB_INPUT)

    # Load upper-left pixel in $v00, and upper line in $v01,
    # and leave s3 pointing at the first left pixel.
    addi s3, s4, -INTRA4RES_MBINPUT_WIDTH-1
    luv $v00,0, 0*8,s3
    addi s3, 1
    luv $v01,0, 0*8,s3
    addi s3, INTRA4RES_MBINPUT_WIDTH-1

    # Load mode_avail
    lbu t0, %lo(OMXDL_ARGS+16)(npart)

    # Extract mode and availability (remapping bits)
    srl t2, t0, 4
    andi t0, t0, 0xF
    lbu fp, %lo(INTRA4RES_AVAILABILITY)(t0)

    li t3, INTRA4RES_MBINPUT_WIDTH   # left pitch
    jal IL4X4
    li t4, INTRA4RES_MBINPUT_WIDTH   # dest pitch

    # Check whether we need to apply residual to this block.
    lbu t0, %lo(TCOEFF_TABLE)(npart)
    beqz t0, LumaBlockLoopEnd
    nop

    jal UnpackNextDeltaBlock
    nop
    jal Dequant4x4
    nop
    # s4 still points to the output buffer, where residual
    # will be applied
    jal TransformResidual4x4IDCT
    nop

LumaBlockLoopEnd:
    bne npart, 15, LumaBlockLoop
    addi npart, 1

    # Our macroblock is now written in MB_INPUT at (8,8)-(24,24)
    # We need to move it to MB_OUTPUT as a 16x16 block without extra pixels,
    # to be able to DMA it to DMEM.
    li s0, %lo(MB_INPUT)+INTRA4RES_INITIAL_OFFSET
    li s4, %lo(MB_OUTPUT)

    # Copy 4 lines at a time
    li t0, 3
Intra4Res_CopyLoop:
    luv $v00,0, 0*8,s0  # row 0
    luv $v01,0, 1*8,s0
    luv $v02,0, 5*8,s0  # row 1
    luv $v03,0, 6*8,s0
    luv $v04,0, 10*8,s0 # row 2
    luv $v05,0, 11*8,s0
    luv $v06,0, 15*8,s0 # row 3
    luv $v07,0, 16*8,s0
    suv $v00,0, 0*8,s4
    suv $v01,0, 1*8,s4
    suv $v02,0, 2*8,s4
    suv $v03,0, 3*8,s4
    suv $v04,0, 4*8,s4
    suv $v05,0, 5*8,s4
    suv $v06,0, 6*8,s4
    suv $v07,0, 7*8,s4

    addi s0, 4*INTRA4RES_MBINPUT_WIDTH
    addi s4, 4*16
    bnez t0, Intra4Res_CopyLoop
    addi t0, -1

    # DMA from MB_OUTPUT into DMEM and exit
    move s0, dst
    li s4, %lo(MB_OUTPUT)
    move t1, dst_dma_width
    li t0, DMA_SIZE(16,16)
    jal_and_j DMAOut, RSPQ_Loop

    .endfunc

    #undef src
    #undef dst
    #undef src_dma_width
    #undef dst_dma_width

    #undef npart


###################################################################
# Task_ProcessLumaIntra16x16Residual
#
####################################################################
# ARGS:
#    0: src frame (used only for neighbor pixels)
#    1: dst buffer (where the destination macroblock will be written)
#    2: src_dma_width
#    3: dst_dma_width
#    4: predMode
#    5: availability
#    6: qp
#    7: totalCoeff pointer (can be NULL)

    #define src a0
    #define dst a1
    #define src_dma_width k0   // note: this must match the one used in IL16x16
    #define dst_dma_width k1   // note: this must match the one used in IL16x16
    #define availability  fp   // note: this must match the one used in IL16x16

    #define dc    t7
    #define npart t8

    .func Task_ProcessLumaIntra16x16Residual
Task_ProcessLumaIntra16x16Residual:
    move src_dma_width, a2
    move dst_dma_width, a3
    lw gp, %lo(OMXDL_ARGS+24)  # qp
    lw availability, %lo(OMXDL_ARGS+20)

    jal CalcDequantFactors
    move t0, gp

    # Load TotalCoeff table (if asked to do so). Otherwise,
    # it's already loaded correctly and we can just use it.
    jal SetTotalCoeff
    lw s0, %lo(OMXDL_ARGS+28)

    # Reset LUMA_DC to zero
    li s4, %lo(LUMA_DC)
    vxor $v00, $v00, $v00,0
    sqv $v00,0, 0*16,s4
    sqv $v00,0, 1*16,s4

    # Check whether totalCoeff[24] != 0, which means that
    # there are DC data to unpack and transform.
    lbu t0, %lo(TCOEFF_TABLE+24)
    beqz t0, Luma16Predict
    nop

    jal UnpackNextDeltaBlock
    nop
    jal TransformDequantLumaDC
    nop

Luma16Predict:
    sub a1, src, src_dma_width   # U
    addi a2, a1, -1              # UL
    addi a0, src, -1             # L
    jal IL16x16
    lw t3, %lo(OMXDL_ARGS+16)    # prediction mode

    # Go through the 16 4x4 blocks
    li npart, 0
Luma16ResidualLoop:
    # Read AC for the current block from TCOEFF_TABLE
    # Read DC for the current block from LUMA_DC
    lbu t1, %lo(DC_DEQUANT_INDEX)(npart)
    lbu t0, %lo(TCOEFF_TABLE)(npart)
    lh dc, %lo(LUMA_DC)(t1)

    # If AC/DC are both 0, just skip this block, there's no residual.
    or t2, dc, t0
    beqz t2, Luma16ResidualNext
    nop

    # If AC is not 0, there is a residual coefficient matrix to unpack
    # and dequant (into $v00/$v01). Otherwise, initialize the matrix
    # to zero.
    vxor $v00, $v00, $v00,0
    beqz t0, Luma16ResidualIDCT
    vxor $v01, $v01, $v01,0

    jal UnpackNextDeltaBlock
    nop
    jal Dequant4x4
    nop

Luma16ResidualIDCT:
    # Set DC as first coefficient in the matrix
    mtc2 dc, $v00,0

    # Run IDCT over the coefficient matrix and add the result to the
    # predicted pixels in s4
    lbu t2, %lo(LUMA_DEQUANT_INDEX)(npart)
    li t4, 16
    jal TransformResidual4x4IDCT
    addi s4, t2, %lo(MB_OUTPUT)

Luma16ResidualNext:
    bne npart, 15, Luma16ResidualLoop
    addi npart, 1

    # Copy 16x16 macroblock back into RDRAM
    lw a3, %lo(OMXDL_ARGS+4)
    li t2, DMA_OUT   # Direction DMEM -> RDRAM
    jal_and_j IntraMB16InOut, RSPQ_Loop
    .endfunc

#endif