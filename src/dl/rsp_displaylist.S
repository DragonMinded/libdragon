#include <rsp.inc>
#include <rsp_dl.inc>

.set noreorder
.set at

#define INPUT_BUFFER_SIZE 0x100

#define OVERLAY_TABLE_SIZE 0x10

#define OVERLAY_DESC_SIZE  0x10
#define MAX_OVERLAY_COUNT 8

#define OVERLAY_HEADER_SIZE 0x6

.data

.align 2
# Input properties
DL_DRAM_ADDR:     .long 0
DL_POINTERS_ADDR: .long 0

OVERLAY_TABLE:         .ds.b OVERLAY_TABLE_SIZE
OVERLAY_DESCRIPTORS:   .ds.b (OVERLAY_DESC_SIZE * MAX_OVERLAY_COUNT)

.align 3
READ_POINTER:    .quad 0
WRITE_POINTER:   .quad 0
WRAP_POINTER:    .quad 0

.align 1
INTERNAL_COMMAND_TABLE:
commandTableEntry command_noop,      1
commandTableEntry command_interrupt, 1

.bss

.align 3
INPUT_BUFFER:    .ds.b INPUT_BUFFER_SIZE

.align 4
# Overlay data will be loaded at this address
OVERLAY_DATA_LOAD_ADDR:
OVERLAY_HEADER:

.text

.globl _start
_start:
    li t1, SP_WSTATUS_RESET_INTR_ON_BREAK
    mtc0 t1, COP0_SP_STATUS

    move input_buf_start, zero
    move input_buf_ptr, zero
    move input_buf_end, zero
    move read_ptr, zero
    move write_ptr, zero
    move wrap_ptr, zero
    j loop
    addi cur_ovl, zero, -1

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
    #define cmd_mask  s1
    #define cmd_size  s2

    # Make sure there are at least 8 bytes left in the buffer
    jal request_input
    li t0, 8

    # Read first two words
    lw a0, %lo(INPUT_BUFFER) + 0x0 (input_buf_ptr)
    lw a1, %lo(INPUT_BUFFER) + 0x4 (input_buf_ptr)

    # Index into overlay table
    srl t0, a0, 28

    # Overlay 0 is reserved for internal commands
    beqz t0, is_default_overlay
    srl cmd_index, a0, 23

    # Load overlay index from overlay table
    lb ovl_index, %lo(OVERLAY_TABLE)(t0)
    # Check if the requested overlay is already in memory
    beq ovl_index, cur_ovl, overlay_loaded
    lh t0, %lo(OVERLAY_HEADER) + 0x2

    # Skip saving overlay data if none is loaded (cur_ovl < 0)
    bltz cur_ovl, load_overlay
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x8 (ovl_index)

    # Save current overlay data
    # TODO: skip if size is 0
    jal DMAOut
    lh s4, %lo(OVERLAY_HEADER) + 0x0

load_overlay:
    # Load overlay data
    lh t0, %lo(OVERLAY_DESCRIPTORS) + 0xC (ovl_index)
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x0 (ovl_index)
    jal DMAIn
    li s4, %lo(OVERLAY_DATA_LOAD_ADDR)

    # Load saved overlay data 
    # TODO: skip if size is 0
    lh t0, %lo(OVERLAY_HEADER) + 0x2
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x8 (ovl_index)
    jal DMAInAsync
    lh s4, %lo(OVERLAY_HEADER) + 0x0

    # Load overlay code
    lh t0, %lo(OVERLAY_DESCRIPTORS) + 0xE (ovl_index)
    lw s0, %lo(OVERLAY_DESCRIPTORS) + 0x4 (ovl_index)
    jal DMAIn
    li s4, %lo(OVERLAY_CODE_LOAD_ADDR) + 0x1000

    # Remember loaded overlay
    move cur_ovl, ovl_index

overlay_loaded:
    # Load the command mask
    lh cmd_mask, %lo(OVERLAY_HEADER) + 0x4

    # Mask the shifted command index with the command mask to determine the final offset into the command table.
    # This is done so overlays can take up multiple entries in the overlay table but still use
    # the a single command table which can be bigger as well (only used for the RDP overlay for now, which uses
    # overlays 2 and 3)
    and cmd_index, cmd_mask
    j execute_command
    # Load command descriptor from overlay command table
    lh cmd_desc, %lo(OVERLAY_HEADER) + OVERLAY_HEADER_SIZE(cmd_index)

is_default_overlay:
    andi cmd_index, 0x1E
    # Load command descriptor from internal command table if using the default overlay
    lh cmd_desc, %lo(INTERNAL_COMMAND_TABLE)(cmd_index)

execute_command:
    # Command size
    srl cmd_size, cmd_desc, 12
    
    # Load more input if necessary, according to command size
    addi t1, cmd_size, -9
    bgezal t1, request_input
    move t0, cmd_size

    # Jump location (mask is technically not necessary, but do it anyway for easier debugging)
    andi cmd_desc, 0xFFF

    # Jump to command
    jr cmd_desc
    # Advance input pointer
    addu input_buf_ptr, cmd_size

    #undef ovl_index
    #undef cmd_index
    #undef cmd_desc
    #undef cmd_mask
    #undef cmd_size
.endfunc

# Same as load_new_input, but only if there is less input left than requested
# INPUT:
#   t0: requested length
.func request_input
request_input:
    sub t1, input_buf_end, input_buf_ptr
    bge t1, t0, JrRa
.endfunc

# This function will do two things:
#   1) Mark the input data that has been processed since the last call to it as read.
#   2) Check if new data is available and load it into the internal buffer.
.func load_new_input
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

    # 2) write_ptr < read_ptr: We can read up to the wrap pointer
    bne wrap_ptr, read_ptr, length_calculated
    sub len, t0, read_ptr

    # Special case: if the read pointer is exactly at the wrap pointer,
    # move the read pointer back to 0 and check again
    j calc_valid_length
    move read_ptr, zero

length_calculated:
    # Check if the buffer is empty (length == 0)
    bgtz len, buffer_not_empty
    nop

    # If buffer is empty -> enter idle mode
    # After being woken up, perform the check again
    j check_for_new_input
    break

buffer_not_empty:
    # length = max(length, INPUT_BUFFER_SIZE)
    slti t1, len, INPUT_BUFFER_SIZE
    bnez t1, dma_input
    lw s0, %lo(DL_DRAM_ADDR)
    li len, INPUT_BUFFER_SIZE

dma_input:
    addi t0, len, -1
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
.endfunc

#include <rsp_dma.inc>

.align 3
# Overlay code will be loaded at this address
OVERLAY_CODE_LOAD_ADDR:
