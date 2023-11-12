/*
 *   N64 init code for GNU as
 */

#include "regs.S"

	.set noreorder

	.section .boot
	.global _start
_start:
	/* Watchpoints have been proven to persist across resets and even
	 * with the console being off. Zero it as early as possible, to
	 * avoid it triggering during boot. This should really be done
	 * at the start IPL3. */
	mtc0 $0, C0_WATCHLO

	/* Check whether we are running on iQue or N64. Use the MI version register
	   which has LSB set to 0xB0 on iQue. We assume 0xBn was meant for BBPlayer.
	   Notice that we want this test to be hard for emulators to pass by mistake,
	   so checking for a specific value while reading seems solid enough. */
	lw t0, 0x80000318			/* memory size */
	lw t1, 0xA4300004
	andi t1, 0xF0
	bne t1, 0xB0, .Lset_sp
	li fp, 0                    /* fp=0 -> vanilla N64 */

	/* In iQue player, memory allocated to game can be configured and it appears
	   in 0x80000318. On the other hand, the top 8 MiB of RDRAM is reserved to
	   savegames. So avoid putting the stack there, capping the size to 0x7C0000.
	   See also get_memory_size. */
	li fp, 1                    /* fp=1 -> iQue player */
	li t1, 0x800000
	blt t0, t1, .Lset_sp
	nop
	li t0, 0x7C0000

.Lset_sp:
	li t1, 0x80000000 - 0x10	/* sp = KSEG0 + memsize - 0x10 */
	addu sp,t0,t1				/* init stack */
	la gp, _gp					/* init data pointer */
	li v0, 8
	sw v0,(0xbfc007fc)			/* magic N64 hardware init */

	/* a bit from libgloss so we start at a known state */
	li v0,SR_CU1|SR_PE|SR_FR|SR_KX|SR_SX|SR_UX
	mtc0 v0,C0_SR
	mtc0 $0,C0_CAUSE

	/* Store the bbplayer flag now that BSS has been cleared */
	sw fp, __bbplayer

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
