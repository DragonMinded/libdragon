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

#if 0
	/* Run a check to verify that BSS is cleared. This is useful while debugging
	   IPL3 changes. */
	jal __bss_check
	nop
#endif

	/* Copy bootcode flags, stored in DMEM */
	lui v0, 0xA400
	lw t0, 0x0000(v0)
	lw t1, 0x0004(v0)
	sw t0, %gprel(__boot_memsize)(gp)
	sw t1, %gprel(__entropy_state+0)(gp)
	sw t1, %gprel(__entropy_state+4)(gp)

	/* Copy other bootcode flags */
	lbu t0, 0x0009(v0)
	lbu t1, 0x000A(v0)
	lbu t2, 0x000B(v0)
	sw t0, %gprel(__boot_tvtype)(gp)
	sw t1, %gprel(__boot_resettype)(gp)
	sw t2, %gprel(__boot_consoletype)(gp)

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
