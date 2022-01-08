#include "../rspq/rsp_queue.S"

	.section .data.overlay

    RSPQ_OverlayHeader MPEG1_STATE_START, MPEG1_STATE_END, 0x50

COMMAND_TABLE:
	RSPQ_DefineCommand cmd_mpeg1_load_matrix     4  # 0x50
	RSPQ_DefineCommand cmd_mpeg1_store_matrix    4  # 0x51
	RSPQ_DefineCommand cmd_mpeg1_idct            4  # 0x52

	vsll_data
	vsll8_data

	.align 4
	.ascii "Dragon RSP MPEG1"
	.ascii " Coded by Rasky "

	.align 4
MPEG1_STATE_START:
IDCT_MATRIX: .dcb.w 64
MPEG1_STATE_END:

	.align 4

IDCT_CONSTS:
	.half 473<<5
	.half -196<<5
	.half 362<<5
	.half 196<<5


	.text 1

	.func cmd_mpeg1_load_matrix
cmd_mpeg1_load_matrix:
	move s0, a0
	li t0, DMA_SIZE(8*8*2, 1)
	j DMAIn
	li s4, %lo(IDCT_MATRIX)
	.endfunc

	.func cmd_mpeg1_store_matrix
cmd_mpeg1_store_matrix:
	move s0, a0
	li t0, DMA_SIZE(8*8*2, 1)
	j DMAOut
	li s4, %lo(IDCT_MATRIX)
	.endfunc

	.func cmd_mpeg1_idct
cmd_mpeg1_idct:
	li s0, %lo(IDCT_MATRIX)
	lqv $v00,0, 0*16,s0
	lqv $v01,0, 1*16,s0
	lqv $v02,0, 2*16,s0
	lqv $v03,0, 3*16,s0
	lqv $v04,0, 4*16,s0
	lqv $v05,0, 5*16,s0
	lqv $v06,0, 6*16,s0
	lqv $v07,0, 7*16,s0

	setup_vsll $v30

	li s1, %lo(IDCT_CONSTS)
	lqv $v31,0, 0,s1

	# Transform columns
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
#define x1    $v16
#define x2    $v17
#define x3    $v18
#define y3    $v19
#define y4    $v20
#define y5    $v21
#define y6    $v22
#define y7    $v23
#define vzero $v24
#define k473  $v31,e(0)
#define km196 $v31,e(1)
#define k362  $v31,e(2)
#define k196  $v31,e(3)

	vxor vzero, vzero, vzero
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
	vsubc y7, vzero, y7

	vaddc $v00, b7, y4
	vaddc $v01, x4, y3
	vsubc $v02, y5, x0
	vsubc $v03, y6, y7
	vaddc $v04, y6, y7
	vaddc $v05, x0, y5
	vsubc $v06, y3, x4
	vsubc $v07, y4, b7

	sqv $v00,0, 0*16,s0
	sqv $v01,0, 1*16,s0
	sqv $v02,0, 2*16,s0
	sqv $v03,0, 3*16,s0
	sqv $v04,0, 4*16,s0
	sqv $v05,0, 5*16,s0
	sqv $v06,0, 6*16,s0
	sqv $v07,0, 7*16,s0

	j RSPQ_Loop
	nop
	.endfunc


 # v0 = v1+v7+v3+v5+v2+v6+v4+v0
 # v1 = lerp((v1-v7)*473, (v5-v3)*196) - (v1+v7+v3+v5) +
 #       (v2-v6)*362 - (v2+v6) +
 #       v0 - v4
 # v2 = x1-x2 - (x4 - (v1+v7-v3-v5)*362) = 
 #      = v0-v4 - (v2-v6)*362 - 
 #      (lerp((v1-v7)*473, (v5-v3)*196) - (v1+v7+v3+v5) - (v1+v7-v3-v5)*362))
 # v3 = y6 - y7 = x3 - b3 + x0 + (b4*473+b6*196) =
 #       v0+v4 -v2-v6  
 #  
 # v4 = y6 + y7 = x3 - b3 - x0 - (b4*473+b6*196)
#        v0+v4 -v2-v6

