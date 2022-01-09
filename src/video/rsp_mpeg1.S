#include "../rspq/rsp_queue.S"
#include "mpeg1_internal.h"

	.section .data.overlay

    RSPQ_OverlayHeader MPEG1_STATE_START, MPEG1_STATE_END, 0x50

COMMAND_TABLE:
	RSPQ_DefineCommand cmd_mpeg1_load_matrix     4  # 0x50
	RSPQ_DefineCommand cmd_mpeg1_store_pixels    4  # 0x51
	RSPQ_DefineCommand cmd_mpeg1_idct            4  # 0x52
	RSPQ_DefineCommand cmd_mpeg1_set_block       8  # 0x53
	RSPQ_DefineCommand cmd_mpeg1_decode_block    8  # 0x54

	vsll_data
	vsll8_data

	.align 4
	.ascii "Dragon RSP MPEG1"
	.ascii " Coded by Rasky "

	.align 4
MPEG1_STATE_START:
IDCT_MATRIX:  .dcb.w    8*8       # 8x8 coefficient matrix
BLOCK_PIXELS: .dcb.b    8*8       # 8x8 pixels (current block)

RDRAM_BLOCK:   .long     0         # Current block in RDRAM: Luminance
RDRAM_BLOCK_PITCH: .long   0           # Pitch of the frame in RDRAM (Luminance)
MPEG1_STATE_END:

	.align 4

IDCT_CONSTS:
	.half 473<<5                  # e(0)
	.half -196<<5                 # e(1)
	.half 362<<5                  # e(2)
	.half 196<<5                  # e(3)
	.half 0x80                    # e(4)
	.half 1<<(7+RSP_IDCT_SCALER)  # e(5)
	.half 1<<8                    # e(6)
	.half 255                     # e(7)

	.text 1

#define pred0  $v22
#define pred1  $v23
#define pred2  $v24
#define pred3  $v25
#define pred4  $v26
#define pred5  $v27
#define pred6  $v28
#define pred7  $v29
#define vshift $v30
#define vconst $v31
#define k473   vconst,e(0)
#define km196  vconst,e(1)
#define k362   vconst,e(2)
#define k196   vconst,e(3)
#define k128   vconst,e(4)
#define k1u    vconst,e(5)
#define k2     vconst,e(6)
#define k255   vconst,e(7)


	.func cmd_mpeg1_set_block
cmd_mpeg1_set_block:
	li s0, %lo(RDRAM_BLOCK)
	sw a0, 0(s0)
	jr ra
	sw a1, 4(s0)
	.endfunc

	.func cmd_mpeg1_load_matrix
cmd_mpeg1_load_matrix:
	move s0, a0
	li t0, DMA_SIZE(8*8*2, 1)
	j DMAIn
	li s4, %lo(IDCT_MATRIX)
	.endfunc

	.func cmd_mpeg1_store_pixels
cmd_mpeg1_store_pixels:
	move s0, a0
	li t0, DMA_SIZE(8*8, 1)
	j DMAOut
	li s4, %lo(BLOCK_PIXELS)
	.endfunc

	.func load_consts
load_consts:
	setup_vsll vshift
	li s1, %lo(IDCT_CONSTS)
	lqv vconst,0, 0,s1
	jr ra
	nop
	.endfunc

	.func idct
idct:
	move ra2, ra

	li s0, %lo(IDCT_MATRIX)
	lqv $v00,0, 0*16,s0
	lqv $v01,0, 1*16,s0
	lqv $v02,0, 2*16,s0
	lqv $v03,0, 3*16,s0
	lqv $v04,0, 4*16,s0
	lqv $v05,0, 5*16,s0
	lqv $v06,0, 6*16,s0
	lqv $v07,0, 7*16,s0

	# Transform columns
	jal mtx_idct_half
	nop

	jal mtx_transpose
	nop

	# Transform rows
	jal mtx_idct_half
	nop

	jal mtx_transpose
	nop

	jr ra2
	nop
	.endfunc

	.func add_pred
