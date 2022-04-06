# RSP ucode that is used as part of the crash handler.
# It extracts the value of all registers into DMEM so that they can be
# shown in the exception screen.

#include <rsp.inc>

	.data

EMPTY: .long 0

	.text

	.globl _start
_start:
	.set noat
	sw $0,  0*4(zero)
	sw $1,  1*4(zero)
	sw $2,  2*4(zero)
	sw $3,  3*4(zero)
	sw $4,  4*4(zero)
	sw $5,  5*4(zero)
	sw $6,  6*4(zero)
	sw $7,  7*4(zero)
	sw $8,  8*4(zero)
	sw $9,  9*4(zero)
	sw $10, 10*4(zero)
	sw $11, 11*4(zero)
	sw $12, 12*4(zero)
	sw $13, 13*4(zero)
	sw $14, 14*4(zero)
	sw $15, 15*4(zero)
	sw $16, 16*4(zero)
	sw $17, 17*4(zero)
	sw $18, 18*4(zero)
	sw $19, 19*4(zero)
	sw $20, 20*4(zero)
	sw $21, 21*4(zero)
	sw $22, 22*4(zero)
	sw $23, 23*4(zero)
	sw $24, 24*4(zero)
	sw $25, 25*4(zero)
	sw $26, 26*4(zero)
	sw $27, 27*4(zero)
	sw $28, 28*4(zero)
	sw $29, 29*4(zero)
	sw $30, 30*4(zero)
	sw $31, 31*4(zero)

	li s0, 32*4
	sqv $v00,  0*16,s0
	sqv $v01,  1*16,s0
	sqv $v02,  2*16,s0
	sqv $v03,  3*16,s0
	sqv $v04,  4*16,s0
	sqv $v05,  5*16,s0
	sqv $v06,  6*16,s0
	sqv $v07,  7*16,s0
	sqv $v08,  8*16,s0
	sqv $v09,  9*16,s0
	sqv $v10, 10*16,s0
	sqv $v11, 11*16,s0
	sqv $v12, 12*16,s0
	sqv $v13, 13*16,s0
	sqv $v14, 14*16,s0
	sqv $v15, 15*16,s0
	sqv $v16, 16*16,s0
	sqv $v17, 17*16,s0
	sqv $v18, 18*16,s0
	sqv $v19, 19*16,s0
	sqv $v20, 20*16,s0
	sqv $v21, 21*16,s0
	sqv $v22, 22*16,s0
	sqv $v23, 23*16,s0
	sqv $v24, 24*16,s0
	sqv $v25, 25*16,s0
	sqv $v26, 26*16,s0
	sqv $v27, 27*16,s0
	sqv $v28, 28*16,s0
	sqv $v29, 29*16,s0
	sqv $v30, 30*16,s0
	sqv $v31, 31*16,s0

    vsar $v00, COP2_ACC_HI
    vsar $v01, COP2_ACC_MD
    vsar $v02, COP2_ACC_LO

	sqv $v00,  32*16,s0
	sqv $v01,  33*16,s0
	sqv $v02,  34*16,s0

	add s0, 35*16

	mfc0 t0, $0
	mfc0 t1, $1
	mfc0 t2, $2
	mfc0 t3, $3
	mfc0 t4, $4
	mfc0 t5, $5
	mfc0 t6, $6
	mfc0 t7, $7

	sw t0, 0*4(s0)
	sw t1, 1*4(s0)
	sw t2, 2*4(s0)
	sw t3, 3*4(s0)
	sw t4, 4*4(s0)
	sw t5, 5*4(s0)
	sw t6, 6*4(s0)
	sw t7, 7*4(s0)

	mfc0 t0, $8
	mfc0 t1, $9
	mfc0 t2, $10
	mfc0 t3, $11
	mfc0 t4, $12
	mfc0 t5, $13
	mfc0 t6, $14
	mfc0 t7, $15

	sw t0, 8*4(s0)
	sw t1, 9*4(s0)
	sw t2, 10*4(s0)
	sw t3, 11*4(s0)
	sw t4, 12*4(s0)
	sw t5, 13*4(s0)
	sw t6, 14*4(s0)
	sw t7, 15*4(s0)

	add s0, 16*4

	cfc2 t0, $0
	cfc2 t1, $1
	cfc2 t2, $2

	sw t0, 0*4(s0)
	sw t1, 1*4(s0)
	sw t2, 2*4(s0)

	break


