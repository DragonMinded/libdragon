/*
   Simple interrupt handler, hands off MIPS interrupts to higher level processes.
   Based on INITS.inc from Neon64.

   It is not reentrant, so interrupts are disabled for the duration.
   Safe for doing most things, including FPU operations, within handlers.
*/

#include "regs.S"

	.align 5
inthandler:
	.global inthandler

	.set noat
	.set noreorder

# The exception stack contains a dump of all GPRs/FPRs. This requires 544 bytes.
# On top of that, we need 32 bytes of empty space at offset 0-31, because
# that is required by MIPS ABI when calling C functions (it's a space called
# "argument slots" -- even if the function takes no arguments, or are only passed in 
# registers, the ABI requires reserving that space and called functions might
# use it to store local variables).
# So we keep 0-31 empty, and we start saving GPRs from 32, and then FPR. See
# the other macros to see the actual layout.
#
# *NOTE*: this layout is also exposed in C via regblock_t in exception.h
# Please keep in sync!
#define EXC_STACK_SIZE (544+32)
#define STACK_GPR      32
#define STACK_HI     (STACK_GPR+(32*8))
#define STACK_LO     (STACK_HI+8)
#define STACK_SR     (STACK_LO+8)
#define STACK_CR     (STACK_SR+4)
#define STACK_EPC    (STACK_CR+4)
#define STACK_FC31   (STACK_EPC+4)
#define STACK_FPR    (STACK_FC31+4)

	addiu sp, -EXC_STACK_SIZE

	# Save caller-saved GPRs only. These are the only
	# ones required to call a C function from assembly, as the
	# others (callee-saved) would be preserved by the function 
	# itself, if modified.
	sd $1, (STACK_GPR+ 1*8)(sp) # AT
	.set at
	sd $2, (STACK_GPR+ 2*8)(sp) # V0
	sd $3, (STACK_GPR+ 3*8)(sp) # V1
	sd $4, (STACK_GPR+ 4*8)(sp) # A0
	sd $5, (STACK_GPR+ 5*8)(sp) # A1
	sd $6, (STACK_GPR+ 6*8)(sp) # A2
	sd $7, (STACK_GPR+ 7*8)(sp) # A3
	sd $8, (STACK_GPR+ 8*8)(sp) # T0
	sd $9, (STACK_GPR+ 9*8)(sp) # T1
	sd $10,(STACK_GPR+10*8)(sp) # T2
	sd $11,(STACK_GPR+11*8)(sp) # T3 
	sd $12,(STACK_GPR+12*8)(sp) # T4
	sd $13,(STACK_GPR+13*8)(sp) # T5
	sd $14,(STACK_GPR+14*8)(sp) # T6
	sd $15,(STACK_GPR+15*8)(sp) # T7
	sd $24,(STACK_GPR+24*8)(sp) # T8
	sd $25,(STACK_GPR+25*8)(sp) # T9
	sd $31,(STACK_GPR+31*8)(sp) # RA

	mflo k0
	mfhi k1
	sd k0,STACK_LO(sp)
	sd k1,STACK_HI(sp)

	mfc0 k0, C0_EPC
	mfc0 k1, C0_SR
	sw k0, STACK_EPC(sp)
	sw k1, STACK_SR(sp)

	# Since all critical information about current exception has been saved,
	# we can now turn off EXL. This allows a reentrant exception to save its
	# own full context for operating. At the same time, it is better to keep
	# interrupts disabled so that we don't risk triggering recursive interrupts,
	# so disable IE as well.
	and k1, ~(SR_IE | SR_EXL)
	mtc0 k1, C0_SR

	# WARNING: it is now possible to trigger reentrant exceptions (and not only
	# crashing one. Avoid using k0/k1 from now on, as they would get corrupted
	# by a reentrant exception.
	#define cause t8

	mfc0 cause, C0_CAUSE
	sw cause, STACK_CR(sp)

	andi t0, cause, 0xff
	beqz t0, interrupt
	nop

exception:
	# This is an exception, not an interrupt. We want to save the full processor
	# state in the exception frame, so all registers including FPU regs.
	# Make sure FPU is activated in this context. It could be deactivated if
	# this exception happened within an interrupt (where FPU is disabled by default).
	mfc0 t0, C0_SR
	or t0, SR_CU1
	mtc0 t0, C0_SR

	# Save the callee-saved FPU regs
	jal save_fpu_regs
	move a0, sp

	# Save all the CPU+FPU caller-saved regs, which are normally
	# not saved for an interrupt.
	jal finalize_exception_frame
	nop

	# Check the exception type
	andi t0, cause, CAUSE_EXC_MASK
	bne t0, CAUSE_EXC_COPROCESSOR, critical_exception
	nop

exception_coprocessor:
	# Extract CE bits (28..29) from CR
	srl t0, cause, 28
	andi t0, 3
	# If == 1 (COP1), it is an FPU exception
	bne t0, 1, critical_exception
	nop

exception_coprocessor_fpu:
	# FPU exception. This happened because of the use of FPU in an interrupt handler,
	# where it is disabled by default. We must save the full FPU context,
	# reactivate the FPU, and then return from exception, so that the FPU instruction
	# is executed again and this time it will work.

	# Make sure that FPU will also be enabled when we exit this exception
	lw t0, STACK_SR(sp)
	or t0, SR_CU1
	sw t0, STACK_SR(sp)

	# Save the FPU registers into the *underlying* interrupt context.
	# That is, we want to make sure that they get restored when the
	# underlying interrupt exits.
	jal save_fpu_regs
	lw a0, interrupt_exception_frame

	# OK we are done. We can now exit the exception
	j end_interrupt
	nop

critical_exception:

	/* Exception not specially handled. */
	addiu a0, sp, 32
	jal __onCriticalException
	nop

	j end_interrupt
	nop

interrupt:
	# This is an interrupt. 
	# First of all, disable FPU coprocessor so that we can avoid saving FPU
	# registers altogether.
	mfc0 t0, C0_SR
	and t0, ~SR_CU1
	mtc0 t0, C0_SR

	# If a FPU instruction is executed during the interrupt handler, a nested
	# exception will trigger. The nested handler will enable the FPU and save
	# the FPU registers into the interrupt exception frame. To do so, it needs
	# to know *where* the interrupt exception frame is. That is, we need
	# to store the current stack pointer somewhere.
	# Notice that interrupts cannot be reentrant (only exceptions are), so
	# a single variable will suffice.
	sw sp, interrupt_exception_frame

	/* check for "pre-NMI" (reset) */
	andi t0, cause, 0x1000
	beqz t0, notprenmi
	nop

	/* handle reset */
	addiu a0, sp, 32
	jal __onResetException
	nop

	j end_interrupt
	nop

notprenmi:

	/* check for count=compare */
	and t0, cause, 0x8000
	beqz t0,notcount
	nop
	/* Writing C0_COMPARE acknowledges the timer interrupt (clear the interrupt
	   bit in C0_CAUSE, otherwise the interrupt would retrigger). We write
	   the current value so that we don't destroy it in case it's needed. */
	mfc0 t0,C0_COMPARE
	mtc0 t0,C0_COMPARE

	/* handle timer interrupt */
	jal __TI_handler
	nop

	j end_interrupt
	nop
notcount:
	and t0, cause, 0x800
	beqz t0, notcart
	nop

	/* handle CART interrupt */
	jal __CART_handler
	nop

	j end_interrupt
	nop

notcart:
	/* pass anything else along to MI (RCP) handler */
	jal __MI_handler
	addiu a0, sp, 32
	j end_interrupt
	nop

end_interrupt:
	mfc0 t0, C0_SR
	and t0, SR_CU1
	beqz t0, end_interrupt_gpr
	nop

	ldc1 $f0, (STACK_FPR+ 0*8)(sp)
	ldc1 $f1, (STACK_FPR+ 1*8)(sp)
	ldc1 $f2, (STACK_FPR+ 2*8)(sp)
	ldc1 $f3, (STACK_FPR+ 3*8)(sp)
	ldc1 $f4, (STACK_FPR+ 4*8)(sp)
	ldc1 $f5, (STACK_FPR+ 5*8)(sp)
	ldc1 $f6, (STACK_FPR+ 6*8)(sp)
	ldc1 $f7, (STACK_FPR+ 7*8)(sp)
	ldc1 $f8, (STACK_FPR+ 8*8)(sp)
	ldc1 $f9, (STACK_FPR+ 9*8)(sp)
	ldc1 $f10,(STACK_FPR+10*8)(sp)
	ldc1 $f11,(STACK_FPR+11*8)(sp)
	ldc1 $f12,(STACK_FPR+12*8)(sp)
	ldc1 $f13,(STACK_FPR+13*8)(sp)
	ldc1 $f14,(STACK_FPR+14*8)(sp)
	ldc1 $f15,(STACK_FPR+15*8)(sp)
	ldc1 $f16,(STACK_FPR+16*8)(sp)
	ldc1 $f17,(STACK_FPR+17*8)(sp)
	ldc1 $f18,(STACK_FPR+18*8)(sp)
	ldc1 $f19,(STACK_FPR+19*8)(sp)

	lw t0, STACK_FC31(sp)
	ctc1 t0, $f31

