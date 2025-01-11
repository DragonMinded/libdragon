#include "rsp_opus_fft.inc"
#include "rsp_opus_fft_twiddles.inc"

#define IMDCT_MAX_VALUES    960
#define COMPACT_CHUNK       (1024 - 960)     

    .data

DUMMY: .long 0

    .text

#######################################################################
# IMDCT Pre-rotation
#######################################################################
# This is technically not part of the FFT proper but it's the initial part
# of the IMDCT. Nonetheless, it fits very well as an overlay, as first FFT step.

    ############################################################################
    # PRE-ROTATION:
    # Step 2: scramble in bitrev order and multiply by twiddles
    #
    # The RDRAM work buffer now contains the input values deinterleaved for
    # each band, and rotated so that the odd-indexed values have been lienarized.
    #
    # We now need to perform two operations:
    #   * Fetch the values again into DMEM, rearranging them in bitrev order.
    #     This step will leave them in DMEM for the FFT to process.
    #   * Multiply the values by the twiddle factors. Since there is no way
    #     to store precalculated factors in DMEM, we compute them on the
    #     fly using a 4th-order cosine approximation.
    #
    # The cosine approximation has been measured to provide a RMSD of 45 
    # (0.06% error) for the range [0, π/2], compared to the ideal 16-bit
    # value the C Opus reference precalculates.
    #
    # INPUT:
    #   t6: work buffer in RDRAM contained deinterleave/rotated values
    #   t7: bitrev array in RDRAM
    #
    ############################################################################

    .text

    #define vxp1i           $v02
    #define vxp1f           $v03
    #define vxp2i           $v04
    #define vxp2f           $v05
    #define vyrf            $v06
    #define vyri            $v07
    #define vyif            $v08
    #define vyii            $v09
    #define vtrig1          $v10
    #define vtrig2          $v11
    #define vbitrev0        $v12
    #define vbitrev1        $v13
    #define vbitrev2        $v14
    #define vbitrev3        $v15

    #define vyrf_prev       $v16
    #define vyri_prev       $v17
    #define vyif_prev       $v18
    #define vyii_prev       $v19

    #define vcarry1         $v20
    #define vcarry2         $v21

    #define vx12            $v22
    #define vx22            $v23
    #define vx3l            $v24
    #define vx3h            $v25
    #define vtmp1i          $v22
    #define vtmp1f          $v23
    #define vtmp2i          $v24
    #define vtmp2f          $v25
    #undef vk4010
    #define vk4010          $v26

    #define in_dmem_start   s3
    #define in_dmem         s4
    #define bitrev_rdram    t7 
    #define in_rdram        t6
    #define samples         v0
    #define chunk           v1

OPUS_imdct_prerot_step2:
    move ra2, ra

    # Load input constants for twiddle calculations.
    lqv vtwidx1,  0x00,fTW
    lsv vtwk1.e0, 0x10,fTW
    vor vtwidx2, vtwidx1, vtwk1.e7

    # Put IMDCT_DATA address (destination buffer) into vtwk1.e1, which will
    # be used as base for the bitrev offsets
    mtc2 fZ, vtwk1.e1

    andi in_dmem_start, fTW, 0xFFF
    move bitrev_rdram, a3
    move in_rdram, a0
    move fN, k1

    vcopy vk4010, vtwk1.e6
    bal DMAWaitIdle
    srl samples, fN, 2

    #ifdef RSPQ_DEBUG
    andi t0, samples, 3
    assert_eq t0, 0, 0x8502
    #endif

OPUS_imdct_prerot_step2_loop8:
    move in_dmem, in_dmem_start
    li t0, DMA_SIZE(COMPACT_CHUNK/2 * 2, 1)
    bal DMAIn
    move s0, bitrev_rdram
    addiu bitrev_rdram, COMPACT_CHUNK/2 * 2

    lqv vbitrev0, 0x00,in_dmem
    lqv vbitrev1, 0x10,in_dmem
    lqv vbitrev2, 0x20,in_dmem
    lqv vbitrev3, 0x30,in_dmem

    move in_dmem, in_dmem_start
    li t0, DMA_SIZE(COMPACT_CHUNK/2 * 8, 1)
    bal DMAIn
    move s0, in_rdram

    move chunk, samples
    ble chunk, COMPACT_CHUNK/2, OPUS_imdct_prerot_step2_loop_start
    addiu in_rdram, COMPACT_CHUNK/2 * 8

    li chunk, COMPACT_CHUNK/2

    # C = 7058 = 0x1b92
    # B = -19930 = 0xb226
    # (x3l*C + ((x3h*C)<<16) + ((x2*B)<<16) + ((0x4000*0x8000)<<16)) >> 30
    #
    # Input vx in [0, π) => [-32768, 32767]
    # vk4000 = [0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000 ]
    # vconst = [0x4000, 0xc000, 0xb244, 0x4dbc, 0x1b78, 0xe488, ...]

OPUS_imdct_prerot_step2_loop_start:
    sub samples, chunk

    # Initialize t0-t9 to scratch memory, so that the stores in the first
    # loop become NOPs.
    move t0, in_dmem; move t1, in_dmem; move t2, in_dmem; move t3, in_dmem
    move t4, in_dmem; move t5, in_dmem; move t8, in_dmem; move t9, in_dmem

    # Main loop: process 8 complex values at a time.
    # The loop will run only 4 times unfortuantely because of the short
    # IMDCT_TMP buffer that only holds 32 complex values (256 bytes).
    
    # Timing:
    # 54 cyles (with 0 stalls!) for 8 complex values
    # Plus 32 cycles of epilogue.
    # total 248 cycles / 32 values = 7.75 cycles/value
    .align 3
