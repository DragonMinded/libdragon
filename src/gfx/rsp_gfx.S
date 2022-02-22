#include <rsp_queue.inc>
#include "gfx_internal.h"

    .data

    RSPQ_BeginOverlayHeader GFX_STATE_START, GFX_STATE_END
        RSPQ_DefineCommand GFXCmd_FillTriangle,     32 # 0x20 
        RSPQ_DefineCommand GFXCmd_ModifyOtherModes, 12 # 0x21
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand GFXCmd_Passthrough16,    16 # 0x24 TEXTURE_RECTANGLE
        RSPQ_DefineCommand GFXCmd_Passthrough16,    16 # 0x25 TEXTURE_RECTANGLE_FLIP
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x26 SYNC_LOAD
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x27 SYNC_PIPE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x28 SYNC_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x29 SYNC_FULL
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x2A SET_KEY_GB
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x2B SET_KEY_R
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x2C SET_CONVERT
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x2D SET_SCISSOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x2E SET_PRIM_DEPTH
        RSPQ_DefineCommand GFXCmd_SetOtherModes,    8  # 0x2F SET_OTHER_MODES
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x30 LOAD_TLUT
        RSPQ_DefineCommand RSPQCmd_Noop,            8  # Invalid
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x32 SET_TILE_SIZE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x33 LOAD_BLOCK
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x34 LOAD_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x35 SET_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x36 FILL_RECTANGLE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x37 SET_FILL_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x38 SET_FOG_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x39 SET_BLEND_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x3A SET_PRIM_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x3B SET_ENV_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x3C SET_COMBINE_MODE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x3D SET_TEXTURE_IMAGE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x3E SET_Z_IMAGE
        RSPQ_DefineCommand GFXCmd_Passthrough8,     8  # 0x3F SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    .align 3
    # Everything between GFX_STATE_START and GFX_STATE_END is persistent state that is automatically saved by the overlay system.
    # Should be kept consistent with gfx_state_t in gfx_internal.h
GFX_STATE_START:
RDP_DMEM_BUFFER:       .ds.b GFX_RDP_DMEM_BUFFER_SIZE
RDP_OTHER_MODES:       .quad  0
GFX_STATE_END:

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
    # The uppermost byte is always set to 0x2F.
    #
    # ARGS:
    #   a0: Command id and upper word of other modes
    #   a1: Lower word of other modes
    #############################################################
    .func GFXCmd_SetOtherModes
GFXCmd_SetOtherModes:
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    sw a1, %lo(RDP_OTHER_MODES) + 0x4
    li t0, 0x2F
    j GFX_SendOtherModes
    sb t0, %lo(RDP_OTHER_MODES)
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
    lw t0, %lo(RDP_OTHER_MODES)(a0)
    and t0, a1
    or t0, a2
    sw t0, %lo(RDP_OTHER_MODES)(a0)
    .endfunc

    #############################################################
    # GFX_SendOtherModes
    #
    # Sends the value in RDP_OTHER_MODES to the RDP.
    #
    #############################################################
    .func GFX_SendOtherModes
GFX_SendOtherModes:
    li s4, %lo(RDP_OTHER_MODES)
    li t0, 8
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc

    
    #############################################################
    # GFXCmd_Passthrough8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #############################################################
    .func GFXCmd_Passthrough8
GFXCmd_Passthrough8:
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    li t0, 8
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc


    #############################################################
    # GFXCmd_Passthrough16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #   a2: Third 4 bytes of RDP command
    #   a3: Fourth 4 bytes of RDP command
    #############################################################
    .func GFXCmd_Passthrough16
GFXCmd_Passthrough16:
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    sw a2, 0x08(s4)
    sw a3, 0x0C(s4)
    li t0, 16
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc

    #############################################################
    # GFXCmd_FillTriangle
    #
    # Forwards a basic triangle command (edge coefficients only).
    # Note that the command id is swapped out.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #   a2: Third 4 bytes of RDP command
    #   a3: Fourth 4 bytes of RDP command
    #############################################################
    .func GFXCmd_FillTriangle
GFXCmd_FillTriangle:
    # Replace 0x20 with 0x08
    lui t0, 0xFF
    ori t0, 0xFFFF
    and a0, t0
    lui t0, 0x0800
    or a0, t0
    lw t0, CMD_ADDR(0x10, 32)
    lw t1, CMD_ADDR(0x14, 32)
    lw t2, CMD_ADDR(0x18, 32)
    lw t3, CMD_ADDR(0x1C, 32)
    li s4, %lo(RDP_CMD_STAGING)
    sw a0, 0x00(s4)
    sw a1, 0x04(s4)
    sw a2, 0x08(s4)
    sw a3, 0x0C(s4)
    sw t0, 0x10(s4)
    sw t1, 0x14(s4)
    sw t2, 0x18(s4)
    sw t3, 0x1C(s4)
    li t0, 32
    jal_and_j RSPQ_RdpSendDynamic, RSPQ_Loop
    .endfunc
