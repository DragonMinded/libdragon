GL_STATE:
    # This is the GL state that is also used by the pipeline.
    GL_MATRICES:
        GL_MATRIX_MODELVIEW:    .ds.b   MATRIX_SIZE
        GL_MATRIX_PROJECTION:   .ds.b   MATRIX_SIZE
        GL_MATRIX_TEXTURE:      .ds.b   MATRIX_SIZE
        GL_MATRIX_PALETTE:      .ds.b   MATRIX_SIZE
        GL_MATRIX_MVP:          .ds.b   MATRIX_SIZE
    GL_LIGHTS:              .ds.b   LIGHT_STRUCT_SIZE
    GL_TEX_GEN:             .ds.b   TEX_GEN_STRUCT_SIZE
    GL_VIEWPORT_SCALE:      .half   0,0,0,0
    GL_VIEWPORT_OFFSET:     .half   0,0,0,0
    GL_STATE_LIGHT_AMBIENT: .half   0,0,0,0
    GL_MAT_AMBIENT:         .half   0,0,0,0
    GL_MAT_DIFFUSE:         .half   0,0,0,0
    GL_MAT_SPECULAR:        .half   0,0,0,0
    GL_MAT_EMISSIVE:        .half   0,0,0,0
    GL_MAT_COLOR_TARGET:    .half   0,0,0
    GL_MAT_SHININESS:       .half   0
    GL_CURRENT_ATTRIBUTES:
        GL_CUR_COLOR:       .half   0,0,0,0
        GL_CUR_TEX_COORDS:  .half   0,0,0,0
        GL_CUR_NORMAL:      .byte   0,0,0
        GL_CUR_MTX_INDEX:   .byte   0
    GL_MATRIX_POINTERS:     
        GL_MODELVIEW_PTR:       .word   0
        GL_PROJECTION_PTR:      .word   0
        GL_TEXTURE_PTR:         .word   0
        GL_PALETTE_PTR:         .word   0
        GL_MVP_PTR:             .word   0
    GL_LOADED_MTX_INDEX:    .word   0,0
    GL_STATE_FLAGS:         .word   0
    GL_STATE_FOG_PARAMS:    .half   0,0,0,0
    GL_STATE_TEX_SIZE:      .half   0,0
    GL_STATE_TEX_OFFSET:    .half   0,0
    GL_STATE_POLYGON_MODE:  .half   0
    GL_STATE_PRIM_TYPE:     .half   0
    GL_STATE_CULL_MODE:     .half   0
    GL_STATE_FRONT_FACE:    .half   0
    GL_STATE_SHADE_MODEL:   .half   0
    GL_STATE_POINT_SIZE:    .half   0
    GL_STATE_LINE_WIDTH:    .half   0
    GL_STATE_MATRIX_MODE:   .half   0
    GL_TRI_CMD:             .half   0
    GL_TRI_CULL:            .byte   0,0
    .align 3
GL_STATE_END:

#define GL_STATE_SIZE       (GL_STATE_END - GL_STATE)
