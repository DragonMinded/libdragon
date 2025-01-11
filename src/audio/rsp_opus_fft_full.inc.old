	####################################################################
    #
    # Opus FFT implementation (for IMDCT)
    #
	####################################################################
    #    
    # This file is part of the rsp_opus_imdct ucode. It implements
    # the FFT functions required for the IMDCT transform.
    #
    # The values for the FFT are always stored at %lo(IMDCT_DATA). The data
    # has been prepared by the IMDCT pre-rotation step, and is arranged
    # as an array of complex numbers with the following layout:
    #
    # RI0 II0 RF0 IF0 RI1 II1 RF1 IF1...
    #
    # where: 
    #
    #    RIx = Real integer part of value x
    #    IIx = Imaginary integer part of value x
    #    RFx = Real fractional part of value x
    #    IFx = Imaginary fractional part of value x
    #
    # The various FFT functions that operate on complex numbers load them
    # into vector registers putting real and imaginary parts in consecutive
    # lanes, and separating integer and fractional part into two vector register,
    # which is the best layout to operate on 32-bit fixed points in RSP.
    #
    # For instance, this shows the two vector registers vidx0i/vidx0f holding
    # a total of 4 complex numbers:
    #
    #   $vidx0i = RI0 II0 RI1 II1 RI2 II2 RI3 II3
    #   $vidx0f = RF0 IF0 RF1 IF1 RF2 IF2 RF3 IF3
    #
    # This layout seems reasomable to perform standard complex operations.
    # For instance, adding complex numbers is simply done via a standard
    # 32-bit addition sequence:
    #
    #    vaddc vidx0f, vidx1f
    #    vadd  vidx0i, vidx1i
    #
    # (recall that RSP opcodes mnemonics are reversed, so vaddc writes the
    # the carry, while vadd uses it as input to the addition).
    #
	####################################################################

    #define fmm  t7
    #define fM   t8
    #define fN   t9
    #define fZ   s7
    #define fTW  t6
    #define fK   t5

    .text


#######################################################################
# TWIDDLES
#######################################################################

    #define vtmp1i          $v15
    #define vtmp1f          $v16
    #define vtmp2i          $v17
    #define vtmp2f          $v18
    #define vtmp3i          $v19
    #define vtmp3f          $v20

    #define vtwiddle1       $v21
    #define vtwiddle2       $v22
    #define vtwiddle1inv    $v23
    #define vtwiddle2inv    $v24

    #define vk4010          $v25
    #define vk2             $v26
    #define vtwidx1         $v27
    #define vtwidx2         $v28
    #define vtwk1           $v29

    .func kf_twiddle_2
kf_twiddle_2:
    li t0, -2
    mtc2 t0, vtmp3f.e0


    # TWIDDLE1
    vsll vtmp1i, vtwidx1, 1
    vxor vtmp1f, vtmp1i, vtwidx1
    vge vtmp1f, vzero
    vmrg vtmp1f, vk2, vtmp3f.e0
    vmulf vtmp2i, vtmp1i, vtmp1i
    vmudm vtmp3i, vtmp2i, vtmp2i
    vmudn vtwiddle1, vtmp2i, vtwk1.e2
    vmadn vtwiddle1, vtmp2i, vtwk1.e2
    vmacf vtwiddle1, vk4010, vtwk1.e7   
    vmacf vtwiddle1, vtmp3i, vtwk1.e4
    vmudn vtwiddle1, vtmp1f

    # TWIDDLE2
    vsll vtmp1i, vtwidx2, 1
    vxor vtmp1f, vtmp1i, vtwidx2
    vge vtmp1f, vzero
    vmrg vtmp1f, vk2, vtmp3f.e0
    vmulf vtmp2i, vtmp1i, vtmp1i
    vmudm vtmp3i, vtmp2i, vtmp2i
    vmudn vtwiddle2, vtmp2i, vtwk1.e2
    vmadn vtwiddle2, vtmp2i, vtwk1.e2
    vmacf vtwiddle2, vk4010, vtwk1.e7   
    vmacf vtwiddle2, vtmp3i, vtwk1.e4
    vmudn vtwiddle2, vtmp1f

    # Increment twiddle angles
    lqv vtmp1f, 0x20,fTW
    lqv vtmp2f, 0x30,fTW
    vaddc vtwidx1, vtmp1f
    vaddc vtwidx2, vtmp2f

    # Load VCC with the bitpatternt to allow merging
    # of the real and imaginary parts. Notice that VCC
    # is modified by the vge opcode in the twiddle
    # calculations, so we need to do this every interation.
    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC

    # swap real/imag
    vsubc vtwiddle1inv, vzero, vtwiddle1.q1
    vsubc vtwiddle2inv, vzero, vtwiddle2.q1
    vmrg  vtwiddle1inv, vtwiddle1.q0
    vmrg  vtwiddle2inv, vtwiddle2.q0

    jr ra
    nop

    .endfunc

    #undef vtmp1i
    #undef vtmp1f
    #undef vtmp2i
    #undef vtmp2f
    #undef vtmp3i
    #undef vtmp3f

#######################################################################
# KF_BFLY2 - 2-point FFT butterfly
#######################################################################

    #.section .textovl1
    #.org 0x800

    # 0x5a82 is QCONST16(0.7071067812f, 15); negative is 0xa57e
    #define KF_BFLY2_CONST1  \
        .half 0x7fff, 0x0000, 0x5a82, 0xa57e, 0x0000, 0x8000, 0xa57e, 0xa57e
    #define KF_BFLY2_CONST2  \
        .half 0x0000, 0x7fff, 0x5a82, 0x5a82, 0x7fff, 0x0000, 0x5a82, 0xa57e

    #define vfh     $v01
    #define vfl     $v02
    #define vgh     $v03
    #define vgl     $v04
    #define vtmph   $v05
    #define vtmpl   $v06
    #define v___    $v07

    .func kf_bfly2
kf_bfly2:
    assert_eq fM, 4, 0x8500

    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC
    addiu fN, -1

    lqv vtwiddle1,    0x00,fTW
    lqv vtwiddle1inv, 0x10,fTW

