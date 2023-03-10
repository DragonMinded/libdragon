###################################################################
#
# Example RSPQ overlay that does pixel processing on the RSP.
#
###################################################################

# All rspq overlays must include rspq_queue.inc
#include <rsp_queue.inc>

# This is an internal header to share some constants between the C code and the assembly.
#include "rsp_blend_constants.h"

    .set noreorder
    .set at

    .data

# Standard RSPQ command table. This defines the entrypoints that can be called from C.
# In this case, we define two commands. The number "8" refers to the number of bytes the
# commands is made of. Currently, rspq only allow commands to be specified in multipe of
# 32-bit words, so this nunber is always expected to be a multiple of 4.
#
# The commands are:
#   * BlendCmd_SetSource: this is used to configure the rspq overlay with the pointer to
#     source texture to blend (the lens flare)
#   * BlendCmd_Add: this is used to blend the previously_configured source texture into
#     the destination texture.
#
# Notice that this is just an example; we could have used just one commmand receiving
# both source and destination pointers, or three commands (source, dest, process). There
# would be little differences anyway.
#
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand BlendCmd_SetSource,   8  # 0x0
    RSPQ_DefineCommand BlendCmd_ProcessLine, 4  # 0x1
RSPQ_EndOverlayHeader

# Standard RSPQ DMEM state. This block must contain all variables that must save
# their value *between* calls to the overlay, that is between different commands.
# In general, anytime the overlay is swapped out and then back in, the data segment
# is restored ot its initial value. The only exception is this saved state, which is
# saved into RDRAM and then restored in DMEM anytime the overlay is swapped.
RSPQ_BeginSavedState
SRC_RDRAM:   .long  0     # Pointer to the source image in RDRAM
SRC_WIDTH:   .half  0     # Source image width in bytes
SRC_HEIGHT:  .half  0     # Source image height
RSPQ_EndSavedState

    .balign 16
VCONST:     .half 0x1F << 10    # 5-bit mask in bits 14..10
            .half 0             # Padding
            .half 0
            .half 0
            .half 0
            .half 0
            .half 0
            .half 0

    # The BSS segment holds all uniniitialized memory buffers
    # Notice that the contents of this variable is *random* (not zero).
    # RSP code should never expect these to be zero-initialized.
    .bss

    .balign 8    # Buffers accessed via DMA must be 8-byte aligned
SRC_BUF:    .dcb.b  640*2       # Source image buffer
DST_BUF:    .dcb.b  640*2+8     # Dest image buffer (+ 8 bytes to handle misalignment of RDRAM pointer)

    .text

    #######################################
    # BlendCmd_SetSource
    #
    # Set source image pointer and size
    #
    # Input:
    #   a0 = pointer to source image in RDRAM
    #   a1 = source image width (top 16 bits) + height (bottom 16 bits)
    #
    #######################################
    .func BlendCmd_SetSource
BlendCmd_SetSource:
    # Extract width and check if it's a multiple of 8
    srl t0, a1, 16
    andi t1, t0, 0x7
    assert_eq t1, 0, ASSERT_INVALID_WIDTH
    li t1, 640
    assert_le t0, t1, ASSERT_INVALID_WIDTH

    # Multiply width by 2 (16-bit pixels)
    sll t0, 1

    sh t0, %lo(SRC_WIDTH)   # Save source width
    sh a1, %lo(SRC_HEIGHT)  # Save source height
    jr ra
    sw a0, %lo(SRC_RDRAM)   # Save source pointer
    .endfunc


    .func BlendCmd_ProcessLine

    # Register allocation: define aliases for readability
    #define width   t7
    #define src_ptr s3
    #define dst_ptr s4
    
    #define vsrc16   $v01
    #define vsrc_r   $v02
    #define vsrc_g   $v03
    #define vsrc_b   $v04
    #define vdst16   $v05
    #define vdst_r   $v06
    #define vdst_g   $v07
    #define vdst_b   $v08

    #define vconst         $v29
    #define k_color_mask   vconst.e0

