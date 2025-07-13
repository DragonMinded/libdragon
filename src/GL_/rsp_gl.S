#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_SetFlag,       8   # 0x0
        RSPQ_DefineCommand GLCmd_SetByte,       8   # 0x1
        RSPQ_DefineCommand GLCmd_SetShort,      8   # 0x2
        RSPQ_DefineCommand GLCmd_SetWord,       8   # 0x3
        RSPQ_DefineCommand GLCmd_SetLong,       12  # 0x4
        RSPQ_DefineCommand GLCmd_Update,        4   # 0x5
        RSPQ_DefineCommand GLCmd_BindTexture,   8   # 0x6 
        RSPQ_DefineCommand GLCmd_GetValue,      8   # 0x7
        RSPQ_DefineCommand GLCmd_CopyFillColor, 4   # 0x8
        RSPQ_DefineCommand GLCmd_SetLightPos,   12  # 0x9
        RSPQ_DefineCommand GLCmd_MatrixPush,    4   # 0xA
        RSPQ_DefineCommand GLCmd_MatrixPop,     4   # 0xB
        RSPQ_DefineCommand GLCmd_MatrixLoad,    68  # 0xC
        RSPQ_DefineCommand GLCmd_PreInitPipe,   4   # 0xD
        RSPQ_DefineCommand GLCmd_PreInitPipeTex,4   # 0xE
        RSPQ_DefineCommand GLCmd_SetPaletteIdx, 4   # 0xF
        RSPQ_DefineCommand GLCmd_MatrixCopy,    4   # 0x10
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
    #include "rsp_gl_state.inc"

    # These are only required for RDP state changes
    GL_BOUND_TEXTURES:
        GL_BOUND_TEXTURE_1D:    .ds.b   TEXTURE_OBJECT_SIZE
        GL_BOUND_TEXTURE_2D:    .ds.b   TEXTURE_OBJECT_SIZE
    GL_STATE_SCISSOR_RECT:  .half   0, 0, 0, 0
    GL_STATE_BLEND_CYCLE:   .word   0
    GL_STATE_FOG_COLOR:     .word   0
    GL_STATE_FLAGS2:        .word   0
    GL_STATE_TEXTURE_IDS:   .word   0, 0
    GL_STATE_UPLOADED_TEX:  .word   0
    GL_STATE_FILL_COLOR:    .word   0
    GL_STATE_FILL_DEPTH:    .word   0
    GL_STATE_PALETTE_INDEX: .word   0
    GL_STATE_DITHER_MODE:   .word   0
    GL_STATE_FB_SIZE:       .half   0, 0
    GL_STATE_DEPTH_FUNC:    .half   0
    GL_STATE_ALPHA_FUNC:    .half   0
    GL_STATE_BLEND_SRC:     .half   0
    GL_STATE_BLEND_DST:     .half   0
    GL_STATE_TEX_ENV_MODE:  .half   0
    GL_STATE_ALPHA_REF:     .byte   0
    GL_STATE_PALETTE_DIRTY: .ds.b   PALETTE_DIRTY_FLAGS_SIZE
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
    .short GL_UpdateScissor             - _start
    .short GL_UpdateTextureCompleteness - _start
    .short GL_UpdateTextureObjects      - _start

CONVERT_CONST:  .half 1, 2, 0x8000, 0x200, 0xFF, 134, 0x7F, 0x80

    .bss

    .align 3
TEX_UPLOAD_STAGING: .ds.b 0x150

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
    beqz t1, 1f
    and t3, t2, a1
    or t3, t2, a1

1:
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

    .func GLCmd_SetLong
GLCmd_SetLong:
    sw a2, %lo(GL_STATE) + 4(a0)
    # fallthrough!
    .endfunc

    .func GLCmd_SetWord
GLCmd_SetWord:
    sw a1, %lo(GL_STATE) + 0(a0)
    # fallthrough!
    .endfunc

GLCmd_Update:
    srl t0, a0, 12
    lhu t1, %lo(UPDATE_FUNCTIONS)(t0)
    jr t1
    #nop

