/*
 *   N64 init code for GNU as
 */

#include "regs.S"

	.set noreorder

	.section .boot
	.global _start
_start:
	lw t0, 0x80000318			/* memory size */

	/* Check whether we are running on iQue or N64. Use the MI version register
	   which has LSB set to 0xB0 on iQue. We assume 0xBn was meant for BBPlayer.
	   Notice that we want this test to be hard for emulators to pass by mistake,
	   so checking for a specific value while reading seems solid enough. */
	lw t1, 0xA4300004
	andi t1, 0xF0
	bne t1, 0xB0, set_sp
	li fp, 0                    /* fp=0 -> vanilla N64 */

	/* In iQue player, memory allocated to game can be configured and it appears
	   in 0x80000318. On the other hand, the top 8 MiB of RDRAM is reserved to
	   savegames. So avoid putting the stack there, capping the size to 0x7C0000.
	   See also get_memory_size. */
	li fp, 1                    /* fp=1 -> iQue player */
	li t1, 0x800000
	blt t0, t1, set_sp
	nop
	li t0, 0x7C0000

set_sp:
	li t1, 0x7FFFFFF0
	addu sp,t0,t1				/* init stack */
	la gp, _gp					/* init data pointer */
	li v0, 8
	sw v0,(0xbfc007fc)			/* magic N64 hardware init */

	/* a bit from libgloss so we start at a known state */
	li v0,SR_CU1|SR_PE|SR_FR|SR_KX|SR_SX|SR_UX
	mtc0 v0,C0_SR
	mtc0 $0,C0_CAUSE

	/* copy code and data via DMA */
	la a0, __text_start
	la a1, __data_end
	la t0, __libdragon_text_start
	subu a2, a0, t0				/* skip over .boot section */
	addu a2, 0x10001000			/* address in rom */

	/* Start PI DMA transfer */
	lui t0, 0xA460
	sw a0, 0x00(t0)
	sw a2, 0x04(t0)
	sub t1, a1, a0
	addi t1, -1
	sw t1, 0x0C(t0)

	/* fill .bss with 0s */
	la a0, __bss_start
	or a0, 0x20000000
	la a1, __bss_end
	or a1, 0x20000000
bss_init:
	sd $0,(a0)
	addiu a0,8
	bltu a0,a1, bss_init
	nop

	/* Wait for DMA transfer to be finished */
	lui t0, 0xA460
wait_dma_end:
	lw t1, 0x10(t0)
	andi t1, 3
	bnez t1, wait_dma_end
	nop

	/* Store the bbplayer flag now that BSS has been cleared */
	sw fp, __bbplayer

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

	la t0, debug_assert_func    /* install assert function in system.c */
	la t1, __assert_func_ptr
	sw t0, 0(t1)	

	jal __do_global_ctors		/* call global constructors */
	nop
	li a0, 0
	jal main					/* call main app */
	li a1, 0

deadloop:
	j deadloop
	nop

intvector:
	la k1,inthandler
	jr k1
	nop

	.section .code
