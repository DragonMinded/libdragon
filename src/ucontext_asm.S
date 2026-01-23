.section .text.swapcontext
.p2align 5
.globl swapcontext
.func swapcontext
.set at
.set noreorder

#define OFFSET_STACK 4
#define OFFSET_REGS 16

swapcontext:
    # Save callee-saved registers
    sw  $s0,  0 + OFFSET_REGS($a0)
    sw  $s1,  4 + OFFSET_REGS($a0)
    sw  $s2,  8 + OFFSET_REGS($a0)
    sw  $s3, 12 + OFFSET_REGS($a0)
    sw  $s4, 16 + OFFSET_REGS($a0)
    sw  $s5, 20 + OFFSET_REGS($a0)
    sw  $s6, 24 + OFFSET_REGS($a0)
    sw  $s7, 28 + OFFSET_REGS($a0)
    sw  $ra, 32 + OFFSET_REGS($a0)
    sw  $sp, OFFSET_STACK($a0)

    # Restore new context
    lw  $s0,  0 + OFFSET_REGS($a1)
    lw  $s1,  4 + OFFSET_REGS($a1)
    lw  $s2,  8 + OFFSET_REGS($a1)
    lw  $s3, 12 + OFFSET_REGS($a1)
    lw  $s4, 16 + OFFSET_REGS($a1)
    lw  $s5, 20 + OFFSET_REGS($a1)
    lw  $s6, 24 + OFFSET_REGS($a1)
    lw  $s7, 28 + OFFSET_REGS($a1)
    lw  $ra, 32 + OFFSET_REGS($a1)
    lw  $sp, OFFSET_STACK($a1)

    jr  $ra
    lw  $a0, 36 + OFFSET_REGS($a1)
    nop
.endfunc


.section .text.getcontext
.p2align 5
.globl getcontext
.func getcontext
.set at
.set noreorder

getcontext:
    /* Save callee-saved registers */
    sw  $s0,  0 + OFFSET_REGS($a0)
    sw  $s1,  4 + OFFSET_REGS($a0)
    sw  $s2,  8 + OFFSET_REGS($a0)
    sw  $s3, 12 + OFFSET_REGS($a0)
    sw  $s4, 16 + OFFSET_REGS($a0)
    sw  $s5, 20 + OFFSET_REGS($a0)
    sw  $s6, 24 + OFFSET_REGS($a0)
    sw  $s7, 28 + OFFSET_REGS($a0)

    /* Save return address so swapcontext can resume here */
    sw  $ra, 32 + OFFSET_REGS($a0)
    sw  $sp, OFFSET_STACK($a0)

    /* Return 0 */
    jr $ra
    or $v0, $zero, $zero

.endfunc