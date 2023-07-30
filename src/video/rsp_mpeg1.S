#include <rsp_queue.inc>
#include "mpeg1_internal.h"

    .data

    RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand cmd_mpeg1_load_matrix     4  # 0x50
    RSPQ_DefineCommand cmd_mpeg1_store_pixels    4  # 0x51
    RSPQ_DefineCommand cmd_mpeg1_idct            4  # 0x52
    RSPQ_DefineCommand cmd_mpeg1_block_begin    12  # 0x53
    RSPQ_DefineCommand cmd_mpeg1_block_coeff     4  # 0x54
    RSPQ_DefineCommand cmd_mpeg1_block_dequant   4  # 0x55
    RSPQ_DefineCommand cmd_mpeg1_block_decode    8  # 0x56
    RSPQ_DefineCommand cmd_mpeg1_store_matrix    4  # 0x57
    RSPQ_DefineCommand cmd_mpeg1_set_quant_mtx1 36  # 0x58
    RSPQ_DefineCommand cmd_mpeg1_set_quant_mtx2 36  # 0x59
    RSPQ_DefineCommand cmd_mpeg1_block_predict  12  # 0x5A
    RSPQ_DefineCommand cmd_mpeg1_block_switch    4  # 0x5B
    RSPQ_DefineCommand cmd_mpeg1_load_pixels     4  # 0x5C
    RSPQ_DefineCommand cmd_mpeg1_zero_pixels     4  # 0x5D
    .dcb.w 16-14
    RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RSP MPEG1"
    .ascii " Coded by Rasky "

    .align 4
    RSPQ_BeginSavedState
IDCT_MATRIX:  .dcb.w    8*8       # 8x8 coefficient matrix
COEFF_MASK:   .dcb.b    8

INTER_QUANT_MATRIX:  .dcb.b 64    # 8x8 quantization matrix for inter frames
INTRA_QUANT_MATRIX:  .dcb.b 64    # 8x8 quantization matrix for intra frames

RDRAM_BLOCK:       .long   0         # Current block in RDRAM
RDRAM_BLOCK_SIZE:  .long   0         # Current block size in RDRAM (DMA_SIZE format)
RDRAM_BLOCK_PITCH: .long   0         # Pitch of the block/frame in RDRAM

RDRAM_BLOCKS:      .long   0,0,0,0,0,0
RDRAM_PITCHS:      .long   0,0,0,0,0,0
CUR_PIXELS:        .long   0

    .align 4
PIXELS:            .dcb.b    (16*16 + 8*8 + 8*8)
PIXELCHECK:        .long   0xBADC0DE
    RSPQ_EndSavedState

PIXELS_OFFSET:     .half    0, 8, 16*8, 16*8+8, 16*16, 16*16+8*8

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
    .half 0
    .half 16
    .half -16
    .half 8
    .half 0xFFE1

ZIGZAG:
    .byte  0*2,  1*2,  8*2, 16*2,  9*2,  2*2,  3*2, 10*2
    .byte 17*2, 24*2, 32*2, 25*2, 18*2, 11*2,  4*2,  5*2
    .byte 12*2, 19*2, 26*2, 33*2, 40*2, 48*2, 41*2, 34*2
    .byte 27*2, 20*2, 13*2,  6*2,  7*2, 14*2, 21*2, 28*2
    .byte 35*2, 42*2, 49*2, 56*2, 57*2, 50*2, 43*2, 36*2
    .byte 29*2, 22*2, 15*2, 23*2, 30*2, 37*2, 44*2, 51*2
    .byte 58*2, 59*2, 52*2, 45*2, 38*2, 31*2, 39*2, 46*2
    .byte 53*2, 60*2, 61*2, 54*2, 47*2, 55*2, 62*2, 63*2




    .align 3
SOURCE_PIXELS: .dcb.b 24*16

    .text 1

#define pred0  $v21
#define pred1  $v22
#define pred2  $v23
#define pred3  $v24
#define pred4  $v25
#define pred5  $v26
#define pred6  $v27
#define pred7  $v28
#define vconst  $v29
#define k473   vconst,e(0)
#define km196  vconst,e(1)
#define k362   vconst,e(2)
#define k196   vconst,e(3)
#define k128   vconst,e(4)
#define k1u    vconst,e(5)
#define k2     vconst,e(6)
#define k255   vconst,e(7)

    .func load_idct_consts
