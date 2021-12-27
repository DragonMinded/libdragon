#include "../src/rspq/rsp_rspq.S"

    .section .data.overlay

    overlayHeader OVL_TEST_SAVED_DATA_START, OVL_TEST_SAVED_DATA_END, 0xF0

    .align 1
COMMAND_TABLE:
    commandTableEntry command_test, 4
    commandTableEntry command_test, 8
    commandTableEntry command_test, 16
    commandTableEntry command_wait, 8
    commandTableEntry command_output, 8
    commandTableEntry command_reset, 4
    commandTableEntry command_test_high, 4

    .align 3
OVL_TEST_SAVED_DATA_START:
TEST_DATA:
TEST_PADDING:  .long 0
TEST_VARIABLE: .long 0
TEST_PADDING2:  .long 0
TEST_VARIABLE2: .long 0
OVL_TEST_SAVED_DATA_END:

    .text 1

command_test:
    lw t0, %lo(TEST_VARIABLE)
    and a0, 0xFFFFFF
    add t0, a0
    jr ra
    sw t0, %lo(TEST_VARIABLE)

command_test_high:
    lw t0, %lo(TEST_VARIABLE2)
    and a0, 0xFFFFFF
    add t0, a0
    jr ra
    sw t0, %lo(TEST_VARIABLE2)

command_wait:
    bgtz a1, command_wait
    addi a1, -1
    j loop
    nop

command_output:
    move s0, a1
    li s4, %lo(TEST_DATA)
    j DMAOut
    li t0, DMA_SIZE(16, 1)

command_reset:
    sw zero, %lo(TEST_VARIABLE)
    j loop
    sw zero, %lo(TEST_VARIABLE2)
