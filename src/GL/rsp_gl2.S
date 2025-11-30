#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"
#include <rsp_magma_state.inc>
#include "mgfx_constants.h"

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_CopyState,     4   # 0x0
        RSPQ_DefineCommand GLCmd_PreInitPipe,   4   # 0x1
        RSPQ_DefineCommand GLCmd_PreInitPipeTex,4   # 0x2
        RSPQ_DefineCommand GLCmd_SetPipeline,   8   # 0x3
        RSPQ_DefineCommand GLCmd_PreInitMagma,  4   # 0x4
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

    .func GLCmd_CopyState
GLCmd_CopyState:
    move s0, a0
    li s4, %lo(GL_STATE)
    j DMAIn
    li t0, DMA_SIZE(GL_STATE_SIZE, 1)
    .endfunc

    .func GLCmd_PreInitPipe
GLCmd_PreInitPipe:
    sh a0, %lo(GL_STATE_PRIM_TYPE)

    #define state_flags  k1
    #define state_flags2 k0

    lw state_flags, %lo(GL_STATE_FLAGS)
    lw state_flags2, %lo(GL_STATE_FLAGS2)

    # Update matrix if required

    andi t0, state_flags, FLAG_MATRIX_PALETTE
    beqz t0, 1f
    li v1, 1

    #define mtx_dirty       t1
    #define mtx_offset      t3
    #define palette_size    t4
    #define palette_index   t5
    #define dirty_flags     t6
    #define cur_flag        t7
    #define palette_dram    s1
    #define mvp_dram        s2
    #define flag_offset     s3
    #define mtx_lhs         s5
    #define mtx_rhs         s6
    #define mtx_dst         s7

    lw palette_dram, %lo(GL_PALETTE_PTR)
    lw palette_index, %lo(GL_STATE_PALETTE_INDEX)

    # Write back current palette matrix to the palette storage
    li s4, %lo(GL_MATRIX_PALETTE)
    li t0, DMA_SIZE(MATRIX_SIZE, 1)
    jal DMAOutAsync
    add s0, palette_dram, palette_index

    lw mvp_dram, %lo(GL_MVP_PTR)
    li mtx_lhs, %lo(GL_MATRIX_PROJECTION)
    li mtx_rhs, %lo(GL_MATRIX_PALETTE)
    li mtx_dst, %lo(GL_MATRIX_MVP)
    li palette_index, 0
    li palette_size, MATRIX_PALETTE_SIZE

    # TODO: Load projection matrix into vector regs only once
gl_matrix_palette_loop:
    beq palette_index, palette_size, 2f
    srl flag_offset, palette_index, 3

    lbu dirty_flags, %lo(GL_STATE_PALETTE_DIRTY)(flag_offset)
    and cur_flag, palette_index, 0x7
    sllv cur_flag, v1, cur_flag
    and mtx_dirty, dirty_flags, cur_flag
    sll mtx_offset, palette_index, 6
    beqz mtx_dirty, gl_matrix_palette_loop
    addi palette_index, 1

    xor dirty_flags, mtx_dirty
    sb dirty_flags, %lo(GL_STATE_PALETTE_DIRTY)(flag_offset)

    move s4, mtx_rhs
    jal DMAIn
    add s0, palette_dram, mtx_offset

    jal GL_MtxMult
    move s4, mtx_dst

    add s0, mvp_dram, mtx_offset
    jal_and_j DMAOutAsync, gl_matrix_palette_loop
    
1:
    and mtx_dirty, state_flags, FLAG_FINAL_MTX_DIRTY
    beqz mtx_dirty, 2f
    li mtx_lhs, %lo(GL_MATRIX_PROJECTION)
    li mtx_rhs, %lo(GL_MATRIX_MODELVIEW)
    jal GL_MtxMult
    li mtx_dst, %lo(GL_MATRIX_MVP)
    and state_flags, ~FLAG_FINAL_MTX_DIRTY

    #undef mtx_dirty
    #undef mtx_offset
    #undef palette_size
    #undef palette_index
    #undef dirty_flags
    #undef cur_flag
    #undef palette_dram
    #undef mvp_dram
    #undef flag_offset
    #undef mtx_lhs
    #undef mtx_rhs
    #undef mtx_dst