add_pred:
	# Add prediction to residual
	# The exact formula, assuming fixed 16.16, is:
	#    clamp_unsigned((PRED + RES + 0x8000) >> 16)
	#
	# where clamp unsigned is clamping the resulting pixel in both
	# directions (so to both 0 and 255).
	# 
	# This sequence VMULU+VMACU is used to perform the addition with rounding
	# *and* clamping to 0 at the same time. The VMULU moves the PRED into the
	# higher part of the accumulator and adds the rounding (0x8000),
	# while the second VMACU moves the RES (residual/pixel) value into the
	# higher part of the accumulator, does the addition, and perform
	# the unsigned clamping in range [0, FFFF]. Obviously the higher
	# range is useless (our pixels are [0..FF]) but at least we get
	# the clamp towards 0 done, which is very annoying to do with
	# RSP otherwise.
	#
	# The two coefficients (k1u and k2) are basically shift values used
	# to align both PRED and RES into bits 16..31 of the accumulator. We need
	# to align them there because that allows us to get the rounding for free
	# since VMULU adds 0x8000 (bit 15).
	vmulu pred0, pred0, k2
	vmacu $v00, $v00, k1u
	vmulu pred1, pred1, k2
	vmacu $v01, $v01, k1u
	vmulu pred2, pred2, k2
	vmacu $v02, $v02, k1u
	vmulu pred3, pred3, k2
	vmacu $v03, $v03, k1u
	vmulu pred4, pred4, k2
	vmacu $v04, $v04, k1u
	vmulu pred5, pred5, k2
	vmacu $v05, $v05, k1u
	vmulu pred6, pred6, k2
	vmacu $v06, $v06, k1u
	vmulu pred7, pred7, k2
	vmacu $v07, $v07, k1u

	# Perform clamping towards 0xFF. This one is easy to do with VCH.
	vch $v00, $v00, k255
	vch $v01, $v01, k255
	vch $v02, $v02, k255
	vch $v03, $v03, k255
	vch $v04, $v04, k255
	vch $v05, $v05, k255
	vch $v06, $v06, k255
	vch $v07, $v07, k255

	# Shift back pixels into the correct bits to be stored in memory with SUV
	vsll $v00, $v00, 7
	vsll $v01, $v01, 7
	vsll $v02, $v02, 7
	vsll $v03, $v03, 7
	vsll $v04, $v04, 7
	vsll $v05, $v05, 7
	vsll $v06, $v06, 7
	vsll $v07, $v07, 7

store_pixels:
	# Store as pixels
	li s0, %lo(BLOCK_PIXELS)
	suv $v00,0, 0*8,s0
	suv $v01,0, 1*8,s0
	suv $v02,0, 2*8,s0
	suv $v03,0, 3*8,s0
	suv $v04,0, 4*8,s0
	suv $v05,0, 5*8,s0
	suv $v06,0, 6*8,s0
	suv $v07,0, 7*8,s0

	jr ra
	nop
	.endfunc

	.func zero_pred
zero_pred:
	vxor pred0, pred0, pred0
	vxor pred1, pred1, pred1
	vxor pred2, pred2, pred2
	vxor pred3, pred3, pred3
	vxor pred4, pred4, pred4
	vxor pred5, pred5, pred5
	vxor pred6, pred6, pred6
	jr ra
	vxor pred7, pred7, pred7
	.endfunc

	.func cmd_mpeg1_idct
cmd_mpeg1_idct:
	jal load_consts
	nop
	jal idct
	nop

	#if RSP_IDCT_SCALER != 0
	vsll $v00, $v00, RSP_IDCT_SCALER
	vsll $v01, $v01, RSP_IDCT_SCALER
	vsll $v02, $v02, RSP_IDCT_SCALER
	vsll $v03, $v03, RSP_IDCT_SCALER
	vsll $v04, $v04, RSP_IDCT_SCALER
	vsll $v05, $v05, RSP_IDCT_SCALER
	vsll $v06, $v06, RSP_IDCT_SCALER
	vsll $v07, $v07, RSP_IDCT_SCALER
	#endif

	vaddc $v00, $v00, k128
	vaddc $v01, $v01, k128
	vaddc $v02, $v02, k128
	vaddc $v03, $v03, k128
	vaddc $v04, $v04, k128
	vaddc $v05, $v05, k128
	vaddc $v06, $v06, k128
	vaddc $v07, $v07, k128

	# Store as pixels
	li s0, %lo(BLOCK_PIXELS)
	spv $v00,0, 0*8,s0
	spv $v01,0, 1*8,s0
	spv $v02,0, 2*8,s0
	spv $v03,0, 3*8,s0
	spv $v04,0, 4*8,s0
	spv $v05,0, 5*8,s0
	spv $v06,0, 6*8,s0
	spv $v07,0, 7*8,s0

	j RSPQ_Loop
	nop
	.endfunc

	.func dma_src_block
dma_src_block:
	li s4, %lo(BLOCK_PIXELS)
	li s0, %lo(RDRAM_BLOCK)
	lw t1, 4(s0)  # pitch
	lw s0, 0(s0)  # address
	j DMAExec
	li t0, DMA_SIZE(8, 8)
	.endfunc

	.func cmd_mpeg1_decode_block
cmd_mpeg1_decode_block:
	jal load_consts
	nop
	# a0 = ncoeffs in matrix (low bytes)
	# a1 = 1=intra 0=inter
	andi a0, 0xFF
	beqz a1, decode_inter
	addi a0, -1

decode_intra:
	# Intra frame: prediction is zero
	jal_and_j zero_pred, decode_step2