kf_bfly2_loop:
    llv vfh.e0, 0x00,fZ
    llv vfl.e0, 0x04,fZ
    llv vfh.e2, 0x08,fZ
    llv vfl.e2, 0x0C,fZ
    llv vfh.e4, 0x10,fZ
    llv vfl.e4, 0x14,fZ
    llv vfh.e6, 0x18,fZ
    llv vfl.e6, 0x1C,fZ

    llv vgh.e0, 0x20,fZ
    llv vgl.e0, 0x24,fZ
    llv vgh.e2, 0x28,fZ
    llv vgl.e2, 0x2C,fZ
    llv vgh.e4, 0x30,fZ
    llv vgl.e4, 0x34,fZ
    llv vgh.e6, 0x38,fZ
    llv vgl.e6, 0x3C,fZ

    vaddc vgl, vgl
    vadd  vgh, vgh

    # tw    = 1    0     tw             tw              0    -1        -tw             -tw
    # twinv = 0    1     tw            -tw              1     0         tw             -tw
    # vtmp = vg0r vg0i  (vg1r+vg1i)*tw (vg1r-vg1i)*tw  vg2i -vg2r  (vg3i-vg3r)*tw  (-vg3i-vg3r)*tw
    #
    vmudm v___, vtwiddle1inv, vgl.q1
    vmadh v___, vtwiddle1inv, vgh.q1
    vmadm v___, vtwiddle1,    vgl.q0
    vmadh v___, vtwiddle1,    vgh.q0
    vsar vtmpl, COP2_ACC_MD
    vsar vtmph, COP2_ACC_HI

    # C_SUB( Fout2[0] ,  Fout[0] , t );
    # C_SUB( Fout2[1] ,  Fout[1] , t );
    # C_SUB( Fout2[2] ,  Fout[2] , t );
    # C_SUB( Fout2[3] ,  Fout[3] , t );
    vsubc vgl, vfl, vtmpl
    vsub  vgh, vfh, vtmph

    # C_ADDTO( Fout[0] ,  t );
    # C_ADDTO( Fout[1] ,  t );
    # C_ADDTO( Fout[2] ,  t );
    # C_ADDTO( Fout[3] ,  t );
    vaddc vfl, vtmpl
    vadd  vfh, vtmph

    slv vfh.e0, 0x00,fZ
    slv vfl.e0, 0x04,fZ
    slv vfh.e2, 0x08,fZ
    slv vfl.e2, 0x0C,fZ
    slv vfh.e4, 0x10,fZ
    slv vfl.e4, 0x14,fZ
    slv vfh.e6, 0x18,fZ
    slv vfl.e6, 0x1C,fZ

    slv vgh.e0, 0x20,fZ
    slv vgl.e0, 0x24,fZ
    slv vgh.e2, 0x28,fZ
    slv vgl.e2, 0x2C,fZ
    slv vgh.e4, 0x30,fZ
    slv vgl.e4, 0x34,fZ
    slv vgh.e6, 0x38,fZ
    slv vgl.e6, 0x3C,fZ

    addiu fZ, 0x40
    bgtz fN, kf_bfly2_loop
    addiu fN, -1

    jr ra
    nop

    .endfunc

    #undef vfh  
    #undef vfl  
    #undef vgh  
    #undef vgl  
    #undef vtmph
    #undef vtmpl
    #undef v___

    .text

#######################################################################
# KF_BFLY3 - 3-point FFT butterfly
#######################################################################

    #define __KF_ANGLE16_COS(i, N)             (((i) * (65536-1) / N) & 0xFFFF)
    #define __KF_ANGLE16_SIN(i, N)             ((__KF_ANGLE16_COS(i, N) + 0x4000) & 0xFFFF)
    #define __KF_BFLY_FSTRIDE_CPX(stride, N)   __KF_ANGLE16_COS(stride, N), __KF_ANGLE16_SIN(stride, N)

    # The first twiddle's angle is incremented by stride every iteration.
    # Since we process 4 complex numbers at a time, we need to create
    # a vector like this (where s=stride)
    #
    #   vtwidx1 = 0 0+0x4000 s s+0x4000 s*2 s*2+0x4000 s*3 s*3+0x4000
    #
    # since complex twiddles need to compute cosine for the imaginary part,
    # and sine for the imaginary part, and we calculate sin(x) as cos(x+0x4000.
    #
    # The second twiddle's angle is incremented by 2*stride every iteration,
    # so we must create the following vector:
    #
    #   vtwidx2 = 0 0+0x4000 s*2 s*2+0x4000 s*4 s*4+0x4000 s*6 s*6+0x4000
    #

    #define KF_BFLY3_TWIDDLE1(stride, N)  \
        .half __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*1, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*2, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*3, N)

    #define KF_BFLY3_TWIDDLE2(stride, N)  \
        .half __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*2, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*4, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*6, N)

    #define KF_BFLY3_TWINCR1(stride, N)  \
        .half __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
              __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
              __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
              __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N)

    #define KF_BFLY3_TWINCR2(stride, N)  \
        .half __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
              __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
              __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
              __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N)

    #define fZ0   s0
    #define fZm   s1
    #define fZm2  s2
    #define k     t4

    #define vidx0i          $v01
    #define vidx0f          $v02
    #define vidx1i          $v03
    #define vidx1f          $v04
    #define vidx2i          $v05
    #define vidx2f          $v06
    #define vidx3i          $v07
    #define vidx3f          $v08

    #define vtmp1i          $v09
    #define vtmp1f          $v10
    #define vtmp2i          $v11
    #define vtmp2f          $v12
    #define vtmp3i          $v13
    #define vtmp3f          $v14

    #define vtmp1invf       $v15
    #define vtmp1invi       $v16
    #define vtmp2invf       $v17
    #define vtmp2invi       $v18


    .func kf_bfly3
kf_bfly3:
    move ra2, ra

    # Some magic numbers we will need:
    # vtwk1.e3 = 0x9126 = -28378, just like in the C reference code
    li t0, 28378
    mtc2 t0, vtwk1.e3

    li t0, 2
    mtc2 t0, vk2.e0
    vcopy vk2, vk2.e0

    vcopy vk4010, vtwk1.e6

    sll fmm, 3

kf_bfly3_outer_loop:
    move fZ0, fZ
    sll t0, fM, 3
    addu fZm, fZ, t0
    addu fZm2, fZm, t0
    move k, fM

    lqv vtwidx1, 0x00,fTW
    lqv vtwidx2, 0x10,fTW

    # The innner loop processes fZ[0], fZ[m], Fz[m*2], and then moves to
    # next complex number. We vectorize 4 at a time, so we will load:
    #   vidx0 = fZ[0..3]
    #   vidx1 = fZ[m..m+3]
    #   vidx2 = fZ[m*2..m*2+3]

kf_bfly3_loop:
    llv vidx0i.e0, 0x00,fZ0
    llv vidx0f.e0, 0x04,fZ0
    llv vidx0i.e2, 0x08,fZ0
    llv vidx0f.e2, 0x0C,fZ0
    llv vidx0i.e4, 0x10,fZ0
    llv vidx0f.e4, 0x14,fZ0
    llv vidx0i.e6, 0x18,fZ0
    llv vidx0f.e6, 0x1C,fZ0

    llv vidx1i.e0, 0x00,fZm
    llv vidx1f.e0, 0x04,fZm
    llv vidx1i.e2, 0x08,fZm
    llv vidx1f.e2, 0x0C,fZm
    llv vidx1i.e4, 0x10,fZm
    llv vidx1f.e4, 0x14,fZm
    llv vidx1i.e6, 0x18,fZm
    llv vidx1f.e6, 0x1C,fZm

    llv vidx2i.e0, 0x00,fZm2
    llv vidx2f.e0, 0x04,fZm2
    llv vidx2i.e2, 0x08,fZm2
    llv vidx2f.e2, 0x0C,fZm2
    llv vidx2i.e4, 0x10,fZm2
    llv vidx2f.e4, 0x14,fZm2
    llv vidx2i.e6, 0x18,fZm2
    llv vidx2f.e6, 0x1C,fZm2

    jal kf_twiddle_2
    nop

    # C_MUL(scratch[1], Fout[m], *tw1)
    vmudm vtmp1f, vtwiddle1inv, vidx1f.q1
    vmadh vtmp1f, vtwiddle1inv, vidx1i.q1
    vmadm vtmp1i, vtwiddle1,    vidx1f.q0
    vmadh vtmp1i, vtwiddle1,    vidx1i.q0
    vsar vtmp1f, COP2_ACC_MD
    vsar vtmp1i, COP2_ACC_HI

    # C_MUL(scratch[2], Fout[m2], *tw2)
    vmudm vtmp2f, vtwiddle2inv, vidx2f.q1
    vmadh vtmp2f, vtwiddle2inv, vidx2i.q1
    vmadm vtmp2i, vtwiddle2,    vidx2f.q0
    vmadh vtmp2i, vtwiddle2,    vidx2i.q0
    vsar vtmp2f, COP2_ACC_MD
    vsar vtmp2i, COP2_ACC_HI

    # C_ADD(scratch[3], scratch[1], scratch[2])
    vaddc vtmp3f, vtmp1f, vtmp2f
    vadd  vtmp3i, vtmp1i, vtmp2i

    # C_SUB(scratch[0], scratch[1], scratch[2])
    vsubc vtmp1f, vtmp1f, vtmp2f
    vsub  vtmp1i, vtmp1i, vtmp2i

    # Fout[m].r = SUB32_ovflw(Fout->r, HALF_OF(scratch[3].r))
    # Fout[m].i = SUB32_ovflw(Fout->i, HALF_OF(scratch[3].i))
    # Notice that C_MUL results are halved becuse we multiplied by twiddle1/2
    # which are Q15, but we scaled by 16. 
    vsubc vidx1f, vidx0f, vtmp3f
    vsub  vidx1i, vidx0i, vtmp3i

    # Scale scratch[0] by 4. This recovers the bit of precision
    # we lost in C_MUL, and add another one in preparation of next multiplication
    vmudn vtmp1f, K4
    vmadh vtmp1i, K4

    # C_ADDTO(*Fout, scratch[3]);
    vaddc vidx0f, vtmp3f
    vadd  vidx0i, vtmp3i
    vaddc vidx0f, vtmp3f
    vadd  vidx0i, vtmp3i

    # C_MULBYSCALAR( scratch[0] , epi3.i );
    # This again looses one bit because epi3.i is Q15,
    # but we already premultiplied scratch[0] to account
    # for this.
    # NOTE: we keep epi3.i as inverted sign, so that it's
    # positive and we can use vmudl/madm. The result sign
    # will be inverted.
    vmudl vtmp1f, vtmp1f, vtwk1.e3
    vmadm vtmp1i, vtmp1i, vtwk1.e3
    vmadn vtmp1f, vzero, vzero

    # Swap real/image in scratch[0], and change sign of image part
    # Given that scratch[0] was already sign-inverted, we 
    # now have: -real, +image
    vsubc vtmp2f, vzero, vtmp1f.q1
    vsub  vtmp2i, vzero, vtmp1i.q1
    vmrg  vtmp1f, vtmp2f, vtmp1f.q0
    vmrg  vtmp1i, vtmp2i, vtmp1i.q0

    # Fout[m2].r = ADD32_ovflw(Fout[m].r, scratch[0].i);
    # Fout[m2].i = SUB32_ovflw(Fout[m].i, scratch[0].r);
    vaddc vidx2f, vidx1f, vtmp1f
    vadd  vidx2i, vidx1i, vtmp1i

    # Fout[m].r = SUB32_ovflw(Fout[m].r, scratch[0].i);
    # Fout[m].i = ADD32_ovflw(Fout[m].i, scratch[0].r);
    vsubc vidx1f, vtmp1f
    vsub  vidx1i, vtmp1i

    slv vidx0i.e0, 0x00,fZ0
    slv vidx0f.e0, 0x04,fZ0
    slv vidx0i.e2, 0x08,fZ0
    slv vidx0f.e2, 0x0C,fZ0
    slv vidx0i.e4, 0x10,fZ0
    slv vidx0f.e4, 0x14,fZ0
    slv vidx0i.e6, 0x18,fZ0
    slv vidx0f.e6, 0x1C,fZ0

    slv vidx1i.e0, 0x00,fZm
    slv vidx1f.e0, 0x04,fZm
    slv vidx1i.e2, 0x08,fZm
    slv vidx1f.e2, 0x0C,fZm
    slv vidx1i.e4, 0x10,fZm
    slv vidx1f.e4, 0x14,fZm
    slv vidx1i.e6, 0x18,fZm
    slv vidx1f.e6, 0x1C,fZm

    slv vidx2i.e0, 0x00,fZm2
    slv vidx2f.e0, 0x04,fZm2
    slv vidx2i.e2, 0x08,fZm2
    slv vidx2f.e2, 0x0C,fZm2
    slv vidx2i.e4, 0x10,fZm2
    slv vidx2f.e4, 0x14,fZm2
    slv vidx2i.e6, 0x18,fZm2
    slv vidx2f.e6, 0x1C,fZm2

    addiu fZ0, 0x20
    addiu fZm, 0x20
    addiu fZm2, 0x20

    addiu k, -4
    bgtz k, kf_bfly3_loop
    nop

    addu fZ, fmm
    addiu fN, -1
    bgtz fN, kf_bfly3_outer_loop
    nop

    jr ra2
    nop
    .endfunc

    #undef fZ0 
    #undef fZm 
    #undef fZm2
    #undef k   

    #undef vtmp1invf       
    #undef vtmp1invi       
    #undef vtmp2invf       
    #undef vtmp2invi       

