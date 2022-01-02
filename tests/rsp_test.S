#include "../src/rspq/rsp_queue.S"

    .section .data.overlay

    overlayHeader OVL_TEST_SAVED_DATA_START, OVL_TEST_SAVED_DATA_END, 0xF0

    .align 1
COMMAND_TABLE:
    commandTableEntry command_test, 4           # 0xF0
    commandTableEntry command_test, 8           # 0xF1
    commandTableEntry command_test, 16          # 0xF2
    commandTableEntry command_wait, 8           # 0xF3
    commandTableEntry command_output, 8         # 0xF4
    commandTableEntry command_reset, 4          # 0xF5
    commandTableEntry command_test_high, 4      # 0xF6
    commandTableEntry command_reset_log, 4      # 0xF7

    .align 3
OVL_TEST_SAVED_DATA_START:
TEST_DATA:
TEST_PADDING:  .long 0
TEST_VARIABLE: .long 0
TEST_PADDING2:  .long 0
TEST_VARIABLE2: .long 0
OVL_TEST_SAVED_DATA_END:

BIG_LOG_PTR: .long 0

    .align 10
BIG_LOG:  .ds.b 2048

    .text 1

command_test:
    lw t0, %lo(TEST_VARIABLE)
    and a0, 0xFFFFFF
    add t0, a0
    jr ra
    sw t0, %lo(TEST_VARIABLE)

command_test_high:
    lw s0, %lo(BIG_LOG_PTR)
    lw t1, %lo(RSPQ_RDRAM_PTR)
    lw t2, %lo(BIG_LOG) -16(s0)
    bne t1, t2, 1f
    lw t2, %lo(BIG_LOG) -12(s0)
    bgt rspq_dmem_buf_ptr, t2, 1f
    nop
    assert ASSERT_GP_BACKWARD

1:
    and a0, 0xFFFFFF
    sw t1, %lo(BIG_LOG) + 0(s0)
    sw rspq_dmem_buf_ptr, %lo(BIG_LOG) + 4(s0)
    sw a0, %lo(BIG_LOG) + 8(s0)
    lw t0, %lo(TEST_VARIABLE2)
    sw t0, %lo(BIG_LOG) + 12(s0)
    addi s0, 16
    sw s0, %lo(BIG_LOG_PTR)

    lw t0, %lo(TEST_VARIABLE2)
    add t0, a0
    jr ra
    sw t0, %lo(TEST_VARIABLE2)

command_wait:
    bgtz a1, command_wait
    addi a1, -1
    jr ra
    nop

command_output:
    move s0, a1
    li s4, %lo(TEST_DATA)
    j DMAOut
    li t0, DMA_SIZE(16, 1)

command_reset:
    lw s0, %lo(BIG_LOG_PTR)
    lw t1, %lo(RSPQ_RDRAM_PTR)
    sw t1, %lo(BIG_LOG) + 0(s0)
    sw gp, %lo(BIG_LOG) + 4(s0)
    sw zero, %lo(BIG_LOG) + 8(s0)
    sw zero, %lo(BIG_LOG) + 12(s0)
    addi s0, 16
    sw s0, %lo(BIG_LOG_PTR)

    sw zero, %lo(TEST_VARIABLE)
    jr ra
    sw zero, %lo(TEST_VARIABLE2)

command_reset_log:
    jr ra
    sw zero, %lo(BIG_LOG_PTR)
