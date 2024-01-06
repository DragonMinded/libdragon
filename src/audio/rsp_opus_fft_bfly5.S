#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

    .data

DUMMY: .long 0

    .text

#######################################################################
# KF_BFLY5 - 5-point FFT butterfly
#######################################################################

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

    #define tmp         t3
    #define k           t4

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
    bal kf_twiddle_2
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
