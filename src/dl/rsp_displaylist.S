#include <rsp.inc>

.set noreorder
.set at

#define INPUT_BUFFER_SIZE 0x100

#define OVERLAY_SIZE  0x10
#define OVERLAY_COUNT 16

.data

.align 2
# Input properties
DL_DRAM_ADDR:     .long 0
DL_DRAM_SIZE:     .long 0
DL_POINTERS_ADDR: .long 0
# TODO: compress the overlay table more
OVERLAY_TABLE:   .ds.b (OVERLAY_SIZE * OVERLAY_COUNT)

.align 1
COMMAND_JUMP_TABLE:

.macro jumpTableEntry function
    .short \function - _start
.endm

jumpTableEntry command_noop
jumpTableEntry command_interrupt

.bss

.align 3
INPUT_BUFFER:    .ds.b INPUT_BUFFER_SIZE

.align 3
READ_POINTER:    .quad 0
WRITE_POINTER:   .quad 0
WRAP_POINTER:    .quad 0

.align 4
# Overlay data will be loaded at this address
OVERLAY_DATA_LOAD_ADDR:

.text

# Globally reserved registers
# TODO: Move some of these to DMEM
#define input_buf_start fp
#define input_buf_ptr   gp
#define input_buf_end   k1
#define read_ptr        k0
#define write_ptr       t9
#define wrap_ptr        t8
#define cur_ovl         t7

.globl _start
_start:
    j loop
    nop

# Triggers an RSP interrupt
command_interrupt:
    li t1, SP_WSTATUS_SET_RSP_INTERRUPT
    mtc0 t1, COP0_SP_STATUS

# Does nothing
command_noop:
    li t0, 1

# INPUT:
#   t0: length of input that has been read since the last loop.
.func loop
loop:
    # Advance input pointer
    addu input_buf_ptr, t0
    # Make sure there is at least 1 byte left in the buffer
    jal request_input
    li t0, 1

    # Read next byte
    lb t0, %lo(INPUT_BUFFER)(input_buf_ptr)
    # Overlay index (times overlay size, so offset into overlay table)
    andi a0, t0, 0xF0

    # Overlay #0 is reserved for internal commands
    beqz a0, is_default_overlay
    # Overlay command index
    andi a1, t0, 0x0F

    # Check if the requested overlay is already in memory
    beq a0, cur_ovl, overlay_loaded

    # Load overlay data
    lw t0, %lo(OVERLAY_TABLE) + 0x4 (a0)
    beqz t0, no_overlay_data
    lw s0, %lo(OVERLAY_TABLE) + 0x0 (a0)
    jal DMAInAsync
    li s4, %lo(OVERLAY_DATA_LOAD_ADDR)

no_overlay_data:
    # Load overlay code
    lw s0, %lo(OVERLAY_TABLE) + 0x8 (a0)
    lw t0, %lo(OVERLAY_TABLE) + 0xC (a0)
    jal DMAIn
    li s4, %lo(OVERLAY_CODE_LOAD_ADDR)

overlay_loaded:
    # Jump into overlay
    j OVERLAY_CODE_LOAD_ADDR
    # Remember loaded overlay
    move cur_ovl, a0

is_default_overlay:
    # Load command address from jump table and jump there
    sll a1, 1
    lh t0, %lo(COMMAND_JUMP_TABLE)(a1)
    jr t0
    nop
.endfunc

# Same as load_new_input, but only if there is less input left than requested
# INPUT:
#   t0: requested length
.func request_input
request_input:
    sub t1, input_buf_end, input_buf_ptr
    blt t1, t0, load_new_input
    nop
    jr ra
    nop
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
    li t0, 8

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
    bgtz len, buffer_not_empty

    # If buffer is empty -> enter idle mode
    li t0, SP_WSTATUS_SET_HALT
    mtc0 t0, COP0_SP_STATUS
    # "Delay slots" for halting the RSP
    # Otherwise weird things happen (at least on emulator)
    nop
    nop

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
.endfunc

#include <rsp_dma.inc>

.align 3
# Overlay code will be loaded at this address
OVERLAY_CODE_LOAD_ADDR:
# TODO: export overlay offset somehow?
