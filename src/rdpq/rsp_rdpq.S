#include <rsp_queue.inc>
#include "rdpq_constants.h"

#define rdpq_write_ptr s7

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xC0 NOOP
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          32  # 0xC8 Filled
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          48  # 0xC9 Filled ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          96  # 0xCA Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          112 # 0xCB Textured ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          96  # 0xCC Shaded
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          112 # 0xCD Shaded ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          160 # 0xCE Shaded Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,          176 # 0xCF Shaded Textured ZBuffered

        RSPQ_DefineCommand RDPQCmd_TextureRectEx,           16  # 0xD0
        RSPQ_DefineCommand RDPQCmd_TextureRectEx_Static,    16  # 0xD1
        RSPQ_DefineCommand RDPQCmd_SetScissorEx,            8   # 0xD2
        RSPQ_DefineCommand RDPQCmd_SetScissorEx_Static,     8   # 0xD3
        RSPQ_DefineCommand RDPQCmd_ModifyOtherModes,        12  # 0xD4
        RSPQ_DefineCommand RDPQCmd_ModifyOtherModes_Static, 12  # 0xD5
        RSPQ_DefineCommand RDPQCmd_SetFillColor32,          8   # 0xD6
        RSPQ_DefineCommand RDPQCmd_SetFillColor32_Static,   8   # 0xD7
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_SetColorImage_Static,    8   # 0xDF

        RSPQ_DefineCommand RDPQCmd_SetOtherModes_Static,    8   # 0xE0
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_Passthrough16,           16  # 0xE4 TEXTURE_RECTANGLE
        RSPQ_DefineCommand RDPQCmd_TextureRectFlip,         16  # 0xE5 TEXTURE_RECTANGLE_FLIP
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE6 SYNC_LOAD
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE7 SYNC_PIPE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE8 SYNC_TILE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE9 SYNC_FULL
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEA SET_KEY_GB
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEB SET_KEY_R
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEC SET_CONVERT
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xED SET_SCISSOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEE SET_PRIM_DEPTH
        RSPQ_DefineCommand RDPQCmd_SetOtherModes,           8   # 0xEF SET_OTHER_MODES
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF0 LOAD_TLUT
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF2 SET_TILE_SIZE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF3 LOAD_BLOCK
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF4 LOAD_TILE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF5 SET_TILE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF6 FILL_RECTANGLE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF7 SET_FILL_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF8 SET_FOG_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF9 SET_BLEND_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFA SET_PRIM_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFB SET_ENV_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFC SET_COMBINE_MODE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFD SET_TEXTURE_IMAGE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFE SET_Z_IMAGE
        RSPQ_DefineCommand RDPQCmd_SetColorImage,           8   # 0xFF SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
RDP_OTHER_MODES:       .quad  0
RDP_SCISSOR_RECT:      .quad  0
RDP_FILL_COLOR:        .word  0
RDP_TARGET_BITDEPTH:   .byte  0
    RSPQ_EndSavedState

    .bss

    .align 4
    # Enough for a full triangle command
RDP_CMD_STAGING:       .ds.b 0xB0

    .text

    #############################################################
    # RDPQ_Finalize
    #
    # Submits the RDP command(s) in RDP_CMD_STAGING to the RDP.
    #############################################################
    .func RDPQ_Finalize
RDPQ_Finalize:
    j RSPQ_RdpFinalize
    li s4, %lo(RDP_CMD_STAGING)
    .endfunc

    #############################################################
    # RDPQCmd_Passthrough8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough8
RDPQCmd_Passthrough8:
    jal RSPQ_RdpDynamicReserve
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    li ra, %lo(RDPQ_Finalize)
    # fallthrough
    .endfunc

    #############################################################
    # RDPQ_Write8
    #
    # Appends 8 bytes from a0-a1 to the staging area (RDP_CMD_STAGING).
    #############################################################
    .func RDPQ_Write8
RDPQ_Write8:
    sw a0, 0x00(rdpq_write_ptr)
    sw a1, 0x04(rdpq_write_ptr)
    jr ra
    addi rdpq_write_ptr, 8
    .endfunc

    #############################################################
    # RDPQCmd_TextureRectFlip
    #
    # Falls through to RDPQCmd_Passthrough16, but asserts that copy
    # mode is not active (only in debug build).
    #############################################################
    .func RDPQCmd_TextureRectFlip