#######################################################################
# KF_BFLY4 - 4-point FFT butterfly (M=1 simple case)
#######################################################################

    #define KF_BFLY4_TWIDDLE1(stride, N)  \
        .half 0, 0, 0, 0, \
              __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*1, N)
    #define KF_BFLY4_TWIDDLE2(stride, N)  \
        .half __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*2, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
              __KF_BFLY_FSTRIDE_CPX(stride*3, N)
    #define KF_BFLY4_TWINCR1(stride, N)  \
        .half 0, 0, 0, 0 \
              __KF_ANGLE16_COS(stride*1, N), __KF_ANGLE16_COS(stride*1, N), \
              __KF_ANGLE16_COS(stride*1, N), __KF_ANGLE16_COS(stride*1, N)
    #define KF_BFLY4_TWINCR2(stride, N)  \
        .half __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
              __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
              __KF_ANGLE16_COS(stride*3, N), __KF_ANGLE16_COS(stride*3, N), \
              __KF_ANGLE16_COS(stride*3, N), __KF_ANGLE16_COS(stride*3, N)

    #define vidx0h     $v01
    #define vidx0l     $v02
    #define vidx1h     $v03
    #define vidx1l     $v04
    #define vidx2h     $v05
    #define vidx2l     $v06
    #define vidx3h     $v07
    #define vidx3l     $v08

    #define vtmp1h     $v09
    #define vtmp1l     $v10
    #define vtmp2h     $v11
    #define vtmp2l     $v12
    #define vtmp3h     $v13
    #define vtmp3l     $v14

    #define v___       $v15
    #define tmp        s5
    #define j_idx      t8

    .func kf_bfly4
kf_bfly4:
kf_bfly4_m1:
    move ra2, ra
    addiu tmp, fTW, 0x40

    addiu fN, -2
    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC

    sll fmm, 3
    sll fM, 3

kf_bfly4_outer_loop:
    move s0, fZ
    addu s1, s0, fM
    addu s2, s1, fM
    addu s3, s2, fM
    lqv vtwidx1,  0x00,fTW
    lqv vtwidx2,  0x10,fTW

    srl j_idx, fM, 3

    # Process 2 iterations at a time. We need 16+16 SU for memory transfers
    # and we only have ~20 VUs to do, so this seems like a good compromise.
