#include "ugfx_internal.h"
#include "../dl/rsp_dl.S"

    .section .data.overlay

    # Each overlay requires a header. The first two arguments to overlayHeader are start and end of the area in DMEM that should
    # be saved when the overlay is swapped out. Put your persistent state here.
    # The last argument is the command base (The id of the first command in the overlay).
    overlayHeader UGFX_STATE_START, UGFX_STATE_END, 0x20

    .align 1
    # The command table maps the command ids to command descriptors, which are the IMEM address and the size
    # of the command encoded as a 16 bit value.
    # The first argument of commandTableEntry is just the text label of the command, the second is the command size in bytes.
COMMAND_TABLE:
    commandTableEntry command_noop,               8
    commandTableEntry command_noop,               8
    commandTableEntry command_noop,               8
    commandTableEntry command_noop,               8
    commandTableEntry command_rdp_passthrough_16, 16 # TEXTURE_RECTANGLE
    commandTableEntry command_rdp_passthrough_16, 16 # TEXTURE_RECTANGLE_FLIP
    commandTableEntry command_noop,               8
    commandTableEntry command_rdp_passthrough_8,  8  # SYNC_PIPE
    commandTableEntry command_rdp_passthrough_8,  8  # SYNC_TILE
    commandTableEntry command_sync_full,          8  # SYNC_FULL
    commandTableEntry command_rdp_passthrough_8,  8  # SET_KEY_GB
    commandTableEntry command_rdp_passthrough_8,  8  # SET_KEY_R
    commandTableEntry command_rdp_passthrough_8,  8  # SET_CONVERT
    commandTableEntry command_rdp_passthrough_8,  8  # SET_SCISSOR
    commandTableEntry command_rdp_passthrough_8,  8  # SET_PRIM_DEPTH
    commandTableEntry command_set_other_modes,    8  # SET_OTHER_MODES
    commandTableEntry command_rdp_passthrough_8,  8  # LOAD_TLUT
    commandTableEntry command_rdp_passthrough_8,  8  # SYNC_LOAD
    commandTableEntry command_rdp_passthrough_8,  8  # SET_TILE_SIZE
    commandTableEntry command_rdp_passthrough_8,  8  # LOAD_BLOCK
    commandTableEntry command_rdp_passthrough_8,  8  # LOAD_TILE
    commandTableEntry command_rdp_passthrough_8,  8  # SET_TILE
    commandTableEntry command_rdp_passthrough_8,  8  # FILL_RECTANGLE
    commandTableEntry command_rdp_passthrough_8,  8  # SET_FILL_COLOR
    commandTableEntry command_rdp_passthrough_8,  8  # SET_FOG_COLOR
    commandTableEntry command_rdp_passthrough_8,  8  # SET_BLEND_COLOR
    commandTableEntry command_rdp_passthrough_8,  8  # SET_PRIM_COLOR
    commandTableEntry command_rdp_passthrough_8,  8  # SET_ENV_COLOR
    commandTableEntry command_rdp_passthrough_8,  8  # SET_COMBINE_MODE
    commandTableEntry command_rdp_passthrough_8,  8  # SET_TEXTURE_IMAGE
    commandTableEntry command_rdp_passthrough_8,  8  # SET_Z_IMAGE
    commandTableEntry command_rdp_passthrough_8,  8  # SET_COLOR_IMAGE

    .align 3
    # Everything between UGFX_STATE_START and UGFX_STATE_END is persistent state that is automatically saved by the overlay system.
    # Should be kept consistent with ugfx_state_t in ugfx_internal.h
UGFX_STATE_START:
RDP_DMEM_BUFFER:       .ds.b UGFX_RDP_DMEM_BUFFER_SIZE

RDP_OTHER_MODES:       .quad  0

RDP_DRAM_BUFFER:       .long  0
RDP_DRAM_BUFFER_SIZE:  .long  0
RDP_DRAM_BUFFER_END:   .long  0
RDP_DMEM_BUFFER_PTR:   .short 0
RDP_INITIALIZED:       .short 0

UGFX_STATE_END:

    .section .bss.overlay

    .text 1

    #############################################################
    # command_set_other_modes
    #
    # Does the same as command_rdp_passthrough_8 and also saves the command in RDP_OTHER_MODES.
    # (not used yet, can theoretically be used to enable partial updates of other modes)
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #############################################################
    .func command_set_other_modes
command_set_other_modes:
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    sw a1, %lo(RDP_OTHER_MODES) + 0x4
    .endfunc

    
    #############################################################
    # command_rdp_passthrough_8
    #
    # Forwards the RDP command contained in a0 and a1 to the RDP stream.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #############################################################
    .func command_rdp_passthrough_8
command_rdp_passthrough_8:
    jal rdp_write_begin
    li t3, 8
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)
    jal_and_j rdp_write_end, loop
    .endfunc


    #############################################################
    # command_rdp_passthrough_16
    #
    # Forwards the RDP command contained in a0-a3 to the RDP stream.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #   a2: Third 4 bytes of RDP command
    #   a3: Fourth 4 bytes of RDP command
    #############################################################
    .func command_rdp_passthrough_16
command_rdp_passthrough_16:
    jal rdp_write_begin
    li t3, 16
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)
    sw a2, %lo(RDP_DMEM_BUFFER) + 0x8(s1)
    sw a3, %lo(RDP_DMEM_BUFFER) + 0xC(s1)
    jal_and_j rdp_write_end, loop
    .endfunc


    #############################################################
    # command_sync_full
    #
    # Behaves the same as command_rdp_passthrough_8 and forces a rdp_flush afterwards.
    # A sync_full command usually denotes the end of a frame, which is why this also
    # resets the state of the RDP stream.
    #
    # ARGS:
    #   a0: First 4 bytes of RDP command
    #   a1: Second 4 bytes of RDP command
    #############################################################
    .func command_sync_full
command_sync_full:
    # This is the same as command_rdp_passthrough_8, but duplicating it seems easier for now
    jal rdp_write_begin
    li t3, 8
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    jal rdp_write_end
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)

    # Afterwards, force flushing the buffer...
    jal rdp_flush
    nop
    j loop
    # ...and set the RDP system back to uninitialized
    sh zero, %lo(RDP_INITIALIZED)
    .endfunc


    #############################################################
    # rdp_write_begin
    #
    # Opens the RDP command stream for writing. Takes the data size in bytes
    # and returns a pointer in DMEM to write the data to. Call rdp_write_end with the same argument when done.
    # If the RDP buffer is full, will flush it to RDRAM first.
    #
    # ARGS:
    #   t3: Data size
    # RETURNS:
    #   s1: Output pointer
    #############################################################
    .func rdp_write_begin
rdp_write_begin:
    # Load current buffer pointer
    lhu s1, %lo(RDP_DMEM_BUFFER_PTR)

    # If the requested size fits in the buffer, just return the current pointer
    add s2, s1, t3
    addi s2, -UGFX_RDP_DMEM_BUFFER_SIZE
    blez s2, JrRa
    move t1, zero

    # Otherwise, flush the buffer and reset the pointer to zero
    j rdp_flush
    move s1, zero
    .endfunc


    #############################################################
    # rdp_write_end
    #
    # Closes the RDP command stream for writing. Takes the data size in bytes.
    #
    # ARGS:
    #   t3: Data size
    #############################################################
    .func rdp_write_end
rdp_write_end:
    # Advance dmem buffer pointer
    lhu s2, %lo(RDP_DMEM_BUFFER_PTR)
    add s2, t3
    jr ra
    sh s2, %lo(RDP_DMEM_BUFFER_PTR)
    .endfunc


    #############################################################
    # rdp_flush
    #
    # Copies the contents of the DMEM buffer to the RDRAM buffer via DMA.
    # If the RDRAM buffer is full, waits for the RDP to finish processing all commands
    # and wraps back to the start.
    # Updates the DP_START and DP_END registers to process the new commands.
    #
    #############################################################
    .func rdp_flush
rdp_flush:
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