OPUS_imdct_prerot_step2_loop:
    # Load 8 complexes at a time:           # Calculate 16 twiddles. We know that the first 8 twiddles
    # 8 real + 8 imag                       # are angles in [0..π/2], and the second 8 are in [π/2..π].
                                            # This approximation return abs(cos(x)) but we can deduce the sign.
    ldv vxp1i.e0, 0x00,in_dmem;             vmulf vx12, vtwidx1, vtwidx1
    ldv vxp1f.e0, 0x10,in_dmem;             vmulf vx22, vtwidx2, vtwidx2
                                            # Increment twiddle angles
    ldv vxp2i.e0, 0x08,in_dmem;             vaddc vtwidx1, vtwk1.e0
    ldv vxp2f.e0, 0x18,in_dmem;             vaddc vtwidx2, vtwk1.e0
                                            # Calculate 16 cosines (2x8)
    ldv vxp1i.e4, 0x20,in_dmem;             vmudm vx3h, vx12, vx12
    ldv vxp1f.e4, 0x30,in_dmem;             vmudn vtrig1, vx12, vtwk1.e2
    ldv vxp2i.e4, 0x28,in_dmem;             vmadn vtrig1, vx12, vtwk1.e2
    ldv vxp2f.e4, 0x38,in_dmem;             vmacf vtrig1, vk4010, vtwk1.e7                                            
    # Increment counters/pointers.
    # (also prevent store/load stall)
    addiu in_dmem, 64;                      vmacf vtrig1, vx3h, vtwk1.e4
    addiu chunk, -8;                        vmudm vx3h, vx22, vx22
        
    # Store previous loop result
    # On the first loop, this is
    # NOP, because t0-t9 contain
    # in_dmem which can be corrupted.
    ssv vyii_prev.e0, 0,t0;                 vmudn vtrig2, vx22, vtwk1.e2
    ssv vyri_prev.e0, 2,t0;                 vmadn vtrig2, vx22, vtwk1.e2
    ssv vyif_prev.e0, 4,t0;                 vmacf vtrig2, vk4010, vtwk1.e7
    ssv vyrf_prev.e0, 6,t0;                 vmacf vtrig2, vx3h, vtwk1.e4
                                            
                                            # Double xp1/xp2 in preparation for 
                                            # 32-bit multiplication by Q15.
    ssv vyii_prev.e1, 0,t1;                 vaddc vxp1f, vxp1f
    ssv vyri_prev.e1, 2,t1;                 vadd  vxp1i, vxp1i                  
    ssv vyif_prev.e1, 4,t1;                 vaddc vxp2f, vxp2f
    ssv vyrf_prev.e1, 6,t1;                 vadd  vxp2i, vxp2i 
                                            # The calculated cosines are Q14. We need Q15.
                                            # Also this might change signed into unsigned,
                                            # but is correct because of the way the cosine approx
                                            # formula works (subtracting 0x4010_0000 is a trick).
    ssv vyii_prev.e2, 0,t2;                 vmudn vtrig1, K2
    ssv vyri_prev.e2, 2,t2;                 vmudn vtrig2, K2
                                            # Prepare bitrev offsets.
    ssv vyif_prev.e2, 4,t2;                 vsll vbitrev0, vbitrev0, 3  # FIXME: remove
    ssv vyrf_prev.e2, 6,t2;                 vaddc vbitrev0, vtwk1.e1    # FIXME: remove    
                                            # YR = XP2 * TRIG1
    ssv vyii_prev.e3, 0,t3;                 vmudl vyrf, vxp2f, vtrig1
    ssv vyri_prev.e3, 2,t3;                 vmadm vyri, vxp2i, vtrig1
    ssv vyif_prev.e3, 4,t3;                 vmadn vyrf, vzero, vzero
                                            # TMP1 = XP1 * TRIG2
                                            # NOTE: trig2 is sign-reversed
    ssv vyrf_prev.e3, 6,t3;                 vmudl vtmp1f, vxp1f, vtrig2
    # Store bitrev0 into scratch memory,        
    # so that we can later read it via lhu.     
    # Note that 8 mfc2s would create lots       
    # of stalls, so we avoid those.         
    sqv vbitrev0, -0x10,in_dmem;            vmadm vtmp1i, vxp1i, vtrig2
    # Continue storing previous
    # loop results.
    ssv vyii_prev.e4, 0,t4;                 vmadn vtmp1f, vzero, vzero

                                            # YI = XP1 * TRIG1
    ssv vyri_prev.e4, 2,t4;                 vmudl vyif, vxp1f, vtrig1
    
    ssv vyif_prev.e4, 4,t4;                 vmadm vyii, vxp1i, vtrig1
    ssv vyrf_prev.e4, 6,t4;                 vmadn vyif, vzero, vzero
                                            # TMP2 = XP2 * TRIG2
                                            # NOTE: trig2 is sign-reversed
    ssv vyii_prev.e5, 0,t5;                 vmudl vtmp2f, vxp2f, vtrig2
    ssv vyri_prev.e5, 2,t5;                 vmadm vtmp2i, vxp2i, vtrig2
    ssv vyif_prev.e5, 4,t5;                 vmadn vtmp2f, vzero, vzero
                                            # YR = XP2 * TRIG1 + XP1 * TRIG2
                                            # We use vsub here because TRIG2 is reverse-signed
    ssv vyrf_prev.e5, 6,t5;                 vsubc vyrf, vyrf, vtmp1f
    ssv vyii_prev.e6, 0,t8;                 vsub vcarry1, vzero, vzero
    ssv vyri_prev.e6, 2,t8;                 vsubc vyri, vyri, vtmp1i
                                            # YI = XP1 * TRIG1 - XP2 * TRIG2
                                            # We use vadd here because TRIG2 is reverse-signed
    ssv vyif_prev.e6, 4,t8;                 vaddc vyif, vyif, vtmp2f
    ssv vyrf_prev.e6, 6,t8;                 vadd vcarry2, vzero, vzero
    ssv vyii_prev.e7, 0,t9;                 vaddc vyii, vyii, vtmp2i
                                            # Shift bitrev registers up
    ssv vyri_prev.e7, 2,t9;                 vcopy vbitrev0, vbitrev1
    ssv vyif_prev.e7, 4,t9;                 vcopy vbitrev1, vbitrev2
    ssv vyrf_prev.e7, 6,t9;                 vcopy vbitrev2, vbitrev3

    # Load bitrev offsets for next          # Complete 32-bit operation by manually adding
    # loop.                                 # the carry (so that we ignore overflows)
    lhu t0, -0x10(in_dmem);                 vaddc vyri, vcarry1
    lhu t1, -0x0E(in_dmem);                 vaddc vyii, vcarry2
                                            # Copy values to prev register for next loop
    lhu t2, -0x0C(in_dmem);                 vcopy vyrf_prev, vyrf
    lhu t3, -0x0A(in_dmem);                 vcopy vyif_prev, vyif
    lhu t4, -0x08(in_dmem);                 vcopy vyri_prev, vyri
    lhu t5, -0x06(in_dmem);                 vcopy vyii_prev, vyii
    lhu t8, -0x04(in_dmem);                 
    bgtz chunk, OPUS_imdct_prerot_step2_loop
    lhu t9, -0x02(in_dmem);                 

