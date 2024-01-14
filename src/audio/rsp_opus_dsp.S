#include <rsp_queue.inc>

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand OPUS_deemphasis, 16           # 0x0
        RSPQ_DefineCommand OPUS_comb_fetch, 8            # 0x1
        RSPQ_DefineCommand OPUS_comb_filter_const, 16    # 0x2
        RSPQ_DefineCommand OPUS_comb_result, 8           # 0x3
        RSPQ_DefineCommand OPUS_comb_filter_dual, 20     # 0x4

    RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RSP OPUS2"
    .ascii " Coded by Rasky "

    .align 4
    RSPQ_BeginSavedState
DUMMY:    .long 0
    RSPQ_EndSavedState

###################################################################
# RSP macros
###################################################################

.macro vsra32f_sat  vdst, vregf, qty
    veVectorAccessors
    .if (\qty == 9)
        .set vsra_shift, vshift.e0
    .elseif (\qty == 10)
        .set vsra_shift, vshift.e1
    .elseif (\qty == 11)
        .set vsra_shift, vshift.e2
    .elseif (\qty == 12)
        .set vsra_shift, vshift.e3
    .elseif (\qty == 13)
        .set vsra_shift, vshift.e4
    .elseif (\qty == 14)
        .set vsra_shift, vshift.e5
    .elseif (\qty == 15)
        .set vsra_shift, vshift.e6
    .elseif (\qty == 16)
        .set vsra_shift, vshift.e7
    .else
        .error "vsra32: invalid qty"
    .endif
    vmudn \vdst, \vregf, vsra_shift
.endm

.macro vsra32i_sat  vdst, vregi, qty
    veVectorAccessors
    .if (\qty == 9)
        .set vsra_shift, vshift.e0
    .elseif (\qty == 10)
        .set vsra_shift, vshift.e1
    .elseif (\qty == 11)
        .set vsra_shift, vshift.e2
    .elseif (\qty == 12)
        .set vsra_shift, vshift.e3
    .elseif (\qty == 13)
        .set vsra_shift, vshift.e4
    .elseif (\qty == 14)
        .set vsra_shift, vshift.e5
    .elseif (\qty == 15)
        .set vsra_shift, vshift.e6
    .elseif (\qty == 16)
        .set vsra_shift, vshift.e7
    .else
        .error "vsra32: invalid qty"
    .endif
    vmadh \vdst, \vregi, vsra_shift
.endm

.macro vsll32f_sat  vdst, vregf, qty
    veVectorAccessors
    .if (\qty == 1)
        .set vsll_shift, vshift.e6
    .else
        .error "vsll32: invalid qty"
    .endif
    vmudn \vdst, \vregf, vsll_shift
.endm

.macro vsll32i_sat  vdst, vregi, qty
    veVectorAccessors
    .if (\qty == 1)
        .set vsll_shift, vshift.e6
    .else
        .error "vsll32: invalid qty"
    .endif
    vmadh \vdst, \vregi, vsll_shift
.endm

###################################################################
# COMB FILTER
###################################################################

#define FRAME_SIZE  960
#define OVERLAP     120

    .data

# Squared overlap window (120 shorts). The basic overlap windows is
# calculated by opus_custom_mode_create(48000, 960, NULL), and then
# it is squared (keeping it as Q15).
    .align 4
WINDOW2: .half 0, 0, 0, 0, 0, 2, 4, 7, 12, 19, 28, 40, 56, 77, 102, 133, 170, 214, 266, 327, 397, 478, 570, 674, 791, 922, 1068, 1228, 1406, 1600, 1811, 2041, 2290, 2558, 2845, 3154, 3483, 3832, 4203, 4595, 5007, 5441, 5895, 6369, 6863, 7376, 7908, 8458, 9025, 9608, 10205, 10817, 11441, 12076, 12722, 13376, 14037, 14703, 15374, 16047, 16720, 17394, 18064, 18730, 19392, 20045, 20691, 21325, 21950, 22561, 23159, 23743, 24310, 24859, 25391, 25904, 26397, 26872, 27326, 27759, 28173, 28564, 28935, 29283, 29613, 29922, 30209, 30477, 30726, 30955, 31168, 31361, 31539, 31698, 31844, 31976, 32093, 32196, 32289, 32369, 32440, 32500, 32554, 32598, 32634, 32666, 32690, 32712, 32728, 32740, 32748, 32756, 32760, 32764, 32766, 32766, 32766, 32766, 32766, 32766

    .section .bssovl2

