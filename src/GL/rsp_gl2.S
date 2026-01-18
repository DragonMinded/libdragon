#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"
#include <rsp_magma_state.inc>

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
    LIGHTING_LIGHTS: .ds.b 208
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

    # Skip processing texture state if we are using rdpq API for texturing
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
  lw $k0, %lo(GL_STATE_FLAGS + 0)                    ## L:1095 |      ^ | u32<$k0> flags = load(GL_STATE_FLAGS, 0);
  or $k1, $zero, $zero                               ## L:1097 |      2 | u32<$k1> diffuse_tracking_color = 0;
  andi $s7, $k0, 2048                                ## L:1099 |     *4 | u32 tmp = flags & 0x800;
  beq $s7, $zero, LABEL_GLCmd_PreInitMagma_0001      ## L:1100 |      5 | if (tmp != 0) {
  nop                                                ## L:1100 |     *7 | if (tmp != 0) {
  lhu $s6, %lo(GL_MAT_COLOR_TARGET + 0)              ## L:1101 |      8 | u16 color_material = load(GL_MAT_COLOR_TARGET);
  addiu $at, $zero, 4609                             ## L:1102 |      9 | if (color_material == 0x1201) {
  bne $s6, $at, LABEL_GLCmd_PreInitMagma_0002        ## L:1102 |    *11 | if (color_material == 0x1201) {
  nop                                                ## L:1102 |    *13 | if (color_material == 0x1201) {
  addiu $k1, $zero, 1                                ## L:1103 |     14 | diffuse_tracking_color = 1;
  LABEL_GLCmd_PreInitMagma_0002:
  addiu $at, $zero, 5634                             ## L:1105 |     15 | if (color_material == 0x1602) {
  bne $s6, $at, LABEL_GLCmd_PreInitMagma_0001        ## L:1105 |     16 | if (color_material == 0x1602) {
  nop                                                ## L:1105 |    *18 | if (color_material == 0x1602) {
  addiu $k1, $zero, 1                                ## L:1106 |     19 | diffuse_tracking_color = 1;
  LABEL_GLCmd_PreInitMagma_0001:
  lui $s6, 0x3000                                    ## L:1115 |     20 | u32 env_mask = 0x30000000;
  and $s5, $k0, $s6                                  ## L:1116 |     21 | u32 tmp = flags & env_mask;
  bne $s5, $s6, LABEL_GLCmd_PreInitMagma_0004        ## L:1117 |     22 | if (tmp == env_mask)
  or $s7, $zero, $zero                               ## L:1113 |    *24 | u32 features = 0;
  lhu $s3, %lo(GL_TEX_GEN + 128)                     ## L:1120 |     25 | u16 mode_s = load(GL_TEX_GEN, 128);
  addiu $s4, $zero, 9218                             ## L:1119 |     26 | u16 sphere_map = 0x2402;
  lhu $s2, %lo(GL_TEX_GEN + 130)                     ## L:1121 |     27 | u16 mode_t = load(GL_TEX_GEN, 130);
  bne $s3, $s4, LABEL_GLCmd_PreInitMagma_0004        ## L:1122 |     28 | if (mode_s == sphere_map)
  nop                                                ## L:1122 |    *30 | if (mode_s == sphere_map)
  bne $s2, $s4, LABEL_GLCmd_PreInitMagma_0004        ## L:1124 |     31 | if (mode_t == sphere_map)
  nop                                                ## L:1124 |    *33 | if (mode_t == sphere_map)
  ori $s7, $s7, 1                                    ## L:1126 |     34 | features |= 1;
  LABEL_GLCmd_PreInitMagma_0004:
  andi $s5, $k0, 1024                                ## L:1131 |     35 | tmp = flags & 0x400;
  beq $s5, $zero, LABEL_GLCmd_PreInitMagma_0007      ## L:1132 |     36 | if (tmp != 0)
  nop                                                ## L:1132 |    *38 | if (tmp != 0)
  bne $k1, $zero, LABEL_GLCmd_PreInitMagma_0007      ## L:1134 |     39 | if (diffuse_tracking_color == 0) {
  nop                                                ## L:1134 |    *41 | if (diffuse_tracking_color == 0) {
  ori $s7, $s7, 2                                    ## L:1135 |     42 | features |= 2;
  LABEL_GLCmd_PreInitMagma_0007:
  sll $s7, $s7, 3                                    ## L:1139 |     43 | features = features << 3;
  addiu $at, $s7, GL_PIPELINES                       ## L:1141 |     44 | vec16 pipeline = load(features, GL_PIPELINES).xyzw;
  ldv $v28, 0, 0, $at                                ## L:1141 |     45 | vec16 pipeline = load(features, GL_PIPELINES).xyzw;
  lhu $s6, %lo(GL_MATRIX_PROJECTION + 54)            ## L:1155 |     46 | u16 m23f = load(GL_MATRIX_PROJECTION, 0x36);
  ori $at, $zero, %lo(MG_SHADER_TEXT)                ## L:1142 |     47 | store(pipeline.xyzw, MG_SHADER_TEXT);
  lh $s7, %lo(GL_MATRIX_PROJECTION + 22)             ## L:1154 |     48 | s16 m23i = load(GL_MATRIX_PROJECTION, 0x16);
  sdv $v28, 0, 0, $at                                ## L:1142 |     49 | store(pipeline.xyzw, MG_SHADER_TEXT);
  ori $at, $zero, %lo(GL_VIEWPORT_SCALE)             ## L:1147 |     50 | vec16 viewport = load(GL_VIEWPORT_SCALE, 0);
  lqv $v28, 0, 0, $at                                ## L:1147 |     51 | vec16 viewport = load(GL_VIEWPORT_SCALE, 0);
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:1148 |     52 | store(viewport, MG_VIEWPORT, 0);
  or $s5, $s7, $s6                                   ## L:1156 |     53 | u16 m23 = m23i | m23f;
  bne $s5, $zero, LABEL_GLCmd_PreInitMagma_0009      ## L:1157 |     54 | if (m23 == 0)
  sqv $v28, 0, 0, $at                                ## L:1148 |    *56 | store(viewport, MG_VIEWPORT, 0);
  ori $s4, $zero, 0xFFFF                             ## L:1161 |     57 | u16 norm_factor = 0xFFFF;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_000A    ## L:1162 |     58 | store(norm_factor, MG_VIEWPORT, 0x6);
  sh $s4, %lo(6 + MG_VIEWPORT)($zero)                ## L:1162 |    *60 | store(norm_factor, MG_VIEWPORT, 0x6);
  LABEL_GLCmd_PreInitMagma_0009:
  ori $at, $zero, %lo(GL_MATRIX_PROJECTION)          ## L:1184 |     61 | m:sint = load(GL_MATRIX_PROJECTION, 0x10);
  lqv $v28, 0, 48, $at                               ## L:1185 |     62 | m:ufract = load(GL_MATRIX_PROJECTION, 0x30);
  lqv $v27, 0, 16, $at                               ## L:1184 |     63 | m:sint = load(GL_MATRIX_PROJECTION, 0x10);
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:1197 |   **66 | store(a:sfract.x, MG_VIEWPORT, 0x6);
  vmudl $v29, $v28, $v28.e2                          ## L:1188 |      ^ | vec32 a = m * m.z;
  vmadm $v29, $v27, $v28.e2                          ## L:1188 |     67 | vec32 a = m * m.z;
  vmadn $v26, $v28, $v27.e2                          ## L:1188 |     68 | vec32 a = m * m.z;
  vmadh $v25, $v27, $v27.e2                          ## L:1188 |     69 | vec32 a = m * m.z;
  vsubc $v24, $v26, $v00.e7                          ## L:1190 |   **72 | vec32 b = a - 1;
  vsub $v23, $v25, $v30.e7                           ## L:1190 |     73 | vec32 b = a - 1;
  vrcph $v25.e0, $v25.e6                             ## L:1192 |     74 | a.x = invert_half(a).Z;
  vrcpl $v26.e0, $v26.e6                             ## L:1192 |     75 | a.x = invert_half(a).Z;
  vrcph $v25.e0, $v00.e6                             ## L:1192 |     76 | a.x = invert_half(a).Z;
  vmudl $v29, $v26, $v24.e2                          ## L:1194 |   **79 | a *= b.z;
  vmadm $v29, $v25, $v24.e2                          ## L:1194 |     80 | a *= b.z;
  vmadn $v26, $v26, $v23.e2                          ## L:1194 |     81 | a *= b.z;
  vmadh $v25, $v25, $v23.e2                          ## L:1194 |     82 | a *= b.z;
  vmudn $v26, $v26, $v30.e6                          ## L:1195 |   **85 | a *= 2;
  ssv $v26, 0, 6, $at                                ## L:1197 |  ***89 | store(a:sfract.x, MG_VIEWPORT, 0x6);
  vmadh $v25, $v25, $v30.e6                          ## L:1195 |      ^ | a *= 2;
  LABEL_GLCmd_PreInitMagma_000A:
  lhu $s7, %lo(GL_TRI_CMD + 0)                       ## L:1203 |     90 | u16 tri_cmd = load(GL_TRI_CMD, 0);
  sh $s7, %lo(0 + MG_TRI_CMD)($zero)                 ## L:1204 |   **93 | store(tri_cmd, MG_TRI_CMD, 0);
  lbu $s7, %lo(GL_TRI_CULL + 0)                      ## L:1209 |     94 | u8 cull = load(GL_TRI_CULL, 0);
  ori $s5, $zero, %lo(GL_MATRIX_PROJECTION)          ## L:1221 |     95 | mtx_lhs = GL_MATRIX_PROJECTION;
  andi $s3, $k0, 16384                               ## L:1224 |     96 | u32 palette_enabled = flags & 0x4000;
  sb $s7, %lo(0 + MG_CULL_MODE)($zero)               ## L:1210 |     97 | store(cull, MG_CULL_MODE, 0);
  beq $s3, $zero, LABEL_GLCmd_PreInitMagma_000B      ## L:1225 |     98 | if (palette_enabled != 0)
  ori $s7, $zero, %lo(MATRICES_MVP)                  ## L:1222 |   *100 | mtx_dst = MATRICES_MVP;
  addiu $t2, $zero, -32768                           ## L:1231 |    101 | dma_out_async(dmem_ptr, rdram_ptr, 64);
  lw $s2, %lo(GL_STATE_PALETTE_INDEX + 0)            ## L:1227 |    102 | u32 palette_index = load(GL_STATE_PALETTE_INDEX);
  ori $s4, $zero, %lo(GL_MATRIX_PALETTE)             ## L:1230 |    103 | dmem_ptr = GL_MATRIX_PALETTE;
  lw $s0, %lo(GL_PALETTE_PTR + 0)                    ## L:1228 |    104 | rdram_ptr = load(GL_PALETTE_PTR);
  ori $t0, $zero, 63                                 ## L:1231 |    105 | dma_out_async(dmem_ptr, rdram_ptr, 64);
  jal DMAExec                                        ## L:1231 |    106 | dma_out_async(dmem_ptr, rdram_ptr, 64); ## Args: $t0, $t1, $s0, $s4, $t2
  addu $s0, $s0, $s2                                 ## L:1229 |   *108 | rdram_ptr += palette_index;
  ori $s4, $zero, %lo(MATRICES_MVP)                  ## L:1235 |    109 | dmem_ptr = MATRICES_MVP;
  ori $s6, $zero, %lo(MATRICES_MV)                   ## L:1233 |    110 | mtx_rhs = MATRICES_MV;
  lw $s0, %lo(GL_PALETTE_PTR + 0)                    ## L:1236 |    111 | rdram_ptr = load(GL_PALETTE_PTR);
  or $s1, $zero, $zero                               ## L:1237 |    112 | u16 i = 0;
  LABEL_GLCmd_PreInitMagma_000D:
  sltiu $at, $s1, 32                                 ## L:1238 |    113 | while (i < 32) {
  beq $at, $zero, LABEL_GLCmd_PreInitMagma_000E      ## L:1238 |    114 | while (i < 32) {
  nop                                                ## L:1238 |   *116 | while (i < 32) {
  ori $t0, $zero, 159                                ## L:1239 |    117 | dma_in(dmem_ptr, rdram_ptr, 160);
  jal DMAExec                                        ## L:1239 |    118 | dma_in(dmem_ptr, rdram_ptr, 160); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t2, $zero, 12                               ## L:1239 |   *120 | dma_in(dmem_ptr, rdram_ptr, 160);
  ori $at, $zero, %lo(MATRICES_MV)                   ## L:1243 |    121 | m0:sint   = load(MATRICES_MV, 0x00);
  lqv $v25, 0, 16, $at                               ## L:1244 |    122 | m1:sint   = load(MATRICES_MV, 0x10);
  lqv $v26, 0, 48, $at                               ## L:1246 |    123 | m1:ufract = load(MATRICES_MV, 0x30);
  lqv $v28, 0, 32, $at                               ## L:1245 |    124 | m0:ufract = load(MATRICES_MV, 0x20);
  lqv $v27, 0, 0, $at                                ## L:1243 |    125 | m0:sint   = load(MATRICES_MV, 0x00);
  addiu $at, $zero, 32767                            ## L:1085 |    126 | c.x = 0x7FFF;
  mtc2 $at, $v24.e0                                  ## L:1085 |   *128 | c.x = 0x7FFF;
  vmudn $v28, $v28, $v24.e0                          ## L:1086 | ***132 | m0 = m0 * c.x;
  vmadh $v27, $v27, $v24.e0                          ## L:1086 |    133 | m0 = m0 * c.x;
  vmudn $v26, $v26, $v24.e0                          ## L:1087 |    134 | m1 = m1 * c.x;
  vmadh $v25, $v25, $v24.e0                          ## L:1087 |    135 | m1 = m1 * c.x;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:1089 |      ^ | store(m0:sint, MATRICES_NORMAL, 0x00);
  sqv $v27, 0, 0, $at                                ## L:1089 |   *137 | store(m0:sint, MATRICES_NORMAL, 0x00);
  jal GL_MtxMult                                     ## L:1251 |    138 | GL_MtxMult();
  sqv $v25, 0, 16, $at                               ## L:1090 |   *140 | store(m1:sint, MATRICES_NORMAL, 0x10);
  addiu $t2, $zero, -32768                           ## L:1252 |    141 | dma_out_async(dmem_ptr, rdram_ptr, 160);
  jal DMAExec                                        ## L:1252 |    142 | dma_out_async(dmem_ptr, rdram_ptr, 160); ## Args: $t0, $t1, $s0, $s4, $t2
  ori $t0, $zero, 159                                ## L:1252 |   *144 | dma_out_async(dmem_ptr, rdram_ptr, 160);
  addiu $s1, $s1, 1                                  ## L:1254 |    145 | i += 1;
  j LABEL_GLCmd_PreInitMagma_000D                    ## L:1254 |    146 | i += 1;
  addiu $s0, $s0, 160                                ## L:1253 |   *148 | rdram_ptr += 160;
  LABEL_GLCmd_PreInitMagma_000E:
  addiu $t2, $zero, 12                               ## L:1264 |    149 | dma_in(dmem_ptr, rdram_ptr, 160);
  lbu $fp, %lo(GL_CUR_MTX_INDEX + 0)                 ## L:1257 |    150 | u8 cur_mtx_index = load(GL_CUR_MTX_INDEX);
  lw $s0, %lo(GL_PALETTE_PTR + 0)                    ## L:1262 |    151 | rdram_ptr = load(GL_PALETTE_PTR);
  ori $t0, $zero, 159                                ## L:1264 |    152 | dma_in(dmem_ptr, rdram_ptr, 160);
  sll $sp, $fp, 7                                    ## L:1258 |    153 | u32 tmp = cur_mtx_index << 7;
  sll $s2, $fp, 5                                    ## L:1259 |    154 | palette_index = cur_mtx_index << 5;
  addu $s2, $s2, $sp                                 ## L:1260 |    155 | palette_index += tmp;
  jal DMAExec                                        ## L:1264 |    156 | dma_in(dmem_ptr, rdram_ptr, 160); ## Args: $t0, $t1, $s0, $s4, $t2
  addu $s0, $s0, $s2                                 ## L:1263 |   *158 | rdram_ptr += palette_index;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_000C    ## L:1264 |    159 | dma_in(dmem_ptr, rdram_ptr, 160);
  nop                                                ## L:1264 |   *161 | dma_in(dmem_ptr, rdram_ptr, 160);
  LABEL_GLCmd_PreInitMagma_000B:
  jal GL_MtxMult                                     ## L:1269 |    162 | GL_MtxMult();
  ori $s6, $zero, %lo(GL_MATRIX_MODELVIEW)           ## L:1268 |   *164 | mtx_rhs = GL_MATRIX_MODELVIEW;
  ori $at, $zero, %lo(GL_MATRIX_MODELVIEW)           ## L:1273 |    165 | m0:sint   = load(GL_MATRIX_MODELVIEW, 0x00);
  lqv $v27, 0, 0, $at                                ## L:1273 |    166 | m0:sint   = load(GL_MATRIX_MODELVIEW, 0x00);
  lqv $v28, 0, 32, $at                               ## L:1275 |    167 | m0:ufract = load(GL_MATRIX_MODELVIEW, 0x20);
  lqv $v25, 0, 16, $at                               ## L:1274 |    168 | m1:sint   = load(GL_MATRIX_MODELVIEW, 0x10);
  lqv $v26, 0, 48, $at                               ## L:1276 |    169 | m1:ufract = load(GL_MATRIX_MODELVIEW, 0x30);
  ori $at, $zero, %lo(MATRICES_MV)                   ## L:1278 |    170 | store(m0:sint, MATRICES_MV, 0x00);
  sqv $v27, 0, 0, $at                                ## L:1278 |   *172 | store(m0:sint, MATRICES_MV, 0x00);
  sqv $v26, 0, 48, $at                               ## L:1281 |    173 | store(m1:ufract, MATRICES_MV, 0x30);
  sqv $v25, 0, 16, $at                               ## L:1279 |    174 | store(m1:sint, MATRICES_MV, 0x10);
  sqv $v28, 0, 32, $at                               ## L:1280 |    175 | store(m0:ufract, MATRICES_MV, 0x20);
  addiu $at, $zero, 32767                            ## L:1085 |    176 | c.x = 0x7FFF;
  mtc2 $at, $v24.e0                                  ## L:1085 |    177 | c.x = 0x7FFF;
  vmudn $v28, $v28, $v24.e0                          ## L:1086 | ***181 | m0 = m0 * c.x;
  vmadh $v27, $v27, $v24.e0                          ## L:1086 |    182 | m0 = m0 * c.x;
  vmudn $v26, $v26, $v24.e0                          ## L:1087 |    183 | m1 = m1 * c.x;
  vmadh $v25, $v25, $v24.e0                          ## L:1087 |    184 | m1 = m1 * c.x;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:1089 |      ^ | store(m0:sint, MATRICES_NORMAL, 0x00);
  sqv $v27, 0, 0, $at                                ## L:1089 |   *186 | store(m0:sint, MATRICES_NORMAL, 0x00);
  sqv $v25, 0, 16, $at                               ## L:1090 |   *188 | store(m1:sint, MATRICES_NORMAL, 0x10);
  LABEL_GLCmd_PreInitMagma_000C:
  ori $at, $zero, %lo(GL_MATRIX_TEXTURE)             ## L:1290 |    189 | tm0:sint   = load(GL_MATRIX_TEXTURE, 0x00);
  lqv $v28, 0, 32, $at                               ## L:1291 |    190 | tm0:ufract = load(GL_MATRIX_TEXTURE, 0x20);
  lqv $v27, 0, 0, $at                                ## L:1290 |    191 | tm0:sint   = load(GL_MATRIX_TEXTURE, 0x00);
  llv $v26, 0, 56, $at                               ## L:1295 |  **194 | tm3:ufract = load(GL_MATRIX_TEXTURE, 0x38).xy;
  vmudl $v29, $v28, $v28.v                           ## L:1297 |      ^ | vec32 tmp = tm0 * tm0;
  vmadm $v29, $v27, $v28.v                           ## L:1297 |    195 | vec32 tmp = tm0 * tm0;
  llv $v25, 0, 24, $at                               ## L:1294 |      ^ | tm3:sint   = load(GL_MATRIX_TEXTURE, 0x18).xy;
  vmadn $v24, $v28, $v27.v                           ## L:1297 |    196 | vec32 tmp = tm0 * tm0;
  ori $at, $zero, %lo(GL_STATE_TEX_SIZE)             ## L:1306 |      ^ | vec16 tex_size = load(GL_STATE_TEX_SIZE, 0).xy;
  vmadh $v23, $v27, $v27.v                           ## L:1297 |    197 | vec32 tmp = tm0 * tm0;
  llv $v19, 0, 4, $at                                ## L:1313 |      ^ | vec16 tex_offset = load(GL_STATE_TEX_SIZE, 4).xy;
  vaddc $v22, $v24, $v24.h1                          ## L:1298 |  **200 | vec32 tmp2 = tmp + tmp.yyyyYYYY;
  vadd $v21, $v23, $v23.h1                           ## L:1298 |    201 | vec32 tmp2 = tmp + tmp.yyyyYYYY;
  vaddc $v24, $v22, $v24.h2                          ## L:1299 |  **204 | tmp = tmp2 + tmp.zzzzZZZZ;
  vadd $v23, $v21, $v23.h2                           ## L:1299 |    205 | tmp = tmp2 + tmp.zzzzZZZZ;
  vrsqh $v21.e0, $v23.e0                             ## L:1300 | ***209 | tmp2.x = invert_half_sqrt(tmp).x;
  vrsql $v22.e0, $v24.e0                             ## L:1300 |    210 | tmp2.x = invert_half_sqrt(tmp).x;
  llv $v20, 0, 0, $at                                ## L:1306 |      ^ | vec16 tex_size = load(GL_STATE_TEX_SIZE, 0).xy;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:1316 |    211 | store(tex_size.xy, TEXTURING_SCALE, 0);
  vrsqh $v21.e0, $v23.e4                             ## L:1301 |      ^ | tmp2.X = invert_half_sqrt(tmp).X;
  vrsql $v22.e4, $v24.e4                             ## L:1301 |    212 | tmp2.X = invert_half_sqrt(tmp).X;
  vrsqh $v21.e4, $v00.e0                             ## L:1301 |    213 | tmp2.X = invert_half_sqrt(tmp).X;
  vrcph $v27.e0, $v21.e0                             ## L:1302 | ***217 | tm0.x = invert_half(tmp2).x;
  vrcpl $v28.e0, $v22.e0                             ## L:1302 |    218 | tm0.x = invert_half(tmp2).x;
  vrcph $v27.e0, $v21.e4                             ## L:1303 |    219 | tm0.y = invert_half(tmp2).X;
  vrcpl $v28.e1, $v22.e4                             ## L:1303 |    220 | tm0.y = invert_half(tmp2).X;
  vrcph $v27.e1, $v00.e4                             ## L:1303 |    221 | tm0.y = invert_half(tmp2).X;
  vmudl $v29, $v28, $v30.e2                          ## L:1304 |  **224 | tm0 = tm0 << 5;
  vmadn $v27, $v27, $v30.e2                          ## L:1304 |    225 | tm0 = tm0 << 5;
  vmudn $v28, $v28, $v30.e2                          ## L:1304 |    226 | tm0 = tm0 << 5;
  vmudn $v26, $v26, $v20.v                           ## L:1308 |    227 | tm3 = tm3 * tex_size;
  vmadh $v25, $v25, $v20.v                           ## L:1308 |    228 | tm3 = tm3 * tex_size;
  vmudl $v29, $v26, $v30.e2                          ## L:1309 |  **231 | tm3 = tm3 << 5;
  vmadn $v25, $v25, $v30.e2                          ## L:1309 |    232 | tm3 = tm3 << 5;
  vmudn $v26, $v26, $v30.e2                          ## L:1309 |    233 | tm3 = tm3 << 5;
  vmudm $v29, $v20, $v28.v                           ## L:1311 |    234 | tex_size = tex_size * tm0;
  vmadh $v20, $v20, $v27.v                           ## L:1311 |    235 | tex_size = tex_size * tm0;
  vaddc $v19, $v19, $v25.v                           ## L:1314 |    236 | tex_offset = tex_offset + tm3;
  or $s7, $zero, $zero                               ## L:1322 |      ^ | u32 color_constant = 0;
  slv $v20, 0, 0, $at                                ## L:1316 |  **239 | store(tex_size.xy, TEXTURING_SCALE, 0);
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:1317 |    240 | store(tex_offset.xy, TEXTURING_OFFSET, 0);
  bne $a1, $zero, LABEL_GLCmd_PreInitMagma_000F      ## L:1323 |    241 | if (client_flags == 0) {
  slv $v19, 0, 0, $at                                ## L:1317 |   *243 | store(tex_offset.xy, TEXTURING_OFFSET, 0);
  addiu $s7, $zero, 1                                ## L:1324 |    244 | color_constant = 1;
  LABEL_GLCmd_PreInitMagma_000F:
  andi $s4, $k0, 1024                                ## L:1331 |    245 | u32 enabled = flags & 0x400;
  or $s5, $zero, $zero                               ## L:1329 |    246 | u8 point_count = 0;
  beq $s4, $zero, LABEL_GLCmd_PreInitMagma_0010      ## L:1332 |    247 | if (enabled != 0)
  or $s6, $zero, $zero                               ## L:1328 |   *249 | u8 light_count = 0;
  bne $k1, $zero, LABEL_GLCmd_PreInitMagma_0012      ## L:1335 |    250 | if (diffuse_tracking_color == 0) {
  nop                                                ## L:1335 |   *252 | if (diffuse_tracking_color == 0) {
  ori $at, $zero, %lo(GL_MAT_DIFFUSE)                ## L:1337 |    253 | color = load(GL_MAT_DIFFUSE).xyzw;
  ldv $v27, 0, 0, $at                                ## L:1337 |    254 | color = load(GL_MAT_DIFFUSE).xyzw;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0013    ## L:1337 |    255 | color = load(GL_MAT_DIFFUSE).xyzw;
  addiu $s7, $zero, 1                                ## L:1336 |   *257 | color_constant = 1;
  LABEL_GLCmd_PreInitMagma_0012:
  ori $at, $zero, %lo(GL_CUR_COLOR)                  ## L:1339 |    258 | color = load(GL_CUR_COLOR).xyzw;
  ldv $v27, 0, 0, $at                                ## L:1339 |    259 | color = load(GL_CUR_COLOR).xyzw;
  LABEL_GLCmd_PreInitMagma_0013:
  ori $at, $zero, %lo(GL_STATE_LIGHT_AMBIENT)        ## L:1342 |    260 | ambient.xyzw = load(GL_STATE_LIGHT_AMBIENT, 0);
  lqv $v28, 0, 0, $at                                ## L:1342 |    261 | ambient.xyzw = load(GL_STATE_LIGHT_AMBIENT, 0);
  beq $s7, $zero, LABEL_GLCmd_PreInitMagma_0014      ## L:1343 |    262 | if (color_constant != 0) {
  nop                                                ## L:1343 |   *264 | if (color_constant != 0) {
  vmulf $v28, $v28, $v27.v                           ## L:1344 |    265 | ambient = ambient:sfract * color:sfract;
  LABEL_GLCmd_PreInitMagma_0014:
  ori $s2, $zero, %lo(GL_LIGHTS)                     ## L:1348 |      ^ | u16 light_ptr = GL_LIGHTS;
  lui $s3, 0x10                                      ## L:1347 |    266 | u32 light_flag = 0x100000;
  ori $s1, $zero, %lo(GL_TEX_GEN)                    ## L:1349 |    267 | u16 light_end = GL_TEX_GEN;
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:1350 |    268 | u16 out_ptr = LIGHTING_LIGHTS;
  LABEL_GLCmd_PreInitMagma_0015:
  sltu $at, $s2, $s1                                 ## L:1351 |    269 | while (light_ptr < light_end)
  beq $at, $zero, LABEL_GLCmd_PreInitMagma_0016      ## L:1351 |    270 | while (light_ptr < light_end)
  nop                                                ## L:1351 |   *272 | while (light_ptr < light_end)
  and $fp, $k0, $s3                                  ## L:1353 |    273 | u32 light_enabled = flags & light_flag;
  beq $fp, $zero, LABEL_GLCmd_PreInitMagma_0017      ## L:1354 |    274 | if (light_enabled != 0) {
  nop                                                ## L:1354 |   *276 | if (light_enabled != 0) {
  ldv $v26, 0, 0, $s2                                ## L:1355 |    277 | vec16 light_position = load(light_ptr, 0).xyzw;
  addiu $sp, $s2, 14                                 ## L:1358 |    278 | u16 att_ptr = light_ptr + 14;
  ldv $v25, 0, 8, $s2                                ## L:1356 |    279 | vec16 light_color = load(light_ptr, 8).xyzw;
  ldv $v24, 0, 0, $sp                                ## L:1360 |    280 | attenuation = load(att_ptr).xyzw;
  beq $s7, $zero, LABEL_GLCmd_PreInitMagma_0018      ## L:1363 |    281 | if (color_constant != 0) {
  llv $v24, 8, 8, $sp                                ## L:1361 |   *283 | attenuation.XY = load(att_ptr).XY;
  vmulf $v25, $v25, $v27.v                           ## L:1364 |    284 | light_color = light_color:sfract * color:sfract;
  LABEL_GLCmd_PreInitMagma_0018:
  lh $t9, 6($s2)                                     ## L:1374 |      ^ | s16 w = load(light_ptr, 6);
  sdv $v26, 0, 0, $s0                                ## L:1367 |    285 | store(light_position.xyzw, out_ptr, 0);
  addiu $sp, $s0, 14                                 ## L:1370 |    286 | att_ptr = out_ptr + 14;
  sdv $v24, 0, 0, $sp                                ## L:1371 |    287 | store(attenuation.xyzw, att_ptr, 0);
  sdv $v25, 0, 8, $s0                                ## L:1368 |    288 | store(light_color.xyzw, out_ptr, 8);
  beq $t9, $zero, LABEL_GLCmd_PreInitMagma_0019      ## L:1375 |    289 | if (w != 0) {
  slv $v24, 8, 8, $sp                                ## L:1372 |   *291 | store(attenuation.XY, att_ptr, 8);
  addiu $s5, $s5, 1                                  ## L:1376 |    292 | point_count += 1;
  LABEL_GLCmd_PreInitMagma_0019:
  addiu $s0, $s0, 26                                 ## L:1380 |    293 | out_ptr += 26;
  addiu $s6, $s6, 1                                  ## L:1379 |    294 | light_count += 1;
  LABEL_GLCmd_PreInitMagma_0017:
  sll $s3, $s3, 1                                    ## L:1383 |    295 | light_flag = light_flag << 1;
  j LABEL_GLCmd_PreInitMagma_0015                    ## L:1383 |    296 | light_flag = light_flag << 1;
  addiu $s2, $s2, 26                                 ## L:1382 |   *298 | light_ptr += 26;
  LABEL_GLCmd_PreInitMagma_0016:
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0011    ## L:1383 |    299 | light_flag = light_flag << 1;
  nop                                                ## L:1383 |   *301 | light_flag = light_flag << 1;
  LABEL_GLCmd_PreInitMagma_0010:
  bne $s7, $zero, LABEL_GLCmd_PreInitMagma_001A      ## L:1385 |    302 | } else if (color_constant == 0) {
  nop                                                ## L:1385 |   *304 | } else if (color_constant == 0) {
  addiu $at, $zero, 32767                            ## L:1386 |    305 | ambient.x = 0x7FFF;
  mtc2 $at, $v28.e0                                  ## L:1386 |    306 | ambient.x = 0x7FFF;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0011    ## L:1387 |    307 | ambient = ambient.x;
  vor $v28, $v00, $v28.e0                            ## L:1387 | ***311 | ambient = ambient.x;
  LABEL_GLCmd_PreInitMagma_001A:
  ori $at, $zero, %lo(GL_CUR_COLOR)                  ## L:1389 |    312 | ambient = load(GL_CUR_COLOR).xyzw;
  ldv $v28, 0, 0, $at                                ## L:1389 |    313 | ambient = load(GL_CUR_COLOR).xyzw;
  LABEL_GLCmd_PreInitMagma_0011:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:1392 |    314 | @Barrier("store_lighting") store(ambient.xyzw, LIGHTING_AMBIENT, 0); ## Barrier: 0x1
  sdv $v28, 0, 0, $at                                ## L:1392 |  **317 | @Barrier("store_lighting") store(ambient.xyzw, LIGHTING_AMBIENT, 0); ## Barrier: 0x1
  ori $at, $zero, %lo(GL_STATE_FOG_PARAMS)           ## L:1399 |    318 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  llv $v28, 0, 4, $at                                ## L:1399 |    319 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  llv $v27, 0, 0, $at                                ## L:1399 |    320 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  sb $s5, %lo(0 + LIGHTING_HAS_POINTS)($zero)        ## L:1393 |  **323 | @Barrier("store_lighting") store(point_count, LIGHTING_HAS_POINTS, 0); ## Barrier: 0x1
  vsubc $v26, $v28, $v28.e0                          ## L:1400 |      ^ | vec32 diff = fog_parms - fog_parms.x;
  vsub $v25, $v27, $v27.e0                           ## L:1400 |    324 | vec32 diff = fog_parms - fog_parms.x;
  sb $s6, %lo(0 + LIGHTING_COUNT)($zero)             ## L:1394 |      ^ | @Barrier("store_lighting") store(light_count, LIGHTING_COUNT, 0); ## Barrier: 0x1
  vrcph $v23.e0, $v25.e1                             ## L:1402 | ***328 | factor.x = invert_half(diff).y;
  addiu $s6, $zero, 119                              ## L:1408 |      ^ | u8 mask = 0x77;
  vrcpl $v24.e0, $v26.e1                             ## L:1402 |    329 | factor.x = invert_half(diff).y;
  andi $s7, $k0, 8                                   ## L:1407 |      ^ | u32 enabled = flags & 0x8;
  vrcph $v23.e0, $v00.e1                             ## L:1402 |    330 | factor.x = invert_half(diff).y;
  vmudl $v22, $v24, $v31.e4                          ## L:1403 |  **333 | vec16 factor_f = factor << 11;
  vmadn $v22, $v23, $v31.e4                          ## L:1403 |    334 | vec16 factor_f = factor << 11;
  vsubc $v21, $v00, $v28.e0                          ## L:1404 |    335 | vec32 offset = VZERO - fog_parms.x;
  vsub $v20, $v00, $v27.e0                           ## L:1404 |    336 | vec32 offset = VZERO - fog_parms.x;
  vmudl $v29, $v21, $v30.e2                          ## L:1405 |  **339 | offset = offset << 5;
  vmadn $v20, $v20, $v30.e2                          ## L:1405 |    340 | offset = offset << 5;
  bne $s7, $zero, LABEL_GLCmd_PreInitMagma_001C      ## L:1409 |      ^ | if (enabled == 0) {
  vmudn $v21, $v21, $v30.e2                          ## L:1405 |   *342 | offset = offset << 5;
  addiu $s6, $zero, 255                              ## L:1410 |    343 | mask = 0xFF;
  LABEL_GLCmd_PreInitMagma_001C:
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:1413 |    344 | store(offset:sint.x, FOG_PARAMS, 0x0);
  ssv $v20, 0, 0, $at                                ## L:1413 |    345 | store(offset:sint.x, FOG_PARAMS, 0x0);
  sb $s6, %lo(0 + FOG_MASK)($zero)                   ## L:1416 |    346 | store(mask, FOG_MASK, 0);
  ssv $v21, 0, 2, $at                                ## L:1414 |    347 | store(offset:ufract.x, FOG_PARAMS, 0x2);
  jal DmaMagmaState                                  ## L:1419 |    348 | DmaMagmaState();
  ssv $v22, 0, 4, $at                                ## L:1415 |   *350 | store(factor_f.x, FOG_PARAMS, 0x4);
  j RSPQ_Loop                                        ## L:1420 |    351 | }
  nop                                                ## L:1420 |   *353 | }

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
