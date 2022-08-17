#include <rsp_queue.inc>
#include "rdpq_constants.h"
#include "rdp_commands.h"

#define rdpq_write_ptr s7

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xC0 NOOP
        RSPQ_DefineCommand RDPQCmd_SetLookupAddress,        8   # 0xC1 Set lookup address
        RSPQ_DefineCommand RDPQCmd_PushMode,                8   # 0xC2 Push Mode
        RSPQ_DefineCommand RDPQCmd_PopMode,                 8   # 0xC3 Pop Mode
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xC4
        RSPQ_DefineCommand RDPQCmd_SetCombineMode_2Pass,    8   # 0xC5 SET_COMBINE_MODE (two pass)
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xC6
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     32  # 0xC8 Filled
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     48  # 0xC9 Filled ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     96  # 0xCA Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     112 # 0xCB Textured ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     96  # 0xCC Shaded
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     112 # 0xCD Shaded ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     160 # 0xCE Shaded Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     176 # 0xCF Shaded Textured ZBuffered

        RSPQ_DefineCommand RDPQCmd_TextureRectEx,           16  # 0xD0
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD1
        RSPQ_DefineCommand RDPQCmd_SetScissorEx,            8   # 0xD2
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD3
        RSPQ_DefineCommand RDPQCmd_ModifyOtherModes,        12  # 0xD4
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD5
        RSPQ_DefineCommand RDPQCmd_SetFillColor32,          8   # 0xD6
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD7
        RSPQ_DefineCommand RDPQCmd_SetBlendingMode,         8   # 0xD8 Set Blending Mode
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD9
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_SetCombineMode_1Pass,    8   # 0xDB SET_COMBINE_MODE (one pass)
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDC
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDD
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDE
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDF

        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE0
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE1
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE2
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # Invalid
        RSPQ_DefineCommand RDPQCmd_Passthrough16,           16  # 0xE4 TEXTURE_RECTANGLE
        RSPQ_DefineCommand RDPQCmd_Passthrough16,           16  # 0xE5 TEXTURE_RECTANGLE_FLIP
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE6 SYNC_LOAD
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE7 SYNC_PIPE
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xE8 SYNC_TILE
        RSPQ_DefineCommand RDPQCmd_SyncFull,                8   # 0xE9 SYNC_FULL
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEA SET_KEY_GB
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEB SET_KEY_R
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEC SET_CONVERT
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xED SET_SCISSOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xEE SET_PRIM_DEPTH
        RSPQ_DefineCommand RDPQCmd_SetOtherModes,           8   # 0xEF SET_OTHER_MODES
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF0 LOAD_TLUT
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xF1 RDPQ_DEBUG (debugging command)
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
        RSPQ_DefineCommand RDPQCmd_SetFixupImage,           8   # 0xFD SET_TEXTURE_IMAGE
        RSPQ_DefineCommand RDPQCmd_SetFixupImage,           8   # 0xFE SET_Z_IMAGE
        RSPQ_DefineCommand RDPQCmd_SetColorImage,           8   # 0xFF SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RDP Queue"
    .ascii "Rasky & Snacchus"

    .align 4
    RSPQ_BeginSavedState
RDP_SYNCFULL:          .quad  0   # Last syncfull command (includes callback). NOTE: this must stay as first variable in the state
RDP_SCISSOR_RECT:      .quad  0

# RDP MODE (32 bytes). NOTE: This must be 16-byte aligned (because we use lqv in push/pop),
# but we can't use .align 4 here, otherwise it's not easy to keep this structure layout
# in sync with the C side (rdpq_state_t in rdpq.c).
RDP_MODE:
    # Combiner setting to use in 1 cycle mode
    RDP_MODE_COMBINER_1CYC: .quad  0
    # Combiner setting to use in 2 cycle mode
    RDP_MODE_COMBINER_2CYC: .quad  0
    # Blender settings: up to two steps. Either of them
    # is already in a format valid for both 1cyc and 2cyc mode.
    RDP_MODE_BLENDER_STEPS: .word  0,0
    # Other modes
    RDP_OTHER_MODES:        .quad  0