end_interrupt_gpr:

	# Restore SR. This also disables reentrant exceptions by
	# restoring the EXL bit into SR
	.set noat
	lw t0, STACK_SR(sp)
	mtc0 t0, C0_SR

	ld t0, STACK_LO(sp)
	ld t1, STACK_HI(sp)
	lw t2, STACK_EPC(sp)
	mtlo t0
	mthi t1
	mtc0 t2, C0_EPC

	/* restore GPRs */
	ld $1,(STACK_GPR + 1*8)(sp)
	ld $2,(STACK_GPR + 2*8)(sp)
	ld $3,(STACK_GPR + 3*8)(sp)
	ld $4,(STACK_GPR + 4*8)(sp)
	ld $5,(STACK_GPR + 5*8)(sp)
	ld $6,(STACK_GPR + 6*8)(sp)
	ld $7,(STACK_GPR + 7*8)(sp)
	ld $8,(STACK_GPR + 8*8)(sp)
	ld $9,(STACK_GPR + 9*8)(sp)
	ld $10,(STACK_GPR+10*8)(sp)
	ld $11,(STACK_GPR+11*8)(sp)
	ld $12,(STACK_GPR+12*8)(sp)
	ld $13,(STACK_GPR+13*8)(sp)
	ld $14,(STACK_GPR+14*8)(sp)
	ld $15,(STACK_GPR+15*8)(sp)
	ld $24,(STACK_GPR+24*8)(sp)
	ld $25,(STACK_GPR+25*8)(sp)
	ld $31,(STACK_GPR+31*8)(sp)
	addiu sp, EXC_STACK_SIZE
	eret

	.align 5
finalize_exception_frame:
	sd $16,(STACK_GPR+16*8)(sp)   # S0
	sd $17,(STACK_GPR+17*8)(sp)   # S1
	sd $18,(STACK_GPR+18*8)(sp)   # S2
	sd $19,(STACK_GPR+19*8)(sp)   # S3
	sd $20,(STACK_GPR+20*8)(sp)   # S4
	sd $21,(STACK_GPR+21*8)(sp)   # S5
	sd $22,(STACK_GPR+22*8)(sp)   # S6
	sd $23,(STACK_GPR+23*8)(sp)   # S7
	sd $28,(STACK_GPR+28*8)(sp)   # GP
	# SP has been modified to make space for the exception frame,
	# but we want to save the previous value in the exception frame itself.
	addiu $1, sp, EXC_STACK_SIZE
	sd $1, (STACK_GPR+29*8)(sp)   # SP
	sd $30,(STACK_GPR+30*8)(sp)   # FP
	sdc1 $f20,(STACK_FPR+20*8)(sp)
	sdc1 $f21,(STACK_FPR+21*8)(sp)
	sdc1 $f22,(STACK_FPR+22*8)(sp)
	sdc1 $f23,(STACK_FPR+23*8)(sp)
	sdc1 $f24,(STACK_FPR+24*8)(sp)
	sdc1 $f25,(STACK_FPR+25*8)(sp)
	sdc1 $f26,(STACK_FPR+26*8)(sp)
	sdc1 $f27,(STACK_FPR+27*8)(sp)
	sdc1 $f28,(STACK_FPR+28*8)(sp)
	sdc1 $f29,(STACK_FPR+29*8)(sp)
	sdc1 $f30,(STACK_FPR+30*8)(sp)
	sdc1 $f31,(STACK_FPR+31*8)(sp)
	jr ra
	nop

	.align 5
save_fpu_regs:
	cfc1 $1, $f31
	sw $1, STACK_FC31(a0)
	sdc1 $f0, (STACK_FPR+ 0*8)(a0)
	sdc1 $f1, (STACK_FPR+ 1*8)(a0)
	sdc1 $f2, (STACK_FPR+ 2*8)(a0)
	sdc1 $f3, (STACK_FPR+ 3*8)(a0)
	sdc1 $f4, (STACK_FPR+ 4*8)(a0)
	sdc1 $f5, (STACK_FPR+ 5*8)(a0)
	sdc1 $f6, (STACK_FPR+ 6*8)(a0)
	sdc1 $f7, (STACK_FPR+ 7*8)(a0)
	sdc1 $f8, (STACK_FPR+ 8*8)(a0)
	sdc1 $f9, (STACK_FPR+ 9*8)(a0)
	sdc1 $f10,(STACK_FPR+10*8)(a0)
	sdc1 $f11,(STACK_FPR+11*8)(a0)
	sdc1 $f12,(STACK_FPR+12*8)(a0)
	sdc1 $f13,(STACK_FPR+13*8)(a0)
	sdc1 $f14,(STACK_FPR+14*8)(a0)
	sdc1 $f15,(STACK_FPR+15*8)(a0)
	sdc1 $f16,(STACK_FPR+16*8)(a0)
	sdc1 $f17,(STACK_FPR+17*8)(a0)
	sdc1 $f18,(STACK_FPR+18*8)(a0)
	sdc1 $f19,(STACK_FPR+19*8)(a0)
	jr ra
	nop


	.section .bss
 	.align 8
	.lcomm interrupt_exception_frame, 4

