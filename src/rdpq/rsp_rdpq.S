#include <rsp_queue.inc>
#include "rdpq_constants.h"
#include "rdpq_macros.h"

#define rdpq_write_ptr s7

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xC0 NOOP
        RSPQ_DefineCommand RDPQCmd_SetLookupAddress,        8   # 0xC1 Set lookup address
        RSPQ_DefineCommand RDPQCmd_RectEx,                  8   # 0xC2 Fill Rectangle (esclusive bounds)
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xC3
        RSPQ_DefineCommand RDPQCmd_ResetMode,               16  # 0xC4 Reset Mode (set mode standard)
        RSPQ_DefineCommand RDPQCmd_SetCombineMode_2Pass,    8   # 0xC5 SET_COMBINE_MODE (two pass)
        RSPQ_DefineCommand RDPQCmd_PushMode,                8   # 0xC6 Push Mode
        RSPQ_DefineCommand RDPQCmd_PopMode,                 8   # 0xC7 Pop Mode
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     32  # 0xC8 Filled
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     48  # 0xC9 Filled ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     96  # 0xCA Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     112 # 0xCB Textured ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     96  # 0xCC Shaded
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     112 # 0xCD Shaded ZBuffered
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     160 # 0xCE Shaded Textured
        RSPQ_DefineCommand RDPQCmd_PassthroughTriangle,     176 # 0xCF Shaded Textured ZBuffered

        RSPQ_DefineCommand RDPQCmd_RectEx,                  16  # 0xD0 Texture Rectangle (esclusive bounds)
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD1
        RSPQ_DefineCommand RDPQCmd_SetScissorEx,            8   # 0xD2 Set Scissor (exclusive bounds)
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD3
        RSPQ_DefineCommand RDPQCmd_ModifyOtherModes,        12  # 0xD4 Modify SOM
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD5
        RSPQ_DefineCommand RDPQCmd_SetFillColor32,          8   # 0xD6
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD7
        RSPQ_DefineCommand RDPQCmd_SetBlendingMode,         8   # 0xD8 Set Blending Mode
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD9
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDA
        RSPQ_DefineCommand RDPQCmd_SetCombineMode_1Pass,    16  # 0xDB SET_COMBINE_MODE (one pass)
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDC
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDD
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDE
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDF

        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE0
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE1
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE2
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xE3
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
RDPQ_SYNCFULL:          .quad  0   # Last syncfull command (includes callback). NOTE: this must stay as first variable in the state

RDPQ_ADDRESS_TABLE:     .ds.l  RDPQ_ADDRESS_TABLE_SIZE

# Stack slots for 3 saved RDP modes
RDPQ_MODE_STACK:        .ds.b (RDPQ_MODE_END - RDPQ_MODE)*3    

RDPQ_RDRAM_STATE_ADDR:  .word  0

    RSPQ_EndSavedState

    .text

    #############################################################
    # RDPQCmd_Passthrough8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough8
RDPQCmd_Passthrough8:
    jal_and_j RDPQ_Write8, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_Passthrough16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #############################################################
    .func RDPQCmd_Passthrough16
RDPQCmd_Passthrough16:
    jal_and_j RDPQ_Write16, RDPQ_Finalize
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
    # Save the other modes to internal cache, then call RDPQ_WriteOtherModes
    # RDPQ_WriteOtherModes will write both SetOtherModes and SetScissor to the staging area
    li ra, RDPQ_WriteOtherModes
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
    sw a0, %lo(RDPQ_OTHER_MODES) + 0x0
    jr ra
    sw a1, %lo(RDPQ_OTHER_MODES) + 0x4
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
    srl t0, a0, 19
    andi t0, 3
    # fixup DRAM address using address slots
    jal RDPQ_FixupAddress
    sb t0, %lo(RDPQ_TARGET_BITDEPTH)
    lui t1, 0xF000
    # Append this command to staging area
    jal RDPQ_Write8
    or a0, t1

    # Append SetFillColor next, then submit everything
    lw a1, %lo(RDPQ_FILL_COLOR)
    jal_and_j RDPQ_WriteSetFillColor, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_RectEx
    #
    # Provides a consistent API for the FlllRectangle/TextureRectangle
    # command that always uses exclusive ranges across all cycle modes.
    #############################################################
    .func RDPQCmd_RectEx
RDPQCmd_RectEx:
    lb t0, %lo(RDPQ_OTHER_MODES) + 0x1
    # Bit 21 of the first word is set if FILL or COPY mode is active
    andi t0, 0x1 << 5
    # Leave unchanged when not in FILL or COPY mode
    beqz t0, rect_substitute
    lui t1, 0xD000 ^ 0xE400  # TextureRectEx -> TEXTURE_RECTANGLE (or FillRectEx -> FILL_RECTANGLE)

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
    # Call Write16 for TexRect and Wirte8 for FillRect, and then RDPQ_Finalize in tailcall
    sll t0, a0, 3
    bltz t0, rect_end
    li s0, RDPQ_Write8
    li s0, RDPQ_Write16
rect_end:
    jr s0
    li ra, RDPQ_Finalize
    .endfunc

    #############################################################
    # RDPQCmd_PassthroughTriangle
    # 
    # Forwards the RDP command in the input stream to the RDP stream.
    # The size of the command is automatically detected by reading #rspq_cmd_size
    #############################################################
    .func RDPQCmd_PassthroughTriangle