2:

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

    # Check if eye space transformation is needed in T&L
    and t1, state_flags, FLAG_LIGHTING | FLAG_FOG | FLAG_TEXTURE_ACTIVE
    sltu t1, zero, t1
    sll t1, NEED_EYE_SPACE_SHIFT
    and t2, state_flags, ~FLAG_NEED_EYE_SPACE
    or state_flags, t2, t1

    #define vmodes  $v01
    #define vsphere $v02
    #define v___    $v29

    # Check which tex gen modes are active
    srl t0, state_flags, TEX_GEN_S_SHIFT
    andi t0, 0xF
    ctc2 t0, COP2_CTRL_VCC
    li s1, %lo(GL_TEX_GEN)
    lqv vmodes, TEX_GEN_MODE_OFFSET, s1
    vmrg vmodes, vzero
    vand vsphere, vmodes, K2
    veq  v___, vsphere, vzero
    vmrg vmodes, vzero
    vor vsphere, vsphere.q1
    vor vsphere, vsphere.e2
    vor vmodes, vmodes.q1
    vor vmodes, vmodes.e2
    mfc2 t0, vsphere.e0
    mfc2 t1, vmodes.e0
    sltu t0, zero, t0
    sltu t1, zero, t1
    sll t0, TEX_GEN_SPHERICAL_FLAG_SHIFT
    sll t1, TEX_GEN_LINEAR_FLAG_SHIFT
    or t0, t1
    and t2, state_flags, ~(FLAG_TEX_GEN_LINEAR | FLAG_TEX_GEN_SPHERICAL)
    or state_flags, t2, t0

    #undef vmodes
    #undef vsphere
    #undef v___

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
  ori $at, $zero, %lo(GL_PIPELINES)                  ## L:326  |      ^ | vec16 pipeline = load(GL_PIPELINES, 0).xyzw;
  ldv $v28, 0, 0, $at                                ## L:326  |      2 | vec16 pipeline = load(GL_PIPELINES, 0).xyzw;
  ori $at, $zero, %lo(MG_SHADER_TEXT)                ## L:327  |      3 | store(pipeline.xyzw, MG_SHADER_TEXT);
  sdv $v28, 0, 0, $at                                ## L:327  |    **6 | store(pipeline.xyzw, MG_SHADER_TEXT);
  ori $at, $zero, %lo(GL_VIEWPORT_SCALE)             ## L:332  |      7 | vec16 viewport = load(GL_VIEWPORT_SCALE, 0);
  lqv $v28, 0, 0, $at                                ## L:332  |      8 | vec16 viewport = load(GL_VIEWPORT_SCALE, 0);
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:333  |      9 | store(viewport, MG_VIEWPORT, 0);
  sqv $v28, 0, 0, $at                                ## L:333  |   **12 | store(viewport, MG_VIEWPORT, 0);
  lh $s6, %lo(GL_MATRIX_PROJECTION + 22)             ## L:339  |     13 | s16 m23i = load(GL_MATRIX_PROJECTION, 0x16);
  lhu $s5, %lo(GL_MATRIX_PROJECTION + 54)            ## L:340  |     14 | u16 m23f = load(GL_MATRIX_PROJECTION, 0x36);
  or $s4, $s6, $s5                                   ## L:341  |   **17 | u16 m23 = m23i | m23f;
  bne $s4, $zero, LABEL_GLCmd_PreInitMagma_0001      ## L:342  |     18 | if (m23 == 0)
  lw $s7, %lo(GL_STATE_FLAGS + 0)                    ## L:321  |    *20 | u32 flags = load(GL_STATE_FLAGS, 0);
  ori $s3, $zero, 0xFFFF                             ## L:346  |     21 | u16 norm_factor = 0xFFFF;
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0002    ## L:347  |     22 | store(norm_factor, MG_VIEWPORT, 0x6);
  sh $s3, %lo(6 + MG_VIEWPORT)($zero)                ## L:347  |    *24 | store(norm_factor, MG_VIEWPORT, 0x6);
  LABEL_GLCmd_PreInitMagma_0001:
  ori $at, $zero, %lo(GL_MATRIX_PROJECTION)          ## L:369  |     25 | m:sint = load(GL_MATRIX_PROJECTION, 0x10);
  lqv $v27, 0, 16, $at                               ## L:369  |     26 | m:sint = load(GL_MATRIX_PROJECTION, 0x10);
  lqv $v28, 0, 48, $at                               ## L:370  |     27 | m:ufract = load(GL_MATRIX_PROJECTION, 0x30);
  vmudl $v29, $v28, $v28.e2                          ## L:373  |  ***31 | vec32 a = m * m.z;
  vmadm $v29, $v27, $v28.e2                          ## L:373  |     32 | vec32 a = m * m.z;
  vmadn $v26, $v28, $v27.e2                          ## L:373  |     33 | vec32 a = m * m.z;
  vmadh $v25, $v27, $v27.e2                          ## L:373  |     34 | vec32 a = m * m.z;
  vsubc $v24, $v26, $v00.e7                          ## L:375  |   **37 | vec32 b = a - 1;
  vsub $v23, $v25, $v30.e7                           ## L:375  |     38 | vec32 b = a - 1;
  vrcph $v25.e0, $v25.e6                             ## L:377  |     39 | a.x = invert_half(a).Z;
  vrcpl $v26.e0, $v26.e6                             ## L:377  |     40 | a.x = invert_half(a).Z;
  vrcph $v25.e0, $v00.e6                             ## L:377  |     41 | a.x = invert_half(a).Z;
  vmudl $v29, $v26, $v24.e2                          ## L:379  |   **44 | a *= b.z;
  vmadm $v29, $v25, $v24.e2                          ## L:379  |     45 | a *= b.z;
  vmadn $v26, $v26, $v23.e2                          ## L:379  |     46 | a *= b.z;
  vmadh $v25, $v25, $v23.e2                          ## L:379  |     47 | a *= b.z;
  vmudn $v26, $v26, $v30.e6                          ## L:380  |   **50 | a *= 2;
  vmadh $v25, $v25, $v30.e6                          ## L:380  |     51 | a *= 2;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:382  |      ^ | store(a:sfract.x, MG_VIEWPORT, 0x6);
  ssv $v26, 0, 6, $at                                ## L:382  |   **54 | store(a:sfract.x, MG_VIEWPORT, 0x6);
  LABEL_GLCmd_PreInitMagma_0002:
  lhu $s6, %lo(GL_TRI_CMD + 0)                       ## L:388  |     55 | u16 tri_cmd = load(GL_TRI_CMD, 0);
  sh $s6, %lo(0 + MG_TRI_CMD)($zero)                 ## L:389  |   **58 | store(tri_cmd, MG_TRI_CMD, 0);
  lbu $s6, %lo(GL_TRI_CULL + 0)                      ## L:394  |     59 | u8 cull = load(GL_TRI_CULL, 0);
  sb $s6, %lo(0 + MG_CULL_MODE)($zero)               ## L:395  |   **62 | store(cull, MG_CULL_MODE, 0);
  ori $at, $zero, %lo(GL_MATRIX_MODELVIEW)           ## L:402  |     63 | m0:sint   = load(GL_MATRIX_MODELVIEW, 0x00);
  lqv $v27, 0, 0, $at                                ## L:402  |     64 | m0:sint   = load(GL_MATRIX_MODELVIEW, 0x00);
  lqv $v25, 0, 16, $at                               ## L:403  |     65 | m1:sint   = load(GL_MATRIX_MODELVIEW, 0x10);
  lqv $v28, 0, 32, $at                               ## L:404  |     66 | m0:ufract = load(GL_MATRIX_MODELVIEW, 0x20);
  lqv $v26, 0, 48, $at                               ## L:405  |     67 | m1:ufract = load(GL_MATRIX_MODELVIEW, 0x30);
  ori $at, $zero, %lo(MATRICES_MV)                   ## L:407  |     68 | store(m0:sint, MATRICES_MV, 0x00);
  sqv $v27, 0, 0, $at                                ## L:407  |    *70 | store(m0:sint, MATRICES_MV, 0x00);
  sqv $v25, 0, 16, $at                               ## L:408  |     71 | store(m1:sint, MATRICES_MV, 0x10);
  sqv $v28, 0, 32, $at                               ## L:409  |     72 | store(m0:ufract, MATRICES_MV, 0x20);
  sqv $v26, 0, 48, $at                               ## L:410  |     73 | store(m1:ufract, MATRICES_MV, 0x30);
  vmudl $v28, $v28, $v31.e0                          ## L:413  |      ^ | m0 = m0 >> 1;
  vmadm $v27, $v27, $v31.e0                          ## L:413  |     74 | m0 = m0 >> 1;
  vmadn $v28, $v00, $v00                             ## L:413  |     75 | m0 = m0 >> 1;
  vmudl $v26, $v26, $v31.e0                          ## L:414  |     76 | m1 = m1 >> 1;
  vmadm $v25, $v25, $v31.e0                          ## L:414  |     77 | m1 = m1 >> 1;
  vmadn $v26, $v00, $v00                             ## L:414  |     78 | m1 = m1 >> 1;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:416  |      ^ | store(m0:sfract, MATRICES_NORMAL, 0x00);
  sqv $v28, 0, 0, $at                                ## L:416  |     79 | store(m0:sfract, MATRICES_NORMAL, 0x00);
  sqv $v26, 0, 16, $at                               ## L:417  |   **82 | store(m1:sfract, MATRICES_NORMAL, 0x10);
  ori $at, $zero, %lo(GL_MATRIX_MVP)                 ## L:421  |     83 | vec16 m0 = load(GL_MATRIX_MVP, 0x00);
  lqv $v28, 0, 0, $at                                ## L:421  |     84 | vec16 m0 = load(GL_MATRIX_MVP, 0x00);
  lqv $v27, 0, 16, $at                               ## L:422  |     85 | vec16 m1 = load(GL_MATRIX_MVP, 0x10);
  lqv $v26, 0, 32, $at                               ## L:423  |     86 | vec16 m2 = load(GL_MATRIX_MVP, 0x20);
  lqv $v25, 0, 48, $at                               ## L:424  |     87 | vec16 m3 = load(GL_MATRIX_MVP, 0x30);
  ori $at, $zero, %lo(MATRICES_MVP)                  ## L:425  |     88 | store(m0, MATRICES_MVP, 0x00);
  sqv $v28, 0, 0, $at                                ## L:425  |    *90 | store(m0, MATRICES_MVP, 0x00);
  sqv $v27, 0, 16, $at                               ## L:426  |     91 | store(m1, MATRICES_MVP, 0x10);
  sqv $v26, 0, 32, $at                               ## L:427  |     92 | store(m2, MATRICES_MVP, 0x20);
  sqv $v25, 0, 48, $at                               ## L:428  |     93 | store(m3, MATRICES_MVP, 0x30);
  ori $at, $zero, %lo(GL_MATRIX_TEXTURE)             ## L:434  |     94 | tm0:sint   = load(GL_MATRIX_TEXTURE, 0x00);
  lqv $v27, 0, 0, $at                                ## L:434  |     95 | tm0:sint   = load(GL_MATRIX_TEXTURE, 0x00);
  lqv $v28, 0, 32, $at                               ## L:435  |     96 | tm0:ufract = load(GL_MATRIX_TEXTURE, 0x20);
  llv $v25, 0, 24, $at                               ## L:438  |     97 | tm3:sint   = load(GL_MATRIX_TEXTURE, 0x18).xy;
  llv $v26, 0, 56, $at                               ## L:439  |  **100 | tm3:ufract = load(GL_MATRIX_TEXTURE, 0x38).xy;
  vmudl $v29, $v28, $v28.v                           ## L:441  |      ^ | vec32 tmp = tm0 * tm0;
  vmadm $v29, $v27, $v28.v                           ## L:441  |    101 | vec32 tmp = tm0 * tm0;
  vmadn $v24, $v28, $v27.v                           ## L:441  |    102 | vec32 tmp = tm0 * tm0;
  vmadh $v23, $v27, $v27.v                           ## L:441  |    103 | vec32 tmp = tm0 * tm0;
  vaddc $v22, $v24, $v24.h1                          ## L:442  |  **106 | vec32 tmp2 = tmp + tmp.yyyyYYYY;
  vadd $v21, $v23, $v23.h1                           ## L:442  |    107 | vec32 tmp2 = tmp + tmp.yyyyYYYY;
  vaddc $v24, $v22, $v24.h2                          ## L:443  |  **110 | tmp = tmp2 + tmp.zzzzZZZZ;
  vadd $v23, $v21, $v23.h2                           ## L:443  |    111 | tmp = tmp2 + tmp.zzzzZZZZ;
  vrsqh $v21.e0, $v23.e0                             ## L:444  | ***115 | tmp2.x = invert_half_sqrt(tmp).x;
  vrsql $v22.e0, $v24.e0                             ## L:444  |    116 | tmp2.x = invert_half_sqrt(tmp).x;
  vrsqh $v21.e0, $v23.e4                             ## L:445  |    117 | tmp2.X = invert_half_sqrt(tmp).X;
  vrsql $v22.e4, $v24.e4                             ## L:445  |    118 | tmp2.X = invert_half_sqrt(tmp).X;
  vrsqh $v21.e4, $v00.e0                             ## L:445  |    119 | tmp2.X = invert_half_sqrt(tmp).X;
  vrcph $v27.e0, $v21.e0                             ## L:446  | ***123 | tm0.x = invert_half(tmp2).x;
  vrcpl $v28.e0, $v22.e0                             ## L:446  |    124 | tm0.x = invert_half(tmp2).x;
  vrcph $v27.e0, $v21.e4                             ## L:447  |    125 | tm0.y = invert_half(tmp2).X;
  vrcpl $v28.e1, $v22.e4                             ## L:447  |    126 | tm0.y = invert_half(tmp2).X;
  vrcph $v27.e1, $v00.e4                             ## L:447  |    127 | tm0.y = invert_half(tmp2).X;
  vmudl $v29, $v28, $v30.e2                          ## L:448  |  **130 | tm0 = tm0 << 5;
  vmadn $v27, $v27, $v30.e2                          ## L:448  |    131 | tm0 = tm0 << 5;
  vmudn $v28, $v28, $v30.e2                          ## L:448  |    132 | tm0 = tm0 << 5;
  ori $at, $zero, %lo(GL_STATE_TEX_SIZE)             ## L:450  |      ^ | vec16 tex_size = load(GL_STATE_TEX_SIZE, 0).xy;
  llv $v20, 0, 0, $at                                ## L:450  |    133 | vec16 tex_size = load(GL_STATE_TEX_SIZE, 0).xy;
  vmudn $v26, $v26, $v20.v                           ## L:452  | ***137 | tm3 = tm3 * tex_size;
  vmadh $v25, $v25, $v20.v                           ## L:452  |    138 | tm3 = tm3 * tex_size;
  vmudl $v29, $v26, $v30.e2                          ## L:453  |  **141 | tm3 = tm3 << 5;
  vmadn $v25, $v25, $v30.e2                          ## L:453  |    142 | tm3 = tm3 << 5;
  vmudn $v26, $v26, $v30.e2                          ## L:453  |    143 | tm3 = tm3 << 5;
  vmudm $v29, $v20, $v28.v                           ## L:455  |    144 | tex_size = tex_size * tm0;
  vmadh $v20, $v20, $v27.v                           ## L:455  |    145 | tex_size = tex_size * tm0;
  llv $v19, 0, 4, $at                                ## L:457  |      ^ | vec16 tex_offset = load(GL_STATE_TEX_SIZE, 4).xy;
  vaddc $v19, $v19, $v25.v                           ## L:458  | ***149 | tex_offset = tex_offset + tm3;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:460  |      ^ | store(tex_size.xy, TEXTURING_SCALE, 0);
  slv $v20, 0, 0, $at                                ## L:460  |    150 | store(tex_size.xy, TEXTURING_SCALE, 0);
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:461  |    151 | store(tex_offset.xy, TEXTURING_OFFSET, 0);
  or $s6, $zero, $zero                               ## L:467  |    152 | u8 light_count = 0;
  or $s5, $zero, $zero                               ## L:468  |    153 | u8 point_count = 0;
  andi $s4, $s7, 1024                                ## L:470  |    154 | u32 enabled = flags & 0x400;
  beq $s4, $zero, LABEL_GLCmd_PreInitMagma_0003      ## L:471  |    155 | if (enabled != 0)
  slv $v19, 0, 0, $at                                ## L:461  |   *157 | store(tex_offset.xy, TEXTURING_OFFSET, 0);
  ori $at, $zero, %lo(GL_STATE_LIGHT_AMBIENT)        ## L:473  |    158 | ambient.xyzw = load(GL_STATE_LIGHT_AMBIENT, 0);
  lqv $v28, 0, 0, $at                                ## L:473  |    159 | ambient.xyzw = load(GL_STATE_LIGHT_AMBIENT, 0);
  lui $s3, 0x10                                      ## L:474  |    160 | u32 light_flag = 0x100000;
  ori $s2, $zero, %lo(GL_LIGHTS)                     ## L:475  |    161 | u16 light_ptr = GL_LIGHTS;
  ori $s1, $zero, %lo(GL_TEX_GEN)                    ## L:476  |    162 | u16 light_end = GL_TEX_GEN;
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:477  |    163 | u16 out_ptr = LIGHTING_LIGHTS;
  LABEL_GLCmd_PreInitMagma_0005:
  sltu $at, $s2, $s1                                 ## L:478  |    164 | while (light_ptr < light_end)
  beq $at, $zero, LABEL_GLCmd_PreInitMagma_0006      ## L:478  |    165 | while (light_ptr < light_end)
  nop                                                ## L:478  |   *167 | while (light_ptr < light_end)
  and $fp, $s7, $s3                                  ## L:480  |    168 | u32 light_enabled = flags & light_flag;
  beq $fp, $zero, LABEL_GLCmd_PreInitMagma_0007      ## L:481  |    169 | if (light_enabled != 0) {
  nop                                                ## L:481  |   *171 | if (light_enabled != 0) {
  lqv $v27, 0, 0, $s2                                ## L:482  |    172 | vec16 light = load(light_ptr, 0);
  sdv $v27, 8, 8, $s0                                ## L:484  | ***176 | store(light.XYZW, out_ptr, 8);
  lh $sp, 6($s2)                                     ## L:486  |    177 | s16 w = load(light_ptr, 6);
  beq $sp, $zero, LABEL_GLCmd_PreInitMagma_0008      ## L:487  |  **180 | if (w != 0) {
  sdv $v27, 0, 0, $s0                                ## L:483  |   *182 | store(light.xyzw, out_ptr, 0);
  addiu $s5, $s5, 1                                  ## L:488  |    183 | point_count += 1;
  LABEL_GLCmd_PreInitMagma_0008:
  addiu $s6, $s6, 1                                  ## L:491  |    184 | light_count += 1;
  addiu $s0, $s0, 16                                 ## L:492  |    185 | out_ptr += 16;
  LABEL_GLCmd_PreInitMagma_0007:
  sll $s3, $s3, 1                                    ## L:495  |    186 | light_flag = light_flag << 1;
  j LABEL_GLCmd_PreInitMagma_0005                    ## L:495  |    187 | light_flag = light_flag << 1;
  addiu $s2, $s2, 16                                 ## L:494  |   *189 | light_ptr += 16;
  LABEL_GLCmd_PreInitMagma_0006:
  beq $zero, $zero, LABEL_GLCmd_PreInitMagma_0004    ## L:495  |    190 | light_flag = light_flag << 1;
  nop                                                ## L:495  |   *192 | light_flag = light_flag << 1;
  LABEL_GLCmd_PreInitMagma_0003:
  addiu $at, $zero, 32767                            ## L:498  |    193 | ambient.x = 0x7FFF;
  mtc2 $at, $v28.e0                                  ## L:498  |    194 | ambient.x = 0x7FFF;
  vor $v28, $v00, $v28.e0                            ## L:499  | ***198 | ambient = ambient.x;
  LABEL_GLCmd_PreInitMagma_0004:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:502  |      ^ | @Barrier("store_lighting") store(ambient.xyzw, LIGHTING_AMBIENT, 0); ## Barrier: 0x1
  sdv $v28, 0, 0, $at                                ## L:502  | ***202 | @Barrier("store_lighting") store(ambient.xyzw, LIGHTING_AMBIENT, 0); ## Barrier: 0x1
  sb $s5, %lo(0 + LIGHTING_HAS_POINTS)($zero)        ## L:503  |    203 | @Barrier("store_lighting") store(point_count, LIGHTING_HAS_POINTS, 0); ## Barrier: 0x1
  sb $s6, %lo(0 + LIGHTING_COUNT)($zero)             ## L:504  |    204 | @Barrier("store_lighting") store(light_count, LIGHTING_COUNT, 0); ## Barrier: 0x1
  ori $at, $zero, %lo(GL_STATE_FOG_PARAMS)           ## L:509  |    205 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  llv $v27, 0, 0, $at                                ## L:509  |    206 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  llv $v28, 0, 4, $at                                ## L:509  |    207 | vec32 fog_parms = load(GL_STATE_FOG_PARAMS, 0).xy;
  vsubc $v26, $v28, $v28.e0                          ## L:510  | ***211 | vec32 diff = fog_parms - fog_parms.x;
  vsub $v25, $v27, $v27.e0                           ## L:510  |    212 | vec32 diff = fog_parms - fog_parms.x;
  vrcph $v23.e0, $v25.e1                             ## L:512  | ***216 | factor.x = invert_half(diff).y;
  vrcpl $v24.e0, $v26.e1                             ## L:512  |    217 | factor.x = invert_half(diff).y;
  vrcph $v23.e0, $v00.e1                             ## L:512  |    218 | factor.x = invert_half(diff).y;
  vmudl $v22, $v24, $v31.e4                          ## L:513  |  **221 | vec16 factor_f = factor << 11;
  vmadn $v22, $v23, $v31.e4                          ## L:513  |    222 | vec16 factor_f = factor << 11;
  vsubc $v21, $v00, $v28.e0                          ## L:514  |    223 | vec32 offset = VZERO - fog_parms.x;
  vsub $v20, $v00, $v27.e0                           ## L:514  |    224 | vec32 offset = VZERO - fog_parms.x;
  vmudl $v29, $v21, $v30.e2                          ## L:515  |  **227 | offset = offset << 5;
  vmadn $v20, $v20, $v30.e2                          ## L:515  |    228 | offset = offset << 5;
  andi $s6, $s7, 8                                   ## L:517  |      ^ | u32 enabled = flags & 0x8;
  addiu $s5, $zero, 119                              ## L:518  |    229 | u8 mask = 0x77;
  bne $s6, $zero, LABEL_GLCmd_PreInitMagma_0009      ## L:519  |    230 | if (enabled == 0) {
  vmudn $v21, $v21, $v30.e2                          ## L:515  |   *232 | offset = offset << 5;
  addiu $s5, $zero, 255                              ## L:520  |    233 | mask = 0xFF;
  LABEL_GLCmd_PreInitMagma_0009:
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:523  |    234 | store(offset:sint.x, FOG_PARAMS, 0x0);
  ssv $v21, 0, 2, $at                                ## L:524  |   *236 | store(offset:ufract.x, FOG_PARAMS, 0x2);
  ssv $v22, 0, 4, $at                                ## L:525  |    237 | store(factor_f.x, FOG_PARAMS, 0x4);
  sb $s5, %lo(0 + FOG_MASK)($zero)                   ## L:526  |    238 | store(mask, FOG_MASK, 0);
  j DmaMagmaState                                    ## L:529  |    239 | DmaMagmaState();
  ssv $v20, 0, 0, $at                                ## L:523  |   *241 | store(offset:sint.x, FOG_PARAMS, 0x0);

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
