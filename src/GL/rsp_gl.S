############################################################
# rsp_gl.S
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
#          Dennis Heinze <dennisjp.heinze@gmail.com>
############################################################
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
        RSPQ_DefineCommand GLCmd_SetPaletteIdx, 4   # 0xD
        RSPQ_DefineCommand GLCmd_MatrixCopy,    4   # 0xE
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
    #include "rsp_gl_state.inc"
    RSPQ_EndSavedState

UPDATE_FUNCTIONS:
    .short RSPQ_Loop                    - _start  # Do nothing
    .short GL_UpdateScissor             - _start
    .short GL_UpdateTextureCompleteness - _start
    .short GL_UpdateTextureObjects      - _start

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

    addi s0, a0, %lo(GL_STATE) + GLP_LIGHT_POSITION
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
