#include "regs.S"

    .text
    .set noreorder

.macro skc_call id
    li v0, \id              # Load the system call number in $v0
    lw zero, 0xA4300014     # Force a SK trap. This will cause a NMI to be issued.
    nop                     # NMI is a bit delayed to avoid retriggering on the load
    nop
    jr ra
     nop
.endm

    .global skc_get_id
skc_get_id:
    skc_call 0

    .global skc_launch_setup
skc_launch_setup:
    skc_call 1

    .global skc_launch
skc_launch:
    skc_call 2

    .global skc_exit
skc_exit:
    skc_call 13

    .global skc_keep_alive
skc_keep_alive:
    skc_call 14