# Maximum number of samples to process at once. This must be a
# multiple of 8 as we generally process 8 samples at a time.
#define COMB_MAX_SAMPLES       688

#define COMB_MAX_DMA_SAMPLES    64

    .align 4
COMB_TMP:     .space COMB_MAX_DMA_SAMPLES*4+8
    .align 4
COMB_F:       .space COMB_MAX_SAMPLES*2+8
    .align 4
COMB_I:       .space COMB_MAX_SAMPLES*2+8

    .text

    #define samples_left    t4
    #define dmem_idx        t5
    #define nproc           t6
    #define num_samples     k1
    #define dmem_in0        s1
    #define dmem_in1        s2
    #define dmem_out0       s5
    #define dmem_out1       s6
    #define dmem_in2        v0
    #define dmem_in3        v1
    #define vtmp1           $v01
    #define vtmp2           $v02
    #define vtmp3           $v03
    #define vtmp4           $v04
    #define vsigi           $v05
    #define vsigf           $v06

OPUS_32to16:
    andi t0, dmem_in0, 0xF
    assert_eq t0, 0, 0x1234

    #emux_dump_gpr dmem_out0, dmem_out1

    addiu dmem_in1, dmem_in0, 1
    addiu dmem_in2, dmem_in1, 1
    addiu dmem_in3, dmem_in2, 1
    addiu dmem_out0, -16
    addiu dmem_out1, -16
    addiu num_samples, -8

    lqv vsigi.e0, 0x00,dmem_out0
    lrv vsigi.e0, 0x10,dmem_out0
    lqv vsigf.e0, 0x00,dmem_out1
    lrv vsigf.e0, 0x10,dmem_out1

    # 19 cycles / 8 samples
OPUS_32to16_loop:
    lfv vtmp1.e0, 0x00,dmem_in0
    lfv vtmp1.e4, 0x10,dmem_in0
    lfv vtmp2.e0, 0x00,dmem_in1
    lfv vtmp2.e4, 0x10,dmem_in1
    lfv vtmp3.e0, 0x00,dmem_in2
    lfv vtmp3.e4, 0x10,dmem_in2
    lfv vtmp4.e0, 0x00,dmem_in3
    lfv vtmp4.e4, 0x10,dmem_in3

    addiu dmem_in0, 0x20;          vsll vtmp1, vtmp1, 1
    addiu dmem_in1, 0x20;          vsrl vtmp2, vtmp2, 7
    addiu dmem_in2, 0x20;          vsll vtmp3, vtmp3, 1
    addiu dmem_in3, 0x20;          vsrl vtmp4, vtmp4, 7

    sqv vsigi.e0, 0x00,dmem_out0
    srv vsigi.e0, 0x10,dmem_out0
    sqv vsigf.e0, 0x00,dmem_out1
    srv vsigf.e0, 0x10,dmem_out1

    addiu dmem_out0, 0x10;          vor vsigi, vtmp1, vtmp2
    addiu dmem_out1, 0x10;          vor vsigf, vtmp3, vtmp4

    bgtz num_samples, OPUS_32to16_loop
    addiu num_samples, -8

    sqv vsigi.e0, 0x00,dmem_out0
    srv vsigi.e0, 0x10,dmem_out0
    sqv vsigf.e0, 0x00,dmem_out1
    srv vsigf.e0, 0x10,dmem_out1
    addiu dmem_out0, 0x10
    addiu dmem_out1, 0x10
    #emux_dump_vpr vsigi, vsigf

    jr ra
    nop

OPUS_16to32:
    addiu num_samples, -8
