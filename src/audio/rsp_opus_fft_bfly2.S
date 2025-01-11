#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

    .data

DUMMY: .long 0

    .text

#######################################################################
# KF_BFLY2 - 2-point FFT butterfly
#######################################################################

    #define vfh     $v01
    #define vfl     $v02
    #define vgh     $v03
    #define vgl     $v04
    #define vtmph   $v05
    #define vtmpl   $v06
    #define v___    $v07

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

    #undef vfh  
    #undef vfl  
    #undef vgh  
    #undef vgl  
    #undef vtmph
    #undef vtmpl
    #undef v___

#include <rsp_assert.inc>
