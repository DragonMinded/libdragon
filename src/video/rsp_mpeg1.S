#include "../rspq/rsp_queue.S"
#include "mpeg1_internal.h"

	.section .data.overlay

    RSPQ_OverlayHeader MPEG1_STATE_START, MPEG1_STATE_END, 0x50

COMMAND_TABLE:
	RSPQ_DefineCommand cmd_mpeg1_load_matrix     4  # 0x50
	RSPQ_DefineCommand cmd_mpeg1_store_pixels    4  # 0x51
	RSPQ_DefineCommand cmd_mpeg1_idct            4  # 0x52
	RSPQ_DefineCommand cmd_mpeg1_block_begin     8  # 0x53
	RSPQ_DefineCommand cmd_mpeg1_block_coeff     4  # 0x54
	RSPQ_DefineCommand cmd_mpeg1_block_dequant   4  # 0x55
	RSPQ_DefineCommand cmd_mpeg1_block_decode    8  # 0x56
	RSPQ_DefineCommand cmd_mpeg1_store_matrix    4  # 0x57
	RSPQ_DefineCommand cmd_mpeg1_set_quant_mtx1 36  # 0x58
	RSPQ_DefineCommand cmd_mpeg1_set_quant_mtx2 36  # 0x59
	.dcb.w 16-10

	vsll_data
	vsll8_data

	.align 4
	.ascii "Dragon RSP MPEG1"
	.ascii " Coded by Rasky "

	.align 4
MPEG1_STATE_START:
IDCT_MATRIX:  .dcb.w    8*8       # 8x8 coefficient matrix
BLOCK_PIXELS: .dcb.b    8*8       # 8x8 pixels (current block)

INTER_QUANT_MATRIX:  .dcb.b 64    # 8x8 quantization matrix for inter frames
INTRA_QUANT_MATRIX:  .dcb.b 64    # 8x8 quantization matrix for intra frames

RDRAM_BLOCK:       .long   0         # Current block in RDRAM: Luminance
RDRAM_BLOCK_PITCH: .long   0           # Pitch of the frame in RDRAM (Luminance)
MPEG1_STATE_END:

	.align 4

IDCT_PREMULT:
	#define PMSH  (0)
	.half 32<<PMSH, 44<<PMSH, 42<<PMSH, 38<<PMSH, 32<<PMSH, 25<<PMSH, 17<<PMSH,  9<<PMSH
	.half 44<<PMSH, 62<<PMSH, 58<<PMSH, 52<<PMSH, 44<<PMSH, 35<<PMSH, 24<<PMSH, 12<<PMSH
	.half 42<<PMSH, 58<<PMSH, 55<<PMSH, 49<<PMSH, 42<<PMSH, 33<<PMSH, 23<<PMSH, 12<<PMSH
	.half 38<<PMSH, 52<<PMSH, 49<<PMSH, 44<<PMSH, 38<<PMSH, 30<<PMSH, 20<<PMSH, 10<<PMSH
	.half 32<<PMSH, 44<<PMSH, 42<<PMSH, 38<<PMSH, 32<<PMSH, 25<<PMSH, 17<<PMSH,  9<<PMSH
	.half 25<<PMSH, 35<<PMSH, 33<<PMSH, 30<<PMSH, 25<<PMSH, 20<<PMSH, 14<<PMSH,  7<<PMSH
	.half 17<<PMSH, 24<<PMSH, 23<<PMSH, 20<<PMSH, 17<<PMSH, 14<<PMSH,  9<<PMSH,  5<<PMSH
	.half  9<<PMSH, 12<<PMSH, 12<<PMSH, 10<<PMSH,  9<<PMSH,  7<<PMSH,  5<<PMSH,  2<<PMSH
	#undef PMSH

IDCT_CONSTS:
	.half 473<<5                  # e(0) - IDCT constant
	.half -196<<5                 # e(1) - IDCT constant
	.half 362<<5                  # e(2) - IDCT constant
	.half 196<<5                  # e(3) - IDCT constant
	.half 0x80                    # e(4) - Rounding constant (for IDCT test)
	.half 1<<(7+RSP_IDCT_SCALER)  # e(5) - Residual scale constant
	.half 1<<8                    # e(6) - Residual scale constant
	.half 255                     # e(7) - Residual clamping constant

