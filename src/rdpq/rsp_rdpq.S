#include <rsp_queue.inc>

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8   # 0xC0 NOOP
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8   # Invalid
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   32  # 0xC8 Filled
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   48  # 0xC9 Filled ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   96  # 0xCA Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   112 # 0xCB Textured ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   96  # 0xCC Shaded
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   112 # 0xCD Shaded ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   160 # 0xCE Shaded Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughBig,   176 # 0xCF Shaded Textured ZBuffered

        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid

        RSPQ_DefineCommand RDPQCmd_ModifyOtherModes, 12 # 0xE0
        RSPQ_DefineCommand RDPQCmd_SetFillColor32,   8  # 0xE1 Set Fill Color (32bit version)
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RDPQCmd_SetScissorEx,     8  # 0xE3 Set Scissor (exclusive range always version)
        RSPQ_DefineCommand RDPQCmd_Passthrough16,    16 # 0xE4 TEXTURE_RECTANGLE
        RSPQ_DefineCommand RDPQCmd_Passthrough16,    16 # 0xE5 TEXTURE_RECTANGLE_FLIP
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xE6 SYNC_LOAD
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xE7 SYNC_PIPE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xE8 SYNC_TILE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xE9 SYNC_FULL
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xEA SET_KEY_GB
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xEB SET_KEY_R
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xEC SET_CONVERT
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xED SET_SCISSOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xEE SET_PRIM_DEPTH
        RSPQ_DefineCommand RDPQCmd_SetOtherModes,    8  # 0xEF SET_OTHER_MODES
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF0 LOAD_TLUT
        RSPQ_DefineCommand RSPQCmd_Noop,             8  # Invalid
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF2 SET_TILE_SIZE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF3 LOAD_BLOCK
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF4 LOAD_TILE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF5 SET_TILE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF6 FILL_RECTANGLE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF7 SET_FILL_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF8 SET_FOG_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xF9 SET_BLEND_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xFA SET_PRIM_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xFB SET_ENV_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xFC SET_COMBINE_MODE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xFD SET_TEXTURE_IMAGE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,     8  # 0xFE SET_Z_IMAGE
        RSPQ_DefineCommand RDPQCmd_SetColorImage,    8  # 0xFF SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
RDP_OTHER_MODES:       .quad  0xEF00000000000000
RDP_TARGET_BITDEPTH:   .byte  0
    RSPQ_EndSavedState

    .bss

    .align 4
    # Enough for a full triangle command
RDP_CMD_STAGING:       .ds.b 0xB0

    .text

    #############################################################
    # RDPQCmd_SetOtherModes
    #
    # Completely ovewrites the internal cache of the RDP other modes with the 
    # values provided in a0 and a1 and sends it to the RDP.
    #
    # ARGS:
    #   a0: Command id and upper word of other modes
    #   a1: Lower word of other modes
    #############################################################
    .func RDPQCmd_SetOtherModes
RDPQCmd_SetOtherModes:
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    j RDPQ_SendOtherModes
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
    lw t1, %lo(RDP_OTHER_MODES)(a0)
    and t1, a1
    or t1, a2
    sw t1, %lo(RDP_OTHER_MODES)(a0)
    # Need to override command size here since RDPQCmd_ModifyOtherModes is 12 bytes
    li rspq_cmd_size, 8
    .endfunc

    #############################################################
    # RDPQ_SendOtherModes
    #
    # Sends the value in RDP_OTHER_MODES to the RDP.
    #############################################################
    .func RDPQ_SendOtherModes
RDPQ_SendOtherModes:
    li s4, %lo(RDP_OTHER_MODES)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc

    #############################################################
    # RDPQCmd_SetColorImage
    #
    # Simple wrapper around RDP Set Color Image, that records
    # the target bitdepth.
    #############################################################
    .func RDPQCmd_SetColorImage
RDPQCmd_SetColorImage:
    srl t0, a0, 19
    andi t0, 3
    j RDPQCmd_Passthrough8
    sb t0, %lo(RDP_TARGET_BITDEPTH)
    .endfunc

    .func RDPQCmd_SetColorImage_Fixup
RDPQCmd_SetColorImage_Fixup:
    srl t0, a0, 19
    andi t0, 3
    sb t0, %lo(RDP_TARGET_BITDEPTH)

    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    mfc0 s0, COP0_DP_END
    jal DMAOut
    li t0, DMA_SIZE(8, 1)

    add s0, 8
    j RSPQ_Loop
    mtc0 s0, COP0_DP_END
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
    lbu t0, %lo(RDP_TARGET_BITDEPTH)
    beq t0, 3, RDPQCmd_Passthrough8
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
    j RDPQCmd_Passthrough8
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
    lb t0, %lo(RDP_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t0, 0x1 << 5
    # Prepare t3 to contain 0xFFFFFF
    lui t3, 0xFF
    # Leave unchanged when not in FILL or COPY mode
    beqz t0, SetScissorExSubstitute
    ori t3, 0xFFFF

    # Subtract 1 subpixel from XL (bits 23:12, as 10.2 fixed point)
    addi t1, a1, -(1 << 12)
    and t1, t3
    lui t2, 0xFF00
    and a1, t2
    or a1, t1

scissor_substitute:
    # Substitute command ID
    and t0, t3, a0
    lui a0, 0xED00 # SET_SCISSOR
    j RDPQCmd_Passthrough8
    or a0, t0
    .endfunc

    #############################################################
    # RDPQCmd_Passthrough8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough8
RDPQCmd_Passthrough8:
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc


    #############################################################
    # RDPQCmd_Passthrough16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough16
RDPQCmd_Passthrough16:
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    sw a2, 0x08(s4)
    sw a3, 0x0C(s4)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc

    #############################################################
    # RDPQCmd_PassthroughBig
    # 
    # Forwards the RDP command in the input stream to the RDP stream.
    # The size of the command is automatically detected by reading #rspq_cmd_size
    #############################################################
    .func RDPQCmd_PassthroughBig
RDPQCmd_PassthroughBig:
    sub s1, rspq_dmem_buf_ptr, rspq_cmd_size
    addi s1, %lo(RSPQ_DMEM_BUFFER)
    li s2, %lo(RDP_CMD_STAGING)
passthrough_copy_loop:
    lqv $v00,0, 0x00,s1
    lrv $v00,0, 0x10,s1
    sqv $v00,0, 0x00,s2
    addi s1, 0x10
    addi t1, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER)
    blt s1, t1, passthrough_copy_loop
    addi s2, 0x10

    li s4, %lo(RDP_CMD_STAGING)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc
