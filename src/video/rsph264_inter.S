############################################################
# RSPH264 - Inter-prediction functions
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
############################################################
#include <rsp_queue.inc>

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand OMX_DequantTransformResidual,         28
        # OMX_TransformDequantLumaDC moved to the intra overlay:
        # TransformDequantLumaDC needs ~324 bytes of IMEM that this
        # overlay can't spare. rsph264_queue_transform_dequant_lumadc
        # now dispatches to the intra overlay (slot 5). 
        # Slot kept as a no-op placeholder so the existing
        # command IDs don't shift.
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand OMX_TransformDequantChromaDC,         12
        RSPQ_DefineCommand Task_ProcessLumaInterResidual,        28
        RSPQ_DefineCommand Task_ProcessChromaResidual,           28
        RSPQ_DefineCommand OMX_Interpolate_Luma_Overfill,        44
        RSPQ_DefineCommand OMX_Interpolate_Chroma_Overfill,      44
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand RSPQ_Loop,                             4
        RSPQ_DefineCommand Task_SetPackedDeltaBuffer,             4
    RSPQ_EndOverlayHeader

#if 0
    # NOTE: this must be at the start of the .data overlay section
    # so that it concatenates to the main table.
DECLARE_OMXDL_TABLE(
    0,        // FIXME: remove
    0,        // FIXME: remove
    OMX_DequantTransformResidual - _start,
    OMX_TransformDequantLumaDC - _start,
    OMX_TransformDequantChromaDC - _start,
    Task_ProcessLumaInterResidual - _start,
    Task_ProcessChromaResidual - _start,
    OMX_Interpolate_Luma_Overfill - _start,
    OMX_Interpolate_Chroma_Overfill - _start
)
#endif

#include "rsph264_common.inc"

    .data

    .align 4
    .ascii "OVL: Inter-pred "

    .align 4
InterLumaCoordConstants:
    .half  1         # +1/-1 constant for range adjustment and src offset calculation
    .half  0xFFF8    # ~7: align dma width
    .half  0 
    .half  7         # +7: align dma width

    .half  1         # +1/-1 constant for range adjustment and src offset calculation
    .half  0xFFFF    # ~0: align dma height
    .half  0
    .half  0         # +0: align dma height (also used as 0 elsewhere)

InterLumaMVConst:
    .half  3         # Motion vector: isolate fractional part (2 bits)
    .half  2         # overread before x
    .half  3         # overread after x
    .half  0

    .half  0x4000    # Motion vector SRA constant (vmudm 0x4000 = SRA 2)
    .half  2         # overread before y
    .half  3         # overread after y
    .half  0         

InterChromaMVConst:
    .half  7         # Motion vector: isolate fractional part (3 bits)
    .half  1         # overread before x
    .half  1         # overread after x
    .half  0         # must stay 0 (used as "overread before x" for dst buffer)

    .half  0x2000    # Motion vector SRA constant (vmudm 0x2000 = SRA 3)
    .half  1         # overread before y
    .half  1         # overread after y
    .half  0

Interpolate5_AverageYes:
    .half  0x80      # shift pixel <<7 before average
    .half  0x1       # average pixel multiplier
    .half  0x80      # rounding (0.5)
Interpolate5_AverageNo:
    .half  0x100     # shift pixel <<8 to store with lpv
    .half  0x0       # average pixel multiplier (= discard average)
    .half  0x0       # rounding


    # FIXME **************************
OMXDL_ARGS: 

    .bss

    # Macroblock input buffer. A macroblock is max 16x16,
    # but we need to read more lines/pixels because of
    # over-reading by interpolation and because of DMA
    # alignment constraints, so space for 32x21 pixels
    # is required.
    .align 8   # 4 is sufficient, but 8 visually helps debugging
MB_INPUT:   .dcb.b 32*21

    # Maximum macroblock output. We fill this in without
    # over-reading (and RDRAM buffer is always aligned),
    # so we don't need more than 16x16
    .align 8   # 4 is sufficient, but 8 visually helps debugging
MB_OUTPUT:  .dcb.b 16*16

    # Scratch memory for intermediate computations
    .align 8   # 4 is sufficient, but 8 visually helps debugging
MB_SCRATCH: .dcb.w 16*21


    .text

####################################################################
#
#  INTERPREDICTION: CHROMA & LUMA INTERPOLATION
#
####################################################################

    # Registers with a fixed meaning across all the chroma/luma
    # interpolation functions.
    #define src  a0                // Source memory buffer (in RDRAM)
    #define src_pitch  a1          // Source pitch (in RDRAM)
    #define dst  a2                // Dest memory buffer (in RDRAM)
    #define dst_pitch  a3          // Dest pitch (in RDRAM)
    #define width  v0              // Width of macroblock (real pixels)
    #define height  v1             // Height of macroblock (real pixels)
    #define dx  k0                 // Fractional horizontal motion vector
    #define dy  k1                 // Fractional vertical motion vector
    #define unalign_first_line s5  // Offset of first pixel on first DMA line (from MB_INPUT)
    #define src_unalign t4         // Pointer to first real macroblock pixel within MB_INPUT
    #define dst_unalign t5         // Pointer to first real macroblock pixel within MB_OUTPUT
    #define src_dma_width t6       // Width/pitch of source macroblock in DMEM
    #define dst_dma_width t7       // Width/pitch of destination macroblock in DMEM
    #define src_dma_height t8      // Height of the source macroblock in DMEM
    #define dst_dma_height t9      // Height of the destination macroblock in DMEM

    #define v_mv_const         $v18  // constants used for bootstrapping
    #define v_frame_size       $v19  // frame size ([e8]=width, [e12]=height)
    #define v_block_size       $v20  // block size ([e8]=width, [e12]=height)
    #define v_mv               $v21  // fractional motion vector ([e8]=X, [e12]=Y)
    #define v_overread         $v22  // overread pixels ([e9/10]=before/after_x, [e13/14]=before/after_y)
    #define v_const            $v23  // constants used for bootstrapping
    #define v_pos              $v24  // macroblock position within frame ([e8]=X, [e12]=Y)
    #define v_dma_size         $v25

    # Local registers
    # 
    # t0-t3: used as fully scratch
    # s0-s3: usually used as pointers to DMEM buffers.
    #
    # Sometimes used as arguments to function:
    #
    #   s0: mostly used as argument to indicate the address of
    #     the input macroblock to process in a function.
    #   s4: mostly used as argument to indicate the address of
    #     the output macroblock to process in a function.


####################################################################
# Interpolate_Bootstrap
#
# Bootstrap function for interpolation (luma or chroma)
####################################################################
#
# ARGS:
#   t0: either %lo(InterLumaMVConst) or %lo(InterChromaMVConst)
#

    .func Interpolate_Bootstrap
Interpolate_Bootstrap:
    vxor v_mv, v_mv, v_mv,0
    vxor v_block_size, v_block_size, v_block_size,0

    # Load constants used in initial calculations
    lqv v_mv_const,0, 0*16,t0
    li t1, %lo(InterLumaCoordConstants)
    lqv v_const,0, 0*16,t1

    # Load input coordinates
    la t0, CMD_ADDR(0, 44)    # FIXME: Valid for overfill commands ONLY
    lsv v_mv,0, 12*2,t0
    lsv v_mv,8, 13*2,t0
    lsv v_pos,0, 14*2,t0
    lsv v_pos,8, 15*2,t0
    lsv v_frame_size,0, 8*2,t0
    lsv v_frame_size,8, 9*2,t0
    lsv v_block_size,0, 10*2,t0
    lsv v_block_size,8, 11*2,t0

    # isolate integer part of motion vector. This is exactly like
    # a "vsra" operator, where the shift factor is provided by the 
    # caller in v_mv_const
    vmudm $v00, v_mv, v_mv_const,12
    vaddc v_pos, v_pos, $v00,0

    # Set overread constants
    vor v_mv, v_mv, v_mv,4                 # repeat motion vectors (0-3, 4-7)
    vand v_mv, v_mv, v_mv_const,8          # isolate fractional parts
    vne $v00, v_mv, v_const,15             # compare to zero
    vmrg v_overread, v_mv_const, v_const,15

    cfc2 fp, COP2_CTRL_VCC                 # fp != 0 if at least 1 fractional MV
    mfc2 dx, v_mv,0                        # extract fractional X
    mfc2 dy, v_mv,8                        # extract fractional Y

    # Clamp against [-W-ORAX+1, SW+ORBX-1] / [-H-ORAY+1, SH+ORBY-1], which
    # are the largest values that will cause a DMA that contains at least
    # 1 pixel of the reference image. Because of overfilling, coordinates
    # outside of these ranges produce identical results than the clamped ones.
    vsubc $v02, v_const, v_block_size,0    # $v02 = 1-W, 1-H
    vsubc $v03, v_frame_size, v_const,8    # $v03 = SW-1, SH-1
    vsubc $v02, $v02, v_overread,6         # $v02 -= ORAX/ORAY
    vaddc $v03, $v03, v_overread,5         # $v03 += ORBX/ORBY
    vge v_pos, v_pos, $v02,0
    vlt v_pos, v_pos, $v03,0

    # Now see if there's overfill (that is, if at least one required pixel
    # is outside of the reference image).

    # Calculate the corners of the macro-block, starting from the center,
    # so:
    #    $v00 = left/top corner (v_pos - overread)
    #    $v01 = bottom/right corner (v_pos + overread + block_size)
    vsubc $v02, v_pos, v_overread,5  # before
    vaddc $v03, v_pos, v_overread,6  # after

    vaddc $v01, $v03, v_block_size,0
    vsubc $v01, $v01, v_const,8

    vge $v00, $v02, v_const,15
    cfc2 s6, COP2_CTRL_VCC    # bit 0/4: overfill left/top
    vlt $v01, $v01, v_frame_size,0
    cfc2 s7, COP2_CTRL_VCC    # bit 0/4: overfill right/bottom

    # Calculate SRC += (Y-OVER_READ_Y_BEFORE)*SRC_PITCH + (X-OVER_READ_X_BEFORE). 
    # We use a vector instruction to do the multiplication, and we need to fetch
    # the 32-bit result and add it to src.
    mtc2 src_pitch, $v04,8
    vmudn $v04, $v04, $v02,0    # $v02 is v_pos - v_overread[before]
    vmadn $v04, v_const, $v02,8    # += (v_pos_x-v_overred_before_x)*1
    mfc2 t0, $v04,8
    vsar $v05, $v05, $v05,9
    mfc2 t1, $v05,8
    and t0, 0xffff
    sll t1, 16
    add src, t0
    add src, t1

    # Now src points to the (unaligned) address of the first pixel to
    # read (including overread). dst points to the (unaligned) address
    # of destination.

    # Compute all DMA sizes and alignments. These are the formulas
    # we implement:
    #
    #  src_unalign = src & 7
    #  dst_unalign = dst & 7
    #
    #  dst_width = round8(block_width + dst_unalign)
    #  dst_height = block_height
    #
    #  src_width = round8(block_width + src_unalign + overread_before_x + overread_after_x)
    #  src_height = block_height + overread_before_y + overread_after_y
    #
    #  src_unalign += overread_before_x + src_dma_width*overread_before_y
    #
    # FIXME: src_unalign is a misnomer for "offset of the first pixel within
    # MB_INPUT" (same for dst_unalign). Rename.
    #

    # $v00:
    #    <1>: src & 7 (RDRAM address unalignment)
    vxor $v00, $v00, $v00,0
    and t0, src, 7
    and dst_unalign, dst, 7
    mtc2 t0, $v00,2

    # v_dma_size
    #    <0>: width
    #    <4>: height
    vor v_dma_size, v_block_size, v_block_size,4
    vaddc v_dma_size, v_dma_size, v_const,7           # add 7 (only on width)
    vaddc $v01, v_dma_size, $v00,5                    # + src_unalign (only x)
    vaddc $v01, $v01, v_overread,5                    # + overread before (x/y)
    vaddc $v01, $v01, v_overread,6                    # + overread after (x/y)
    vand $v01, $v01, v_const,5                        # and ~7 (only on width)
    vand v_dma_size, v_dma_size, v_const,5            # and ~7 (only on width)

    mfc2 dst_dma_height, v_dma_size,8
    mfc2 dst_dma_width, v_dma_size,0
    mfc2 src_dma_height, $v01,8
    mfc2 src_dma_width, $v01,0
    mfc2 width, v_block_size,0
    mfc2 height, v_block_size,8

    # Calculate src_unalign += overread_before_x + src_dma_width*overread_before_y
    vaddc $v00, $v00, v_overread,9
    mfc2 unalign_first_line, $v00,2
    vmudn $v02, $v01, v_overread,13
    vaddc $v00, $v00, $v02,0
    mfc2 src_unalign, $v00,2

    # Prepare the output buffer merge mask.
    # When writing to the output buffer, we must not override
    # bytes outside the requested size. For instance, we might
    # have DMA'd in 8 bytes per row, but the partition is only
    # 4 bytes, so we need not to touch the last 4 bytes in each
    # row. Prepare a mask in VCC that will be used by VMRG.
    #
    #   Width      Mask        
    #     2        11111100
    #     4        11110000
    #     8        00000000    
    #     16       00000000
    li t0, 0xFF
    sllv t0, t0, width
    ctc2 t0, COP2_CTRL_VCC

Interpolate_Bootstrap_Phase2:
    move ra2, ra

    # DMA into DMEM the input macroblock (source pixels)
    sll t0, src_dma_height, 12
    or t0, src_dma_width
    addi t0, -0x1001
    li s4, %lo(MB_INPUT)
    move s0, src
    jal DMAInAsync
    move t1, src_pitch

    # DMA into DMEM the output macroblock (destination pixels)
    jal Interpolate_ChromaLuma_DMADest
    li t2, DMA_IN_ASYNC

    # We now have one transfer in progress (input macroblock)
    # and another one queued (output macroblock). Check the overfill
    # conditions. If we need access to the source pixels for overfill,
    # we will call DMAWaitReady to wait the end of the first transfer only
    # (source pixel) and keep the second one running in background.

OverfillStart:
    # Handle overfilling over just-read source buffer.
    #
    # Overfilling is required when accessing a macroblock on the boundary
    # of the screen: in case the motion vector requires interpolating it,
    # we will access pixels outside of the screen, and those must be
    # simulated by extending the boundary pixel (eg: a pixel at coordinate
    # -3,15 is the same of 0,15).

    # Check overfill left
    andi t0, s6, 1
    bnez t0, OverfillCheckRight
    mfc2 t0, v_pos,0

    jal DMAWaitReady       # We'll need the pixel soon, wait for source transfer to finish.

    # Let s0 point to pixel on coordinate 0. This is always
    # the first pixel of the screen, so it's always 8-byte aligned.
    sub t0, unalign_first_line, t0
    addi s0, t0, %lo(MB_INPUT)

    # What we need to do is to copy the first pixel to the
    # N previous 8-byte words that were DMA'd, that is up to s4.
    # There is at least 1 8-byte word to overfill (because X < 0).
    # There are at most 3 8-byte words to overfill, in case the x
    # coordinate was -15 and there were 3 over_read_x pixels.
    addi t1, t0, -8      # t1 > 0 if there are 2 words to overfill
    addi t2, t1, -8      # t2 > 0 if there are 3 words to overfill

    # Loop two rows at a time
    addi t3, src_dma_height, -2
    vxor $v00, $v00, $v00,0
    SpinWait 2             # a little bit more waiting on the output pixels
