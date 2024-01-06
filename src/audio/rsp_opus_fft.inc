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

#include <rsp.inc>

    #define fmm                 t7
    #define fM                  t8
    #define fN                  t9
    #define fZ                  s7
    #define fTW                 t6
    #define fK                  t5

#######################################################################
# KF_BFLY2 - 2-point FFT butterfly
#######################################################################

    # 0x5a82 is QCONST16(0.7071067812f, 15); negative is 0xa57e
    #define KF_BFLY2_CONST1  \
        .half 0x7fff, 0x0000, 0x5a82, 0xa57e, 0x0000, 0x8000, 0xa57e, 0xa57e
    #define KF_BFLY2_CONST2  \
        .half 0x0000, 0x7fff, 0x5a82, 0x5a82, 0x7fff, 0x0000, 0x5a82, 0xa57e

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
        .half 0, 0, 0, 0, \
              __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
              __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N)
    #define KF_BFLY4_TWINCR2(stride, N)  \
        .half __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
              __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
              __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N), \
              __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N)


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

