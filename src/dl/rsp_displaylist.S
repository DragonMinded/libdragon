#include <rsp.inc>

#include "displaylist_shared.h"

.set noreorder
.set at

#define INPUT_BUFFER_SIZE 0x100

.data

DL_DRAM_ADDR:     .long 0
DL_DRAM_SIZE:     .long 0
DL_POINTERS_ADDR: .long 0

.align 3
READ_POINTER:  .double 0
WRITE_POINTER: .double 0
WRAP_POINTER:  .double 0

.bss

.align 3
INPUT_BUFFER:    .dcb.b INPUT_BUFFER_SIZE

.text

init:

# TODO: Move some of these to DMEM
#define input_buf_start fp
#define input_buf_ptr   gp
#define input_buf_end   k1
#define read_ptr        k0
#define write_ptr       t9
#define wrap_ptr        t8

    j loop
    nop

# INPUT:
#   t0: length of input that has been read since the last loop.
loop_advance_and_continue:
    addu input_buf_ptr, t0

loop:
    jal ensure_input
    nop

process_next_command:
    lb a0, %lo(INPUT_BUFFER)(input_buf_ptr)

    # TODO: process next command

    j loop_advance_and_continue
    li t0, 1

# Same as load_new_input, but only if the internal input buffer is empty
ensure_input:
    blt input_buf_ptr, input_buf_end, JrRa

# This function will do two things:
#   1) Mark the input data that has been processed since the last call to it as read.
#   2) Check if new data is available and load it into the internal buffer.
load_new_input:
    #define len t4

    move ra2, ra

    # Don't need to update read_ptr if no input has been read
    sub len, input_buf_ptr, input_buf_start
    beqz len, check_for_new_input

    # If the following condition is true
    #   write_ptr < read_ptr && len == wrap_ptr - read_ptr
    # then the write pointer has wrapped around to the start and the length
    # of the read input would take the read pointer exactly to the wrap pointer.
    # In that case, we reset read_ptr back to zero.
    # Otherwise, just advance read_ptr by len.
    slt t0, write_ptr, read_ptr
    sub t1, wrap_ptr, read_ptr
    seq t1, len
    and t0, t1
    beqz t0, store_read_ptr
    add read_ptr, len
    move read_ptr, zero

store_read_ptr:
    # Communicate the updated read pointer to the CPU
    sw read_ptr, %lo(READ_POINTER) + 0x4
    li s4, %lo(READ_POINTER)
    lw s0, %lo(DL_POINTERS_ADDR)
    jal DMAOutAsync
    li t0, 8

check_for_new_input:
    # Check if write pointer has been updated by the CPU
    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG0
    beqz t0, calc_valid_length

    # Load new values of write_ptr and wrap_ptr
    li s4, %lo(WRITE_POINTER)
    lw s0, %lo(DL_POINTERS_ADDR)
    li t0, 16 # Load write and wrap pointer
    jal DMAIn
    addiu s0, 8
    lw write_ptr, %lo(WRITE_POINTER) + 0x4
    lw wrap_ptr, %lo(WRAP_POINTER) + 0x4

calc_valid_length:
    # Calculate the length of contiguous data that can be read.
    
    # Check for one of two cases:
    bge write_ptr, read_ptr, length_calculated

    # 1) write_ptr >= read_ptr: we can read up to the write pointer
    sub len, write_ptr, read_ptr

    # 2) write_ptr < read_ptr: We can read up to the wrap pointer
    bne wrap_ptr, read_ptr, length_calculated
    sub len, t0, read_ptr

    # Special case: if the read pointer is exactly at the wrap pointer,
    # move the read pointer back to 0 and check again
    j calc_valid_length
    move read_ptr, zero

length_calculated:
    # Check if the buffer is empty (length == 0)
    bnez len, buffer_not_empty

    # If buffer is empty -> enter idle mode
    # TODO: does this work?
    li t0, SP_WSTATUS_SET_HALT
    mtc0 t0, COP0_SP_STATUS

    # After being woken up, perform the check again
    j check_for_new_input
    nop

buffer_not_empty:

    # length = max(length, INPUT_BUFFER_SIZE)
    slti t1, len, INPUT_BUFFER_SIZE
    bnez t1, dma_input
    lw s0, %lo(DL_DRAM_ADDR)
    li len, INPUT_BUFFER_SIZE

dma_input:
    move t0, len
    li s4, %lo(INPUT_BUFFER)
    jal DMAIn
    add s0, read_ptr

    # Reset input pointer, taking DMA non-alignment into account
    li t1, %lo(INPUT_BUFFER)
    sub input_buf_ptr, s4, t1
    # Remember the actual start of new data in the buffer,
    # because due to possible non-alignment it might not be at index 0
    move input_buf_start, input_buf_ptr
    jr ra2
    add input_buf_end, input_buf_ptr, len

    #undef len

#include <rsp_dma.inc>

rsp_displaylist_overlay:
# TODO: export overlay offset somehow?