DEQUANT_CONSTS:
	.half 1
	.half -1
	.half 2

ZIGZAG:
	.byte  0*2,  1*2,  8*2, 16*2,  9*2,  2*2,  3*2, 10*2
	.byte 17*2, 24*2, 32*2, 25*2, 18*2, 11*2,  4*2,  5*2
	.byte 12*2, 19*2, 26*2, 33*2, 40*2, 48*2, 41*2, 34*2
	.byte 27*2, 20*2, 13*2,  6*2,  7*2, 14*2, 21*2, 28*2
	.byte 35*2, 42*2, 49*2, 56*2, 57*2, 50*2, 43*2, 36*2
	.byte 29*2, 22*2, 15*2, 23*2, 30*2, 37*2, 44*2, 51*2
	.byte 58*2, 59*2, 52*2, 45*2, 38*2, 31*2, 39*2, 46*2
	.byte 53*2, 60*2, 61*2, 54*2, 47*2, 55*2, 62*2, 63*2

DCT_COEFF_TABLE:
	.byte   1 << 1; .half      0; .byte        0; .half 0x0001  #   0: x
	.byte   2 << 1; .half      0; .byte   3 << 1; .half      0  #   1: 0x
	.byte   4 << 1; .half      0; .byte   5 << 1; .half      0  #   2: 00x
	.byte   6 << 1; .half      0; .byte        0; .half 0x0101  #   3: 01x
	.byte   7 << 1; .half      0; .byte   8 << 1; .half      0  #   4: 000x
	.byte   9 << 1; .half      0; .byte  10 << 1; .half      0  #   5: 001x
	.byte        0; .half 0x0002; .byte        0; .half 0x0201  #   6: 010x
	.byte  11 << 1; .half      0; .byte  12 << 1; .half      0  #   7: 0000x
	.byte  13 << 1; .half      0; .byte  14 << 1; .half      0  #   8: 0001x
	.byte  15 << 1; .half      0; .byte        0; .half 0x0003  #   9: 0010x
	.byte        0; .half 0x0401; .byte        0; .half 0x0301  #  10: 0011x
	.byte  16 << 1; .half      0; .byte        0; .half 0xffff  #  11: 0000 0x
	.byte  17 << 1; .half      0; .byte  18 << 1; .half      0  #  12: 0000 1x
	.byte        0; .half 0x0701; .byte        0; .half 0x0601  #  13: 0001 0x
	.byte        0; .half 0x0102; .byte        0; .half 0x0501  #  14: 0001 1x
	.byte  19 << 1; .half      0; .byte  20 << 1; .half      0  #  15: 0010 0x
	.byte  21 << 1; .half      0; .byte  22 << 1; .half      0  #  16: 0000 00x
	.byte        0; .half 0x0202; .byte        0; .half 0x0901  #  17: 0000 10x
	.byte        0; .half 0x0004; .byte        0; .half 0x0801  #  18: 0000 11x
	.byte  23 << 1; .half      0; .byte  24 << 1; .half      0  #  19: 0010 00x
	.byte  25 << 1; .half      0; .byte  26 << 1; .half      0  #  20: 0010 01x
	.byte  27 << 1; .half      0; .byte  28 << 1; .half      0  #  21: 0000 000x
	.byte  29 << 1; .half      0; .byte  30 << 1; .half      0  #  22: 0000 001x
	.byte        0; .half 0x0d01; .byte        0; .half 0x0006  #  23: 0010 000x
	.byte        0; .half 0x0c01; .byte        0; .half 0x0b01  #  24: 0010 001x
	.byte        0; .half 0x0302; .byte        0; .half 0x0103  #  25: 0010 010x
	.byte        0; .half 0x0005; .byte        0; .half 0x0a01  #  26: 0010 011x
	.byte  31 << 1; .half      0; .byte  32 << 1; .half      0  #  27: 0000 0000x
	.byte  33 << 1; .half      0; .byte  34 << 1; .half      0  #  28: 0000 0001x
	.byte  35 << 1; .half      0; .byte  36 << 1; .half      0  #  29: 0000 0010x
	.byte  37 << 1; .half      0; .byte  38 << 1; .half      0  #  30: 0000 0011x
	.byte  39 << 1; .half      0; .byte  40 << 1; .half      0  #  31: 0000 0000 0x
	.byte  41 << 1; .half      0; .byte  42 << 1; .half      0  #  32: 0000 0000 1x
	.byte  43 << 1; .half      0; .byte  44 << 1; .half      0  #  33: 0000 0001 0x
	.byte  45 << 1; .half      0; .byte  46 << 1; .half      0  #  34: 0000 0001 1x
	.byte        0; .half 0x1001; .byte        0; .half 0x0502  #  35: 0000 0010 0x
	.byte        0; .half 0x0007; .byte        0; .half 0x0203  #  36: 0000 0010 1x
	.byte        0; .half 0x0104; .byte        0; .half 0x0f01  #  37: 0000 0011 0x
	.byte        0; .half 0x0e01; .byte        0; .half 0x0402  #  38: 0000 0011 1x
	.byte  47 << 1; .half      0; .byte  48 << 1; .half      0  #  39: 0000 0000 00x
	.byte  49 << 1; .half      0; .byte  50 << 1; .half      0  #  40: 0000 0000 01x
	.byte  51 << 1; .half      0; .byte  52 << 1; .half      0  #  41: 0000 0000 10x
	.byte  53 << 1; .half      0; .byte  54 << 1; .half      0  #  42: 0000 0000 11x
	.byte  55 << 1; .half      0; .byte  56 << 1; .half      0  #  43: 0000 0001 00x
	.byte  57 << 1; .half      0; .byte  58 << 1; .half      0  #  44: 0000 0001 01x
	.byte  59 << 1; .half      0; .byte  60 << 1; .half      0  #  45: 0000 0001 10x
	.byte  61 << 1; .half      0; .byte  62 << 1; .half      0  #  46: 0000 0001 11x
	.byte       -1; .half      0; .byte  63 << 1; .half      0  #  47: 0000 0000 000x
	.byte  64 << 1; .half      0; .byte  65 << 1; .half      0  #  48: 0000 0000 001x
	.byte  66 << 1; .half      0; .byte  67 << 1; .half      0  #  49: 0000 0000 010x
	.byte  68 << 1; .half      0; .byte  69 << 1; .half      0  #  50: 0000 0000 011x
	.byte  70 << 1; .half      0; .byte  71 << 1; .half      0  #  51: 0000 0000 100x
	.byte  72 << 1; .half      0; .byte  73 << 1; .half      0  #  52: 0000 0000 101x
	.byte  74 << 1; .half      0; .byte  75 << 1; .half      0  #  53: 0000 0000 110x
	.byte  76 << 1; .half      0; .byte  77 << 1; .half      0  #  54: 0000 0000 111x
	.byte        0; .half 0x000b; .byte        0; .half 0x0802  #  55: 0000 0001 000x
	.byte        0; .half 0x0403; .byte        0; .half 0x000a  #  56: 0000 0001 001x
	.byte        0; .half 0x0204; .byte        0; .half 0x0702  #  57: 0000 0001 010x
	.byte        0; .half 0x1501; .byte        0; .half 0x1401  #  58: 0000 0001 011x
	.byte        0; .half 0x0009; .byte        0; .half 0x1301  #  59: 0000 0001 100x
	.byte        0; .half 0x1201; .byte        0; .half 0x0105  #  60: 0000 0001 101x
	.byte        0; .half 0x0303; .byte        0; .half 0x0008  #  61: 0000 0001 110x
	.byte        0; .half 0x0602; .byte        0; .half 0x1101  #  62: 0000 0001 111x
	.byte  78 << 1; .half      0; .byte  79 << 1; .half      0  #  63: 0000 0000 0001x
	.byte  80 << 1; .half      0; .byte  81 << 1; .half      0  #  64: 0000 0000 0010x
	.byte  82 << 1; .half      0; .byte  83 << 1; .half      0  #  65: 0000 0000 0011x
	.byte  84 << 1; .half      0; .byte  85 << 1; .half      0  #  66: 0000 0000 0100x
	.byte  86 << 1; .half      0; .byte  87 << 1; .half      0  #  67: 0000 0000 0101x
	.byte  88 << 1; .half      0; .byte  89 << 1; .half      0  #  68: 0000 0000 0110x
	.byte  90 << 1; .half      0; .byte  91 << 1; .half      0  #  69: 0000 0000 0111x
	.byte        0; .half 0x0a02; .byte        0; .half 0x0902  #  70: 0000 0000 1000x
	.byte        0; .half 0x0503; .byte        0; .half 0x0304  #  71: 0000 0000 1001x
	.byte        0; .half 0x0205; .byte        0; .half 0x0107  #  72: 0000 0000 1010x
	.byte        0; .half 0x0106; .byte        0; .half 0x000f  #  73: 0000 0000 1011x
	.byte        0; .half 0x000e; .byte        0; .half 0x000d  #  74: 0000 0000 1100x
	.byte        0; .half 0x000c; .byte        0; .half 0x1a01  #  75: 0000 0000 1101x
	.byte        0; .half 0x1901; .byte        0; .half 0x1801  #  76: 0000 0000 1110x
	.byte        0; .half 0x1701; .byte        0; .half 0x1601  #  77: 0000 0000 1111x
	.byte  92 << 1; .half      0; .byte  93 << 1; .half      0  #  78: 0000 0000 0001 0x
	.byte  94 << 1; .half      0; .byte  95 << 1; .half      0  #  79: 0000 0000 0001 1x
	.byte  96 << 1; .half      0; .byte  97 << 1; .half      0  #  80: 0000 0000 0010 0x
	.byte  98 << 1; .half      0; .byte  99 << 1; .half      0  #  81: 0000 0000 0010 1x
	.byte 100 << 1; .half      0; .byte 101 << 1; .half      0  #  82: 0000 0000 0011 0x
	.byte 102 << 1; .half      0; .byte 103 << 1; .half      0  #  83: 0000 0000 0011 1x
	.byte        0; .half 0x001f; .byte        0; .half 0x001e  #  84: 0000 0000 0100 0x
	.byte        0; .half 0x001d; .byte        0; .half 0x001c  #  85: 0000 0000 0100 1x
	.byte        0; .half 0x001b; .byte        0; .half 0x001a  #  86: 0000 0000 0101 0x
	.byte        0; .half 0x0019; .byte        0; .half 0x0018  #  87: 0000 0000 0101 1x
	.byte        0; .half 0x0017; .byte        0; .half 0x0016  #  88: 0000 0000 0110 0x
	.byte        0; .half 0x0015; .byte        0; .half 0x0014  #  89: 0000 0000 0110 1x
	.byte        0; .half 0x0013; .byte        0; .half 0x0012  #  90: 0000 0000 0111 0x
	.byte        0; .half 0x0011; .byte        0; .half 0x0010  #  91: 0000 0000 0111 1x
	.byte 104 << 1; .half      0; .byte 105 << 1; .half      0  #  92: 0000 0000 0001 00x
	.byte 106 << 1; .half      0; .byte 107 << 1; .half      0  #  93: 0000 0000 0001 01x
	.byte 108 << 1; .half      0; .byte 109 << 1; .half      0  #  94: 0000 0000 0001 10x
	.byte 110 << 1; .half      0; .byte 111 << 1; .half      0  #  95: 0000 0000 0001 11x
	.byte        0; .half 0x0028; .byte        0; .half 0x0027  #  96: 0000 0000 0010 00x
	.byte        0; .half 0x0026; .byte        0; .half 0x0025  #  97: 0000 0000 0010 01x
	.byte        0; .half 0x0024; .byte        0; .half 0x0023  #  98: 0000 0000 0010 10x
	.byte        0; .half 0x0022; .byte        0; .half 0x0021  #  99: 0000 0000 0010 11x
	.byte        0; .half 0x0020; .byte        0; .half 0x010e  # 100: 0000 0000 0011 00x
	.byte        0; .half 0x010d; .byte        0; .half 0x010c  # 101: 0000 0000 0011 01x
	.byte        0; .half 0x010b; .byte        0; .half 0x010a  # 102: 0000 0000 0011 10x
	.byte        0; .half 0x0109; .byte        0; .half 0x0108  # 103: 0000 0000 0011 11x
	.byte        0; .half 0x0112; .byte        0; .half 0x0111  # 104: 0000 0000 0001 000x
	.byte        0; .half 0x0110; .byte        0; .half 0x010f  # 105: 0000 0000 0001 001x
	.byte        0; .half 0x0603; .byte        0; .half 0x1002  # 106: 0000 0000 0001 010x
	.byte        0; .half 0x0f02; .byte        0; .half 0x0e02  # 107: 0000 0000 0001 011x
	.byte        0; .half 0x0d02; .byte        0; .half 0x0c02  # 108: 0000 0000 0001 100x
	.byte        0; .half 0x0b02; .byte        0; .half 0x1f01  # 109: 0000 0000 0001 101x
	.byte        0; .half 0x1e01; .byte        0; .half 0x1d01  # 110: 0000 0000 0001 110x
	.byte        0; .half 0x1c01; .byte        0; .half 0x1b01  # 111: 0000 0000 0001 111x

#define BITSTREAM_SIZE  512

BITSTREAM: .dcb.b   BITSTREAM_SIZE
BITSTREAM_END:


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


	.func load_consts
load_consts:
	setup_vsll vshift
	li s1, %lo(IDCT_CONSTS)
	lqv vconst,0, 0,s1
	jr ra
	nop
	.endfunc

	.func cmd_mpeg1_set_quant_mtx2
cmd_mpeg1_set_quant_mtx2:
	# a0: 1=intra, 0=inter
	j cmd_mpeg1_set_quant_mtx
	li s0, 32
	.endfunc

	.func cmd_mpeg1_set_quant_mtx1
cmd_mpeg1_set_quant_mtx1:
	# a0: 1=intra, 0=inter
	li s0, 0

cmd_mpeg1_set_quant_mtx:
	andi a0, 0xFF
	sll a0, 6
	add s0, a0

	lw t0, %lo(RSPQ_DMEM_BUFFER) - 0x04 (rspq_dmem_buf_ptr) 
	lw t1, %lo(RSPQ_DMEM_BUFFER) - 0x08 (rspq_dmem_buf_ptr) 
	lw t2, %lo(RSPQ_DMEM_BUFFER) - 0x0C (rspq_dmem_buf_ptr) 
	lw t3, %lo(RSPQ_DMEM_BUFFER) - 0x10 (rspq_dmem_buf_ptr) 
	lw t4, %lo(RSPQ_DMEM_BUFFER) - 0x14 (rspq_dmem_buf_ptr) 

	sw a1, %lo(INTER_QUANT_MATRIX) + 0x00 (s0)
	sw a2, %lo(INTER_QUANT_MATRIX) + 0x04 (s0)
	sw a3, %lo(INTER_QUANT_MATRIX) + 0x08 (s0)
	sw t0, %lo(INTER_QUANT_MATRIX) + 0x0C (s0)
	sw t1, %lo(INTER_QUANT_MATRIX) + 0x10 (s0)
	sw t2, %lo(INTER_QUANT_MATRIX) + 0x14 (s0)
	sw t3, %lo(INTER_QUANT_MATRIX) + 0x18 (s0)
	sw t4, %lo(INTER_QUANT_MATRIX) + 0x1C (s0)

	jr ra
	nop
	.endfunc

	.func cmd_mpeg1_block_begin