RDPQCmd_PassthroughTriangle:
    addi s1, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER)   # End of command in RSPQ buffer
    sub s0, s1, rspq_cmd_size                           # Start of command in RSPQ buffer
    li s4, %lo(RDPQ_CMD_STAGING)                        # Pointer into RDPQ buffer
    # Add num mipmap levels to the triangle
    lbu t1, %lo(RDPQ_OTHER_MODES) + 0
    lbu t0, 1(s0)
    andi t1, 0x38     # Isolate bits 2-5 (aka 59-61 of SOM)
    or t0, t1
    sb t0, 1(s0)
passthrough_copy_loop:
    lqv $v00,0, 0x00,s0
    lrv $v00,0, 0x10,s0
    sqv $v00,0, 0x00,s4
    addi s0, 0x10
    blt s0, s1, passthrough_copy_loop
    addi s4, 0x10

    j RDPQ_Finalize
    sw s4, %lo(RDPQ_CMD_PTR)
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
    sw a1, %lo(RDPQ_ADDRESS_TABLE)(a0)
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
    lw t1, %lo(RDPQ_ADDRESS_TABLE)(t0)
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
    sw a0, %lo(RDPQ_SYNCFULL) + 0
    sw a1, %lo(RDPQ_SYNCFULL) + 4
    li s4, %lo(RDPQ_SYNCFULL)
    lw s0, %lo(RDPQ_RDRAM_STATE_ADDR)
    li t0, DMA_SIZE(8, 1)
    jal_and_j DMAOut, RDPQCmd_Passthrough8
    .endfunc

    #############################################################
    # RDPQCmd_PushMode
    #
    # Execute a push on the RDP mode stack. The current RDP mode
    # (blender+combiner) is pushed one slot deeper in a stack,
    # form which it can be recovered later with RDPQCmd_PopMode
    #############################################################
    .func RDPQCmd_PushMode
RDPQCmd_PushMode:
    li s0, %lo(RDPQ_MODE)
    li s1, %lo(RDPQ_MODE_STACK)

PushPopMode:
    lqv $v00,0, 0x00,s0
    lqv $v01,0, 0x10,s0
    lqv $v02,0, 0x00,s1
    lqv $v03,0, 0x10,s1
    lqv $v04,0, 0x20,s1
    lqv $v05,0, 0x30,s1

    sqv $v00,0, 0x00,s1
    sqv $v01,0, 0x10,s1
    sqv $v02,0, 0x20,s1
    sqv $v03,0, 0x30,s1
    sqv $v04,0, 0x40,s1
    jr ra
    sqv $v05,0, 0x50,s1
    .endfunc

    #############################################################
    # RDPQCmd_PopMode
    #
    # Execute a pop on the RDP mode stack. The current RDP mode
    # (blender+combiner) is popped one slot in a stack, recovering
    # the mode that was active when RDPQCmd_PushMode was last called.
    #############################################################
    .func RDPQCmd_PopMode
RDPQCmd_PopMode:
    li s0, %lo(RDPQ_MODE_STACK)
    li s1, %lo(RDPQ_MODE)
    # Pop from the stack and then reconfigure the cycle type.
    # Notice that technically it wouldn't be necessary to run
    # the full RDPQ_UpdateRenderMode (it would be sufficient to call
    # RDPQ_Write16+RDPQ_Finalize after loading combiner+other_modes)
    # but this way we get to reuse the function without adding more
    # specialized code.
    lqv $v00,0, 0x00,s0
    lqv $v01,0, 0x10,s0
    lqv $v02,0, 0x20,s0
    lqv $v03,0, 0x30,s0
    lqv $v04,0, 0x40,s0
    lqv $v05,0, 0x50,s0

    sqv $v00,0, 0x00,s1
    sqv $v01,0, 0x10,s1
    sqv $v02,0, 0x00,s0
    sqv $v03,0, 0x10,s0
    sqv $v04,0, 0x20,s0
    j RDPQ_UpdateRenderMode
    sqv $v05,0, 0x30,s0
    .endfunc

    #############################################################
    # RDPQCmd_ResetMode
    #
    # Reset the current RDP mode to the default setting. This is
    # called by rdpq_mode_standard to configure a base setting.
    #
    #  a0,a1: Color combiner to configure
    #  a2,a3: SOM to configure
    #############################################################
RDPQCmd_ResetMode:
    # Keep SOMX_UPDATE_FREEZE if set in the current state.
    # This is the only special state that we keep valid over
    # a reset mode, which in general always resets SOM.
    lw t3, %lo(RDPQ_OTHER_MODES) + 0
    andi t2, t3, SOMX_UPDATE_FREEZE >> 32
    or a2, t2

    # Clear RDPQ_MODE
    li s0, %lo(RDPQ_MODE)
    vxor $v00, $v00, $v00
    sqv $v00,0, 0x00,s0
    sqv $v00,0, 0x10,s0

    # Save SOM right away
    sw a2, %lo(RDPQ_OTHER_MODES) + 0
    sw a3, %lo(RDPQ_OTHER_MODES) + 4

    # Check if the FILL/COPY bit is changed compared to the current mode
    # If so, update scissoring
    xor t3, a2
    sll t3, 63 - (SOM_CYCLE_SHIFT+1)
    bgez t3, reset_end
 
    move t0, a0
    move t1, a1
    lw a0, %lo(RDPQ_SCISSOR_RECT) + 0x0
    jal RDPQ_WriteSetScissor
    lw a1, %lo(RDPQ_SCISSOR_RECT) + 0x4
    move a0, t0
    move a1, t1
    
reset_end:
    # Call RDPQCmd_SetCombineMode_1Pass, which will set the standard CC 
    # and call RDPQ_UpdateRenderMode once
    j RDPQCmd_SetCombineMode_1Pass
    nop

# Include RDPQ library
#include <rsp_rdpq.inc>