# Stack slots for 3 saved RDP modes
RDP_MODE_STACK:        .ds.b 32*3    

RDP_ADDRESS_TABLE:     .ds.l  RDPQ_ADDRESS_TABLE_SIZE
RDP_FILL_COLOR:        .word  0
RDP_RDRAM_STATE_ADDR:  .word  0
RDP_TARGET_BITDEPTH:   .byte  0

    RSPQ_EndSavedState

COMB0_MASK:            .quad RDPQ_COMB0_MASK

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
    j RSPQ_RdpSend
    li s4, %lo(RDP_CMD_STAGING)
    .endfunc

    #############################################################
    # RDPQCmd_Passthrough8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough8
RDPQCmd_Passthrough8:
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
    # RDPQCmd_Passthrough16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough16
RDPQCmd_Passthrough16:
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
    # Reserve space for SetOtherModes + SetScissor:
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
    # RDPQCmd_SetFixupImage
    #
    # Acts as passthrough for Set*Image commands with the additional
    # address lookup capability. The most significant 6 bits of a1
    # are interpreted as an index into an address table. This command
    # will load the value from the table at that index and add it
    # to the address in the lower 26 bits.
    # To set entries in the table, see #RDPQCmd_SetLookupAddress.
    #############################################################
    .func RDPQCmd_SetFixupImage
RDPQCmd_SetFixupImage:
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    # fixup DRAM address using address slots
    jal RDPQ_FixupAddress
    lui t3, 0xF000
    or a0, t3
    jal_and_j RDPQ_Write8, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_SetColorImage
    #
    # Wrapper around RDP Set Color Image, that records
    # the target bitdepth. Works with address lookup (see #RDPQCmd_SetFixupImage). 
    # Will also re-execute any fixup commands that depend on the bitdepth.
    #############################################################
    .func RDPQCmd_SetColorImage
