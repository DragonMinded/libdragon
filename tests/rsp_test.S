#include "../src/dl/rsp_dl.S"

.section .data.overlay

overlayHeader OVL_TEST_SAVED_DATA_START, OVL_TEST_SAVED_DATA_END, 0xF0

.align 1
COMMAND_TABLE:
commandTableEntry command_test, 4
commandTableEntry command_test, 8
commandTableEntry command_test, 16
commandTableEntry command_wait, 8

.section .bss.overlay
.globl _ovl_bss_start
_ovl_bss_start:

.align 3
OVL_TEST_SAVED_DATA_START:
TEST_VARIABLE: .long 0
TEST_PADDING:  .long 0
OVL_TEST_SAVED_DATA_END:

.text 1

command_test:
    j loop
    sw a0, %lo(TEST_VARIABLE)

command_wait:
    bgtz a1, command_wait
    addi a1, -1
    j loop
    nop