kf_bfly4_m1_loop:
    # vidx0:  f0r f0i -- --   f1r f1i -- --
    # vidx2:  f2r f2i -- --   f3r f3i -- --
    llv vidx0h.e0, 0x00,s0
    llv vidx0l.e0, 0x04,s0
    llv vidx0h.e2, 0x08,s0
    llv vidx0l.e2, 0x0C,s0

    llv vidx0h.e4, 0x00,s1
    llv vidx0l.e4, 0x04,s1
    llv vidx0h.e6, 0x08,s1
    llv vidx0l.e6, 0x0C,s1

    llv vidx2h.e0, 0x00,s2
    llv vidx2l.e0, 0x04,s2
    llv vidx2h.e2, 0x08,s2
    llv vidx2l.e2, 0x0C,s2

    llv vidx2h.e4, 0x00,s3
    llv vidx2l.e4, 0x04,s3
    llv vidx2h.e6, 0x08,s3
    llv vidx2l.e6, 0x0C,s3

    jal kf_twiddle_2
    nop

    # C_MUL(scratch[0],Fout[m] , *tw1 );
    vmudm v___,  vtwiddle1,    vidx0l.q0
    vmadh v___,  vtwiddle1,    vidx0h.q0
    vmadm v___,  vtwiddle1inv, vidx0l.q1
    vmadh v___,  vtwiddle1inv, vidx0h.q1
    vsar  vidx1l, COP2_ACC_MD
    vsar  vidx1h, COP2_ACC_HI

    # C_MUL(scratch[1],Fout[m2] , *tw2 );
    # C_MUL(scratch[2],Fout[m3] , *tw3 );
    vmudm v___,  vtwiddle2,    vidx2l.q0
    vmadh v___,  vtwiddle2,    vidx2h.q0
    vmadm v___,  vtwiddle2inv, vidx2l.q1
    vmadh v___,  vtwiddle2inv, vidx2h.q1
    vsar  vidx2l, COP2_ACC_MD
    vsar  vidx2h, COP2_ACC_HI

    # Recover 1 bit of precision lost after CMUL
    vaddc vidx1l, vidx1l
    vadd  vidx1h, vidx1h
    vaddc vidx2l, vidx2l
    vadd  vidx2h, vidx2h

    # Rotate the vectors
    # vidx0:  f0r f0i -- --   -- -- -- --
    # vidx1:  s1r s1i -- --   -- -- -- --
    # vidx2:  s2r s2i -- --   s3r s3i -- --
    # vidx3:  s3r s3i -- --   -- -- -- --
    sqv vidx1h.e4, 0x00,tmp
    sqv vidx1l.e4, 0x10,tmp
    sqv vidx2h.e4, 0x20,tmp
    sqv vidx2l.e4, 0x30,tmp
    lqv vidx1h.e0, 0x00,tmp
    lqv vidx1l.e0, 0x10,tmp
    lqv vidx3h.e0, 0x20,tmp
    lqv vidx3l.e0, 0x30,tmp

    # C_SUB( scratch0 , *Fout, Fout[2] );
    vsubc vtmp1l, vidx0l, vidx2l
    vsub  vtmp1h, vidx0h, vidx2h

    # C_ADDTO(*Fout, Fout[2]);
    vaddc vidx0l, vidx2l
    vadd  vidx0h, vidx2h

    # C_ADD( scratch1 , Fout[1] , Fout[3] );
    vaddc vtmp2l, vidx1l, vidx3l
    vadd  vtmp2h, vidx1h, vidx3h

    # C_SUB( Fout[2], *Fout, scratch1 );
    vsubc vidx2l, vidx0l, vtmp2l
    vsub  vidx2h, vidx0h, vtmp2h

    # C_ADDTO( *Fout , scratch1 );
    vaddc vidx0l, vtmp2l
    vadd  vidx0h, vtmp2h

    # C_SUB( scratch1 , Fout[1] , Fout[3] );
    vsubc vtmp2l, vidx1l, vidx3l
    vsub  vtmp2h, vidx1h, vidx3h

    # Invert scratch2 real/imag and change sign of imag
    vsubc vtmp3l, vzero, vtmp2l.q1
    vsub  vtmp3h, vzero, vtmp2h.q1
    vmrg  vtmp2l, vtmp3l, vtmp2l.q0
    vmrg  vtmp2h, vtmp3h, vtmp2h.q0

    # Fout[1].r = ADD32_ovflw(scratch0.r, scratch1.i);
    # Fout[1].i = SUB32_ovflw(scratch0.i, scratch1.r);
    vsubc vidx1l, vtmp1l, vtmp2l
    vsub  vidx1h, vtmp1h, vtmp2h

    # Fout[3].r = SUB32_ovflw(scratch0.r, scratch1.i);
    # Fout[3].i = ADD32_ovflw(scratch0.i, scratch1.r);
    vaddc vidx3l, vtmp1l, vtmp2l
    vadd  vidx3h, vtmp1h, vtmp2h

    slv vidx0h.e0, 0x00,s0
    slv vidx0l.e0, 0x04,s0
    slv vidx0h.e2, 0x08,s0
    slv vidx0l.e2, 0x0C,s0

    slv vidx1h.e0, 0x00,s1
    slv vidx1l.e0, 0x04,s1
    slv vidx1h.e2, 0x08,s1
    slv vidx1l.e2, 0x0C,s1

    slv vidx2h.e0, 0x00,s2
    slv vidx2l.e0, 0x04,s2
    slv vidx2h.e2, 0x08,s2
    slv vidx2l.e2, 0x0C,s2

    slv vidx3h.e0, 0x00,s3
    slv vidx3l.e0, 0x04,s3
    slv vidx3h.e2, 0x08,s3
    slv vidx3l.e2, 0x0C,s3

    addiu s0, 0x10
    addiu s1, 0x10
    addiu s2, 0x10
    addiu s3, 0x10

    bgtz fN, kf_bfly4_m1_loop
    addiu fN, -2

    jr ra2
    nop

    .endfunc

    #undef vidx0i
    #undef vidx0f
    #undef vidx1i
    #undef vidx1f
    #undef vidx2i
    #undef vidx2f
    #undef vidx3i
    #undef vidx3f

    #undef vtmp1i
    #undef vtmp1f
    #undef vtmp2i
    #undef vtmp2f
    #undef vtmp3i
    #undef vtmp3f
    #undef v___ 

    #undef tmp

#######################################################################
# KF_BFLY4 - 4-point FFT butterfly (generic case)
#######################################################################

    #define fZ0   s0
    #define fZm   s1
    #define fZm2  s2
    #define fZm3  s3

    # f0r f0i f1r f1i f2r f2i f3r f3i    # 6
    # 16 su, 1 / loop

    # f0r f0i f1r f1i -- -- -- --
    # f2r f2i f3r f3i -- -- -- --
    # 32 su / 2 loop  
#if 0

    .func kf_bfly4
kf_bfly4:
    # This function is harder to parallelize efficiently because there are
    # 3 complex muls / twiddles per loop plus the fourh complex number, so it
    # doesn't align well to vector sizes.
    
    2 CMUL => 12
    4 ADD/SUB
    8 reverse + ADD/SUB reversed
    3 ADD/SUB straight


vx   f0 s0
vy   s1 s2
     ------

vx-vy =  (f0-s1)  (s0-s2)
            s5      s4

vx+vy =  (f0+s1)  (s0+s2)

    s5 = f0 - s1
    s4 = s0 - s2
    f2 = f0 + s1 - (s0 + s2)
    f0 = f0 + s1 + (s0 + s2)


    jr ra
    nop

    .endfunc
#endif



