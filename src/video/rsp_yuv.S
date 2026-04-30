############################################################
# rsp_yuv.S
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
############################################################
#include <rsp_queue.inc>
#include "yuv_internal.h"

    .data

    RSPQ_BeginOverlayHeader
	RSPQ_DefineCommand cmd_yuv_set_input                     16
	RSPQ_DefineCommand cmd_yuv_set_output                     8
	RSPQ_DefineCommand cmd_yuv_interleave4_block_32x16,       4
	RSPQ_DefineCommand cmd_yuv_interleave2_block_32x16,       4
	RSPQ_DefineCommand cmd_yuv_interleave2_block_32x16_422,   4
	RSPQ_DefineCommand cmd_yuv_interleave2_nv16_block_32x16,  4
	RSPQ_DefineCommand cmd_yuv_copy_block_32x16,              4
	RSPQ_EndOverlayHeader

	.align 4
	.ascii " Dragon RSP YUV "
	.ascii " Coded by Rasky "

	RSPQ_BeginSavedState
RDRAM_YBUF_STRIDE:   .half 0
RDRAM_CBUF_STRIDE:   .half 0
RDRAM_OUTBUF_STRIDE: .half 0
RDRAM_YBUF: 	     .long 0
RDRAM_CBBUF:         .long 0
RDRAM_CRBUF:         .long 0
RDRAM_OUTBUF: 	     .long 0
	RSPQ_EndSavedState

	# FIXME?
	#.section bss.overlay

    .align 4

V1TEMP: .quad 0,0,0,0
V2TEMP: .quad 0,0,0,0

    .align 3

YBUF:   .ds.b  BLOCK_W * BLOCK_H
# CBBUF/CRBUF are sized for the 4:2:2 case (full BLOCK_H rows of chroma).
# The 4:2:0 commands only consume the first half of each buffer.
CBBUF:  .ds.b  (BLOCK_W/2) * BLOCK_H
CRBUF:  .ds.b  (BLOCK_W/2) * BLOCK_H
OUTBUF: .ds.b  BLOCK_W * BLOCK_H * 2

	.text

	.func cmd_yuv_set_input
cmd_yuv_set_input:
	sw a0, %lo(RDRAM_YBUF)
	sw a1, %lo(RDRAM_CBBUF)
	sw a2, %lo(RDRAM_CRBUF)
	sh a3, %lo(RDRAM_YBUF_STRIDE)
	srl a3, 1
	jr ra
	sh a3, %lo(RDRAM_CBUF_STRIDE)
	.endfunc

	.func cmd_yuv_set_output
cmd_yuv_set_output:
	sw a0, %lo(RDRAM_OUTBUF)
	jr ra
	sh a1, %lo(RDRAM_OUTBUF_STRIDE)
	.endfunc



	.func cmd_yuv_interleave4_block_32x16