load_idct_consts:
    li s1, %lo(IDCT_CONSTS)
    jr ra
    lqv vconst,0, 0,s1
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
    sw t4, %lo(INTER_QUANT_MATRIX) + 0x0C (s0)
    sw t3, %lo(INTER_QUANT_MATRIX) + 0x10 (s0)
    sw t2, %lo(INTER_QUANT_MATRIX) + 0x14 (s0)
    sw t1, %lo(INTER_QUANT_MATRIX) + 0x18 (s0)
    sw t0, %lo(INTER_QUANT_MATRIX) + 0x1C (s0)

    jr ra
    nop
    .endfunc

    .func cmd_mpeg1_block_begin
cmd_mpeg1_block_begin:
    # a0: block type (0=Y, 4=CB, 5=CR)
    # a1: block address in RDRAM
    # a2: block pitch in RDRAM
    andi t0, a0, 0xFF
    sll t0, 2
    sw a1, %lo(RDRAM_BLOCKS)(t0)
    bnez t0, 1f
    sw a2, %lo(RDRAM_PITCHS)(t0)

    # block type = Y. Fill also the other partitions
    addi t1, a1, 8
    sw t1, %lo(RDRAM_BLOCKS) +  4
    sw a2, %lo(RDRAM_PITCHS) +  4
    sll t1, a2, 3   # calculate addr+8*pitch
    add t1, a1
    sw t1, %lo(RDRAM_BLOCKS) +  8
    sw a2, %lo(RDRAM_PITCHS) +  8
    addi t1, 8
    sw t1, %lo(RDRAM_BLOCKS) + 12
    sw a2, %lo(RDRAM_PITCHS) + 12
1:
    # fallthrough
    .endfunc

    .func cmd_mpeg1_block_switch
cmd_mpeg1_block_switch:
    # a0: partition index (0-3=Y, 4=CB, 5=CR)
    andi t0, a0, 0xFF
    sll t0, 1
    lhu t0, %lo(PIXELS_OFFSET)(t0)
    addi t0, %lo(PIXELS)
    sw t0, %lo(CUR_PIXELS)

    andi a0, 0xFF
    sll a0, 2
    lw t0, %lo(RDRAM_BLOCKS)(a0)
    lw t2, %lo(RDRAM_PITCHS)(a0)
    ble a0, 3*4, 1f
    li t1, DMA_SIZE(16, 16)
    li t1, DMA_SIZE(8, 8)
1:
    sw t0, %lo(RDRAM_BLOCK)
    sw t1, %lo(RDRAM_BLOCK_SIZE)
    sw t2, %lo(RDRAM_BLOCK_PITCH)

    # Clear coefficient mask
    sw zero, %lo(COEFF_MASK) + 0
    sw zero, %lo(COEFF_MASK) + 4

    # Clear coefficient matrix
    vxor $v00, $v00, $v00
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

    # Mark the matrix cell as used in the mask
    srl t0, index, 4
    srl t1, index, 1
    andi t1, 7
    li t2, 1
    sllv t1, t2, t1
    lbu t2, %lo(COEFF_MASK)(t0)
    or t2, t1
    sb t2, %lo(COEFF_MASK)(t0)

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
    #define v_scale     $v08
    #define v_const2    $v29
    #define kp1         v_const2,e(0)
    #define km1         v_const2,e(1)
    #define kp2         v_const2,e(2)
    #define kzero       v_const2,e(3)
    #define kp16        v_const2,e(4)
    #define km16        v_const2,e(5)
    #define kp8         v_const2,e(6)
    #define km31        v_const2,e(7)

    lw t0, %lo(PIXELCHECK)
    assert_eq t0, 0xBADC0DE, ASSERT_PIXELCHECK(5)

    li s0, %lo(DEQUANT_CONSTS)
    lqv v_const2,0, 0,s0

    andi quant_scale, a0, 0xFF00
    sll quant_scale, 1
    mtc2 quant_scale, v_scale,0
    andi intra, a0, 0xFF

    li s0, %lo(IDCT_MATRIX)
    li s1, %lo(INTER_QUANT_MATRIX)
    li s2, %lo(IDCT_PREMULT)
    li s3, %lo(COEFF_MASK)
    sll t0, intra, 6
    add s1, t0
    
    lhu dc, 0(s0)

    li loop_idx, 7