#######################################################################
# KF_BFLY5 - 5-point FFT butterfly
#######################################################################

    #define KF_BFLY5_TWIDDLE1(stride, N)  \
        .half   __KF_ANGLE16_COS(stride*0, N), __KF_ANGLE16_SIN(stride*0, N), \
                __KF_ANGLE16_COS(stride*1, N), __KF_ANGLE16_SIN(stride*1, N), \
                __KF_ANGLE16_SIN(stride*0, N), __KF_ANGLE16_COS(stride*0, N), \
                __KF_ANGLE16_SIN(stride*2, N), __KF_ANGLE16_COS(stride*2, N)

    #define KF_BFLY5_TWIDDLE2(stride, N)  \
        .half   __KF_ANGLE16_COS(stride*0, N), __KF_ANGLE16_SIN(stride*0, N), \
                __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_SIN(stride*4, N), \
                __KF_ANGLE16_SIN(stride*0, N), __KF_ANGLE16_COS(stride*0, N), \
                __KF_ANGLE16_SIN(stride*3, N), __KF_ANGLE16_COS(stride*3, N)

    #define KF_BFLY5_TWINCR1(stride, N)  \
        .half __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
              __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
              __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
              __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N)

    #define KF_BFLY5_TWINCR2(stride, N)  \
        .half __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
              __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
              __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N), \
              __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N)

    #define KF_BFLY5_YAR  (10126)
    #define KF_BFLY5_YAI (-31164)
    #define KF_BFLY5_YBR (-26510)
    #define KF_BFLY5_YBI (-19261)

    #define KF_BFLY5_CONST1 \
        .half KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR, \
              KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR
    #define KF_BFLY5_CONST2 \
        .half KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR, \
              KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR
    #define KF_BFLY5_CONST3 \
        .half KF_BFLY5_YAI, -KF_BFLY5_YBI, KF_BFLY5_YAI, -KF_BFLY5_YBI, \
              -KF_BFLY5_YAI, -KF_BFLY5_YBI, -KF_BFLY5_YAI, -KF_BFLY5_YBI
    #define KF_BFLY5_CONST4 \
        .half KF_BFLY5_YBI, KF_BFLY5_YAI, KF_BFLY5_YBI, KF_BFLY5_YAI, \
              KF_BFLY5_YBI, -KF_BFLY5_YAI, KF_BFLY5_YBI, -KF_BFLY5_YAI


    #define vf0h        $v01
    #define vf0l        $v02
    #define vf12h       $v03
    #define vf12l       $v04
    #define vf43h       $v05
    #define vf43l       $v06
    #define vout13h     $v03
    #define vout13l     $v04
    #define vout42h     $v05
    #define vout42l     $v06
    #define yconst1     $v07
    #define yconst2     $v08
    #define yconst3     $v09
    #define yconst4     $v10
    #define vs78h       $v11
    #define vs78l       $v12
    #define vs109h      $v13
    #define vs109l      $v14
    #define vs87h       $v15
    #define vs87l       $v16
    #define vs910h      $v17
    #define vs910l      $v18
    #define vout0h      $v19
    #define vout0l      $v20  
    #define v___        $v20  // reuse vout0l
    #define vs511h      $v11  // reuse vs78
    #define vs511l      $v12  // reuse vs78
    #define vs612h      $v13  // reuse vs109
    #define vs612l      $v14  // reuse vs109
    #define vk1m1       $v11  // reuse vs78

    #define tmp         s5
    #define k           t4


    .func kf_bfly5
kf_bfly5:
    move ra2, ra
    sll fmm, 3
    sll fM, 3
    lqv yconst1, 0x40,fTW
    lqv yconst2, 0x50,fTW
    lqv yconst3, 0x60,fTW
    lqv yconst4, 0x70,fTW
    addiu tmp, fTW, 0x40

kf_bfly5_outer_loop:
    lqv vtwidx1,  0x00,fTW
    lqv vtwidx2,  0x10,fTW

    addiu s0, fZ, 0
    addu s1, s0, fM
    addu s2, s1, fM
    addu s3, s2, fM
    addu s4, s3, fM
    addu fZ, fmm
    addiu fN, -1
    addiu k, fM, -2*8


    # The inner loop vectorizes two iterations at the time. That is,
    # each loop we process 5x2 = 10 complex numbers.
    # NOTE: in the various register representations in comments,
    # we only show the lanes that contain the first loop iteration.
    # The other lanes containt the second iteration, but we "hide" them
    # not to create more confusion.

kf_bfly5_loop:
    # vf0:   f0r -- -- -- f0i -- -- --
    lsv vf0h.e0, 0x00,s0
    lsv vf0h.e4, 0x02,s0
    lsv vf0l.e0, 0x04,s0
    lsv vf0l.e4, 0x06,s0
    lsv vf0h.e2, 0x08,s0
    lsv vf0h.e6, 0x0A,s0
    lsv vf0l.e2, 0x0C,s0
    lsv vf0l.e6, 0x0E,s0

    # vf12:   f1r f1i -- -- f2r f2i -- -- 
    # vf43:   f4r f4i -- -- f3r f3i -- --
    llv vf12h.e0, 0x00,s1
    llv vf12h.e4, 0x00,s2
    llv vf12l.e0, 0x04,s1
    llv vf12l.e4, 0x04,s2

    llv vf12h.e2, 0x08,s1
    llv vf12h.e6, 0x08,s2
    llv vf12l.e2, 0x0C,s1
    llv vf12l.e6, 0x0C,s2

    llv vf43h.e0, 0x00,s4
    llv vf43h.e4, 0x00,s3
    llv vf43l.e0, 0x04,s4
    llv vf43l.e4, 0x04,s3

    llv vf43h.e2, 0x08,s4
    llv vf43h.e6, 0x08,s3
    llv vf43l.e2, 0x0C,s4
    llv vf43l.e6, 0x0C,s3

    # Compute twiddles. Notice how, in the angle definitions,
    # we swapped cos/sin in the right half of the vector, so
    # imaginary and real parts end up swapped.
    #
    # vtwiddle1:        tw1r tw1i -- --    tw2i tw2r -- --
    # vtwiddle1inv:     tw1i tw1r -- --   -tw2r tw2i -- --
    # vtwiddle2:        tw3r tw3i -- --    tw4i tw4r -- --
    # vtwiddle2inv:     tw3i tw3r -- --   -tw4r tw4i -- --
    jal kf_twiddle_2
    nop

    # vk1m1:    1 -- -- --   -1 -- -- --
    # vk1m1.h0: 1  1  1  1   -1 -1 -1 -1
    li t0, 1
    mtc2 t0, vk1m1.e0
    li t0, -1
    mtc2 t0, vk1m1.e4

    # Invert sign on the right half of the vector. This is necessary
    # becuase the code in kf_twiddle_2 changed sign to tw2r/tw4r in the
    # inverted vectors, becase the real/imag parts were swapped. We need
    # the imag part with the negative sign, so just invert the right
    # side.
    # vtwiddle1:        tw1r tw1i -- --    tw2i  tw2r -- --
    # vtwiddle1inv:     tw1i tw1r -- --    tw2r -tw2i -- --
    # vtwiddle2:        tw3r tw3i -- --    tw4i  tw4r -- --
    # vtwiddle2inv:     tw3i tw3r -- --    tw4r -tw4i -- --
    vmudh vtwiddle1inv, vk1m1.h0
    vmudh vtwiddle2inv, vk1m1.h0

    # C_MUL(scratch[1] ,*Fout1, tw[u*fstride]);
    # C_MUL(scratch[2] ,*Fout2, tw[2*u*fstride]);
    # C_MUL(scratch[3] ,*Fout3, tw[3*u*fstride]);
    # C_MUL(scratch[4] ,*Fout4, tw[4*u*fstride]);
    # 
    # vf12:  s1r s1i -- --    s2i s2r -- --
    # vf43:  s4r s4i -- --    s4i s4r -- --
    vmudm v___,  vtwiddle1,    vf12l.q0
    vmadh v___,  vtwiddle1,    vf12h.q0
    vmadm v___,  vtwiddle1inv, vf12l.q1
    vmadh v___,  vtwiddle1inv, vf12h.q1
    vsar  vf12l, COP2_ACC_MD
    vsar  vf12h, COP2_ACC_HI

    vmudm v___,  vtwiddle2,    vf43l.q0
    vmadh v___,  vtwiddle2,    vf43h.q0
    vmadm v___,  vtwiddle2inv, vf43l.q1
    vmadh v___,  vtwiddle2inv, vf43h.q1
    vsar  vf43l, COP2_ACC_MD
    vsar  vf43h, COP2_ACC_HI

    # C_ADD( scratch[7],scratch[1],scratch[4]);
    # C_SUB( scratch[10],scratch[1],scratch[4]);
    # C_ADD( scratch[8],scratch[2],scratch[3]);
    # C_SUB( scratch[9],scratch[2],scratch[3]);
    #
    # vs78:   s7r s7i -- --    s8i s8r -- --
    # vs109:  s10r s10i -- --  s9i s9r -- --
    vaddc vs78l,  vf12l, vf43l
    vadd  vs78h,  vf12h, vf43h
    vsubc vs109l, vf12l, vf43l
    vsub  vs109h, vf12h, vf43h

    # recover 1 bit of precision lost in C_MUL,
    # and add 1 bit more to prepare for next multiplication
    vmudn vs78l,  K4
    vmadh vs78h,  K4
    vmudn vs109l, K4
    vmadh vs109h, K4

    # Rotate vectors
    #
    # vs87:  s8i s8r -- --    s7r s7i -- --
    # vs910: s9i s9r -- --    s10r s10i -- --
    sqv vs78l.e4,  0x00,tmp
    sqv vs78h.e4,  0x10,tmp
    sqv vs109l.e4, 0x20,tmp
    sqv vs109h.e4, 0x30,tmp
    lqv vs87l.e0,  0x00,tmp
    lqv vs87h.e0,  0x10,tmp
    lqv vs910l.e0, 0x20,tmp
    lqv vs910h.e0, 0x30,tmp

    # Fout0->r = ADD32_ovflw(Fout0->r, ADD32_ovflw(scratch[7].r, scratch[8].r));
    # Fout0->i = ADD32_ovflw(Fout0->i, ADD32_ovflw(scratch[7].i, scratch[8].i));
    #
    # Notice that vs78/vs87 have one excess bit of preparation (in
    # preperation for next multiplications), so we want to scale it
    # down here. We multiply by 0x8000 to achieve so, turning the
    # three additions into a MAC sequence. 
    #
    # vf0:      f0r  -- -- --    f0i  -- -- --
    # vs78:     s7r s7i -- --    s8i s8r -- --
    # vs87.q1:  s8r s8r -- --    s7i s7i -- --
    # -----------------------------------------
    # vout0:    f0r  -- -- --    f0i  -- -- --
    vcopy vout0h, K32768
    vmudl vout0l, vout0h, vs78l
    vmadn vout0l, vout0h, vs78h
    vmadl vout0l, vout0h, vs87l.q1
    vmadn vout0l, vout0h, vs87h.q1
    vmadn vout0l, vf0l, K1
    vmadh vout0h, vf0h, K1

    # scratch[5].r  = ADD32_ovflw(scratch[0].r, ADD32_ovflw(S_MUL(scratch[7].r,ya.r), S_MUL(scratch[8].r,yb.r)));
    # scratch[5].i  = ADD32_ovflw(scratch[0].i, ADD32_ovflw(S_MUL(scratch[7].i,ya.r), S_MUL(scratch[8].i,yb.r)));
    # scratch[11].r = ADD32_ovflw(scratch[0].r, ADD32_ovflw(S_MUL(scratch[7].r,yb.r), S_MUL(scratch[8].r,ya.r)));
    # scratch[11].i = ADD32_ovflw(scratch[0].i, ADD32_ovflw(S_MUL(scratch[7].i,yb.r), S_MUL(scratch[8].i,ya.r)));
    #
    # vs78.q0:  s7r s7r -- --    s8i s8i -- --
    # yconst1:  yar ybr -- --    yar ybr -- --
    # vs87.q1:  s8r s8r -- --    s7i s7i -- --
    # yconst2:  ybr yar -- --    ybr yar -- --
    # vf0.q0:   f0r f0r -- --    f0i f0i -- --
    # ------------------------------------------
    # vs511:    s5r s11r -- --   s11i s5i -- --
    vmudm vs511l, yconst1, vs78l.q0
    vmadh vs511h, yconst1, vs78h.q0 
    vmadm vs511l, yconst2, vs87l.q1
    vmadh vs511h, yconst2, vs87h.q1 
    vsar vs511l, COP2_ACC_MD
    vsar vs511h, COP2_ACC_HI
    vaddc vs511l, vf0l.q0
    vadd  vs511h, vf0h.q0


    # scratch[6].r = ADD32_ovflw(S_MUL(scratch[10].i,ya.i), S_MUL(scratch[9].i,yb.i));
    # scratch[6].i = NEG32_ovflw(ADD32_ovflw(S_MUL(scratch[10].r,ya.i), S_MUL(scratch[9].r,yb.i)));
    # scratch[12].r = SUB32_ovflw(S_MUL(scratch[9].i,ya.i), S_MUL(scratch[10].i,yb.i));
    # scratch[12].i = SUB32_ovflw(S_MUL(scratch[10].r,yb.i), S_MUL(scratch[9].r,ya.i));
    #
    # vs109.q1:  s10i s10i -- --    s9r  s9r -- --
    # yconst3:   yai  -ybi -- --   -yai -ybi -- --
    # vs910.q0:  s9i  s9i  -- --    s10r s10r -- --
    # yconst4:   ybi  yai  -- --    ybi  -yai -- --
    # ---------------------------------------------
    # vs612:     s6r  s12r -- --   s12i s6i  -- --
    vmudm vs612l, yconst3, vs109l.q1
    vmadh vs612h, yconst3, vs109h.q1
    vmadm vs612l, yconst4, vs910l.q0
    vmadh vs612h, yconst4, vs910h.q0
    vsar vs612l, COP2_ACC_MD
    vsar vs612h, COP2_ACC_HI

    # C_SUB(*Fout1,scratch[5],scratch[6]);
    # C_ADD(*Fout4,scratch[5],scratch[6]);
    # C_ADD(*Fout2,scratch[11],scratch[12]);
    # C_SUB(*Fout3,scratch[11],scratch[12]);
    #
    # vout13:   f1r f3r -- -- f3i f1i -- --
    # vout42:   f4r f2r -- -- f2i f4i -- --
    vsubc vout13l, vs511l, vs612l
    vsub  vout13h, vs511h, vs612h
    vaddc vout42l, vs511l, vs612l
    vadd  vout42h, vs511h, vs612h

    ssv vout0h.e0, 0x00,s0
    ssv vout0h.e4, 0x02,s0
    ssv vout0l.e0, 0x04,s0
    ssv vout0l.e4, 0x06,s0
    ssv vout0h.e2, 0x08,s0
    ssv vout0h.e6, 0x0A,s0
    ssv vout0l.e2, 0x0C,s0
    ssv vout0l.e6, 0x0E,s0

    ssv vout13h.e0, 0x00,s1
    ssv vout13h.e5, 0x02,s1
    ssv vout13l.e0, 0x04,s1
    ssv vout13l.e5, 0x06,s1

    ssv vout13h.e1, 0x00,s3
    ssv vout13h.e4, 0x02,s3
    ssv vout13l.e1, 0x04,s3
    ssv vout13l.e4, 0x06,s3

    ssv vout42h.e0, 0x00,s4
    ssv vout42h.e5, 0x02,s4
    ssv vout42l.e0, 0x04,s4
    ssv vout42l.e5, 0x06,s4

    ssv vout42h.e1, 0x00,s2
    ssv vout42h.e4, 0x02,s2
    ssv vout42l.e1, 0x04,s2
    ssv vout42l.e4, 0x06,s2

    ssv vout13h.e2, 0x08,s1
    ssv vout13h.e7, 0x0A,s1
    ssv vout13l.e2, 0x0C,s1
    ssv vout13l.e7, 0x0E,s1

    ssv vout13h.e3, 0x08,s3
    ssv vout13h.e6, 0x0A,s3
    ssv vout13l.e3, 0x0C,s3
    ssv vout13l.e6, 0x0E,s3

    ssv vout42h.e2, 0x08,s4
    ssv vout42h.e7, 0x0A,s4
    ssv vout42l.e2, 0x0C,s4
    ssv vout42l.e7, 0x0E,s4

    ssv vout42h.e3, 0x08,s2
    ssv vout42h.e6, 0x0A,s2
    ssv vout42l.e3, 0x0C,s2
    ssv vout42l.e6, 0x0E,s2

    addiu s0, 16
    addiu s1, 16
    addiu s2, 16
    addiu s3, 16
    addiu s4, 16

    bgtz k, kf_bfly5_loop
    addiu k, -2*8

    bgtz fN, kf_bfly5_outer_loop
    addiu fN, -1

    jr ra2
    nop


    # SU: 24 loads + 40 stores + 8 rotation = 72
    # VU: 26 twiddle + 44 ops = 70

    .endfunc






/*
TO BE REMOVED once comments in bfly5 are done


#define C_MUL(m,a,b) \
    do{ (m).r = (a).r*(b).r - (a).i*(b).i;\
        (m).i = (a).r*(b).i + (a).i*(b).r; }while(0)

f0  =  s0r s0i -- -- s0r s0i -- --


fr.q0        =   f1r  f1r   NEXT  --  f2r  f2r  NEXT --
twiddl1      =  tw1r tw1i            tw2i tw2r

fi.q1        =   f1i  f1i   NEXT --   f2i  f2i 
twiddle1inv  = -tw1i tw1r           -tw2r tw2i


s12   =   s1r  s1i              s2i  s2r
s43   =   s4r  s4i              s3i  s3r
-
s78   =   s7r  s7i              s8i  s8r
+
s109  =   s10r s10i             s9i  s9r


    5r  11r         11i  5i  

    s0r s0r         s0i  s0i 
  +    
    s7r s7r         s8i  s8i 
  * yar ybr         yar  ybr 
  +    
    s8r s8r         s7i  s7i 
    ybr yar         ybr  yar 
  

    6r   12r        12i    6i  
                             
    s10i s10i       s9r    s9r 
    yai  -ybi       -yai   -ybi 
                                 
    s9i   s9i       s10r   s10r 
    ybi   yai        ybi   -yai 



f0  =  s0r -- -- --  s0i -- -- --
s78=   s7r s7i       s8i s8r
s87=   s8i s8r       s8r s7i


*/