GLCmd_GetValue:
    srl t0, a0, 12
    addiu s4, a0, %lo(GL_STATE)
    andi s4, 0xFFF
    andi t0, 0xFFF
    j DMAOut
    move s0, a1

    ########################################
    # GLCmd_BindTexture
    #
    # Args:
    #   a0: Bit 31..24: Command id
    #       Bit 23..16: Offset into GL_STATE_TEXTURE_IDS
    #       Bit 15..0:  Offset into GL_BOUND_TEXTURES
    #   a1: The texture's RDRAM address
    ########################################
GLCmd_BindTexture:
    srl t3, a0, 16
    andi t3, 0xFC
    lw s0, %lo(GL_STATE_TEXTURE_IDS)(t3)
    # Do nothing if texture is already bound
    beq s0, a1, RSPQ_Loop
    addiu s4, a0, %lo(GL_BOUND_TEXTURES)

    # DMA currently bound texture out
    jal DMAOutAsync
    li t0, DMA_SIZE(TEXTURE_OBJECT_SIZE, 1)

    # DMA new texture in
    jal DMAIn
    move s0, a1

    # Always reset uploaded texture when binding
    sw zero, %lo(GL_STATE_UPLOADED_TEX)
    j RSPQ_Loop
    sw s0, %lo(GL_STATE_TEXTURE_IDS)(t3)

GLCmd_CopyFillColor:
    lw a1, %lo(GL_STATE)(a0)
    jal_and_j RDPQ_WriteSetFillColor, RDPQ_Finalize

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

    #############################################################
    # GL_MatrixMarkDirty
    #
    # ARGS:
    #   t1: Matrix index
    #############################################################
    .func GL_MatrixMarkDirty
GL_MatrixMarkDirty:
    #define mtx_index       t1
    #define flag_offset     s7
    #define flag_shift      t6
    #define palette_index   t7
    #define flag            t8
    #define cur_flags       t9
    bne mtx_index, PALETTE_MATRIX_INDEX, 1f
    lw palette_index, %lo(GL_STATE_PALETTE_INDEX)

    li flag, 1
    srl palette_index, 6
    srl flag_offset, palette_index, 3
    lbu cur_flags, %lo(GL_STATE_PALETTE_DIRTY)(flag_offset)
    and flag_shift, palette_index, 0x7
    sllv flag, flag, flag_shift
    or cur_flags, flag
    jr ra
    sb cur_flags, %lo(GL_STATE_PALETTE_DIRTY)(flag_offset)

1:
    lw cur_flags, %lo(GL_STATE_FLAGS)
    # mark final matrix as dirty if modelview or projection matrix was changed
    blt mtx_index, (GL_TEXTURE & 0xF), 1f
    li flag, FLAG_FINAL_MTX_DIRTY
    move flag, zero
1:
    or cur_flags, flag
    jr ra
    sw cur_flags, %lo(GL_STATE_FLAGS)
    #undef mtx_index
    #undef flag_offset
    #undef flag_shift
    #undef palette_index
    #undef flag
    #undef cur_flags
    .endfunc

GL_GetMatrixIndex:
    lbu t0, %lo(GL_STATE_MATRIX_MODE) + 1
    andi t1, t0, GL_MATRIX_PALETTE_ARB
    bnez t1, JrRa
    li t1, PALETTE_MATRIX_INDEX
    jr ra
    move t1, t0

GLCmd_MatrixPush:
    j GL_MatrixPushPop
    li t2, DMA_OUT

GLCmd_MatrixPop:
    li t2, DMA_IN
    # fallthrough!
