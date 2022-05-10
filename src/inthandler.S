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

	# Save caller-saved GPRs only. These are the only
	# ones required to call a C function from assembly, as the
	# others (callee-saved) would be preserved by the function 
	# itself, if modified.
	sd $1, (STACK_GPR+ 1*8)(k0) # AT
	.set at
	sd $2, (STACK_GPR+ 2*8)(k0) # V0
	sd $3, (STACK_GPR+ 3*8)(k0) # V1
	sd $4, (STACK_GPR+ 4*8)(k0) # A0
	sd $5, (STACK_GPR+ 5*8)(k0) # A1
	sd $6, (STACK_GPR+ 6*8)(k0) # A2
	sd $7, (STACK_GPR+ 7*8)(k0) # A3
	sd $8, (STACK_GPR+ 8*8)(k0) # T0
	sd $9, (STACK_GPR+ 9*8)(k0) # T1
	sd $10,(STACK_GPR+10*8)(k0) # T2
	sd $11,(STACK_GPR+11*8)(k0) # T3 
	sd $12,(STACK_GPR+12*8)(k0) # T4
	sd $13,(STACK_GPR+13*8)(k0) # T5
	sd $14,(STACK_GPR+14*8)(k0) # T6
	sd $15,(STACK_GPR+15*8)(k0) # T7
	sd $24,(STACK_GPR+24*8)(k0) # T8
	sd $25,(STACK_GPR+25*8)(k0) # T9
	sd $31,(STACK_GPR+31*8)(k0) # RA

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

	mfc0 k1, C0_EPC
	sw k1, STACK_EPC(k0)

	mfc0 k1, C0_SR
	sw k1, STACK_SR(k0)

	# Since all critical information about current exception has been saved,
	# we can now turn off EXL. This allows a reentrant exception to save its
	# own full context for operating. At the same time, it is better to keep
	# interrupts disabled so that we don't risk triggering recursive interrupts,
	# so disable IE as well.
	and k1, ~(SR_IE | SR_EXL)
	mtc0 k1, C0_SR

	mfc0 k1, C0_CAUSE
	sw k1, STACK_CR(k0)

	move sp, k0

	andi t0, k1, 0xff
	beqz t0, interrupt
	nop

critical_exception:
	# Make sure that all registers are saved in the exception frame
	jal finalize_exception_frame
	nop

	/* Exception not specially handled. */
	addiu a0, sp, 32
	jal __onCriticalException
	nop

	j end_interrupt
	nop

interrupt:
	/* check for "pre-NMI" (reset) */
	andi t0,k1,0x1000
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

	j end_interrupt
	nop
notcount:

	/* pass anything else along to handler */
	jal __MI_handler
	nop
	j end_interrupt
	nop

end_interrupt:
	move k0, sp
	addiu sp, EXC_STACK_SIZE
	/* restore GPRs */
	ld $2,(STACK_GPR + 2*8)(k0)
	ld $3,(STACK_GPR + 3*8)(k0)
	ld $4,(STACK_GPR + 4*8)(k0)
	ld $5,(STACK_GPR + 5*8)(k0)
	ld $6,(STACK_GPR + 6*8)(k0)
	ld $7,(STACK_GPR + 7*8)(k0)
	ld $8,(STACK_GPR + 8*8)(k0)
	ld $9,(STACK_GPR + 9*8)(k0)
	ld $10,(STACK_GPR+10*8)(k0)
	ld $11,(STACK_GPR+11*8)(k0)
	ld $12,(STACK_GPR+12*8)(k0)
	ld $13,(STACK_GPR+13*8)(k0)
	ld $14,(STACK_GPR+14*8)(k0)
	ld $15,(STACK_GPR+15*8)(k0)
	ld $24,(STACK_GPR+24*8)(k0)
	ld $25,(STACK_GPR+25*8)(k0)
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

	lw k1, STACK_FC31(k0)
	ctc1 k1, $f31

	.set noat
	ld $1,(STACK_GPR+1*8)(k0)
	eret
	nop

finalize_exception_frame:
	sd $16,(STACK_GPR+16*8)(k0)   # S0
	sd $17,(STACK_GPR+17*8)(k0)   # S1
	sd $18,(STACK_GPR+18*8)(k0)   # S2
	sd $19,(STACK_GPR+19*8)(k0)   # S3
	sd $20,(STACK_GPR+20*8)(k0)   # S4
	sd $21,(STACK_GPR+21*8)(k0)   # S5
	sd $22,(STACK_GPR+22*8)(k0)   # S6
	sd $23,(STACK_GPR+23*8)(k0)   # S7
	sd $28,(STACK_GPR+28*8)(k0)   # GP
	# SP has been modified to make space for the exception frame,
	# but we want to save the previous value in the exception frame itself.
	addiu $1, sp, EXC_STACK_SIZE
	sd $1, (STACK_GPR+29*8)(k0)   # SP
	sd $30,(STACK_GPR+30*8)(k0)   # FP
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
	jr ra
	nop
