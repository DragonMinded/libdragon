#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

    .data

DUMMY: .long 0

    .text

#######################################################################
# KF_BFLY3 - 2-point FFT butterfly
#######################################################################

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

    bal kf_twiddle_2
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