OPUS_16to32_loop:
    lqv vsigi, 0x00,dmem_in0
    lqv vsigf, 0x00,dmem_in1

    ssv vsigi.e0, 0x00,dmem_out0
    ssv vsigf.e0, 0x02,dmem_out0
    ssv vsigi.e1, 0x04,dmem_out0
    ssv vsigf.e1, 0x06,dmem_out0
    ssv vsigi.e2, 0x08,dmem_out0
    ssv vsigf.e2, 0x0A,dmem_out0
    ssv vsigi.e3, 0x0C,dmem_out0
    ssv vsigf.e3, 0x0E,dmem_out0
    ssv vsigi.e4, 0x10,dmem_out0
    ssv vsigf.e4, 0x12,dmem_out0
    ssv vsigi.e5, 0x14,dmem_out0
    ssv vsigf.e5, 0x16,dmem_out0
    ssv vsigi.e6, 0x18,dmem_out0
    ssv vsigf.e6, 0x1A,dmem_out0
    ssv vsigi.e7, 0x1C,dmem_out0
    ssv vsigf.e7, 0x1E,dmem_out0

    addiu dmem_in0, 0x10
    addiu dmem_in1, 0x10
    addiu dmem_out0, 0x20

    bgtz num_samples, OPUS_16to32_loop
    addiu num_samples, -8

    jr ra
    nop

    ##############################################
    # OPUS_comb_fetch - Fetch incoming samples for comb filter
    #
    # Input:
    #   a0:  0..23: input 32-bit sample buffer
    #   a1:  0..15: number of samples to fetch
    #       16..31: starting index in DMEM COMB buffer
    #
    ##############################################

    .func OPUS_comb_fetch
OPUS_comb_fetch:
    andi samples_left, a1, 0xFFFF
    srl dmem_idx, a1, 16
    sll dmem_idx, 1

    #emux_log_string "OPUS_comb_fetch\n"
    #emux_dump_gpr dmem_idx, samples_left

    #if RSPQ_DEBUG
    andi t0, a0, 7
    assert_eq t0, 0, 0x8000   # Check input buffer is always aligned
    andi t0, dmem_idx, 7
    assert_eq t0, 0, 0x8002   # Check DMEM index is multiple of 8
    #endif

    addiu dmem_out0, dmem_idx, %lo(COMB_I)
    addiu dmem_out1, dmem_idx, %lo(COMB_F)

OPUS_comb_fetch_loop:
    ble samples_left, COMB_MAX_DMA_SAMPLES, 1f
    move nproc, samples_left
    li nproc, COMB_MAX_DMA_SAMPLES
1:  li s4, %lo(COMB_TMP)
    move s0, a0
    sll t0, nproc, 2
    jal DMAIn
    addiu t0, -1

    li dmem_in0, %lo(COMB_TMP)
    jal OPUS_32to16
    move num_samples, nproc

    sll t0, nproc, 2
    addu a0, t0

    sub samples_left, nproc
    bgtz samples_left, OPUS_comb_fetch_loop
    nop

    j RSPQ_Loop
    nop
    .endfunc


    ##############################################
    # OPUS_comb_result - Fetch result samples for comb filter
    #
    # Input:
    #   a0:  0..23: input 32-bit sample buffer
    #   a1:  0..15: number of samples to fetch
    #       16..31: starting index in DMEM COMB buffer
    #
    ##############################################

    .func OPUS_comb_result
OPUS_comb_result:
    andi samples_left, a1, 0xFFFF
    srl dmem_idx, a1, 16
    sll dmem_idx, 1

    #if RSPQ_DEBUG
    andi t0, a0, 7
    assert_eq t0, 0, 0x8200   # Check input buffer is always aligned
    andi t0, samples_left, 7
    assert_eq t0, 0, 0x8201   # Check number of samples is multiple of 8
    andi t0, dmem_idx, 7
    assert_eq t0, 0, 0x8202   # Check DMEM index is multiple of 8
    #endif

    addiu dmem_in0, dmem_idx, %lo(COMB_I)
    addiu dmem_in1, dmem_idx, %lo(COMB_F)

OPUS_comb_result_loop:
    ble samples_left, COMB_MAX_DMA_SAMPLES, 1f
    move nproc, samples_left
    li nproc, COMB_MAX_DMA_SAMPLES
