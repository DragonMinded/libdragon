
    .section .text.gl_lighting

    ################################################################
    # GL_VtxLighting
    #   Perform lighting for a single vertex
    # Args:
    #   $v02 = Eye space position
    #   $v03 = Eye space normal
    #   $v04 = Vertex color RGBA
    # Returns:
    #   $v04 = Calculated RGBA
    ################################################################
    .func GL_VtxLighting
GL_VtxLighting:
    #define v___            $v01
    #define veyepos         $v02
    #define veyenormal      $v03
    #define vcolor          $v04
    #define vmemissive      $v05
    #define vmambient       $v06
    #define vmdiffuse       $v07
    #define vlmambient      $v08
    #define vldiffuse       $v09
    #define vlout           $v10
    #define vlightdir       $v11
    #define vsqdist_f       $v12
    #define vsqdist_i       $v13
    #define vdist_f         $v14
    #define vdist_i         $v15
    #define vinvdist_f      $v16
    #define vinvdist_i      $v17
    #define vndl            $v18
    #define vattenuation_f  $v19
    #define vattenuation_i  $v20

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

    # Initialize the third lane of vinvdist to 1.0
    vxor vinvdist_f, vinvdist_f
    vmov vinvdist_i.e2, K1

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

    # If the light is directional, the light vector is simply a direction (pre-normalized on CPU)
    ldv vlightdir,  LIGHT_POSITION_OFFSET,light
    ldv vlout,      LIGHT_AMBIENT_OFFSET,light
    beqz t1, gl_light_directional
    ldv vldiffuse,  LIGHT_DIFFUSE_OFFSET,light

    # Light is positional: We need to compute light vector, normalize it, and apply attenuation

    # Load attenuation coefficients k0, k1, k2 (constant, linear, quadratic)
    # vattenuation: k0  k1  k2  --  --  --  --  --
    ldv vattenuation_i.e0,  LIGHT_ATTENUATION_INTEGER_OFFSET,light
    ldv vattenuation_f.e0,  LIGHT_ATTENUATION_FRACTION_OFFSET,light

    # If light is positional, the light vector points from the vertex to the light position
    # This is shifted left by 5 because both values are in s10.5 format
    vsub vlightdir, veyepos
    
    # Dot product of light vector with itself
    # Product is shifted left by 10 because two s10.5 values were multiplied,
    # and then shifted right by 16 because of vsar.
    # This means the result is shifted right by 6
    vmudh v___, vlightdir, vlightdir
    vsar  vsqdist_f, COP2_ACC_MD
    vsar  vsqdist_i, COP2_ACC_HI
    vaddc vdist_f, vsqdist_f, vsqdist_f.e1
    vadd  vdist_i, vsqdist_i, vsqdist_i.e1
    vaddc vsqdist_f, vdist_f, vsqdist_f.e2
    vadd  vsqdist_i, vdist_i, vsqdist_i.e2

    # Compute inverse distance (reciprocal square root)
    # Result is shifted left by 10:
    #   - Taking the square root halves the bit-shift, and the reciprocal then inverts it.
    #     So the original (right) shift of -6 becomes -(-6/2) = 3
    #   - vrsq additionally shifts left by 7 (NOT by 8 as the manual claims!)
    # vinvdist: --  1/d  1.0  --  --  --  --  --
    vrsqh v___.e0,       vsqdist_i.e0
    vrsql vinvdist_f.e1, vsqdist_f.e0
    vrsqh vinvdist_i.e1, vzero.e0

    # Get actual distance by multiplying the inverse with the squared distance: d^-1 * d^2 = d^(2-1) = d
    # Because vinvdist.e2 is initialized to 1, the squared distance will be in vdist.e2
    # d is shifted left by 4, d^2 is still shifted right by 6
    # vdist: --  d  d^2  --  --  --  --  --  --
    vmudl v___,    vinvdist_f, vsqdist_f.e0
    vmadm v___,    vinvdist_i, vsqdist_f.e0
    vmadn vdist_f, vinvdist_f, vsqdist_i.e0
    vmadh vdist_i, vinvdist_i, vsqdist_i.e0

    # Multiply with attenuation coefficients
    # The coefficients are pre-shifted in such a way that all values end up being shifted right by 1, 
    # so the final result ends up non-shifted after the reciprocal below.
    # - d is shifted left by 4, so k1 is pre-shifted right by 4 on CPU
    # - d^2 is shifted right by 6, so k2 is pre-shifted left by 6 on CPU
    # vdist: --  k1*d  k2*d^2  --  --  --  --  --  --
    vmudl v___,    vdist_f, vattenuation_f
    vmadm v___,    vdist_i, vattenuation_f
    vmadn vdist_f, vdist_f, vattenuation_i
    vmadh vdist_i, vdist_i, vattenuation_i

    # Compute final attenuation factor
    # Sum is shifted right by 1
    # k0 + k1*d + k2*d^2
    vaddc vattenuation_f, vdist_f.e1
    vadd  vattenuation_i, vdist_i.e1
    vaddc vattenuation_f, vdist_f.e2
    vadd  vattenuation_i, vdist_i.e2
    # Final factor is not shifted
    # 1 / (k0 + k1*d + k2*d^2)
    vrcph v___.e0,           vattenuation_i.e0
    vrcpl vattenuation_f.e0, vattenuation_f.e0
    vrcph vattenuation_i.e0, vzero.e0

    # Normalize light vector by multiplying the reciprocal distance.
    # Light vector is shifted left by 5 and inverse distance is shifted left by 10.
    # This means the result is shifted left by 15, which makes the result in vlightdir a signed fraction.
    # This happens to match perfectly so we can continue the following calculations without any adjustment.
    vmudm v___,      vlightdir, vinvdist_f.e1
    vmadh vlightdir, vlightdir, vinvdist_i.e1

gl_light_directional:

    # Dot product of light vector with vertex normal
    # Both are a signed fraction, so we can just use vmulf
    vmulf vndl, veyenormal, vlightdir
    vadd  v___, vndl, vndl.e1
    vadd  vndl, v___, vndl.e2
    vge   vndl, vzero

    # Compute diffuse light
    vmulf vldiffuse, vndl.e0
    vmulf vldiffuse, vmdiffuse

    # Compute ambient light
    vmulf vlout, vmambient

    # TODO: Is it better to not branch here and instead multiply by 1?
    beqz t1, gl_light_no_attenuation
    vadd vlout, vldiffuse

    # If light is positional, apply attenuation factor
    vmudm v___,  vlout, vattenuation_f.e0
    vmadh vlout, vlout, vattenuation_i.e0

gl_light_no_attenuation:
    # Add final light to output
    vadd vcolor, vlout

gl_light_loop_end:
    addi light, LIGHT_SIZE
    blt light, light_end, gl_light_loop
    sll light_flag, 1

    jr ra
    vmov vcolor.a, vmdiffuse.a

    #undef v___
    #undef veyepos
    #undef veyenormal
    #undef vcolor
    #undef vmemissive
    #undef vmambient
    #undef vmdiffuse
    #undef vlmambient
    #undef vldiffuse
    #undef vlout
    #undef vlightdir
    #undef vsqdist_f
    #undef vsqdist_i
    #undef vdist_f
    #undef vdist_i
    #undef vinvdist_f
    #undef vinvdist_i
    #undef vndl
    #undef vattenuation_f
    #undef vattenuation_i
    #undef state_flags
    #undef light_flag
    #undef light
    #undef light_end
    #undef r
    #undef g
    #undef b
    #undef a
    .endfunc