OPUS_imdct_prerot_step2_loop_epilogue:
    # Store last loop's results. Notice that the in case of IMDCT 120, the loop
    # must copy only 30 numbers, which is not a multiple of 8. Given that bitrev
    # will contain OOB values, we must avoid writing those values.
    ssv vyii_prev.e0, 0,t0; ssv vyri_prev.e0, 2,t0; ssv vyif_prev.e0, 4,t0; ssv vyrf_prev.e0, 6,t0;
    ssv vyii_prev.e1, 0,t1; ssv vyri_prev.e1, 2,t1; ssv vyif_prev.e1, 4,t1; ssv vyrf_prev.e1, 6,t1;
    ssv vyii_prev.e2, 0,t2; ssv vyri_prev.e2, 2,t2; ssv vyif_prev.e2, 4,t2; ssv vyrf_prev.e2, 6,t2;
    ssv vyii_prev.e3, 0,t3; ssv vyri_prev.e3, 2,t3; ssv vyif_prev.e3, 4,t3; 
    bltz chunk, OPUS_imdct_prerot_step2_loop_exit
    ssv vyrf_prev.e3, 6,t3;

    ssv vyii_prev.e4, 0,t4; ssv vyri_prev.e4, 2,t4; ssv vyif_prev.e4, 4,t4; ssv vyrf_prev.e4, 6,t4;
    ssv vyii_prev.e5, 0,t5; ssv vyri_prev.e5, 2,t5; ssv vyif_prev.e5, 4,t5; ssv vyrf_prev.e5, 6,t5;
    ssv vyii_prev.e6, 0,t8; ssv vyri_prev.e6, 2,t8; ssv vyif_prev.e6, 4,t8; ssv vyrf_prev.e6, 6,t8;
    ssv vyii_prev.e7, 0,t9; ssv vyri_prev.e7, 2,t9; ssv vyif_prev.e7, 4,t9; ssv vyrf_prev.e7, 6,t9;

OPUS_imdct_prerot_step2_loop_exit:
    bgtz samples, OPUS_imdct_prerot_step2_loop8
    nop

    jr ra2
    nop


    #undef vxp1i        
    #undef vxp1f        
    #undef vxp2i        
    #undef vxp2f        
    #undef vyrf         
    #undef vyri         
    #undef vyif         
    #undef vyii         
    #undef vtrig1       
    #undef vtrig2       
    #undef vbitrev0     
    #undef vbitrev1     
    #undef vbitrev2     
    #undef vbitrev3     

    #undef vyrf_prev    
    #undef vyri_prev    
    #undef vyif_prev    
    #undef vyii_prev    

    #undef vcarry1      
    #undef vcarry2      

    #undef vx12         
    #undef vx22         
    #undef vx3l         
    #undef vx3h         
    #undef vtmp1i       
    #undef vtmp1f       
    #undef vtmp2i       
    #undef vtmp2f       
    #undef vk4010       

    #undef in_dmem    
    #undef bitrev_rdram
    #undef in_rdram   
    #undef samples    
    #undef chunk      


#include <rsp_dma.inc>