1:  
    li dmem_out0, %lo(COMB_TMP)
    jal OPUS_16to32
    move num_samples, nproc

    li s4, %lo(COMB_TMP)
    move s0, a0
    sll t0, nproc, 2
    jal DMAOut
    addiu t0, -1

    sll t0, nproc, 2
    addu a0, t0

    sub samples_left, nproc
    bgtz samples_left, OPUS_comb_result_loop
    nop

    j RSPQ_Loop
    nop
    .endfunc

    #undef samples_left
    #undef dmem_idx    
    #undef nproc       
    #undef num_samples  
    #undef dmem_in0   
    #undef dmem_in1   
    #undef dmem_out0  
    #undef dmem_out1  
    #undef dmem_in2   
    #undef dmem_in3   
    #undef vtmp1        
    #undef vtmp2        
    #undef vtmp3        
    #undef vtmp4        
    #undef vsigi        
    #undef vsigf        

    ##############################################
    # OPUS_comb_filter - Apply FIR comb filter
    #
    # Input:
    #   a0:  0.. 7: input/output index in DMEM COMB buffer (divided by 4)
    #        8..15: number of samples to process (divded by 8, minus 1)
    #   a1:  0..15: g10
    #       16..31: g11
    #   a2:  0..15: g12
    #       16..31: gi/2 (should be 0x4000 to add x[i] to output)
    #   a3:  0..15: T feedback index in DMEM COMB buffer
    #
    ##############################################

    .func OPUS_comb_filter_const
OPUS_comb_filter_const:

#define RSP_OPUS_COMB_DUAL 0
#include "rsp_opus_comb.inc"
#undef RSP_OPUS_COMB_DUAL

    .endfunc

    ##############################################
    # OPUS_comb_filter_dual - Apply Dual FIR comb filter
    #
    # Input:
    #   a0:  0.. 7: input/output index in DMEM COMB buffer (divided by 4)
    #        8..15: number of samples to process (divded by 8, minus 1)
    #   a1:  0..15: g00
    #       16..31: g01
    #   a2:  0..15: g02
    #       16..31: g10
    #   a3:  0..15: T0 feedback index in DMEM COMB buffer
    #       16..31: T1 feedback index in DMEM COMB buffer
    #  5th:  0..15: g11
    #       16..31: g12
    #
    ##############################################
    .func OPUS_comb_filter_dual
OPUS_comb_filter_dual:

#define RSP_OPUS_COMB_DUAL 1
#include "rsp_opus_comb.inc"
#undef RSP_OPUS_COMB_DUAL

    .endfunc

###################################################################
# EMPHASIS FILTER
###################################################################

    .data

#define KEMPH1    0x6ccd    // Q15(0.8500061035 ^ 1)
#define KEMPH2    0x5c7b    // Q15(0.8500061035 ^ 2)
#define KEMPH3    0x4ec9    // Q15(0.8500061035 ^ 3)
#define KEMPH4    0x42d2    // Q15(0.8500061035 ^ 4)
#define KEMPH5    0x38cc    // Q15(0.8500061035 ^ 5)
#define KEMPH6    0x3047    // Q15(0.8500061035 ^ 6)
#define KEMPH7    0x2909    // Q15(0.8500061035 ^ 7)
#define KEMPH8    0x22e1    // Q15(0.8500061035 ^ 8)
#define KEMPH1RCP 0x1696    // Q15(1.0 - 1.0 / 0.8500061035)

    .align 4
KEMPHTABLE:
    .half   KEMPH1, KEMPH2, KEMPH3, KEMPH4
    .half   KEMPH5, KEMPH6, KEMPH7, KEMPH8
IDENTITY:
    .half   1<<15-1, 1<<15-1, 1<<15-1, 1<<15-1
    .half   1<<15-1, 1<<15-1, 1<<15-1, 1<<15-1

    .section .bssovl1

#define DEEMPH_MAX_SAMPLES  256

    .align 4
DEEMPH_IN_L:    .space DEEMPH_MAX_SAMPLES*4         # mono 32-bit PCM samples (left)
DEEMPH_IN_R:    .space DEEMPH_MAX_SAMPLES*4         # mono 32-bit PCM samples (right)
                .space 8*4                          # padding
