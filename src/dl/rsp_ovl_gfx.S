#include <rsp.inc>
#include <rsp_dl.inc>

#define RDP_BUFFER_SIZE 0x100

.section .overlay.data

overlayHeader OVL_GFX_SAVED_DATA_START, OVL_GFX_SAVED_DATA_END, 0x7E

.align 1
COMMAND_TABLE:
commandTableEntry command_noop, 8
commandTableEntry command_noop, 8
commandTableEntry command_noop, 8
commandTableEntry command_noop, 8
commandTableEntry command_rdp_passthrough_16, 16 # TEXTURE_RECTANGLE
commandTableEntry command_rdp_passthrough_16, 16 # TEXTURE_RECTANGLE_FLIP
commandTableEntry command_noop, 8
commandTableEntry command_rdp_passthrough_8, 8   # SYNC_PIPE
commandTableEntry command_rdp_passthrough_8, 8   # SYNC_TILE
commandTableEntry command_sync_full, 8           # SYNC_FULL
commandTableEntry command_rdp_passthrough_8, 8   # SET_KEY_GB
commandTableEntry command_rdp_passthrough_8, 8   # SET_KEY_R
commandTableEntry command_rdp_passthrough_8, 8   # SET_CONVERT
commandTableEntry command_rdp_passthrough_8, 8   # SET_SCISSOR
commandTableEntry command_rdp_passthrough_8, 8   # SET_PRIM_DEPTH
commandTableEntry command_set_other_modes, 8     # SET_OTHER_MODES
commandTableEntry command_rdp_passthrough_8, 8   # LOAD_TLUT
commandTableEntry command_rdp_passthrough_8, 8   # SYNC_LOAD
commandTableEntry command_rdp_passthrough_8, 8   # SET_TILE_SIZE
commandTableEntry command_rdp_passthrough_8, 8   # LOAD_BLOCK
commandTableEntry command_rdp_passthrough_8, 8   # LOAD_TILE
commandTableEntry command_rdp_passthrough_8, 8   # SET_TILE
commandTableEntry command_rdp_passthrough_8, 8   # FILL_RECTANGLE
commandTableEntry command_rdp_passthrough_8, 8   # SET_FILL_COLOR
commandTableEntry command_rdp_passthrough_8, 8   # SET_FOG_COLOR
commandTableEntry command_rdp_passthrough_8, 8   # SET_BLEND_COLOR
commandTableEntry command_rdp_passthrough_8, 8   # SET_PRIM_COLOR
commandTableEntry command_rdp_passthrough_8, 8   # SET_ENV_COLOR
commandTableEntry command_rdp_passthrough_8, 8   # SET_COMBINE_MODE
commandTableEntry command_rdp_passthrough_8, 8   # SET_TEXTURE_IMAGE
commandTableEntry command_rdp_passthrough_8, 8   # SET_Z_IMAGE
commandTableEntry command_rdp_passthrough_8, 8   # SET_COLOR_IMAGE

.section .overlay.bss

.align 3
OVL_GFX_SAVED_DATA_START:
RDP_DMEM_BUFFER:      .ds.b RDP_BUFFER_SIZE

RDP_OTHER_MODES:       .quad  0

RDP_DRAM_BUFFER:       .long  0
RDP_DRAM_BUFFER_SIZE:  .long  0
RDP_DRAM_BUFFER_END:   .long  0
RDP_DMEM_BUFFER_PTR:   .short 0
RDP_INITIALIZED:       .short 0

OVL_GFX_SAVED_DATA_END:

.section .overlay.text

command_set_other_modes:
    sw a0, %lo(RDP_OTHER_MODES) + 0x0
    sw a1, %lo(RDP_OTHER_MODES) + 0x4
    
command_rdp_passthrough_8:
    jal rdp_write_begin
    li t0, 8
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)
    jal_and_j rdp_write_end, loop

command_rdp_passthrough_16:
    jal rdp_write_begin
    li t0, 16
    lw a2, %lo(INPUT_BUFFER) - 0x8 (input_buf_ptr)
    lw a3, %lo(INPUT_BUFFER) - 0x4 (input_buf_ptr)
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)
    sw a2, %lo(RDP_DMEM_BUFFER) + 0x8(s1)
    sw a3, %lo(RDP_DMEM_BUFFER) + 0xC(s1)
    jal_and_j rdp_write_end, loop

command_sync_full:
    # This is the same as command_rdp_passthrough_8, but duplicating it seems easier for now
    jal rdp_write_begin
    li t0, 8
    sw a0, %lo(RDP_DMEM_BUFFER) + 0x0(s1)
    jal rdp_write_end
    sw a1, %lo(RDP_DMEM_BUFFER) + 0x4(s1)

    # Afterwards, force flushing the buffer...
    jal rdp_flush
    nop
    j loop
    # ...and set the RDP system to uninitialized
    sh zero, %lo(RDP_INITIALIZED)

# INPUT:
#   t0: Data size
# OUTPUT:
#   s1: Output pointer
rdp_write_begin:
    # Load current buffer pointer
    lw s1, %lo(RDP_DMEM_BUFFER_PTR)

    # If the requested size fits in the buffer, just return the current pointer
    add s2, s1, t0
    addi s2, -RDP_BUFFER_SIZE
    blez s2, JrRa
    move t1, zero

    # Otherwise, flush the buffer and reset the pointer to zero
    j rdp_flush
    move s1, zero

# INPUT:
#   t0: Data size
rdp_write_end:
    # Advance dmem buffer pointer
    lw s2, %lo(RDP_DMEM_BUFFER_PTR)
    add s2, t0
    jr ra
    sw s2, %lo(RDP_DMEM_BUFFER_PTR)

rdp_flush:
    #define dram_size t4
    #define dram_addr t5
    #define init      t6
    #define dram_end  s5

    lh t0, %lo(RDP_DMEM_BUFFER_PTR)
    blez t0, JrRa

    lh init,      %lo(RDP_INITIALIZED)
    lw dram_end,  %lo(RDP_DRAM_BUFFER_END)
    lw dram_size, %lo(RDP_DRAM_BUFFER_SIZE)
    
    # If RDP is not initialized, always do init
    beqz init, rdp_flush_init_rdp
    move ra2, ra

    # Otherwise, we only need to wrap around if dram buffer would overflow
    add t1, dram_end, t0
    ble t1, dram_size, rdp_flush_dma

rdp_flush_init_rdp:
    mfc0 t2, COP0_DP_STATUS

    # Wait for RDP to be done
rdp_flush_wait_rdp_idle:
    andi t1, t2, DP_STATUS_START_VALID | DP_STATUS_END_VALID
    bnez t1, rdp_flush_wait_rdp_idle
    mfc0 t2, COP0_DP_STATUS

    lw dram_addr, %lo(RDP_DRAM_BUFFER)

    # Clear XBUS/Flush/Freeze
    li t1, DP_WSTATUS_RESET_FLUSH | DP_WSTATUS_RESET_FREEZE | DP_WSTATUS_RESET_XBUS_DMEM_DMA
    mtc0 t1, COP0_DP_STATUS

    mtc0 dram_addr, COP0_DP_START

    # Reset dram pointer
    move dram_end, zero

rdp_flush_dma:
    # DMA contents of dmem buffer to dram buffer
    add s0, dram_end, dram_addr
    jal DMAOut # TODO: async?
    li s4, %lo(RDP_DMEM_BUFFER)

    # Set new end of RDP command buffer
    mtc0 s0, COP0_DP_END

    # Advance dram pointer and save it
    add dram_end, t0
    sw dram_end, %lo(RDP_DRAM_BUFFER_END)

    jr ra2
    # Reset dmem buffer pointer
    sh zero, %lo(RDP_DMEM_BUFFER_PTR)

    #undef dram_size
    #undef dram_addr
    #undef init
    #undef dram_end
