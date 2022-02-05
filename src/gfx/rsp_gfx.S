#include <rsp_queue.inc>
#include "gfx_internal.h"

    .data

    RSPQ_BeginOverlayHeader GFX_STATE_START, GFX_STATE_END
        RSPQ_DefineCommand GFXCmd_FillTriangle,  32 # 0x20 
        RSPQ_DefineCommand RSPQCmd_Noop,         8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,         8  # Invalid
        RSPQ_DefineCommand RSPQCmd_Noop,         8  # Invalid
        RSPQ_DefineCommand GFXCmd_Passthrough16, 16 # 0x24 TEXTURE_RECTANGLE
        RSPQ_DefineCommand GFXCmd_Passthrough16, 16 # 0x25 TEXTURE_RECTANGLE_FLIP
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x26 SYNC_LOAD
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x27 SYNC_PIPE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x28 SYNC_TILE
        RSPQ_DefineCommand GFXCmd_SyncFull,      8  # 0x29 SYNC_FULL
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x2A SET_KEY_GB
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x2B SET_KEY_R
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x2C SET_CONVERT
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x2D SET_SCISSOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x2E SET_PRIM_DEPTH
        RSPQ_DefineCommand GFXCmd_SetOtherModes, 8  # 0x2F SET_OTHER_MODES
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x30 LOAD_TLUT
        RSPQ_DefineCommand RSPQCmd_Noop,         8  # Invalid
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x32 SET_TILE_SIZE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x33 LOAD_BLOCK
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x34 LOAD_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x35 SET_TILE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x36 FILL_RECTANGLE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x37 SET_FILL_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x38 SET_FOG_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x39 SET_BLEND_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x3A SET_PRIM_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x3B SET_ENV_COLOR
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x3C SET_COMBINE_MODE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x3D SET_TEXTURE_IMAGE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x3E SET_Z_IMAGE
        RSPQ_DefineCommand GFXCmd_Passthrough8,  8  # 0x3F SET_COLOR_IMAGE
    RSPQ_EndOverlayHeader

    .align 3
    # Everything between GFX_STATE_START and GFX_STATE_END is persistent state that is automatically saved by the overlay system.
    # Should be kept consistent with gfx_state_t in gfx_internal.h
GFX_STATE_START:
RDP_DMEM_BUFFER:       .ds.b GFX_RDP_DMEM_BUFFER_SIZE

RDP_OTHER_MODES:       .quad  0

RDP_DRAM_BUFFER:       .long  0
RDP_DRAM_BUFFER_SIZE:  .long  0
RDP_DRAM_BUFFER_END:   .long  0
RDP_DMEM_BUFFER_PTR:   .short 0
RDP_INITIALIZED:       .short 0

GFX_STATE_END:

    .text

    #############################################################
    # GFXCmd_SetOtherModes
    #
    # Does the same as GFXCmd_Passthrough8 and also saves the command in RDP_OTHER_MODES.
    # (not used yet, can theoretically be used to enable partial updates of other modes)
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #############################################################
    .func GFXCmd_SetOtherModes
GFXCmd_SetOtherModes:
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    sw a1, %lo(RDP_OTHER_MODES) + 0x4
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
    jal GFX_RdpWriteBegin
    li t3, 8
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)
    jal_and_j GFX_RdpWriteEnd, RSPQ_Loop
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
    jal GFX_RdpWriteBegin
    li t3, 16
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)
    sw a2, %lo(RDP_DMEM_BUFFER) + 0x8(s1)
    sw a3, %lo(RDP_DMEM_BUFFER) + 0xC(s1)
    jal_and_j GFX_RdpWriteEnd, RSPQ_Loop
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
    jal GFX_RdpWriteBegin
    li t3, 32
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
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x00(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x04(s1)
    sw a2, %lo(RDP_DMEM_BUFFER) + 0x08(s1)
    sw a3, %lo(RDP_DMEM_BUFFER) + 0x0C(s1)
    sw t0, %lo(RDP_DMEM_BUFFER) + 0x10(s1)
    sw t1, %lo(RDP_DMEM_BUFFER) + 0x14(s1)
    sw t2, %lo(RDP_DMEM_BUFFER) + 0x18(s1)
    sw t3, %lo(RDP_DMEM_BUFFER) + 0x1C(s1)
    jal_and_j GFX_RdpWriteEnd, RSPQ_Loop
    .endfunc


    #############################################################
    # GFXCmd_SyncFull
    #
    # Behaves the same as GFXCmd_Passthrough8 and forces a GFX_RdpFlush afterwards.
    # A sync_full command usually denotes the end of a frame, which is why this also
    # resets the state of the RDP stream.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #############################################################
    .func GFXCmd_SyncFull
GFXCmd_SyncFull:
    # This is the same as GFXCmd_Passthrough8, but duplicating it seems easier for now
    jal GFX_RdpWriteBegin
    li t3, 8
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    jal GFX_RdpWriteEnd
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)

    # Afterwards, force flushing the buffer...
    jal GFX_RdpFlush
    nop
    j RSPQ_Loop
    # ...and set the RDP system back to uninitialized
    sh zero, %lo(RDP_INITIALIZED)
    .endfunc


    #############################################################
    # GFX_RdpWriteBegin
    #
    # Opens the RDP command stream for writing. Takes the data size in bytes
    # and returns a pointer in DMEM to write the data to. Call GFX_RdpWriteEnd with the same argument when done.
    # If the RDP buffer is full, will flush it to RDRAM first.
    #
    # ARGS:
    #   t3: Data size
    # RETURNS:
    #   s1: Output pointer
    #############################################################
    .func GFX_RdpWriteBegin
GFX_RdpWriteBegin:
    # Load current buffer pointer
    lhu s1, %lo(RDP_DMEM_BUFFER_PTR)

    # If the requested size fits in the buffer, just return the current pointer
    add s2, s1, t3
    addi s2, -GFX_RDP_DMEM_BUFFER_SIZE
    blez s2, JrRa
    move t1, zero

    # Otherwise, flush the buffer and reset the pointer to zero
    j GFX_RdpFlush
    move s1, zero
    .endfunc


    #############################################################
    # GFX_RdpWriteEnd
    #
    # Closes the RDP command stream for writing. Takes the data size in bytes.
    #
    # ARGS:
    #   t3: Data size
    #############################################################
    .func GFX_RdpWriteEnd
GFX_RdpWriteEnd:
    # Advance dmem buffer pointer
    lhu s2, %lo(RDP_DMEM_BUFFER_PTR)
    add s2, t3
    jr ra
    sh s2, %lo(RDP_DMEM_BUFFER_PTR)
    .endfunc


    #############################################################
    # GFX_RdpFlush
    #
    # Copies the contents of the DMEM buffer to the RDRAM buffer via DMA.
    # If the RDRAM buffer is full, waits for the RDP to finish processing all commands
    # and wraps back to the start.
    # Updates the DP_START and DP_END registers to process the new commands.
    #
    #############################################################
    .func GFX_RdpFlush
GFX_RdpFlush:
    #define dram_size t4
    #define init      t6
    #define dmem_ptr  t7
    #define dram_addr s5
    #define dram_end  s6

    lhu dmem_ptr, %lo(RDP_DMEM_BUFFER_PTR)
    blez dmem_ptr, JrRa

    lhu init,     %lo(RDP_INITIALIZED)
    lw dram_addr, %lo(RDP_DRAM_BUFFER)
    lw dram_size, %lo(RDP_DRAM_BUFFER_SIZE)
    lw dram_end,  %lo(RDP_DRAM_BUFFER_END)
    
    # If RDP is not initialized, always do init
    beqz init, rdp_flush_init_rdp
    move ra2, ra

    # Otherwise, we only need to wrap around if dram buffer would overflow
    add t1, dram_end, dmem_ptr
    ble t1, dram_size, rdp_flush_dma

rdp_flush_init_rdp:
    mfc0 t2, COP0_DP_STATUS

    # Wait for RDP to be done
rdp_flush_wait_rdp_idle:
    andi t1, t2, DP_STATUS_START_VALID | DP_STATUS_END_VALID
    bnez t1, rdp_flush_wait_rdp_idle
    mfc0 t2, COP0_DP_STATUS

    # Clear XBUS/Flush/Freeze
    li t1, DP_WSTATUS_RESET_FLUSH | DP_WSTATUS_RESET_FREEZE | DP_WSTATUS_RESET_XBUS_DMEM_DMA
    mtc0 t1, COP0_DP_STATUS

    mtc0 dram_addr, COP0_DP_START

    # Reset dram pointer
    move dram_end, zero
    li t1, 1
    sh t1, %lo(RDP_INITIALIZED)

rdp_flush_dma:
    # DMA contents of dmem buffer to dram buffer
    add s0, dram_end, dram_addr
    li s4, %lo(RDP_DMEM_BUFFER)
    jal DMAOut # TODO: async?
    addi t0, dmem_ptr, -1

    # Set new end of RDP command buffer
    add s0, dmem_ptr
    mtc0 s0, COP0_DP_END

    # Advance dram pointer and save it
    add dram_end, dmem_ptr
    sw dram_end, %lo(RDP_DRAM_BUFFER_END)

    jr ra2
    # Reset dmem buffer pointer
    sh zero, %lo(RDP_DMEM_BUFFER_PTR)

    #undef dram_size
    #undef dram_addr
    #undef init
    #undef dram_end
    .endfunc