cmd_yuv_interleave4_block_32x16:
	#define x0y0       a0
	#define ybuf_off   t5
	#define cbuf_off   t6
	#define outbuf_off t7
    #define y          t8

    #define ybuf       s0
    #define cbbuf      s1
    #define crbuf      s2
    #define outbuf     s3

	# Calculate y0*stride+x0 for both input and output buffers
	li s0, %lo(RDRAM_YBUF_STRIDE)
	lqv $v01,0, 0,s0

	andi t0, x0y0, 0xFFF
	mtc2 t0, $v00,0*2  # ybuf => y
	mtc2 t0, $v00,2*2  # outbuf => y
	srl t0, 1
	mtc2 t0, $v00,1*2  # cbuf => y/2

	vmudn $v00, $v01, $v00
	srl t3, x0y0, 12
	andi t3, 0xFFF

	mfc2 ybuf_off, $v00,0*2
	mfc2 cbuf_off, $v00,1*2
	mfc2 outbuf_off, $v00,2*2    

    vsar $v01, $v01, $v01,9
	mfc2 t0, $v01,0*2
	mfc2 t1, $v01,1*2
	mfc2 t2, $v01,2*2

	andi ybuf_off, 0xFFFF
	andi cbuf_off, 0xFFFF
	andi outbuf_off, 0xFFFF

	sll t0, 16
	sll t1, 16
	sll t2, 16

	add ybuf_off, t0
	add cbuf_off, t1
	add outbuf_off, t2

	add ybuf_off, t3
	sll t3, 1
	add outbuf_off, t3
	srl t3, 2
	add cbuf_off, t3


	# Fetch Y plane
	lh t1, %lo(RDRAM_YBUF_STRIDE)
	lw s0, %lo(RDRAM_YBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_Y
	add s0, ybuf_off
	li s4, %lo(YBUF)
	jal DMAInAsync
	li t0, DMA_SIZE(BLOCK_W, BLOCK_H)

	# Fetch CB plane
	lh t1, %lo(RDRAM_CBUF_STRIDE)
	lw s0, %lo(RDRAM_CBBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CB
	add s0, cbuf_off
	li s4, %lo(CBBUF)
	jal DMAInAsync
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H/2)

	# Fetch CR plane
	lw s0, %lo(RDRAM_CRBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CR
	add s0, cbuf_off
	li s4, %lo(CRBUF)
	jal DMAIn
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H/2)

    li y, BLOCK_H/2-1
    li t1, %lo(V1TEMP)
    li t2, %lo(V2TEMP)
    li ybuf, %lo(YBUF)
    li cbbuf, %lo(CBBUF)
    li crbuf, %lo(CRBUF)
    li outbuf, %lo(OUTBUF)

#if BLOCK_W != 32
    break
#endif

    # Interleave 2 lines of 32 horizontal pixels each loop. The code
    # has been laid out to take maximum advantage of parallel VU/SU execution
    # of opcodes, taking also into account instruction latency to avoid
    # pipeline stalls (4 ops for memory accesses and 3 ops for each
    # vector instruction).
    #
    # This optimized loop takes 34 cycles to interleave 64 pixels.
    .align 3 
    InterleaveLoopRow2:
                                            # Read CB/CR elements
                                            lpv $v15,0,  0*8,crbuf # CR<<8 (first half)           # 1
                                            lpv $v17,0,  1*8,crbuf # CR<<8 (second half)          # 2
                                            lpv $v14,0,  0*8,cbbuf # CB<<8 (first half)           # 3
                                            lpv $v16,0,  1*8,cbbuf # CB<<8 (second half)          # 4

        # Calculate (CB<<8)|CR              # Read Y buffer elements
        vsrl $v15, $v15, 8;                 lpv $v00,0,  0*8,ybuf  # row 0, first quarter         # 5
        vsrl $v17, $v17, 8;                 lpv $v01,0,  1*8,ybuf  # row 0, second quarter        # 6
        
                                            lpv $v08,0,  4*8,ybuf  # row 1, first quarter         # 7
        vor $v15, $v15, $v14,0;             lpv $v09,0,  5*8,ybuf  # row 1, second quarter        # 8
        vor $v17, $v17, $v16,0;             lpv $v02,0,  2*8,ybuf  # row 0, third quarter         # 9
                                            lpv $v03,0,  3*8,ybuf  # row 0, fourth quarter        # 10

        # Shift Y buffer elements right     # Store interleaved CB/CR into t1/t2
        # to go into the lower byte of      # and finish reading Y buffer
        # each lane of vregs.
        vsrl $v00, $v00, 8;                 sqv $v15,0,  0*16,t1                                   # 11,12 (stall: store 2 cycles after load)
        vsrl $v01, $v01, 8;                 sqv $v17,0,  0*16,t2                                   # 13
        vsrl $v08, $v08, 8;                 lpv $v10,0,  6*8,ybuf  # row 1, third quarter         # 14
        vsrl $v09, $v09, 8;                 lpv $v11,0,  7*8,ybuf  # row 1, fourth quarter        # 15

                                            # Read back CB/CR interleaved values
        vsrl $v02, $v02, 8;                 lpv $v14,0,  0*8,t1                                   # 16
        vsrl $v03, $v03, 8;                 lpv $v15,0,  1*8,t1                                   # 17
        vsrl $v10, $v10, 8;                 lpv $v16,0,  0*8,t2                                   # 18
        vsrl $v11, $v11, 8;                 lpv $v17,0,  1*8,t2                                   # 19

        # Compose Y with CB/Cr              # Prepare pointers for next loop
        vor $v00, $v00, $v14,0;             addi crbuf, 8*2                                     # 20
        vor $v01, $v01, $v15,0;             addi cbbuf, 8*2                                     # 21
        vor $v08, $v08, $v14,0;             addi ybuf, 32*2                                     # 22
        vor $v09, $v09, $v15,0;             addi outbuf, 64*2                                   # 23

                                            # Write interleaved pixels in output
                                            # buffer.
        vor $v02, $v02, $v16,0;             sqv $v00,0,  -8*16,outbuf                              # 24
        vor $v03, $v03, $v17,0;             sqv $v01,0,  -7*16,outbuf                              # 25
        vor $v10, $v10, $v16,0;             sqv $v08,0,  -4*16,outbuf                              # 26
        vor $v11, $v11, $v17,0;             sqv $v09,0,  -3*16,outbuf                              # 27

                                            sqv $v02,0,  -6*16,outbuf                              # 28
                                            sqv $v03,0,  -5*16,outbuf                              # 29
                                            sqv $v10,0,  -2*16,outbuf                              # 30
                                            sqv $v11,0,  -1*16,outbuf                              # 31

        bnez y, InterleaveLoopRow2                                                              # 32
        addi y, -1                                                                              # 33,34 (stall: taken branch)


    # DMA output buffer
	lh t1, %lo(RDRAM_OUTBUF_STRIDE)
	lw s0, %lo(RDRAM_OUTBUF)
	assert_ne s0, 0, ASSERT_INVALID_OUTPUT
	add s0, outbuf_off
	li s4, %lo(OUTBUF)
	li t0, DMA_SIZE(BLOCK_W*2, BLOCK_H)
	jal_and_j DMAOut, RSPQ_Loop

	#undef x0y0      
	#undef ybuf_off  
	#undef cbuf_off  
	#undef outbuf_off
    #undef y         

    #undef ybuf      
    #undef cbbuf     
    #undef crbuf     
    #undef outbuf    
    #undef outbuf2   
	.endfunc

	.func cmd_yuv_interleave2_block_32x16
cmd_yuv_interleave2_block_32x16:
	#define x0y0       a0
	#define ybuf_off   t5
	#define cbuf_off   t6
	#define outbuf_off t7
    #define y          t8

    #define ybuf       s0
    #define cbbuf      s1
    #define crbuf      s2
    #define outbuf     s3
    #define outbuf2    s4
	# Calculate y0*stride+x0 for both input and output buffers
	li s0, %lo(RDRAM_YBUF_STRIDE)
	lqv $v01,0, 0,s0

	andi t0, x0y0, 0xFFF
	mtc2 t0, $v00,0*2  # ybuf => y
	srl t0, 1
	mtc2 t0, $v00,2*2  # outbuf => y/2
	mtc2 t0, $v00,1*2  # cbuf => y/2

	vmudn $v00, $v01, $v00
	srl t3, x0y0, 12
	andi t3, 0xFFF

	mfc2 ybuf_off, $v00,0*2
	mfc2 cbuf_off, $v00,1*2
	mfc2 outbuf_off, $v00,2*2    

    vsar $v01, $v01, $v01,9
	mfc2 t0, $v01,0*2
	mfc2 t1, $v01,1*2
	mfc2 t2, $v01,2*2

	andi ybuf_off, 0xFFFF
	andi cbuf_off, 0xFFFF
	andi outbuf_off, 0xFFFF

	sll t0, 16
	sll t1, 16
	sll t2, 16

	add ybuf_off, t0
	add cbuf_off, t1
	add outbuf_off, t2

	add ybuf_off, t3
	add outbuf_off, t3
	srl t3, 1
	add cbuf_off, t3


	# Fetch CB plane
	lh t1, %lo(RDRAM_CBUF_STRIDE)
	lw s0, %lo(RDRAM_CBBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CB
	add s0, cbuf_off
	li s4, %lo(CBBUF)
	jal DMAInAsync
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H/2)

	# Fetch CR plane
	lw s0, %lo(RDRAM_CRBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CR
	add s0, cbuf_off
	li s4, %lo(CRBUF)
	jal DMAIn
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H/2)

    li y, BLOCK_H/2-1
    li t1, %lo(V1TEMP)
    li t2, %lo(V2TEMP)
    li cbbuf, %lo(CBBUF)
    li crbuf, %lo(CRBUF)
    li outbuf, %lo(OUTBUF)
    addi outbuf2, outbuf, 1

#if BLOCK_W != 32
    break
#endif

1:
	luv $v00,0, 0*8,cbbuf
	luv $v02,0, 1*8,cbbuf
	luv $v04,0, 2*8,cbbuf
	luv $v06,0, 3*8,cbbuf

	luv $v01,0, 0*8,crbuf
	luv $v03,0, 1*8,crbuf
	luv $v05,0, 2*8,crbuf
	luv $v07,0, 3*8,crbuf

	shv $v00,0, 0*16,outbuf
	shv $v01,0, 0*16,outbuf2
	shv $v02,0, 1*16,outbuf
	shv $v03,0, 1*16,outbuf2
	shv $v04,0, 2*16,outbuf
	shv $v05,0, 2*16,outbuf2
	shv $v06,0, 3*16,outbuf
	shv $v07,0, 3*16,outbuf2

	addi cbbuf, 32
	addi crbuf, 32
	addi outbuf, 64
	addi outbuf2, 64

	bgtz y, 1b
	addi y, -2

    # DMA output buffer
	lh t1, %lo(RDRAM_OUTBUF_STRIDE)
	lw s0, %lo(RDRAM_OUTBUF)
	assert_ne s0, 0, ASSERT_INVALID_OUTPUT
	add s0, outbuf_off
	li s4, %lo(OUTBUF)
	li t0, DMA_SIZE(BLOCK_W, BLOCK_H/2)
	jal_and_j DMAOut, RSPQ_Loop

	#undef x0y0
	#undef ybuf_off
	#undef cbuf_off
	#undef outbuf_off
    #undef y

    #undef ybuf
    #undef cbbuf
    #undef crbuf
    #undef outbuf
    #undef outbuf2
	.endfunc

	# 4:2:2 sibling of cmd_yuv_interleave2_block_32x16. Reads BLOCK_H rows
	# of CB and CR (instead of BLOCK_H/2) and writes BLOCK_H rows of
	# interleaved IA16 chroma (instead of BLOCK_H/2). The output stride
	# (RDRAM_OUTBUF_STRIDE) is the chroma row stride and is identical to
	# the 4:2:0 case; only the row count and the row index used for the
	# chroma/output offsets differ (full y0 instead of y0/2).
	.func cmd_yuv_interleave2_block_32x16_422
cmd_yuv_interleave2_block_32x16_422:
	#define x0y0       a0
	#define cbuf_off   t6
	#define outbuf_off t7
    #define y          t8

    #define cbbuf      s1
    #define crbuf      s2
    #define outbuf     s3
    #define outbuf2    s4
	# Calculate y0*stride+x0 for chroma input and output buffers. For 4:2:2
	# chroma is full-height, so the row index is y0 (not y0/2) for both.
	li s0, %lo(RDRAM_YBUF_STRIDE)
	lqv $v01,0, 0,s0

	andi t0, x0y0, 0xFFF
	mtc2 t0, $v00,2*2  # outbuf => y0
	mtc2 t0, $v00,1*2  # cbuf   => y0

	vmudn $v00, $v01, $v00
	srl t3, x0y0, 12
	andi t3, 0xFFF

	mfc2 cbuf_off, $v00,1*2
	mfc2 outbuf_off, $v00,2*2

    vsar $v01, $v01, $v01,9
	mfc2 t1, $v01,1*2
	mfc2 t2, $v01,2*2

	andi cbuf_off, 0xFFFF
	andi outbuf_off, 0xFFFF

	sll t1, 16
	sll t2, 16

	add cbuf_off, t1
	add outbuf_off, t2

	add outbuf_off, t3
	srl t3, 1
	add cbuf_off, t3


	# Fetch CB plane (full BLOCK_H rows for 4:2:2)
	lh t1, %lo(RDRAM_CBUF_STRIDE)
	lw s0, %lo(RDRAM_CBBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CB
	add s0, cbuf_off
	li s4, %lo(CBBUF)
	jal DMAInAsync
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H)

	# Fetch CR plane (full BLOCK_H rows for 4:2:2)
	lw s0, %lo(RDRAM_CRBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CR
	add s0, cbuf_off
	li s4, %lo(CRBUF)
	jal DMAIn
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H)

    li y, BLOCK_H-1
    li t1, %lo(V1TEMP)
    li t2, %lo(V2TEMP)
    li cbbuf, %lo(CBBUF)
    li crbuf, %lo(CRBUF)
    li outbuf, %lo(OUTBUF)
    addi outbuf2, outbuf, 1

#if BLOCK_W != 32
    break
#endif

1:
	luv $v00,0, 0*8,cbbuf
	luv $v02,0, 1*8,cbbuf
	luv $v04,0, 2*8,cbbuf
	luv $v06,0, 3*8,cbbuf

	luv $v01,0, 0*8,crbuf
	luv $v03,0, 1*8,crbuf
	luv $v05,0, 2*8,crbuf
	luv $v07,0, 3*8,crbuf

	shv $v00,0, 0*16,outbuf
	shv $v01,0, 0*16,outbuf2
	shv $v02,0, 1*16,outbuf
	shv $v03,0, 1*16,outbuf2
	shv $v04,0, 2*16,outbuf
	shv $v05,0, 2*16,outbuf2
	shv $v06,0, 3*16,outbuf
	shv $v07,0, 3*16,outbuf2

	addi cbbuf, 32
	addi crbuf, 32
	addi outbuf, 64
	addi outbuf2, 64

	bgtz y, 1b
	addi y, -2

    # DMA output buffer (full BLOCK_H rows for 4:2:2)
	lh t1, %lo(RDRAM_OUTBUF_STRIDE)
	lw s0, %lo(RDRAM_OUTBUF)
	assert_ne s0, 0, ASSERT_INVALID_OUTPUT
	add s0, outbuf_off
	li s4, %lo(OUTBUF)
	li t0, DMA_SIZE(BLOCK_W, BLOCK_H)
	jal_and_j DMAOut, RSPQ_Loop

	#undef x0y0
	#undef cbuf_off
	#undef outbuf_off
    #undef y
    #undef cbbuf
    #undef crbuf
    #undef outbuf
    #undef outbuf2
	.endfunc

	# 2D RDRAM->RDRAM tiled copy of a single 32x16 byte plane (e.g. luma).
	# Source plane base/stride live in RDRAM_YBUF/RDRAM_YBUF_STRIDE (set by
	# cmd_yuv_set_input); destination plane base/stride in RDRAM_OUTBUF/
	# RDRAM_OUTBUF_STRIDE (set by cmd_yuv_set_output). Both 1 byte per pixel.
	.func cmd_yuv_copy_block_32x16
cmd_yuv_copy_block_32x16:
	#define x0y0       a0
	#define ybuf_off   t5
	#define outbuf_off t7

	li s0, %lo(RDRAM_YBUF_STRIDE)
	lqv $v01,0, 0,s0

	andi t0, x0y0, 0xFFF
	mtc2 t0, $v00,0*2  # ybuf   => y
	mtc2 t0, $v00,2*2  # outbuf => y

	vmudn $v00, $v01, $v00
	srl t3, x0y0, 12
	andi t3, 0xFFF

	mfc2 ybuf_off, $v00,0*2
	mfc2 outbuf_off, $v00,2*2

	vsar $v01, $v01, $v01,9
	mfc2 t0, $v01,0*2
	mfc2 t2, $v01,2*2

	andi ybuf_off, 0xFFFF
	andi outbuf_off, 0xFFFF

	sll t0, 16
	sll t2, 16

	add ybuf_off, t0
	add outbuf_off, t2

	# 1 byte per pixel for both input and output.
	add ybuf_off, t3
	add outbuf_off, t3

	# Fetch source tile (32x16 bytes) into YBUF
	lh t1, %lo(RDRAM_YBUF_STRIDE)
	lw s0, %lo(RDRAM_YBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_Y
	add s0, ybuf_off
	li s4, %lo(YBUF)
	jal DMAIn
	li t0, DMA_SIZE(BLOCK_W, BLOCK_H)

	# DMA out to destination
	lh t1, %lo(RDRAM_OUTBUF_STRIDE)
	lw s0, %lo(RDRAM_OUTBUF)
	assert_ne s0, 0, ASSERT_INVALID_OUTPUT
	add s0, outbuf_off
	li s4, %lo(YBUF)
	li t0, DMA_SIZE(BLOCK_W, BLOCK_H)
	jal_and_j DMAOut, RSPQ_Loop

	#undef x0y0
	#undef ybuf_off
	#undef outbuf_off
	.endfunc

	# Hybrid sibling of cmd_yuv_interleave2_block_32x16 used for converting
	# I420 (half-height chroma) into NV16 layout (full luma-height chroma).
	# Reads BLOCK_H/2 rows of CB/CR (4:2:0 source) and writes BLOCK_H rows
	# of interleaved IA16 chroma (each input row duplicated to two output
	# rows). The output offset uses full y0 (4:2:2 destination); chroma
	# input offset uses y0/2 (4:2:0 source).
	.func cmd_yuv_interleave2_nv16_block_32x16
cmd_yuv_interleave2_nv16_block_32x16:
	#define x0y0       a0
	#define cbuf_off   t6
	#define outbuf_off t7
    #define y          t8

    #define cbbuf      s1
    #define crbuf      s2
    #define outbuf     s3
    #define outbuf2    s4
	# Compute offsets: chroma index = y0/2 (half-height source); output
	# index = y0 (full-height destination).
	li s0, %lo(RDRAM_YBUF_STRIDE)
	lqv $v01,0, 0,s0

	andi t0, x0y0, 0xFFF
	mtc2 t0, $v00,2*2  # outbuf => y0
	srl t0, 1
	mtc2 t0, $v00,1*2  # cbuf   => y0/2

	vmudn $v00, $v01, $v00
	srl t3, x0y0, 12
	andi t3, 0xFFF

	mfc2 cbuf_off, $v00,1*2
	mfc2 outbuf_off, $v00,2*2

    vsar $v01, $v01, $v01,9
	mfc2 t1, $v01,1*2
	mfc2 t2, $v01,2*2

	andi cbuf_off, 0xFFFF
	andi outbuf_off, 0xFFFF

	sll t1, 16
	sll t2, 16

	add cbuf_off, t1
	add outbuf_off, t2

	add outbuf_off, t3
	srl t3, 1
	add cbuf_off, t3


	# Fetch CB plane (BLOCK_H/2 rows; same as 4:2:0 NV12)
	lh t1, %lo(RDRAM_CBUF_STRIDE)
	lw s0, %lo(RDRAM_CBBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CB
	add s0, cbuf_off
	li s4, %lo(CBBUF)
	jal DMAInAsync
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H/2)

	# Fetch CR plane (BLOCK_H/2 rows; same as 4:2:0 NV12)
	lw s0, %lo(RDRAM_CRBUF)
	assert_ne s0, 0, ASSERT_INVALID_INPUT_CR
	add s0, cbuf_off
	li s4, %lo(CRBUF)
	jal DMAIn
	li t0, DMA_SIZE(BLOCK_W/2, BLOCK_H/2)

    li y, BLOCK_H/2-1
    li t1, %lo(V1TEMP)
    li t2, %lo(V2TEMP)
    li cbbuf, %lo(CBBUF)
    li crbuf, %lo(CRBUF)
    li outbuf, %lo(OUTBUF)
    addi outbuf2, outbuf, 1

#if BLOCK_W != 32
    break
#endif

	# Each loop iteration consumes 2 chroma rows (16 px each) and writes
	# 4 output rows of 32 bytes (each input row duplicated to 2 output
	# rows for the NV16 vertical chroma upsample).
1:
	luv $v00,0, 0*8,cbbuf      # CB row 0, first half (8 px)
	luv $v02,0, 1*8,cbbuf      # CB row 0, second half
	luv $v04,0, 2*8,cbbuf      # CB row 1, first half
	luv $v06,0, 3*8,cbbuf      # CB row 1, second half

	luv $v01,0, 0*8,crbuf
	luv $v03,0, 1*8,crbuf
	luv $v05,0, 2*8,crbuf
	luv $v07,0, 3*8,crbuf

	# Output rows 0,1 from input row 0 (v00/v01 + v02/v03)
	shv $v00,0, 0*16,outbuf
	shv $v01,0, 0*16,outbuf2
	shv $v02,0, 1*16,outbuf
	shv $v03,0, 1*16,outbuf2
	shv $v00,0, 2*16,outbuf       # duplicate of row 0
	shv $v01,0, 2*16,outbuf2
	shv $v02,0, 3*16,outbuf
	shv $v03,0, 3*16,outbuf2

	# Output rows 2,3 from input row 1 (v04/v05 + v06/v07)
	shv $v04,0, 4*16,outbuf
	shv $v05,0, 4*16,outbuf2
	shv $v06,0, 5*16,outbuf
	shv $v07,0, 5*16,outbuf2
	shv $v04,0, 6*16,outbuf       # duplicate of row 2
	shv $v05,0, 6*16,outbuf2
	shv $v06,0, 7*16,outbuf
	shv $v07,0, 7*16,outbuf2

	addi cbbuf, 32
	addi crbuf, 32
	addi outbuf, 128
	addi outbuf2, 128

	bgtz y, 1b
	addi y, -2

    # DMA output buffer (full BLOCK_H rows for NV16)
	lh t1, %lo(RDRAM_OUTBUF_STRIDE)
	lw s0, %lo(RDRAM_OUTBUF)
	assert_ne s0, 0, ASSERT_INVALID_OUTPUT
	add s0, outbuf_off
	li s4, %lo(OUTBUF)
	li t0, DMA_SIZE(BLOCK_W, BLOCK_H)
	jal_and_j DMAOut, RSPQ_Loop

	#undef x0y0
	#undef cbuf_off
	#undef outbuf_off
    #undef y
    #undef cbbuf
    #undef crbuf
    #undef outbuf
    #undef outbuf2
	.endfunc

