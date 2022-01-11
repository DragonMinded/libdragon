/*
   Simple interrupt handler, hands off MIPS interrupts to higher level processes.
   Based on INITS.inc from Neon64.

   It is not reentrant, so interrupts are disabled for the duration.
   Safe for doing most things, including FPU operations, within handlers.
*/

#include "regs.S"

inthandler:
	.global inthandler

	.set noat
	.set noreorder

#define EXCEPTION_CODE_SYS_CALL      (8<<2)
#define EXCEPTION_CODE_BREAKPOINT    (9<<2)

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

	addiu k0, sp, -EXC_STACK_SIZE
	srl k0, 3
	sll k0, 3

	/* save GPRs */
	sd $1,(STACK_GPR+1*8)(k0)
	.set at
	sd $2,(STACK_GPR+2*8)(k0)
	sd $3,(STACK_GPR+3*8)(k0)
	sd $4,(STACK_GPR+4*8)(k0)
	sd $5,(STACK_GPR+5*8)(k0)
	sd $6,(STACK_GPR+6*8)(k0)
	sd $7,(STACK_GPR+7*8)(k0)
	sd $8,(STACK_GPR+8*8)(k0)
	sd $9,(STACK_GPR+9*8)(k0)
	sd $10,(STACK_GPR+10*8)(k0)
	sd $11,(STACK_GPR+11*8)(k0)
	sd $12,(STACK_GPR+12*8)(k0)
	sd $13,(STACK_GPR+13*8)(k0)
	sd $14,(STACK_GPR+14*8)(k0)
	sd $15,(STACK_GPR+15*8)(k0)
	sd $16,(STACK_GPR+16*8)(k0)
	sd $17,(STACK_GPR+17*8)(k0)
	sd $18,(STACK_GPR+18*8)(k0)
	sd $19,(STACK_GPR+19*8)(k0)
	sd $20,(STACK_GPR+20*8)(k0)
	sd $21,(STACK_GPR+21*8)(k0)
	sd $22,(STACK_GPR+22*8)(k0)
	sd $23,(STACK_GPR+23*8)(k0)
	sd $24,(STACK_GPR+24*8)(k0)
	sd $25,(STACK_GPR+25*8)(k0)
	# No need to save $26 (k0) & $27 (k1), the int handler is free to use them
	sd $28,(STACK_GPR+28*8)(k0)
	sd $29,(STACK_GPR+29*8)(k0)
	sd $30,(STACK_GPR+30*8)(k0)
	sd $31,(STACK_GPR+31*8)(k0)

	mflo k1
	sd k1,STACK_LO(k0)
	mfhi k1
	sd k1,STACK_HI(k0)
	cfc1 k1,$f31
	sw k1,STACK_FC31(k0)

	sdc1 $f0,(STACK_FPR+0*8)(k0)
	sdc1 $f1,(STACK_FPR+1*8)(k0)
	sdc1 $f2,(STACK_FPR+2*8)(k0)
	sdc1 $f3,(STACK_FPR+3*8)(k0)
	sdc1 $f4,(STACK_FPR+4*8)(k0)
	sdc1 $f5,(STACK_FPR+5*8)(k0)
	sdc1 $f6,(STACK_FPR+6*8)(k0)
	sdc1 $f7,(STACK_FPR+7*8)(k0)
	sdc1 $f8,(STACK_FPR+8*8)(k0)
	sdc1 $f9,(STACK_FPR+9*8)(k0)
	sdc1 $f10,(STACK_FPR+10*8)(k0)
	sdc1 $f11,(STACK_FPR+11*8)(k0)
	sdc1 $f12,(STACK_FPR+12*8)(k0)
	sdc1 $f13,(STACK_FPR+13*8)(k0)
	sdc1 $f14,(STACK_FPR+14*8)(k0)
	sdc1 $f15,(STACK_FPR+15*8)(k0)
	sdc1 $f16,(STACK_FPR+16*8)(k0)
	sdc1 $f17,(STACK_FPR+17*8)(k0)
	sdc1 $f18,(STACK_FPR+18*8)(k0)
	sdc1 $f19,(STACK_FPR+19*8)(k0)
	sdc1 $f20,(STACK_FPR+20*8)(k0)
	sdc1 $f21,(STACK_FPR+21*8)(k0)
	sdc1 $f22,(STACK_FPR+22*8)(k0)
	sdc1 $f23,(STACK_FPR+23*8)(k0)
	sdc1 $f24,(STACK_FPR+24*8)(k0)
	sdc1 $f25,(STACK_FPR+25*8)(k0)
	sdc1 $f26,(STACK_FPR+26*8)(k0)
	sdc1 $f27,(STACK_FPR+27*8)(k0)
	sdc1 $f28,(STACK_FPR+28*8)(k0)
	sdc1 $f29,(STACK_FPR+29*8)(k0)
	sdc1 $f30,(STACK_FPR+30*8)(k0)
	sdc1 $f31,(STACK_FPR+31*8)(k0)

	/* Fetch exception pc off cop0 */
	mfc0 k1, C0_EPC
	sw k1, STACK_EPC(k0)

	/* Mark interrupts as disabled. TODO: is this really required? */
	mfc0 k1, C0_SR
	sw k1, STACK_SR(k0)
	li t0, ~1
	and k1, t0
	mtc0 k1, C0_SR

	mfc0 k1, C0_CAUSE
	sw k1, STACK_CR(k0)

	# Set standard GP register, in case we were called from assembly
	# code that has temporarily used GP as scratch register.
	la gp, _gp

	move sp, k0

	andi t0, k1, 0xff
	beqz t0, justaninterrupt
	nop
	bne t0, EXCEPTION_CODE_SYS_CALL, critical_exception
	nop

thread_schedule:
	# Call tkreadh_syscall_schedule, passing a pointer to the exception stack
	# (reg_block_t in C). As explained above, this is found on the stack
	# after the 32-bytes argument slot required by MIPS C ABI.
	addiu a0, sp, 32
	jal __kthread_syscall_schedule
	nop

	addiu sp, v0, -32
	j endint2
	nop

critical_exception:
	/*:(*/
	addiu a0, sp, 32
	jal __onCriticalException
	nop

	j endint
	nop

justaninterrupt:
	/* check for "pre-NMI" (reset) */
	andi t0,k1,0x1000
	beqz t0, notprenmi
	nop

	/* handle reset */
	addiu a0, sp, 32
	jal __onResetException
	nop

	j endint
	nop

notprenmi:

	/* check for count=compare */
	and t0,k1,0x8000
	beqz t0,notcount
	nop
	/* Writing C0_COMPARE acknowledges the timer interrupt (clear the interrupt
	   bit in C0_CAUSE, otherwise the interrupt would retrigger). We write
	   the current value so that we don't destroy it in case it's needed. */
	mfc0 t0,C0_COMPARE
	mtc0 t0,C0_COMPARE

	/* handle timer exception */
	jal __TI_handler
	nop

	j endint
	nop
notcount:

	/* pass anything else along to handler */
	jal __MI_handler
	nop

endint:
	la t0, __isr_force_schedule
	lbu t1, (t0)
	bnez t1, thread_schedule
	sb zero, (t0)

endint2:
	/* restore GPRs */
	move k0, sp
	ld $2,(STACK_GPR+2*8)(k0)
	ld $3,(STACK_GPR+3*8)(k0)
	ld $4,(STACK_GPR+4*8)(k0)
	ld $5,(STACK_GPR+5*8)(k0)
	ld $6,(STACK_GPR+6*8)(k0)
	ld $7,(STACK_GPR+7*8)(k0)
	ld $8,(STACK_GPR+8*8)(k0)
	ld $9,(STACK_GPR+9*8)(k0)
	ld $10,(STACK_GPR+10*8)(k0)
	ld $11,(STACK_GPR+11*8)(k0)
	ld $12,(STACK_GPR+12*8)(k0)
	ld $13,(STACK_GPR+13*8)(k0)
	ld $14,(STACK_GPR+14*8)(k0)
	ld $15,(STACK_GPR+15*8)(k0)
	ld $16,(STACK_GPR+16*8)(k0)
	ld $17,(STACK_GPR+17*8)(k0)
	ld $18,(STACK_GPR+18*8)(k0)
	ld $19,(STACK_GPR+19*8)(k0)
	ld $20,(STACK_GPR+20*8)(k0)
	ld $21,(STACK_GPR+21*8)(k0)
	ld $22,(STACK_GPR+22*8)(k0)
	ld $23,(STACK_GPR+23*8)(k0)
	ld $24,(STACK_GPR+24*8)(k0)
	ld $25,(STACK_GPR+25*8)(k0)
	# No need to restore $26 (k0) & $27 (k1), the int handler is free to use them
	ld $28,(STACK_GPR+28*8)(k0)
	ld $29,(STACK_GPR+29*8)(k0)
	ld $30,(STACK_GPR+30*8)(k0)
	ld $31,(STACK_GPR+31*8)(k0)

	lw k1,STACK_EPC(k0)
	mtc0 k1,C0_EPC

	lw k1,STACK_SR(k0)
	mtc0 k1,C0_SR

	ld k1,STACK_LO(k0)
	mtlo k1

	ld k1,STACK_HI(k0)
	mthi k1

	ldc1 $f0,(STACK_FPR+0*8)(k0)
	ldc1 $f1,(STACK_FPR+1*8)(k0)
	ldc1 $f2,(STACK_FPR+2*8)(k0)
	ldc1 $f3,(STACK_FPR+3*8)(k0)
	ldc1 $f4,(STACK_FPR+4*8)(k0)
	ldc1 $f5,(STACK_FPR+5*8)(k0)
	ldc1 $f6,(STACK_FPR+6*8)(k0)
	ldc1 $f7,(STACK_FPR+7*8)(k0)
	ldc1 $f8,(STACK_FPR+8*8)(k0)
	ldc1 $f9,(STACK_FPR+9*8)(k0)
	ldc1 $f10,(STACK_FPR+10*8)(k0)
	ldc1 $f11,(STACK_FPR+11*8)(k0)
	ldc1 $f12,(STACK_FPR+12*8)(k0)
	ldc1 $f13,(STACK_FPR+13*8)(k0)
	ldc1 $f14,(STACK_FPR+14*8)(k0)
	ldc1 $f15,(STACK_FPR+15*8)(k0)
	ldc1 $f16,(STACK_FPR+16*8)(k0)
	ldc1 $f17,(STACK_FPR+17*8)(k0)
	ldc1 $f18,(STACK_FPR+18*8)(k0)
	ldc1 $f19,(STACK_FPR+19*8)(k0)
	ldc1 $f20,(STACK_FPR+20*8)(k0)
	ldc1 $f21,(STACK_FPR+21*8)(k0)
	ldc1 $f22,(STACK_FPR+22*8)(k0)
	ldc1 $f23,(STACK_FPR+23*8)(k0)
	ldc1 $f24,(STACK_FPR+24*8)(k0)
	ldc1 $f25,(STACK_FPR+25*8)(k0)
	ldc1 $f26,(STACK_FPR+26*8)(k0)
	ldc1 $f27,(STACK_FPR+27*8)(k0)
	ldc1 $f28,(STACK_FPR+28*8)(k0)
	ldc1 $f29,(STACK_FPR+29*8)(k0)
	ldc1 $f30,(STACK_FPR+30*8)(k0)
	ldc1 $f31,(STACK_FPR+31*8)(k0)

	lw k1, STACK_FC31(k0)
	ctc1 k1, $f31

	.set noat
	ld $1,(STACK_GPR+1*8)(k0)
	eret
	nop
