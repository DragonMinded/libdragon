#include <rsp_queue.inc>
#include "rdpq_constants.h"
#include "rdpq_macros.h"

#define rdpq_write_ptr s7

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xC0 NOOP
        RSPQ_DefineCommand RDPQCmd_SetLookupAddress,        8   # 0xC1 Set lookup address
        RSPQ_DefineCommand RDPQCmd_RectEx,                  8   # 0xC2 Fill Rectangle (esclusive bounds)
        RSPQ_DefineCommand RDPQCmd_ClearZBuffer,            16  # 0xC3 CLear Z-Buffer
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
        RSPQ_DefineCommand RDPQCmd_SetDebugMode,            4   # 0xD1 Set Debug mode
        RSPQ_DefineCommand RDPQCmd_SetScissorEx,            8   # 0xD2 Set Scissor (exclusive bounds)
        RSPQ_DefineCommand RDPQCmd_SetPrimColorComponent,   8   # 0xD3 Set Primimive Color Component (minlod or primlod or rgba)
        RSPQ_DefineCommand RDPQCmd_ModifyOtherModes,        12  # 0xD4 Modify SOM
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD5
        RSPQ_DefineCommand RDPQCmd_SetFillColor32,          8   # 0xD6
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xD7
        RSPQ_DefineCommand RDPQCmd_SetBlendingMode,         8   # 0xD8 Set Blending Mode
        RSPQ_DefineCommand RDPQCmd_SetFogMode,              8   # 0xD9 Set Fog Mode
        RSPQ_DefineCommand RSPQCmd_Noop,                    8   # 0xDA
        RSPQ_DefineCommand RDPQCmd_SetCombineMode_1Pass,    16  # 0xDB SET_COMBINE_MODE (one pass)
        RSPQ_DefineCommand RDPQCmd_AutoTmem_SetAddr,        4   # 0xDC AutoTmem_SetAddr
        RSPQ_DefineCommand RDPQCmd_AutoTmem_SetTile,        8   # 0xDD AutoTmem_SetTile
        RSPQ_DefineCommand RDPQCmd_Triangle,                4   # 0xDE Triangle (assembled by RSP)
        RSPQ_DefineCommand RDPQCmd_TriangleData,            28  # 0xDF Set Triangle Data

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
        RSPQ_DefineCommand RDPQCmd_SetPrimColor,            8   # 0xFA SET_PRIM_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFB SET_ENV_COLOR
        RSPQ_DefineCommand RDPQCmd_Passthrough8,            8   # 0xFC SET_COMBINE_MODE
        RSPQ_DefineCommand RDPQCmd_SetFixupImage,           8   # 0xFD SET_TEXTURE_IMAGE
        RSPQ_DefineCommand RDPQCmd_SetFixupImage,           8   # 0xFE SET_Z_IMAGE
        RSPQ_DefineCommand RDPQCmd_SetColorImage,           8   # 0xFF SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RDP Queue"
    .ascii "Rasky & Snacchus"

    # RDPQ Overlay state
    # NOTE: keep this in sync with rdpq_state_t in rdpq.c
    .align 4
    RSPQ_BeginSavedState
RDPQ_SYNCFULL:          .quad  0   # Last syncfull command (includes callback). NOTE: this must stay as first variable in the state
RDPQ_SYNCPOINT_ID:      .long  0   # Syncpoint ID for the last syncfull command
_PADDING:               .long  0

RDPQ_RDRAM_STATE_ADDR:      .word  0
RDPQ_RDRAM_SYNCPOINT_ADDR:  .word  0

RDPQ_ADDRESS_TABLE:     .ds.l  RDPQ_ADDRESS_TABLE_SIZE

RDPQ_AUTOTMEM_ADDR:     .half  0
RDPQ_AUTOTMEM_ADDR_PREV:.half  0
RDPQ_AUTOTMEM_LIMIT:    .half  0
RDPQ_AUTOTMEM_ENABLED:  .byte  0

# Store individual components of the complex Prim Color structure for sync between commands
# Used in SetPrimColorComponent and SetPrimColor
    .align 4
RDPQ_PRIM_COLOR_EX:     .word  0
RDPQ_PRIM_COLOR_RGBA:   .word  0

    .align 4
# Stack slots for 3 saved RDP modes
RDPQ_MODE_STACK:        .ds.b (RDPQ_MODE_END - RDPQ_MODE)*3    

    .align 4
RDPQ_TRI_DATA0:          .dcb.l 7
    .align 4
RDPQ_TRI_DATA1:          .dcb.l 7
    .align 4
RDPQ_TRI_DATA2:          .dcb.l 7

    RSPQ_EndSavedState

    .bss

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
    # RDPQCmd_SetPrimColor
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    # and saves the PRIM color data into DMEM for sync
    #############################################################
    .func RDPQCmd_SetPrimColor
RDPQCmd_SetPrimColor:
    sw a0, %lo(RDPQ_PRIM_COLOR_EX)
    j RDPQCmd_Passthrough8
    sw a1, %lo(RDPQ_PRIM_COLOR_RGBA)
    .endfunc

    #############################################################
    # RDPQCmd_SetPrimColorComponent
    #
    # Allows for partial setup of data through the 2 bits in a0
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    # and saves the PRIM color data into DMEM for sync
    #############################################################
    .func RDPQCmd_SetPrimColorComponent
RDPQCmd_SetPrimColorComponent:
    lui  t0, 0xD300 ^ 0xFA00
    xor  a0, t0

    srl  t3, a0, 16 # the selection is stored in the upper half of a0, in the lowest 2 bits
    andi t3, 3
    beqz t3, set_rgba
    addi t3, -1
    beqz t3, set_primlod
set_minlod:
    lw   t1, %lo(RDPQ_PRIM_COLOR_EX) # restore current prim color data
    and  t1, 0x000000FF
    or   a0, t1
    j    setprimcolor_finalize
set_primlod:
    lw   t1, %lo(RDPQ_PRIM_COLOR_EX) # restore current prim color data
    and  t1, 0x0000FF00
    or   a0, t1
    j    setprimcolor_finalize
set_rgba:
    lw   t1, %lo(RDPQ_PRIM_COLOR_EX) # restore current prim color data
    and  t1, 0x0000FFFF
    or   a0, t1
    sw   a1, %lo(RDPQ_PRIM_COLOR_RGBA)
setprimcolor_finalize:
    sw   a0, %lo(RDPQ_PRIM_COLOR_EX)
    li   s4, %lo(RDPQ_PRIM_COLOR_EX)
    j    RDPQ_Send
    li   s3, %lo(RDPQ_PRIM_COLOR_EX)+8
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
    # Save the other modes to internal cache. The MSB of a0 containes
    # the 0xEF command ID, but we use that byte for extended SOM flags
    # (SOMX_*) in the mode API, so reset it to zero as initial state.
    sw a0, %lo(RDPQ_OTHER_MODES) + 0x0
    sb zero, %lo(RDPQ_OTHER_MODES) + 0x0
    j RDPQ_WriteOtherModes
    sw a1, %lo(RDPQ_OTHER_MODES) + 0x4
    .endfunc

    #############################################################
    # RDPQCmd_ModifyOtherModes
    #
    # Modifies a specific part of the other modes and sends the updated
    # value to the RDP.
    # This function can be used as a standard fixup (in which case,
    # it will potential emit a SET_SCISSOR in case the cycle type
    # changed between COPY/FILL and 1CYC/2CYC), or as a mode fixup,
    # part of the mode API, in which case it must call RDPQ_UpdateRenderMode
    # to regenerate the new render mode.
    #
    # NOTE: The new value in a2 should never have bits set outside of
    #       the inverse bitmask to ensure correct results.
    #
    # ARGS:
    #   a0: Bit 24..31: Command id
    #       Bit 15:     If 1, call RDPQ_UpdateRenderMode.
    #       Bit 12-0:   Word offset into other modes (0 or 4)
    #   a1: Inverse bit mask of the value to be written
    #   a2: New value
    #############################################################
    .func RDPQCmd_ModifyOtherModes
RDPQCmd_ModifyOtherModes:
    lw t1, %lo(RDPQ_OTHER_MODES)(a0)
    and t1, a1
    or t1, a2
    sw t1, %lo(RDPQ_OTHER_MODES)(a0)
    sll a0, 16
    bltz a0, RDPQ_UpdateRenderMode

    # Prepare the updated command in a0-a1 for the following steps
    lw a0, %lo(RDPQ_OTHER_MODES) + 0x0
    lw a1, %lo(RDPQ_OTHER_MODES) + 0x4
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
    # Write other modes command to staging area, then overwrite
    # top byte with 0xEF which is not part of RDPQ_OTHER_MODES but must
    # be present when sending to RDP.
    jal RDPQ_Write8
    li t1, 0xEF
    sb t1, -8(s0)
    # fallthrough!
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
    # Update other commands that need to change some state depending on the other modes

    # SetScissor:
    # load the cached command first
    lw a0, %lo(RDPQ_SCISSOR_RECT) + 0x0
    lw a1, %lo(RDPQ_SCISSOR_RECT) + 0x4
    # Append the fixed up SetScissor command to staging area and then finalize
    jal_and_j RDPQ_WriteSetScissor, RDPQ_Finalize
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
    jal_and_j RDPQ_WriteSetFillColor, RDPQ_Finalize
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
    lui t1, 0xED00 ^ 0xD200 # SetScissorEx -> SET_SCISSOR
    xor a0, t1
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
    # Wait until:
    #  * the previous SYNC_FULL interrupt has been processed by the CPU.
    #  * any pending RSP interrupt for checkpoint has been processed by the CPU.
    # We need to wait for checkpoints so that we can fetch the correct checkpoint ID,
    # which in turn is necessary for implementing rdpq_call_deferred.
#if RSPQ_PROFILE
    mfc0 t1, COP0_DP_CLOCK
#endif
    jal SpStatusWait
    li t2, SP_STATUS_SIG_RDPSYNCFULL | SP_STATUS_SIG_SYNCPOINT
#if RSPQ_PROFILE
    mfc0 t0, COP0_DP_CLOCK
    lw t2, %lo(RSPQ_PROFILE_START_TIME)
    li s0, %lo(RSPQ_PROFILESLOT_IDLE_RDPSYNCFULLMULTI)
    subu t0, t1
    and t0, 0xFFFFFF
    add t2, t0
    jal RSPQ_ProfileAccumulate
    sw t2, %lo(RSPQ_PROFILE_START_TIME)
#endif

    # Fetch the current syncpoint ID and put it into the state
    lw s0, %lo(RDPQ_RDRAM_SYNCPOINT_ADDR)
    li s4, %lo(RDPQ_SYNCPOINT_ID)
    jal DMAIn
    li t0, DMA_SIZE(8, 1)

    # Set the signal because we're about to schedule a new SYNC_FULL
    li t0, SP_WSTATUS_SET_SIG_RDPSYNCFULL
    mtc0 t0, COP0_SP_STATUS

    # Store the current SYNC_FULL command in the state. This includes the
    # interrupt callback that the CPU will have to run.
    sw a0, %lo(RDPQ_SYNCFULL) + 0
    sw a1, %lo(RDPQ_SYNCFULL) + 4
    
    # DMA the command plus the syncpoint ID it to RDRAM.
    li s4, %lo(RDPQ_SYNCFULL)
    lw s0, %lo(RDPQ_RDRAM_STATE_ADDR)
    jal DMAOut
    li t0, DMA_SIZE(16, 1)

    # Actually send the SYNC_FULL command to the RDP
    jal RDPQ_Write8
    nop
    li s4, %lo(RDPQ_CMD_STAGING)
    lw s3, %lo(RDPQ_CMD_PTR)
    sw s4, %lo(RDPQ_CMD_PTR)
    jal RDPQ_Send
    sb zero, %lo(RDPQ_SYNCFULL_ONGOING)

    # Put futher command writing on hold. By setting RDPQ_SYNCFULL_ONGOING, 
    # we prevent further calls to RDPQ_Send / RSPQCmd_RdpSetBuffer / RSPQCmd_RdpAppendBuffer
    # to actually start a new RDP DMA transfer, until the SYNC_FULL is done.
    # This is done to avoid a hardware bug.
    li t0, DP_STATUS_BUSY
    sb t0, %lo(RDPQ_SYNCFULL_ONGOING)

    j RSPQ_Loop
    nop

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

    .func RDPQCmd_SetBlendingMode
RDPQCmd_SetBlendingMode:
    j RDPQ_SetBlendingMode
    nop
    .endfunc

    .func RDPQCmd_SetFogMode