DEEMPH_OUT:     .space DEEMPH_MAX_SAMPLES*2*2       # stereo 16-bit PCM samples
DEEMPH_STATE:   .space 4*2                          # two 32-bit samples (left/right)

    .text

#define rdram_in_l           a0
#define rdram_in_r           a1
#define rdram_out            a2
#define rdram_state          a3
#define downsample_factor    t7
#define is_stereo            a1
#define num_samples          k0
#define total_samples        k1
#define stereo_toggle        v0
#define out_step             v1
#define dmem_in              s0
#define dmem_out             s4
#define dmem_state           s1

#define vxf       $v02
#define vxi       $v03
#define vpcmf     $v04
#define vpcmi     $v05
#define vpcm      $v06

#define vmtx0     $v07
#define vmtx1     $v08
#define vmtx2     $v09
#define vmtx3     $v10
#define vmtx4     $v11
#define vmtx5     $v12
#define vmtx6     $v13
#define vmtx7     $v14
#define vmtx8     $v15
#define vin0      $v16
#define vin1      $v17
#define v____     $v29

#define sig0      e0
#define sig1      e2
#define sig2      e4
#define sig3      e6
#define sig4      e1
#define sig5      e3
#define sig6      e5
#define sig7      e7

    ##############################################
    # OPUS_deemphasis - Apply IIR emphasis filter
    #
    # Input:
    #   a0: RDRAM pointer to L input buffer (32-bit PCM samples)
    #   a1: RDRAM pointer to R input buffer (32-bit PCM samples), or zero if mono
    #       MSB: downsample factor
    #   a2: RDRAM pointer to output buffer (16-bit PCM samples, interleaved if stereo)
    #       MSB: number of input samples in each buffer, divided by 4, minus 1
    #   a3: RDRAM pointer to IIR state buffer
    ##############################################

    .align 3
    .func OPUS_deemphasis
OPUS_deemphasis:
    # Extract total samples                 # Clean coefficient matrix
    srl total_samples, a2, 24;              vxor vmtx1, vmtx1
    addiu total_samples, 1;                 vxor vmtx2, vmtx2
    sll total_samples, 2;                   vxor vmtx3, vmtx3

    assert_le total_samples, DEEMPH_MAX_SAMPLES, 0x1237

    # DMA first buffer of samples
    move s0, a0;                            vxor vmtx4, vmtx4
    li s4, %lo(DEEMPH_IN_L);                vxor vmtx5, vmtx5
    sll t0, total_samples, 2;               vxor vmtx6, vmtx6
    jal DMAIn
    addiu t0, -1

    # Fetch IIR state
    move s0, a3
    li s4, %lo(DEEMPH_STATE)
    jal DMAIn
    li t0, DMA_SIZE(4*2, 1)

    li out_step, 2
    li s4, %lo(DEEMPH_OUT) - 2*8

    # Check if there is a second buffer
    srl downsample_factor, a1, 24;          vxor vmtx7, vmtx7     
    sll a1, 8;                              vxor vmtx8, vmtx8
    srl a1, 8
    beqz a1, 1f
    move s0, a1
    li s4, %lo(DEEMPH_IN_R)
    sll t0, total_samples, 2;
    jal DMAIn
    addiu t0, -1

    li out_step, 4
    li dmem_out, %lo(DEEMPH_OUT) - 4*8
    andi t0, rdram_out, 7
    addu dmem_out, t0

    # Load coefficient matrix
1:  li s0, %lo(KEMPHTABLE)
    lqv vmtx0.e0, 0x00,s0
    lqv vmtx1.e1, 0x00,s0
    lqv vmtx2.e2, 0x00,s0
    lqv vmtx3.e3, 0x00,s0
    lqv vmtx4.e4, 0x00,s0
    lqv vmtx5.e5, 0x00,s0
    lqv vmtx6.e6, 0x00,s0
    lqv vmtx7.e7, 0x00,s0
    ltv vmtx1.e0, 0x10,s0   # load identity
    li stereo_toggle, 0
    li dmem_state, %lo(DEEMPH_STATE)
    li dmem_in, %lo(DEEMPH_IN_L)

