#include "regs.S"

    .text
    .set noreorder
    .set noat
#define at $1

.macro skc_call id
    li v0, \id
    lui at, 0xA430
    jr ra
     lw zero, 0x14(at)
.endm

    .global skc_get_id
skc_get_id:
    skc_call 0

    .global skc_exit
skc_exit:
    skc_call 13

    .global skc_keep_alive
skc_keep_alive:
    skc_call 14
