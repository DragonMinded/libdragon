
    .section .text.gl_lighting

    ################################################################
    # GL_VtxLighting
    #   Perform lighting for a single vertex
    # Args:
    #   $v02 = Eye space position (fractional part)
    #   $v03 = Eye space position (integer part)
    #   $v04 = Eye space normal (fractional part)
    #   $v05 = Eye space normal (integer part)
    #   $v06 = Vertex color RGBA
    # Returns:
    #   $v06 = Calculated RGBA
    ################################################################
    .func GL_VtxLighting
GL_VtxLighting:
    #define v___            $v01
    #define veyepos_f       $v02
    #define veyepos_i       $v03
    #define veyenormal_f    $v04
    #define veyenormal_i    $v05
    #define vcolor          $v06
    #define vmemissive      $v07
    #define vmambient       $v08
    #define vmdiffuse       $v09
    #define vlmambient      $v10
    #define vlpos           $v11
    #define vldiffuse       $v12
    #define vlout           $v13
    #define vposdiff        $v14
    #define vpdmag_f        $v15
    #define vpdmag_i        $v16
    #define vnormpd_f       $v17
    #define vnormpd_i       $v18
    #define vndl            $v19

    #define state_flags     t8
    #define light_flag      t9
    #define light           s0
    #define light_end       s1

    #define r               e0
    #define g               e1
    #define b               e2
    #define a               e3

    li s0, %lo(GL_STATE_LIGHT_AMBIENT)
    ldv vlmambient,  0x00,s0
    ldv vmambient,   0x08,s0
    ldv vmdiffuse,   0x10,s0
    ldv vmemissive,  0x20,s0

    lw state_flags, %lo(GL_STATE_FLAGS)
    li light_flag, FLAG_LIGHT0
    li light, %lo(GL_LIGHTS)
    addi light_end, light, LIGHT_SIZE * LIGHT_COUNT

    vmulf vlmambient, vmambient
    vadd vcolor, vmemissive, vlmambient

gl_light_loop:
    and t0, state_flags, light_flag
    beqz t0, gl_light_loop_end
    lh t1,          LIGHT_POSITION_OFFSET+6(light)
    ldv vlout,      LIGHT_AMBIENT_OFFSET,light
    ldv vlpos,      LIGHT_POSITION_OFFSET,light
    ldv vldiffuse,  LIGHT_DIFFUSE_OFFSET,light

    beqz t1, gl_light_directional
    vor vposdiff, vzero, vlpos

    vsub vposdiff, veyepos_i

    # TODO: Attenuation

gl_light_directional:
    # normalize
    vmudh v___, vposdiff, vposdiff
    vsar  vpdmag_f, COP2_ACC_MD
    vsar  vpdmag_i, COP2_ACC_HI

    vaddc vpdmag_f, vpdmag_f.e1
    vadd  vpdmag_i, vpdmag_i.e1
    vaddc vpdmag_f, vpdmag_f.e2
    vadd  vpdmag_i, vpdmag_i.e2

    vrsqh v___.e0,     vpdmag_i.e0
    vrsql vpdmag_f.e0, vpdmag_f.e0
    vrsqh vpdmag_i.e0, vzero.e0

    vmudm v___,      vposdiff, vpdmag_f.e0
    vmadh vnormpd_i, vposdiff, vpdmag_i.e0

    vmulf vndl, veyenormal_i, vnormpd_i
    vadd  vndl, vndl.e1
    vadd  vndl, vndl.e2
    vge   vndl, vzero

    vmulf vldiffuse, vndl.e0
    vmulf vldiffuse, vmdiffuse
    vadd vlout, vldiffuse

    vadd vcolor, vlout

gl_light_loop_end:
    addi light, LIGHT_SIZE
    blt light, light_end, gl_light_loop
    sll light_flag, 1

    jr ra
    vmov vcolor.a, vmdiffuse.a

    #undef v___
    #undef veye_f
    #undef veye_i
    #undef vcolor
    #undef vmemissive
    #undef vmambient
    #undef vmdiffuse
    #undef vlmambient
    #undef state_flags
    #undef light_flag
    #undef light
    #undef light_end
    #undef r
    #undef g
    #undef b
    #undef a
    .endfunc