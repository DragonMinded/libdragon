
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

# Input properties
OVERLAY_TABLE:         .ds.b DL_OVERLAY_TABLE_SIZE
OVERLAY_DESCRIPTORS:   .ds.b (DL_OVERLAY_DESC_SIZE * DL_MAX_OVERLAY_COUNT)

# RDRAM address of the current command list.
# NOTE: this *MUST* be initialized before running the RSP code.
DL_RDRAM_PTR:         .long 0
# RDRAM address of the high priority command list.
# NOTE: this *MUST* be initialized before running the RSP code.
DL_RDRAM_PTR_HIGHPRI: .long 0 

CURRENT_OVL:     .half -1

    .align 4
HBANNER0: .ascii " Dragon RSP DL  "
HBANNER1: .ascii "Rasky & Snacchus"

    .align 3
INTERNAL_COMMAND_TABLE:
commandTableEntry command_invalid,        4     # 0x00
commandTableEntry command_wait_new_input, 4     # 0x01
commandTableEntry command_write_status,   4     # 0x02 -- must be even (bit 24 must be 0)
commandTableEntry command_call,           4     # 0x03
commandTableEntry command_jump,           4     # 0x04
commandTableEntry command_ret,            4     # 0x05
commandTableEntry command_call_highpri,   4     # 0x06
commandTableEntry command_noop,           4     # 0x07

    .align 3
DL_DMEM_BUFFER:            .ds.b DL_DMEM_BUFFER_SIZE
DL_DMEM_BUFFER_TERMINATOR: .byte 0x01    # terminate the buffer with command_wait_for_new_input


    .bss

# Save slots for RDRAM addresses used during nested lists calls.
DL_POINTER_STACK:     .ds.l (8+1)


    .align 3
# Overlay data will be loaded at this address
_ovl_data_start:

    .text

.globl _start
_start:
    # NOTE: DL_RDRAM_PTR must have been initialized before spinning up RSP!
    li dl_dmem_buf_ptr, 0

    .func command_wait_new_input
command_wait_new_input:
    # Check if new commands were added in the display list (SIG7)
    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG7
    bnez t0, wakeup
    li t0, SP_WSTATUS_RESET_SIG7

    # No new commands yet, go to sleep
    break
wakeup:
    mtc0 t0, COP0_SP_STATUS

fetch_buffer:
    # Fetch the RDRAM pointer, and adjust it to the current reading index.
    # We will fetch commands starting from there
    lw s0, %lo(DL_RDRAM_PTR)
    add s0, dl_dmem_buf_ptr
fetch_buffer_with_ptr:
    # Store the updated pointer
    sw s0, %lo(DL_RDRAM_PTR)

    # Reset the reading index to the first actual byte of the buffer (after
    # taking misalignment into account)
    andi dl_dmem_buf_ptr, s0, 7

    # Fetch the whole DMEM buffer. We will use the 0x00 invalid fill value to
    # realize where the buffer is empty.
    li s4, %lo(DL_DMEM_BUFFER)
    jal DMAIn
    li t0, DMA_SIZE(DL_DMEM_BUFFER_SIZE, 1)

    .endfunc
    # fallthrough into the main loop

command_invalid:  # invalid command -> repeat the loop
command_noop:     # invalid command -> repeat the loop
.func loop
loop:
    #define ovl_index t4
    #define cmd_index t5
    #define cmd_desc  t6
    #define cmd_size  t7

    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG6
    bnez t0, command_call_highpri

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

    # Check if the command is truncated because of buffer overflow (that is,
    # it finishes beyond the buffer end). If so, we must refetch the buffer
    # starting from the current position.
    addu t0, dl_dmem_buf_ptr, cmd_size
    bgt t0, DL_DMEM_BUFFER_SIZE, fetch_buffer

    # Load second command word (might be garbage, but will never be read in that case)
    # This is done because the vast majority of commands will use two words or more.
    lw a1, %lo(DL_DMEM_BUFFER) + 0x4 (dl_dmem_buf_ptr)

    # Check if there's an invalid command (0x00) just after the current command.
    # If so, the previous command might have been fetched partially (as it was
    # being written in RDRAM by CPU), so wait for it to be complete and
    # then fetch the buffer again.
    lbu t0, %lo(DL_DMEM_BUFFER)(t0)
    beqz t0, command_wait_new_input

    # Jump to command. Set ra to the loop function, so that commands can 
    # either do "j loop" or "jr ra" (or a tail call) to get back to the main loop
    li ra, %lo(loop)
    jr cmd_desc
    add dl_dmem_buf_ptr, cmd_size

    #undef ovl_index
    #undef cmd_index
    #undef cmd_desc
    #undef cmd_size
.endfunc

    ############################################################
    # command_call_highpri
    #
    # Force execution of the high-priority list by doing
    # a nested call. This is invoked when SIG6 is set, but
    # it can also explicitly scheduled in case it is
    # preferred to be done at specific moments rather than
    # potentially between any command.
    ############################################################
    .func command_call_highpri
command_call_highpri:
    lw a0, %lo(DL_RDRAM_PTR_HIGHPRI)
    li a1, %lo(DL_POINTER_STACK+8*4)
    # fallthrough
    .endfunc
    
    #############################################################
    # command_call
    #
    # Do a nested call to a different command list. Save the
    # current RDRAM position to be able to resume execution
    # later.
    #
    # ARGS:
    #   a0: New RDRAM address (plus command opcode)
    #   a1: DMEM address of the save slot for the current address
    #############################################################
    .func command_call
command_call:
    # a0: command opcode + RDRAM address
    # a1: call slot in DMEM
    lw s0, %lo(DL_RDRAM_PTR)
    add s0, dl_dmem_buf_ptr
    sw s0, 0(a1)  # save return address
    # fallthrough
    .endfunc

    #############################################################
    # command_jump
    #
    # Jump to a different command list. The current RDRAM position
    # is lost.
    #
    # ARGS:
    #   a0: New RDRAM address (plus command opcode)
    #############################################################
    .func command_jump
command_jump:
    # a0: command opcode + RDRAM address
    j fetch_buffer_with_ptr
    move s0, a0
    .endfunc

    #############################################################
    # command_ret
    #
    # Restore a previously saved RDRAM position and jump to it.
    #
    # ARGS:
    #   a0: DMDM address of the save slot (plus command opcode)
    #############################################################
    .func command_ret
command_ret:
    # a0: command opcode + call slot in DMEM to recover
    j fetch_buffer_with_ptr
    lw s0, 0(a0)
    .endfunc

    #############################################################
    # command_write_status
    #
    # Write COP0 SP status register with a specified value
    #
    # ARGS:
    #   a0: value to write into COP0_SP_STATUS
    #############################################################
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
