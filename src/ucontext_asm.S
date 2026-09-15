.section .text.swapcontext
.p2align 5
.globl swapcontext
.func swapcontext
.set noat
.set noreorder

#define OFFSET_STACK 4
#define OFFSET_REGS 16
#define OFFSET_FPR OFFSET_REGS+80

swapcontext:
    # Save callee-saved registers
    sd  $s0, OFFSET_REGS + 0*8($a0)
    sd  $s1, OFFSET_REGS + 1*8($a0)
    sd  $s2, OFFSET_REGS + 2*8($a0)
    sd  $s3, OFFSET_REGS + 3*8($a0)
    sd  $s4, OFFSET_REGS + 4*8($a0)
    sd  $s5, OFFSET_REGS + 5*8($a0)
    sd  $s6, OFFSET_REGS + 6*8($a0)
    sd  $s7, OFFSET_REGS + 7*8($a0)
    sd  $fp, OFFSET_REGS + 8*8($a0)
    sw  $ra, OFFSET_REGS + 9*8($a0)
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
    ld  $s0, OFFSET_REGS + 0*8($a1)
    ld  $s1, OFFSET_REGS + 1*8($a1)
    ld  $s2, OFFSET_REGS + 2*8($a1)
    ld  $s3, OFFSET_REGS + 3*8($a1)
    ld  $s4, OFFSET_REGS + 4*8($a1)
    ld  $s5, OFFSET_REGS + 5*8($a1)
    ld  $s6, OFFSET_REGS + 6*8($a1)
    ld  $s7, OFFSET_REGS + 7*8($a1)
    ld  $fp, OFFSET_REGS + 8*8($a1)
    lw  $ra, OFFSET_REGS + 9*8($a1)
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
    lw  $a0, OFFSET_REGS + 9*8 + 4($a1)
    nop
.endfunc