RDPQCmd_TextureRectFlip:
#ifndef NDEBUG
    lb t0, %lo(RDP_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t0, 0x1 << 5
    assert_eq t0, 0, RDPQ_ASSERT_FLIP_COPY
#endif
    # fallthrough!
    .endfunc

    #############################################################
    # RDPQCmd_Passthrough16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough16
RDPQCmd_Passthrough16:
    jal RSPQ_RdpDynamicReserve
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    li ra, %lo(RDPQ_Finalize)
    # fallthrough!
    .endfunc

    #############################################################
    # RDPQ_Write16
    #
    # Appends 16 bytes from a0-a3 to the staging area (RDP_CMD_STAGING).
    #############################################################
    .func RDPQ_Write16
RDPQ_Write16:
    sw a0, 0x00(rdpq_write_ptr)
    sw a1, 0x04(rdpq_write_ptr)
    sw a2, 0x08(rdpq_write_ptr)
    sw a3, 0x0C(rdpq_write_ptr)
    jr ra
    addi rdpq_write_ptr, 16
    .endfunc

    #############################################################
    # RDPQCmd_SetOtherModes
    #
    # Completely overwrites the internal cache of the RDP other modes with the 
    # values provided in a0 and a1 and submits the command to the RDP.
    # It will also re-evaluate any commands that depend on the other modes
    # and submit them at the same time.
    #
    # ARGS:
    #   a0: Command id and upper word of other modes
    #   a1: Lower word of other modes
    #############################################################
    .func RDPQCmd_SetOtherModes
RDPQCmd_SetOtherModes:
    # Reserve space for SetOtherModes + SetScissor
    jal RSPQ_RdpDynamicReserve
    li rspq_cmd_size, 16
    # Save the other modes to internal cache, then call RDPQ_WriteOtherModes
    # RDPQ_WriteOtherModes will write both SetOtherModes and SetScissor to the staging area
    jal_and_j RDPQ_SaveOtherModes, RDPQ_WriteOtherModes
    .endfunc

    #############################################################
    # RDPQCmd_SetOtherModes_Static
    #
    # This is the static mode version of #RDPQCmd_SetOtherModes.
    # It will also save the other modes to the internal cache, but doesn't
    # submit the command to the static buffer because it already got written on the CPU side.
    # It will however submit commands that depend on the other modes, just like the dynamic 
    # mode version does.
    #
    # ARGS:
    #   a0: Command id and upper word of other modes
    #   a1: Lower word of other modes
    #############################################################
    .func RDPQCmd_SetOtherModes_Static
RDPQCmd_SetOtherModes_Static:
    # This will fall through to RDPQ_SaveOtherModes first, and then call RDPQ_FinalizeOtherModes.
    # We don't want to jump to RDPQ_WriteOtherModes in this case, because the SetOtherModes command
    # is already in the static buffer!
    li ra, %lo(RDPQ_FinalizeOtherModes)
    # fallthrough!
    .endfunc

    #############################################################
    # RDPQ_SaveOtherModes
    #
    # Saves the other mode command in a0-a1 to an internal cache.
    #
    # ARGS:
    #   a0: Command id and upper word of other modes
    #   a1: Lower word of other modes
    #############################################################
    .func RDPQ_SaveOtherModes
RDPQ_SaveOtherModes:
    lui t0, 0xEF00
    or a0, t0
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    jr ra
    sw a1, %lo(RDP_OTHER_MODES) + 0x4
    .endfunc

    #############################################################
    # RDPQCmd_ModifyOtherModes
    #
    # Modifies a specific part of the other modes and sends the updated value to the RDP.
    # NOTE: The new value in a2 should never have bits set outside of
    #       the inverse bitmask to ensure correct results.
    #
    # ARGS:
    #   a0: Command id and word offset into other modes (0 or 4)
    #   a1: Inverse bit mask of the value to be written
    #   a2: New value
    #############################################################
    .func RDPQCmd_ModifyOtherModes
RDPQCmd_ModifyOtherModes:
    # Only need to reserve space in dynamic mode. In static mode, DP_END already points to the right location.
    jal RSPQ_RdpDynamicReserve
RDPQCmd_ModifyOtherModes_Static:
    li rspq_cmd_size, 16
    lw t1, %lo(RDP_OTHER_MODES)(a0)
    and t1, a1
    or t1, a2
    sw t1, %lo(RDP_OTHER_MODES)(a0)

    # Prepare the updated command in a0-a1 for the following steps
    lw a0, %lo(RDP_OTHER_MODES) + 0x0
    lw a1, %lo(RDP_OTHER_MODES) + 0x4
    # fallthrough
    .endfunc

    #############################################################
    # RDPQ_WriteOtherModes
    #
    # Appends the other modes command in a0-a1 to the staging area
    # and falls through to #RDPQ_FinalizeOtherModes.
    #############################################################
    .func RDPQ_WriteOtherModes
RDPQ_WriteOtherModes:
    # Write other modes command to staging area
    jal RDPQ_Write8
    # fallthrough and delay slot!
    .endfunc

    #############################################################
    # RDPQ_FinalizeOtherModes
    #
    # Re-evaluates any commands that depend on the other modes,
    # appends them to the staging area, and finally calls #RDPQ_Finalize,
    # finishing the current command.
    #############################################################
    .func RDPQ_FinalizeOtherModes
RDPQ_FinalizeOtherModes:
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)

    # Update other commands that need to change some state depending on the other modes

    # SetScissor:
    # load the cached command first
    lw a0, %lo(RDP_SCISSOR_RECT) + 0x0
    lw a1, %lo(RDP_SCISSOR_RECT) + 0x4
    # Append the fixed up SetScissor command to staging area and then finalize
    jal_and_j RDPQ_WriteSetScissor, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_SetColorImage
    #
    # Simple wrapper around RDP Set Color Image, that records
    # the target bitdepth. Will also re-execute any fixup commands
    # that depend on the bitdepth.
    #############################################################
    .func RDPQCmd_SetColorImage
