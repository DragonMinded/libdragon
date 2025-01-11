#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

    .data

DUMMY: .long 0

    .text

#######################################################################
# KF_BFLY4M1 - 4-point FFT butterfly (M=1 simple case)
#######################################################################

    #define vidx0i     $v01
    #define vidx0f     $v02
    #define vidx1i     $v03
    #define vidx1f     $v04
    #define vidx2i     $v05
    #define vidx2f     $v06
    #define vidx3i     $v07
    #define vidx3f     $v08

    #define vtmp1i     $v09
    #define vtmp1f     $v10
    #define vtmp2i     $v11
    #define vtmp2f     $v12
    #define vtmp3i     $v13
    #define vtmp3f     $v14

kf_bfly4_m1:
    addiu fN, -2
    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC

    # Process 2 iterations at a time. We need 16+16 SU for memory transfers
    # and we only have ~20 VUs to do, so this seems like a good compromise.
kf_bfly4_m1_loop:
    llv vidx0i.e0, 0x00,fZ
    llv vidx0f.e0, 0x04,fZ
    llv vidx0i.e2, 0x20,fZ
    llv vidx0f.e2, 0x24,fZ

    llv vidx1i.e0, 0x08,fZ
    llv vidx1f.e0, 0x0C,fZ
    llv vidx1i.e2, 0x28,fZ
    llv vidx1f.e2, 0x2C,fZ

    llv vidx2i.e0, 0x10,fZ
    llv vidx2f.e0, 0x14,fZ
    llv vidx2i.e2, 0x30,fZ
    llv vidx2f.e2, 0x34,fZ

    llv vidx3i.e0, 0x18,fZ
    llv vidx3f.e0, 0x1C,fZ
    llv vidx3i.e2, 0x38,fZ
    llv vidx3f.e2, 0x3C,fZ

    # C_SUB( scratch0 , *Fout, Fout[2] );
    vsubc vtmp1f, vidx0f, vidx2f
    vsub  vtmp1i, vidx0i, vidx2i

    # C_ADDTO(*Fout, Fout[2]);
    vaddc vidx0f, vidx2f
    vadd  vidx0i, vidx2i

    # C_ADD( scratch1 , Fout[1] , Fout[3] );
    vaddc vtmp2f, vidx1f, vidx3f
    vadd  vtmp2i, vidx1i, vidx3i

    # C_SUB( Fout[2], *Fout, scratch1 );
    vsubc vidx2f, vidx0f, vtmp2f
    vsub  vidx2i, vidx0i, vtmp2i

    # C_ADDTO( *Fout , scratch1 );
    vaddc vidx0f, vtmp2f
    vadd  vidx0i, vtmp2i

    # C_SUB( scratch1 , Fout[1] , Fout[3] );
    vsubc vtmp2f, vidx1f, vidx3f
    vsub  vtmp2i, vidx1i, vidx3i

    # Invert scratch2 real/imag and change sign of imag
    vsubc vtmp3f, vzero, vtmp2f.q1
    vsub  vtmp3i, vzero, vtmp2i.q1
    vmrg  vtmp2f, vtmp3f, vtmp2f.q0
    vmrg  vtmp2i, vtmp3i, vtmp2i.q0

    # Fout[1].r = ADD32_ovflw(scratch0.r, scratch1.i);
    # Fout[1].i = SUB32_ovflw(scratch0.i, scratch1.r);
    vsubc vidx1f, vtmp1f, vtmp2f
    vsub  vidx1i, vtmp1i, vtmp2i

    # Fout[3].r = SUB32_ovflw(scratch0.r, scratch1.i);
    # Fout[3].i = ADD32_ovflw(scratch0.i, scratch1.r);
    vaddc vidx3f, vtmp1f, vtmp2f
    vadd  vidx3i, vtmp1i, vtmp2i

    slv vidx0i.e0, 0x00,fZ
    slv vidx0f.e0, 0x04,fZ
    slv vidx0i.e2, 0x20,fZ
    slv vidx0f.e2, 0x24,fZ

    slv vidx1i.e0, 0x08,fZ
    slv vidx1f.e0, 0x0C,fZ
    slv vidx1i.e2, 0x28,fZ
    slv vidx1f.e2, 0x2C,fZ

    slv vidx2i.e0, 0x10,fZ
    slv vidx2f.e0, 0x14,fZ
    slv vidx2i.e2, 0x30,fZ
    slv vidx2f.e2, 0x34,fZ

    slv vidx3i.e0, 0x18,fZ
    slv vidx3f.e0, 0x1C,fZ
    slv vidx3i.e2, 0x38,fZ
    slv vidx3f.e2, 0x3C,fZ

    addiu fZ, 0x40

    bgtz fN, kf_bfly4_m1_loop
    addiu fN, -2

    jr ra
    nop
