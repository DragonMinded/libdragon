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

#endif
