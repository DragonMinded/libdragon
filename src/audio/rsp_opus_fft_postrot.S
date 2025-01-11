#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

    .data

DUMMY: .long 0

    .text

#define fYP0    s0
#define fYP1    s1

#define vidx0h    $v01
#define vidx0l    $v02
#define vidx1h    $v03
#define vidx1l    $v04
#define vtmp      $v05

#define vreal0    $v06
#define vimag0    $v07
#define vreal1    $v08
#define vimag1    $v09

# Saturation for 32-bit signals, to avoid overflows at deemphasis time.
# See arch.h
#define SIG_SAT (300000000)

#######################################################################
# IMDCT Post-rotation
#######################################################################
# This is technically not part of the FFT proper but it's the final part
# of the IMDCT. Nonetheless, it fits very well as an overlay, as last FFT step.

imdct_postrot:
    move ra2, ra

    # Load upper part of SIG_SAT for saturation. We don't care eabout the low
    # part, we will use 0 for that, as the precise value doesn't matter much.
    li t0, SIG_SAT >> 16
    mtc2 t0, vtwk1.e3
    
    # Initialize twiddle constants
    li t0, 2
    mtc2 t0, vk2.e0
    vcopy vk2, vk2.e0
    vcopy vk4010, vtwk1.e6

    move fYP0, fZ
    sll t0, fN, 2
    addu fYP1, fYP0, t0
    addiu fYP1, -0x20

    srl fN, 1
    addiu fN, 7
    srl fN, 3

    lqv vtwidx1, 0x00,fTW
    lqv vtwidx2, 0x10,fTW

    # SU = 16 loads + 32 stores (?)
    # VU = 18 twiddle + 4 invert + 12 CMUL + 4 prec
loop:
    llv vidx0h.e0, 0x00,fYP0
    llv vidx0l.e0, 0x04,fYP0
    llv vidx0h.e2, 0x08,fYP0
    llv vidx0l.e2, 0x0C,fYP0
    llv vidx0h.e4, 0x10,fYP0
    llv vidx0l.e4, 0x14,fYP0
    llv vidx0h.e6, 0x18,fYP0
    llv vidx0l.e6, 0x1C,fYP0

    llv vidx1h.e6, 0x00,fYP1
    llv vidx1l.e6, 0x04,fYP1
    llv vidx1h.e4, 0x08,fYP1
    llv vidx1l.e4, 0x0C,fYP1
    llv vidx1h.e2, 0x10,fYP1
    llv vidx1l.e2, 0x14,fYP1
    llv vidx1h.e0, 0x18,fYP1
    llv vidx1l.e0, 0x1C,fYP1

    bal kf_twiddle_2
    nop

    li t0, 0xAAAA
    ctc2 t0, COP2_CTRL_VCC
    vsubc vtwiddle1inv, vzero, vtwiddle1.q0
    vsubc vtwiddle2inv, vzero, vtwiddle2.q0
    vmrg  vtwiddle1inv, vtwiddle1.q1
    vmrg  vtwiddle2inv, vtwiddle2.q1

    vmudm vtmp, vtwiddle1inv, vidx0l.q0
    vmadh vtmp, vtwiddle1inv, vidx0h.q0
    vmadm vtmp, vtwiddle1,    vidx0l.q1
    vmadh vtmp, vtwiddle1,    vidx0h.q1
    vsar vidx0l, COP2_ACC_MD
    vsar vidx0h, COP2_ACC_HI

    vmudm vtmp, vtwiddle2inv, vidx1l.q0
    vmadh vtmp, vtwiddle2inv, vidx1h.q0
    vmadm vtmp, vtwiddle2,    vidx1l.q1
    vmadh vtmp, vtwiddle2,    vidx1h.q1
    vsar vidx1l, COP2_ACC_MD
    vsar vidx1h, COP2_ACC_HI

    # Recover 1 bit of precision
    vaddc vidx0l, vidx0l
    vadd  vidx0h, vidx0h
    vaddc vidx1l, vidx1l
    vadd  vidx1h, vidx1h

    # Saturate to SIG_SAT, to avoid overflows during deemphasis
    vch vidx0h, vtwk1.e3
    vcl vidx0l, vzero
    vch vidx1h, vtwk1.e3
    vcl vidx1l, vzero

    # vidx0h: r0h i0h r1h i1h r2h i2h r3h i3h 
    # vidx0l: r0l i0l r1l i1l r2l i2l r3l i3l
    # we want to build:
    # vreal0: r0h r0l r1h r1l r2h r2l r3h r3l
    # vimag0: i0h i0l i1h i1l i2h i2l i3h i3l

    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC
    vmrg vreal0, vidx0h, vidx0l.q0
    vcopy vtmp, vidx0h.q1
    vmrg vimag0, vtmp, vidx0l.q1

    vmrg vreal1, vidx1h, vidx1l.q0
    vcopy vtmp, vidx1h.q1
    vmrg vimag1, vtmp, vidx1l.q1

    slv vreal0.e0, 0x00,fYP0
    slv vimag1.e0, 0x04,fYP0
    slv vreal0.e2, 0x08,fYP0
    slv vimag1.e2, 0x0C,fYP0
    slv vreal0.e4, 0x10,fYP0
    slv vimag1.e4, 0x14,fYP0
    slv vreal0.e6, 0x18,fYP0
    slv vimag1.e6, 0x1C,fYP0

    slv vreal1.e6, 0x00,fYP1
    slv vimag0.e6, 0x04,fYP1
    slv vreal1.e4, 0x08,fYP1
    slv vimag0.e4, 0x0C,fYP1
    slv vreal1.e2, 0x10,fYP1
    slv vimag0.e2, 0x14,fYP1
    slv vreal1.e0, 0x18,fYP1
    slv vimag0.e0, 0x1C,fYP1

    addiu fYP0, 0x20
    addiu fYP1, -0x20

    addiu fN, -1
    bnez fN, loop
    nop

    jr ra2
    nop
