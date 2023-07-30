
    .section .text.gl_lighting

    ################################################################
    # GL_VtxLighting
    #   Perform lighting for a single vertex
    # Args:
    #   $v28 = Eye space position
    #   $v03 = Eye space normal
    #   $v04 = Vertex color RGBA
    # Returns:
    #   $v04 = Calculated RGBA
    ################################################################
    .func GL_VtxLighting
GL_VtxLighting:
    #define v___            $v01
    #define veyepos         $v28
    #define veyenormal      $v03
    #define vcolor          $v04
    #define vmemissive      $v05
    #define vmambient       $v06
    #define vmdiffuse       $v07
    #define vldiffuse       $v08
    #define vlout           $v09
    #define vlightdir       $v10
    #define vlightvec       $v11
    #define vsqdist_f       $v12
    #define vsqdist_i       $v13
    #define vdist_f         $v14
    #define vdist_i         $v15
    #define vinvdist_f      $v16
    #define vinvdist_i      $v17
    #define vndl            $v18
    #define vattenuation_f  $v19
    #define vattenuation_i  $v20
    #define venable         $v21
    #define vlmambient      $v22
    #define vcolortarget    $v23

    #define light_flags     t9
    #define light           s0
    #define light_end       s1

    #define r               e0
    #define g               e1
    #define b               e2
    #define a               e3


    li s0, %lo(GL_STATE_LIGHT_AMBIENT)

    vxor vlmambient, vlmambient
    ldv vlmambient,   0x00,s0

    # TODO: Prepare in DMEM?
    ldv vmambient.e0, 0x08,s0
    ldv vmambient.e4, 0x08,s0
    ldv vmdiffuse.e0, 0x10,s0
    ldv vmdiffuse.e4, 0x10,s0

    vxor vmemissive, vmemissive
    ldv vmemissive,   0x20,s0

    vxor vcolortarget, vcolortarget
    ldv vcolortarget,  0x28,s0

    lw light_flags, %lo(GL_STATE_FLAGS)

    # Initialize the third lane of vinvdist to 1.0
    vxor vinvdist_f, vinvdist_f
    vmov vinvdist_i.e2, K1
    vmov vinvdist_i.e6, K1

    andi t0, light_flags, FLAG_COLOR_MATERIAL
    beqz t0, gl_light_color_target_disabled
    veq  v___, vzero, vcolortarget.e0
    vmrg vmambient, vcolor
    veq  v___, vzero, vcolortarget.e1
    vmrg vmdiffuse, vcolor
    veq  v___, vzero, vcolortarget.h2
    vmrg vmemissive, vcolor

gl_light_color_target_disabled:

    li light, %lo(GL_LIGHTS)
    addi light_end, light, LIGHT_ATTR_SIZE * LIGHT_COUNT
    srl light_flags, LIGHT0_SHIFT

    vmulf vcolor, vlmambient, vmambient
    vadd vcolor, vmemissive

gl_light_loop:
    andi t0, light_flags, 3
    beqz t0, gl_light_loop_end
    andi t1, t0, 1
    andi t2, t0, 2

    beqz t1, light0_disabled
    mtc2 t1, venable.e0
    lh t1, LIGHT_POSITION_OFFSET+6(light)

light0_disabled:
    beqz t2, light1_disabled
    mtc2 t2, venable.e4
    lh t2, LIGHT_POSITION_OFFSET+LIGHT_ATTR_SIZE+6(light)

light1_disabled:
    # If the light is directional, the light vector is simply a direction (pre-normalized on CPU)
    lqv vlightdir,  LIGHT_POSITION_OFFSET,light
    lqv vlout,      LIGHT_AMBIENT_OFFSET,light
    add t1, t2
    beqz t1, gl_light_directional
    lqv vldiffuse,  LIGHT_DIFFUSE_OFFSET,light

    # Light is positional: We need to compute light vector, normalize it, and apply attenuation

    # Load attenuation coefficients k0, k1, k2 (constant, linear, quadratic)
    # vattenuation: k0  k1  k2  --  --  --  --  --
    lqv vattenuation_i,  LIGHT_ATTENUATION_INT_OFFSET,light
    lqv vattenuation_f,  LIGHT_ATTENUATION_FRAC_OFFSET,light

    # If light is positional, the light vector points from the vertex to the light position
    # This is shifted left by 5 because both values are in s10.5 format
    vsub vlightvec, vlightdir, veyepos
    
    # Dot product of light vector with itself
    # Product is shifted left by 10 because two s10.5 values were multiplied,
    # and then shifted right by 16 because of vsar.
    # This means the result is shifted right by 6
    vmudh v___, vlightvec, vlightvec
    vsar  vsqdist_f, COP2_ACC_MD
    vsar  vsqdist_i, COP2_ACC_HI
    vaddc vdist_f, vsqdist_f, vsqdist_f.h1
    vadd  vdist_i, vsqdist_i, vsqdist_i.h1
    vaddc vsqdist_f, vdist_f, vsqdist_f.h2
    vadd  vsqdist_i, vdist_i, vsqdist_i.h2

    # Compute inverse distance (reciprocal square root)
    # Result is shifted left by 10:
    #   - Taking the square root halves the bit-shift, and the reciprocal then inverts it.
    #     So the original (right) shift of -6 becomes -(-6/2) = 3
    #   - vrsq additionally shifts left by 7
    # vinvdist: --  1/d0  1.0  --  --  1/d1  1.0  --
    vrsqh v___.e0,       vsqdist_i.e0
    vrsql vinvdist_f.e1, vsqdist_f.e0
    vrsqh vinvdist_i.e1, vsqdist_i.e4
    vrsql vinvdist_f.e5, vsqdist_f.e4
    vrsqh vinvdist_i.e5, vzero.e0

    # Get actual distance by multiplying the inverse with the squared distance: d^-1 * d^2 = d^(2-1) = d
    # Because vinvdist.e2 is initialized to 1, the squared distance will be in vdist.e2
    # d is shifted left by 4, d^2 is still shifted right by 6
    # vdist: --  d0  d0^2  --  --  d1  d1^2  --
    vmudl v___,    vinvdist_f, vsqdist_f.h0
    vmadm v___,    vinvdist_i, vsqdist_f.h0
    vmadn vdist_f, vinvdist_f, vsqdist_i.h0
    vmadh vdist_i, vinvdist_i, vsqdist_i.h0

    # Multiply with attenuation coefficients
    # The coefficients are pre-shifted in such a way that all values end up being shifted right by 1, 
    # so the final result ends up non-shifted after the reciprocal below.
    # - d is shifted left by 4, so k1 is pre-shifted right by 4 on CPU
    # - d^2 is shifted right by 6, so k2 is pre-shifted left by 6 on CPU
    # vdist: --  k1*d0  k2*d0^2  --  --  k1*d1  k2*d1^2  --
    vmudl v___,    vdist_f, vattenuation_f
    vmadm v___,    vdist_i, vattenuation_f
    vmadn vdist_f, vdist_f, vattenuation_i
    vmadh vdist_i, vdist_i, vattenuation_i

    # Compute final attenuation factor
    # Sum is shifted right by 1
    # k0 + k1*d + k2*d^2
    vaddc vattenuation_f, vdist_f.h1
    vadd  vattenuation_i, vdist_i.h1
    vaddc vattenuation_f, vdist_f.h2
    vadd  vattenuation_i, vdist_i.h2
    # Final factor is not shifted
    # 1 / (k0 + k1*d + k2*d^2)
    vrcph v___.e0,           vattenuation_i.e0
    vrcpl vattenuation_f.e0, vattenuation_f.e0
    vrcph vattenuation_i.e0, vattenuation_i.e4
    vrcpl vattenuation_f.e4, vattenuation_f.e4
    vrcph vattenuation_i.e4, vzero.e0

    # Normalize light vector by multiplying the reciprocal distance.
    # Light vector is shifted left by 5 and inverse distance is shifted left by 10.
    # This means the result is shifted left by 15, which makes the result in vlightdir a signed fraction.
    # This happens to match perfectly so we can continue the following calculations without any adjustment.
    vmudm v___,      vlightvec, vinvdist_f.h1
    vmadh vlightvec, vlightvec, vinvdist_i.h1

gl_light_directional:

    vne v___, vzero, vlightdir.h3
    vmrg vlightdir, vlightvec, vlightdir
    vmrg vattenuation_i, vattenuation_i, K1
    vmrg vattenuation_f, vattenuation_f, vzero

    # Dot product of light vector with vertex normal
    # Both are a signed fraction, so we can just use vmulf
    vmulf vndl, veyenormal, vlightdir
    vadd  v___, vndl, vndl.h1
    vadd  vndl, v___, vndl.h2
    vge   vndl, vzero

    # Compute diffuse light
    vmulf vldiffuse, vndl.h0
    vmulf vldiffuse, vmdiffuse

    # Compute ambient light
    vmulf vlout, vmambient

    vadd vlout, vldiffuse

    # If light is positional, apply attenuation factor
    vmudm v___,  vlout, vattenuation_f.h0
    vmadh vlout, vlout, vattenuation_i.h0

    # Add final light to output (only those that are enabled)
    vne  v___, vzero, venable.h0
    vmrg vlout, vzero
    vadd vcolor, vlout

gl_light_loop_end:
    addi light, LIGHT_ATTR_SIZE*2
    blt light, light_end, gl_light_loop
    srl light_flags, 2

    vmov v___.e0, vcolor.e4
    vmov v___.e1, vcolor.e5
    vmov v___.e2, vcolor.e6
    vadd vcolor, v___

    jr ra
    vmov vcolor.a, vmdiffuse.a

    #undef v___
    #undef veyepos
    #undef veyenormal
    #undef vcolor
    #undef vmemissive
    #undef vmambient
    #undef vmdiffuse
    #undef vldiffuse
    #undef vlout
    #undef vlightdir
    #undef vlightvec
    #undef vsqdist_f
    #undef vsqdist_i
    #undef vdist_f
    #undef vdist_i
    #undef vinvdist_f
    #undef vinvdist_i
    #undef vndl
    #undef vattenuation_f
    #undef vattenuation_i
    #undef venable
    #undef vlmambient
    #undef vcolortarget
    #undef light_flags
    #undef light
    #undef light_end
    #undef r
    #undef g
    #undef b
    #undef a
    .endfunc