##########################################################################
# RSP QUEUE UCODE
##########################################################################
#
# The documentation for the RSP Queue engine is in rspq.h (API) and
# rspq.c (implementation). The latter contains an architectural overview
# that also describes the behavior of this ucode.
#
##########################################################################

#include <rsp.inc>
#include <rsp_queue.inc>

#include "rspq_internal.h"

    .set noreorder
    .set at

    .data

# Overlay tables. See rsp_overlay_t in rsp.c
RSPQ_OVERLAY_TABLE:           .ds.b RSPQ_OVERLAY_TABLE_SIZE
RSPQ_OVERLAY_DESCRIPTORS:     .ds.b (RSPQ_OVERLAY_DESC_SIZE * RSPQ_MAX_OVERLAY_COUNT)

# Save slots for RDRAM addresses used during nested lists calls.
# Notice that the two extra slots are used to save the lowpri
# and highpri current pointer (used when switching between the two)
RSPQ_POINTER_STACK:           .ds.l (RSPQ_MAX_BLOCK_NESTING_LEVEL+2)

# RDRAM address of the current command list.
RSPQ_RDRAM_PTR:               .long 0

# Index (not ID!) of the current overlay, as byte offset in the descriptor array
RSPQ_CURRENT_OVL:             .half 0

    .align 4
    .ascii "Dragon RSP Queue"
    .ascii "Rasky & Snacchus"

    .align 3
RSPQ_INTERNAL_COMMAND_TABLE:
RSPQ_DefineCommand RSPQCmd_WaitNewInput,    0     # 0x00
RSPQ_DefineCommand RSPQCmd_Noop,            4     # 0x01
RSPQ_DefineCommand RSPQCmd_Jump,            4     # 0x02
RSPQ_DefineCommand RSPQCmd_Call,            8     # 0x03
RSPQ_DefineCommand RSPQCmd_Ret,             4     # 0x04
RSPQ_DefineCommand RSQPCmd_Dma,             16    # 0x05
RSPQ_DefineCommand RSPQCmd_WriteStatus,     4     # 0x06 -- must be even (bit 24 must be 0)
RSPQ_DefineCommand RSPQCmd_SwapBuffers,     12    # 0x07
RSPQ_DefineCommand RSPQCmd_TestWriteStatus, 8     # 0x08 -- must be even (bit 24 must be 0)

#if RSPQ_DEBUG
RSPQ_LOG_IDX:                .long 0
RSPQ_LOG:                    .ds.l 16
RSPQ_LOG_END:                .long 0xFFFFFFFF
#endif

    .bss

    .align 3
RSPQ_DMEM_BUFFER:            .ds.b RSPQ_DMEM_BUFFER_SIZE


    .align 4
# Overlay data will be loaded at this address
_ovl_data_start:

    .text

    .globl _start
_start:
    li rspq_dmem_buf_ptr, 0

    .func RSPQCmd_WaitNewInput
RSPQCmd_WaitNewInput:
    # Check if new commands were added in the display list (SIG_MORE)
    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG_MORE
    bnez t0, wakeup
    li t0, SP_WSTATUS_CLEAR_SIG_MORE

    # No new commands yet, go to sleep
    break
wakeup:
    mtc0 t0, COP0_SP_STATUS

rspq_fetch_buffer:
    # Fetch the RDRAM pointer, and adjust it to the current reading index.
    # We will fetch commands starting from there
    lw s0, %lo(RSPQ_RDRAM_PTR)
    add s0, rspq_dmem_buf_ptr
rspq_fetch_buffer_with_ptr:
    # Buffer into which the DMA will be performed
    # NOTE: this instruction has been moved here to workaround what
    # seems to be a RSP hardware bug, related to weird pipeline
    # behavior during RSP un-halt. If you move this opcode later
    # just before "jal DMAIn", bruteforce tests in testrom will start
    # to fail. Unfortunately, we are still not able to isolate this
    # bug.
    li s4, %lo(RSPQ_DMEM_BUFFER)

    # Reset the reading index to the first actual byte of the buffer (after
    # taking misalignment into account)
    andi rspq_dmem_buf_ptr, s0, 7

    # Correction for misalignment
    sub s0, rspq_dmem_buf_ptr

    # Store the updated pointer
    sw s0, %lo(RSPQ_RDRAM_PTR)

    # Fetch the whole DMEM buffer. We will use the 0x00 invalid fill value to
    # realize where the buffer is empty.
    jal DMAIn
    li t0, DMA_SIZE(RSPQ_DMEM_BUFFER_SIZE, 1)

    # fallthrough into the main loop
    .endfunc

RSPQCmd_Noop:     # noop -> repeat the loop
    .func RSPQ_Loop
RSPQ_Loop:
    #define ovl_index t4
    #define cmd_index t5    // referenced in rspq_assert_invalid_overlay
    #define cmd_desc  t6
    #define cmd_size  t7

    jal RSPQ_CheckHighpri
    li t0, 0

    # Read first word
    lw a0, %lo(RSPQ_DMEM_BUFFER) + 0x0 (rspq_dmem_buf_ptr)

#if RSPQ_DEBUG
    lw t0, %lo(RSPQ_LOG_IDX)
    sw a0, %lo(RSPQ_LOG)(t0)
    addi t0, 4
    andi t0, 16*4-1
    sw t0, %lo(RSPQ_LOG_IDX)
#endif

    # Index into overlay table
    srl t0, a0, 28

    # Index into command table
    srl cmd_index, a0, 23
    andi cmd_index, 0x1FE

    # Overlay 0 is reserved for internal commands
    beqz t0, rspq_execute_command
    # Load command descriptor from internal command table if using the default overlay.
    # Otherwise, cmd_desc will be overwritten further down
    lhu cmd_desc, %lo(RSPQ_INTERNAL_COMMAND_TABLE)(cmd_index)

    lhu t1, %lo(RSPQ_CURRENT_OVL)

    # Load overlay index from overlay table
    # NOTE: May be optimised away by getting rid of the indirection and remembering the (code) address of the previously loaded overlay instead.
    lbu ovl_index, %lo(RSPQ_OVERLAY_TABLE)(t0)
    #if RSPQ_DEBUG
    assert_ne ovl_index, 0, ASSERT_INVALID_OVERLAY
    #endif
    # Check if the requested overlay is already in memory
    beq ovl_index, t1, rspq_overlay_loaded
    lhu t0, %lo(_ovl_data_start) + 0x4

    # Save current overlay state
    lw s0, %lo(RSPQ_OVERLAY_DESCRIPTORS) + 0x8 (t1)
    jal DMAOutAsync
    lw s4, %lo(_ovl_data_start) + 0x0

    # Load overlay data (saved state is included)
    lhu t0, %lo(RSPQ_OVERLAY_DESCRIPTORS) + 0xE (ovl_index)
    lw s0, %lo(RSPQ_OVERLAY_DESCRIPTORS) + 0x4 (ovl_index)
    jal DMAInAsync
    li s4, %lo(_ovl_data_start)

    # Load overlay code
    lhu t0, %lo(RSPQ_OVERLAY_DESCRIPTORS) + 0xC (ovl_index)
    lw s0, %lo(RSPQ_OVERLAY_DESCRIPTORS) + 0x0 (ovl_index)
    jal DMAIn
    li s4, %lo(_ovl_text_start) + 0x1000

    # Remember loaded overlay
    sh ovl_index, %lo(RSPQ_CURRENT_OVL)

rspq_overlay_loaded:
    # Subtract the command base to determine the final offset into the command table.
    lhu t0, %lo(_ovl_data_start) + 0x6
    sub cmd_index, t0

    # Load command descriptor from overlay command table
    lhu cmd_desc, %lo(_ovl_data_start) + OVERLAY_HEADER_SIZE(cmd_index)

rspq_execute_command:
    #if RSPQ_DEBUG
    assert_ne cmd_desc, 0, ASSERT_INVALID_COMMAND
    #endif

    # Command size
    # NOTE: Could be optimised either by doubling the size of command descriptors (so that the command size can be loaded directly instead of having to decode it),
    #       or by storing the command size in the overlay header instead. The latter would mean that all commands in an overlay need to be the same size though.
    srl cmd_size, cmd_desc, 10

    # Check if the command is truncated because of buffer overflow (that is,
    # it finishes beyond the buffer end). If so, we must refetch the buffer
    # starting from the current position.
    # Notice that we use "bge" instead of "bgt" so we actually refetch the buffer
    # also if the current command ends exactly at buffer end; this is slighly
    # wasteful but saves us a few instructions (that would be required to check
    # whether we are then trying to load a command outside of the buffer).
    addu t0, rspq_dmem_buf_ptr, cmd_size
    bge t0, RSPQ_DMEM_BUFFER_SIZE, rspq_fetch_buffer

    # Load second to fourth command words (might be garbage, but will never be read in that case)
    # This saves some instructions in all overlays that use more than 4 bytes per command.
    lw a1, %lo(RSPQ_DMEM_BUFFER) + 0x4 (rspq_dmem_buf_ptr)
    lw a2, %lo(RSPQ_DMEM_BUFFER) + 0x8 (rspq_dmem_buf_ptr)
    lw a3, %lo(RSPQ_DMEM_BUFFER) + 0xC (rspq_dmem_buf_ptr)
    add rspq_dmem_buf_ptr, cmd_size

    # Jump to command. Set ra to the loop function, so that commands can 
    # either do "j RSPQ_Loop" or "jr ra" (or a tail call) to get back to the main loop
    jr cmd_desc
    li ra, %lo(RSPQ_Loop)

    #undef ovl_index
    #undef cmd_index
    #undef cmd_desc
    #undef cmd_size
    .endfunc

    ############################################################
    # RSPQ_CheckHighpri
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
    #   t0: size of the current command
    ############################################################

    .func RSPQ_CheckHighpri
RSPQ_CheckHighpri:
    # We need to enter high-pri mode if highpri was requested and it is not
    # already running (to avoid infinite loops).
    mfc0 t2, COP0_SP_STATUS
    andi t2, SP_STATUS_SIG_HIGHPRI_REQUESTED | SP_STATUS_SIG_HIGHPRI_RUNNING
    bne t2, SP_STATUS_SIG_HIGHPRI_REQUESTED, JrRa

    # Switch to highpri mode, calling RSPQCMD_SwapBuffers. We prepare the
    # argument to switch to the highpri buffer, clears the highpri requested
    # signal, and set the highpri running signal.
    li a2, SP_WSTATUS_CLEAR_SIG_HIGHPRI_REQUESTED | SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING
    li a0, RSPQ_HIGHPRI_CALL_SLOT<<2
    li a1, RSPQ_LOWPRI_CALL_SLOT<<2
    sub rspq_dmem_buf_ptr, t0
    #fallthrough
    .endfunc

    #############################################################
    # RSPQCmd_SwapBuffers
    #
    # Switch between lowpri and highpri or viceversa. This is
    # called by RSP itself to go into highpri mode, and scheduled
    # as normal command by CPU when going back into lowpri.
    #
    # ARGS:
    #   a0: Pointer stack slot that contains the address to switch to.
    #       (either RSPQ_LOWPRI_CALL_SLOT<<2 or RSPQ_HIGHPRI_CALL_SLOT<<2)
    #   a1: Pointer stack slot where to save the current address to.
    #       (either RSPQ_LOWPRI_CALL_SLOT<<2 or RSPQ_HIGHPRI_CALL_SLOT<<2)
    #   a2: New mask to check for HIGHPRI signal (0 in highpri mode).
    #############################################################
    .func RSPQCmd_SwapBuffers
