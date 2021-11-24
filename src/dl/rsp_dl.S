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
WRAP_POINTER:    .quad 0

DL_DRAM_ADDR:     .long 0
DL_POINTERS_ADDR: .long 0

DMEM_BUF_START:  .half 0
DMEM_BUF_END:    .half 0
CURRENT_OVL:     .half -1

.align 3
INTERNAL_COMMAND_TABLE:
commandTableEntry command_noop,      4
commandTableEntry command_interrupt, 4
commandTableEntry command_signal,    4

.bss

.align 3
# Reserve some extra space of 8 bytes to make sure that DMAs can always load the expected amount of data even if the DRAM address is unaligned
DL_DMEM_BUFFER:    .ds.b DL_DMEM_BUFFER_SIZE + 8

.align 4
# Overlay data will be loaded at this address
.globl _ovl_data_start
_ovl_data_start:

.text

.globl _start
_start:
    move dl_dmem_buf_ptr, zero
    li t1, SP_WSTATUS_RESET_INTR_ON_BREAK
    j loop
    mtc0 t1, COP0_SP_STATUS

command_signal:
    andi a0, 0xFFFC
    sll t0, a0, 9
    j loop
    mtc0 t0, COP0_SP_STATUS

# Triggers an RSP interrupt
command_interrupt:
    li t1, SP_WSTATUS_SET_RSP_INTERRUPT
    mtc0 t1, COP0_SP_STATUS

# Does nothing
command_noop:

.func loop
loop:
    #define ovl_index t4
    #define cmd_index t5
    #define cmd_desc  t6
    #define cmd_size  s2

    # Make sure there are at least 8 bytes left in the buffer
    jal request_input
    li t0, 8

    # Read first two words
    lw a0, %lo(DL_DMEM_BUFFER) + 0x0 (dl_dmem_buf_ptr)
    lw a1, %lo(DL_DMEM_BUFFER) + 0x4 (dl_dmem_buf_ptr)

    # Index into overlay table
    srl t0, a0, 28

    # Overlay 0 is reserved for internal commands
    beqz t0, is_default_overlay
    srl cmd_index, a0, 23

    lh t1, %lo(CURRENT_OVL)

    # Load overlay index from overlay table
    lb ovl_index, %lo(OVERLAY_TABLE)(t0)
    # Check if the requested overlay is already in memory
    beq ovl_index, t1, overlay_loaded
    lhu t0, %lo(_ovl_data_start) + 0x4

    # Skip saving overlay data if none is loaded (CURRENT_OVL < 0)
    bltz t1, load_overlay
    lw s4, %lo(_ovl_data_start) + 0x0

    # Save current overlay data
    # TODO: skip if size is 0
    jal DMAOut
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x8 (t1)

load_overlay:
    # Load overlay data
    lhu t0, %lo(OVERLAY_DESCRIPTORS) + 0xE (ovl_index)
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x4 (ovl_index)
    jal DMAIn
    li s4, %lo(_ovl_data_start)

    # Load saved overlay data 
    # TODO: skip if size is 0
    lhu t0, %lo(_ovl_data_start) + 0x4
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x8 (ovl_index)
    jal DMAInAsync
    lw s4, %lo(_ovl_data_start) + 0x0

    # Load overlay code
    lhu t0, %lo(OVERLAY_DESCRIPTORS) + 0xC (ovl_index)
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x0 (ovl_index)
    jal DMAIn
    li s4, %lo(_ovl_text_start) + 0x1000

    # Remember loaded overlay
    sh ovl_index, %lo(CURRENT_OVL)

overlay_loaded:
    # Load the command base
    lhu t0, %lo(_ovl_data_start) + 0x6

    # Mask command index
    andi cmd_index, 0xFE

    # Subtract the command base to determine the final offset into the command table.
    sub cmd_index, t0
    j execute_command
    # Load command descriptor from overlay command table
    lhu cmd_desc, %lo(_ovl_data_start) + OVERLAY_HEADER_SIZE(cmd_index)

is_default_overlay:
    andi cmd_index, 0x1E
    # Load command descriptor from internal command table if using the default overlay
    lhu cmd_desc, %lo(INTERNAL_COMMAND_TABLE)(cmd_index)

execute_command:
    # Command size
    srl cmd_size, cmd_desc, 10
    andi cmd_size, 0x3C
    
    # Load more input if necessary, according to command size
    move t0, cmd_size
    slti t1, cmd_size, 9
    beqz t1, request_input
    li ra, %lo(execute_command2)

execute_command2:
    # Jump location (mask is technically not necessary, but do it anyway for easier debugging)
    andi cmd_desc, 0xFFF

    # Jump to command
    jr cmd_desc
    # Advance input pointer
    addu dl_dmem_buf_ptr, cmd_size

    #undef ovl_index
    #undef cmd_index
    #undef cmd_desc
    #undef cmd_size
.endfunc

# Same as load_new_input, but only if there is less input left than requested
# INPUT:
#   t0: requested length
.func request_input
request_input:
    lhu t2, %lo(DMEM_BUF_END)
    sub t1, t2, dl_dmem_buf_ptr
    bge t1, t0, JrRa
.endfunc

# This function will do two things:
#   1) Mark the input data that has been processed since the last call to it as read.
#   2) Check if new data is available and load it into the internal buffer.
.func load_new_input
load_new_input:
    #define len       t4
    #define read_ptr  s3
    #define write_ptr s5
    #define wrap_ptr  s6

    move ra2, ra

    lw read_ptr, %lo(READ_POINTER) + 0x4
    lw write_ptr, %lo(WRITE_POINTER) + 0x4


    # Don't need to update read_ptr if no input has been read
    lhu t0, %lo(DMEM_BUF_START)
    sub len, dl_dmem_buf_ptr, t0
    beqz len, check_for_new_input
    lw wrap_ptr, %lo(WRAP_POINTER) + 0x4

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
    li t0, DMA_SIZE(8, 1)

check_for_new_input:
    # Check if write pointer has been updated by the CPU
    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG0
    beqz t0, calc_valid_length

    # Reset signal
    # TODO: race condition?
    li t1, SP_WSTATUS_RESET_SIG0
    mtc0 t1, COP0_SP_STATUS

    # Load new values of write_ptr and wrap_ptr
    li s4, %lo(WRITE_POINTER)
    lw s0, %lo(DL_POINTERS_ADDR)
    li t0, DMA_SIZE(16, 1) # Load write and wrap pointer
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

    # 2) write_ptr < read_ptr:
    bne wrap_ptr, read_ptr, length_calculated
    # 2.a) wrap_ptr != read_ptr: We can read up to the wrap pointer
    sub len, wrap_ptr, read_ptr

    # 2.b) wrap_ptr == read_ptr: move the read pointer back to 0 and check again
    move read_ptr, zero
    j calc_valid_length
    sw read_ptr, %lo(READ_POINTER) + 0x4

length_calculated:
    # Check if the buffer is empty (length == 0)
    bgtz len, buffer_not_empty
    nop

    # If buffer is empty -> enter idle mode
    # After being woken up, perform the check again
    j check_for_new_input
    break

buffer_not_empty:
    # length = max(length, DL_DMEM_BUFFER_SIZE)
    slti t1, len, DL_DMEM_BUFFER_SIZE
    bnez t1, dma_input
    lw s0, %lo(DL_DRAM_ADDR)
    li len, DL_DMEM_BUFFER_SIZE

dma_input:
    # Always load additional 8 bytes to make sure that at least the expected amount of data is loaded, even if the DRAM address is not aligned.
    addi t0, len, 7
    li s4, %lo(DL_DMEM_BUFFER)
    jal DMAIn
    add s0, read_ptr

    # Reset input pointer, taking DMA non-alignment into account
    li t1, %lo(DL_DMEM_BUFFER)
    sub dl_dmem_buf_ptr, s4, t1
    # Remember the actual start of new data in the buffer,
    # because due to possible non-alignment it might not be at index 0
    sh dl_dmem_buf_ptr, %lo(DMEM_BUF_START)
    add t0, dl_dmem_buf_ptr, len
    jr ra2
    sh t0, %lo(DMEM_BUF_END)

    #undef len
    #undef read_ptr
    #undef write_ptr
    #undef wrap_ptr
.endfunc

#include <rsp_dma.inc>

.align 3
# Overlay code will be loaded at this address
.globl _ovl_text_start
_ovl_text_start:
