
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

# Save slots for RDRAM addresses used during nested lists calls.
DL_POINTER_STACK:      .ds.l (DL_MAX_BLOCK_NESTING_LEVEL)

DL_RDRAM_PTR_LOWPRI:  .long 0
# RDRAM address of the high priority command list.
# NOTE: this *MUST* be initialized before running the RSP code.
DL_RDRAM_PTR_HIGHPRI: .long 0

# RDRAM address of the current command list.
# NOTE: this *MUST* be initialized before running the RSP code.
DL_RDRAM_PTR:         .long 0

CURRENT_OVL:          .half 0

PRIMODE_STATUS_CHECK: .half 0


    .align 4
HBANNER0: .ascii " Dragon RSP DL  "
HBANNER1: .ascii "Rasky & Snacchus"

    .align 3
INTERNAL_COMMAND_TABLE:
commandTableEntry command_invalid,          4     # 0x00
commandTableEntry command_wait_new_input,   4     # 0x01
commandTableEntry command_write_status,     4     # 0x02 -- must be even (bit 24 must be 0)
commandTableEntry command_call,             8     # 0x03
commandTableEntry command_jump,             4     # 0x04
commandTableEntry command_ret,              4     # 0x05
commandTableEntry command_swap_buffers,     12    # 0x06
commandTableEntry command_noop,             4     # 0x07
commandTableEntry command_test_wstatus,     8     # 0x08 -- must be even (bit 24 must be 0)
commandTableEntry command_dma,              16    # 0x09

    .bss

    .align 3
DL_DMEM_BUFFER:            .ds.b DL_DMEM_BUFFER_SIZE


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

    # Reset the reading index to the first actual byte of the buffer (after
    # taking misalignment into account)
    andi dl_dmem_buf_ptr, s0, 7

    # Correction for misalignment
    sub s0, dl_dmem_buf_ptr

    # Store the updated pointer
    sw s0, %lo(DL_RDRAM_PTR)

    # Fetch the whole DMEM buffer. We will use the 0x00 invalid fill value to
    # realize where the buffer is empty.
    li s4, %lo(DL_DMEM_BUFFER)
    jal DMAIn
    li t0, DMA_SIZE(DL_DMEM_BUFFER_SIZE, 1)

    # fallthrough into the main loopm but skip the highpri check. This is
    # important because we want to give a chance to the first highpri instruction
    # to clear the SIG5
    .endfunc

command_invalid:  # invalid command -> repeat the loop
command_noop:     # invalid command -> repeat the loop
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
    # NOTE: May be optimised away by getting rid of the indirection and remembering the (code) address of the previously loaded overlay instead.
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
    # NOTE: Could be optimised either by doubling the size of command descriptors (so that the command size can be loaded directly instead of having to decode it),
    #       or by storing the command size in the overlay header instead. The latter would mean that all commands in an overlay need to be the same size though.
    srl cmd_size, cmd_desc, 10
    andi cmd_size, 0x3C

    # Check if the command is truncated because of buffer overflow (that is,
    # it finishes beyond the buffer end). If so, we must refetch the buffer
    # starting from the current position.
    addu t0, dl_dmem_buf_ptr, cmd_size
    bge t0, DL_DMEM_BUFFER_SIZE, fetch_buffer

    # Check if there's an invalid command (0x00) just after the current command.
    # If so, the previous command might have been fetched partially (as it was
    # being written in RDRAM by CPU), so wait for it to be complete and
    # then fetch the buffer again.
    lbu t0, %lo(DL_DMEM_BUFFER)(t0)
    beqz t0, command_wait_new_input

    # Load second to fourth command words (might be garbage, but will never be read in that case)
    # This saves some instructions in all overlays that use more than 4 bytes per command.
    lw a1, %lo(DL_DMEM_BUFFER) + 0x4 (dl_dmem_buf_ptr)
    lw a2, %lo(DL_DMEM_BUFFER) + 0x8 (dl_dmem_buf_ptr)
    lw a3, %lo(DL_DMEM_BUFFER) + 0xC (dl_dmem_buf_ptr)

    jal dl_check_highpri
    add dl_dmem_buf_ptr, cmd_size

    # Jump to command. Set ra to the loop function, so that commands can 
    # either do "j loop" or "jr ra" (or a tail call) to get back to the main loop
    jr cmd_desc
    li ra, %lo(loop)

    #undef ovl_index
    #undef cmd_index
    #undef cmd_desc
    #undef cmd_size
    .endfunc

    ############################################################
    # dl_check_highpri
    #
    # Polling function. Check whether the highpri mode has been
    # requested by the CPU, and if so start executing it right away.
    #
    # This is called by the main loop automatically between each
    # command, but can be also polled by any overlay function
    # that takes a long time and want to yield. In this case, 
    # the same command will be executed again when the highpri queue
    # is finished, so make sure there is state to continue rather
    # than restart the execution.
    #
    # ARGS:
    #   t7: size of the current command
    ############################################################

    .func dl_check_highpri
dl_check_highpri:
    lhu t1, %lo(PRIMODE_STATUS_CHECK)
    mfc0 t0, COP0_SP_STATUS
    and t0, t1
    beqz t0, JrRa
    nop
    li a0, DL_HIGHPRI_CALL_SLOT<<2
    li a1, DL_LOWPRI_CALL_SLOT<<2
    li a2, 0
    sub dl_dmem_buf_ptr, t7
    #fallthrough
    .endfunc

    .func command_swap_buffers
command_swap_buffers:
    sh a2, %lo(PRIMODE_STATUS_CHECK)
    lw a0, %lo(DL_POINTER_STACK)(a0)
    #fallthrough
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
    sw s0, %lo(DL_POINTER_STACK)(a1)  # save return address
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
    #   a0: DMEM address of the save slot (plus command opcode)
    #############################################################
    .func command_ret
command_ret:
    # a0: command opcode + call slot in DMEM to recover
    j fetch_buffer_with_ptr
    lw s0, %lo(DL_POINTER_STACK)(a0)
    .endfunc

    #############################################################
    # command_test_wstatus
    #
    # Like command_write_status, writes COP0 SP status register with a specified value,
    # but first waits until the SP status AND'ed with the specified bitmask is zero.
    #
    # ARGS:
    #   a0: value to write into COP0_SP_STATUS
    #   a1: bitmask to test COP0_SP_STATUS for
    #############################################################
    .func command_test_wstatus
command_test_wstatus:
    jal SpStatusWait
    move t2, a1
    # fallthrough
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

    #############################################################
    # command_dma
    #
    # Executes an arbitrary DMA request by taking the arguments to DMAExec from a0-a3
    #
    # ARGS:
    #   a0: RDRAM address
    #   a1: DMEM address
    #   a2: length/height
    #   a3: flags (in/out, sync/async)
    #############################################################
    .func command_dma
command_dma:
    move s0, a0
    move s4, a1
    move t0, a2
    j DMAExec
    move t2, a3
    .endfunc

#include <rsp_dma.inc>

    .align 3
# Overlay code will be loaded at this address
_ovl_text_start:
