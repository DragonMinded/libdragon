
#
# RSP holds a single pointer to a command list. It goes through it.
#
# To write a new command in the list (CPU):
#   * Write the command identifier (first byte at least)
#   * Write all arguments
#   * Terminate with 0x01 (will be overwritten by next command)
#   * Set SIG7 and reset BREAK in SP_STATUS
#
# 0x01 achieves two goals:
#
#   * If reached by RSP, it is executed as command_wait_
#
#

#


# Psuedo-code on RSP:
# * Fetch current command first byte
# * Calculate command size
# * if cur pointer + command size overflow DMEM buffer => 
#      unconditional DMA (rolling back the DL_BUFFFERDESC_START to refetch the current command)
# * if byte at cur pointer + command size is 0x00 => 
#      wait for new input (rolling back the DL_BUFFFERDESC_START to refetch the current command)
# * Run current command. 
#      If it's 0x01 the command is actually "wait for new input" 

#include <rsp.inc>
#include <rsp_dl.inc>

#include "dl_internal.h"

.set noreorder
.set at

.data

.align 3
# Input properties
OVERLAY_TABLE:         .ds.b DL_OVERLAY_TABLE_SIZE
OVERLAY_DESCRIPTORS:   .ds.b (DL_OVERLAY_DESC_SIZE * DL_MAX_OVERLAY_COUNT)

READ_POINTER:    .quad 0
WRITE_POINTER:   .quad 0

DL_DRAM_ADDR:     .long 0
DL_POINTERS_ADDR: .long 0

#DMEM_BUF_START:  .half 0
#DMEM_BUF_END:    .half 0
CURRENT_OVL:     .half -1

.align 3
INTERNAL_COMMAND_TABLE:
commandTableEntry command_noop,           4     # 0x00
commandTableEntry command_wait_new_input, 4     # 0x01
commandTableEntry command_write_status,   4     # 0x02 -- must be even (bit 24 must be 0)
commandTableEntry command_call,           4     # 0x03
commandTableEntry command_jump,           4     # 0x04
commandTableEntry command_ret,            4     # 0x05


    .align 3
# Reserve some extra space of 8 bytes to make sure that DMAs can always load the expected amount of data even if the DRAM address is unaligned
DL_DMEM_BUFFER:    .ds.b DL_DMEM_BUFFER_SIZE + 8
DL_DMEM_BUFFER_END:
DL_DMEM_BUFFER_TERMINATOR: .byte 0x01    # terminate the buffer with command_wait_for_new_input

    .bss

    .align 3
DL_RDRAM_PTR:      .long 0


.align 3
# Overlay data will be loaded at this address
_ovl_data_start:

.text

.globl _start
_start:
    # NOTE: DL_RDRAM_PTR must have been initialized before spinning up RSP!
    li dl_dmem_buf_ptr, 0

.func command_noop
command_wait_new_input:
    # Check if new commands were added in the display list (SIG7)
    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG7
    beqz t0, wakeup
    li t0, SP_WSTATUS_RESET_SIG7

    break
wakeup:
    mtc0 t0, COP0_SP_STATUS

fetch_buffer:
    # Fetch the RDRAM pointer
    lw s0, %lo(DL_RDRAM_PTR)
    add s0, dl_dmem_buf_ptr
fetch_buffer_with_ptr:
    # Update pointer with the value that will be valid after fetch
    sw s0, %lo(DL_RDRAM_PTR)

    # Reset the internal pointer to the first actual byte of the buffer (after
    # taking misalignment into account
    andi dl_dmem_buf_ptr, s0, 7

    # Fetch the whole DMEM buffer. We will use the 0x00 invalid fill value to
    # realize where the buffer is empty.
    li s4, %lo(DL_DMEM_BUFFER)
    jal DMAIn
    li t0, DMA_SIZE(DL_DMEM_BUFFER_SIZE, 1)

    .endfunc
    # fallthrough into the main loop

