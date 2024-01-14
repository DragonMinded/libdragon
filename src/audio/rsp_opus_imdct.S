#include <rsp_queue.inc>

#define stall    nop
#define vstall   vnop

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand OPUS_imdct, 24                # 0x0
        RSPQ_DefineCommand OPUS_window, 8                # 0x1
        RSPQ_DefineCommand OPUS_memmove, 12              # 0x2
        RSPQ_DefineCommand OPUS_clear, 8                 # 0x3
    RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RSP OPUS1"
    .ascii " Coded by Rasky "

    .align 4
    RSPQ_BeginSavedState
DUMMY:    .long 0
    RSPQ_EndSavedState

    #define B                        k0
    #define N                        k1
    #define SHIFT                    fp
    #define fft_steps_cur_rdram      fp
    #define fft_steps_init_rdram     s6
    #define imdct_out_rdram          s5

    #define vtwidx1      $v27
    #define vtwidx2      $v28
    #define vtwk1        $v29
    
    .data

    .align 4
TWIDDLE_CONST1: .half 0         # used to store twiddle angle increment
                .half 0         # used to store %lo(IMDCT_DATA)
                .half 0xb226    # B constant
                .half 0         # used in bfly3/postrot
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
    # We want to deinterleave it, that is separate each band. For instance,
    # when there are 64 samples per band, we want our output to be:
    #
    #   A0 A1 A2 ... A63 B0 B1 B2 ... B63 ...
    #
    # Moreover, in addition to de-interleaving, we also "rotate" the data. This
    # is a common "pre-rotation" step necessary to implement IMDCT via a FFT.
    # After this rotation, odd-indexed values are reverse ordered in each band.
    # So basically the actual ouput will be:
    #
    #   A0 A63 A2 A61 ... A60 A3 A62 A1 B0 B63 ....
    #
    ############################################################################
    #
    # Input:
    #    a0: input buffer in RDRAM
    #    t6: output buffer in RDRAM where deinterleaved values will be stored
    #
    ############################################################################

    #define chunk_left      t5
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
    sll t0, cur_band, 2
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
    move chunk_left, chunk

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

    addiu chunk_left, -8
    addiu out_dmem, 0x20

    sqv vini, -0x20,out_dmem;       vcopy vprev1, vin1
    sqv vinf, -0x10,out_dmem;       vcopy vprev2, vin2

    bgtz chunk_left, OPUS_imdct_prerot_compact_loop
    nop

    mtc0 out_rdram, COP0_DMA_RAMADDR
    li out_dmem, %lo(IMDCT_TMP)
    andi out_dmem, 0xFFF
    mtc0 out_dmem, COP0_DMA_SPADDR
    sll t0, chunk, 2
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
    #     :  0..23. RDRAM pointer to ouput buffer
    #
    ############################################################################

    .func OPUS_imdct
OPUS_imdct:
    lw fft_steps_init_rdram, CMD_ADDR(16, 24)
    lw imdct_out_rdram, CMD_ADDR(20, 24)

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
    
OPUS_imdct_start:
    # From now on, we assume that DMEM contains only two buffers:
    #  IMDCT_DATA: holding all the values/samples for the IMDCT
    #  IMDCT_TMP:  holding instructions for the current FFT step.
    # We can't load from DMEM anymore other constants.

    # Do deinterleaving of input data. The value will be fetched
    # from RDRAM, deinterleaved, and then moved again to RDRAM into the
    # work buffer.
    jal OPUS_imdct_prerot_compact
    move t6, a2

    # Since we all input values have been fetched and processed into the
    # temporary buffer, repurpose a0 to point to the temporary buffer, and
    # advance for each processed band
    move a0, a2

OPUS_imdct_band:
    # Fetch first step of the cmd table
    move fft_steps_cur_rdram, fft_steps_init_rdram
OPUS_imdct_fft_loop:
    move s0, fft_steps_cur_rdram
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

    # Update next pointer, in case IMDCT_TMP is overwritten by the overlay
    lw fft_steps_cur_rdram, 128(s1)

    # Load the arguments and run the overlay
    li fZ, %lo(IMDCT_DATA)
    li fTW, %lo(IMDCT_TMP)
    lw fM, 140(s1)
    lw fN, 144(s1)
    jal FFT_OVERLAY_FUNC
    lw fmm, 148(s1)

    # Check if there's another step
    bnez fft_steps_cur_rdram, OPUS_imdct_fft_loop
    nop

OPUS_imdct_fft_end:
    # DMA final 32-bit samples into output buffer
    move s0, imdct_out_rdram
    li s4, %lo(IMDCT_DATA)
    sll t0, N, 1
    jal DMAOut
    addiu t0, -1

    # Calculate updated pointers for input and output buffers
    sll t0, N, 1
    addu a0, t0
    addu imdct_out_rdram, t0

    # Check if there's another band to process
    addiu B, -1
    bnez B, OPUS_imdct_band
    nop

    # Restore DMEM and exit
    addiu s0, a2, 3840
    li s4, 0
    li t0, DMA_SIZE(4096, 1)
    jal_and_j DMAIn, RSPQ_Loop

    .endfunc

    .section .bssovl2

#define MAX_WINDOW_SIZE     120

    .align 4
WND_BUFFER: .space MAX_WINDOW_SIZE*4*2
WND_COEEFS: .space MAX_WINDOW_SIZE*2*2

    .text

    ############################################################################
    # OPUS_window
    #
    # Input values:
    #   a0: 0..23:  RDRAM pointer to input/output buffer
    #   a1: 0..23:  RDRAM pointer to window coefficients
    #       24..27: Window size in 32-bit samples
    #
    ############################################################################

    #define dmem_left     s3
    #define dmem_right    s5
    #define wnd_left      s6
    #define wnd_right     s7
    #define num_bytes     t8

    #define vin0          $v01
    #define vin1          $v02
    #define vin2          $v03
    #define vin3          $v04
    #define vxl           $v05
    #define vxh           $v06
    #define vyl           $v07
    #define vyh           $v08
    #define vwina         $v09
    #define vwinb         $v10
    #define vwinan        $v11
    #define voutxl        $v12
    #define voutxh        $v13
    #define voutyl        $v14
    #define voutyh        $v15

    .func OPUS_window
OPUS_window:
    srl N, a1, 24

    sll num_bytes, N, 2
    li dmem_left, %lo(WND_BUFFER)
    addu dmem_right, dmem_left, num_bytes

    move s0, a0
    li s4, %lo(WND_BUFFER)
    jal DMAIn
    addiu t0, num_bytes, -1

    # Fetch window coefficients
    # 16-bit coefficients, but doubled in size 
    # (with mirrored version for backward scanning)
    li wnd_left, %lo(WND_COEEFS)
    sll t0, N, 2
    addu wnd_right, wnd_left, t0
    move s0, a1
    li s4, %lo(WND_COEEFS)
    jal DMAIn
    addiu t0, -1

    # Initialize VCC flags for vmrg
    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC


    # Processing: 16 32-bit values at a time
    # SU: 12 loads, 10 stores, 4 incrs, 2 loop-related = 28
    # VU: 6 swizzle, 1 preproc, 16 macs, 6 swizzle = 29

    .align 3
OPUS_window_loop:
    lqv vin0, 0x00,dmem_left        # H0 L0 H1 L1  H2 L2 H3 L3
    lqv vin1, 0x10,dmem_left        # H4 L4 H5 L5  H6 L6 H7 L7

    llv vin2.e0, -0x04,dmem_right
    llv vin2.e2, -0x08,dmem_right
    llv vin2.e4, -0x0C,dmem_right
    llv vin2.e6, -0x10,dmem_right
    llv vin3.e0, -0x14,dmem_right
    llv vin3.e2, -0x18,dmem_right
    llv vin3.e4, -0x1C,dmem_right
    llv vin3.e6, -0x20,dmem_right

    lqv vwina, 0x00,wnd_left
    lqv vwinb, -0x10,wnd_right

    vcopy vxl, vin0.q1              # L0 L0 L1 L1  L2 L2 L3 L3
    vmrg vxh, vin0, vin1.q0         # H0 H4 H1 H5  H2 H6 H3 H7
    vmrg vxl, vxl,  vin1.q1         # L0 L4 L1 L5  L2 L6 L3 L7

    vcopy vyl, vin2.q1              # L0 L0 L1 L1  L2 L2 L3 L3
    vmrg vyh, vin2, vin3.q0         # H0 H4 H1 H5  H2 H6 H3 H7
    vmrg vyl, vyl,  vin3.q1         # L0 L4 L1 L5  L2 L6 L3 L7

    vsubc vwinan, vzero, vwina

    vmudm voutxl, vwina, vxl
    vmadh voutxl, vwina, vxh
    vmadm voutxl, vwinb, vyl
    vmadh voutxl, vwinb, vyh
    vsar voutxl, COP2_ACC_MD
    vsar voutxh, COP2_ACC_HI

    vmudm voutyl, vwinan, vyl
    vmadh voutyl, vwinan, vyh
    vmadm voutyl, vwinb, vxl
    vmadh voutyl, vwinb, vxh
    vsar voutyl, COP2_ACC_MD
    vsar voutyh, COP2_ACC_HI

    vaddc voutxl, voutxl
    vadd  voutxh, voutxh
    vaddc voutyl, voutyl
    vadd  voutyh, voutyh

    vmrg vxh, voutxh, voutxl.q0     # H0 L0 H1 L1  H2 L2 H3 L3
    vcopy vxl, voutxh.q1            # H4 H4 H5 H5  H6 H6 H7 H7
    vmrg vxl, voutxl

    vmrg vyh, voutyh, voutyl.q0     # H0 L0 H1 L1  H2 L2 H3 L3
    vcopy vyl, voutyh.q1            # H4 H4 H5 H5  H6 H6 H7 H7
    vmrg vyl, voutyl

    sqv vyh, 0x00,dmem_left
    slv vxh.e0, -0x04,dmem_right
    slv vxh.e2, -0x08,dmem_right
    slv vxh.e4, -0x0C,dmem_right
    slv vxh.e6, -0x10,dmem_right

    addiu N, -8
    blez N, OPUS_window_exit
    nop

    sqv vyl, 0x10,dmem_left
    slv vxl.e0, -0x14,dmem_right
    slv vxl.e2, -0x18,dmem_right
    slv vxl.e4, -0x1C,dmem_right
    slv vxl.e6, -0x20,dmem_right

    addiu dmem_left, 0x20
    addiu dmem_right, -0x20
    addiu wnd_left, 0x10
    addiu wnd_right, -0x10

    addiu N, -8
    bgtz N, OPUS_window_loop
    nop

OPUS_window_exit:
    move s0, a0
    li s4, %lo(WND_BUFFER)
    addiu t0, num_bytes, -1
    jal_and_j DMAOut, RSPQ_Loop

    .endfunc


    .section .bssovl3

#define MOVE_BUFFER_SIZE    3072

MOVE_BUFFER: .space MOVE_BUFFER_SIZE

    .text

    ############################################################################
    # OPUS_memmove
    #
    # This is called at the beginning of the opus frame. Put it in the first
    # opus overlay being used.
    #
    # Notice that this is not a generic memmove: we handle the simplified case
    # that we need: data is moved backward into the output buffer, and it's ok
    # to move a few bytes more if they're not 8 bytes aligned.
    #
    # Input values:
    #   a0: 0..23:  RDRAM pointer to destination data
    #   a1: 0..23:  RDRAM pointer to source data
    #   a2:         Number of bytes word to move
    #
    ############################################################################

    .func OPUS_memmove
OPUS_memmove:
    #if RSPQ_DEBUG
    andi t0, a0, 7
    assert_eq t0, 0, 0x8509
    andi t0, a1, 7
    assert_eq t0, 0, 0x8509
    #endif

OPUS_memmove_loop:
    move t4, a2
    ble t4, MOVE_BUFFER_SIZE, 1f
    nop
    li t4, MOVE_BUFFER_SIZE
1:
    move s0, a1
    li s4, %lo(MOVE_BUFFER)
    jal DMAIn
    addiu t0, t4, -1

    move s0, a0
    li s4, %lo(MOVE_BUFFER)
    jal DMAOut
    addiu t0, t4, -1

    addu a0, t4
    addu a1, t4
    addiu a2, -MOVE_BUFFER_SIZE
    bgtz a2, OPUS_memmove_loop
    nop

    j RSPQ_Loop
    nop

    .endfunc


    ############################################################################
    # OPUS_clear
    #
    # This is called to clear the decode buffer, during OPUS_RESET_STATE.
    #
    # Input values:
    #   a0: 0..23:  RDRAM pointer to destination data
    #   a1:         Number of bytes word to clear
    ############################################################################

    .func OPUS_clear
OPUS_clear:
    #if RSPQ_DEBUG
    andi t0, a0, 7
    assert_eq t0, 0, 0x850A
    #endif

    li t4, MOVE_BUFFER_SIZE
    li s4, %lo(MOVE_BUFFER)
1:  sqv vzero, 0x00,s4
    addiu t4, -16
    bgtz t4, 1b
    nop

OPUS_clear_loop:
    move t4, a1
    ble t4, MOVE_BUFFER_SIZE, 1f
    nop
    li t4, MOVE_BUFFER_SIZE
1:
    move s0, a0
    li s4, %lo(MOVE_BUFFER)
    jal DMAOut
    addiu t0, t4, -1

    addu a0, t4
    addiu a1, -MOVE_BUFFER_SIZE
    bgtz a1, OPUS_clear_loop
    nop

    j RSPQ_Loop
    nop

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
