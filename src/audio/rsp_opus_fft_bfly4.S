#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

    .data

DUMMY: .long 0

    .text

#######################################################################
# KF_BFLY4 - 4-point FFT butterfly
#######################################################################

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
    #define tmp        t3
    #define j_idx      t4

    .func kf_bfly4
kf_bfly4:
    move ra2, ra
    addiu tmp, fTW, 0x40

    li t0, 2
    mtc2 t0, vk2.e0
    vcopy vk2, vk2.e0
    vcopy vk4010, vtwk1.e6

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

kf_bfly4_loop:
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

    bal kf_twiddle_2
    nop

    # C_MUL(scratch[0],Fout[m] , *tw1 );
    vmudm v___,  vtwiddle1inv, vidx0l.q1
    vmadh v___,  vtwiddle1inv, vidx0h.q1
    vmadm v___,  vtwiddle1,    vidx0l.q0
    vmadh v___,  vtwiddle1,    vidx0h.q0
    vsar  vidx1l, COP2_ACC_MD
    vsar  vidx1h, COP2_ACC_HI

    # C_MUL(scratch[1],Fout[m2] , *tw2 );
    # C_MUL(scratch[2],Fout[m3] , *tw3 );
    vmudm v___,  vtwiddle2inv, vidx2l.q1
    vmadh v___,  vtwiddle2inv, vidx2h.q1
    vmadm v___,  vtwiddle2,    vidx2l.q0
    vmadh v___,  vtwiddle2,    vidx2h.q0
    vsar  vidx2l, COP2_ACC_MD
    vsar  vidx2h, COP2_ACC_HI

    # Recover 1 bit of precision lost after CMUL
    vaddc vidx1l, vidx1l
    vadd  vidx1h, vidx1h
    vaddc vidx2l, vidx2l
    vadd  vidx2h, vidx2h

    # Rotate the vectors
    #           vidx0:  f0r f0i -- --   --  --  -- --
    # scratch0: vidx1:  s1r s1i -- --   --  --  -- --
    # scratch1: vidx2:  s2r s2i -- --   s3r s3i -- --
    # scratch2: vidx3:  s3r s3i -- --   --  --  -- --
    sqv vidx1h.e4, 0x00,tmp
    sqv vidx1l.e4, 0x10,tmp
    sqv vidx2h.e4, 0x20,tmp
    sqv vidx2l.e4, 0x30,tmp
    lqv vidx1h.e0, 0x00,tmp
    lqv vidx1l.e0, 0x10,tmp
    lqv vidx3h.e0, 0x20,tmp
    lqv vidx3l.e0, 0x30,tmp

    # C_SUB( scratch5 , *Fout, scratch[1] );
    vsubc vtmp1l, vidx0l, vidx2l
    vsub  vtmp1h, vidx0h, vidx2h

    # C_ADDTO(*Fout, scratch[1]);
    vaddc vidx0l, vidx2l
    vadd  vidx0h, vidx2h

    # C_ADD( scratch[3] , scratch[0] , scratch[2] );
    vaddc vtmp2l, vidx1l, vidx3l
    vadd  vtmp2h, vidx1h, vidx3h

    # C_SUB( scratch[4] , scratch[0] , scratch[2] );
    vsubc vtmp3l, vidx1l, vidx3l
    vsub  vtmp3h, vidx1h, vidx3h

    # C_SUB( Fout[m2], *Fout, scratch[3] );
    vsubc vidx2l, vidx0l, vtmp2l
    vsub  vidx2h, vidx0h, vtmp2h

    # C_ADDTO( *Fout , scratch[3] );
    vaddc vidx0l, vtmp2l
    vadd  vidx0h, vtmp2h

    # Invert scratch4 real/imag and change sign of imag
    vsubc vtmp2l, vzero, vtmp3l.q1
    vsub  vtmp2h, vzero, vtmp3h.q1
    vmrg  vtmp3l, vtmp2l, vtmp3l.q0
    vmrg  vtmp3h, vtmp2h, vtmp3h.q0

    # Fout[1].r = ADD32_ovflw(scratch5.r, scratch4.i);
    # Fout[1].i = SUB32_ovflw(scratch5.i, scratch4.r);
    vsubc vidx1l, vtmp1l, vtmp3l
    vsub  vidx1h, vtmp1h, vtmp3h

    # Fout[3].r = SUB32_ovflw(scratch5.r, scratch4.i);
    # Fout[3].i = ADD32_ovflw(scratch5.i, scratch4.r);
    vaddc vidx3l, vtmp1l, vtmp3l
    vadd  vidx3h, vtmp1h, vtmp3h

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

    addiu j_idx, -2
    bgtz j_idx, kf_bfly4_loop
    nop

    addu fZ, fmm

    addiu fN, -1
    bgtz fN, kf_bfly4_outer_loop
    nop

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