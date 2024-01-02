
    # rsp_queue.inc
    
#define vzero    $v00
#define vshift   $v30
#define vshift8  $v31
.macro vcopy dest, source
    vor \dest, vzero, \source
.endm

#define K1       vshift.e7
#define K2       vshift.e6
#define K4       vshift.e5
#define K8       vshift.e4
#define K16      vshift.e3
#define K32      vshift.e2
#define K64      vshift.e1
#define K128     vshift.e0
#define K256     vshift8.e7
#define K512     vshift8.e6
#define K1024    vshift8.e5
#define K2048    vshift8.e4
#define K4096    vshift8.e3
#define K8192    vshift8.e2
#define K16384   vshift8.e1
#define K32768   vshift8.e0
#define KM32768  vshift8.e0    // note: this is actually -32768 for most arithmetic operations

#######################################################################
# TWIDDLES
#######################################################################

    .section .text.twiddles

    setup_vsll  vshift,  0
    setup_vsll8 vshift8, 0

    #define vtmp1i          $v15
    #define vtmp1f          $v16
    #define vtmp2i          $v17
    #define vtmp2f          $v18
    #define vtmp3i          $v19
    #define vtmp3f          $v20

    #define vtwiddle1       $v21
    #define vtwiddle2       $v22
    #define vtwiddle1inv    $v23
    #define vtwiddle2inv    $v24

    #define vk4010          $v25
    #define vk2             $v26
    #define vtwidx1         $v27
    #define vtwidx2         $v28
    #define vtwk1           $v29

    .func kf_twiddle_2
kf_twiddle_2:
    li t0, -2
    mtc2 t0, vtmp3f.e0


    # TWIDDLE1
    vsll vtmp1i, vtwidx1, 1
    vxor vtmp1f, vtmp1i, vtwidx1
    vge vtmp1f, vzero
    vmrg vtmp1f, vk2, vtmp3f.e0
    vmulf vtmp2i, vtmp1i, vtmp1i
    vmudm vtmp3i, vtmp2i, vtmp2i
    vmudn vtwiddle1, vtmp2i, vtwk1.e2
    vmadn vtwiddle1, vtmp2i, vtwk1.e2
    vmacf vtwiddle1, vk4010, vtwk1.e7   
    vmacf vtwiddle1, vtmp3i, vtwk1.e4
    vmudn vtwiddle1, vtmp1f

    # TWIDDLE2
    vsll vtmp1i, vtwidx2, 1
    vxor vtmp1f, vtmp1i, vtwidx2
    vge vtmp1f, vzero
    vmrg vtmp1f, vk2, vtmp3f.e0
    vmulf vtmp2i, vtmp1i, vtmp1i
    vmudm vtmp3i, vtmp2i, vtmp2i
    vmudn vtwiddle2, vtmp2i, vtwk1.e2
    vmadn vtwiddle2, vtmp2i, vtwk1.e2
    vmacf vtwiddle2, vk4010, vtwk1.e7   
    vmacf vtwiddle2, vtmp3i, vtwk1.e4
    vmudn vtwiddle2, vtmp1f

    # Increment twiddle angles
    lqv vtmp1f, 0x20,fTW
    lqv vtmp2f, 0x30,fTW
    vaddc vtwidx1, vtmp1f
    vaddc vtwidx2, vtmp2f

    # Load VCC with the bitpatternt to allow merging
    # of the real and imaginary parts. Notice that VCC
    # is modified by the vge opcode in the twiddle
    # calculations, so we need to do this every interation.
    li t0, 0x5555
    ctc2 t0, COP2_CTRL_VCC

    # swap real/imag
    vsubc vtwiddle1inv, vzero, vtwiddle1.q1
    vsubc vtwiddle2inv, vzero, vtwiddle2.q1
    vmrg  vtwiddle1inv, vtwiddle1.q0
    vmrg  vtwiddle2inv, vtwiddle2.q0

    jr ra
    nop

    .endfunc

    #undef vtmp1i
    #undef vtmp1f
    #undef vtmp2i
    #undef vtmp2f
    #undef vtmp3i
    #undef vtmp3f