cmd_mpeg1_block_begin:
	# a0: block address in RDRAM
	# a1: block pitch in RDRAM

	vxor $v00, $v00, $v00

	# Store RDRAM block address and pitch
	li s0, %lo(RDRAM_BLOCK)
	sw a0, 0(s0)
	sw a1, 4(s0)

	# Clear coefficient matrix
	li s1, %lo(IDCT_MATRIX)
	sqv $v00,0, 0*16,s1
	sqv $v00,0, 1*16,s1
	sqv $v00,0, 2*16,s1
	sqv $v00,0, 3*16,s1
	sqv $v00,0, 4*16,s1
	sqv $v00,0, 5*16,s1
	sqv $v00,0, 6*16,s1
	sqv $v00,0, 7*16,s1

	jr ra
	nop
	.endfunc

	.func cmd_mpeg1_block_coeff
cmd_mpeg1_block_coeff:
	# a0: (index << 16) | level
	#define index  t4
	#define level  t5

	# Extract level and index from input
	andi level, a0, 0xFFFF
	srl index, a0, 16
	andi index, 0x3F
	
	# Apply zigzag to get memory index into matrix
	lbu index, %lo(ZIGZAG)(index)

	# Store coefficient into matrix
	sh level, %lo(IDCT_MATRIX)(index)

	jr ra
	nop
	#undef index
	#undef level
	.endfunc


	.func cmd_mpeg1_block_dequant
