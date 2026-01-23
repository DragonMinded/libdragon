.section .text.swapcontext
.p2align 5
.globl swapcontext
.func swapcontext
.set noat
.set noreorder

#define OFFSET_STACK 4
#define OFFSET_REGS 16
#define OFFSET_FPR OFFSET_REGS+40

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

    # FPU
    cfc1 $1, $f31
    sdc1 $f20,(OFFSET_FPR +  0*8)($a0)
    sdc1 $f21,(OFFSET_FPR +  1*8)($a0)
    sdc1 $f22,(OFFSET_FPR +  2*8)($a0)
    sdc1 $f23,(OFFSET_FPR +  3*8)($a0)
    sdc1 $f24,(OFFSET_FPR +  4*8)($a0)
    sdc1 $f25,(OFFSET_FPR +  5*8)($a0)
    sdc1 $f26,(OFFSET_FPR +  6*8)($a0)
    sdc1 $f27,(OFFSET_FPR +  7*8)($a0)
    sdc1 $f28,(OFFSET_FPR +  8*8)($a0)
    sdc1 $f29,(OFFSET_FPR +  9*8)($a0)
    sdc1 $f30,(OFFSET_FPR + 10*8)($a0)
    sdc1 $f31,(OFFSET_FPR + 11*8)($a0)
    sw $1, (OFFSET_FPR + 12*8)($a0)

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

    # FPU
    ldc1 $f20,(OFFSET_FPR +  0*8)($a1)
    ldc1 $f21,(OFFSET_FPR +  1*8)($a1)
    ldc1 $f22,(OFFSET_FPR +  2*8)($a1)
    ldc1 $f23,(OFFSET_FPR +  3*8)($a1)
    ldc1 $f24,(OFFSET_FPR +  4*8)($a1)
    ldc1 $f25,(OFFSET_FPR +  5*8)($a1)
    ldc1 $f26,(OFFSET_FPR +  6*8)($a1)
    ldc1 $f27,(OFFSET_FPR +  7*8)($a1)
    ldc1 $f28,(OFFSET_FPR +  8*8)($a1)
    ldc1 $f29,(OFFSET_FPR +  9*8)($a1)
    ldc1 $f30,(OFFSET_FPR + 10*8)($a1)
    ldc1 $f31,(OFFSET_FPR + 11*8)($a1)

    lw $at, (OFFSET_FPR + 12*8)($a1)
	  ctc1 $at, $f31

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