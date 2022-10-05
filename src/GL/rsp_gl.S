#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_SetFlag,       8
        RSPQ_DefineCommand GLCmd_SetByte,       8
        RSPQ_DefineCommand GLCmd_SetShort,      8
        RSPQ_DefineCommand GLCmd_SetWord,       8
        RSPQ_DefineCommand GLCmd_SetLong,       12
        RSPQ_DefineCommand GLCmd_Update,        4
        RSPQ_DefineCommand GLCmd_BindTexture,   8
        RSPQ_DefineCommand GLCmd_GetValue,      8
        RSPQ_DefineCommand GLCmd_CopyFillColor, 4
        RSPQ_DefineCommand GLCmd_SetLightPos,   12
        RSPQ_DefineCommand GLCmd_SetLightDir,   8
        RSPQ_DefineCommand GLCmd_MatrixPush,    4
        RSPQ_DefineCommand GLCmd_MatrixPop,     4
        RSPQ_DefineCommand GLCmd_MatrixLoad,    68
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
GL_STATE:
    # These are required by the pipeline
    GL_BOUND_TEXTURES:      .ds.b   TEXTURE_OBJECT_SIZE * 2
    GL_MATRICES:
    GL_MATRIX_MODELVIEW:    .ds.b   MATRIX_SIZE
    GL_MATRIX_PROJECTION:   .ds.b   MATRIX_SIZE
    GL_MATRIX_TEXTURE:      .ds.b   MATRIX_SIZE
    GL_LIGHTS:              .ds.b   LIGHT_SIZE * LIGHT_COUNT
    GL_TEX_GEN:             .ds.b   TEX_GEN_SIZE * 4
    GL_VIEWPORT_SCALE:      .half   0,0,0,0
    GL_VIEWPORT_OFFSET:     .half   0,0,0,0
    GL_TEX_GEN_MODE:        .half   0,0,0,0
    GL_CUR_TEX_COORDS:      .half   0,0,0,0
    GL_CUR_NORMAL:          .byte   0,0,0,0
    GL_CUR_COLOR:           .word   0
    GL_MATRIX_POINTERS:     .word   0,0,0
    GL_MAT_AMBIENT:         .word   0
    GL_MAT_DIFFUSE:         .word   0
    GL_MAT_SPECULAR:        .word   0
    GL_MAT_EMISSIVE:        .word   0
    GL_MAT_SHININESS:       .half   0
    GL_MAT_COLOR_TARGET:    .half   0
    GL_STATE_FLAGS:         .word   0
    GL_STATE_LIGHT_AMBIENT: .word   0
    GL_STATE_FOG_START:     .word   0
    GL_STATE_FOG_END:       .word   0
    GL_STATE_POLYGON_MODE:  .half   0
    GL_STATE_PRIM_TYPE:     .half   0
    GL_STATE_CULL_MODE:     .half   0
    GL_STATE_FRONT_FACE:    .half   0
    GL_STATE_SHADE_MODEL:   .half   0
    GL_STATE_POINT_SIZE:    .half   0
    GL_STATE_LINE_WIDTH:    .half   0
    GL_STATE_MATRIX_MODE:   .half   0

    # These are only required for RDP state changes
    GL_STATE_SCISSOR_RECT:  .half   0, 0, 0, 0  # Needs to be aligned to 8 bytes
    GL_STATE_BLEND_CYCLE:   .word   0
    GL_STATE_FOG_COLOR:     .word   0
    GL_STATE_TEXTURE_IDS:   .word   0, 0
    GL_STATE_UPLOADED_TEX:  .word   0
    GL_STATE_FILL_COLOR:    .word   0
    GL_STATE_FILL_DEPTH:    .word   0
    GL_STATE_FB_SIZE:       .half   0, 0
    GL_STATE_DEPTH_FUNC:    .half   0
    GL_STATE_ALPHA_FUNC:    .half   0
    GL_STATE_BLEND_SRC:     .half   0
    GL_STATE_BLEND_DST:     .half   0
    GL_STATE_TEX_ENV_MODE:  .half   0
    GL_STATE_ALPHA_REF:     .byte   0
    RSPQ_EndSavedState

COMBINER_TABLE:
    # No texture
    .quad RDPQ_COMBINER1((0, 0, 0, SHADE), (0, 0, 0, SHADE))         # "modulate"
    .quad RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM))           # constant "modulate"
    .quad RDPQ_COMBINER1((0, 0, 0, SHADE), (0, 0, 0, SHADE))         # "replace"
    .quad RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM))           # constant "replace"

    # Texture enabled
    .quad RDPQ_COMBINER1((TEX0, 0, SHADE, 0), (TEX0, 0, SHADE, 0))   # modulate
    .quad RDPQ_COMBINER1((TEX0, 0, PRIM, 0), (TEX0, 0, PRIM, 0))     # constant modulate
    .quad RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, TEX0))           # replace
    .quad RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, TEX0))           # constant replace

COMBINER_MIPMAPMASK_TABLE: 
    .quad RDPQ_COMB1_MASK
    .quad RDPQ_COMB1_MASK
    .quad RDPQ_COMB1_MASK
    .quad RDPQ_COMB1_MASK

    .quad RDPQ_COMB1_MASK ^ (1<<37) ^ (1<<21)
    .quad RDPQ_COMB1_MASK ^ (1<<37) ^ (1<<21)
    .quad RDPQ_COMB1_MASK ^ (1<<6) ^ (1<<0)
    .quad RDPQ_COMB1_MASK ^ (1<<6) ^ (1<<0)

UPDATE_FUNCTIONS:
    .short RSPQ_Loop                    - _start  # Do nothing
    .short GL_UpdateDepthTest           - _start
    .short GL_UpdateDepthMask           - _start
    .short GL_UpdateBlend               - _start
    .short GL_UpdateDither              - _start
    .short GL_UpdatePoints              - _start
    .short GL_UpdateAlphaTest           - _start
    .short GL_UpdateBlendCycle          - _start
    .short GL_UpdateFogCycle            - _start
    .short GL_UpdateScissor             - _start
    .short GL_UpdateCombiner            - _start
    .short GL_UpdateTexture             - _start
    .short GL_UpdateTextureCompleteness - _start
    .short GL_UpdateTextureUpload       - _start

CONVERT_CONST:  .half 1, 2, 0x8000, 0x200, 0xFF, 134, 0x7F, 0x80

    .text

    #############################################################
    # GLCmd_SetFlag
    #
    # Sets or clears a flag
    #
    # ARGS:
    #   a0: Bit 31..24: Command id
    #       Bit 23..12: Offset into UPDATE_FUNCTIONS jump table
    #       Bit 11..2: Offset of flag value in GL_STATE
    #       Bit 0:     If 1, set the flag, otherwise clear it
    #   a1: flag mask (inverted if clearing)
    #############################################################
    .func GLCmd_SetFlag
GLCmd_SetFlag:
    li t0, ~0x3
    and t0, a0, t0
    andi t1, a0, 1
    lw t2, %lo(GL_STATE)(t0)
    beqz t1, flag_clear
    and t3, t2, a1
    or t3, t2, a1

flag_clear:
    j GLCmd_Update
    sw t3, %lo(GL_STATE)(t0)
    .endfunc

    .func GLCmd_SetByte
GLCmd_SetByte:
    j GLCmd_Update
    sb a1, %lo(GL_STATE)(a0)
    .endfunc

    .func GLCmd_SetShort
GLCmd_SetShort:
    j GLCmd_Update
    sh a1, %lo(GL_STATE)(a0)
    .endfunc

    .func GLCmd_SetWord
GLCmd_SetWord:
    j GLCmd_Update
    sw a1, %lo(GL_STATE)(a0)
    .endfunc

    .func GLCmd_SetLong
GLCmd_SetLong:
    sw a1, %lo(GL_STATE) + 0(a0)
    sw a2, %lo(GL_STATE) + 4(a0)
    # fallthrough!
    .endfunc

GLCmd_Update:
    srl t0, a0, 12
    lhu t1, %lo(UPDATE_FUNCTIONS)(t0)
    jr t1
    nop

GLCmd_GetValue:
    srl t0, a0, 12
    addiu s4, a0, %lo(GL_STATE)
    andi s4, 0xFFF
    andi t0, 0xFFF
    j DMAOut
    move s0, a1

GLCmd_BindTexture:
    sll t3, a0, 2
    lw s0, %lo(GL_STATE_TEXTURE_IDS)(t3)
    # Do nothing if texture is already bound
    beq s0, a1, RSPQ_Loop
    sll s4, a0, TEXTURE_OBJECT_SIZE_LOG
    addiu s4, %lo(GL_BOUND_TEXTURES)

    # DMA currently bound texture out
    jal DMAOutAsync
    li t0, DMA_SIZE(TEXTURE_OBJECT_DMA_SIZE, 1)

    # DMA new texture in
    jal DMAIn
    move s0, a1

    jal GL_UpdateTexture
    sw s0, %lo(GL_STATE_TEXTURE_IDS)(t3)

    jal_and_j GL_UpdateCombiner, RSPQ_Loop

GLCmd_CopyFillColor:
    lw t0, %lo(GL_STATE)(a0)
    jr ra
    sw t0, %lo(RDPQ_FILL_COLOR)

GLCmd_SetLightPos:
    #define v___     $v01

    #define vpos     $v02

    #define vmtx0_i  $v03
    #define vmtx0_f  $v04
    #define vmtx1_i  $v05
    #define vmtx1_f  $v06
    #define vmtx2_i  $v07
    #define vmtx2_f  $v08
    #define vmtx3_i  $v09
    #define vmtx3_f  $v10

    addi s0, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 8
    li s1, %lo(GL_MATRIX_MODELVIEW)
    ldv vpos,     0x00,s0
    ldv vmtx0_i,  0x00,s1
    ldv vmtx1_i,  0x08,s1
    ldv vmtx2_i,  0x10,s1
    ldv vmtx3_i,  0x18,s1
    ldv vmtx0_f,  0x20,s1
    ldv vmtx1_f,  0x28,s1
    ldv vmtx2_f,  0x30,s1
    ldv vmtx3_f,  0x38,s1

    vmudn v___, vmtx0_f, vpos.e0
    vmadh v___, vmtx0_i, vpos.e0
    vmadn v___, vmtx1_f, vpos.e1
    vmadh v___, vmtx1_i, vpos.e1
    vmadn v___, vmtx2_f, vpos.e2
    vmadh v___, vmtx2_i, vpos.e2
    vmadn v___, vmtx3_f, vpos.e3
    vmadh vpos, vmtx3_i, vpos.e3

    addi s0, a0, %lo(GL_STATE) + LIGHT_POSITION_OFFSET
    jr ra
    sdv vpos,  0x00,s0

    #undef v___

    #undef vpos

    #undef vmtx0_i
    #undef vmtx0_f
    #undef vmtx1_i
    #undef vmtx1_f
    #undef vmtx2_i
    #undef vmtx2_f
    #undef vmtx3_i
    #undef vmtx3_f

GLCmd_SetLightDir:
    #define v___     $v01

    #define vpos     $v02

    #define vmtx0_i  $v03
    #define vmtx0_f  $v04
    #define vmtx1_i  $v05
    #define vmtx1_f  $v06
    #define vmtx2_i  $v07
    #define vmtx2_f  $v08

    addi s0, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 4
    li s1, %lo(GL_MATRIX_MODELVIEW)
    lpv vpos,     0x00,s0
    ldv vmtx0_i,  0x00,s1
    ldv vmtx1_i,  0x08,s1
    ldv vmtx2_i,  0x10,s1
    ldv vmtx0_f,  0x20,s1
    ldv vmtx1_f,  0x28,s1
    ldv vmtx2_f,  0x30,s1

    # TODO: verify this

    vmudn v___, vmtx0_f, vpos.e0
    vmadh v___, vmtx0_i, vpos.e0
    vmadn v___, vmtx1_f, vpos.e1
    vmadh v___, vmtx1_i, vpos.e1
    vmadn v___, vmtx2_f, vpos.e2
    vmadh vpos, vmtx2_i, vpos.e2

    addi s0, a0, %lo(GL_STATE) + LIGHT_DIRECTION_OFFSET
    sbv vpos.e0,  0,s0
    sbv vpos.e1,  1,s0
    jr ra
    sbv vpos.e2,  2,s0

    #undef v___

    #undef vpos

    #undef vmtx0_i
    #undef vmtx0_f
    #undef vmtx1_i
    #undef vmtx1_f
    #undef vmtx2_i
    #undef vmtx2_f
    #undef vmtx3_i
    #undef vmtx3_f

# ARGS: t1: matrix index
GL_MatrixMarkDirty:
    lw t9, %lo(GL_STATE_FLAGS)
    li t8, FLAG_MTX_MV_DIRTY
    sllv t8, t8, t1 # 2-arg syntax doesn't work with sllv for some reason...?
    or t9, t8
    jr ra
    sw t9, %lo(GL_STATE_FLAGS)

GLCmd_MatrixPush:
    j GL_MatrixPushPop
    li t2, DMA_OUT_ASYNC

GLCmd_MatrixPop:
    li t2, DMA_IN
    # fallthrough!
GL_MatrixPushPop:
    #define post_incr   t0
    #define mtx_index   t1
    #define stack_ptr   t3

    # Get matrix pointer for the current matrix stack
    lbu mtx_index, %lo(GL_STATE_MATRIX_MODE) + 1
    sll stack_ptr, mtx_index, 2
    lw s0, %lo(GL_MATRIX_POINTERS)(stack_ptr)

    # Get location in DMEM for the current matrix stack
    sll s4, mtx_index, 6
    addi s4, %lo(GL_MATRICES)
    
    bltz t2, gl_matrix_push
    # If pushing the stack, post-increment the stack pointer after the DMA
    li post_incr, MATRIX_SIZE

    # If popping the stack, pre-decrement the stack pointer before the DMA
    addi s0, -MATRIX_SIZE
    # Also mark the matrix as dirty
    jal GL_MatrixMarkDirty
    move post_incr, zero

gl_matrix_push:
    add s1, s0, post_incr

    # Save new stack pointer
    sw s1, %lo(GL_MATRIX_POINTERS)(stack_ptr)

    # Do DMA
    li t0, DMA_SIZE(MATRIX_SIZE, 1)
    jal_and_j DMAExec, RSPQ_Loop