GL_MatrixPushPop:
    #define post_incr   t0
    #define mtx_index   t1
    #define stack_ptr   t3

    # Get matrix pointer for the current matrix stack
    jal GL_GetMatrixIndex
    nop
    sll stack_ptr, mtx_index, 2
    lw s0, %lo(GL_MATRIX_POINTERS)(stack_ptr)

    # Get location in DMEM for the current matrix stack
    sll s4, mtx_index, 6
    addi s4, %lo(GL_MATRICES)
    
    bltz t2, 1f
    # If pushing the stack, post-increment the stack pointer after the DMA
    li post_incr, MATRIX_SIZE

    # If popping the stack, pre-decrement the stack pointer before the DMA
    addi s0, -MATRIX_SIZE
    # Also mark the matrix as dirty
    jal GL_MatrixMarkDirty
    move post_incr, zero

1:
    add s1, s0, post_incr

    # Save new stack pointer
    sw s1, %lo(GL_MATRIX_POINTERS)(stack_ptr)

    # Do DMA
    li t0, DMA_SIZE(MATRIX_SIZE, 1)
    jal_and_j DMAExec, RSPQ_Loop

GLCmd_MatrixLoad:
    #define multiply    t0
    #define mtx_index   t1
    #define lhs         s5
    #define rhs         s6
    #define dst         s7

    #define vrhs01_i     $v02
    #define vrhs01_f     $v03
    #define vrhs23_i     $v04
    #define vrhs23_f     $v05

    move ra2, ra
    jal GL_GetMatrixIndex
    addi rhs, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 64
    andi multiply, a0, 1
    sll dst, mtx_index, 6
    addiu dst, %lo(GL_MATRICES)

GL_MatrixCopyMultiplyImpl:
    # Load the right hand side matrix from command parameters (misaligned)
    lqv vrhs01_i, 0x00,rhs
    lrv vrhs01_i, 0x10,rhs
    lqv vrhs23_i, 0x10,rhs
    lrv vrhs23_i, 0x20,rhs
    lqv vrhs01_f, 0x20,rhs
    lrv vrhs01_f, 0x30,rhs
    lqv vrhs23_f, 0x30,rhs
    lrv vrhs23_f, 0x40,rhs

    beqz multiply, GL_MtxMultSave # skip multiplication if only loading the matrix
    li ra, 1f

    jal GL_MtxMultWithRhs
    move lhs, dst

1:
    j GL_MatrixMarkDirty
    move ra, ra2

GLCmd_MatrixCopy:
    #############################################################
    # GLCmd_MatrixCopy
    #
    # Copies from 1 modelview stack to another
    #
    # ARGS:
    #   a0: Bit 31..24: Command id
    #       Bit 7..6: Source Matrix
    #############################################################
    move ra2, ra
    jal GL_GetMatrixIndex
    addiu rhs, a0, %lo(GL_MATRICES)
    sll dst, mtx_index, 6
    addiu dst, %lo(GL_MATRICES)
    j GL_MatrixCopyMultiplyImpl
    li multiply, 0
    
    
GL_UpdateScissor:
    lhu a1, %lo(GL_STATE_FB_SIZE) + 0x0
    lhu t7, %lo(GL_STATE_FB_SIZE) + 0x2
    lw a0, %lo(GL_STATE_FLAGS)
    andi a0, FLAG_SCISSOR_TEST
    beqz a0, 1f
    move t2, zero

    lhu t2, %lo(GL_STATE_SCISSOR_RECT) + 0x2
    lhu a0, %lo(GL_STATE_SCISSOR_RECT) + 0x0
    subu t7, t2
    lhu a1, %lo(GL_STATE_SCISSOR_RECT) + 0x4
    lhu t2, %lo(GL_STATE_SCISSOR_RECT) + 0x6
    addu a1, a0
    subu t2, t7, t2
    
1:
    sll t2, 2
    sll a0, 14
    or a0, t2
    lui t2, 0xED00
    or a0, t2
    sll t7, 2
    sll a1, 14
    or a1, t7
    jal_and_j RDPQ_WriteSetScissor, RDPQ_Finalize

