/*
   N64 init code for GNU as
*/

#include "regs.S"
.section .init
	.globl _start
_start:
	la sp, 0x803ffff0	/* init stack (assumes no mem pack for now) */
	la gp, _gp		/* init data pointer */
        li v0, 8
        sw v0,(0xbfc007fc)	/* magic N64 hardware init */

    	/* a bit from libgloss so we start at a known state */
    	li v0,SR_CU1|SR_PE|SR_FR|SR_KX|SR_SX|SR_UX
	mtc0 v0,C0_SR
	mtc0 $0,C0_CAUSE

/* fill .bss with 0s */
    	la a0, __bss_start
	la a1, __bss_end-8
bss_init:
	sd $0,(a0)
    	addiu a0,8
	bltu a0,a1, bss_init
	nop

/* make sure the zeroes are actually written */
	la a0,__bss_start
	la a1,__bss_end
	sub a1,a0
	jal data_cache_writeback_invalidate
	nop
/*
/* commit data 
	la a0,__data_start
	la a1,__data_end
	sub a1,a0
	jal data_cache_writeback_invalidate
	nop

/* commit text 
	la a0,__text_start
	la a1,__text_end
	sub a1,a0
	jal inst_cache_writeback_invalidate
	nop
*/
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
	//sync
	cache HIT_INVALIDATE_I,0(t1)
    	cache HIT_INVALIDATE_I,0x80(t1)
    	cache HIT_INVALIDATE_I,0x100(t1)
    	cache HIT_INVALIDATE_I,0x180(t1)
    	addi t0,4
	addi t1,4
	addiu t2,-1
	bnez t2,loadintvectorloop
	nop

        jal main
	nop
deadloop:
	j deadloop
	nop

intvector:
	la k1,inthandler
	jr k1
	nop
