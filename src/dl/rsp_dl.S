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
    j loop
    move dl_dmem_buf_ptr, zero

command_signal:
    andi a0, 0xFFFC
    j write_sp_status
    sll t0, a0, 9

# Triggers an RSP interrupt
command_interrupt:
    li t0, SP_WSTATUS_SET_RSP_INTERRUPT

write_sp_status:
    mtc0 t0, COP0_SP_STATUS

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
    li t0, 4

    # Read first word
    lw a0, %lo(DL_DMEM_BUFFER) + 0x0 (dl_dmem_buf_ptr)

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
    andi cmd_index, 0x1FE

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
    slti t1, cmd_size, 5
    beqz t1, request_input
    li ra, %lo(execute_command2)

execute_command2:
    # Jump location (mask is technically not necessary, but do it anyway for easier debugging)
    # andi cmd_desc, 0xFFF

    # Load second command word (might be garbage, but will never be read in that case)
    # This is done because the vast majority of commands will use two words or more.
    lw a1, %lo(DL_DMEM_BUFFER) + 0x4 (dl_dmem_buf_ptr)

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

    move ra2, ra

    # Mark processed data as read
    # Note that we need to do this directly before loading new input, because we might not have processed
    # all data that was loaded last time yet (Because we are branching to a different display list for example).
    lw read_ptr, %lo(READ_POINTER) + 0x4
    lhu t0, %lo(DMEM_BUF_START)
    sub len, dl_dmem_buf_ptr, t0
    add read_ptr, len

store_read_ptr:
    # Communicate the updated read pointer to the CPU
    li s4, %lo(READ_POINTER)
    sw read_ptr, %lo(READ_POINTER) + 0x4
    lw s0, %lo(DL_POINTERS_ADDR)
    jal DMAOutAsync
    li t0, DMA_SIZE(8, 1)

check_for_new_input:
    # Calculate remaining "safe" area
    lw t0, %lo(WRITE_POINTER) + 0x4
    ble read_ptr, t0, check_remaining_length

    # 1) read pointer <= write pointer: len = write pointer - read pointer (read up to write pointer)
    sub len, t0, read_ptr

    # 2) read pointer > write pointer: len = DL_DRAM_BUFFER_SIZE - read pointer (read up to end of buffer)
    li t1, DL_DRAM_BUFFER_SIZE
    sub len, t1, read_ptr
    bgtz len, buffer_not_empty
    nop
    move read_ptr, zero
    sw read_ptr, %lo(READ_POINTER) + 0x4
    move len, t0

check_remaining_length:
    bgtz len, buffer_not_empty

wait_for_signal:
    nop
    nop
    nop
    nop
    nop
    nop
    # Wait until the CPU updates the write pointer
    mfc0 t0, COP0_SP_STATUS
    andi t0, SP_STATUS_SIG0
    bnez t0, write_pointer_updated
    li t1, SP_WSTATUS_RESET_SIG0

    # Enter idle mode
    j wait_for_signal
    break

write_pointer_updated:
    # Reset signal
    # TODO: race condition?
    mtc0 t1, COP0_SP_STATUS

    # Load new value of write pointer
    li s4, %lo(WRITE_POINTER)
    lw s0, %lo(DL_POINTERS_ADDR)
    li t0, DMA_SIZE(8, 1)
    addiu s0, 8
    jal_and_j DMAIn, check_for_new_input

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
    # because due to possible non-alignment it might not be at 0
    sh dl_dmem_buf_ptr, %lo(DMEM_BUF_START)

    # Remember the end of the new data
    add t0, dl_dmem_buf_ptr, len
    jr ra2
    sh t0, %lo(DMEM_BUF_END)

    #undef len
    #undef read_ptr
.endfunc

#include <rsp_dma.inc>

.align 3
# Overlay code will be loaded at this address
.globl _ovl_text_start
_ovl_text_start:
