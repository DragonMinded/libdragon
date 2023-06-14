#ifndef RSP_GL_COMMON_INC
#define RSP_GL_COMMON_INC

    .section .text.mtxmult

.func GL_MtxMult
GL_MtxMult:
    #define lhs s5
    #define rhs s6
    #define dst s7

    #define v___         $v01

    #define vrhs01_i     $v02
    #define vrhs01_f     $v03
    #define vrhs23_i     $v04
    #define vrhs23_f     $v05

    #define vlhs0_i      $v06
    #define vlhs0_f      $v07
    #define vlhs1_i      $v08
    #define vlhs1_f      $v09
    #define vlhs2_i      $v10
    #define vlhs2_f      $v11
    #define vlhs3_i      $v12
    #define vlhs3_f      $v13

    lqv vrhs01_i, 0x00,rhs
    lqv vrhs23_i, 0x10,rhs
    lqv vrhs01_f, 0x20,rhs
    lqv vrhs23_f, 0x30,rhs

GL_MtxMultWithRhs:
    ldv vlhs0_i.e0,  0x00,lhs
    ldv vlhs0_i.e4,  0x00,lhs
    ldv vlhs1_i.e0,  0x08,lhs
    ldv vlhs1_i.e4,  0x08,lhs
    ldv vlhs2_i.e0,  0x10,lhs
    ldv vlhs2_i.e4,  0x10,lhs
    ldv vlhs3_i.e0,  0x18,lhs
    ldv vlhs3_i.e4,  0x18,lhs
    ldv vlhs0_f.e0,  0x20,lhs
    ldv vlhs0_f.e4,  0x20,lhs
    ldv vlhs1_f.e0,  0x28,lhs
    ldv vlhs1_f.e4,  0x28,lhs
    ldv vlhs2_f.e0,  0x30,lhs
    ldv vlhs2_f.e4,  0x30,lhs
    ldv vlhs3_f.e0,  0x38,lhs
    ldv vlhs3_f.e4,  0x38,lhs

    vmudl v___,     vlhs0_f, vrhs01_f.h0
    vmadm v___,     vlhs0_i, vrhs01_f.h0
    vmadn v___,     vlhs0_f, vrhs01_i.h0
    vmadh v___,     vlhs0_i, vrhs01_i.h0

    vmadl v___,     vlhs1_f, vrhs01_f.h1
    vmadm v___,     vlhs1_i, vrhs01_f.h1
    vmadn v___,     vlhs1_f, vrhs01_i.h1
    vmadh v___,     vlhs1_i, vrhs01_i.h1

    vmadl v___,     vlhs2_f, vrhs01_f.h2
    vmadm v___,     vlhs2_i, vrhs01_f.h2
    vmadn v___,     vlhs2_f, vrhs01_i.h2
    vmadh v___,     vlhs2_i, vrhs01_i.h2

    vmadl v___,     vlhs3_f, vrhs01_f.h3
    vmadm v___,     vlhs3_i, vrhs01_f.h3
    vmadn vrhs01_f, vlhs3_f, vrhs01_i.h3
    vmadh vrhs01_i, vlhs3_i, vrhs01_i.h3

    vmudl v___,     vlhs0_f, vrhs23_f.h0
    vmadm v___,     vlhs0_i, vrhs23_f.h0
    vmadn v___,     vlhs0_f, vrhs23_i.h0
    vmadh v___,     vlhs0_i, vrhs23_i.h0

    vmadl v___,     vlhs1_f, vrhs23_f.h1
    vmadm v___,     vlhs1_i, vrhs23_f.h1
    vmadn v___,     vlhs1_f, vrhs23_i.h1
    vmadh v___,     vlhs1_i, vrhs23_i.h1

    vmadl v___,     vlhs2_f, vrhs23_f.h2
    vmadm v___,     vlhs2_i, vrhs23_f.h2
    vmadn v___,     vlhs2_f, vrhs23_i.h2
    vmadh v___,     vlhs2_i, vrhs23_i.h2

    vmadl v___,     vlhs3_f, vrhs23_f.h3
    vmadm v___,     vlhs3_i, vrhs23_f.h3
    vmadn vrhs23_f, vlhs3_f, vrhs23_i.h3
    vmadh vrhs23_i, vlhs3_i, vrhs23_i.h3

GL_MtxMultSave:
    sqv vrhs01_i, 0x00,dst
    sqv vrhs23_i, 0x10,dst
    sqv vrhs01_f, 0x20,dst
    jr ra
    sqv vrhs23_f, 0x30,dst

    .endfunc

    .section .text.mtxtransform

    .func GL_MtxTransformDouble
GL_MtxTransformDouble:
    #define mtx_ptr      s4

    #define vmtx0_i      $v19       //  m00 m01 m02 m03
    #define vmtx0_f      $v20
    #define vmtx1_i      $v21       //  m00 m01 m02 m03
    #define vmtx1_f      $v22
    #define vmtx2_i      $v23       //  m00 m01 m02 m03
    #define vmtx2_f      $v24
    #define vmtx3_i      $v25       //  m00 m01 m02 m03
    #define vmtx3_f      $v26

    #define vvec         $v27
    #define vresult_i    $v28
    #define vresult_f    $v29

    ldv vmtx0_i.e4,  0x00,mtx_ptr
    ldv vmtx1_i.e4,  0x08,mtx_ptr
    ldv vmtx2_i.e4,  0x10,mtx_ptr
    ldv vmtx3_i.e4,  0x18,mtx_ptr
    ldv vmtx0_f.e4,  0x20,mtx_ptr
    ldv vmtx1_f.e4,  0x28,mtx_ptr
    ldv vmtx2_f.e4,  0x30,mtx_ptr
    ldv vmtx3_f.e4,  0x38,mtx_ptr

GL_MtxTransformSingle:
    ldv vmtx0_i.e0,  0x00,mtx_ptr
    ldv vmtx1_i.e0,  0x08,mtx_ptr
    ldv vmtx2_i.e0,  0x10,mtx_ptr
    ldv vmtx3_i.e0,  0x18,mtx_ptr
    ldv vmtx0_f.e0,  0x20,mtx_ptr
    ldv vmtx1_f.e0,  0x28,mtx_ptr
    ldv vmtx2_f.e0,  0x30,mtx_ptr
    ldv vmtx3_f.e0,  0x38,mtx_ptr

    vmudn v___,      vmtx0_f, vvec.h0
    vmadh v___,      vmtx0_i, vvec.h0
    vmadn v___,      vmtx1_f, vvec.h1
    vmadh v___,      vmtx1_i, vvec.h1
    vmadn v___,      vmtx2_f, vvec.h2
    vmadh v___,      vmtx2_i, vvec.h2
    vmadn v___,      vmtx3_f, vvec.h3
    vmadh vresult_i, vmtx3_i, vvec.h3
    jr ra
    vmadn vresult_f, vzero,   vzero
    .endfunc

#endif