RSPQCmd_SwapBuffers:
    mtc0 a2, COP0_SP_STATUS
    lw a0, %lo(RSPQ_POINTER_STACK)(a0)
    #fallthrough
    .endfunc    
    
    #############################################################
    # RSPQCmd_Call
    #
    # Do a nested call to a different command list. Save the
    # current RDRAM position to be able to resume execution
    # later.
    #
    # ARGS:
    #   a0: New RDRAM address (plus command opcode)
    #   a1: DMEM address of the save slot for the current address
    #############################################################
    .func RSPQCmd_Call
RSPQCmd_Call:
    # a0: command opcode + RDRAM address
    # a1: call slot in DMEM
    lw s0, %lo(RSPQ_RDRAM_PTR)
    add s0, rspq_dmem_buf_ptr
    sw s0, %lo(RSPQ_POINTER_STACK)(a1)  # save return address
    # fallthrough
    .endfunc

    #############################################################
    # RSPQCmd_Jump
    #
    # Jump to a different command list. The current RDRAM position
    # is lost.
    #
    # ARGS:
    #   a0: New RDRAM address (plus command opcode)
    #############################################################
    .func RSPQCmd_Jump
RSPQCmd_Jump:
    # a0: command opcode + RDRAM address
    j rspq_fetch_buffer_with_ptr
    move s0, a0
    .endfunc

    #############################################################
    # RSPQCmd_Ret
    #
    # Restore a previously saved RDRAM position and jump to it.
    #
    # ARGS:
    #   a0: DMEM address of the save slot (plus command opcode)
    #############################################################
    .func RSPQCmd_Ret
RSPQCmd_Ret:
    # a0: command opcode + call slot in DMEM to recover
    j rspq_fetch_buffer_with_ptr
    lw s0, %lo(RSPQ_POINTER_STACK)(a0)
    .endfunc

    #############################################################
    # RSPQCmd_TestWriteStatus
    #
    # Like RSPQCmd_WriteStatus, writes COP0 SP status register with a specified value,
    # but first waits until the SP status AND'ed with the specified bitmask is zero.
    #
    # ARGS:
    #   a0: value to write into COP0_SP_STATUS
    #   a1: bitmask to test COP0_SP_STATUS for
    #############################################################
    .func RSPQCmd_TestWriteStatus
RSPQCmd_TestWriteStatus:
    jal SpStatusWait
    move t2, a1
    # fallthrough
    .endfunc

    #############################################################
    # RSPQCmd_WriteStatus
    #
    # Write COP0 SP status register with a specified value
    #
    # ARGS:
    #   a0: value to write into COP0_SP_STATUS
    #############################################################
    .func RSPQCmd_WriteStatus
RSPQCmd_WriteStatus:
    # Write the status value and go back to the main loop.
    # Notice that we write the whole a0 register, whose top 8-bits
    # contain the RSPQ_CMD_WRITE_STATUS command ID. This is not
    # a problem because the SP_STATUS register uses only 25 bits,
    # and we have assured that RSPQ_CMD_WRITE_STATUS has an ID
    # which is an even number (so that bit 24 is always 0 and not used).
    # This means that this command cannot ever write bit 24 to SP_STATUS:
    # that is SP_WSTATUS_SET_SIG_MORE, that we don't need to set
    # through this internal command.
    j RSPQ_Loop
    mtc0 a0, COP0_SP_STATUS
    .endfunc

    #############################################################
    # RSQPCmd_Dma
    #
    # Executes an arbitrary DMA request by taking the arguments to DMAExec from a0-a3
    #
    # ARGS:
    #   a0: RDRAM address
    #   a1: DMEM address
    #   a2: length/height
    #   a3: flags (in/out, sync/async)
    #############################################################
    .func RSQPCmd_Dma
RSQPCmd_Dma:
    move s0, a0
    move s4, a1
    move t0, a2
    j DMAExec
    move t2, a3
    .endfunc

#include <rsp_dma.inc>
#include <rsp_assert.inc>

    .align 3
# Overlay code will be loaded at this address
_ovl_text_start:
