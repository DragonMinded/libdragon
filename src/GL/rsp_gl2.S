#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"
#include <rsp_magma_state.inc>
#include "mgfx_constants.h"

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_PreInitPipe,   4   # 0x0
        RSPQ_DefineCommand GLCmd_PreInitPipeTex,4   # 0x1
        RSPQ_DefineCommand GLCmd_SetPipeline,   8   # 0x2
        RSPQ_DefineCommand GLCmd_PreInitMagma,  8   # 0x3
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
    #include "rsp_gl_state.inc"
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

    .align 4
MG_STATE_STAGING:
    _MgState 1
    .align 4
    MATRICES_MVP: .ds.b 64
    .align 4
    MATRICES_MV: .ds.b 64
    .align 4
    MATRICES_NORMAL: .ds.b 32
    .align 3
    TEXTURING_SCALE: .ds.b 4
    .align 1
    TEXTURING_OFFSET: .ds.b 4
    .align 3
    LIGHTING_LIGHTS: .ds.b 128
    .align 1
    LIGHTING_AMBIENT: .ds.b 6
    LIGHTING_HAS_POINTS: .ds.b 1
    LIGHTING_COUNT: .ds.b 1
    .align 3
    FOG_PARAMS: .ds.b 6
    FOG_MASK: .ds.b 1
    .align 3
MG_STATE_STAGING_END:

    .text

    .func GLCmd_PreInitPipe
GLCmd_PreInitPipe:
    sh a0, %lo(GL_STATE_PRIM_TYPE)

    #define state_flags  k1
    #define state_flags2 k0

    lw state_flags, %lo(GL_STATE_FLAGS)
    lw state_flags2, %lo(GL_STATE_FLAGS2)

    # Determine Culling mode

    #define cull_mode       t1
    #define front_face      t2
    #define cull_enabled    t3
    #define tri_cull        t4

    andi cull_enabled, state_flags, FLAG_CULL_FACE
    beqz cull_enabled, 1f
    # Any non-negative value other than 0 or 1 signifies that no faces should be culled
    li tri_cull, 2

    lhu cull_mode, %lo(GL_STATE_CULL_MODE)
    lhu front_face, %lo(GL_STATE_FRONT_FACE)

    # Set TRI_CULL to a negative number to cull all faces
    beq cull_mode, GL_FRONT_AND_BACK, 1f
    li tri_cull, -1

    # tri_cull = (cull_mode == GL_BACK) ^ (front_face == GL_CW)
    xori cull_mode, GL_FRONT
    sltu tri_cull, zero, cull_mode
    xori front_face, GL_CCW
    sltu front_face, zero, front_face
    xor tri_cull, front_face

1:
    sb tri_cull, %lo(GL_TRI_CULL)

    #undef cull_mode
    #undef front_face
    #undef cull_enabled
    #undef tri_cull

    #define modes0          t8
    #define modes1          t9
    #define is_points       v0
    #define active_tex      s7

    # Get Active texture
    
    andi t1, state_flags, FLAG_TEXTURE_2D
    bnez t1, 1f
    li active_tex, %lo(GL_BOUND_TEXTURE_2D)
    andi t1, state_flags, FLAG_TEXTURE_1D
    bnez t1, 1f
    li active_tex, %lo(GL_BOUND_TEXTURE_1D)
    move active_tex, zero
1:
    #define tri_cmd     t4
    srl tri_cmd, state_flags, TRICMD_ATTR_SHIFT
    andi tri_cmd, TRICMD_ATTR_MASK
    ori tri_cmd, 0xCC00
    sh tri_cmd, %lo(GL_TRI_CMD)

    #undef tri_cmd

    # Multisampling
    and t0, state_flags2, FLAG2_REDUCED_ALIASING
    or modes0, t0, (SOM_TF0_RGB | SOM_TF1_RGB) >> 32
    and modes1, state_flags2, FLAG2_MULTISAMPLE

    # Alpha test
    andi t0, state_flags, FLAG_ALPHA_TEST
    beqz t0, 1f         
    lhu t1, %lo(GL_STATE_ALPHA_FUNC)
    xori t1, GL_GREATER
    sltu t0, t1, 1
    or modes1, t0
1:

    # Depth mask
    and t0, state_flags, FLAG_DEPTH_TEST | FLAG_DEPTH_MASK
    xor t0, FLAG_DEPTH_TEST | FLAG_DEPTH_MASK
    sltu t0, 1
    sll t0, SOM_Z_WRITE_SHIFT
    or modes1, t0

    #define depth_func t3

    # Depth test
    and t0, state_flags, FLAG_DEPTH_TEST
    beqz t0, 1f
    lhu depth_func, %lo(GL_STATE_DEPTH_FUNC)
    xori t1, depth_func, GL_ALWAYS
    sltu t1, zero, t1
    sll t0, t1, SOM_Z_COMPARE_SHIFT
    or modes1, t0
1:

    # Z mode
    xori t1, depth_func, GL_EQUAL
    beqz t1, 1f
    li t0, SOM_ZMODE_DECAL
    xori t1, depth_func, GL_LESS_INTERPENETRATING_N64
    beqz t1, 1f
    li t0, SOM_ZMODE_INTERPENETRATING
    andi t0, state_flags, FLAG_BLEND
    # t0 = (state_flags & FLAG_BLEND) ? SOM_ZMODE_TRANSPARENT : SOM_ZMODE_OPAQUE
    sll t0, ZMODE_BLEND_FLAG_SHIFT
1:
    or modes1, t0

    #undef depth_func

    # Dither
    andi t1, state_flags, FLAG_DITHER
    beqz t1, 1f
    li t0, (SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE) >> 32
    lw t0, %lo(GL_STATE_DITHER_MODE)
1:
    or modes0, t0

    # Points
    lhu t0, %lo(GL_STATE_PRIM_TYPE)
    beqz t0, 1f                         # prim_type == GL_POINTS
    li is_points, 1

    addi t0, -1
    sltu t0, (GL_TRIANGLES - 1)
    bne t0, zero, 1f                    # prim_type == GL_LINES || prim_type == GL_LINE_LOOP || prim_type == GL_LINE_STRIP
    move is_points, zero

    lhu t0, %lo(GL_STATE_POLYGON_MODE)
    xori t0, GL_POINT
    sltu is_points, t0, 1               # polygon_mode == GL_POINT

1:
    bnez is_points, 1f
    sll t3, is_points, SOM_ZSOURCE_SHIFT
    or modes0, SOM_TEXTURE_PERSP >> 32
1:
    or modes1, t3

    # * Skip construction of CC/CB if FLAG2_USE_RDPQ_MATERIAL is active
    andi t1, state_flags2, FLAG2_USE_RDPQ_MATERIAL
    bnez t1, gl_skipcombinerblender

    # Blend cycle
    andi t0, state_flags, FLAG_BLEND
    beqz t0, 1f
    move t1, zero
    lw t1, %lo(GL_STATE_BLEND_CYCLE)
1:
    sw t1, %lo(RDPQ_MODE_BLENDER_STEPS) + 0x4

    # Fog cycle
    andi t0, state_flags, FLAG_FOG
    beqz t0, 1f
    move t2, zero
    li t2, RDPQ_BLENDER((FOG_RGB, SHADE_ALPHA, IN_RGB, INV_MUX_ALPHA)) | SOM_BLENDING
1:
    sw t2, %lo(RDPQ_MODE_BLENDER_STEPS) + 0x0

    # Combiner
    move t5, is_points
    lw t3, %lo(GL_STATE_TEX_ENV_MODE)

    li t4, GL_REPLACE
    bne t3, t4, 1f
    nop
    ori t5, COMBINER_FLAG_REPLACE
1:
    srl t0, state_flags, TEX_ACTIVE_COMBINER_SHIFT
    andi t0, COMBINER_FLAG_TEXTURE
    or t5, t0

    sll t5, 3
    lw t0, %lo(COMBINER_TABLE) + 0x0(t5)
    lw t1, %lo(COMBINER_TABLE) + 0x4(t5)
    lw t2, %lo(COMBINER_MIPMAPMASK_TABLE) + 0x0(t5)
    lw t3, %lo(COMBINER_MIPMAPMASK_TABLE) + 0x4(t5)

    # TODO: The following is sort of equivalent to RDPQCmd_ResetMode. Maybe make that callable from ucode?

    sw t0, %lo(RDPQ_COMBINER) + 0x0
    sw t1, %lo(RDPQ_COMBINER) + 0x4
    sw t2, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0x0
    sw t3, %lo(RDPQ_COMBINER_MIPMAPMASK) + 0x4

gl_skipcombinerblender:

    # * Skip specific GL texture flags if FLAG2_USE_RDPQ_TEXTURING is active and instead apply RDPQ ones

    andi t1, state_flags2, FLAG2_USE_RDPQ_TEXTURING
    bnez t1, gl_mergemask
    nop

    # Texture
    beqz active_tex, 2f
    lw t4, TEXTURE_FLAGS_OFFSET(active_tex)

    andi t5, t4, TEX_FLAG_COMPLETE
    beqz t5, 2f
    lhu t3, TEXTURE_MIN_FILTER_OFFSET(active_tex)
    lhu t2, TEXTURE_MAG_FILTER_OFFSET(active_tex)

    andi t5, t3, TEXTURE_MIPMAP_MASK
    or t2, t3
    andi t2, TEXTURE_BILINEAR_MASK

    # If bilinear sampling is active, texture coords need to be offset by half a texel,
    # which is 0x10 in s10.5
    andi t7, state_flags2, FLAG2_TEX_FLIP_T
    lh t6, %lo(GL_STATE_TEX_SIZE) + 2
    sll t1, t2, TEX_BILINEAR_OFFSET_SHIFT
    neg t1, t1
    beqz t7, 1f
    sh t1, %lo(GL_STATE_TEX_OFFSET) + 0
    sra t6, 1
    sub t1, t6
1:
    sh t1, %lo(GL_STATE_TEX_OFFSET) + 2

    beqz t5, 1f
    sll t2, TEX_BILINEAR_SHIFT
    or t2, SOM_TEXTURE_LOD >> 32

    andi t5, t4, TEX_FLAG_DETAIL
    sll t5, TEX_DETAIL_SHIFT
    or t2, t5

1:
    andi t3, TEXTURE_INTERPOLATE_MASK
    beqz t3, 1f
    nop # :(
    ori t2, SOMX_LOD_INTERPOLATE >> 32

1:
    # TODO: Combine into a single field?
    lbu t0, TEXTURE_LEVELS_COUNT_OFFSET(active_tex)
    lbu t1, TEXTURE_TLUT_MODE_OFFSET(active_tex)

    sll t0, SOMX_NUMLODS_SHIFT - 32
    sll t1, SOM_TLUT_SHIFT - 32

    or t0, t1
    or t7, t0, t2
    or modes0, t7
2:

    j 1f
    nop # :(
gl_mergemask:
    # If we have FLAG2_USE_RDPQ_TEXTURING active, we need to use some of the RDPQ params instead of GL's

    lw t1, %lo(RDPQ_OTHER_MODES) + 0x0
    and t1, RDPQ_TEXTURING_MASK
    and t2, modes0, ~(RDPQ_TEXTURING_MASK)
    or  modes0, t1, t2


1:

    lw t0, %lo(RDPQ_OTHER_MODES) + 0x0

    sw modes0, %lo(RDPQ_OTHER_MODES) + 0x0
    sw modes1, %lo(RDPQ_OTHER_MODES) + 0x4

    # If cycle mode was copy or fill, update scissor rect
    sll t0, ((64-2)-SOM_CYCLE_SHIFT)
    bgez t0, 1f
    lw a0, %lo(RDPQ_SCISSOR_RECT) + 0x0
    jal RDPQ_WriteSetScissor
    lw a1, %lo(RDPQ_SCISSOR_RECT) + 0x4

1:

    j RDPQ_UpdateRenderMode
    sw state_flags, %lo(GL_STATE_FLAGS)

    #undef modes0
    #undef modes1
    #undef is_points
    #undef active_tex
    #undef state_flags
    .endfunc

    .func GLCmd_PreInitPipeTex
GLCmd_PreInitPipeTex:
    #define tex_id          s0
    #define uploaded_tex    s1
    #define active_tex      s7
    #define active_tex_id   s6
    #define tex_flags       t7
    #define state_flags     k1
    #define state_flags2    k0

    # Skip processing textutre state if we are using rdpq API for texturing
    # (FLAG2_USE_RDPQ_TEXTURING)
    lw state_flags2, %lo(GL_STATE_FLAGS2)
    andi t1, state_flags2, FLAG2_USE_RDPQ_TEXTURING
    beqz t1, 2f
    lw state_flags, %lo(GL_STATE_FLAGS)

    or state_flags, FLAG_TEXTURE_ACTIVE
    sw state_flags, %lo(GL_STATE_FLAGS)

    # If bilinear sampling is active, texture coords need to be offset by half a texel,
    # which is 0x10 in s10.5
    lw t2, %lo(RDPQ_OTHER_MODES) + 0x4
    and t2, (SOM_SAMPLE_BILINEAR >> 32)
    sll t2, t2, (SOM_SAMPLE_SHIFT - 32 + 1)

    andi t3, state_flags2, FLAG2_TEX_FLIP_T
    lh t6, %lo(GL_STATE_TEX_SIZE) + 2
    sll t1, t2, TEX_BILINEAR_OFFSET_SHIFT
    neg t1, t1
    beqz t7, 1f
    sh t1, %lo(GL_STATE_TEX_OFFSET) + 0
    sra t6, 1
    sub t1, t6

1:
    sh t1, %lo(GL_STATE_TEX_OFFSET) + 2
    j RSPQ_Loop

2:
    # Get pointer to active texture state in DMEM
    li active_tex_id, %lo(GL_STATE_TEXTURE_IDS) + 0x4
    andi t1, state_flags, FLAG_TEXTURE_2D
    bnez t1, 1f
    li active_tex, %lo(GL_BOUND_TEXTURE_2D)
    andi t1, state_flags, FLAG_TEXTURE_1D
    li active_tex_id, %lo(GL_STATE_TEXTURE_IDS) + 0x0
    bnez t1, 1f
    li active_tex, %lo(GL_BOUND_TEXTURE_1D)
    j gl_set_texture_not_active
1:
    lw uploaded_tex, %lo(GL_STATE_UPLOADED_TEX)

    # CHeck if the texture is complete (that is, all mipmaps have been loaded)
    # otherwise, we consider it not complete and avoid displaying it
    lw tex_flags, TEXTURE_FLAGS_OFFSET(active_tex)
    andi t0, tex_flags, TEX_FLAG_COMPLETE
    beqz t0, gl_set_texture_not_active

    # Copy width/height of the active texture into the state
    lh t0, IMAGE_WIDTH_OFFSET(active_tex)
    lh t1, IMAGE_HEIGHT_OFFSET(active_tex)
    andi t2, state_flags2, FLAG2_TEX_FLIP_T
    beqz t2, 1f
    sh t0, %lo(GL_STATE_TEX_SIZE) + 0
    neg t1, t1
1:
    sh t1, %lo(GL_STATE_TEX_SIZE) + 2

    # Make sure FLAG_TEXTURE_ACTIVE is set
    or state_flags, FLAG_TEXTURE_ACTIVE
    sw state_flags, %lo(GL_STATE_FLAGS)

    # Load ID of active texture (that is, RDRAM pointer)
    lw tex_id, (active_tex_id)
    
    # Proceed with the upload if:
    #  * the requested texture ID is different from the one currently uploaded to TMEM
    #  * the dirty flag is set
    bne tex_id, uploaded_tex, 1f
    andi t0, tex_flags, TEX_FLAG_UPLOAD_DIRTY
    beqz t0, RSPQ_Loop
#    nop

1:
    # OK we are going to do the upload. Update the state: clear dirty flag and
    # set the current texture ID.
    li t0, ~TEX_FLAG_UPLOAD_DIRTY
    and tex_flags, t0
    sw tex_flags, TEXTURE_FLAGS_OFFSET(active_tex)
    sw tex_id, %lo(GL_STATE_UPLOADED_TEX)

    # Do the upload: we tail call to the RSPQ block that is within 
    # the texture state (at offset TEXTURE_LEVELS_BLOCK_OFFSET). This
    # block was recorded by glTexImageN64 and contains the commands
    # necessary to upload the texture(s) to TMEM.
    addi a0, tex_id, TEXTURE_LEVELS_BLOCK_OFFSET
    j RSPQCmd_Call
    li a1, 1<<2   # nesting level 1

gl_set_texture_not_active:
    and state_flags, ~FLAG_TEXTURE_ACTIVE
    j RSPQ_Loop
    sw state_flags, %lo(GL_STATE_FLAGS)

    #undef active_tex
    #undef uploaded_tex
    #undef tex_id
    #undef tex_flags
    #undef state_flags
    #undef state_flags2
    .endfunc

    #############################################################
    # GLCmd_SetPipeline
    #
    # ARGS:
    #   a0: Bit 31..24: Command id
    #       Bit 23..0: RDRAM pointer to shader text
    #   a1: Bit 31..16: Offset into GL_PIPELINES
    #       Bit 15..0: shader text size
    #############################################################
    .func GLCmd_SetPipeline
GLCmd_SetPipeline:
    srl t0, a1, 16
    sw a0, %lo(GL_PIPELINES) + 0(t0)
    jr ra
    sh a1, %lo(GL_PIPELINES) + 6(t0)
    .endfunc

.set noreorder
.set noat
.set nomacro

#undef zero
#undef at
#undef v0
#undef v1
#undef a0
#undef a1
#undef a2
#undef a3
#undef t0
#undef t1
#undef t2
#undef t3
#undef t4
#undef t5
#undef t6
#undef t7
#undef s0
#undef s1
#undef s2
#undef s3
#undef s4
#undef s5
#undef s6
#undef s7
#undef t8
#undef t9
#undef k0
#undef k1
#undef gp
#undef sp
#undef fp
#undef ra
.equ hex.$zero, 0
.equ hex.$at, 1
.equ hex.$v0, 2
.equ hex.$v1, 3
.equ hex.$a0, 4
.equ hex.$a1, 5
.equ hex.$a2, 6
.equ hex.$a3, 7
.equ hex.$t0, 8
.equ hex.$t1, 9
.equ hex.$t2, 10
.equ hex.$t3, 11
.equ hex.$t4, 12
.equ hex.$t5, 13
.equ hex.$t6, 14
.equ hex.$t7, 15
.equ hex.$s0, 16
.equ hex.$s1, 17
.equ hex.$s2, 18
.equ hex.$s3, 19
.equ hex.$s4, 20
.equ hex.$s5, 21
.equ hex.$s6, 22
.equ hex.$s7, 23
.equ hex.$t8, 24
.equ hex.$t9, 25
.equ hex.$k0, 26
.equ hex.$k1, 27
.equ hex.$gp, 28
.equ hex.$sp, 29
.equ hex.$fp, 30
.equ hex.$ra, 31
#define vco 0
#define vcc 1
#define vce 2

GLCmd_PreInitMagma:
  lw $k0, %lo(GL_STATE_FLAGS + 0)                    ## L:1064 |      ^ | u32<$k0> flags = load(GL_STATE_FLAGS, 0);
  andi $s7, $k0, 2048                                ## L:1068 |    **4 | u32 tmp = flags & 0x800;
  beq $s7, $zero, LABEL_GLCmd_PreInitMagma_0001      ## L:1069 |      5 | if (tmp != 0) {
  or $k1, $zero, $zero                               ## L:1066 |     *7 | u32<$k1> diffuse_tracking_color = 0;
  lhu $s6, %lo(GL_MAT_COLOR_TARGET + 0)              ## L:1070 |      8 | u16 color_material = load(GL_MAT_COLOR_TARGET);
  addiu $at, $zero, 4609                             ## L:1071 |      9 | if (color_material == 0x1201) {
  bne $s6, $at, LABEL_GLCmd_PreInitMagma_0002        ## L:1071 |    *11 | if (color_material == 0x1201) {
  nop                                                ## L:1071 |    *13 | if (color_material == 0x1201) {
  addiu $k1, $zero, 1                                ## L:1072 |     14 | diffuse_tracking_color = 1;
  LABEL_GLCmd_PreInitMagma_0002:
  addiu $at, $zero, 5634                             ## L:1074 |     15 | if (color_material == 0x1602) {
  bne $s6, $at, LABEL_GLCmd_PreInitMagma_0001        ## L:1074 |     16 | if (color_material == 0x1602) {
  nop                                                ## L:1074 |    *18 | if (color_material == 0x1602) {
  addiu $k1, $zero, 1                                ## L:1075 |     19 | diffuse_tracking_color = 1;
  LABEL_GLCmd_PreInitMagma_0001:
  lui $s6, 0x3000                                    ## L:1084 |     20 | u32 env_mask = 0x30000000;
  and $s5, $k0, $s6                                  ## L:1085 |     21 | u32 tmp = flags & env_mask;
  bne $s5, $s6, LABEL_GLCmd_PreInitMagma_0004        ## L:1086 |     22 | if (tmp == env_mask)
  or $s7, $zero, $zero                               ## L:1082 |    *24 | u32 features = 0;
  addiu $s4, $zero, 9218                             ## L:1088 |     25 | u16 sphere_map = 0x2402;
  lhu $s3, %lo(GL_TEX_GEN + 128)                     ## L:1089 |     26 | u16 mode_s = load(GL_TEX_GEN, 128);
  bne $s3, $s4, LABEL_GLCmd_PreInitMagma_0004        ## L:1091 |   **29 | if (mode_s == sphere_map)
  lhu $s2, %lo(GL_TEX_GEN + 130)                     ## L:1090 |    *31 | u16 mode_t = load(GL_TEX_GEN, 130);
  bne $s2, $s4, LABEL_GLCmd_PreInitMagma_0004        ## L:1093 |   **34 | if (mode_t == sphere_map)
  nop                                                ## L:1093 |    *36 | if (mode_t == sphere_map)
  ori $s7, $s7, 1                                    ## L:1095 |     37 | features |= 1;
  LABEL_GLCmd_PreInitMagma_0004:
  andi $s5, $k0, 1024                                ## L:1100 |     38 | tmp = flags & 0x400;
  beq $s5, $zero, LABEL_GLCmd_PreInitMagma_0007      ## L:1101 |     39 | if (tmp != 0)
  nop                                                ## L:1101 |    *41 | if (tmp != 0)
  bne $k1, $zero, LABEL_GLCmd_PreInitMagma_0007      ## L:1103 |     42 | if (diffuse_tracking_color == 0) {
  nop                                                ## L:1103 |    *44 | if (diffuse_tracking_color == 0) {
  ori $s7, $s7, 2                                    ## L:1104 |     45 | features |= 2;
  LABEL_GLCmd_PreInitMagma_0007:
  sll $s7, $s7, 3                                    ## L:1108 |     46 | features = features << 3;
  addiu $at, $s7, GL_PIPELINES                       ## L:1110 |     47 | vec16 pipeline = load(features, GL_PIPELINES).xyzw;
  ldv $v28, 0, 0, $at                                ## L:1110 |     48 | vec16 pipeline = load(features, GL_PIPELINES).xyzw;
  ori $at, $zero, %lo(MG_SHADER_TEXT)                ## L:1111 |     49 | store(pipeline.xyzw, MG_SHADER_TEXT);
  sdv $v28, 0, 0, $at                                ## L:1111 |   **52 | store(pipeline.xyzw, MG_SHADER_TEXT);
  ori $at, $zero, %lo(GL_VIEWPORT_SCALE)             ## L:1116 |     53 | vec16 viewport = load(GL_VIEWPORT_SCALE, 0);
  lqv $v28, 0, 0, $at                                ## L:1116 |     54 | vec16 viewport = load(GL_VIEWPORT_SCALE, 0);
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:1117 |     55 | store(viewport, MG_VIEWPORT, 0);
  lh $s7, %lo(GL_MATRIX_PROJECTION + 22)             ## L:1123 |     56 | s16 m23i = load(GL_MATRIX_PROJECTION, 0x16);
  lhu $s6, %lo(GL_MATRIX_PROJECTION + 54)            ## L:1124 |     57 | u16 m23f = load(GL_MATRIX_PROJECTION, 0x36);
  or $s5, $s7, $s6                                   ## L:1125 |   **60 | u16 m23 = m23i | m23f;
  bne $s5, $zero, LABEL_GLCmd_PreInitMagma_0009      ## L:1126 |     61 | if (m23 == 0)
  sqv $v28, 0, 0, $at                                ## L:1117 |    *63 | store(viewport, MG_VIEWPORT, 0);
  ori $s4, $zero, 0xFFFF                             ## L:1130 |     64 | u16 norm_factor = 0xFFFF;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_000A    ## L:1131 |     65 | store(norm_factor, MG_VIEWPORT, 0x6);
  sh $s4, %lo(6 + MG_VIEWPORT)($zero)                ## L:1131 |    *67 | store(norm_factor, MG_VIEWPORT, 0x6);
  LABEL_GLCmd_PreInitMagma_0009:
  ori $at, $zero, %lo(GL_MATRIX_PROJECTION)          ## L:1153 |     68 | m:sint = load(GL_MATRIX_PROJECTION, 0x10);
  lqv $v27, 0, 16, $at                               ## L:1153 |     69 | m:sint = load(GL_MATRIX_PROJECTION, 0x10);
  lqv $v28, 0, 48, $at                               ## L:1154 |     70 | m:ufract = load(GL_MATRIX_PROJECTION, 0x30);
  vmudl $v29, $v28, $v28.e2                          ## L:1157 |  ***74 | vec32 a = m * m.z;
  vmadm $v29, $v27, $v28.e2                          ## L:1157 |     75 | vec32 a = m * m.z;
  vmadn $v26, $v28, $v27.e2                          ## L:1157 |     76 | vec32 a = m * m.z;
  vmadh $v25, $v27, $v27.e2                          ## L:1157 |     77 | vec32 a = m * m.z;
  vsubc $v24, $v26, $v00.e7                          ## L:1159 |   **80 | vec32 b = a - 1;
  vsub $v23, $v25, $v30.e7                           ## L:1159 |     81 | vec32 b = a - 1;
  vrcph $v25.e0, $v25.e6                             ## L:1161 |     82 | a.x = invert_half(a).Z;
  vrcpl $v26.e0, $v26.e6                             ## L:1161 |     83 | a.x = invert_half(a).Z;
  vrcph $v25.e0, $v00.e6                             ## L:1161 |     84 | a.x = invert_half(a).Z;
  vmudl $v29, $v26, $v24.e2                          ## L:1163 |   **87 | a *= b.z;
  vmadm $v29, $v25, $v24.e2                          ## L:1163 |     88 | a *= b.z;
  vmadn $v26, $v26, $v23.e2                          ## L:1163 |     89 | a *= b.z;
  vmadh $v25, $v25, $v23.e2                          ## L:1163 |     90 | a *= b.z;
  vmudn $v26, $v26, $v30.e6                          ## L:1164 |   **93 | a *= 2;
  vmadh $v25, $v25, $v30.e6                          ## L:1164 |     94 | a *= 2;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:1166 |      ^ | store(a:sfract.x, MG_VIEWPORT, 0x6);
  ssv $v26, 0, 6, $at                                ## L:1166 |   **97 | store(a:sfract.x, MG_VIEWPORT, 0x6);
  LABEL_GLCmd_PreInitMagma_000A:
  lhu $s7, %lo(GL_TRI_CMD + 0)                       ## L:1172 |     98 | u16 tri_cmd = load(GL_TRI_CMD, 0);
  sh $s7, %lo(0 + MG_TRI_CMD)($zero)                 ## L:1173 |  **101 | store(tri_cmd, MG_TRI_CMD, 0);
  lbu $s7, %lo(GL_TRI_CULL + 0)                      ## L:1178 |    102 | u8 cull = load(GL_TRI_CULL, 0);
  sb $s7, %lo(0 + MG_CULL_MODE)($zero)               ## L:1179 |  **105 | store(cull, MG_CULL_MODE, 0);
  ori $s7, $zero, %lo(MATRICES_MVP)                  ## L:1191 |    106 | mtx_dst = MATRICES_MVP;
  andi $s3, $k0, 16384                               ## L:1193 |    107 | u32 palette_enabled = flags & 0x4000;
  beq $s3, $zero, LABEL_GLCmd_PreInitMagma_000B      ## L:1194 |    108 | if (palette_enabled != 0)
  ori $s5, $zero, %lo(GL_MATRIX_PROJECTION)          ## L:1190 |   *110 | mtx_lhs = GL_MATRIX_PROJECTION;
  lw $s2, %lo(GL_STATE_PALETTE_INDEX + 0)            ## L:1196 |    111 | u32 palette_index = load(GL_STATE_PALETTE_INDEX);
  lw $s0, %lo(GL_PALETTE_PTR + 0)                    ## L:1197 |    112 | rdram_ptr = load(GL_PALETTE_PTR);
  ori $s4, $zero, %lo(GL_MATRIX_PALETTE)             ## L:1199 |    113 | dmem_ptr = GL_MATRIX_PALETTE;
  ori $t0, $zero, 63                                 ## L:1200 |    114 | dma_out_async(dmem_ptr, rdram_ptr, 64);
  addiu $t2, $zero, -32768                           ## L:1200 |    115 | dma_out_async(dmem_ptr, rdram_ptr, 64);
  jal DMAExec                                        ## L:1200 |    116 | dma_out_async(dmem_ptr, rdram_ptr, 64); ## Args: $t0, $t1, $s0, $s4, $t2
  addu $s0, $s0, $s2                                 ## L:1198 |   *118 | rdram_ptr += palette_index;
  ori $s6, $zero, %lo(MATRICES_MV)                   ## L:1202 |    119 | mtx_rhs = MATRICES_MV;
  ori $s4, $zero, %lo(MATRICES_MVP)                  ## L:1204 |    120 | dmem_ptr = MATRICES_MVP;
  lw $s0, %lo(GL_PALETTE_PTR + 0)                    ## L:1205 |    121 | rdram_ptr = load(GL_PALETTE_PTR);
  or $s1, $zero, $zero                               ## L:1206 |    122 | u16 i = 0;
  LABEL_GLCmd_PreInitMagma_000D:
  sltiu $at, $s1, 32                                 ## L:1207 |    123 | while (i < 32) {
  beq $at, $zero, LABEL_GLCmd_PreInitMagma_000E      ## L:1207 |    124 | while (i < 32) {
  nop                                                ## L:1207 |   *126 | while (i < 32) {
  addiu $t2, $zero, 12                               ## L:1208 |    127 | dma_in(dmem_ptr, rdram_ptr, 160);
  jal DMAExec                                        ## L:1208 |    128 | dma_in(dmem_ptr, rdram_ptr, 160); ## Args: $t0, $t1, $s0, $s4, $t2
  ori $t0, $zero, 159                                ## L:1208 |   *130 | dma_in(dmem_ptr, rdram_ptr, 160);
  ori $at, $zero, %lo(MATRICES_MV)                   ## L:1212 |    131 | m0:sint   = load(MATRICES_MV, 0x00);
  lqv $v27, 0, 0, $at                                ## L:1212 |    132 | m0:sint   = load(MATRICES_MV, 0x00);
  lqv $v25, 0, 16, $at                               ## L:1213 |    133 | m1:sint   = load(MATRICES_MV, 0x10);
  lqv $v28, 0, 32, $at                               ## L:1214 |    134 | m0:ufract = load(MATRICES_MV, 0x20);
  lqv $v26, 0, 48, $at                               ## L:1215 | ***138 | m1:ufract = load(MATRICES_MV, 0x30);
  vmudl $v28, $v28, $v31.e0                          ## L:1218 |      ^ | m0 = m0 >> 1;
  vmadm $v27, $v27, $v31.e0                          ## L:1218 |    139 | m0 = m0 >> 1;
  vmadn $v28, $v00, $v00                             ## L:1218 |    140 | m0 = m0 >> 1;
  vmudl $v26, $v26, $v31.e0                          ## L:1219 |   *142 | m1 = m1 >> 1;
  vmadm $v25, $v25, $v31.e0                          ## L:1219 |    143 | m1 = m1 >> 1;
  vmadn $v26, $v00, $v00                             ## L:1219 |    144 | m1 = m1 >> 1;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:1221 |      ^ | store(m0:sfract, MATRICES_NORMAL, 0x00);
  sqv $v26, 0, 16, $at                               ## L:1222 | ***148 | store(m1:sfract, MATRICES_NORMAL, 0x10);
  jal GL_MtxMult                                     ## L:1225 |    149 | GL_MtxMult();
  sqv $v28, 0, 0, $at                                ## L:1221 |   *151 | store(m0:sfract, MATRICES_NORMAL, 0x00);
  addiu $t2, $zero, -32768                           ## L:1226 |    152 | dma_out_async(dmem_ptr, rdram_ptr, 160);
  jal DMAExec                                        ## L:1226 |    153 | dma_out_async(dmem_ptr, rdram_ptr, 160); ## Args: $t0, $t1, $s0, $s4, $t2
  ori $t0, $zero, 159                                ## L:1226 |   *155 | dma_out_async(dmem_ptr, rdram_ptr, 160);
  addiu $s1, $s1, 1                                  ## L:1228 |    156 | i += 1;
  j LABEL_GLCmd_PreInitMagma_000D                    ## L:1228 |    157 | i += 1;
  addiu $s0, $s0, 160                                ## L:1227 |   *159 | rdram_ptr += 160;
  LABEL_GLCmd_PreInitMagma_000E:
  lbu $fp, %lo(GL_CUR_MTX_INDEX + 0)                 ## L:1231 |    160 | u8 cur_mtx_index = load(GL_CUR_MTX_INDEX);
  sll $sp, $fp, 7                                    ## L:1232 |  **163 | u32 tmp = cur_mtx_index << 7;
  sll $s2, $fp, 5                                    ## L:1233 |    164 | palette_index = cur_mtx_index << 5;
  addu $s2, $s2, $sp                                 ## L:1234 |    165 | palette_index += tmp;
  lw $s0, %lo(GL_PALETTE_PTR + 0)                    ## L:1236 |    166 | rdram_ptr = load(GL_PALETTE_PTR);
  ori $t0, $zero, 159                                ## L:1238 |    167 | dma_in(dmem_ptr, rdram_ptr, 160);
  addiu $t2, $zero, 12                               ## L:1238 |    168 | dma_in(dmem_ptr, rdram_ptr, 160);
  jal DMAExec                                        ## L:1238 |    169 | dma_in(dmem_ptr, rdram_ptr, 160); ## Args: $t0, $t1, $s0, $s4, $t2
  addu $s0, $s0, $s2                                 ## L:1237 |   *171 | rdram_ptr += palette_index;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_000C    ## L:1238 |    172 | dma_in(dmem_ptr, rdram_ptr, 160);
  nop                                                ## L:1238 |   *174 | dma_in(dmem_ptr, rdram_ptr, 160);
  LABEL_GLCmd_PreInitMagma_000B:
  jal GL_MtxMult                                     ## L:1243 |    175 | GL_MtxMult();
  ori $s6, $zero, %lo(GL_MATRIX_MODELVIEW)           ## L:1242 |   *177 | mtx_rhs = GL_MATRIX_MODELVIEW;
  ori $at, $zero, %lo(GL_MATRIX_MODELVIEW)           ## L:1247 |    178 | m0:sint   = load(GL_MATRIX_MODELVIEW, 0x00);
  lqv $v27, 0, 0, $at                                ## L:1247 |    179 | m0:sint   = load(GL_MATRIX_MODELVIEW, 0x00);
  lqv $v25, 0, 16, $at                               ## L:1248 |    180 | m1:sint   = load(GL_MATRIX_MODELVIEW, 0x10);
  lqv $v28, 0, 32, $at                               ## L:1249 |    181 | m0:ufract = load(GL_MATRIX_MODELVIEW, 0x20);
  lqv $v26, 0, 48, $at                               ## L:1250 |    182 | m1:ufract = load(GL_MATRIX_MODELVIEW, 0x30);
  ori $at, $zero, %lo(MATRICES_MV)                   ## L:1252 |    183 | store(m0:sint, MATRICES_MV, 0x00);
  sqv $v27, 0, 0, $at                                ## L:1252 |   *185 | store(m0:sint, MATRICES_MV, 0x00);
  sqv $v25, 0, 16, $at                               ## L:1253 |    186 | store(m1:sint, MATRICES_MV, 0x10);
  sqv $v28, 0, 32, $at                               ## L:1254 |    187 | store(m0:ufract, MATRICES_MV, 0x20);
  sqv $v26, 0, 48, $at                               ## L:1255 |    188 | store(m1:ufract, MATRICES_MV, 0x30);
  vmudl $v28, $v28, $v31.e0                          ## L:1258 |      ^ | m0 = m0 >> 1;
  vmadm $v27, $v27, $v31.e0                          ## L:1258 |    189 | m0 = m0 >> 1;
  vmadn $v28, $v00, $v00                             ## L:1258 |    190 | m0 = m0 >> 1;
  vmudl $v26, $v26, $v31.e0                          ## L:1259 |    191 | m1 = m1 >> 1;
  vmadm $v25, $v25, $v31.e0                          ## L:1259 |    192 | m1 = m1 >> 1;
  vmadn $v26, $v00, $v00                             ## L:1259 |    193 | m1 = m1 >> 1;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:1261 |      ^ | store(m0:sfract, MATRICES_NORMAL, 0x00);
  sqv $v28, 0, 0, $at                                ## L:1261 |    194 | store(m0:sfract, MATRICES_NORMAL, 0x00);
  sqv $v26, 0, 16, $at                               ## L:1262 |  **197 | store(m1:sfract, MATRICES_NORMAL, 0x10);
  LABEL_GLCmd_PreInitMagma_000C:
  ori $at, $zero, %lo(GL_MATRIX_TEXTURE)             ## L:1269 |    198 | tm0:sint   = load(GL_MATRIX_TEXTURE, 0x00);
  lqv $v27, 0, 0, $at                                ## L:1269 |    199 | tm0:sint   = load(GL_MATRIX_TEXTURE, 0x00);
  lqv $v28, 0, 32, $at                               ## L:1270 |    200 | tm0:ufract = load(GL_MATRIX_TEXTURE, 0x20);
  llv $v25, 0, 24, $at                               ## L:1273 |    201 | tm3:sint   = load(GL_MATRIX_TEXTURE, 0x18).xy;
  llv $v26, 0, 56, $at                               ## L:1274 |  **204 | tm3:ufract = load(GL_MATRIX_TEXTURE, 0x38).xy;
  vmudl $v29, $v28, $v28.v                           ## L:1276 |      ^ | vec32 tmp = tm0 * tm0;
  vmadm $v29, $v27, $v28.v                           ## L:1276 |    205 | vec32 tmp = tm0 * tm0;
  vmadn $v24, $v28, $v27.v                           ## L:1276 |    206 | vec32 tmp = tm0 * tm0;
  vmadh $v23, $v27, $v27.v                           ## L:1276 |    207 | vec32 tmp = tm0 * tm0;
  vaddc $v22, $v24, $v24.h1                          ## L:1277 |  **210 | vec32 tmp2 = tmp + tmp.yyyyYYYY;
  vadd $v21, $v23, $v23.h1                           ## L:1277 |    211 | vec32 tmp2 = tmp + tmp.yyyyYYYY;
  vaddc $v24, $v22, $v24.h2                          ## L:1278 |  **214 | tmp = tmp2 + tmp.zzzzZZZZ;
  vadd $v23, $v21, $v23.h2                           ## L:1278 |    215 | tmp = tmp2 + tmp.zzzzZZZZ;
  vrsqh $v21.e0, $v23.e0                             ## L:1279 | ***219 | tmp2.x = invert_half_sqrt(tmp).x;
  vrsql $v22.e0, $v24.e0                             ## L:1279 |    220 | tmp2.x = invert_half_sqrt(tmp).x;
  vrsqh $v21.e0, $v23.e4                             ## L:1280 |    221 | tmp2.X = invert_half_sqrt(tmp).X;
  vrsql $v22.e4, $v24.e4                             ## L:1280 |    222 | tmp2.X = invert_half_sqrt(tmp).X;
  vrsqh $v21.e4, $v00.e0                             ## L:1280 |    223 | tmp2.X = invert_half_sqrt(tmp).X;
  vrcph $v27.e0, $v21.e0                             ## L:1281 | ***227 | tm0.x = invert_half(tmp2).x;
  vrcpl $v28.e0, $v22.e0                             ## L:1281 |    228 | tm0.x = invert_half(tmp2).x;
  vrcph $v27.e0, $v21.e4                             ## L:1282 |    229 | tm0.y = invert_half(tmp2).X;
  vrcpl $v28.e1, $v22.e4                             ## L:1282 |    230 | tm0.y = invert_half(tmp2).X;
  vrcph $v27.e1, $v00.e4                             ## L:1282 |    231 | tm0.y = invert_half(tmp2).X;
  vmudl $v29, $v28, $v30.e2                          ## L:1283 |  **234 | tm0 = tm0 << 5;
  vmadn $v27, $v27, $v30.e2                          ## L:1283 |    235 | tm0 = tm0 << 5;
  vmudn $v28, $v28, $v30.e2                          ## L:1283 |    236 | tm0 = tm0 << 5;
  ori $at, $zero, %lo(GL_STATE_TEX_SIZE)             ## L:1285 |      ^ | vec16 tex_size = load(GL_STATE_TEX_SIZE, 0).xy;
  llv $v20, 0, 0, $at                                ## L:1285 |    237 | vec16 tex_size = load(GL_STATE_TEX_SIZE, 0).xy;
  vmudn $v26, $v26, $v20.v                           ## L:1287 | ***241 | tm3 = tm3 * tex_size;
  vmadh $v25, $v25, $v20.v                           ## L:1287 |    242 | tm3 = tm3 * tex_size;
  vmudl $v29, $v26, $v30.e2                          ## L:1288 |  **245 | tm3 = tm3 << 5;
  vmadn $v25, $v25, $v30.e2                          ## L:1288 |    246 | tm3 = tm3 << 5;
  vmudn $v26, $v26, $v30.e2                          ## L:1288 |    247 | tm3 = tm3 << 5;
  vmudm $v29, $v20, $v28.v                           ## L:1290 |    248 | tex_size = tex_size * tm0;
  vmadh $v20, $v20, $v27.v                           ## L:1290 |    249 | tex_size = tex_size * tm0;
  llv $v19, 0, 4, $at                                ## L:1292 |      ^ | vec16 tex_offset = load(GL_STATE_TEX_SIZE, 4).xy;
  vaddc $v19, $v19, $v25.v                           ## L:1293 | ***253 | tex_offset = tex_offset + tm3;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:1295 |      ^ | store(tex_size.xy, TEXTURING_SCALE, 0);
  slv $v20, 0, 0, $at                                ## L:1295 |    254 | store(tex_size.xy, TEXTURING_SCALE, 0);
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:1296 |    255 | store(tex_offset.xy, TEXTURING_OFFSET, 0);
  or $s7, $zero, $zero                               ## L:1301 |    256 | u32 color_constant = 0;
  bne $a1, $zero, LABEL_GLCmd_PreInitMagma_000F      ## L:1302 |    257 | if (client_flags == 0) {
  slv $v19, 0, 0, $at                                ## L:1296 |   *259 | store(tex_offset.xy, TEXTURING_OFFSET, 0);
  addiu $s7, $zero, 1                                ## L:1303 |    260 | color_constant = 1;
  LABEL_GLCmd_PreInitMagma_000F:
  or $s5, $zero, $zero                               ## L:1308 |    261 | u8 point_count = 0;
  andi $s4, $k0, 1024                                ## L:1310 |    262 | u32 enabled = flags & 0x400;
  beq $s4, $zero, LABEL_GLCmd_PreInitMagma_0010      ## L:1311 |    263 | if (enabled != 0)
  or $s6, $zero, $zero                               ## L:1307 |   *265 | u8 light_count = 0;
  bne $k1, $zero, LABEL_GLCmd_PreInitMagma_0012      ## L:1314 |    266 | if (diffuse_tracking_color == 0) {
  nop                                                ## L:1314 |   *268 | if (diffuse_tracking_color == 0) {
  ori $at, $zero, %lo(GL_MAT_DIFFUSE)                ## L:1316 |    269 | color = load(GL_MAT_DIFFUSE).xyzw;
  ldv $v27, 0, 0, $at                                ## L:1316 |    270 | color = load(GL_MAT_DIFFUSE).xyzw;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0013    ## L:1316 |    271 | color = load(GL_MAT_DIFFUSE).xyzw;
  addiu $s7, $zero, 1                                ## L:1315 |   *273 | color_constant = 1;
  LABEL_GLCmd_PreInitMagma_0012:
  ori $at, $zero, %lo(GL_CUR_COLOR)                  ## L:1318 |    274 | color = load(GL_CUR_COLOR).xyzw;
  ldv $v27, 0, 0, $at                                ## L:1318 |    275 | color = load(GL_CUR_COLOR).xyzw;
  LABEL_GLCmd_PreInitMagma_0013:
  ori $at, $zero, %lo(GL_STATE_LIGHT_AMBIENT)        ## L:1321 |    276 | ambient.xyzw = load(GL_STATE_LIGHT_AMBIENT, 0);
  beq $s7, $zero, LABEL_GLCmd_PreInitMagma_0014      ## L:1322 |    277 | if (color_constant != 0) {
  lqv $v28, 0, 0, $at                                ## L:1321 |   *279 | ambient.xyzw = load(GL_STATE_LIGHT_AMBIENT, 0);
  vmulf $v28, $v28, $v27.v                           ## L:1323 | ***283 | ambient = ambient:sfract * color:sfract;
  LABEL_GLCmd_PreInitMagma_0014:
  lui $s3, 0x10                                      ## L:1326 |      ^ | u32 light_flag = 0x100000;
  ori $s2, $zero, %lo(GL_LIGHTS)                     ## L:1327 |    284 | u16 light_ptr = GL_LIGHTS;
  ori $s1, $zero, %lo(GL_TEX_GEN)                    ## L:1328 |    285 | u16 light_end = GL_TEX_GEN;
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:1329 |    286 | u16 out_ptr = LIGHTING_LIGHTS;
  LABEL_GLCmd_PreInitMagma_0015:
  sltu $at, $s2, $s1                                 ## L:1330 |    287 | while (light_ptr < light_end)
  beq $at, $zero, LABEL_GLCmd_PreInitMagma_0016      ## L:1330 |    288 | while (light_ptr < light_end)
  nop                                                ## L:1330 |   *290 | while (light_ptr < light_end)
  and $fp, $k0, $s3                                  ## L:1332 |    291 | u32 light_enabled = flags & light_flag;
  beq $fp, $zero, LABEL_GLCmd_PreInitMagma_0017      ## L:1333 |    292 | if (light_enabled != 0) {
  nop                                                ## L:1333 |   *294 | if (light_enabled != 0) {
  ldv $v25, 0, 8, $s2                                ## L:1335 |    295 | vec16 light_color = load(light_ptr, 8).xyzw;
  beq $s7, $zero, LABEL_GLCmd_PreInitMagma_0018      ## L:1337 |    296 | if (color_constant != 0) {
  ldv $v26, 0, 0, $s2                                ## L:1334 |   *298 | vec16 light_position = load(light_ptr, 0).xyzw;
  vmulf $v25, $v25, $v27.v                           ## L:1338 |    299 | light_color = light_color:sfract * color:sfract;
  lsv $v25, 6, 6, $s2                                ## L:1339 |    300 | light_color.w = load(light_ptr, 6).x;
  LABEL_GLCmd_PreInitMagma_0018:
  sdv $v25, 0, 8, $s0                                ## L:1343 | ***304 | store(light_color.xyzw, out_ptr, 8);
  lh $sp, 6($s2)                                     ## L:1345 |    305 | s16 w = load(light_ptr, 6);
  beq $sp, $zero, LABEL_GLCmd_PreInitMagma_0019      ## L:1346 |  **308 | if (w != 0) {
  sdv $v26, 0, 0, $s0                                ## L:1342 |   *310 | store(light_position.xyzw, out_ptr, 0);
  addiu $s5, $s5, 1                                  ## L:1347 |    311 | point_count += 1;
  LABEL_GLCmd_PreInitMagma_0019:
  addiu $s6, $s6, 1                                  ## L:1350 |    312 | light_count += 1;
  addiu $s0, $s0, 16                                 ## L:1351 |    313 | out_ptr += 16;
  LABEL_GLCmd_PreInitMagma_0017:
  sll $s3, $s3, 1                                    ## L:1354 |    314 | light_flag = light_flag << 1;
  j LABEL_GLCmd_PreInitMagma_0015                    ## L:1354 |    315 | light_flag = light_flag << 1;
  addiu $s2, $s2, 16                                 ## L:1353 |   *317 | light_ptr += 16;
  LABEL_GLCmd_PreInitMagma_0016:
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0011    ## L:1354 |    318 | light_flag = light_flag << 1;
  nop                                                ## L:1354 |   *320 | light_flag = light_flag << 1;
  LABEL_GLCmd_PreInitMagma_0010:
  bne $s7, $zero, LABEL_GLCmd_PreInitMagma_001A      ## L:1356 |    321 | } else if (color_constant == 0) {
  nop                                                ## L:1356 |   *323 | } else if (color_constant == 0) {
  addiu $at, $zero, 32767                            ## L:1357 |    324 | ambient.x = 0x7FFF;
  mtc2 $at, $v28.e0                                  ## L:1357 |    325 | ambient.x = 0x7FFF;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0011    ## L:1358 |    326 | ambient = ambient.x;
  vor $v28, $v00, $v28.e0                            ## L:1358 | ***330 | ambient = ambient.x;
  LABEL_GLCmd_PreInitMagma_001A:
  ori $at, $zero, %lo(GL_CUR_COLOR)                  ## L:1360 |    331 | ambient = load(GL_CUR_COLOR).xyzw;
  ldv $v28, 0, 0, $at                                ## L:1360 |    332 | ambient = load(GL_CUR_COLOR).xyzw;
  LABEL_GLCmd_PreInitMagma_0011:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:1363 |    333 | @Barrier("store_lighting") store(ambient.xyzw, LIGHTING_AMBIENT, 0); ## Barrier: 0x1
  sdv $v28, 0, 0, $at                                ## L:1363 |  **336 | @Barrier("store_lighting") store(ambient.xyzw, LIGHTING_AMBIENT, 0); ## Barrier: 0x1
  sb $s5, %lo(0 + LIGHTING_HAS_POINTS)($zero)        ## L:1364 |    337 | @Barrier("store_lighting") store(point_count, LIGHTING_HAS_POINTS, 0); ## Barrier: 0x1
  sb $s6, %lo(0 + LIGHTING_COUNT)($zero)             ## L:1365 |    338 | @Barrier("store_lighting") store(light_count, LIGHTING_COUNT, 0); ## Barrier: 0x1
  ori $at, $zero, %lo(GL_STATE_FOG_PARAMS)           ## L:1370 |    339 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  llv $v27, 0, 0, $at                                ## L:1370 |    340 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  llv $v28, 0, 4, $at                                ## L:1370 |    341 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  vsubc $v26, $v28, $v28.e0                          ## L:1371 | ***345 | vec32 diff = fog_parms - fog_parms.x;
  vsub $v25, $v27, $v27.e0                           ## L:1371 |    346 | vec32 diff = fog_parms - fog_parms.x;
  vrcph $v23.e0, $v25.e1                             ## L:1373 | ***350 | factor.x = invert_half(diff).y;
  vrcpl $v24.e0, $v26.e1                             ## L:1373 |    351 | factor.x = invert_half(diff).y;
  vrcph $v23.e0, $v00.e1                             ## L:1373 |    352 | factor.x = invert_half(diff).y;
  vmudl $v22, $v24, $v31.e4                          ## L:1374 |  **355 | vec16 factor_f = factor << 11;
  vmadn $v22, $v23, $v31.e4                          ## L:1374 |    356 | vec16 factor_f = factor << 11;
  vsubc $v21, $v00, $v28.e0                          ## L:1375 |    357 | vec32 offset = VZERO - fog_parms.x;
  vsub $v20, $v00, $v27.e0                           ## L:1375 |    358 | vec32 offset = VZERO - fog_parms.x;
  vmudl $v29, $v21, $v30.e2                          ## L:1376 |  **361 | offset = offset << 5;
  vmadn $v20, $v20, $v30.e2                          ## L:1376 |    362 | offset = offset << 5;
  andi $s7, $k0, 8                                   ## L:1378 |      ^ | u32 enabled = flags & 0x8;
  addiu $s6, $zero, 119                              ## L:1379 |    363 | u8 mask = 0x77;
  bne $s7, $zero, LABEL_GLCmd_PreInitMagma_001C      ## L:1380 |    364 | if (enabled == 0) {
  vmudn $v21, $v21, $v30.e2                          ## L:1376 |   *366 | offset = offset << 5;
  addiu $s6, $zero, 255                              ## L:1381 |    367 | mask = 0xFF;
  LABEL_GLCmd_PreInitMagma_001C:
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:1384 |    368 | store(offset:sint.x, FOG_PARAMS, 0x0);
  ssv $v21, 0, 2, $at                                ## L:1385 |   *370 | store(offset:ufract.x, FOG_PARAMS, 0x2);
  ssv $v22, 0, 4, $at                                ## L:1386 |    371 | store(factor_f.x, FOG_PARAMS, 0x4);
  sb $s6, %lo(0 + FOG_MASK)($zero)                   ## L:1387 |    372 | store(mask, FOG_MASK, 0);
  jal DmaMagmaState                                  ## L:1390 |    373 | DmaMagmaState();
  ssv $v20, 0, 0, $at                                ## L:1384 |   *375 | store(offset:sint.x, FOG_PARAMS, 0x0);
  j RSPQ_Loop                                        ## L:1391 |    376 | }
  nop                                                ## L:1391 |   *378 | }

GLCmd_PreInitMagma_end:

#define zero $0
#define v0 $2
#define v1 $3
#define a0 $4
#define a1 $5
#define a2 $6
#define a3 $7
#define t0 $8
#define t1 $9
#define t2 $10
#define t3 $11
#define t4 $12
#define t5 $13
#define t6 $14
#define t7 $15
#define s0 $16
#define s1 $17
#define s2 $18
#define s3 $19
#define s4 $20
#define s5 $21
#define s6 $22
#define s7 $23
#define t8 $24
#define t9 $25
#define k0 $26
#define k1 $27
#define gp $28
#define sp $29
#define fp $30
#define ra $31

.set at
.set macro

DmaMagmaState:
    li t0, MG_STATE_STAGING_END - MG_STATE_STAGING
    addi t0, 0x7
    andi t0, 0xFFF8
    addi t0, -1
    li s4, %lo(MG_STATE_STAGING)
    j DMAOut
    move s0, a0

#include "rsp_gl_common.inc"
#include <rsp_rdpq.inc>
