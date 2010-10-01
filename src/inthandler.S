/*
   Simple interrupt handler, hands off MIPS interrupts to higher level processes.
   Based on INITS.inc from Neon64.

   It is not reentrant, so interrupts are disabled for the duration.
   Safe for doing most things, including FPU operations, within handlers.
*/

#include "regs.S"

inthandler:
	.global inthandler

	/*Save $2 before using*/
	sd $2,save02

	.set noat
	/*Fetch exception pc off cop#0*/
	mfc0 $2,C0_EPC
	nop

	la k1,save01
	sd $1,(k1)

	mfc0 k1,C0_SR
	la $1,saveSR
	sw k1,($1)
	la $1, ~1
	and k1,$1
	mtc0 k1,C0_SR
	.set at

	/*Save EPC now*/
	sw $2,saveEPC

	/* save GPRs */
	/*sd $2,save02 - removed , saved already.At this point it contains epc*/
	sd $3,save03
	sd $4,save04
	sd $5,save05
	sd $6,save06
	sd $7,save07
	sd $8,save08
	sd $9,save09
	sd $10,save10
	sd $11,save11
	sd $12,save12
	sd $13,save13
	sd $14,save14
	sd $15,save15
	sd $16,save16
	sd $17,save17
	sd $18,save18
	sd $19,save19
	sd $20,save20
	sd $21,save21
	sd $22,save22
	sd $23,save23
	sd $24,save24
	sd $25,save25
	sd $26,save26
	sd $27,save27
	sd $28,save28
	sd $29,save29
	sd $30,save30
	sd $31,save31
	mflo $30
	sd $30,saveLO
	mfhi $30
	sd $30,saveHI
	cfc1 $30,$f31
	nop
	sd $30,saveFC31

	sdc1 $f0,saveFR00
	sdc1 $f1,saveFR01
	sdc1 $f2,saveFR02
	sdc1 $f3,saveFR03
	sdc1 $f4,saveFR04
	sdc1 $f5,saveFR05
	sdc1 $f6,saveFR06
	sdc1 $f7,saveFR07
	sdc1 $f8,saveFR08
	sdc1 $f9,saveFR09
	sdc1 $f10,saveFR10
	sdc1 $f11,saveFR11
	sdc1 $f12,saveFR12
	sdc1 $f13,saveFR13
	sdc1 $f14,saveFR14
	sdc1 $f15,saveFR15
	sdc1 $f16,saveFR16
	sdc1 $f17,saveFR17
	sdc1 $f18,saveFR18
	sdc1 $f19,saveFR19
	sdc1 $f20,saveFR20
	sdc1 $f21,saveFR21
	sdc1 $f22,saveFR22
	sdc1 $f23,saveFR23
	sdc1 $f24,saveFR24
	sdc1 $f25,saveFR25
	sdc1 $f26,saveFR26
	sdc1 $f27,saveFR27
	sdc1 $f28,saveFR28
	sdc1 $f29,saveFR29
	sdc1 $f30,saveFR30
	sdc1 $f31,saveFR31

	la sp,(exception_stack+65*1024-8)
	//lui t0,0xa000
	//or sp,t0
	//or gp,t0

	//j notcount
	//nop

	mfc0 k1,C0_CAUSE
	andi $30,k1,0xff
	beqz $30, justaninterrupt
	nop

	/*:(*/
	jal __onCriticalException
	nop

justaninterrupt:
	/* check for "pre-NMI" (reset) */
	andi $30,k1,0x1000
	beqz $30, notprenmi
	nop

	/* handle reset */
	jal __onResetException
	nop

	j endint
	nop