RDPQCmd_SetColorImage:
    # Only need to reserve space in dynamic mode. In static mode, DP_END already points to the right location.
    jal RSPQ_RdpDynamicReserve
RDPQCmd_SetColorImage_Static:
    li rspq_cmd_size, 16
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    srl t0, a0, 19
    andi t0, 3
    sb t0, %lo(RDP_TARGET_BITDEPTH)
    lui t1, 0xFF00
    # Append this command to staging area
    jal RDPQ_Write8
    or a0, t1

    # Append SetFillColor next, then submit everything
    lw a1, %lo(RDP_FILL_COLOR)
    jal_and_j RDPQ_WriteSetFillColor, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_SetFillColor32
    #
    # The RDP command SetFillColor expects a 32-bit value which
    # is a "packed color", that is the 32-bit value that must be
    # blindly repeated in the framebuffer. Semantically, for 32-bit
    # framebuffers, this is the standard RGBA8888 format. For 16-bit
    # framebuffers, it must be RGBA5551 repeated two times.
    #
    # To allow a more flexible approach where the same fill color
    # command can be used irrespective of the target framebuffer, 
    # we create our own SetFillColor32 that only accepts a 
    # RGBA8888 color but convert it automatically to RGBA5551
    # depending on the target bitdepth (using the last value stored
    # by SetColorImage).
    #############################################################
    .func RDPQCmd_SetFillColor32
RDPQCmd_SetFillColor32:
    # Only need to reserve space in dynamic mode. In static mode, DP_END already points to the right location.
    jal RSPQ_RdpDynamicReserve
RDPQCmd_SetFillColor32_Static:
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    sw a1, %lo(RDP_FILL_COLOR)
    li ra, %lo(RDPQ_Finalize)

RDPQ_WriteSetFillColor:
    lbu t0, %lo(RDP_TARGET_BITDEPTH)
    beq t0, 3, RDPQ_Write8
    lui a0, 0xF700   # SET_FILL_COLOR
    srl t0, a1, 24 + (8-5) - 11
    srl t1, a1, 16 + (8-5) - 6
    srl t2, a1, 8  + (8-5) - 1
    srl t3, a1, 0  + (8-1) - 0
    andi t0, 0x1F << 11
    andi t1, 0x1F << 6
    andi t2, 0x1F << 1
    andi t3, 0x01 << 0
    or t4, t0, t1
    or t5, t2, t3
    or a1, t4, t5
    sll t0, a1, 16
    j RDPQ_Write8
    or a1, t0
    .endfunc

    #############################################################
    # RDPQCmd_SetScissorEx
    #
    # The RDP command SetScissor has slightly different behavior
    # for rectangles depending on the current cycle mode. In 1cycle/2cycle 
    # mode the right edge works as an "exclusive" range, meaning
    # that pixels with the same x-coordinate as the edge are not
    # rendered to. In fill/copy mode on the other hand, it's an inclusive
    # range, so one additional column of pixels is included.
    #
    # To make the API more consistent across all cycle modes, this
    # command will adjust the coordinate of the right edge to compensate
    # for this behavior depending on the current cycle mode.
    #############################################################
    .func RDPQCmd_SetScissorEx
RDPQCmd_SetScissorEx:
    lui t1, 0xD200 ^ 0xD300 # SetScissorEx -> SetScissorEx_Static
    xor a0, t1
    # Only need to reserve space in dynamic mode. In static mode, DP_END already points to the right location.
    jal RSPQ_RdpDynamicReserve
RDPQCmd_SetScissorEx_Static:
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    sw a0, %lo(RDP_SCISSOR_RECT) + 0x0
    sw a1, %lo(RDP_SCISSOR_RECT) + 0x4
    li ra, %lo(RDPQ_Finalize)

RDPQ_WriteSetScissor:
    lb t6, %lo(RDP_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t6, 0x1 << 5
    # Leave unchanged when not in FILL or COPY mode
    beqz t6, scissor_substitute
    lui t1, 0xED00 ^ 0xD300 # SetScissorEx_Static -> SET_SCISSOR

    # Subtract 1 subpixel from XL (bits 23:12, as 10.2 fixed point)
    addiu a1, -(1 << 12)

scissor_substitute:
    # Substitute command ID
    j RDPQ_Write8
    xor a0, t1
    .endfunc

    #############################################################
    # RDPQCmd_TextureRectEx
    #
    # Provides a consistent API for the TextureRectangle command
    # that always uses exclusive ranges across all cycle modes.
    #############################################################
    .func RDPQCmd_TextureRectEx
RDPQCmd_TextureRectEx:
    lui t1, 0xD000 ^ 0xD100 # TextureRectEx -> TextureRectEx_Static
    xor a0, t1
    # Only need to reserve space in dynamic mode. In static mode, DP_END already points to the right location.
    jal RSPQ_RdpDynamicReserve
RDPQCmd_TextureRectEx_Static:
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    lb t0, %lo(RDP_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t0, 0x1 << 5
    # Leave unchanged when not in FILL or COPY mode
    beqz t0, rect_substitute
    lui t1, 0xE400 ^ 0xD100 # TextureRectEx_Static -> TEXTURE_RECTANGLE

    # Subtract 1 pixel from XL and YL
    addiu a0, -((4 << 12) + 4)
    # Multiply DsDx by 4
    lui t0, 0xFFFF
    and t2, a3, t0
    sll t2, 2
    andi a3, 0xFFFF
    or a3, t2

rect_substitute:
    # Substitute command ID
    xor a0, t1
    jal_and_j RDPQ_Write16, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_PassthroughBig
    # 
    # Forwards the RDP command in the input stream to the RDP stream.
    # The size of the command is automatically detected by reading #rspq_cmd_size
    #############################################################
    .func RDPQCmd_PassthroughBig
RDPQCmd_PassthroughBig:
    jal RSPQ_RdpDynamicReserve
    sub s5, rspq_dmem_buf_ptr, rspq_cmd_size
    addi s5, %lo(RSPQ_DMEM_BUFFER)
    li s6, %lo(RDP_CMD_STAGING)
passthrough_copy_loop:
    lqv $v00,0, 0x00,s5
    lrv $v00,0, 0x10,s5
    sqv $v00,0, 0x00,s6
    addi s5, 0x10
    addi t1, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER)
    blt s5, t1, passthrough_copy_loop
    addi s6, 0x10

    j RDPQ_Finalize
    nop
    .endfunc
