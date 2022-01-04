#include "../src/rspq/rsp_queue.S"

#define ASSERT_GP_BACKWARD           0xF001   // Also defined in test_rsqp.c

    .section .data.overlay

    RSPQ_OverlayHeader OVL_TEST_SAVED_DATA_START, OVL_TEST_SAVED_DATA_END, 0xF0

    .align 1
COMMAND_TABLE:
    RSPQ_DefineCommand command_test, 4           # 0xF0
    RSPQ_DefineCommand command_test, 8           # 0xF1
    RSPQ_DefineCommand command_test, 16          # 0xF2
    RSPQ_DefineCommand command_wait, 8           # 0xF3
    RSPQ_DefineCommand command_output, 8         # 0xF4
    RSPQ_DefineCommand command_reset, 4          # 0xF5
    RSPQ_DefineCommand command_test_high, 4      # 0xF6
    RSPQ_DefineCommand command_reset_log, 4      # 0xF7

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
    # Compare the last entry in the big log with the current command (RDRAM+GP).
    # If RDRAM pointer is the same, but GP is less than before, it means that
    # GP has moved backward in the same buffer, and this is surely an error.
    # It can be caused by many different bug, so we do a RSP assert that can
    # be useful while debugging.
    lw s0, %lo(BIG_LOG_PTR)
    lw t1, %lo(RSPQ_RDRAM_PTR)
    lw t2, %lo(BIG_LOG) -16(s0)
    bne t1, t2, 1f
    lw t2, %lo(BIG_LOG) -12(s0)
    bgt rspq_dmem_buf_ptr, t2, 1f
    nop
    assert ASSERT_GP_BACKWARD
1:

    # Save the current command in the big log. This is useful as a trace
    # during debugging. The big log contains all command_test_high commands
    # that have been executed.
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
    # Save the command_reset into the big log.
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
    # Reset the big log pointer to the start
    jr ra
    sw zero, %lo(BIG_LOG_PTR)
