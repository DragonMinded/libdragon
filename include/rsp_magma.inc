#include <rsp_queue.inc>
#include "magma_constants.h"

#define TRIANGLE_SIZE       0xB0
#define MAX_UNIFORMS_SIZE   0x200

.macro MgBeginShaderUniforms
    .section .meta.mg_uniforms
_MG_META_UNIFORMS:
    .equ _mg_uniform_count, 0
    .data

    RSPQ_BeginSavedState
    
    .align 3
    MG_VERTEX_CACHE:            .dcb.b  MG_VERTEX_CACHE_COUNT * MG_VTX_SIZE
    MG_VERTEX_OVERFLOW:         .dcb.b  MG_VTX_SIZE

    .align 4
MG_STATE:
    MG_VIEWPORT:
        MG_VIEWPORT_SCALE:      .half   0,0,0,0
        MG_VIEWPORT_OFFSET:     .half   0,0,0,0
    MG_CLIP_FACTORS:            .half   1,1,MG_DEFAULT_GUARD_BAND,MG_DEFAULT_GUARD_BAND
    MG_VERTEX_SIZE:             .half   0,0,0,0
    MG_SHADER_TEXT:             .word   0
    MG_SHADER_TEXT_SIZE:        .word   0
    MG_CLIPPING_TEXT:           .word   0
    MG_CLIPPING_TEXT_SIZE:      .word   0
    MG_VERTEX_BUFFER:           .word   0
    MG_TRI_CMD:                 .half   0x800
    MG_CULL_MODE:               .byte   0
    MG_OUTPUT_OFFSET:           .byte   0

    .align 3
_MG_VTX_SHADER_UNIFORMS:
.endm

.macro MgEndShaderUniforms
    .align 3
_MG_VTX_SHADER_UNIFORMS_END:
    RSPQ_EndSavedState
.endm

.equ _mg_defining_uniform, 0

.macro MgBeginUniform name binding
    .ifne _mg_defining_uniform
        .error "MgBeginUniform must not be preceded by another MgBeginUniform"
        .exitm
    .endif
    .equ _mg_defining_uniform, 1

    .equ _mg_uniform_count, _mg_uniform_count+1

    .section .meta.mg_uniforms
    .long \binding
    .long \name - _MG_VTX_SHADER_UNIFORMS
    .data
    .align 3
\name:
.endm

.macro MgEndUniform
    .ifeq _mg_defining_uniform
        .error "MgEndUniform must be preceded by MgBeginUniform"
        .exitm
    .endif
    .equ _mg_defining_uniform, 0

    .section .meta.mg_uniforms
    .long _MG_UNIFORM_END\@ - _MG_VTX_SHADER_UNIFORMS
    .data
    .align 3
_MG_UNIFORM_END\@ :
.endm

# Defines the uniform region using the maximum allowable size, but filled with zeroes.
# This is needed to make space for uniforms once in the initial overlay.
# Whenever shaders are loaded, they will be reading/writing from that area automatically.
.macro _MgDefineUniformRegion
    MgBeginShaderUniforms
        .fill MAX_UNIFORMS_SIZE
    MgEndShaderUniforms
.endm

.macro MgBeginVertexInput
    .section .meta.mg_attributes
_MG_META_ATTRIBUTES:
    .equ _mg_attribute_count, 0
.endm

.macro MgEndVertexInput
.endm

.macro MgBeginVertexAttribute input is_optional=0
    .equ _mg_attribute_count, _mg_attribute_count + 1

    .equ _mg_attribute_loader_count, 0
    .equ _mg_attribute_patches_count, 0

    .subsection 0
    .long \input
    .long \is_optional
    .long _MG_ATTRIBUTE_LOADERS\input - _MG_META_ATTRIBUTES
    .long _MG_ATTRIBUTE_PATCHES\input - _MG_META_ATTRIBUTES

    .subsection 1
_MG_ATTRIBUTE_LOADERS\input:

    .subsection 2
_MG_ATTRIBUTE_PATCHES\input:
.endm

.macro MgEndVertexAttribute
    .subsection 0
    .long _mg_attribute_loader_count
    .long _mg_attribute_patches_count
.endm

.macro _MgAttrLoader first rest:vararg
    .ifnb \first
        .equ _mg_attribute_loader_count, _mg_attribute_loader_count + 1
        .long \first - _MG_OVERLAY
        _MgAttrLoader \rest
    .endif
.endm

.macro MgVertexAttributeLoaders loaders:vararg
    .subsection 1
    _MgAttrLoader \loaders
.endm

.macro MgBeginVertexAttributePatch address
    .subsection 2
    .equ _mg_attribute_patches_count, _mg_attribute_patches_count + 1
    .long \address - _MG_OVERLAY
.endm

.macro MgEndVertexAttributePatch
.endm

.macro MgBeginShader
    .section .meta
_MG_META_START:
    .long _mg_uniform_count
    .long _MG_META_UNIFORMS - _MG_META_START
    .long _mg_attribute_count
    .long _MG_META_ATTRIBUTES - _MG_META_START
    _MgBeginShader
.endm

.macro _MgBeginShader
    .section .text.mg_overlay
    .align 3
_MG_OVERLAY:

_MG_CLIPPING:
    # Jump to the actual bootstrapper.
    # This piece of code is kept constant, so that _MG_VTX_SHADER is always at the same address.
    j Mg_Clipping
    nop

_MG_VTX_SHADER:
.endm

.macro MgEndShader
    # Place the clipping code bootstrapper in each shader so it can get loaded when required.
Mg_Clipping:
    # Bootstrap the clipping code.
    lw s0, %lo(MG_CLIPPING_TEXT)
    lw t0, %lo(MG_CLIPPING_TEXT_SIZE)
    jal_and_j Mg_LoadOverlay, _MG_CLIPPING
.endm

# Implementation of the shader code bootstrapper
.macro _MgBootstrapShader
    jal_and_j Mg_LoadShader, _MG_VTX_SHADER
.endm


    .data

	RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand MgCmd_SetByte,       8
        RSPQ_DefineCommand MgCmd_SetShort,      8
        RSPQ_DefineCommand MgCmd_SetWord,       8
        RSPQ_DefineCommand MgCmd_SetQuad,       20
        RSPQ_DefineCommand MgCmd_SetShader,     8
        RSPQ_DefineCommand MgCmd_LoadVertices,  8
        RSPQ_DefineCommand MgCmd_DrawIndices,   8
        RSPQ_DefineCommand MgCmd_DrawEnd,       4
        RSPQ_DefineCommand MgCmd_LoadUniform,   8
        RSPQ_DefineCommand MgCmd_LoadUniform,   MG_INLINE_UNIFORM_HEADER + 8
        RSPQ_DefineCommand MgCmd_LoadUniform,   MG_INLINE_UNIFORM_HEADER + 16
        RSPQ_DefineCommand MgCmd_LoadUniform,   MG_INLINE_UNIFORM_HEADER + 32
        RSPQ_DefineCommand MgCmd_LoadUniform,   MG_INLINE_UNIFORM_HEADER + 64
        RSPQ_DefineCommand MgCmd_LoadUniform,   MG_INLINE_UNIFORM_HEADER + 128
        RSPQ_DefineCommand MgCmd_LoadUniform,   248 # TODO: get rid of magic number
	RSPQ_EndOverlayHeader

    .align 4
    .ascii "Dragon RSP Magma"
    .ascii "Code by snacchus"

    .align 4
MG_NORMAL_MASK:                 .half   0xF800, 0x07E0, 0x001F, 0, 1, 1<<5, 1<<11, 0
MG_CLIP_CACHE_OFFSETS:          .half   2,4,6,8,10,12,14,16,18

MG_CLIP_PLANES:              
    .half 1, 0, 0,  1
    .half 0, 1, 0,  1
    .half 0, 0, 1,  1
    .half 1, 0, 0, -1
    .half 0, 1, 0, -1
    .half 0, 0, 1, -1

    .bss
    .align 4
MG_CLIP_CACHE_USED:             .dcb.w  16
    .align 3
MG_OUTPUT_BUFFER:               .dcb.b  TRIANGLE_SIZE * 2

MG_CLIP_CACHE:                  .dcb.b  MG_VTX_SIZE * MG_CLIP_CACHE_SIZE
MG_CLIP_CACHE_END:

MG_CLIP_LIST0:                  .dcb.w  MG_CLIP_CACHE_SIZE
MG_CLIP_LIST1:                  .dcb.w  MG_CLIP_CACHE_SIZE

    .text

    #############################################################
    # MgCmd_SetByte
    #############################################################
    .func MgCmd_SetByte
MgCmd_SetByte:
    jr ra
    sb a1, %lo(MG_STATE)(a0)
    .endfunc

    #############################################################
    # MgCmd_SetShort
    #############################################################
    .func MgCmd_SetShort
MgCmd_SetShort:
    jr ra
    sh a1, %lo(MG_STATE)(a0)
    .endfunc

    #############################################################
    # MgCmd_SetWord
    #############################################################
    .func MgCmd_SetWord
MgCmd_SetWord:
    jr ra
    sw a1, %lo(MG_STATE)(a0)
    .endfunc

    #############################################################
    # MgCmd_SetQuad
    #############################################################
    .func MgCmd_SetQuad