OPUS_deemphasis_start:
    lsv vpcmf.e7, 0x02,dmem_state
    lsv vpcmi.e7, 0x00,dmem_state
    move num_samples, total_samples
    li t0, 0x55
    ctc2 t0, COP2_CTRL_VCC
    lqv vin0, 0x00,dmem_in          # H0 L0 H1 L1  H2 L2 H3 L3
    lqv vin1, 0x10,dmem_in          # H4 L4 H5 L5  H6 L6 H7 L7
    vcopy vxf, vin0.q1              # L0 L0 L1 L1  L2 L2 L3 L3
    vmrg vxi, vin0, vin1.q0         # H0 H4 H1 H5  H2 H6 H3 H7
    vmrg vxf, vxf,  vin1.q1         # L0 L4 L1 L5  L2 L6 L3 L7

    # Apply IIR filter
    # 30 cycles / 8 samples = 3.75 cycles/sample
    .align 3
OPUS_deemphasis_loop:
    vmudm v____, vmtx0, vpcmf.e7;      lqv vin0, 0x20,dmem_in
    vmadh v____, vmtx0, vpcmi.e7;      lqv vin1, 0x30,dmem_in
    vmadm v____, vmtx0, vpcmf.e7;
    vmadh v____, vmtx0, vpcmi.e7;
    vmadm v____, vmtx1, vxf.sig0;      
    vmadh v____, vmtx1, vxi.sig0;      addiu num_samples, -8
    vmadm v____, vmtx2, vxf.sig1;      addiu dmem_in, 0x20
    vmadh v____, vmtx2, vxi.sig1;      ssv vpcm.e0, 0x00,dmem_out
    vmadm v____, vmtx3, vxf.sig2;      addu dmem_out, out_step
    vmadh v____, vmtx3, vxi.sig2;      ssv vpcm.e1, 0x00,dmem_out
    vmadm v____, vmtx4, vxf.sig3;      addu dmem_out, out_step
    vmadh v____, vmtx4, vxi.sig3;      ssv vpcm.e2, 0x00,dmem_out
    vmadm v____, vmtx5, vxf.sig4;      addu dmem_out, out_step
    vmadh v____, vmtx5, vxi.sig4;      ssv vpcm.e3, 0x00,dmem_out
    vmadm v____, vmtx6, vxf.sig5;      addu dmem_out, out_step
    vmadh v____, vmtx6, vxi.sig5;      ssv vpcm.e4, 0x00,dmem_out
    vmadm v____, vmtx7, vxf.sig6;      addu dmem_out, out_step
    vmadh v____, vmtx7, vxi.sig6;      ssv vpcm.e5, 0x00,dmem_out
    vmadm v____, vmtx8, vxf.sig7;      addu dmem_out, out_step
    vmadh v____, vmtx8, vxi.sig7;      ssv vpcm.e6, 0x00,dmem_out
    vsar vpcmf, COP2_ACC_MD;           addu dmem_out, out_step
    vsar vpcmi, COP2_ACC_HI;           ssv vpcm.e7, 0x00,dmem_out
    vcopy vxf, vin0.q1;                addu dmem_out, out_step
    # <stall>
    # Our matrix is stored as 0.15 instead of 0.16. This means that the
    # the result in vpcmf/vpcmi is a divided by 2. The signal data has
    # 12 bit of fixed point precision, so we do a shift-right by 11 instead,
    # to convert to a raw 16bit pcm value.
    # NOTE: this also means that the IIR state is kept divided by 2.
    # This is why we add it twice to the MAC sequence above (multiplied by vmtx0).
    vsra32f_sat v____, vpcmf, 12-1
    vsra32i_sat vpcm,  vpcmi, 12-1
    vmrg vxi, vin0, vin1.q0
    bgtz num_samples, OPUS_deemphasis_loop
    vmrg vxf, vxf,  vin1.q1

    # Store last vector of samples
    ssv vpcm.e0, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e1, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e2, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e3, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e4, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e5, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e6, 0x00,dmem_out
    addu dmem_out, out_step
    ssv vpcm.e7, 0x00,dmem_out
    addu dmem_out, out_step

    # Store IIR state
    ssv vpcmi.e7, 0x00,dmem_state
    ssv vpcmf.e7, 0x02,dmem_state

    # Check if we have to process the second buffer
    xor stereo_toggle, is_stereo
    beqz stereo_toggle, OPUS_downsample

    # Setup state for right channel
    li dmem_in, %lo(DEEMPH_IN_R)
    li dmem_out, %lo(DEEMPH_OUT) - 4*8 + 2
    andi t0, rdram_out, 7
    addu dmem_out, t0
    j OPUS_deemphasis_start
    addiu dmem_state, 4