dequant_loop:
    # Load the coefficient mask and store it in VCC. This is a bitmask
    # which contains 1 for each vector lane that contains an actual coefficient.
    # The others will be zero, but will need to be reset to zero at the end
    # of calculations (via VMRG).
    lbu t0, 0(s3)
    ctc2 t0, COP2_CTRL_VCC

    lqv $v00,0, 0,s0
    lpv $v01,0, 0,s1
    lqv $v02,0, 0,s2

    # Initial scaling of the level
    # C: level <<= 1;
    bnez intra, dequant
    vmudh $v00, $v00, kp2

    # Initial rounding of level (on inter frames only)
    # C: level += (level < 0 ? -1 : 1);
    vrndp16 $v00, kp1
    vrndn16 $v00, km1

dequant:
    # Scale the quantization matrix coefficient by the quantization scale.
    # C: self->quantizer_scale * quant_matrix[]
    vmudl $v01, $v01, v_scale,e(0)

    # Inverse quantization
    # C: level * scale >> 4.
    #
    # NOTE: >>4 is not done here. The 4 additional bits are kept in the
    # accumulator.
    #
    # NOTE: VMULQ has a behavior that, as far as I can tell, differs from
    # published MPEG1 standard: when the number is negative, it adds a
    # rounding value of 31 (!). This does not match official PDFs and other
    # implementations. To be fully accurate, we need to revert this by
    # subtracting 31 (via VRNDN16).
    vmulq $v00, $v00, $v01
    vrndn16 $v00, km31

    # Oddification and clamping
    #
    # C: if ((level & 1) == 0) { level += level > 0 ? -1 : 1; }
    # C: if (level > 2047) { level = 2047; }
    # C: if (level < -2048) { level = -2048; }
    #
    # The final result is <<4, but VMACQ returns a clamped value whose last
    # 4 bits have been masked out, so we can safely use it anyway.
    vmacq $v00

    # Apply pre-multiplier.
    # C: level = (level * PLM_VIDEO_PREMULTIPLIER_MATRIX[]) >> RSP_IDCT_SCALER;
    #
    # The final result doesn't fit in 16-bit, which is why we introduced 
    # a scaling by RSP_IDCT_SCALER. We fetch the high part from the accumulator
    # and do a 32-bit shift. We take the chance to finally remove the <<4
    # left by the dequantization steps.
    #
    vmudn $v00, $v02, $v00
    vsar $v03, $v03, $v03,e(1)
    vsrl $v00, $v00, (RSP_IDCT_SCALER+4)
    vsll8 $v03, $v03, 16-(RSP_IDCT_SCALER+4)
    vor $v00, $v00, $v03

    # Keep only the values that contain actual coefficients. The others are
    # forced to zero as the above sequence could have produced non-zero
    # results.
    vmrg $v00, $v00, kzero

    # Store the output and increment the loop counters
    sqv $v00,0, 0,s0
    addi s0, 16
    addi s1, 8
    addi s2, 16
    addi s3, 1
    bnez loop_idx, dequant_loop
    addi loop_idx, -1

    # Restore initial DC coefficient	
    beqz intra, end_dequant
    li s0, %lo(IDCT_MATRIX)
    sh dc, 0(s0)

end_dequant:
    lw t0, %lo(PIXELCHECK)
    assert_eq t0, 0xBADC0DE, ASSERT_PIXELCHECK(6)

    j RSPQ_Loop
    nop

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

    .func cmd_mpeg1_zero_pixels
cmd_mpeg1_zero_pixels:
    lw s4, %lo(CUR_PIXELS)
    assert_ne s4, zero, ASSERT_UNDEFINED_BLOCK

    # Wait DMA idle before zeroing pixels, in case the
    # previous macroblock was still being DMA'd to RDRAM.
    jal DMAWaitIdle  
    vxor $v00, $v00, $v00
    sqv $v00,0, 0*16,s4
    sqv $v00,0, 1*16,s4
    sqv $v00,0, 2*16,s4
    sqv $v00,0, 3*16,s4
    j RSPQ_Loop
    nop
    .endfunc

    .func cmd_mpeg1_load_pixels