cmd_mpeg1_block_dequant:
	# a0: (intra) | (quant_scale << 8) 
	#define intra       a0
	#define quant_scale t8
	#define loop_idx    t4
	#define dc          t7
	#define vshift8     $v30
	#define v_scale     $v08
	#define v_const2    $v31
	#define kp1         v_const2,e(0)
	#define km1         v_const2,e(1)
	#define kp2         v_const2,e(2)

	setup_vsll8 vshift8
	li s0, %lo(DEQUANT_CONSTS)
	lqv v_const2,0, 0,s0

	andi quant_scale, a0, 0xFF00
	mtc2 quant_scale, v_scale,0
	andi intra, a0, 0xFF

	li s0, %lo(IDCT_MATRIX)
	li s1, %lo(INTER_QUANT_MATRIX)
	li s2, %lo(IDCT_PREMULT)
	sll t0, intra, 6
	add s1, t0

	lhu dc, 0(s0)

	li loop_idx, 7
dequant_loop:
	lqv $v00,0, 0,s0
	lpv $v01,0, 0,s1
	lqv $v02,0, 0,s2

	bnez intra, dequant
	vmudh $v00, $v00, kp2

	vrndn16 $v00, km1
	vrndp16 $v00, kp1
dequant:
	# Scale the quantization matrix coefficient
	# by the quantization scale.
	vmudl $v01, $v01, v_scale,e(0)
	# Inverse quantization with clamping and oddification.
	# The final result is <<4.
	vmulq $v00, $v00, $v01
	vmacq $v00
	vsra $v00, $v00, 4
	# Apply pre-multiplier. The final result doesn't fit in 16-bit
	# so we need to apply a scaling by RSP_IDCT_SCALER, by fetching
	# the high part from the accumulator and doing a 32-bit shift
	vmudn $v00, $v02, $v00
	vsar $v03, $v03, $v03,e(1)
	vsrl $v00, $v00, RSP_IDCT_SCALER
	vsll8 $v03, $v03, 16-RSP_IDCT_SCALER
	vor $v00, $v00, $v03

	sqv $v00,0, 0,s0
	addi s0, 16
	addi s1, 8
	addi s2, 16
	bnez loop_idx, dequant_loop
	addi loop_idx, -1

	# Restore initial DC coefficient	
	li s0, %lo(IDCT_MATRIX)
	sh dc, 0(s0)

	jr ra
	nop

	#undef vshift8
	#undef intra
	#undef v_const2
	#undef kp1
	#undef km1
	.endfunc


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

	.func cmd_mpeg1_store_pixels
cmd_mpeg1_store_pixels:
	move s0, a0
	li t0, DMA_SIZE(8*8, 1)
	j DMAOut
	li s4, %lo(BLOCK_PIXELS)
	.endfunc

	.func load_matrix
load_matrix:
	li s0, %lo(IDCT_MATRIX)
	lqv $v00,0, 0*16,s0
	lqv $v01,0, 1*16,s0
	lqv $v02,0, 2*16,s0
	lqv $v03,0, 3*16,s0
	lqv $v04,0, 4*16,s0
	lqv $v05,0, 5*16,s0
	lqv $v06,0, 6*16,s0
	jr ra
	lqv $v07,0, 7*16,s0
	.endfunc

	.func idct
idct:
	move ra2, ra

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
	jal load_matrix
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

	.func cmd_mpeg1_block_decode
cmd_mpeg1_block_decode:
	# a0 = ncoeffs
	# a1 = 1=intra, 0=inter
	jal load_consts
	nop
	jal load_matrix
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
	li s0, %lo(IDCT_MATRIX)
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