RDPQCmd_SetColorImage:
    li rspq_cmd_size, 16
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    srl t0, a0, 19
    andi t0, 3
    # fixup DRAM address using address slots
    jal RDPQ_FixupAddress
    sb t0, %lo(RDP_TARGET_BITDEPTH)
    lui t1, 0xF000
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
    lui t1, 0xED00 ^ 0xD200 # SetScissorEx -> SET_SCISSOR

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
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)
    lb t0, %lo(RDP_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t0, 0x1 << 5
    # Leave unchanged when not in FILL or COPY mode
    beqz t0, rect_substitute
    lui t1, 0xD000 ^ 0xE400  # TextureRectEx -> TEXTURE_RECTANGLE

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
    # RDPQCmd_PassthroughTriangle
    # 
    # Forwards the RDP command in the input stream to the RDP stream.
    # The size of the command is automatically detected by reading #rspq_cmd_size
    #############################################################
    .func RDPQCmd_PassthroughTriangle
RDPQCmd_PassthroughTriangle:
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

    #############################################################
    # RDPQCmd_SetLookupAddress
    # 
    # Sets an entry in the lookup address table. Note that index
    # zero should never changed, so it can act as the "identity".
    #
    # ARGS:
    #   a0: Command ID
    #   a1: Table entry index and address to set the entry to
    #############################################################
    .func RDPQCmd_SetLookupAddress
RDPQCmd_SetLookupAddress:
    jr ra
    sw a1, %lo(RDP_ADDRESS_TABLE)(a0)
    .endfunc

    #############################################################
    # RDPQ_FixupAddress
    # 
    # Looks up an address from the lookup table and adds it to a1.
    #
    # ARGS:
    #   a1: Table entry index and offset to the contained address
    # OUTPUTS:
    #   a1: Will contain the looked up address plus the offset
    #############################################################
    .func RDPQ_FixupAddress
RDPQ_FixupAddress:
    srl t0, a1, 26
    lw t1, %lo(RDP_ADDRESS_TABLE)(t0)
    jr ra
    add a1, t1
    .endfunc

    #############################################################
    # RDPQCmd_SyncFull
    #
    # SyncFull command in both static and dynamic mode. The command
    # encodes the callback in the lower
    #############################################################
    .func RDPQCmd_SyncFull
RDPQCmd_SyncFull:
    # Wait until the previous SYNC_FULL interrupt has been processed.
    jal SpStatusWait
    li t2, SP_STATUS_SIG_RDPSYNCFULL

    # Set the signal because we're about to schedule a new SYNC_FULL
    li t0, SP_WSTATUS_SET_SIG_RDPSYNCFULL
    mtc0 t0, COP0_SP_STATUS

    # Store the current SYNC_FULL command in the state and DMA it to RDRAM.
    # This includes the interrupt callback that the CPU will have to run.
    sw a0, %lo(RDP_SYNCFULL) + 0
    sw a1, %lo(RDP_SYNCFULL) + 4
    li s4, %lo(RDP_SYNCFULL)
    lw s0, %lo(RDP_RDRAM_STATE_ADDR)
    li t0, DMA_SIZE(8, 1)
    jal_and_j DMAOut, RDPQCmd_Passthrough8
    .endfunc

    .func RDPQCmd_SetCombineMode_1Pass
RDPQCmd_SetCombineMode_1Pass:
    li rspq_cmd_size, 16
    # The combiner settings is 1 pass. Store it as-is for 1cycle mode.
    sw a0, %lo(RDP_MODE_COMBINER_1CYC) + 0
    sw a1, %lo(RDP_MODE_COMBINER_1CYC) + 4

    # For 2 cycle mode, we need to adjust it changing the second pass
    # to be a pure passthrough. We can do this by simply setting to 0
    # all fields of the second pass, as that corresponds to:
    #   (COMBINED - COMBINED) * COMBINED + COMBINED  =  COMBINED
    # The 2PASS flag will not be set, as this combiner does not require 2cycle.
    lw t0, %lo(COMB0_MASK) + 0
    lw t1, %lo(COMB0_MASK) + 4
    and a0, t0
    j store_comb_2cyc
    and a1, t1
    .endfunc

    .func RDPQCmd_SetCombineMode_2Pass
RDPQCmd_SetCombineMode_2Pass:
    li rspq_cmd_size, 16
    # The combiner settings is 2 pass. The flag RDPQ_COMBINER_2PASS
    # (bit 63) is set in the command thanks to the fact that the overlay
    # is registered in slots 0xC0-0xF0 (with the top bit already set).
    # To be resistant to overlay ID changes, we would need the following
    # instruction, but we keep it disabled for now.
    #   or a0, RDP1_COMBINER_2PASS >> 32

    # This combiner setting will force 2cycle mode. Store it
    # in the 2cyc slot, and ignore the 1cyc slot (it's not going
    # to be used).
store_comb_2cyc:
    sw a0, %lo(RDP_MODE_COMBINER_2CYC) + 0
    j UpdateCycleType
    sw a1, %lo(RDP_MODE_COMBINER_2CYC) + 4
    .endfunc

    .func RDPQCmd_SetBlendingMode
RDPQCmd_SetBlendingMode:
    sw a1, %lo(RDP_MODE_BLENDER_STEPS)(a0)
    li rspq_cmd_size, 16
    # fallthrough
    .endfunc

    .func UpdateCycleType
UpdateCycleType:
    # Pointer to staging area where RDP SetCombine+SetOtherModes will be written
    li rdpq_write_ptr, %lo(RDP_CMD_STAGING)

    #define blend_1cyc  t3
    #define blend_2cyc  t4
    #define passthrough t5

    # Merge the two blender steps.
    li passthrough, 0
    lw t0, %lo(RDP_MODE_BLENDER_STEPS) + 0   # Load step0
    lw t1, %lo(RDP_MODE_BLENDER_STEPS) + 4   # Load step1
    
    beqz t0, blender_merge
    move blend_1cyc, t1
    
    beqz t1, blender_merge
    move blend_1cyc, t0

    and passthrough, t1, SOM_BLEND1_MASK
    or passthrough, SOMX_BLEND_2PASS
blender_merge:
    and blend_1cyc, SOM_BLEND0_MASK
    or blend_2cyc, blend_1cyc, passthrough

    # Check if either the current blender and combiner configuration require
    # 2cycle mode:
    #   * Blender: bit 15 is set if 2cyc mode is required
    #   * Combiner: bit 63 is set if 2cyc mode is required
    sll t2, blend_2cyc, 16
    lw t1, %lo(RDP_MODE_COMBINER_2CYC)
    or t1, t2
    # Point to either the 2cyc or 1cyc configuration, depending on what we need
    # to load.
    li s0, %lo(RDP_MODE_COMBINER_2CYC)
    move t0, blend_2cyc
    bltz t1, set_2cyc
    li t4, (SOM_CYCLE_MASK ^ SOM_CYCLE_2) >> 32
set_1cyc:
    li s0, %lo(RDP_MODE_COMBINER_1CYC)
    move t0, blend_1cyc
    li t4, (SOM_CYCLE_MASK ^ SOM_CYCLE_1) >> 32
set_2cyc:
    lw a0, 0(s0)   # Combiner
    lw a1, 4(s0)   # Combiner
    lw a2, %lo(RDP_OTHER_MODES) + 0
    lw a3, %lo(RDP_OTHER_MODES) + 4

    # Set correct SET_COMBINE opcode (0xFC). The opcode can be anything of
    # the other 4 (1pass/2pass dynamic/static).
    or a0, 0xFF000000
    xor a0, 0xFF000000 ^ 0xFC000000

    # Change blender bits in other modes low word
    li t1, SOM_BLEND_MASK
    and t0, t1
    not t1, t1
    and a3, t1
    or a3, t0
    sw a3, %lo(RDP_OTHER_MODES) + 4

    # Change cycle type bits in other modes low word
    or a2, SOM_CYCLE_MASK >> 32
    xor a2, t4
    sw a2, %lo(RDP_OTHER_MODES) + 0

    jal_and_j RDPQ_Write16, RDPQ_Finalize
    .endfunc

    #undef blend_1cyc
    #undef blend_2cyc
    #undef passhthrough


    #############################################################
    # RDPQCmd_PushMode
    #
    # Execute a push on the RDP mode stack. The current RDP mode
    # (blender+combiner) is pushed one slot deeper in a stack,
    # form which it can be recovered later with RDPQCmd_PopMode
    #############################################################
    .func RDPQCmd_PushMode
RDPQCmd_PushMode:
    li s0, %lo(RDP_MODE) + 0
    li s1, %lo(RDP_MODE) + 32

PushPopMode:
    lqv $v00,0, 0x00,s0
    lqv $v01,0, 0x10,s0
    lqv $v02,0, 0x20,s0
    lqv $v03,0, 0x30,s0
    lqv $v04,0, 0x40,s0
    lqv $v05,0, 0x50,s0
    sqv $v00,0, 0x00,s1
    sqv $v01,0, 0x10,s1
    sqv $v02,0, 0x20,s1
    sqv $v03,0, 0x30,s1
    sqv $v04,0, 0x40,s1
    jr ra
    sqv $v05,0, 0x50,s1
    .endfunc

    .func RDPQCmd_PopMode
RDPQCmd_PopMode:
    li rspq_cmd_size, 16
    li s0, %lo(RDP_MODE) + 32
    li s1, %lo(RDP_MODE) + 0
    # Pop from the stack and then reconfigure the cycle type.
    # Notice that technically it wouldn't be necessary to run
    # the full UpdateCycleType (it would be sufficient to call
    # RDPQ_Write16+RDPQ_Finalize after loading combiner+other_modes)
    # but this way we get to reuse the function without adding more
    # specialized code.
    jal_and_j PushPopMode, UpdateCycleType
    .endfunc


