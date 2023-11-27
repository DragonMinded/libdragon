/*
 *   N64 init code for GNU as
 */

#include "regs.S"

	.set noreorder

	.section .boot
	.global _start
_start:
	/* initialize small data pointer */
	la gp, _gp

	/* a bit from libgloss so we start at a known state */
	li v0,SR_CU1|SR_PE|SR_FR|SR_KX|SR_SX|SR_UX
	mtc0 v0,C0_SR
	mtc0 $0,C0_CAUSE

	/* Copy other bootcode flags, stored in DMEM */
	lui v0, 0xA400
	lw t0, 0x0000(v0)
	lw t1, 0x0004(v0)
	sw t0, %gprel(__boot_memsize)(gp)
	sw t1, %gprel(__boot_entropy)(gp)
	/* Setup SP using memory size */
	addu sp, t0, 0x80000000 - 0x10		/* sp = KSEG0 + memsize - 0x10 */

	lbu t0, 0x0009(v0)
	lbu t1, 0x000A(v0)
	lbu t2, 0x000B(v0)
	sw t0, %gprel(__boot_tvtype)(gp)
	sw t1, %gprel(__boot_resettype)(gp)
	sw t2, %gprel(__boot_consoletype)(gp)

	/* load interrupt vector */
	la t0,intvector
	la t1,0xa0000000
	la t2,4
.Lloadintvectorloop:
	lw t3,(t0)
	sw t3,0(t1)
	sw t3,0x80(t1)
	sw t3,0x100(t1)
	sw t3,0x180(t1)
	/* sync */
	cache HIT_INVALIDATE_I,0(t1)
	cache HIT_INVALIDATE_I,0x80(t1)
	cache HIT_INVALIDATE_I,0x100(t1)
	cache HIT_INVALIDATE_I,0x180(t1)
	addi t0,4
	addi t1,4
	addiu t2,-1
	bnez t2,.Lloadintvectorloop
	nop

	la t0, debug_assert_func    /* install assert function in system.c */
	la t1, __assert_func_ptr
	sw t0, 0(t1)	

	jal __do_global_ctors		/* call global constructors */
	nop
	li a0, 0
	jal main					/* call main app */
	li a1, 0

_abort:
	j _abort
	nop

intvector:
	la k1,inthandler
	jr k1
	nop

	.section .code
