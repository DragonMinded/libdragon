#include <rsp_queue.inc>

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8   # Invalid
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   32  # 0xC8 Filled
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   48  # 0xC9 Filled ZBuffered
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   96  # 0xCA Textured
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   112 # 0xCB Textured ZBuffered
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   96  # 0xCC Shaded
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   112 # 0xCD Shaded ZBuffered
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   160 # 0xCE Shaded Textured
        RSPQ_DefineCommand GFXCmd_PassthroughBig,   176 # 0xCF Shaded Textured ZBuffered

        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid

        RSPQ_DefineCommand GFXCmd_ModifyOtherModes, 12 # 0xE0
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand GFXCmd_Passthrough16,    16 # 0xE4 TEXTURE_RECTANGLE
        RSPQ_DefineCommand GFXCmd_Passthrough16,    16 # 0xE5 TEXTURE_RECTANGLE_FLIP
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xE6 SYNC_LOAD
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xE7 SYNC_PIPE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xE8 SYNC_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xE9 SYNC_FULL
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xEA SET_KEY_GB
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xEB SET_KEY_R
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xEC SET_CONVERT
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xED SET_SCISSOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xEE SET_PRIM_DEPTH
        RSPQ_DefineCommand GFXCmd_SetOtherModes,    8  # 0xEF SET_OTHER_MODES
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF0 LOAD_TLUT
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF2 SET_TILE_SIZE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF3 LOAD_BLOCK
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF4 LOAD_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF5 SET_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF6 FILL_RECTANGLE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF7 SET_FILL_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF8 SET_FOG_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xF9 SET_BLEND_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xFA SET_PRIM_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xFB SET_ENV_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xFC SET_COMBINE_MODE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xFD SET_TEXTURE_IMAGE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xFE SET_Z_IMAGE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0xFF SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
RDP_OTHER_MODES:       .quad  0xEF00000000000000
    RSPQ_EndSavedState

    .bss

    .align 4
    # Enough for a full triangle command
RDP_CMD_STAGING:       .ds.b 0xB0

    .text

    #############################################################
    # GFXCmd_SetOtherModes
    #
    # Completely ovewrites the internal cache of the RDP other modes with the 
    # values provided in a0 and a1 and sends it to the RDP.
    #
    # ARGS:
    #   a0: Command id and upper word of other modes
    #   a1: Lower word of other modes
    #############################################################
    .func GFXCmd_SetOtherModes
GFXCmd_SetOtherModes:
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    j GFX_SendOtherModes
    sw a1, %lo(RDP_OTHER_MODES) + 0x4
    .endfunc

    #############################################################
    # GFXCmd_ModifyOtherModes
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
    .func GFXCmd_ModifyOtherModes
GFXCmd_ModifyOtherModes:
    lw t1, %lo(RDP_OTHER_MODES)(a0)
    and t1, a1
    or t1, a2
    sw t1, %lo(RDP_OTHER_MODES)(a0)
    # Need to override command size here since GFXCmd_ModifyOtherModes is 12 bytes
    # and RSPQ_RdpSendDynamic expects the size of the RDP command.
    li rspq_cmd_size, 8
    .endfunc

    #############################################################
    # GFX_SendOtherModes
    #
    # Sends the value in RDP_OTHER_MODES to the RDP.
    #############################################################
    .func GFX_SendOtherModes
GFX_SendOtherModes:
    li s4, %lo(RDP_OTHER_MODES)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc

    
    #############################################################
    # GFXCmd_Passthrough8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #############################################################
    .func GFXCmd_Passthrough8
GFXCmd_Passthrough8:
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc


    #############################################################
    # GFXCmd_Passthrough16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #############################################################
    .func GFXCmd_Passthrough16
GFXCmd_Passthrough16:
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    sw a2, 0x08(s4)
    sw a3, 0x0C(s4)
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc

    #############################################################
    # GFXCmd_PassthroughBig
    # 
    # Forwards the RDP command in the input stream to the RDP stream.
    # The size of the command is automatically detected by reading #rspq_cmd_size
    #############################################################
    .func GFXCmd_PassthroughBig
GFXCmd_PassthroughBig:
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