BlendCmd_ProcessLine:

    # Fetch source buffer into DMEM. Notice that we use to async version
    # of DMAIn here as we don't need to wait for the DMA to complete, we
    # can just continue.
    lw s0, %lo(SRC_RDRAM)
    li s4, %lo(SRC_BUF)
    lh t0, %lo(SRC_WIDTH)
    jal DMAInAsync
    addi t0, -1

    # Fetch destination buffer into DMEM. This is a sync version that will
    # wait for this (and the previous!) DMA to complete.
    # NOTE: the RDRAM pointer might be misaligned (RSP DMA requires 8-byte alignment).
    # DMAIn will automatically adjust the pointer to the previous 8-byte boundary, but
    # we need to fetch 8 bytes more to make sure the correct pixels are fetched.
    move s0, a0
    li s4, %lo(DST_BUF)
    lh t0, %lo(SRC_WIDTH)
    jal DMAIn
    addi t0, +8-1

    # Iniitalize src_ptr and width.
    # Notice that after the previous DMAIn, dst_ptr (s4) already points
    # to the correct first pixel in DMEM (as DMAIn adjusts it to match the
    # RDRAM misalignment). See DMAIn documentation for details.
    li src_ptr, %lo(SRC_BUF)
    lh width, %lo(SRC_WIDTH)

    li s0, %lo(VCONST)   # Read constants used below
    lqv vconst, 0,s0

line_loop:
    # Fetch 8 16-bit pixels from source and dest buffer. dest buffer in DMEM might be
    # misaligned, so we need the lqv/lrv sequence to fully load all the pixels
    lqv vsrc16,  0,src_ptr
    lqv vdst16,  0,dst_ptr
    lrv vdst16, 16,dst_ptr

    # Isolate R,G,B components. Since we want to handle saturation during addition,
    # we need to scale each component into a 16-bit signed integer, which means
    # that the 5-bit components must go into bits 14..10. Thus:
    # 
    #  R is at 15..11, so it must be shift right by 1
    #  G is at 10..6, so it must be shift left by 4
    #  B is at 5..1, so it must be shift left by 9
    vsrl  vsrc_r, vsrc16, 1
    vsll  vsrc_g, vsrc16, 4
    vsll8 vsrc_b, vsrc16, 9

    # Keep only correct bits
    vand vsrc_r, k_color_mask
    vand vsrc_g, k_color_mask
    vand vsrc_b, k_color_mask

    # Now do the same for destination pixels    
    vsrl  vdst_r, vdst16, 1
    vsll  vdst_g, vdst16, 4
    vsll8 vdst_b, vdst16, 9
    vand vdst_r, k_color_mask
    vand vdst_g, k_color_mask
    vand vdst_b, k_color_mask

    # Add source and destination pixels. VADD performs saturation automatically.
    vadd vdst_r, vsrc_r
    vadd vdst_g, vsrc_g
    vadd vdst_b, vsrc_b

    # Now we need to pack the result back into 16-bit pixels. We reverse the shifts
    vsll  vdst_r, vdst_r, 1
    vsrl  vdst_g, vdst_g, 4
    vsrl8 vdst_b, vdst_b, 9
    vor vdst16, vdst_r, vdst_g
    vor vdst16, vdst_b

    # Store result (again, could be misaligned)
    sqv vdst16,  0,dst_ptr
    srv vdst16, 16,dst_ptr

    # Increment pointers
    addi src_ptr, 8*2
    addi dst_ptr, 8*2

    # 8 pixels done, check if we're done
    addi width, -8
    bgez width, line_loop
    nop

    # Now DMA back into RDRAM
    move s0, a0
    li s4, %lo(DST_BUF)
    lh t0, %lo(SRC_WIDTH)
    jal DMAOut
    addi t0, +8-1

    # Increment line pointer of the source image. Next time we're called,
    # we'll fetch the next line.
    lw t0, %lo(SRC_RDRAM)
    lh t1, %lo(SRC_WIDTH)
    add t0, t1
    sw t0, %lo(SRC_RDRAM)

    # Done! Go back to main loop
    # NOTE: we can't do "jr ra" here as "ra" was discarded by the previous DMA function calls.
    # Jumping to RSPQ_Loop is the standard way to finish an overlay command, in these cases.
    j RSPQ_Loop 
    nop

    .endfunc
