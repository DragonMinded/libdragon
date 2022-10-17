#ifndef RSP_GL_COMMON_INC
#define RSP_GL_COMMON_INC

    ####################################################
    # Returns:
    #   s0: Pointer to the currently active texture,
    #       or 0 if none is active
    #   t0: State flags
    ####################################################
    .func GL_GetActiveTexture
GL_GetActiveTexture:
    lw t0, %lo(GL_STATE_FLAGS)
    andi t1, t0, FLAG_TEXTURE_2D
    bnez t1, JrRa
    li s0, %lo(GL_BOUND_TEXTURES) + TEXTURE_OBJECT_SIZE
    andi t1, t0, FLAG_TEXTURE_1D
    bnez t1, JrRa
    li s0, %lo(GL_BOUND_TEXTURES)
    jr ra
    move s0, zero
    .endfunc

.func GL_MtxMult
GL_MtxMult:
    #define lhs s0
    #define rhs s1
    #define dst s2

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

#endif