RDPQCmd_SetFogMode:
    j RDPQ_SetFogMode
    nop
    .endfunc

    .func RDPQCmd_SetCombineMode_1Pass
RDPQCmd_SetCombineMode_1Pass:
    j RDPQ_SetCombineMode_1Pass
    nop
    .endfunc

    .func RDPQCmd_SetCombineMode_2Pass
RDPQCmd_SetCombineMode_2Pass:
    j RDPQ_SetCombineMode_2Pass
    nop
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


    .func RDPQCmd_TriangleData
RDPQCmd_TriangleData:
    sw a1, %lo(RDPQ_TRI_DATA0) + 0(a0)  # X/Y
    sw a2, %lo(RDPQ_TRI_DATA0) + 4(a0)  # Z
    sw a3, %lo(RDPQ_TRI_DATA0) + 8(a0)  # RGBA

    lw t0, CMD_ADDR(16, 28)
    lw t1, CMD_ADDR(20, 28)
    lw t2, CMD_ADDR(24, 28)

    sw t0, %lo(RDPQ_TRI_DATA0) + 12(a0)  # S/T
    sw t1, %lo(RDPQ_TRI_DATA0) + 16(a0)  # W
    jr ra
    sw t2, %lo(RDPQ_TRI_DATA0) + 20(a0)  # INV_W 
    .endfunc

    .func RDPQCmd_Triangle
RDPQCmd_Triangle:
#if RDPQ_TRIANGLE_REFERENCE
    assert RDPQ_ASSERT_INVALID_CMD_TRI
#else
    li s4, %lo(RDPQ_CMD_STAGING)
    move s3, s4
    li v0, 2   # disable culling
    li a1, %lo(RDPQ_TRI_DATA0)
    li a2, %lo(RDPQ_TRI_DATA1)
    jal RDPQ_Triangle_Send_Async
    li a3, %lo(RDPQ_TRI_DATA2)
    jal_and_j RDPQ_Triangle_Send_End, RSPQ_Loop

#endif /* RDPQ_TRIANGLE_REFERENCE */
    .endfunc

    .func RDPQCmd_SetDebugMode
RDPQCmd_SetDebugMode:
    jr ra
    sb a0, %lo(RDPQ_DEBUG)
    .endfunc


    #########################################
    # RDPQCmd_AutoTmem_SetAddr
    #
    # Set the current auto-TMEM address. This is used by the auto-TMEM
    # feature of rdpq_tex, that will autoallocate TMEM without requiring
    # explicit addresses.
    #
    # ARGS:
    #   a0: zero: begin auto-TMEM, set address to 0
    #       -1:   end auto-TMEM
    #       non-zero: increment auto-TMEM address by a0 multiplied by 8
    #########################################

    .func RDPQCmd_AutoTmem_SetAddr
RDPQCmd_AutoTmem_SetAddr:
    sll a0, 16
    sra a0, 16
    bltz a0, autotmem_end  # -1 => end
    lhu t0, %lo(RDPQ_AUTOTMEM_ADDR) 
    beqz a0, autotmem_begin # 0 => start

    # Positive value: increment address
    lhu t1, %lo(RDPQ_AUTOTMEM_LIMIT)
    #ifdef NDEBUG
    lbu t2, %lo(RDPQ_AUTOTMEM_ENABLED)
    assert_gt t2, 0, RDPQ_ASSERT_AUTOTMEM_UNPAIRED
    #endif
    sh t0, %lo(RDPQ_AUTOTMEM_ADDR_PREV)
    add t0, a0
    assert_le t0, t1, RDPQ_ASSERT_AUTOTMEM_FULL
    jr ra
    sh t0, %lo(RDPQ_AUTOTMEM_ADDR)

autotmem_begin:
    # Increment enabled flag. Do nothing if it was already enabled
    lbu t0, %lo(RDPQ_AUTOTMEM_ENABLED)
    addiu t1, t0, 1
    bnez t0, JrRa
    sb t1, %lo(RDPQ_AUTOTMEM_ENABLED)

    # Set address to zero, and limit to 4096
    li t0, 4096/8
    sh t0, %lo(RDPQ_AUTOTMEM_LIMIT)
    sh zero, %lo(RDPQ_AUTOTMEM_ADDR_PREV)
    jr ra
    sh zero, %lo(RDPQ_AUTOTMEM_ADDR)