command_noop:   # invalid command -> repeat the loop
.func loop
loop:
    #define ovl_index t4
    #define cmd_index t5
    #define cmd_desc  t6
    #define cmd_size  t7

    # Read first word
    lw a0, %lo(DL_DMEM_BUFFER) + 0x0 (dl_dmem_buf_ptr)

    # Index into overlay table
    srl t0, a0, 28

    # Index into command table
    srl cmd_index, a0, 23
    andi cmd_index, 0x1FE

    # Overlay 0 is reserved for internal commands
    beqz t0, execute_command
    # Load command descriptor from internal command table if using the default overlay.
    # Otherwise, cmd_desc will be overwritten further down
    lhu cmd_desc, %lo(INTERNAL_COMMAND_TABLE)(cmd_index)

    lh t1, %lo(CURRENT_OVL)

    # Load overlay index from overlay table
    lb ovl_index, %lo(OVERLAY_TABLE)(t0)
    # Check if the requested overlay is already in memory
    beq ovl_index, t1, overlay_loaded
    lhu t0, %lo(_ovl_data_start) + 0x4

    # Save current overlay state
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x8 (t1)
    jal DMAOutAsync
    lw s4, %lo(_ovl_data_start) + 0x0

    # Load overlay data (saved state is included)
    lhu t0, %lo(OVERLAY_DESCRIPTORS) + 0xE (ovl_index)
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x4 (ovl_index)
    jal DMAInAsync
    li s4, %lo(_ovl_data_start)

    # Load overlay code
    lhu t0, %lo(OVERLAY_DESCRIPTORS) + 0xC (ovl_index)
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x0 (ovl_index)
    jal DMAIn
    li s4, %lo(_ovl_text_start) + 0x1000

    # Remember loaded overlay
    sh ovl_index, %lo(CURRENT_OVL)

overlay_loaded:
    # Subtract the command base to determine the final offset into the command table.
    lhu t0, %lo(_ovl_data_start) + 0x6
    sub cmd_index, t0

    # Load command descriptor from overlay command table
    lhu cmd_desc, %lo(_ovl_data_start) + OVERLAY_HEADER_SIZE(cmd_index)

execute_command:
    # Command size
    srl cmd_size, cmd_desc, 10
    andi cmd_size, 0x3C

    # Check if the command is truncated because of buffer overflow. If so,
    # fetch the buffer again. Note that we calculate in t3 the portion of
    # command that fit the buffer: we will use it as rollback for the RDRAM pointer.
    addu t0, dl_dmem_buf_ptr, cmd_size
    bgt t0, DL_DMEM_BUFFER_SIZE, fetch_buffer

    # Load second command word (might be garbage, but will never be read in that case)
    # This is done because the vast majority of commands will use two words or more.
    lw a1, %lo(DL_DMEM_BUFFER) + 0x4 (dl_dmem_buf_ptr)

    # Check if there's an invalid command just after the current command.
    # If so, the previous command might have been fetched partially (as it was
    # being written in RDRAM by CPU), so wait for it to be complete and
    # then fetch the buffer again.
    lbu t0, (t0)
    beqz t0, command_wait_new_input

    # Jump to command. Set ra to the loop function, so that commands can 
    # either do "j loop" or "jr ra" (or a tail call) to get back to the main loop
    li ra, loop
    add dl_dmem_buf_ptr, cmd_size
    jr cmd_desc
    ### BEWARE: NO DELAY SLOT

    #undef ovl_index
    #undef cmd_index
    #undef cmd_desc
    #undef cmd_size
.endfunc
    
    ### BEWARE: NO DELAY SLOT ABOVE

    .func command_call
command_call:
    # a0: command opcode + RDRAM address
    # a1: call slot in DMEM
    lw s0, %lo(DL_RDRAM_PTR)
    add s0, dl_dmem_buf_ptr
    sw s0, 0(a1)  # save return address
    # fallthrough
    .endfunc

    .func command_jump
command_jump:
    # a0: command opcode + RDRAM address
    j fetch_buffer_with_ptr
    move s0, a0
    .endfunc

    .func command_ret
command_ret:
    # a0: command opcode + call slot in DMEM to recover
    j fetch_buffer_with_ptr
    lw s0, 0(a0)
    .endfunc

    .func command_write_status
command_write_status:
    # a0: command opcode + wstatus flags
    j loop
    mtc0 a0, COP0_SP_STATUS
    .endfunc


#include <rsp_dma.inc>

.align 3
# Overlay code will be loaded at this address
_ovl_text_start:
