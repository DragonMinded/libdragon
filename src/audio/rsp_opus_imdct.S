#include <rsp_queue.inc>

#define stall    nop
#define vstall   vnop

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand OPUS_imdct, 20                # 0x0
    RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RSP OPUS1"
    .ascii " Coded by Rasky "

    .align 4
    RSPQ_BeginSavedState
DUMMY:    .long 0
    RSPQ_EndSavedState

    #define B            k0
    #define N            k1
    #define SHIFT        fp
    #define fft_steps_rdram    s6

    #define vtwidx1      $v27
    #define vtwidx2      $v28
    #define vtwk1        $v29
    
    .data

    .align 4
TWIDDLE_CONST1: .half 0         # used to store twiddle angle increment
                .half 0         # used to store %lo(IMDCT_DATA)
                .half 0xb226    # B constant
                .half 0         # empty slot
                .half 0x1b92    # C constant
                .half 0x4000    # Sine offset (only for 360° cosine variant)
                .half 0x4010    # D1 constant
                .half 0x8000    # D2 constant
TWIDDLE_IV_1920:.half 0x0009, 0x004d, 0x0091, 0x00d5, 0x011a, 0x015e, 0x01a2, 0x01e6
TWIDDLE_IV_240: .half 0x0044, 0x0266, 0x0489, 0x06ab, 0x08cd, 0x0aef, 0x0d11, 0x0f33

    .section .bssovl1

#define IMDCT_MAX_VALUES    960
#define COMPACT_CHUNK       (1024 - 960)     

    .align 4
IMDCT_DATA:  .space IMDCT_MAX_VALUES*4
IMDCT_TMP:   .space COMPACT_CHUNK*4


    .text

    ############################################################################
    # PRE-ROTATION:
    # Step 1: compact (deinterleave) the input bands.
    #
    # We fetch all the input values in DMEM (we have already backed it up,
    # so we can assume the whole DMEM is available). The maximum frame we handle
    # is 960 samples, so the input data to IMDCT is 480 32-bit values.
    #
    # The input 32-bit values are arranged interleaved between bands in couples,
    # like this:
    #
    #   A0 A1 B0 B1 C0 C1 D0 D1 A2 A3 B2 B3 ....
    #
    # Moreover, in addition to pre-interleaving, we also "rotate" the data in
    # preparation for FFT. So we consider the data as if the odd-indexed values
    # are in reverse order. For instance, if there are 64 samples per band
    # (A0..A63, B0..B63, etc.), we treat the input data as thus:
    #
    #   A0 A63 B0 B63 C0 C63 D0 D63 A2 A61 B2 B61 .... A62 A1 B62 B1 C62 C1 D62 D1
    #
    ############################################################################
    #
    # Input:
    #    a0: input buffer in RDRAM
    #    t6: output buffer in RDRAM where deinterleaved values will be stored
    #
    ############################################################################

    #define out_rdram       t6
    #define in_rdram        a0
    #define in_dmem1        s1
    #define in_dmem2        s2
    #define out_dmem        s3
    #define stride          v0
    #define samples         v1
    #define chunk           t7
    #define in_dmem_step    t8
    #define cur_band        t9
    #define vinf            $v01
    #define vini            $v02
    #define vin1            $v03
    #define vin2            $v04
    #define vprev1          $v05
    #define vprev2          $v06


    .func OPUS_imdct_prerot_compact
OPUS_imdct_prerot_compact:
    #ifdef RSPQ_DEBUG
    andi t0, in_rdram, 7
    assert_eq t0, 0, 0x8500
    #endif
    move ra2, ra

    # Convert input bands to a byte stride
    sll stride, B, 2
    sll in_dmem_step, stride, 1
    li cur_band, 0

    # Compute stride*(N/2), which is the number of bytes we will have to fetch
    mtc2 stride, $v01.e0
    srl t0, N, 1
    mtc2 t0, $v02.e0
    vmudh $v01, $v02
    mfc2 t0, $v01.e0
    assert_le t0, IMDCT_MAX_VALUES*4, 0x8501

    move s0, in_rdram
    addiu t0, -1
    jal DMAIn
    li s4, %lo(IMDCT_DATA)

OPUS_imdct_prerot_compact_band:
    sll t0, cur_band, 3
    addiu in_dmem1, t0, %lo(IMDCT_DATA)
    addiu in_dmem2, t0, %lo(IMDCT_DATA)
    srl t0, N, 1      # N/2
    addiu t0, -1      # N/2-1 => offset of the last group of samples
    sll t0, 2         # (N/2-1)*4 => byte offset
    addiu t1, B, -1

1:  addu in_dmem2, t0
    bgtz t1, 1b
    addiu t1, -1

    li out_dmem, %lo(IMDCT_TMP)
    srl samples, N, 1

    # Loop prologue
    llv vin1.e0, 0,in_dmem1;    addu in_dmem1, in_dmem_step;
    llv vin1.e2, 0,in_dmem1;    addu in_dmem1, in_dmem_step;
    llv vin1.e4, 0,in_dmem1;    addu in_dmem1, in_dmem_step;
    llv vin1.e6, 0,in_dmem1;    addu in_dmem1, in_dmem_step;
    llv vin2.e0, 0,in_dmem2;    subu in_dmem2, in_dmem_step;
    llv vin2.e2, 0,in_dmem2;    subu in_dmem2, in_dmem_step;
    llv vin2.e4, 0,in_dmem2;    subu in_dmem2, in_dmem_step;
    llv vin2.e6, 0,in_dmem2;    subu in_dmem2, in_dmem_step;
    vcopy vprev1, vin1
    vcopy vprev2, vin2

OPUS_imdct_prerot_compact_chunk:
    move chunk, samples
    ble chunk, COMPACT_CHUNK, 1f
    nop
    li chunk, COMPACT_CHUNK
1:  sub samples, chunk

    # Deinterelave and rotate hot-loop. To strike a balance between SU and VU,
    # we read each 32-bit value via llv, and then we swizzle it into high/low
    # halves using vmov. We use two pointers: one from the start of the input
    # buffer and one from the end, so that we also rotate the data as we go.
    # Cycles: 22 cycles / 8 32-bit values = 2.75 cycles/value
    # Biggest frame: 960 values => 2640 cycles
    .align 3
OPUS_imdct_prerot_compact_loop:
    llv vin1.e0, 0,in_dmem1;        vmov vini.e0, vprev1.e0
    addu in_dmem1, in_dmem_step;    vmov vini.e1, vprev1.e2
    llv vin1.e2, 0,in_dmem1;        vmov vini.e2, vprev1.e4
    addu in_dmem1, in_dmem_step;    vmov vini.e3, vprev1.e6
    llv vin1.e4, 0,in_dmem1;        vmov vini.e4, vprev2.e0
    addu in_dmem1, in_dmem_step;    vmov vini.e5, vprev2.e2
    llv vin1.e6, 0,in_dmem1;        vmov vini.e6, vprev2.e4
    addu in_dmem1, in_dmem_step;    vmov vini.e7, vprev2.e6
    llv vin2.e0, 0,in_dmem2;        vmov vinf.e0, vprev1.e1
    subu in_dmem2, in_dmem_step;    vmov vinf.e1, vprev1.e3
    llv vin2.e2, 0,in_dmem2;        vmov vinf.e2, vprev1.e5
    subu in_dmem2, in_dmem_step;    vmov vinf.e3, vprev1.e7
    llv vin2.e4, 0,in_dmem2;        vmov vinf.e4, vprev2.e1
    subu in_dmem2, in_dmem_step;    vmov vinf.e5, vprev2.e3
    llv vin2.e6, 0,in_dmem2;        vmov vinf.e6, vprev2.e5
    subu in_dmem2, in_dmem_step;    vmov vinf.e7, vprev2.e7

    addiu chunk, -8
    addiu out_dmem, 0x20

    sqv vini, -0x20,out_dmem;       vcopy vprev1, vin1
    sqv vinf, -0x10,out_dmem;       vcopy vprev2, vin2

    bgtz chunk, OPUS_imdct_prerot_compact_loop
    nop

    mtc0 out_rdram, COP0_DMA_RAMADDR
    li out_dmem, %lo(IMDCT_TMP)
    andi out_dmem, 0xFFF
    mtc0 out_dmem, COP0_DMA_SPADDR
    li t0, COMPACT_CHUNK*4  # FIXME
    addu out_rdram, t0
    addiu t0, -1
    mtc0 t0, COP0_DMA_WRITE
1:  mfc0 t0, COP0_DMA_BUSY
    bnez t0, 1b
    nop

    bgtz samples, OPUS_imdct_prerot_compact_chunk
    nop

    addiu cur_band, 1
    bne cur_band, B, OPUS_imdct_prerot_compact_band
    nop

    jr ra2
    nop
    .endfunc

    #undef out_rdram      
    #undef in_rdram       
    #undef in_dmem1       
    #undef in_dmem2       
    #undef stride         
    #undef samples        
    #undef chunk
    #undef in_dmem_step   
    #undef cur_band       
    #undef vinf           
    #undef vini           
    #undef vin1           
    #undef vin2           
    #undef vprev1         
    #undef vprev2         

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
    #define vk4010          $v26

    #define in_dmem         s4
    #define bitrev_rdram    t7 
    #define in_rdram        t6
    #define samples         v0
    #define chunk           v1


    .func OPUS_imdct_prerot_step2
OPUS_imdct_prerot_step2:
    move ra2, ra

    vcopy vk4010, vtwk1.e6
    jal DMAWaitIdle
    srl samples, N, 2

    #ifdef RSPQ_DEBUG
    andi t0, samples, 3
    assert_eq t0, 0, 0x8502
    #endif

OPUS_imdct_prerot_step2_loop8:
    li in_dmem, %lo(IMDCT_TMP)
    andi in_dmem, 0xFFF
    li t0, DMA_SIZE(COMPACT_CHUNK/2 * 2, 1)
    jal DMAIn
    move s0, bitrev_rdram
    addiu bitrev_rdram, COMPACT_CHUNK/2 * 2

    lqv vbitrev0, 0x00,in_dmem
    lqv vbitrev1, 0x10,in_dmem
    lqv vbitrev2, 0x20,in_dmem
    lqv vbitrev3, 0x30,in_dmem

    li in_dmem, %lo(IMDCT_TMP)
    andi in_dmem, 0xFFF
    li t0, DMA_SIZE(COMPACT_CHUNK/2 * 8, 1)
    jal DMAIn
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

    .endfunc

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


    ############################################################################
    # FFT butterfly functions
    ############################################################################

#include "rsp_opus_fft.inc"

    ############################################################################
    # OPUS_imdct
    #
    # Input values:
    #   a0:  0..23: Input buffer of frequency values
    #   a1:  0..11: "N": size of the IMDCT, minus 1, eg: 1920. This refers to
    #               the output, so input values will be N/2.
    #       12..15: "B": number of interleaved frequency bands, minus 1
    #       16..19: "shift": reduction factor for the FFT. Normally, this is
    #               log2(B), but for genericity it is left as separate argument.
    #   a2:  0..23: RDRAM work buffer (must be 1920+4096 bytes)
    #   a3:  0..23: RDRAM Bitrev array
    #     :  0..23: RDRAM pointer to FFT steps table
    #
    ############################################################################

    .func OPUS_imdct
OPUS_imdct:
    lw fft_steps_rdram, CMD_ADDR(16, 20)

    # Backup DMEM. We are going to need the whole DMEM, so let's backup
    # common data, and we will restore it later.
    # FIXME: try using _ovl_data_start as number of bytes. We probably don't
    # need more than that? Unless we use some state in rsp_opus.
    addiu s0, a2, 3840
    li s4, 0
    jal DMAOutAsync
    li t0, DMA_SIZE(4096, 1)

    # Extract input parameters into the global registers
    andi N, a1, 0xFFF
    addiu N, 1
    srl B, a1, 12
    andi B, 0xF
    addiu B, 1
    srl SHIFT, a1, 16
    andi SHIFT, 0xF

    # Keep N as pre-scaled by shift as it normally refers to the actual
    # number of values we are going to process.
    srlv N, N, SHIFT

    # Load constants for twiddle calculations, now that the data segment is
    # still intact.
    li t1, %lo(TWIDDLE_CONST1)
    lqv vtwk1,  0x00,t1
    li t0, %lo(IMDCT_DATA)
    mtc2 t0, vtwk1.e1

    # Setup twiddle parameters for N=1920 and N=240
    # FIXME: compute these in C and pass them to RSP
    beq N, 1920, OPUS_imdct_setup_1920
    nop
    beq N, 240, OPUS_imdct_setup_240
    nop
    assert 0x8555

OPUS_imdct_setup_1920:
    li t0, 0x222
    mtc2 t0, vtwk1.e0
    j OPUS_imdct_start
    lqv vtwidx1, 0x10,t1
OPUS_imdct_setup_240:
    li t0, 0x1111
    mtc2 t0, vtwk1.e0
    j OPUS_imdct_start
    lqv vtwidx1, 0x20,t1
    
OPUS_imdct_start:
    # From now on, we assume that DMEM contains only two buffers:
    #  IMDCT_DATA: holding all the values/samples for the IMDCT
    #  IMDCT_TMP:  holding instructions for the current FFT step.
    # We can't load from DMEM anymore other constants.

    # Do deinterleaving of input data. The value will be fetched
    # from RDRAM, deinterleaved, and then moved again to RDRAM into the
    # work buffer.
    vor vtwidx2, vtwidx1, vtwk1.e7
    jal OPUS_imdct_prerot_compact
    move t6, a2

