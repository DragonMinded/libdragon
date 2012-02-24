/*
 *   N64 init code for GNU as
 */

#include "regs.S"

	.section .boot
	.global _start
_start:
	lw t0, 0x80000318			/* memory size */
	li t1, 0x7FFFFFF0
	addu sp,t0,t1				/* init stack */
	la gp, _gp					/* init data pointer */
	li v0, 8
	sw v0,(0xbfc007fc)			/* magic N64 hardware init */

	/* a bit from libgloss so we start at a known state */
	li v0,SR_CU1|SR_PE|SR_FR|SR_KX|SR_SX|SR_UX
	mtc0 v0,C0_SR
	mtc0 $0,C0_CAUSE

	/* copy code and data */
	la a0, __text_start
	la a1, __data_end
	subu a2,a0,0x80000400		/* skip over .boot section */
	addu a2,a2,0xB0001000		/* address in rom */
data_init:
	lw t0,(a2)
	addiu a2,4
	sw t0,(a0)
	addiu a0,4
	bltu a0,a1, data_init
	nop

	/* make sure code and data are actually written */
	la a0,__text_start
	la a1,__data_end
	sub a1,a0
	jal data_cache_hit_writeback_invalidate
	nop

	/* fill .bss with 0s */
	la a0, __bss_start
	la a1, __bss_end
bss_init:
	sd $0,(a0)
	addiu a0,8
	bltu a0,a1, bss_init
	nop

	/* make sure .bss is actually written */
	la a0,__bss_start
	la a1,__bss_end
	sub a1,a0
	jal data_cache_hit_writeback_invalidate
	nop

	/* load interrupt vector */
	la t0,intvector
	la t1,0xa0000000
	la t2,4
loadintvectorloop:
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
	bnez t2,loadintvectorloop
	nop

	jal main					/* call main app */
	nop

deadloop:
	j deadloop
	nop

intvector:
	la k1,inthandler
	jr k1
	nop

	.section .code