GLCmd_MatrixLoad:
    #define multiply    t0
    #define mtx_index   t1
    #define dst         s1
    #define src         s2

    #define v___         $v01

    #define vrhs01_i     $v02
    #define vrhs01_f     $v03
    #define vrhs23_i     $v04
    #define vrhs23_f     $v05

    #define vmtx0_i      $v06
    #define vmtx0_f      $v07
    #define vmtx1_i      $v08
    #define vmtx1_f      $v09
    #define vmtx2_i      $v10
    #define vmtx2_f      $v11
    #define vmtx3_i      $v12
    #define vmtx3_f      $v13

    andi multiply, a0, 1
    lbu mtx_index, %lo(GL_STATE_MATRIX_MODE) + 1
    addi src, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 64
    sll dst, mtx_index, 6
    addiu dst, %lo(GL_MATRICES)

    # Load the right hand side matrix from command parameters (misaligned)
    lqv vrhs01_i, 0x00,src
    lrv vrhs01_i, 0x10,src
    lqv vrhs23_i, 0x10,src
    lrv vrhs23_i, 0x20,src
    lqv vrhs01_f, 0x20,src
    lrv vrhs01_f, 0x30,src
    lqv vrhs23_f, 0x30,src
    beqz multiply, gl_matrix_save # skip multiplication if only loading the matrix
    lrv vrhs23_f, 0x40,src

    # Load left hand side matrix columns, repeating each column twice in a register
    ldv vmtx0_i.e0,  0x00,dst
    ldv vmtx0_i.e4,  0x00,dst
    ldv vmtx1_i.e0,  0x08,dst
    ldv vmtx1_i.e4,  0x08,dst
    ldv vmtx2_i.e0,  0x10,dst
    ldv vmtx2_i.e4,  0x10,dst
    ldv vmtx3_i.e0,  0x18,dst
    ldv vmtx3_i.e4,  0x18,dst
    ldv vmtx0_f.e0,  0x20,dst
    ldv vmtx0_f.e4,  0x20,dst
    ldv vmtx1_f.e0,  0x28,dst
    ldv vmtx1_f.e4,  0x28,dst
    ldv vmtx2_f.e0,  0x30,dst
    ldv vmtx2_f.e4,  0x30,dst
    ldv vmtx3_f.e0,  0x38,dst
    ldv vmtx3_f.e4,  0x38,dst

    # Perform matrix x matrix multiplication
    vmudl v___,     vmtx0_f, vrhs01_f.h0
    vmadm v___,     vmtx0_i, vrhs01_f.h0
    vmadn v___,     vmtx0_f, vrhs01_i.h0
    vmadh v___,     vmtx0_i, vrhs01_i.h0

    vmadl v___,     vmtx1_f, vrhs01_f.h1
    vmadm v___,     vmtx1_i, vrhs01_f.h1
    vmadn v___,     vmtx1_f, vrhs01_i.h1
    vmadh v___,     vmtx1_i, vrhs01_i.h1

    vmadl v___,     vmtx2_f, vrhs01_f.h2
    vmadm v___,     vmtx2_i, vrhs01_f.h2
    vmadn v___,     vmtx2_f, vrhs01_i.h2
    vmadh v___,     vmtx2_i, vrhs01_i.h2

    vmadl v___,     vmtx3_f, vrhs01_f.h3
    vmadm v___,     vmtx3_i, vrhs01_f.h3
    vmadn vrhs01_f, vmtx3_f, vrhs01_i.h3
    vmadh vrhs01_i, vmtx3_i, vrhs01_i.h3

    vmudl v___,     vmtx0_f, vrhs23_f.h0
    vmadm v___,     vmtx0_i, vrhs23_f.h0
    vmadn v___,     vmtx0_f, vrhs23_i.h0
    vmadh v___,     vmtx0_i, vrhs23_i.h0

    vmadl v___,     vmtx1_f, vrhs23_f.h1
    vmadm v___,     vmtx1_i, vrhs23_f.h1
    vmadn v___,     vmtx1_f, vrhs23_i.h1
    vmadh v___,     vmtx1_i, vrhs23_i.h1

    vmadl v___,     vmtx2_f, vrhs23_f.h2
    vmadm v___,     vmtx2_i, vrhs23_f.h2
    vmadn v___,     vmtx2_f, vrhs23_i.h2
    vmadh v___,     vmtx2_i, vrhs23_i.h2

    vmadl v___,     vmtx3_f, vrhs23_f.h3
    vmadm v___,     vmtx3_i, vrhs23_f.h3
    vmadn vrhs23_f, vmtx3_f, vrhs23_i.h3
    vmadh vrhs23_i, vmtx3_i, vrhs23_i.h3

gl_matrix_save:
    # Save the resulting matrix
    sqv vrhs01_i, 0x00,dst
    sqv vrhs23_i, 0x10,dst
    sqv vrhs01_f, 0x20,dst
    j GL_MatrixMarkDirty
    sqv vrhs23_f, 0x30,dst

GL_UpdateDepthTest:
    lw t0, %lo(GL_STATE_FLAGS)
    lhu t1, %lo(GL_STATE_DEPTH_FUNC)

    andi t0, FLAG_DEPTH_TEST            # a2 = (GL_STATE_FLAGS & FLAG_DEPTH_TEST)
    beqz t0, depth_test_disable         #
    lw t2, %lo(RDPQ_OTHER_MODES) + 0x4  #
    xori t1, GL_LESS                    #      && (GL_STATE_DEPTH_FUNC == GL_LESS)
    sltu t1, 1                          #
    sll t0, t1, SOM_Z_COMPARE_SHIFT     #      ? SOM_Z_COMPARE : 0;
depth_test_disable:
    li t3, ~DEPTH_TEST_MASK
    and t2, t3
    or t2, t0
    jr ra
    sw t2, %lo(RDPQ_OTHER_MODES) + 0x4

GL_UpdateAlphaTest:
    lw t0, %lo(GL_STATE_FLAGS)
    lhu t1, %lo(GL_STATE_ALPHA_FUNC)

    andi t0, FLAG_ALPHA_TEST            
    beqz t0, alpha_test_disable         
    lw t2, %lo(RDPQ_OTHER_MODES) + 0x4
    xori t1, GL_GREATER                 
    sltu t0, t1, 1
alpha_test_disable:
    li t3, ~ALPHA_TEST_MASK
    and t2, t3
    or t2, t0
    jr ra
    sw t2, %lo(RDPQ_OTHER_MODES) + 0x4

GL_UpdateDepthMask:
    lw t0, %lo(GL_STATE_FLAGS)
    andi t0, FLAG_DEPTH_TEST | FLAG_DEPTH_MASK
    lw t1, %lo(RDPQ_OTHER_MODES) + 0x4
    xori t0, FLAG_DEPTH_TEST | FLAG_DEPTH_MASK
    li t2, ~(DEPTH_MASK_MASK)
    sltu t0, 1
    and t1, t2
    sll t0, SOM_Z_WRITE_SHIFT
    or t0, t1
    jr ra
    sw t0, %lo(RDPQ_OTHER_MODES) + 0x4

GL_UpdateBlend:
    lw t0, %lo(GL_STATE_FLAGS)
    andi t1, t0, FLAG_BLEND
    lw t2, %lo(RDPQ_OTHER_MODES) + 0x4
    beqz t1, blend_disable
    li t0, SOM_ZMODE_OPAQUE
    li t0, SOM_ZMODE_TRANSPARENT
blend_disable:
    li t1, ~(BLEND_MASK)
    and t2, t1
    or t2, t0
    jr ra
    sw t2, %lo(RDPQ_OTHER_MODES) + 0x4

GL_UpdateDither:
    lw t0, %lo(GL_STATE_FLAGS)
    andi t1, t0, FLAG_DITHER
    lw t2, %lo(RDPQ_OTHER_MODES) + 0x0
    beqz t1, dither_disable
    li t0, (SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE) >> 32
    li t0, (SOM_RGBDITHER_SQUARE | SOM_ALPHADITHER_SAME) >> 32
dither_disable:
    li t1, ~(DITHER_MASK >> 32)
    and t2, t1
    or t2, t0
    jr ra
    sw t2, %lo(RDPQ_OTHER_MODES) + 0x0

# output: v0
GL_IsPoints:
    lhu t0, %lo(GL_STATE_PRIM_TYPE)
    beqz t0, JrRa                       # prim_type == GL_POINTS
    li v0, 1

    addi t0, -1
    sltu t0, (GL_TRIANGLES - 1)
    bne t0, zero, JrRa                  # prim_type == GL_LINES || prim_type == GL_LINE_LOOP || prim_type == GL_LINE_STRIP
    move v0, zero

    lhu t0, %lo(GL_STATE_POLYGON_MODE)
    xori t0, GL_POINT
    jr ra
    sltu v0, t0, 1                      # polygon_mode == GL_POINT

GL_UpdatePoints:
    move ra2, ra
    jal GL_IsPoints
    move t2, zero

    bnez v0, is_points
    sll t3, v0, SOM_ZSOURCE_SHIFT

    li t2, SOM_TEXTURE_PERSP >> 32

is_points:
    lw t4, %lo(RDPQ_OTHER_MODES) + 0x0
    lw t5, %lo(RDPQ_OTHER_MODES) + 0x4
    li t6, ~(SOM_TEXTURE_PERSP >> 32)
    li t7, ~(SOM_ZSOURCE_MASK)
    and t4, t6
    and t5, t7
    or t4, t2
    or t5, t3
    sw t4, %lo(RDPQ_OTHER_MODES) + 0x0
    jr ra2
    sw t5, %lo(RDPQ_OTHER_MODES) + 0x4

GL_UpdateBlendCycle:
    lw t0, %lo(GL_STATE_FLAGS)
    andi t0, FLAG_BLEND
    beqz t0, blend_cycle_none
    move t1, zero
    lw t1, %lo(GL_STATE_BLEND_CYCLE)
blend_cycle_none:
    jr ra
    sw t1, %lo(RDPQ_MODE_BLENDER_STEPS) + 0x4

GL_UpdateFogCycle:
    lw t0, %lo(GL_STATE_FLAGS)
    andi t0, FLAG_FOG
    beqz t0, fog_cycle_none
    sltu t2, zero, t0
    li t0, RDPQ_BLENDER((IN_RGB, SHADE_ALPHA, FOG_RGB, INV_MUX_ALPHA)) | SOM_BLENDING
fog_cycle_none:
    sw t0, %lo(RDPQ_MODE_BLENDER_STEPS) + 0x0
    lw t1, %lo(RDPQ_OTHER_MODES) + 0x0
    li t3, ~(SOMX_FOG >> 32)
    and t1, t3
    or t1, t2
    jr ra
    sw t1, %lo(RDPQ_OTHER_MODES) + 0x0

GL_UpdateScissor:
    lhu t6, %lo(GL_STATE_FB_SIZE) + 0x0
    lhu t7, %lo(GL_STATE_FB_SIZE) + 0x2
    lw t4, %lo(GL_STATE_FLAGS)
    andi t4, FLAG_SCISSOR_TEST
    beqz t4, scissor_disabled
    move t2, zero

    lhu t2, %lo(GL_STATE_SCISSOR_RECT) + 0x2
    lhu t4, %lo(GL_STATE_SCISSOR_RECT) + 0x0
    subu t7, t2
    lhu t6, %lo(GL_STATE_SCISSOR_RECT) + 0x4
    lhu t2, %lo(GL_STATE_SCISSOR_RECT) + 0x6
    addu t6, t4
    subu t2, t7, t2
    
scissor_disabled:
    sll t2, 2
    sll t4, 14
    or t4, t2
    lui t2, 0xED00
    or t4, t2
    sll t7, 2
    sll t6, 14
    or t6, t7
    sw t4, %lo(RDPQ_SCISSOR_RECT) + 0x0
    jr ra
    sw t6, %lo(RDPQ_SCISSOR_RECT) + 0x4

# OUTPUT: s0 (zero if none is active)
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

GL_UpdateCombiner:
    move ra2, ra
    jal GL_IsPoints
    lw t3, %lo(GL_STATE_TEX_ENV_MODE)

    li t4, GL_REPLACE
    bne t3, t4, gl_combiner_modulate
    nop
    ori v0, 0x2

gl_combiner_modulate:
    jal GL_GetActiveTexture
    nop

    beqz s0, gl_combiner_no_texture
    lw t0, TEXTURE_FLAGS_OFFSET(s0)
    andi t0, TEX_FLAG_COMPLETE
    srl t0, 1
    or v0, t0

gl_combiner_no_texture:
    sll v0, 3
    lw t0, %lo(COMBINER_TABLE) + 0x0(v0)
    lw t1, %lo(COMBINER_TABLE) + 0x4(v0)
    lw t2, %lo(COMBINER_MIPMAPMASK_TABLE) + 0x0(v0)
    lw t3, %lo(COMBINER_MIPMAPMASK_TABLE) + 0x4(v0)

    sw t0, %lo(RDPQ_COMBINER) + 0x0
    sw t1, %lo(RDPQ_COMBINER) + 0x4
    sw t2, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0x0
    jr ra2
    sw t3, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0x4

GL_UpdateTexture:
    move ra2, ra
    jal GL_GetActiveTexture
    move t7, zero

    beqz s0, texture_set_modes
    lw t4, TEXTURE_FLAGS_OFFSET(s0)

    andi t5, t4, TEX_FLAG_COMPLETE
    beqz t5, texture_set_modes
    lhu t3, TEXTURE_MIN_FILTER_OFFSET(s0)
    lhu t2, TEXTURE_MAG_FILTER_OFFSET(s0)

    andi t5, t3, TEXTURE_MIPMAP_MASK
    or t2, t3
    andi t2, TEXTURE_BILINEAR_MASK
    beqz t5, texture_no_lod
    sll t2, 13 # shift to SOM_SAMPLE_BILINEAR
    or t2, SOM_TEXTURE_LOD >> 32

texture_no_lod:
    andi t3, TEXTURE_INTERPOLATE_MASK
    beqz t3, texture_no_interpolate
    nop # :(
    ori t2, SOMX_LOD_INTERPOLATE >> 32

texture_no_interpolate:
    andi t4, 0x7
    addi t4, -1
    sll t4, SOMX_NUMLODS_SHIFT - 32
    or t7, t4, t2

texture_set_modes:
    lw t2, %lo(RDPQ_OTHER_MODES)
    li t4, ~((SOM_SAMPLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE | SOMX_NUMLODS_MASK) >> 32)
    and t2, t4
    or t2, t7
    jr ra2
    sw t2, %lo(RDPQ_OTHER_MODES)

GL_UpdateTextureCompleteness:
    #define result      t7
    #define width       t1
    #define height      t2
    #define loop_max    t3
    #define one         t6
    #define loop_var    t0
    #define image       s1
    #define format      s2
    #define ra3         k0
    move result, zero   # levels = 0; complete = false

    # If either width or height is zero, the texture is incomplete
    lhu width, (%lo(GL_BOUND_TEXTURES) + IMAGE_WIDTH_OFFSET)(a0)
    beqz width, gl_tex_incomplete
    lhu height, (%lo(GL_BOUND_TEXTURES) + IMAGE_HEIGHT_OFFSET)(a0)
    beqz height, gl_tex_incomplete
    lhu t3, (%lo(GL_BOUND_TEXTURES) + TEXTURE_MIN_FILTER_OFFSET)(a0)

    # Otherwise, if mipmaps are disabled, the texture is complete and has a single level
    andi t3, TEXTURE_MIPMAP_MASK
    beqz t3, gl_tex_mipmaps_disabled
    li one, 1

    # Init the loop
    addiu image, a0, %lo(GL_BOUND_TEXTURES)
    lhu format, IMAGE_INTERNAL_FORMAT_OFFSET(image)
    move loop_var, zero
    li loop_max, MAX_TEXTURE_LEVELS - 1

gl_tex_completeness_loop:
    # Check that image size and format matches the expected values
    lhu t4, IMAGE_WIDTH_OFFSET(image)
    bne t4, width, gl_tex_incomplete
    lhu t5, IMAGE_HEIGHT_OFFSET(image)
    bne t5, height, gl_tex_incomplete
    lhu t4, IMAGE_INTERNAL_FORMAT_OFFSET(image)
    bne t4, format, gl_tex_incomplete
    or t5, width, height

    # TODO: is this true: (w | h) == 1 <=> (w == 1) && (h == 1)
    # If width and height are both 1 we have reached the end of the mipmap chain
    beq t5, one, gl_tex_complete
    andi t4, width, 0x1

    # If width is already 1 skip this check
    beq width, one, gl_check_height
    srl t5, width, 1

    # If width is odd and not equal to one it means the original width is not a power of two,
    # which is not allowed
    bnez t4, gl_tex_incomplete
    move width, t5

gl_check_height:
    andi t5, height, 0x1

    # If height is already 1 skip this check
    beq height, one, gl_completeness_step
    srl t4, height, 1
    
    # If height is odd and not equal to one it means the original height is not a power of two,
    # which is not allowed
    bnez t5, gl_tex_incomplete
    move height, t4

gl_completeness_step:
    # Check if we have reached the maximum number of loops
    beq loop_var, loop_max, gl_tex_incomplete
    addiu image, TEXTURE_IMAGE_SIZE

    # Loop!
    b gl_tex_completeness_loop
    addiu loop_var, 1

gl_tex_complete:
    move result, loop_var               # levels = i
gl_tex_mipmaps_disabled:
    addiu result, TEX_FLAG_COMPLETE | 1 # levels += 1; complete = true
gl_tex_incomplete:
    # Save the result
    lw t0, (%lo(GL_BOUND_TEXTURES) + TEXTURE_FLAGS_OFFSET)(a0)
    and t0, ~(TEX_FLAG_COMPLETE | TEX_LEVELS_MASK)
    or t0, result
    move ra3, ra
    jal GL_UpdateTexture
    sw t0, (%lo(GL_BOUND_TEXTURES) + TEXTURE_FLAGS_OFFSET)(a0)
    j GL_UpdateCombiner
    move ra, ra3
    #undef result
    #undef width
    #undef height
    #undef loop_max
    #undef one
    #undef loop_var
    #undef image
    #undef format
    #undef ra3

GL_UpdateTextureUpload:
    #define active_tex      s0
    #define uploaded_tex    s1
    #define tmem_addr       s2
    #define out_ptr         s3
    #define image           s5
    #define level           s6
    #define tex_id          s7
    #define num_levels      t4
    #define wrap_s          t5
    #define wrap_t          t6
    #define tex_flags       t7
    #define full_width_log  t8
    #define full_height_log t9
    #define mirror          k0

    move ra2, ra
    jal GL_GetActiveTexture
    lw uploaded_tex, %lo(GL_STATE_UPLOADED_TEX)

    beqz active_tex, JrRa
    move ra, ra2

    lw tex_flags, TEXTURE_FLAGS_OFFSET(active_tex)
    andi t0, tex_flags, TEX_FLAG_COMPLETE

    beqz t0, JrRa
    move tmem_addr, zero

    # Load ID of active texture
    li t0, %lo(GL_BOUND_TEXTURES)
    sub t0, active_tex, t0
    srl t0, (TEXTURE_OBJECT_SIZE_LOG - 2)
    lw tex_id, %lo(GL_STATE_TEXTURE_IDS)(t0)

    bne tex_id, uploaded_tex, gl_upload_tex
    andi t0, tex_flags, TEX_FLAG_UPLOAD_DIRTY

    beqz t0, JrRa
    nop

gl_upload_tex:
    lhu wrap_s, TEXTURE_WRAP_S_OFFSET(active_tex)
    lhu wrap_t, TEXTURE_WRAP_T_OFFSET(active_tex)
    lbu full_width_log, IMAGE_WIDTH_LOG_OFFSET(active_tex)
    lbu full_height_log, IMAGE_HEIGHT_LOG_OFFSET(active_tex)

    xori t0, wrap_s, GL_MIRRORED_REPEAT_ARB
    xori t1, wrap_t, GL_MIRRORED_REPEAT_ARB
    sltu t0, 1
    sltu t1, 1
    sll mirror, t0, 8
    sll t1, 18
    or mirror, t1

    xori wrap_s, GL_CLAMP
    xori wrap_t, GL_CLAMP

    li out_ptr, %lo(RDPQ_CMD_STAGING)
    move image, active_tex
    move level, zero
    andi num_levels, tex_flags, 0x7

gl_upload_loop:
    lw a0, IMAGE_TEX_IMAGE_OFFSET(image)
    lw a1, IMAGE_DATA_OFFSET(image)
    lw a2, IMAGE_SET_LOAD_TILE_OFFSET(image)
    lw a3, IMAGE_LOAD_BLOCK_OFFSET(image)

    add a2, tmem_addr
    lui t0, LOAD_TILE << 8
    lui t1, 0xF300

    # SET_TEX_IMAGE
    sw a0, 0x00(out_ptr)
    sw a1, 0x04(out_ptr)
    # SYNC_TILE
    lui a0, 0xE800
    sw a0, 0x08(out_ptr)
    sw zero, 0x0C(out_ptr)
    # SET_TILE
    sw a2, 0x10(out_ptr)
    sw t0, 0x14(out_ptr)
    # LOAD_BLOCK
    sw t1, 0x18(out_ptr)
    sw a3, 0x1C(out_ptr)

    lw a0, IMAGE_SET_TILE_OFFSET(image)
    lbu v0, IMAGE_WIDTH_LOG_OFFSET(image)
    lbu v1, IMAGE_HEIGHT_LOG_OFFSET(image)

    sll t0, level, 24
    add a0, tmem_addr

    # mask_s
    move a1, mirror
    beqz wrap_s, gl_clamp_s
    sll t1, v0, 4
    or a1, t1
gl_clamp_s:

    # mask_t
    beqz wrap_t, gl_clamp_t
    sll t1, v1, 14
    or a1, t1
gl_clamp_t:

    # shift_s, shift_t
    subu t1, full_width_log, v0
    subu t2, full_height_log, v1
    sll t2, 10
    or a1, t0
    or a1, t1
    or a1, t2

    lhu t1, IMAGE_WIDTH_OFFSET(image)
    lhu a3, IMAGE_HEIGHT_OFFSET(image)
    lui a2, 0xF200
    sll a3, 2
    sll t1, 14
    or a3, t1
    or a3, t0

    # SET_TILE
    sw a0, 0x20(out_ptr)
    sw a1, 0x24(out_ptr)
    # SET_TILE_SIZE
    sw a2, 0x28(out_ptr)
    sw a3, 0x2C(out_ptr)

    lhu t0, IMAGE_TMEM_SIZE_OFFSET(image)
    addiu level, 1
    addiu image, TEXTURE_IMAGE_SIZE
    add tmem_addr, t0
    blt level, num_levels, gl_upload_loop
    addiu out_ptr, 6 * 8

    li t0, ~TEX_FLAG_UPLOAD_DIRTY
    and tex_flags, t0
    sw tex_flags, TEXTURE_FLAGS_OFFSET(active_tex)
    sw tex_id, %lo(GL_STATE_UPLOADED_TEX)

    j RDPQ_Send
    li s4, %lo(RDPQ_CMD_STAGING)

/*
    #############################################################
    # Vec_Convert
    #
    # Converts a list of floats to fixed point (in-place)
    #
    # ARGS:
    #   s4: Address in DMEM of float list
    #   t0: Length
    #############################################################
Vec_Convert:
    #define mant_i $v01
    #define mant_f $v02

    #define exp    $v03
    #define nexp   $v04
    #define bit    $v05
    #define sign   $v06

    #define step_i $v07
    #define step_f $v08
    #define fact_i $v09
    #define fact_f $v10

    #define vconst $v11
    #define vhalf  $v12
    #define vtmp   $v13

    li t1, %lo(CONVERT_CONST)
    lqv vconst,0,  0,t1
    vor vhalf, vzero, vconst,e(2)
    add s1, s4, t0

convert_loop:
    lsv mant_i,0x0,  0x00,s4
    lsv mant_i,0x2,  0x04,s4
    lsv mant_i,0x4,  0x08,s4
    lsv mant_i,0x6,  0x0C,s4
    lsv mant_i,0x8,  0x10,s4
    lsv mant_i,0xA,  0x14,s4
    lsv mant_i,0xC,  0x18,s4
    lsv mant_i,0xE,  0x1C,s4
    lsv mant_f,0x0,  0x02,s4
    lsv mant_f,0x2,  0x06,s4
    lsv mant_f,0x4,  0x0A,s4
    lsv mant_f,0x6,  0x0E,s4
    lsv mant_f,0x8,  0x12,s4
    lsv mant_f,0xA,  0x16,s4
    lsv mant_f,0xC,  0x1A,s4
    lsv mant_f,0xE,  0x1E,s4

    vmudm exp, mant_i, vconst,e(3)      # exp = ((mant_i >> 7) & 0xFF)
    vand  exp, exp,    vconst,e(4)

    vsub sign, vzero,  vconst,e(0)      # sign = mant_i < 0 ? -1 : 1
    vlt  vtmp, mant_i, vzero
    vmrg sign, sign,   vconst,e(0)

    veq  vtmp, exp,   vzero             # sign = exp == 0 ? 0 : sign
    vmrg sign, vzero, sign

    vsub exp, exp, vconst,e(5)          # exp -= 134

    vsub nexp,   vzero, exp             # nexp = -exp
    vlt  vtmp,   exp,   vzero           # c = exp < 0
    vmrg exp,    nexp,  exp             # exp = c ? nexp : exp
    vmrg step_i, vzero, vconst,e(1)     # step = c ? 0.5 : 2
    vmrg step_f, vhalf, vzero     

    vand mant_i, mant_i, vconst,e(6)    # mant = (mant & 0x7F) | (1<<7)
    vor  mant_i, mant_i, vconst,e(7)

    li t0, 5
    
flt_2_fxd_loop:
    vand bit,    exp,    vconst,e(0)    # bit = exp & 1
    vlt  vtmp,   vzero,  bit            # c = 0 < bit
    vmrg fact_i, step_i, vconst,e(0)    # fact = c ? step : 1
    vmrg fact_f, step_f, vzero 

    vmudl vtmp,   mant_f, fact_f        # mant *= fact
    vmadm vtmp,   mant_i, fact_f
    vmadn mant_f, mant_f, fact_i
    vmadh mant_i, mant_i, fact_i

    vmudl vtmp,   step_f, step_f        # step *= step
    vmadm vtmp,   step_i, step_f
    vmadn step_f, step_f, step_i;   addi t0, -1
    vmadh step_i, step_i, step_i;   bgtz t0, flt_2_fxd_loop

    vmudm exp,    exp,    vhalf         # exp = exp >> 1

    vmudn vtmp,   mant_f, sign          # mant *= sign
    vmadh mant_i, mant_i, sign
    vmadn vtmp,   mant_f, vzero;    addi s4, 0x20

    sqv mant_i,0, -0x20,s4;         blt s4, s1, convert_loop
    sqv mant_f,0, -0x10,s4

    jr ra
    nop
*/

#include <rsp_rdpq.inc>