OPUS_downsample:
    move s0, rdram_state
    li s4, %lo(DEEMPH_STATE)
    jal DMAOutAsync
    li t0, DMA_SIZE(4*2, 1)

    # Downsample the output buffer in-place
    # TODO: we could have optimized loops for downsample_factor=2 which
    # is probably the most common case.
    li s0, %lo(DEEMPH_OUT)
    li s4, %lo(DEEMPH_OUT)
    andi t0, rdram_out, 7
    addu s0, t0
    addu s4, t0
    sll downsample_factor, 1            # account for 16-bit
    sll num_samples, total_samples, 1   # account for 16-bit

    beqz is_stereo, OPUS_downsample_loop_mono_start
    add s3, s4, num_samples             # calculate end pointer
    
    add s3, num_samples                 # account for stereo
    sll downsample_factor, 1            # account for stereo
    sll num_samples, 1                  # account for stereo

    beq downsample_factor, 4, OPUS_deemphasis_out
    # L0 R0 L1 R1  L2 R2 L3 R3
OPUS_downsample_loop_stereo:
    llv vpcm.e0, 0x00,s0
    add s0, downsample_factor
    llv vpcm.e2, 0x00,s0
    add s0, downsample_factor
    llv vpcm.e4, 0x00,s0
    add s0, downsample_factor
    llv vpcm.e6, 0x00,s0
    add s0, downsample_factor
    add s4, 16
    bne s4, s3, OPUS_downsample_loop_stereo
    sqv vpcm, -0x10,s4

    j OPUS_deemphasis_out
    nop

OPUS_downsample_loop_mono_start:
    beq downsample_factor, 2, OPUS_deemphasis_out
    # L0 L1 L2 L3  L4 L5 L6 L7
OPUS_downsample_loop_mono:
    # NOTE: total_samples is guaranteed to be a multiple of 4 not 8, so 
    # we copy only 4 samples at a time here.
    lsv vpcm.e0, 0x00,s0
    add s0, downsample_factor
    lsv vpcm.e1, 0x00,s0
    add s0, downsample_factor
    lsv vpcm.e2, 0x00,s0
    add s0, downsample_factor
    lsv vpcm.e3, 0x00,s0
    add s0, downsample_factor
    add s4, 8
    bne s4, s3, OPUS_downsample_loop_mono
    sdv vpcm.e0, -8,s4

OPUS_deemphasis_out:
    and t0, rdram_out, 7
    beqz t0, OPUS_deemphasis_out2
    nop

    # Handle unaligned output buffer: we need to fetch and merge
    # existing samples on misaligned addresses
    emux_breakpoint
    move s0, rdram_out
    jal DMAIn
    li s4, %lo(DEEMPH_IN_L)
    
    li s0, %lo(DEEMPH_OUT)
    li s4, %lo(DEEMPH_IN_L)
OPUS_deemphasis_merge_loop:
    lhu t1, 0(s4)
    addiu s4, 2
    sh t1, 0(s0)
    addiu t0, -2
    bnez t0, OPUS_deemphasis_merge_loop
    addiu s0, 2

OPUS_deemphasis_out2:
    move s0, rdram_out
    li s4, %lo(DEEMPH_OUT)
    sub t0, s3, s4
    jal DMAOutAsync
    addiu t0, -1

    j RSPQ_Loop
    nop

    .endfunc
