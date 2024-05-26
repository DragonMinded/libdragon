#include <rsp_queue.inc>
#include "test_rspq_constants.h"

#define ASSERT_GP_BACKWARD           0xF001   // Also defined in test_rspq.c
#define ASSERT_TOO_MANY_NOPS         0xF002

    .set noreorder
    .set at

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand command_test,      4      # 0x00
        RSPQ_DefineCommand command_test,      8      # 0x01
        RSPQ_DefineCommand command_test,      16     # 0x02
        RSPQ_DefineCommand command_wait,      8      # 0x03
        RSPQ_DefineCommand command_output,    8      # 0x04
        RSPQ_DefineCommand command_reset,     4      # 0x05
        RSPQ_DefineCommand command_test_high, 4      # 0x06
        RSPQ_DefineCommand command_reset_log, 4      # 0x07
        RSPQ_DefineCommand command_big,       TEST_RSPQ_BIG_COMMAND_SIZE*4    # 0x08
        RSPQ_DefineCommand command_big_out,   8      # 0x09
        RSPQ_DefineCommand command_send_rdp,  8      # 0x0A
        RSPQ_DefineCommand command_send_rdp_many,  4 # 0x0B
	RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
TEST_DATA:
TEST_PADDING:  .long 0
TEST_VARIABLE: .long 0
TEST_PADDING2:  .long 0
TEST_VARIABLE2: .long 0
    RSPQ_EndSavedState

TEST_RDP_STAGING: .quad 0

BIG_LOG_PTR: .long 0
#define BIG_LOG_SIZE 2048

    .align 10
BIG_LOG:  .ds.b BIG_LOG_SIZE

    .align 2
TEST_BIG: .ds.b TEST_RSPQ_BIG_PAYLOAD_SIZE

    .text

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

command_send_rdp:
    li s4, %lo(TEST_RDP_STAGING)
    li s3, %lo(TEST_RDP_STAGING) + 8
    sw zero, 0(s4)
    j RDPQ_Send
    sw a1, 4(s4)

command_send_rdp_many:
    and a0, 0x00FFFFFF
    assert_lt a0, BIG_LOG_SIZE/8, ASSERT_TOO_MANY_NOPS
    lui t1, 0xC000
    li s3, %lo(BIG_LOG)
1:
    sw t1,   0(s3)
    sw zero, 4(s3)
    addi s3, 8
    addiu a0, -1
    bnez a0, 1b
    nop
    j RDPQ_Send
    li s4, %lo(BIG_LOG)


command_big:
    emux_log_string "command_big\n"
    addi s1, rspq_dmem_buf_ptr, -TEST_RSPQ_BIG_PAYLOAD_SIZE
    move s2, zero
command_big_loop:
    lw t0, %lo(RSPQ_DMEM_BUFFER)(s1)
    lw t1, %lo(TEST_BIG)(s2)
    xor t0, t1
    sw t0, %lo(TEST_BIG)(s2)
    add s1, 0x4
    blt s1, rspq_dmem_buf_ptr, command_big_loop
    add s2, 0x4
    jr ra
    nop

command_big_out:
    move s0, a1
    li s4, %lo(TEST_BIG)
    j DMAOut
    li t0, DMA_SIZE(TEST_RSPQ_BIG_PAYLOAD_SIZE, 1)

#include <rsp_rdpq.inc>