GL_UpdateTextureCompleteness:
    #define result      t7
    #define levels      t8
    #define width       t1
    #define height      t2
    #define loop_max    t3
    #define one         t6
    #define loop_var    t0
    #define image       s1
    #define format      s2
    move result, zero   # complete = false
    move levels, zero

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

    # If width and height are both 1 we have reached the end of the mipmap chain
    beq t5, one, gl_tex_complete
    nop

    # Unless width is one, divide it by two
    beq width, one, 1f
    nop
    srl width, 1
1:
    # Unless height is one, divide it by two
    beq height, one, 1f
    nop
    srl height, 1
1:
    # Check if we have reached the maximum number of loops
    beq loop_var, loop_max, gl_tex_incomplete
    addiu image, TEXTURE_IMAGE_SIZE

    # Loop!
    b gl_tex_completeness_loop
    addiu loop_var, 1

gl_tex_complete:
    move levels, loop_var        # levels = i (Number of mipmaps minus one)
gl_tex_mipmaps_disabled:
    li result, TEX_FLAG_COMPLETE # complete = true
gl_tex_incomplete:
    # Save the result
    lw t0, (%lo(GL_BOUND_TEXTURES) + TEXTURE_FLAGS_OFFSET)(a0)
    sh levels, (%lo(GL_BOUND_TEXTURES) + TEXTURE_LEVELS_COUNT_OFFSET)(a0)
    and t0, ~(TEX_FLAG_COMPLETE)
    or t0, result
    jr ra
    sw t0, (%lo(GL_BOUND_TEXTURES) + TEXTURE_FLAGS_OFFSET)(a0)
    #undef result
    #undef width
    #undef height
    #undef loop_max
    #undef one
    #undef loop_var
    #undef image
    #undef format
    #undef ra3

GL_UpdateTextureObjects:
    lw s0, %lo(GL_STATE_TEXTURE_IDS+0)
    beqz s0, 1f
    li s4, %lo(GL_BOUND_TEXTURE_1D)
    jal DMAOut
    li t0, DMA_SIZE(TEXTURE_OBJECT_SIZE, 1)
1:
    lw s0, %lo(GL_STATE_TEXTURE_IDS+4)
    beqz s0, JrRa
    li s4, %lo(GL_BOUND_TEXTURE_2D)
    li t0, DMA_SIZE(TEXTURE_OBJECT_SIZE, 1)
    jal_and_j DMAOut, RSPQ_Loop

    .func GLCmd_SetPaletteIdx
GLCmd_SetPaletteIdx:
    lw s0, %lo(GL_STATE_PALETTE_INDEX)
    lw t3, %lo(GL_PALETTE_PTR)
    and a0, 0xFFFFFF
    beq a0, s0, JrRa
    sw a0, %lo(GL_STATE_PALETTE_INDEX)

    add s0, t3
    li s4, %lo(GL_MATRIX_PALETTE)
    jal DMAOutAsync
    li t0, DMA_SIZE(MATRIX_SIZE, 1)

    add s0, a0, t3
    jal_and_j DMAIn, RSPQ_Loop
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
    sltu t2, zero, t0
    li t0, RDPQ_BLENDER((FOG_RGB, SHADE_ALPHA, IN_RGB, INV_MUX_ALPHA)) | SOM_BLENDING
1:
    sw t0, %lo(RDPQ_MODE_BLENDER_STEPS) + 0x0
    or modes0, t2

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
    sll t0, TEX_COORD_SHIFT
    sll t1, TEX_COORD_SHIFT
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
/*
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
    ldv vpos,     0x00,s0
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

    li s0, %lo(RDPQ_CMD_STAGING)
    spv vpos,  0,s0
    lbu t0, %lo(GL_STATE) + LIGHT_SPOT_EXPONENT_OFFSET(a0)
    lw t1, %lo(RDPQ_CMD_STAGING)
    sw t1, %lo(GL_STATE) + LIGHT_DIRECTION_OFFSET(a0)
    jr ra
    sb t0, %lo(GL_STATE) + LIGHT_SPOT_EXPONENT_OFFSET(a0)

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
*/

#include "rsp_gl_common.inc"
#include <rsp_rdpq.inc>