MgCmd_SetQuad:
    addi s0, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER)
    lqv $v01, -0x10,s0
    lrv $v01,  0x00,s0
    addi a0, %lo(MG_STATE)
    jr ra
    sqv $v01,  0x00,a0
    .endfunc

    #############################################################
    # MgCmd_LoadUniform
    # ARGS:
    #   a0: RDRAM pointer
    #   a1: Bit 31..16: Size
    #       Bit 15..0: Offset into _MG_VTX_SHADER_UNIFORMS
    #############################################################
    .func MgCmd_LoadUniform
MgCmd_LoadUniform:
    move s0, a0
    addi s4, a1, %lo(_MG_VTX_SHADER_UNIFORMS)
    j DMAIn
    srl t0, a1, 16
    
    .endfunc

    #############################################################
    # MgCmd_SetShader
    # ARGS:
    #   a0: RDRAM pointer to shader ucode text
    #   a1: shader text size
    #############################################################
    .func MgCmd_SetShader
MgCmd_SetShader:
    # TODO: Adjust size of saved data in case the shader uses less DMEM for uniforms.
    #       In theory this could be done by overwriting the state size in the overlay header.
    #       However, this could be problematic because of the profiler buffer that is automatically generated at the end of the state.
    #       There are two potential fixes:
    #       1) Disable this optimization when profiling is enabled.
    #       2) Move the profiler buffer to the beginning of the saved state.
    #       Provided it doesn't break anything, the second option would be preferred.
    sw a0, %lo(MG_SHADER_TEXT)
    sw a1, %lo(MG_SHADER_TEXT_SIZE)
    # fallthrough
    .endfunc

    .func Mg_LoadShader
Mg_LoadShader:
    lw s0, %lo(MG_SHADER_TEXT)
    lw t0, %lo(MG_SHADER_TEXT_SIZE)
    # fallthrough
    .endfunc

    .func Mg_LoadOverlay
Mg_LoadOverlay:
    j DMAIn
    li s4, %lo(_MG_OVERLAY)
    .endfunc

    #############################################################
    # MgCmd_LoadVertices
    # ARGS:
    #   a0: Bit 31..24: Command id
    #       Bit 23..0:  Vertex buffer offset
    #   a1: Bit 31..16: Cache offset
    #       Bit 15..0:  Vertex count
    #############################################################
    .func MgCmd_LoadVertices
MgCmd_LoadVertices:
    # We can't fall through here because RDPQ_Triangle_Send_Async etc. is always placed after the .text segment
    # and we therefore need to put the vertex shader into it's own segment to force it being placed at the end.
    # That means we can't register the command with the shader's address directly because it is not in the .text segment.
    j _MG_VTX_SHADER
    nop
    .endfunc

    #############################################################
    # MgCmd_DrawIndices
    # 
    # ARGS:
    #   a0: Bit 31..24: Command id
    #       Bit 15..0:  Index 0
    #   a1: Bit 31..16: Index 1
    #       Bit 15..0:  Index 2
    #############################################################
    .func MgCmd_DrawIndices
MgCmd_DrawIndices:
    #define vtx1            a0
    #define vtx2            a2
    #define vtx3            a1
    srl a2, a1, 16
    addi s1, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) - 6
    move s2, s1
    lbu v0, %lo(MG_CULL_MODE)
    lbu t7, %lo(MG_OUTPUT_OFFSET)
    li sp, %lo(RSPQ_Loop)
    lhu a3, %lo(MG_TRI_CMD)
    jal RDPQ_Triangle_Send_Async
    addi s3, t7, %lo(MG_OUTPUT_BUFFER)

    xor t7, TRIANGLE_SIZE
    j RSPQ_Loop
    sb t7, %lo(MG_OUTPUT_OFFSET)
    #undef vtx1
    #undef vtx2
    #undef vtx3
    .endfunc

RDPQ_Triangle_Cull:
    jr sp
    nop

    #############################################################
    # MgCmd_DrawEnd
    #############################################################
    .func MgCmd_DrawEnd
MgCmd_DrawEnd:
    j RDPQ_Triangle_Send_End
    sb zero, %lo(MG_OUTPUT_OFFSET)
    .endfunc

#define RDPQ_TRIANGLE_VTX1      a0
#define RDPQ_TRIANGLE_VTX2      a2
#define RDPQ_TRIANGLE_VTX3      a1
#define RDPQ_TRIANGLE_TRICMD    a3
#define RDPQ_TRIANGLE_VTX1_DMEM 0,s1
#define RDPQ_TRIANGLE_VTX2_DMEM 2,s2
#define RDPQ_TRIANGLE_VTX3_DMEM 4,s2
#define RDPQ_Triangle_Clip      _MG_CLIPPING
#include "rsp_rdpq.inc"