autotmem_end:
    lbu t0, %lo(RDPQ_AUTOTMEM_ENABLED)
    addiu t0, -1
    assert_ge t0, 0, RDPQ_ASSERT_AUTOTMEM_UNPAIRED
    jr ra
    sb t0, %lo(RDPQ_AUTOTMEM_ENABLED)

    .endfunc

    #######################################################################
    # RDPQCmd_AutoTmem_SetTile
    #
    # Identical to SetTile, but will automatically use the current
    # auto-TMEM address.
    #
    # Moreover, it will update the auto-TMEM limit depending on texture format.
    # In fact, 32-bit, YUV and CI textures will reduce the available
    # allocation size to 2048.
    ########################################################################

    .func RDPQCmd_AutoTmem_SetTile
RDPQCmd_AutoTmem_SetTile:
    and t1, a0, 1<<18
    beqz t1, 1f
    lh t0, %lo(RDPQ_AUTOTMEM_ADDR)
    lh t0, %lo(RDPQ_AUTOTMEM_ADDR_PREV)
1:
    lui t1, 0xDD00 ^ 0xF500                 # AutoTmem_SetTile => SET_TILE
    xor a0, t1
    add a0, t0                               # Put auto-TMEM address inside the command

    # Check format and see if we need to lower the auto-TMEM limit
    # The following formats use the upper half of TMEM in a special way,
    # so only the lower half is available for auto-TMEM.
    srl t1, a0, 19
    andi t1, 0x1F
    li t0, (0<<2)|3   # RGBA32
    beq t1, t0, autotmem_lowerlimit
    li t0, (1<<2)|2   # YUV16
    beq t1, t0, autotmem_lowerlimit
    li t0, (2<<2)|0   # CI4
    beq t1, t0, autotmem_lowerlimit
    li t0, (2<<2)|1   # CI8
    bne t1, t0, RDPQCmd_Passthrough8
autotmem_lowerlimit:
    li t0, 2048/8
    j RDPQCmd_Passthrough8
    sh t0, %lo(RDPQ_AUTOTMEM_LIMIT)
    .endfunc


    #######################################################################
    # RDPQCmd_ClearZBuffer
    #
    # Clear the Z-Buffer by filling it with 0xFFFC. This is quite a bit
    # faster than using RDP, though it does require a RDP fence to make
    # sure the Z-Buffer is not in use anymore
    #
    #   a0: Address of the Z-Buffer
    #   a1: Size of the Z-Buffer in bytes
    #   a2: Temporary buffer
    #   a3: Z-Buffer clear value
    #
    #######################################################################

    .func RDPQCmd_ClearZBuffer
RDPQCmd_ClearZBuffer:
    # Prepare the fill value in $v01
    mtc2 a3, $v01.e0
    vor $v01, vzero, $v01.e0
    li t7, %lo(_data_end)

    # If this is hit, increase the temporary buffer in __rdpq_clear_z_with_rsp()
    assert_le t7, 1280, RDPQ_ASSERT_ZCLEAR_INVALID_BUFFER

    # Backup DMEM
    jal ClearZBuf_DMEM_Backup
    li t2, DMA_OUT    # FIXME: switch to ASYNC here when Ares is fixed

    # Now start filling the whole DMEM with the clear code. Start from the
    # end of data, so that we don't risk racing with DMA.
    li s4, 1280
    addiu s3, s4, 4096
1:  sqv $v01, 0x00,s4
    sqv $v01, 0x10,s4
    sqv $v01, 0x20,s4
    addiu s4, 0x40
    bne s4, s3, 1b
    sqv $v01, -0x10,s4

    # Clear the Z-Buffer
    jal DMAWaitReady
    nop
    mtc0 $0, COP0_DMA_SPADDR
    mtc0 a0, COP0_DMA_RAMADDR
    mtc0 a1, COP0_DMA_WRITE

    # Restore DMEM
    li t2, DMA_IN
    jal_and_j ClearZBuf_DMEM_Backup, RSPQ_Loop

    .endfunc

ClearZBuf_DMEM_Backup:
    # Backup the used DMEM area
    li s4, 0
    move s0, a2
    j DMAExec
    addiu t0, t7, -1



# Include RDPQ library
#include <rsp_rdpq.inc>