OverfillLeftLoop:
    lpv $v01,0, 0*8,s0     # First row (value in $v01[e8])
    add s1, s0, src_dma_width
    lpv $v02,0, 0*8,s1     # Second row (value in $v02[e8])

    vor $v01, $v00, $v01,8
    vor $v02, $v00, $v02,8

    spv $v01,0, -1*8,s0
    blez t1, OverfillLeftLoopEnd
    spv $v02,0, -1*8,s1

    spv $v01,0, -2*8,s0
    blez t2, OverfillLeftLoopEnd
    spv $v02,0, -2*8,s1

    spv $v01,0, -3*8,s0
    spv $v02,0, -3*8,s1

OverfillLeftLoopEnd:
    add s0, s1, src_dma_width
    bgtz t3, OverfillLeftLoop
    addi t3, -2
    j OverfillCheckTop
    nop

OverfillCheckRight:
    andi t0, s7, 1
    bnez t0, OverfillCheckTop
    mfc2 t0, v_pos,0
    mfc2 t1, v_frame_size,0

    jal DMAWaitReady     # We'll need the pixel soon, wait for source transfer to finish.

    # Let s0 point to pixel on coordinate width (so 1 after the last
    # pixel). Notice that the last pixel of the screen is
    # always at byte 7 within a 8-byte aligned word.
    sub t0, unalign_first_line, t0
    add t0, t1
    addi s0, t0, %lo(MB_INPUT)

    # What we need to do is to copy the last pixel to the
    # N following 8-byte words that were DMA'd.
    # See OverfillLeft for more info.
    sub t0, src_dma_width, t0
    addi t1, t0, -8      # t1 > 0 if there are 2 words to overfill
    addi t2, t1, -8      # t2 > 0 if there are 3 words to overfill

    # Now make s0 point to the last word (so that last pixel of the 
    # screen is in [e15])
    addi s0, -8

    # Loop two rows at a time
OverfillRightStart:
    addi t3, src_dma_height, -2
    vxor $v00, $v00, $v00,0
    SpinWait 1             # a little bit more waiting on the output pixels
OverfillRightLoop:
    lpv $v01,0, 0*8,s0     # First row (value in $v01[e8])
    add s1, s0, src_dma_width
    lpv $v02,0, 0*8,s1     # Second row (value in $v02[e8])

    vor $v01, $v00, $v01,15
    vor $v02, $v00, $v02,15

    spv $v01,0, 1*8,s0
    blez t1, OverfillRightLoopEnd
    spv $v02,0, 1*8,s1

    spv $v01,0, 2*8,s0
    blez t2, OverfillRightLoopEnd
    spv $v02,0, 2*8,s1

    spv $v01,0, 3*8,s0
    spv $v02,0, 3*8,s1

OverfillRightLoopEnd:
    add s0, s1, src_dma_width
    bgtz t3, OverfillRightLoop
    addi t3, -2
    # fallthrough

OverfillCheckTop:
    # Check overfill top
    andi t0, s6, 0x10
    bnez t0, OverfillCheckBottom
    mfc2 t1, v_pos,8
    mfc2 t0, v_overread,10
    sub t1, t0

    # Calculate pointer to first actual line. We want to do
    # MB_INPUT + src_dma_width*t0, but it's complicated because we
    # don't have multiplications
    li s1, %lo(MB_INPUT)
    neg t0, t1
OverfillTopCalcRow:
    add t1, 1
    bnez t1, OverfillTopCalcRow
    add s1, src_dma_width

    jal DMAWaitReady # We'll need the pixel soon, wait for source transfer to finish.
    sub s0, s1, src_dma_width
    j OverfillVertical
    neg s2, src_dma_width  # step

OverfillCheckBottom:
    # Check overfill top
    andi t0, s7, 0x10
    bnez t0, OverfillEnd
    mfc2 t0, v_pos,8
    mfc2 t1, v_frame_size,8
    mfc2 t2, v_overread,10

    sub t1, t0
    add t1, t2
    sub t0, src_dma_height, t1

    li s0, %lo(MB_INPUT)
OverfillBottomCalcRow:
    addi t1, -1
    bnez t1, OverfillBottomCalcRow
    add s0, src_dma_width

    sub s1, s0, src_dma_width
    jal DMAWaitReady     # We'll need the pixel soon, wait for source transfer to finish.
    move s2, src_dma_width  # step

    # Vertical overfilling loop
    # (shared between top/bottom)
OverfillVertical:
    addi t1, src_dma_width, -8
    addi t2, t1, -8
    addi t3, t2, -8

    SpinWait 3    # required after DMAWaitReady as a further wait loop, see documentation
    lpv $v00,0, 0*8,s1
    lpv $v01,0, 1*8,s1
    lpv $v02,0, 2*8,s1
    lpv $v03,0, 3*8,s1

    li s4, %lo(MB_INPUT)
OverfillTop:
    blez t1, OverfillTopCheckEnd
    spv $v00,0, 0*8,s0
    blez t2, OverfillTopCheckEnd
    spv $v01,0, 1*8,s0
    blez t3, OverfillTopCheckEnd
    spv $v02,0, 2*8,s0
    spv $v03,0, 3*8,s0
OverfillTopCheckEnd:
    addi t0, -1
    bnez t0, OverfillTop
    add s0, s2

OverfillEnd:
    jr ra2
    nop
    .endfunc


    #undef unalign_first_line

####################################################################
# Interpolate_ChromaLuma_DMADest
#
# Run DMA transfer between RDRAM "dst" and DMEM buffer (MB_OUTPUT).
#
# Input:
#    t2: direction: 0=fetch (RDRAM->DMEM),  1=store (DMEM->RDRAM)
####################################################################

    .func Interpolate_ChromaLuma_DMADest
Interpolate_ChromaLuma_DMADest:
    sll t0, dst_dma_height, 12
    or t0, dst_dma_width
    addi t0, -0x1001
    li s4, %lo(MB_OUTPUT)
    move s0, dst
    j DMAExec
    move t1, dst_pitch
    .endfunc


############################################################
# Interpolate_CopyMacroblock
#
# "Copy" 8 horizontal bytes from source macroblock (s0) to
# dest macroblock (s4), for the number of lines in "height".
#
# src/dst pitches are read from src_dma_width / src_dma_height
# respectively.
#
# The "copy" is actually a merge respecting the current merge
# mask in VCC (so if the macroblock width is actually less
# then 8 bytes, extra destination bytes are preserved).
#
# For a 16x16 macroblock, we just call this function twice.
#
#############################################################
    .func Interpolate_CopyMacroblock
Interpolate_CopyMacroblock:

    move ra2, ra
    jal DMAWaitIdle
    addi t0, height, -2
    move ra, ra2

    .align 3
Interpolate_CopyLoop:
    lpv $v00,0, 0*8,s0                    ;add s0,src_dma_width
    lpv $v01,0, 0*8,s4                    ;add s1,s4,dst_dma_width
    lpv $v08,0, 0*8,s0                    ;add s0,src_dma_width
    lpv $v09,0, 0*8,s1
    vmrg $v00, $v01, $v00,0
    vmrg $v08, $v09, $v08,0
    spv $v00,0, 0*8,s4                    ;add s4,s1,dst_dma_width
    spv $v08,0, 0*8,s1;                   ;bgtz t0,Interpolate_CopyLoop
                                        addi t0, -2

    jr ra
    nop
    .endfunc


####################################################################
# OMX_Interpolate_Chroma_Overfill
#
# Interpolation of chroma subpixels during interprediction, with
# overfillling
####################################################################
# ARGS:
#   0: SRC: RDRAM address of the full source frame
#   1: SRC_PITCH: source buffer pitch (in bytes)
#   2: DST: RDRAM address of destination buffer
#   3: DST_PITCH: destination buffer pitch (in bytes)
#   4: FRAME_SIZE:
#      0-15: frame width
#      16-31: frame height
#   4: BLOCK_SIZE:
#      0-15: block width
#      16-31: block height
#   5: MV:
#      0-15: motion vector X (fixed 14.2)
#      16-31: motion vector Y (fixed 14.2)
#   6: POS:
#      0-15: source block pos X
#      16-31: source block pos Y
#   7: SRC2: RDRAM address of second full frame (second chroma component).
#   8: DST2: RDRAM address of destination buffer of second chroma component.
#

    .func OMX_Interpolate_Chroma_Overfill
OMX_Interpolate_Chroma_Overfill:
    jal Interpolate_Bootstrap
    li t0, %lo(InterChromaMVConst)

ChromaPostBootstrap:
    addi s0, src_unalign, %lo(MB_INPUT)
    jal Interpolate_Chroma
    addi s4, dst_unalign, %lo(MB_OUTPUT)

    jal Interpolate_ChromaLuma_DMADest
    li t2, DMA_OUT_ASYNC   # Direction: DMEM->RDRAM, background

    # Check if we need to do the second chroma component as well.
    lw t0, CMD_ADDR(32, 44)
    sw zero, CMD_ADDR(32, 44)
    beqz t0, RSPQ_Loop

    # Calculate updated src/dst RDRAM pointers, by applying the same offset
    # that was previously calculated for the first component.
    add src, t0

    lw t1, CMD_ADDR(36, 44)
    lw t0, CMD_ADDR(40, 44)
    sub t1, dst, t1
    add dst, t0, t1

    # Run only the phase 2 of bootstrap (dma input buffer + overfill), then
    # jump back to the beginning of this function.
    jal_and_j Interpolate_Bootstrap_Phase2, ChromaPostBootstrap
    .endfunc


    .func Interpolate_Chroma
Interpolate_Chroma:
    # DX==DY==0: no interpolation: just copy the input macroblock over
    # the output macroblock (respecting the destination mask
    # to avoid overwriting too many bytes).
    beqz fp, Interpolate_CopyMacroblock
    nop

    # Compute the filter coefficients and store them in $v28
    mtc2 dx, $v29,1*2
    mtc2 dx, $v29,3*2
    mtc2 dy, $v27,2*2
    mtc2 dy, $v27,3*2
    li t0,8
    sub t1,t0,dx
    sub t2,t0,dy
    mtc2 t1, $v29,0*2
    mtc2 t1, $v29,2*2
    mtc2 t2, $v27,0*2
    mtc2 t2, $v27,1*2

    # Interpolation loop. We don't need to loop horizontally
    # because we process 8 pixels at a time, and 8x8 is the
    # maximum size of a chroma macroblock (after 4:2:0 subsampling).
    #
    # Since interpolation applies a 4-tap filter on a 2x2 pixel block,
    # we process 2 lines each loop, so that each line is loaded in
    # memory only once. Before the loop, we load the first line once,
    # and then we will load 2 lines per loop, to achieve the maximum
    # parallelism in the loop between VU/SU. 

    # The original formula for each pixel is:
    #   ((VAL * COEFF) + 1<<5) >> 6
    #
    # Notice that we will need the pixel value to by left-shifted by 7 to
    # match the bits expected by luv/suv for memory transfers, so actually
    # what we want is:
    #   ((VAL * COEFF) + 1<<5) >> 6 << 7
    #   ((VAL * COEFF) + 1<<5) << 1
    #   ((VAL * COEFF) << 1) + (1<<6)
    #
    # Remember that we've pre-shifted VAL by 7, COEFF by 10,
    # and we've also prepared a register that contains 1<<11.
    #
    # vmadm will calculate accumulated sums of (A*B)>>16. So this will become:
    #   ((VAL << 7) * (COEFF << 10) + (1<<11)*(1<<11)) >> 16
    #   (((VAL * COEFF) << 17) + (1<<22)) >> 16
    #   ((VAL * COEFF) << 1) + (1<<6)
    # which is exactly the final result.

    ############################################################################
    #  VU OPCODES                                   SU OPCODES                 #
    ############################################################################

                                                   # Wait for DMA transfer
                                                   # and prepare for iteration
                                                   move ra2, ra
                                                   jal DMAWaitIdle
                                                   move t0, height

    # Calculate in $v28 the coefficients
    # for the interpolation:
    #  [0] = (8-dx) * (8-dy)   = [e8]
    #  [1] = dx * (8-dy)       = [e9]
    #  [2] = (8-dx) * dy       = [e10]             # Load first row as VAL<<7
    #  [3] = dx * dy           = [e11]             # in $v00 / $v01
    vmudh $v28, $v29, $v27,0;                      luv $v00,0, 0,s0

    # Zero $v29
    vxor $v29, $v29, $v29,0;                       add s1,s0,1                      

    # Shift coefficients left by 4. See
    # below for rationale
    vsll8 $v28, $v28, 10;                          luv $v01,0, 0,s1

    # vshift8[e12] contains 1<<11 (it's               
    # the shift vector). Create $v29               
    # that contains that value                     
    # in all lanes.                                
    vor $v29, $v29, vshift8,12;                    add s0,src_dma_width

    .align 3   # loops must be 64-bit aligned for full pairing
ChromaInterpolate:
    # Calculate interpolation on                   
    # first and second row                         # Load second row in $v02/$v03
    vmudm $v11, $v00, $v28,8;                      luv $v02,0, 0,s0
    vmadm $v11, $v01, $v28,9;                      add s1, s0, 1
    vmadm $v11, $v29, $v29,0;                      luv $v03,0, 0,s1
    vmadm $v11, $v02, $v28,10;                     add s0, src_dma_width
                                                   # Load third row in $v00/$v01
                                                   # (will be first row in the next loop)
    vmadm $v11, $v03, $v28,11;                     luv $v00,0, 0,s0

    # Calculate interpolation on
    # second and third row
    vmudm $v13, $v02, $v28,8;                      add s1,s0,1
    vmadm $v13, $v03, $v28,9;                      luv $v01,0, 0,s1
                                                   # Read the current pixel in the 
                                                   # two output rows in $v12/$v14
    vmadm $v13, $v29, $v29,0;                      luv $v12,0, 0,s4
    vmadm $v13, $v00, $v28,10;                     add s3, s4, dst_dma_width
    vmadm $v13, $v01, $v28,11;                     luv $v14,0, 0,s3

    # Merge the interpolation output to the
    # current output pixels (using the
    # precalculated mask in VCC) so that we
    # preserve existing pixels outside of
    # the macroblock.                              # Prepare for next loop
    vmrg $v11, $v12, $v11,0;                       add s0, src_dma_width
    vmrg $v13, $v14, $v13,0;                       addi t0, -2

                                                   # Write output pixels
                                                   suv $v11,0, 0,s4
                                                   suv $v13,0, 0,s3
    bnez t0, ChromaInterpolate
    add s4, s3, dst_dma_width

    jr ra2
    nop

	.endfunc


####################################################################
# OMX_Interpolate_Luma_Overfill
#
# Interpolation of luma subpixels during interprediction, with
# overfillling
####################################################################
# ARGS:
#   0: SRC: RDRAM address of the full source frame
#   1: SRC_PITCH: source buffer pitch (in bytes)
#   2: DST: RDRAM address of destination buffer
#   3: DST_PITCH: destination buffer pitch (in bytes)
#   4: FRAME_SIZE:
#      0-15: frame width
#      16-31: frame height
#   4: BLOCK_SIZE:
#      0-15: block width
#      16-31: block height
#   5: MV:
#      0-15: motion vector X (fixed 14.2)
#      16-31: motion vector Y (fixed 14.2)
#   6: POS:
#      0-15: source block pos X
#      0-16: source block pos Y

    .func OMX_Interpolate_Luma_Overfill
OMX_Interpolate_Luma_Overfill:
    jal Interpolate_Bootstrap
    li t0, %lo(InterLumaMVConst)

    .equ srchalfver, s6
    .equ srchalfhor, s7
LumaPostBootstrap:
    li t0,3
LumaSetupHalfVer:
    bne dx, t0, LumaSetupHalfHor
    addi srchalfver, src_unalign, %lo(MB_INPUT)
    addi srchalfver, 1

LumaSetupHalfHor:
    bne dy, t0, LumaSwitch
    addi srchalfhor, src_unalign, %lo(MB_INPUT)
    add srchalfhor, src_dma_width

LumaSwitch:
    addi s0, src_unalign, %lo(MB_INPUT)

    beqz fp, LumaCopy   # dx==0 && dy == 0 -> copy
    nop
    beqz dy, LumaDy0    # dy==0 => halfhor + average
    nop
    beqz dx, LumaDx0    # dx==0 => halver + average
    li t0, 2
    beq dx, t0, LumaDiag
    nop
    beq dy, t0, LumaDiag
    nop
 
LumaQuarter:
    # Interpolate half-hor into output,
    # then interpolate half-ver and average with output
    addi s2, srchalfhor, -2
    addi s3, dst_unalign, %lo(MB_OUTPUT)
    li s5, 1
    li t0, %lo(Interpolate5_AverageNo)
    jal_and_j Interpolate5, LumaInterpolateHalfVerAvgOutput

LumaDy0:
    # DX==1 || DX==3  => Average(dst, halfver) => dst
    andi t0, dx, 1
    beqz t0, LumaInterpolateHalfHor
    li t0, %lo(Interpolate5_AverageNo)

    move s4, srchalfver
    move s0, src_dma_width
    j LumaInterpolateHalfHor
    li t0, %lo(Interpolate5_AverageYes)

LumaDx0:    
    # DY==1 || DY==3  => Average(dst, halfhor) => dst
    andi t0, dy, 1
    beqz t0, LumaInterpolateHalfVer
    li t0, %lo(Interpolate5_AverageNo)

    move s4, srchalfhor
    move s0, src_dma_width
    j LumaInterpolateHalfVer
    li t0, %lo(Interpolate5_AverageYes)

LumaDiag:
    jal LumaInterpolateDiag
    addi s4, dst_unalign, %lo(MB_OUTPUT)

    # DX == 1 || DX == 3
    andi t0,dx,1
    bnez t0,LumaInterpolateHalfVerAvgOutput

    # DY == 1 || DY == 3
    andi t0,dy,1
    beqz t0,LumaEnd
    # fallthrough!

LumaInterpolateHalfHorAvgOutput:
    # Interpolate half-hor buffer and then average with output buffer
    addi s4, dst_unalign, %lo(MB_OUTPUT)
    move s0, dst_dma_width
    li t0, %lo(Interpolate5_AverageYes)

LumaInterpolateHalfHor:
    # Interpolate half-hor buffer. Average yes/no must be already
    # configured in t0.
    addi s2, srchalfhor, -2
    addi s3, dst_unalign, %lo(MB_OUTPUT)
    li s5, 1
    jal_and_j Interpolate5, LumaEnd

LumaInterpolateHalfVerAvgOutput:
    # Interpolate half-ver buffer and then average with output buffer
    addi s4, dst_unalign, %lo(MB_OUTPUT)
    move s0, dst_dma_width
    li t0, %lo(Interpolate5_AverageYes)

LumaInterpolateHalfVer:
    # Interpolate half-ver buffer. Average yes/no must be already
    # configured in t0.
    addi s3, dst_unalign, %lo(MB_OUTPUT)
    sub s2, srchalfver, src_dma_width
    sub s2, src_dma_width
    move s5, src_dma_width
    jal_and_j Interpolate5, LumaEnd


LumaCopy:
    addi s0, src_unalign, %lo(MB_INPUT)
    addi s4, dst_unalign, %lo(MB_OUTPUT)

    # Copy macroblock (up to 8x16)
    addi s2, s0, 8
    jal Interpolate_CopyMacroblock
    addi s3, s4, 8

    # If this is a 16x16 block, do a second copy (second 8x16 column)
    andi t0, width, 16
    beqz t0, LumaEnd
    move s0, s2
    move s4, s3
    jal_and_j Interpolate_CopyMacroblock, LumaEnd

LumaEnd:
    jal Interpolate_ChromaLuma_DMADest
    li t2, DMA_OUT_ASYNC   # Direction: DMEM->RDRAM

    # Check if we also need to interpolate the chroma buffers
    lw t0, CMD_ADDR(32, 44)
    lw src, CMD_ADDR(0, 44)
    beqz t0, RSPQ_Loop

    # t0 is chroma_src-luma_src
    #
    # We need to update pointers and scale all coordinates/sizes
    # by two because of chroma subsampling.
    # Adjust src/dst and pitches for chroma
    srl src_pitch, 1
    srl dst_pitch, 1
    add src, t0
    lw dst, CMD_ADDR(36, 44)

    # Scale back pointer offsets in preparation for the second chroma buffer
    # They will be used as offsets between chroma buffers, so they
    # must be four times smaller.
    srl t0, 2
    sw t0, CMD_ADDR(32, 44)

    # Scale frame size, block size, and position by 2.
    # We don't need to scale the motion vector.
    lw t0, CMD_ADDR(16, 44)  # frame size
    lw t1, CMD_ADDR(20, 44)  # block size
    lw t2, CMD_ADDR(28, 44)  # position
    srl t0, 1
    srl t1, 1
    srl t2, 1
    sw t0, CMD_ADDR(16, 44)
    sw t1, CMD_ADDR(20, 44)
    j OMX_Interpolate_Chroma_Overfill
    sw t2, CMD_ADDR(28, 44)

    .endfunc


####################################################################
# 5-tap interpolation of a macroblock, with optional average with
# another block.
#
# INPUT:
#    s2: input buffer (sizes: width / height / src_dma_width)
#    s3: output buffer (sizes: width / height / dst_dma_width)
#    s5: horizontal step between samples in input buffer to average
#        (it will be 1 for horizontal interpolation, or src_dma_width for
#        vertical interpolation).
#    t0: either %lo(Interpolate5_AverageYes) or %lo(Interpolate5_AverageNo)
#        to enable/disable average with another buffer.
#    s4: average buffer (sizes: width / height / s0)
#
# This function can be used to perform both horizontal and vertical
# interpolation, depending on the value of s5. Moreover, it can
# simultaneously average the result with a third buffer. This is useful
# because, depending on the input parameters, Luma interpolation is often
# made of both an interpolation and averaging step.
####################################################################

    .func Interpolate5
Interpolate5:
    # Load average coefficients
    lqv $v29,0, 0,t0

    # Read coefficients, pre-shifted by 4
    li t0, LUMA_FIR_COEFFS
    lqv $v28,0, 0,t0

    move ra2, ra
    jal DMAWaitIdle  # wait for input pixels to arrive
    addi t1, width, -8
    move ra, ra2

    # Since we process 8 pixels at a time, we either loop one column or two
    # (depending on macroblock width).
Interpolate5LoopCol:
    move t0, height
    move t2, s2
    sub t3, s3, dst_dma_width    # Subtract one because we will add one on each loop before using it
    move sp, s4

    ############################################################################
    #  VU OPCODES                                   SU OPCODES                 #
    ############################################################################

    .align 3
Interpolate5Loop:
    # 5-tap FIR filter, with coefficients in $v28, using
    # MACs with signed coefficients and unsigned clamping.
    #
    # This sequence does many things that we expect:
    #    * Signed multiplications (coefficients can be negative)
    #    * Clamping in [0,0x7FFF]. We need to clamp [0,0xFF], so
    #      half of it is already taken care of.
    #    * Correct last-bit rounding to the nearest (assuming the
    #      values are correctly pre-shifted)
    #
    # The result is shifted by 15, so we get the correct precision:
    #   (VAL<<7)*(COEFF<<3)>>15   =>  (VAL*COFF)>>5
    # 

                                                    # Load pixels into $v00/$v05
                                                    luv $v00,0, 0,t2
                                                    add s1, t2, s5
                                                    luv $v01,0, 0,s1
    vmulu $v10, $v00, $v28,8;                       add s1, s5
                                                    luv $v02,0, 0,s1         
    vmacu $v11, $v01, $v28,9;                       add s1, s5
                                                    luv $v03,0, 0,s1
    vmacu $v12, $v02, $v28,10;                      add s1, s5
                                                    luv $v04,0, 0,s1
    vmacu $v13, $v03, $v28,10;                      add s1, s5
                                                    luv $v05,0, 0,s1
                                                    # Load average buffer
    vmacu $v14, $v04, $v28,9;                       luv $v19,0, 0,sp
                                                    # Prepare for next loop
    vmacu $v15, $v05, $v28,8;                       add t3,dst_dma_width
    #
    #
    
    # Clamp against 255.                            
    vcl $v17, $v15, $v28,15;                        add t2,src_dma_width
    # (latency)
    # (latency)

    # Average calculation. The general formula for average is simply
    #         ((P + A) + 1) >> 1 
    #
    # where P is our interpolated pixel, and A is the pixel coming from
    # the average buffer. Notice that we want the final value to be
    # left-shifted by 8 to be able to store it with spv. So the actual
    # formula becomes:
    #         (P << 7) + (A << 7) + (1 << 7)
    #
    # A has been already loaded in $v19 with luv, so it's already <<7.
    # We thus need to do:
    #         ($v17 << 7) + $v19 + 0x80
    #
    # Remember that we simulate <<7 with a multiplication; so to do a MAC
    # sequence, we also multiply the $v19 by 1, so that it becomes:
    #         ($v17 * 0x80) + ($v19 * 0x1) + 0x80
    #
    # Since the caller might have requested to disable average calculation
    # (sometimes it's not required), we can keep the same code sequence with
    # different coefficients:
    #         ($v17 * 0x100) + ($v19 * 0x0) + 0x0
    # 
    # $v29 has been already loaded with either Interpolate5_AverageYes or
    # Interpolate5_AverageNo, which contains either set of coefficients
    # depending on which calculation we need to perform.

    # shift to <<8 (no average) or <<7
    # (average). Add average if any.                # Load output pixel
    vmudn $v18, $v17, $v29,8;                       lpv $v20,0, 0*8,t3
    vmadn $v18, $v19, $v29,9;                       add sp, s0
    #
    # Add rounding after average (if any).
    vaddc $v18, $v18, $v29,10;                      addi t0, -1

    # OK we're done! Merge it to the previous
    # output (using the precalculated mask in
    # VCC) and write it back.
    vmrg $v18, $v20, $v18,0;                        bgtz t0, Interpolate5Loop
    # (latency)
    #                                               # Store final value
                                                    spv $v18,0, 0,t3

    # Next column (if any)
    addi s2, 8
    addi s3, 8
    addi s4, 8
    bgtz t1, Interpolate5LoopCol
    addi t1, -8

    jr ra
    nop
    .endfunc

####################################################################
# LumaInterpolateDiag
#
# Interpolate half-pels diagonally
# INPUT:
#    s0: input buffer
#    s4: output buffer
####################################################################
    .func LumaInterpolateDiag
LumaInterpolateDiag:
    li t0, LUMA_FIR_COEFFS
    lqv $v29,0, 0,t0

    ########################################
    # 1. horizontal interpolation
    ########################################

    # MB_SCRATCH will store the 16 bpp intermediate results, so we
    # we need twice as many bytes. Store that doubled dest width in t2.
    li s3, %lo(MB_SCRATCH)
    sll t3, dst_dma_width, 1

    # Shift coefficients by 5; they were already pre-shifted by
    # 3, so now we get COEFF<<8.
    vsll $v28, $v29, 5

    # We need to start two lines earlier than where the actual
    # image start. These lines were over-read so they're available.
    sub s0, src_dma_width
    sub s0, src_dma_width
    move ra2, ra
    jal DMAWaitIdle  # wait for input pixel to finish arriving
    addi t0, width, -8
    addi t2, src_dma_width, -5

LumaDiagLoopCol:
    # Start one line before, because the loop will be
    # pre-incrementing the output buffer (for instruction
    # pairing reasons).
    sub s3, t3

    # Start two pixels before on each row (they were over-read)
    addi s2, s0, -2

    # Iteration count
    addi t1, height, 5                 
                       
    .align 3                 
LumaDiagLoop:
    # Compute the 5-tap filter but store the full intermediate results
    # without clamping nor rounding.
    #
    # ((VAL<<7) * (COEFF<<8)) >> 15 = VAL*COEFF (unshifted)
    #
    # We use vmulf/vmacf because they clamp at [0x8000,0x7FFF] (we never
    # hit that); also, the rounding is ignored because it affects a bit
    # lower than the actual precision, which is always zero.
    
                                        luv $v00,0, 0,s2
                                        addi s2,1
    vmulf $v10, $v00, $v28,8;           luv $v01,0, 0,s2
                                        addi s2,1
    vmacf $v11, $v01, $v28,9;           luv $v02,0, 0,s2
                                        addi s2,1
    vmacf $v12, $v02, $v28,10;          luv $v03,0, 0,s2
                                        addi s2,1
    vmacf $v13, $v03, $v28,10;          luv $v04,0, 0,s2
                                        addi s2,1
    vmacf $v14, $v04, $v28,9;           luv $v05,0, 0,s2
                                        add s2, t2        # jump to next row (input)
    vmacf $v15, $v05, $v28,8;           add s3, t3        # jump to next row (output)
                                        addi t1, -1
                                        bnez t1, LumaDiagLoop
                                        # Store the full 16-bit result
                                        sqv $v15,0, 0,s3
    

    # See if we need to iterate on the second column (16x16 macroblock)
    addi s0, 8
    li s3, %lo(MB_SCRATCH)+16
    bgtz t0, LumaDiagLoopCol
    addi t0, -8

    ########################################
    # 2. vertical interpolation
    ########################################

    # Shift coeffs by 2 to obtain COEFF<<5
    vsll $v28, $v29, 2

    li s2, %lo(MB_SCRATCH)

    addi t0, width, -8
LumaDiagLoop2Col:
    # Start one line before because the loop will pre-increment
    # s3 for pairing purposes.
    sub s3, s4, dst_dma_width
    move t1, height

                                    move s1, s2
                                    lqv $v00,0, 0,s1 
                                    add s1, t3
                                    lqv $v01,0, 0,s1 
                                    add s1, t3
                                    lqv $v02,0, 0,s1 
                                    add s1, t3
                                    lqv $v03,0, 0,s1 
                                    add s1, t3
                                    lqv $v04,0, 0,s1 
                                    add s1, t3
                                    lqv $v05,0, 0,s1
                                    add s2, t3
    .align 3                 
LumaDiagLoop2:
    # 5-tap filter:
    #   (VAL16 * (COEFF<<5)) >> 15   =   (VAL16*COEFF)>>10
    #
    # We also benefit from unsigned clamping and rounding here
    # (see LumaInterpolateHalfHor for more information).
    vmulu $v10, $v00, $v28,8;       move s1, s2
    vmacu $v11, $v01, $v28,9;       lqv $v00,0, 0,s1
    vmacu $v12, $v02, $v28,10;      add s1, t3
    vmacu $v13, $v03, $v28,10;      lqv $v01,0, 0,s1
    vmacu $v14, $v04, $v28,9;       add s1, t3
    vmacu $v15, $v05, $v28,8;       lqv $v02,0, 0,s1
                                    add s1, t3
                                    lqv $v03,0, 0,s1
                                    add s1, t3

    # Clamp at 255 (clamping at
    # 0 already performed).         # Load output pixel
    vcl $v17, $v15, $v29,15;        add s3, dst_dma_width
                                    lpv $v19,0, 0*8,s3

                                    # Continue loading next row
                                    lqv $v04,0, 0,s1
    vsll8 $v18, $v17, 8;            add s1, t3
                                    lqv $v05,0, 0,s1
                                    add s2, t3

    # Merge with output pixel
    vmrg $v18, $v19, $v18,0;        addi t1, -1

                                    bnez t1, LumaDiagLoop2
                                    spv $v18,0, 0,s3;


    addi s4, 8
    li s2, %lo(MB_SCRATCH)+16 
    bgtz t0, LumaDiagLoop2Col
    addi t0, -8

    jr ra2
    nop

    .endfunc

####################################################################

    #undef src
    #undef src_pitch
    #undef dst
    #undef dst_pitch
    #undef width
    #undef height
    #undef dx
    #undef dy
    #undef over_read_x
    #undef over_read_y
    #undef src_unalign
    #undef dst_unalign
    #undef src_dma_width
    #undef dst_dma_width
    #undef src_dma_height
    #undef dst_dma_height

####################################################################
####################################################################

####################################################################
#
#  RESIDUAL: CHROMA & LUMA DEQUANTIZATION AND IDCT
#
####################################################################

    #define src a0
    #define dst a1
    #define dst_pitch a2
    #define dc a3
    #define qp t9     // NOTE: this must match definition in rsph264_common.inc
    #define ac fp
    #define tcmask s7

    #define npart s5
    #define dst_dma_width v1

    # Fetch the destination buffer, and load arguments
    # INPUT:
    #   t0: WxH length in DMA register format (eg: 0xF00F for 16x16) 
    #   dst_pitch: must be pre-loaded with the RDRAM pitch
    .func ResidualBootstrap
ResidualBootstrap:
    lw qp, CMD_ADDR(16, 28)
    lw ac, CMD_ADDR(20, 28)
    li t2, DMA_IN # DMA direction: RDRAM->DMEM
    # fallthrough!
    .endfunc

    .func ResidualDma
ResidualDma:
    # We need to fetch the destination buffer. It is guaranteed
    # to be 4-byte aligned, but we need 8-byte alignment for DMA.
    # The width has been already augmented to include possible unalignment.
    move s0, dst
    li s4, %lo(MB_OUTPUT)
    j DMAExec
    move t1, dst_pitch
    .endfunc

    # Move back the destination buffer into RDRAM
    # INPUT:
    #   t0: WxH length in DMA register format (eg: 0xF00F for 16x16) 
    #   dst_pitch: must be pre-loaded with the RDRAM pitch
    .func ResidualEnd
ResidualEnd:
    jal ResidualDma
    li t2, DMA_OUT # DMA direction: DMEM->RDRAM

    jal_and_j UpdateDeltaBlockPointer, RSPQ_Loop
    .endfunc


####################################################################
# OMX_DequantTransformResidual
#
# Reconstruct 4x4 residual block, dequantize it, do iDCT and
# add it to the predicted data (motion compensation or intra prediction).
####################################################################
# ARGS:
#   0: SRC: RDRAM address of coefficient buffer
#   1: DST: RDRAM address of predicted (destination) buffer
#   2: DST_PITCH: destination buffer pitch (in bytes)
#   3: DC: DC coeffient (or -1 if it doesn't exist)
#   4: QP: quantization parameter:
#      Bit 0-7:  matrix to use
#      Bit 8-15: shift factor
#   5: AC: flag to indicate if non-zero AC coefficients exists

    .func OMX_DequantTransformResidual
OMX_DequantTransformResidual:
    # Process a 4x4 block, but fetch a 8x4 block in case there's
    # unalignment of the destination buffer (it is guaranteed to
    # be 4-byte aligned, but not 8-byte).
    li dst_dma_width, 8
    jal ResidualBootstrap
    li t0, DMA_SIZE(8,4)

    #vxor $v00, $v00, $v00,0
    beqz ac, TransformOnly
    vxor $v01, $v01, $v01,0

    jal UnpackNextDeltaBlock
    nop
    move tcmask, ac

    jal CalcDequantFactors
    move t0, qp
    jal Dequant4x4
    nop

TransformOnly:
    jal TransformResidual4x4CheckDC
    nop

    # DMA back into RDRAM
    j ResidualEnd
    li t0, DMA_SIZE(8,4)
    .endfunc

####################################################################
# OMX_TransformDequantLumaDC: removed (dead code, see header table).
# TransformDequantLumaDC is no longer assembled into this overlay.
####################################################################

#define dcdmasize  t8
#define dcfunc     t9

####################################################################
# OMX_TransformDequantChromaDC
#
# Reconstruct the 2x2 Chroma DC block from packed delta buffer. Save
# it within the internal CHROMA_DC structure, and DMA back into RDRAM.
#
#  a0: <unused>
#  a1: dst
#  a2: qp
####################################################################
    .func OMX_TransformDequantChromaDC
OMX_TransformDequantChromaDC:
    li s4, %lo(CHROMA_DC_0)
    li dcdmasize, DMA_SIZE(8,1)
    li dcfunc, %lo(TransformDequantChromaDC)
    # fallthrough!
    .endfunc


####################################################################
# TransformDequantAnyDC
#
# Common code for DC dequantization/transform for both Luma and Chroma
####################################################################

    .func TransformDequantAnyDC
TransformDequantAnyDC:
    jal CalcDequantFactors
    move t0, a2    # qp

    jal UnpackNextDeltaBlock
    nop

    jalr dcfunc
    move t1, a2    # qp

    # DMA back the buffer
    move t0, dcdmasize
    move s0, a1    # dst
    jal_and_j DMAOut, RSPQ_Loop
    .endfunc



####################################################################
# Task_ProcessLumaInterResidual
#
# Reconstruct residual luma 16x16 block, dequantize it, do iDCT and
# add it to the predicted data (motion compensation or intra prediction).
####################################################################

    .func Task_ProcessLumaInterResidual
Task_ProcessLumaInterResidual:
    # Process a 16x16 block, that is guaranteed to be aligned to
    # 8 byte. So we don't need to handle unalignment: always fetch
    # exactly 16x16.
    li dst_dma_width, 16
    jal ResidualBootstrap
    li t0, DMA_SIZE(16,16)

    jal CalcDequantFactors
    move t0, qp

    # Go through 16 4x4 partitions of the 16x16 block
    li npart, 0
LumaResidualLoop:
    # See if there's residual data to process in this 4x4 partition
    srlv t0, ac, npart
    andi t0, 1
    beqz t0, LumaDequantSkip
    nop

    jal UnpackNextDeltaBlock
    nop
    jal Dequant4x4
    nop

    # Load the offset within the 16x16 block of the current partition.
    lbu t0, %lo(LUMA_DEQUANT_INDEX)(npart)
    jal TransformResidual4x4CheckDC
    addi s4, t0, %lo(MB_OUTPUT)

LumaDequantSkip:
    bne npart, 15, LumaResidualLoop
    addi npart, 1

    # DMA back into RDRAM
    j ResidualEnd
    li t0, DMA_SIZE(16,16)

    .endfunc

####################################################################
# Task_ProcessChromaResidual
#
# Process the residual pixels for two 8x8 chroma blocks (Cr/Cb).
####################################################################
    .func Task_ProcessChromaResidual
Task_ProcessChromaResidual:
    li dst_dma_width, 8
    jal ResidualBootstrap
    li t0, DMA_SIZE(8,8)   # first chroma buffer

    # Now load second chroma buffer, consecutive to the first one
    lw s0, CMD_ADDR(24, 28)
    jal DMAIn
    li s4, %lo(MB_OUTPUT+64)

    move tcmask, ac

    jal CalcDequantFactors
    move t0, qp

    #vxor $v00, $v00, $v00,0
    li s4, %lo(CHROMA_DC_0)
    sqv $v00,0, 0,s4

ChromaDC0:
    # Check if there are quantized/transformed DC factors for the
    # first chroma block. If so, dequantize/transform them.
    srl t0, tcmask, 25
    andi t0, 1
    beqz t0, ChromaDC1
    nop
    jal UnpackNextDeltaBlock
    nop
    jal TransformDequantChromaDC
    li s4, %lo(CHROMA_DC_0)

ChromaDC1:
    # Check if there are quantized/transformed DC factors for the
    # second chroma block. If so, dequantize/transform them.
    srl t0, tcmask, 26
    andi t0, 1
    beqz t0, ChromaResidualLoopStart
    nop
    jal UnpackNextDeltaBlock
    nop
    jal TransformDequantChromaDC
    li s4, %lo(CHROMA_DC_1)

ChromaResidualLoopStart:
    # Main Chroma residual loop: go through each 4x4 partition
    # (in a specific order, mandated by CHROMA_DEQUANT_INDEX),
    # and dequantize/transform them. Use the previously-transformed
    # DC coefficients if existing.
    li s6, %lo(CHROMA_DC_0)
    li npart, 0
ChromaResidualLoop:
    vxor $v00, $v00, $v00,0
    lhu dc, 0(s6)
    vxor $v01, $v01, $v01,0
    addi s6, 2

    # AC=0 DC=0   => do nothing
    # AC=1 DC=0   => unpack, dequant, transform
    # AC=0 DC=1   => transform
    # AC=1 DC=1   => unpack, dequant, transform

    addi t1, npart, 16
    srlv t0, tcmask, t1
    andi ac, t0, 1
    beqz ac, ChromaResidualTransform
    nop

    jal UnpackNextDeltaBlock
    nop
    jal Dequant4x4
    nop

ChromaResidualTransform:
    or t0, dc, ac
    beqz t0, ChromaResidualSkip
    lbu t0, %lo(CHROMA_DEQUANT_INDEX)(npart)

    jal TransformResidual4x4CheckDC
    addi s4, t0, %lo(MB_OUTPUT)

ChromaResidualSkip:
    bne npart, 7, ChromaResidualLoop
    addi npart, 1

    # DMA back second chroma buffer
    lw s0, CMD_ADDR(24, 28)
    li t0, DMA_SIZE(8,8)
    li s4, %lo(MB_OUTPUT+64)
    move t1, dst_pitch
    jal_and_j DMAOut, ResidualEnd

    .endfunc


####################################################################
# TransformDequantChromaDC
#
# Reconstruct a 2x2 Chroma DC block from a packed block of coefficients. 
# Do a 2x2 inverse Hadamard transformation, followed by a quantization
# step.
#
# This is exposed as OMX_TransformDequantChromaDC, and used internally
# by Task_ProcessChromaResidual.
####################################################################
#
# ARGS:
#  v00: unpacked coefficients (as generated by UnpackNextDeltaBlock).
#       Notice that only the first 4 lanes are used (2x2 block).
#   s4: output buffer where the 4 coefficients should be written to.

    .func TransformDequantChromaDC
TransformDequantChromaDC:
    vsll $v00, $v00, 1

    li t0, %lo(IHT_DC_COEFF)
    lpv $v04,0, 0*8,t0
    lpv $v05,0, 1*8,t0
    lpv $v06,0, 2*8,t0
    lpv $v07,0, 3*8,t0
    lqv $v08,0, 3*16,t0
    vmulf $v02, $v04, $v00,8
    vmacf $v02, $v05, $v00,9
    vmacf $v02, $v06, $v00,10
    vmacf $v00, $v07, $v00,11

    # Dequantize using single multiplication factor
    vmudn $v00, $v00, $v27,8

    # Use shift factor, reduced by 1 (so just shift right afterwards)
    vmudn $v00, $v00, $v29,8
    vsra $v00, $v00, 1

    # Mask out lanes 4-7. When this function is called for the
    # first 2x2 block, we want to write 0 in the space for the
    # second block (because the second block might not exist,
    # and zero is the default).
    # When called on the second block, only 4 lanes are written
    # because the address is unaligned, so the value of those lanes
    # doesn't really matter.
    vand $v00, $v00, $v08,0
    jr ra
    sqv $v00,0, 0,s4
    .endfunc


####################################################################
# TransformResidual4x4CheckDC
#
# Calls TransformResidual4x4IDCT, but before processes DC in case
# it was set by the caller.
#
####################################################################
# ARGS:
#       s4:  address of predicted pixel buffer in DMEM
#  v00/v01:  unpacked delta coefficients: 4x4 block of 16-bit values
#            (calculated by UnpackNextDeltaBlock)

    .func TransformResidual4x4CheckDC
TransformResidual4x4CheckDC:
    # If there's no fixed DC factor, just jump to the actual IDCT transformation.
    bltz dc, TransformResidual4x4IDCT
    move t4, dst_dma_width    # buffer width is expected in t4

    # Overwrite the first delta coefficient (v00[e8]) with DC, and then
    # execute the transform.
    j TransformResidual4x4IDCT
    mtc2 dc, $v00,0
    .endfunc

####################################################################

    #undef src
    #undef dst
    #undef dst_pitch
    #undef dc
    #undef qp
    #undef ac
    #undef tcmask
    #undef npart
    #undef dst_dma_width
    #undef ac16