OPUS_imdct_band:
    # Fetch the data from RDRAM work buffer for the current band, and finish
    # pre-rotation by applying bitrev order and multiplying with twiddles.
    # The data is now kept in DMEM as it's ready for the FFT.
    move t7, a3
    jal OPUS_imdct_prerot_step2
    move t6, a2

    # Fetch first step of the cmd table
    move s0, fft_steps_rdram
OPUS_imdct_fft_loop:
    li s4, %lo(IMDCT_TMP)
    andi s4, 0xFFF
    jal DMAIn
    li t0, DMA_SIZE(152, 1)

    # First the current FFT function overlay
    li s1, %lo(IMDCT_TMP)
    andi s1, 0xFFF
    lw s0, 132(s1)            # FFT function pointer in RDRAM
    li s4, %lo(FFT_OVERLAY_FUNC)
    jal DMAIn
    li t0, DMA_SIZE(0x400, 1)

    # Load the arguments and run the overlay
    li fZ, %lo(IMDCT_DATA)
    li fTW, %lo(IMDCT_TMP)
    lw fM, 140(s1)
    lw fN, 144(s1)
    jal FFT_OVERLAY_FUNC
    lw fmm, 148(s1)

    # Check if there's another step
    li s1, %lo(IMDCT_TMP)
    andi s1, 0xFFF
    lw s0, 128(s1)
    bnez s0, OPUS_imdct_fft_loop
    nop

    # # All the values are now in DMEM, ready for the FFT.
    # li t0, 1920
    # beq N, t0, OPUS_imdct_fft_1920_0
    # li t0, 240
    # beq N, t0, OPUS_imdct_fft_1920_3
    # nop
    # xassert 0x8555

OPUS_imdct_fft_end:
    # Now do the post-rotation and deshuffling.
    #jal OPUS_imdct_postrot
    nop

    #### DEBUG
    addiu s0, a2, 3840+4096
    li s4, %lo(IMDCT_DATA)
    jal DMAOut
    li t0, IMDCT_MAX_VALUES*4 - 1
    #### DEBUG

    # Restore DMEM and exit
    addiu s0, a2, 3840
    li s4, 0
    li t0, DMA_SIZE(4096, 1)
    jal_and_j DMAIn, RSPQ_Loop

    .endfunc





























































































#if 0
    .align 4
#TWIDDLE_CONST: .half 0x4000, 0xc000, 0xb244, 0x4dbc, 0x1b78, 0xe488
TWIDDLE_CONST:  .half 0
                .half 0
                .half 0xb226    # B constant
                .half 0
                .half 0x1b92    # C constant
                .half 0
                .half 0x4000
                .half 0x8000
    .align 4
TWIDDLE_CONST2: .half 2,-2, 2,-2, 2,-2, 2,-2

    .text

    #define N2   t9

    #define vx      $v20
    #define vy      $v21
    #define vx2     $v22
    #define vx3l    $v23
    #define vx3h    $v24
    #define vcos    $v25
    #define vconst1 $v26
    #define vconst2 $v27
    #define vk4000  $v28

    .func OPUS_test_twiddle
OPUS_test_twiddle:

    emux_breakpoint
    li t0, 0x5f3a
    mtc2 t0, vx.e0
    li t0, 0xa55c
    mtc2 t0, vx.e1

    li t1, %lo(TWIDDLE_CONST)
    lqv vconst1, 0x00,t1
    lqv vconst2, 0x10,t1
    vcopy vk4000, vconst1.e6

    # C = 0xdbc*2
    # B = -0x4dbc
    # (x3l*C + ((x3h*C)<<16) + ((x2*B)<<16) + ((0x4000*0x8000)<<16)) >> 30

    # Input vx in [0, π) => [-32768, 32767]
    # vk4000 = [0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000 ]
    # vconst = [0x4000, 0xc000, 0xb244, 0x4dbc, 0x1b78, 0xe488, ...]

    vmulf vx2, vx, vx
    vlt vcos, vzero, vx
    vmrg vconst2, vconst2, vconst2.q1
    # <1 stall>
    vmudm vx3h, vx2, vx2
    vmudn vcos, vx2, vconst1.e2
    vmadn vcos, vx2, vconst1.e2
    vmacf vcos, vk4000, vconst1.e7
    vmacf vcos, vx3h, vconst1.e4

    vmudn vcos, vconst2   # *2 / *-2

    jr ra
    nop

    .endfunc

#endif

# Reserve space for FFT overlay functions
    .align 4
FFT_OVERLAY_FUNC:
    .space 0x400