decode_inter:
	# Inter frame: load prediction via DMA
	jal dma_src_block
	li t2, DMA_IN
	luv pred0,0, 0*8,s4
	luv pred1,0, 1*8,s4
	luv pred2,0, 2*8,s4
	luv pred3,0, 3*8,s4
	luv pred4,0, 4*8,s4
	luv pred5,0, 5*8,s4
	luv pred6,0, 6*8,s4
	luv pred7,0, 7*8,s4

decode_step2:
	beqz a0, decode_dc_only
	nop

decode_ac:
	jal idct
	nop
	jal_and_j add_pred, decode_finish
	
decode_dc_only:
	li s4, %lo(IDCT_MATRIX)
	vxor $v07, $v07, $v07
	lqv $v00,0, 0,s4
	vor $v00, $v07, $v00,e(0)
	vor $v01, $v07, $v00,e(0)
	vor $v02, $v07, $v00,e(0)
	vor $v03, $v07, $v00,e(0)
	vor $v04, $v07, $v00,e(0)
	vor $v05, $v07, $v00,e(0)
	vor $v06, $v07, $v00,e(0)
	vor $v07, $v07, $v00,e(0)
	jal add_pred
	nop

decode_finish:
	li t2, DMA_OUT
	jal_and_j dma_src_block, RSPQ_Loop
	.endfunc


	.func mtx_transpose
mtx_transpose:
	stv $v00,0,  0*16,s0
	stv $v00,2,  1*16,s0
	stv $v00,4,  2*16,s0
	stv $v00,6,  3*16,s0
	stv $v00,8,  4*16,s0
	stv $v00,10, 5*16,s0
	stv $v00,12, 6*16,s0
	stv $v00,14, 7*16,s0

	ltv $v00,14, 1*16,s0
	ltv $v00,12, 2*16,s0
	ltv $v00,10, 3*16,s0
	ltv $v00,8,  4*16,s0
	ltv $v00,6,  5*16,s0
	ltv $v00,4,  6*16,s0
	ltv $v00,2,  7*16,s0

	jr ra
	nop
	.endfunc

	.func mtx_idct_half
mtx_idct_half:
#define b1    $v04
#define b3    $v08
#define b4    $v09
#define tmp1  $v10
#define tmp2  $v11
#define b6    $v12
#define b7    $v13
#define m0    $v00
#define x4    $v14
#define x0    $v15
#define x1    $v10   // recycle tmp0
#define x2    $v11   // recycle tmp1
#define x3    $v16
#define y3    $v17
#define y4    $v18
#define y5    $v19
#define y6    $v20
#define y7    $v21

	# b3 = v2+v6
	vaddc b3, $v02, $v06
	# b4 = v5-v3
	vsubc b4, $v05, $v03
	vsll b4, b4, 2
	# tmp1 = v1+v7
	vaddc tmp1, $v01, $v07
	# tmp2 = v03 + v05
	vaddc tmp2, $v03, $v05
	# b6 = v1 - v7
	vsubc b6, $v01, $v07
	vsll b6, b6, 2
	# b7 = tmp1 + tmp2
	vaddc b7, tmp1, tmp2
	# x4 = ((b6 * 473 - b4 * 196 + 128) >> 8) - b7
	vmulf x4, b6, k473
	vmacf x4, b4, km196
	vsubc x4, x4, b7
	# x0 = x4 - (((tmp1 - tmp2) * 362 + 128) >> 8);
	vsubc x0, tmp1, tmp2
	vsll x0, x0, 2
	vmulf x0, x0, k362
	vsubc x0, x4, x0
	# x1 = m0 - b1
	vsubc x1, m0, b1
	# x2 = (((v2 - v6) * 362 + 128) >> 8) - b3
	vsubc x2, $v02, $v06
	vsll x2, x2, 2
	vmulf x2, x2, k362
	vsubc x2, x2, b3
	# x3 = m0 + b1
	vaddc x3, m0, b1
	# y3 = x1 + x2
	vaddc y3, x1, x2
	# y4 = x3 + b3
	vaddc y4, x3, b3
	# y5 = x1 - x2
	vsubc y5, x1, x2
	# y6 = x3 - b3
	vsubc y6, x3, b3
	# y7 = -x0 - ((b4 * 473 + b6 * 196 + 128) >> 8)
	vmulf y7, b4, k473
	vmacf y7, b6, k196
	vaddc y7, y7, x0
	vxor $v00, $v00, $v00
	vsubc y7, $v00, y7

	vaddc $v00, b7, y4
	vaddc $v01, x4, y3
	vsubc $v02, y5, x0
	vsubc $v03, y6, y7
	vaddc $v04, y6, y7
	vaddc $v05, x0, y5
	vsubc $v06, y3, x4
	vsubc $v07, y4, b7

	jr ra
	nop

	.endfunc