cmd_mpeg1_load_pixels:
    lw t0, %lo(PIXELCHECK)
    assert_eq t0, 0xBADC0DE, ASSERT_PIXELCHECK(7)

    lw s0, %lo(RDRAM_BLOCK)
    assert_ne s0, zero, ASSERT_UNDEFINED_BLOCK
    lw s4, %lo(CUR_PIXELS)
    assert_ne s4, zero, ASSERT_UNDEFINED_BLOCK
    lw t1, %lo(RDRAM_BLOCK_PITCH)
    jal DMAIn
    li t0, DMA_SIZE(8,8)

    lw t4, %lo(PIXELCHECK)
    assert_eq t4, 0xBADC0DE, ASSERT_PIXELCHECK(8)

    j RSPQ_Loop
    nop

    .endfunc

    .func cmd_mpeg1_store_pixels
cmd_mpeg1_store_pixels:
    lw s0, %lo(RDRAM_BLOCK)
    assert_ne s0, zero, ASSERT_UNDEFINED_BLOCK
    lw s4, %lo(CUR_PIXELS)
    assert_ne s4, zero, ASSERT_UNDEFINED_BLOCK
    lw t1, %lo(RDRAM_BLOCK_PITCH)
    j DMAOutAsync
    lw t0, %lo(RDRAM_BLOCK_SIZE)
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
    lw s4, %lo(CUR_PIXELS)
    lbu t0, %lo(RDRAM_BLOCK_SIZE)+3
    beq t0, 0xF, store_addpred_16
    nop
    suv $v00,0, 0*8,s4
    suv $v01,0, 1*8,s4
    suv $v02,0, 2*8,s4
    suv $v03,0, 3*8,s4
    suv $v04,0, 4*8,s4
    suv $v05,0, 5*8,s4
    suv $v06,0, 6*8,s4
    jr ra
    suv $v07,0, 7*8,s4
store_addpred_16:
    suv $v00,0, 0*16,s4
    suv $v01,0, 1*16,s4
    suv $v02,0, 2*16,s4
    suv $v03,0, 3*16,s4
    suv $v04,0, 4*16,s4
    suv $v05,0, 5*16,s4
    suv $v06,0, 6*16,s4
    jr ra
    suv $v07,0, 7*16,s4
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
    jal load_idct_consts
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
    lw s4, %lo(CUR_PIXELS)
    lbu t0, %lo(RDRAM_BLOCK_SIZE)+3
    beq t0, 0xF, store_pred_16
    nop
    spv $v00,0, 0*8,s4
    spv $v01,0, 1*8,s4
    spv $v02,0, 2*8,s4
    spv $v03,0, 3*8,s4
    spv $v04,0, 4*8,s4
    spv $v05,0, 5*8,s4
    spv $v06,0, 6*8,s4
    j RSPQ_Loop
    spv $v07,0, 7*8,s4
store_pred_16:
    spv $v00,0, 0*16,s4
    spv $v01,0, 1*16,s4
    spv $v02,0, 2*16,s4
    spv $v03,0, 3*16,s4
    spv $v04,0, 4*16,s4
    spv $v05,0, 5*16,s4
    spv $v06,0, 6*16,s4
    j RSPQ_Loop
    spv $v07,0, 7*16,s4
    .endfunc

    .func cmd_mpeg1_block_decode
cmd_mpeg1_block_decode:
    # a0 = ncoeffs in matrix (low bytes)
    # a1 = 1=intra, 0=inter
    lw t0, %lo(PIXELCHECK)
    assert_eq t0, 0xBADC0DE, ASSERT_PIXELCHECK(1)

    jal load_idct_consts
    nop
    jal load_matrix
    nop

    beqz a1, load_pred
    nop
    jal_and_j zero_pred, decode_step2

load_pred:
    # Load prediction. This must have been already
    # loaded into the PIXELS block.
    lw s4, %lo(CUR_PIXELS)
    assert_ne s4, zero, ASSERT_UNDEFINED_BLOCK3

    lbu t0, %lo(RDRAM_BLOCK_SIZE)+3
    beq t0, 0xF, load_pred_16
load_pred_8:
    luv pred0,0, 0*8,s4
    luv pred1,0, 1*8,s4
    luv pred2,0, 2*8,s4
    luv pred3,0, 3*8,s4
    luv pred4,0, 4*8,s4
    luv pred5,0, 5*8,s4
    luv pred6,0, 6*8,s4
    j decode_step2
    luv pred7,0, 7*8,s4

load_pred_16:
    luv pred0,0, 0*16,s4
    luv pred1,0, 1*16,s4
    luv pred2,0, 2*16,s4
    luv pred3,0, 3*16,s4
    luv pred4,0, 4*16,s4
    luv pred5,0, 5*16,s4
    luv pred6,0, 6*16,s4
    luv pred7,0, 7*16,s4

decode_step2:
    andi a0, 0xFF
    addi a0, -1
    beqz a0, decode_dc_only
    nop

decode_ac:
    jal idct
    nop
    li s0, %lo(IDCT_MATRIX)
    sqv $v00,0, 0*16,s0
    sqv $v01,0, 1*16,s0
    sqv $v02,0, 2*16,s0
    sqv $v03,0, 3*16,s0
    sqv $v04,0, 4*16,s0
    sqv $v05,0, 5*16,s0
    sqv $v06,0, 6*16,s0
    sqv $v07,0, 7*16,s0
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
    j RSPQ_Loop
    nop

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
#define y7    $v10   // recycle x1

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

    #undef b1
    #undef b3
    #undef b4
    #undef tmp1
    #undef tmp2
    #undef b6
    #undef b7
    #undef m0
    #undef x4
    #undef x0
    #undef x1
    #undef x2
    #undef x3
    #undef y3
    #undef y4
    #undef y5
    #undef y6
    #undef y7
    .endfunc


#########################################################
#########################################################
#
# Prediction
#
#########################################################
#########################################################

    #define dmem_16x16_pitch 24
    #define dmem_8x8_pitch   16
    #define kp1    vshift,e(7)
    #define kp1e7  vshift,e(0)
    #define kp1e6  vshift,e(1)
    #define kp1e5  vshift,e(2)
    #define kp1e4  vshift,e(3)
    #define kp1e15 vshift8,e(0)
    #define kp1e14 vshift8,e(1)
    #define kp1e13 vshift8,e(2)
    #define block_size t8


    .func block_copy_8x8
block_copy_8x8:
    # s0: source buffer (pitch = dmem_8x8_pitch)
    # s4: dest buffer (pitch = 8)
    beq block_size, 16, block_copy_16x16

    addi t0, block_size, -2
1:
    add s3, s4, block_size
    luv $v00,0, 0*dmem_8x8_pitch,s0
    luv $v01,0, 1*dmem_8x8_pitch,s0
    suv $v00,0, 0,s4
    suv $v01,0, 0,s3
    addi s0, 2*dmem_8x8_pitch
    add s4, s3, block_size
    bgtz t0, 1b
    addi t0, -2

    jr ra
    nop
    .endfunc

    .func block_copy_16x16
block_copy_16x16:
    # s0: source buffer (pitch = dmem_16x16_pitch)
    # s4: dest buffer (pitch = 8)
    li t1, 8
    addi t0, block_size, -2
1:
    add s3, s4, block_size
    luv $v00,0, 0*dmem_16x16_pitch,s0
    luv $v01,0, 1*dmem_16x16_pitch,s0
    suv $v00,0, 0,s4
    suv $v01,0, 0,s3
    luv $v00,0, 0*dmem_16x16_pitch+8,s0
    luv $v01,0, 1*dmem_16x16_pitch+8,s0
    suv $v00,0, 8,s4
    suv $v01,0, 8,s3
    addi s0, 2*dmem_16x16_pitch
    add s4, s3, block_size
    bgtz t0, 1b
    addi t0, -2

    jr ra
    nop
    .endfunc

    .func block_interp_8x8
block_interp_8x8:
    # s0: source buffer (pitch = dmem_8x8_pitch)
    # s4: dest buffer (pitch = 8)
    #define line t1

    beq block_size, 16, block_interp_16x16

    li t0, 8-2
1:
    luv $v00,0, 0*dmem_8x8_pitch,s0
    luv $v01,0, 1*dmem_8x8_pitch,s0
    luv $v02,0, 0*8,s4
    luv $v03,0, 1*8,s4

    vaddc $v04,$v00,$v02,0
    vaddc $v05,$v01,$v03,0

    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    spv $v04,0, 0*8,s4
    spv $v05,0, 1*8,s4

    addi s0, 2*dmem_8x8_pitch
    addi s4, 2*8
    bgtz t0, 1b
    addi t0, -2

    jr ra
    nop
    #undef line
    .endfunc

    .func block_interp_16x16
block_interp_16x16:
    # s0: source buffer (pitch = dmem_16x16_pitch)
    # s4: dest buffer (pitch = 8)
    #define line t1

    li t0, 16-1
1:
    luv $v00,0, 0*8,s0
    luv $v01,0, 1*8,s0
    luv $v02,0, 0*8,s4
    luv $v03,0, 1*8,s4

    vaddc $v04,$v00,$v02
    vaddc $v05,$v01,$v03

    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    spv $v04,0, 0*8,s4
    spv $v05,0, 1*8,s4

    addi s0, dmem_16x16_pitch
    addi s4, 2*8
    bgtz t0, 1b
    addi t0, -1

    jr ra
    nop
    #undef line
    .endfunc


    .func block_copy_8x8_filter2
block_copy_8x8_filter2:
    # s0: source buffer (pitch = dmem_8x8_pitch)
    # s1: second pointer into source buffer (for interpolation)
    # s4: dest buffer (pitch = 8)
    #define line t1

    beq block_size, 16, block_copy_16x16_filter2

    # We calculate two lines at a time, to be faster
    li line, 8-2
1:
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, dmem_8x8_pitch,s0
    luv $v03,0, dmem_8x8_pitch,s1

    vaddc $v04,$v00,$v01,0
    vaddc $v05,$v02,$v03,0

    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    spv $v04,0, 0*8,s4
    spv $v05,0, 1*8,s4

    addi s0, dmem_8x8_pitch*2
    addi s1, dmem_8x8_pitch*2
    addi s4, 2*8
    bgtz line, 1b
    addi line, -2

    jr ra
    nop	

    #undef line
    .endfunc

    .func block_interp_8x8_filter2
block_interp_8x8_filter2:
    # s0: source buffer (pitch = dmem_8x8_pitch)
    # s1: second pointer into source buffer (for interpolation)
    # s4: dest buffer (pitch = 8)
    #define line t1

    beq block_size, 16, block_interp_16x16_filter2

    # We calculate two lines at a time, to be faster
    li line, 8-2
1:
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, dmem_8x8_pitch,s0
    luv $v03,0, dmem_8x8_pitch,s1
    luv $v08,0, 0*8,s4
    luv $v09,0, 1*8,s4

    vaddc $v04,$v00,$v01
    vaddc $v05,$v02,$v03

    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    vsrl $v04, $v04, 1
    vsrl $v05, $v05, 1

    vaddc $v04,$v04,$v08
    vaddc $v05,$v05,$v09

    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    spv $v04,0, 0*8,s4
    spv $v05,0, 1*8,s4

    addi s0, dmem_8x8_pitch*2
    addi s1, dmem_8x8_pitch*2
    addi s4, 2*8
    bgtz line, 1b
    addi line, -2

    jr ra
    nop	

    #undef line
    .endfunc

    .func block_copy_16x16_filter2
block_copy_16x16_filter2:
    # s0: source buffer (pitch = dmem_16x16_pitch)
    # s1: second pointer into source buffer (for interpolation)
    # s4: dest buffer (pitch = 8)
    #define line t1

    li line, 16-1
1:
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, 8,s0
    luv $v03,0, 8,s1

    vaddc $v04,$v00,$v01,0
    vaddc $v05,$v02,$v03,0

    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    spv $v04,0, 0*8,s4
    spv $v05,0, 1*8,s4

    addi s0, dmem_16x16_pitch
    addi s1, dmem_16x16_pitch
    addi s4, 16
    bgtz line, 1b
    addi line, -1

    jr ra
    nop	

    #undef line
    .endfunc

    .func block_interp_16x16_filter2
block_interp_16x16_filter2:
    # s0: source buffer (pitch = dmem_16x16_pitch)
    # s1: second pointer into source buffer (for interpolation)
    # s4: dest buffer (pitch = 8)
    #define line t1

    li line, 16-1
1:
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, 8,s0
    luv $v03,0, 8,s1
    luv $v08,0, 0,s4
    luv $v09,0, 8,s4

    vaddc $v04,$v00,$v01,0
    vaddc $v05,$v02,$v03,0
    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7
    vsrl $v04,$v04,1
    vsrl $v05,$v05,1
    vaddc $v04,$v04,$v08
    vaddc $v05,$v05,$v09
    vaddc $v04,$v04,kp1e7
    vaddc $v05,$v05,kp1e7

    spv $v04,0, 0*8,s4
    spv $v05,0, 1*8,s4

    addi s0, dmem_16x16_pitch
    addi s1, dmem_16x16_pitch
    addi s4, 16
    bgtz line, 1b
    addi line, -1

    jr ra
    nop	

    #undef line
    .endfunc

    .func block_copy_8x8_filter4
block_copy_8x8_filter4:
    # s0: source buffer (pitch = dmem_8x8_pitch)
    # s4: dest buffer (pitch = 8)
    #define line t1

    beq block_size, 16, block_copy_16x16_filter4

    addi s1, s0, 1
    addi s2, s0, dmem_8x8_pitch
    addi s3, s2, 1
    li line, 7

copy_loop_4:
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, 0,s2
    luv $v03,0, 0,s3

    vmudl $v04,$v00,kp1e14
    vmadl $v04,$v01,kp1e14
    vmadl $v04,$v02,kp1e14
    vmadl $v04,$v03,kp1e14
    vaddc $v04,$v04,kp1e6

    suv $v04,0, 0,s4
    add s0, dmem_8x8_pitch
    add s1, dmem_8x8_pitch
    add s2, dmem_8x8_pitch
    add s3, dmem_8x8_pitch
    add s4, 8
    bgtz line, copy_loop_4
    addi line, -1

    jr ra
    nop

    #undef line
    .endfunc

    .func block_copy_16x16_filter4
block_copy_16x16_filter4:
    # s0: source buffer (pitch = dmem_16x16_pitch)
    # s4: dest buffer (pitch = 8)
    #define line t1

    addi s1, s0, 1
    addi s2, s0, dmem_16x16_pitch
    addi s3, s2, 1
    li line, 15

1:
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, 0,s2
    luv $v03,0, 0,s3

    luv $v04,0, 8,s0
    luv $v05,0, 8,s1
    luv $v06,0, 8,s2
    luv $v07,0, 8,s3

    vmudl $v16,$v00,kp1e14
    vmadl $v16,$v01,kp1e14
    vmadl $v16,$v02,kp1e14
    vmadl $v16,$v03,kp1e14

    vmudl $v17,$v04,kp1e14
    vmadl $v17,$v05,kp1e14
    vmadl $v17,$v06,kp1e14
    vmadl $v17,$v07,kp1e14

    vaddc $v16,$v16,kp1e6
    vaddc $v17,$v17,kp1e6

    suv $v16,0, 0,s4
    suv $v17,0, 8,s4
    add s0, dmem_16x16_pitch
    add s1, dmem_16x16_pitch
    add s2, dmem_16x16_pitch
    add s3, dmem_16x16_pitch
    add s4, 16
    bgtz line, 1b
    addi line, -1

    jr ra
    nop

    #undef line
    .endfunc


    .func block_interp_8x8_filter4
block_interp_8x8_filter4:
    # s0: source buffer (pitch = dmem_8x8_pitch)
    # s4: dest buffer (pitch = 8)
    #define line t1
    #define kp1e7p1e6  $v10,e(0)

    li t0, 0xC0
    mtc2 t0, $v10,0

    beq block_size, 16, block_interp_16x16_filter4
    addi s1, s0, 1

    li line, 7
    
    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, dmem_8x8_pitch,s0
    luv $v03,0, dmem_8x8_pitch,s1
    lpv $v05,0, -8,s4
    .align 3
1:
    vmudl $v04,$v00,kp1e14;          luv $v08,0, 0,s4
    vmadl $v04,$v01,kp1e14;          add s0, dmem_8x8_pitch
    vmadl $v04,$v02,kp1e14;          add s1, dmem_8x8_pitch
    vmadl $v04,$v03,kp1e14;          spv $v05,0, -8,s4
                                     luv $v00,0, 0,s0
                                     luv $v01,0, 0,s1
    vaddc $v04,$v04,kp1e7p1e6;       luv $v02,0, dmem_8x8_pitch,s0
                                     luv $v03,0, dmem_8x8_pitch,s1
                                     add s4, 8         
    vaddc $v05,$v04,$v08;            bgtz line, 1b
                                     addi line, -1

    jr ra
                                     spv $v05,0, -8,s4

    .endfunc
    .func block_interp_16x16_filter4
block_interp_16x16_filter4:
    # s0: source buffer (pitch = dmem_16x16_pitch)
    # s4: dest buffer (pitch = 8)
    #define line t1
    #define kp1e7p1e6  $v10,e(0)

    li line, 16

    luv $v00,0, 0,s0
    luv $v01,0, 0,s1
    luv $v02,0, dmem_16x16_pitch,s0
    luv $v03,0, dmem_16x16_pitch,s1
    .align 3
1:
    vmudl $v16,$v00,kp1e14;           luv $v04,0, 8,s0
    vmadl $v16,$v01,kp1e14;           luv $v05,0, 8,s1
    vmadl $v16,$v02,kp1e14;           luv $v06,0, dmem_16x16_pitch+8,s0
    vmadl $v16,$v03,kp1e14;           luv $v07,0, dmem_16x16_pitch+8,s1

    vmudl $v17,$v04,kp1e14;           luv $v08,0, 0,s4
    vmadl $v17,$v05,kp1e14;           luv $v09,0, 8,s4
    vmadl $v17,$v06,kp1e14;           add s0, dmem_16x16_pitch
    vmadl $v17,$v07,kp1e14;           add s1, dmem_16x16_pitch

                                      luv $v00,0, 0,s0
    vaddc $v16,$v16,kp1e7p1e6;        luv $v01,0, 0,s1
    vaddc $v17,$v17,kp1e7p1e6;        luv $v02,0, dmem_16x16_pitch,s0     
                                      luv $v03,0, dmem_16x16_pitch,s1

    vaddc $v16,$v16,$v08;             addi line, -1
    vaddc $v17,$v17,$v09;             add s4, 16

                                      spv $v16,0, -16,s4	
                                      bgtz line, 1b
                                      spv $v17,0, -8,s4
    jr ra
    nop

    #undef kp1e7p1e6
    #undef line
    .endfunc

    .func cmd_mpeg1_block_predict
cmd_mpeg1_block_predict:
    # a0: source
    # a1: source pitch
    # a2: oddh/oddv

    #define src_pitch a1

    # Calculate DMA size. In general, for filtering, we need to
    # DMA one pixel more both horizontally and vertically. Given the
    # 8-byte constraint on RSP DMA, this means 24x17 for a 16x16 block
    # and 16x9 for a 8x8 block. To calculate it, it's sufficient to
    # add 1 to both W and H in the block size, and the RSP will round up
    # to 8 automatically.
    lw t0, %lo(RDRAM_BLOCK_SIZE)
    addi t0, DMA_SIZE(2,2)
    andi block_size, t0, 0xFF

    li s4, %lo(SOURCE_PIXELS)
    move s0, a0
    jal DMAIn
    move t1, a1

    move s0, s4
    lw s4, %lo(CUR_PIXELS)

    andi t0, a2, 0x4
    bnez t0, predict_interpolate
    xor a2, t0

predict_copy:
    beqz a2, copy
    addi a2, -1
    beqz a2, copy_odd_v
    addi a2, -1
    beqz a2, copy_odd_h
    nop

    jal_and_j block_copy_8x8_filter4, RSPQ_Loop

copy_odd_h:
    addi s1, s0, 1
    jal_and_j block_copy_8x8_filter2, RSPQ_Loop

copy_odd_v:
    add s1, s0, block_size
    addi s1, 8
    jal_and_j block_copy_8x8_filter2, RSPQ_Loop

copy:
    jal_and_j block_copy_8x8, RSPQ_Loop

predict_interpolate:
    beqz a2, interpolate
    addi a2, -1
    beqz a2, interpolate_odd_v
    addi a2, -1
    beqz a2, interpolate_odd_h
    nop
    jal_and_j block_interp_8x8_filter4, RSPQ_Loop

interpolate_odd_h:
    addi s1, s0, 1
    jal_and_j block_interp_8x8_filter2, RSPQ_Loop

interpolate_odd_v:
    add s1, s0, block_size
    addi s1, 8
    jal_and_j block_interp_8x8_filter2, RSPQ_Loop

interpolate:
    jal_and_j block_interp_8x8, RSPQ_Loop