notprenmi:

	/* check for count=compare */
	and $30,k1,0x8000
	beqz $30,notcount
	nop
	mtc0 $0,$11

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
	/* restore GPRs */
	ld $2,save02
	ld $3,save03
	ld $4,save04
	ld $5,save05
	ld $6,save06
	ld $7,save07
	ld $8,save08
	ld $9,save09
	ld $10,save10
	ld $11,save11
	ld $12,save12
	ld $13,save13
	ld $14,save14
	ld $15,save15
	ld $16,save16
	ld $17,save17
	ld $18,save18
	ld $19,save19
	ld $20,save20
	ld $21,save21
	ld $22,save22
	ld $23,save23
	ld $24,save24
	ld $25,save25
	ld $26,save26
	ld $27,save27
	ld $28,save28
	ld $29,save29
	ld $31,save31
	lw $30,saveSR
	mtc0 $30,C0_SR
	ld $30,saveLO
	mtlo $30
	ld $30,saveHI
	mthi $30

	ldc1 $f0,saveFR00
	ldc1 $f1,saveFR01
	ldc1 $f2,saveFR02
	ldc1 $f3,saveFR03
	ldc1 $f4,saveFR04
	ldc1 $f5,saveFR05
	ldc1 $f6,saveFR06
	ldc1 $f7,saveFR07
	ldc1 $f8,saveFR08
	ldc1 $f9,saveFR09
	ldc1 $f10,saveFR10
	ldc1 $f11,saveFR11
	ldc1 $f12,saveFR12
	ldc1 $f13,saveFR13
	ldc1 $f14,saveFR14
	ldc1 $f15,saveFR15
	ldc1 $f16,saveFR16
	ldc1 $f17,saveFR17
	ldc1 $f18,saveFR18
	ldc1 $f19,saveFR19
	ldc1 $f20,saveFR20
	ldc1 $f21,saveFR21
	ldc1 $f22,saveFR22
	ldc1 $f23,saveFR23
	ldc1 $f24,saveFR24
	ldc1 $f25,saveFR25
	ldc1 $f26,saveFR26
	ldc1 $f27,saveFR27
	ldc1 $f28,saveFR28
	ldc1 $f29,saveFR29
	ldc1 $f30,saveFR30
	ldc1 $f31,saveFR31

	ld $30,saveFC31
	nop

	ctc1 $30,$f31

	ld $30,save30
	.set noat
	la $1,save01
	ld $1,($1)

	eret
	nop
	.set at

	.section .bss
	.global __baseRegAddr
	.long __baseRegAddr

	.align 8
	__baseRegAddr:
	.lcomm save01, 8
	.lcomm save02, 8
	.lcomm save03, 8
	.lcomm save04, 8
	.lcomm save05, 8
	.lcomm save06, 8
	.lcomm save07, 8
	.lcomm save08, 8
	.lcomm save09, 8
	.lcomm save10, 8
	.lcomm save11, 8
	.lcomm save12, 8
	.lcomm save13, 8
	.lcomm save14, 8
	.lcomm save15, 8
	.lcomm save16, 8
	.lcomm save17, 8
	.lcomm save18, 8
	.lcomm save19, 8
	.lcomm save20, 8
	.lcomm save21, 8
	.lcomm save22, 8
	.lcomm save23, 8
	.lcomm save24, 8
	.lcomm save25, 8
	.lcomm save26, 8
	.lcomm save27, 8
	.lcomm save28, 8
	.lcomm save29, 8
	.lcomm save30, 8
	.lcomm save31, 8
	.lcomm saveSR, 4
	.lcomm saveEPC, 4
	.lcomm saveHI, 8
	.lcomm saveLO, 8
	.lcomm saveFC31, 8
	.lcomm saveFR00, 8
	.lcomm saveFR01, 8
	.lcomm saveFR02, 8
	.lcomm saveFR03, 8
	.lcomm saveFR04, 8
	.lcomm saveFR05, 8
	.lcomm saveFR06, 8
	.lcomm saveFR07, 8
	.lcomm saveFR08, 8
	.lcomm saveFR09, 8
	.lcomm saveFR10, 8
	.lcomm saveFR11, 8
	.lcomm saveFR12, 8
	.lcomm saveFR13, 8
	.lcomm saveFR14, 8
	.lcomm saveFR15, 8
	.lcomm saveFR16, 8
	.lcomm saveFR17, 8
	.lcomm saveFR18, 8
	.lcomm saveFR19, 8
	.lcomm saveFR20, 8
	.lcomm saveFR21, 8
	.lcomm saveFR22, 8
	.lcomm saveFR23, 8
	.lcomm saveFR24, 8
	.lcomm saveFR25, 8
	.lcomm saveFR26, 8
	.lcomm saveFR27, 8
	.lcomm saveFR28, 8
	.lcomm saveFR29, 8
	.lcomm saveFR30, 8
	.lcomm saveFR31, 8
	.lcomm exception_stack, 65*1024

