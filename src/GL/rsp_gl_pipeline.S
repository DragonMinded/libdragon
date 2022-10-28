#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"
#include "pputils.h"

#define __sum_cmd_size(arg) + VTX_CMD_SIZE_ ## arg

#define VTX_CMD_SIZE(...) (4 __CALL_FOREACH_BIS(__sum_cmd_size, ##__VA_ARGS__))

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_InitMtx,       68
        RSPQ_DefineCommand GLCmd_InitPipe,      8
        RSPQ_DefineCommand GLCmd_SetPrimVertex, 28
        RSPQ_DefineCommand GLCmd_DrawTriangle,  8
        RSPQ_DefineCommand GLCmd_SendIndex,     4
        RSPQ_DefineCommand GLCmd_DrawBuffers,   4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4

        RSPQ_DefineCommand RSPQCmd_Noop,        4                                   # 0x10  Invalid
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(               NRM)    # 0x11  Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(          TEX     )    # 0x12  Texcoord
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(          TEX, NRM)    # 0x13  Texcoord + Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(     COL          )    # 0x14  Color
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(     COL,      NRM)    # 0x15  Color + Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(     COL, TEX     )    # 0x16  Color + Texcoord
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(     COL, TEX, NRM)    # 0x17  Color + Texcoord + Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS               )    # 0x18  Position
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS,           NRM)    # 0x19  Position + Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS,      TEX     )    # 0x1A  Position + Texcoord
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS,      TEX, NRM)    # 0x1B  Position + Texcoord + Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS, COL          )    # 0x1C  Position + Color
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS, COL,      NRM)    # 0x1D  Position + Color + Normal
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS, COL, TEX     )    # 0x1E  Position + Color + Texcoord
        RSPQ_DefineCommand GLCmd_Vtx,           VTX_CMD_SIZE(POS, COL, TEX, NRM)    # 0x1F  Position + Color + Texcoord + Normal
    RSPQ_EndOverlayHeader

    .align 4
BANNER0: .ascii " RSP OpenGL T&L "
BANNER1: .ascii "Rasky & Snacchus"

    RSPQ_BeginSavedState
    #include "rsp_gl_state.inc"

    .align 4
FINAL_MATRIX:    .dcb.w 4*4    # integer part
                 .dcb.w 4*4    # fractional part

#define PRIM_VTX_X                 0     // Object space position (16-bit)
#define PRIM_VTX_Y                 2     // Object space position (16-bit)
#define PRIM_VTX_Z                 4     // Object space position (16-bit)
#define PRIM_VTX_W                 6     // Object space position (16-bit)
#define PRIM_VTX_CS_POSi           8     // X, Y, Z, W (all 32-bit)
#define PRIM_VTX_CS_POSf           16    // X, Y, Z, W (all 32-bit)
#define PRIM_VTX_R                 24
#define PRIM_VTX_G                 26
#define PRIM_VTX_B                 28
#define PRIM_VTX_A                 30
#define PRIM_VTX_S                 32
#define PRIM_VTX_T                 34
#define PRIM_VTX_NORMAL            36    // Normal X,Y,Z (8 bit)
#define PRIM_VTX_TRCODE            39    // trivial-reject clipping flags (against -w/+w)
#define PRIM_VTX_ID                40    // 16-bit unique ID for this vertex
#define PRIM_VTX_SIZE              42

    .align 3
PRIM_VERTEX_CACHE:   .dcb.b      PRIM_VTX_SIZE * 5

#define SCREEN_VTX_X                0
#define SCREEN_VTX_Y                2
#define SCREEN_VTX_Z                4
#define SCREEN_VTX_CLIP_CODE        6
#define SCREEN_VTX_PADDING          7
#define SCREEN_VTX_RGBA             8
#define SCREEN_VTX_S               12
#define SCREEN_VTX_T               14
#define SCREEN_VTX_W               16     // FIXME: this is duplicated in CS_POS
#define SCREEN_VTX_INVW            20     // 32-bit
#define SCREEN_VTX_CS_POSi         24     // X, Y, Z, W (all 32-bit)
#define SCREEN_VTX_CS_POSf         32     // X, Y, Z, W (all 32-bit)
#define SCREEN_VTX_SIZE            40

#define SCREEN_VERTEX_CACHE_COUNT  32    // Number of vertices in the cache

    .align 3
SCREEN_VERTEX_CACHE:  .dcb.b     SCREEN_VTX_SIZE * SCREEN_VERTEX_CACHE_COUNT

    .align 4
                             #define SLOTS4(i)  ((i)*SCREEN_VTX_SIZE), (((i)+1)*SCREEN_VTX_SIZE), (((i)+2)*SCREEN_VTX_SIZE), (((i)+3)*SCREEN_VTX_SIZE)
SCREEN_VERTEX_CACHE_IDS:     .half   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
SCREEN_VERTEX_CACHE_SLOTS:   .half   SLOTS4(0), SLOTS4(4), SLOTS4(8), SLOTS4(12)  
                             .half   SLOTS4(16), SLOTS4(20), SLOTS4(24), SLOTS4(28)
                             #undef SLOTS4

PRIM_FUNC:      .half 0
PRIM_NEXT:      .half 0
PRIM_PROGRESS:  .half 0
PRIM_COUNTER:   .half 0
PRIM_INDICES:   .half 0,0,0
PRIM_SIZE:      .half 0
TRI_CMD:        .half 0
TRI_CULL:       .byte 0
    RSPQ_EndSavedState

    .align 4
CACHE_OFFSETS:               .half 2,4,6,8,10,12,14,16

PRIM_FUNCS:
    .half GL_PrimSimple         - _start    # GL_POINTS
    .half GL_PrimSimple         - _start    # GL_LINES
    .half GL_PrimLineStrip      - _start    # GL_LINE_LOOP
    .half GL_PrimLineStrip      - _start    # GL_LINE_STRIP
    .half GL_PrimSimple         - _start    # GL_TRIANGLES
    .half GL_PrimTriangleStrip  - _start    # GL_TRIANGLE_STRIP
    .half GL_PrimTriangleFan    - _start    # GL_TRIANGLE_FAN
    .half GL_PrimQuads          - _start    # GL_QUADS
    .half GL_PrimTriangleStrip  - _start    # GL_QUAD_STRIP
    .half GL_PrimTriangleFan    - _start    # GL_POLYGON

    .align 3
ATTR_FETCH_CACHE:
    ATTR_FETCH_POS:         .ds.l 4
    ATTR_FETCH_COLOR:       .ds.l 4
    ATTR_FETCH_TEXCOORD:    .ds.l 4
    ATTR_FETCH_NORMAL:      .ds.l 4

    .text

    .func GLCmd_InitMtx
GLCmd_InitMtx:
    la s0, CMD_ADDR(4, 68)
    li s4, %lo(FINAL_MATRIX)
    li t0, 15
1:
    lw t1, 0(s0)
    sw t1, 0(s4)
    add s0, 4
    add s4, 4
    bnez t0, 1b
    addi t0, -1

    jr ra
    nop
    .endfunc

    .func GLCmd_InitPipe
GLCmd_InitPipe:
    move s0, a0
    li s4, %lo(GL_STATE)
    jal DMAIn
    li t0, DMA_SIZE(GL_STATE_SIZE, 1)

    # Clear screen vertex cache
    li s0, %lo(SCREEN_VERTEX_CACHE_IDS)
    sqv vzero, 0x00,s0
    sqv vzero, 0x10,s0
    sqv vzero, 0x20,s0
    sqv vzero, 0x30,s0

    lhu t0, %lo(GL_STATE_PRIM_TYPE)
    sh zero, %lo(PRIM_PROGRESS)
    sh zero, %lo(PRIM_COUNTER)

    sll t0, 1
    lhu t0, %lo(PRIM_FUNCS)(t0)
    sh t0, %lo(PRIM_FUNC)

    srl t0, a1, 16
    andi t1, a1, 0xFFFF
    sh t0, %lo(PRIM_SIZE)
    sh t1, %lo(PRIM_NEXT)

    #define state_flags t0
    #define has_depth   t1
    #define has_tex     t3
    #define tri_cmd     t4

    # TODO: Optimize this by rearranging GL state flags
    lw state_flags, %lo(GL_STATE_FLAGS)
    and has_depth, state_flags, FLAG_DEPTH_TEST
    and has_tex, state_flags, FLAG_TEXTURE_ACTIVE
    sll has_depth, TRICMD_ATTR_SHIFT_Z
    srl has_tex, TRICMD_ATTR_SHIFT_TEX

    or tri_cmd, has_depth, has_tex
    ori tri_cmd, 0xCC00
    sh tri_cmd, %lo(TRI_CMD)

    #undef has_depth
    #undef has_tex
    #undef tri_cmd

#if RSP_PRIM_ASSEMBLY

    #define mtx_dirty   t1
    #define mtx_lhs     s0
    #define mtx_rhs     s1
    #define mtx_dst     s2

    and mtx_dirty, state_flags, FLAG_FINAL_MTX_DIRTY
    beqz mtx_dirty, glpipe_mtx_not_dirty
    li mtx_lhs, %lo(GL_MATRIX_PROJECTION)
    li mtx_rhs, %lo(GL_MATRIX_MODELVIEW)
    jal GL_MtxMult
    li mtx_dst, %lo(FINAL_MATRIX)
    and state_flags, ~FLAG_FINAL_MTX_DIRTY
    sw state_flags, %lo(GL_STATE_FLAGS)

    #undef mtx_dirty
    #undef mtx_lhs
    #undef mtx_rhs
    #undef mtx_dst

glpipe_mtx_not_dirty:
#endif

    #define cull_mode       t1
    #define front_face      t2
    #define cull_enabled    t3
    #define tri_cull        t4

    andi cull_enabled, state_flags, FLAG_CULL_FACE
    beqz cull_enabled, glpipe_init_write_cull
    # Any non-negative value other than 0 or 1 signifies that no faces should be culled
    li tri_cull, 2

    lhu cull_mode, %lo(GL_STATE_CULL_MODE)
    lhu front_face, %lo(GL_STATE_FRONT_FACE)

    # Set TRI_CULL to a negative number to cull all faces
    beq cull_mode, GL_FRONT_AND_BACK, glpipe_init_write_cull
    li tri_cull, -1

    # tri_cull = (cull_mode == GL_BACK) ^ (front_face == GL_CW)
    xori cull_mode, GL_FRONT
    sltu tri_cull, zero, cull_mode
    xori front_face, GL_CCW
    sltu front_face, zero, front_face
    xor tri_cull, front_face

glpipe_init_write_cull:
    j RSPQ_Loop
    sb tri_cull, %lo(TRI_CULL)

    #undef cull_mode
    #undef front_face
    #undef cull_enabled
    #undef tri_cull
    #undef state_flags
    .endfunc

    ####################################################
    # Find the screen cache slot for a vertex given its ID.
    #
    # If the vertex is already present in the cache, the
    # cache slot is returned. Otherwise, the least recent
    # used vertex is eviceted and that cache slot is returned.
    #
    # Args:
    #   t0: ID of the vertex
    # Returns:
    #   t1: Slot (offset within SCREEN_VERTEX_CACHE)
    #   t2: 0 if not found, 1 if found
    ####################################################
    .func VertexCacheLookup
VertexCacheLookup:
    #define v___    $v01
    #define vsearch $v02   // ID to search

    #define vids0 $v03     // IDs in the cache
    #define vids1 $v04
    #define vids2 $v05
    #define vids3 $v06

    #define voffs0 $v16    // Cache offsets (values 1-32)
    #define voffs1 $v17
    #define voffs2 $v18
    #define voffs3 $v19

    assert_ne t0, 0, ASSERT_INVALID_VTX_ID
    mtc2 t0, $v02.e0

    li s2, %lo(CACHE_OFFSETS)
    li s1, %lo(SCREEN_VERTEX_CACHE_IDS)

    lqv voffs0, 0,s2
    vadd voffs3, voffs0, K16
    vadd voffs1, voffs0, K16
    vadd voffs2, voffs0, K32
    vadd voffs3, voffs3, K32

    lqv vids0,  0*2,s1
    lqv vids1,  8*2,s1
    lqv vids2, 16*2,s1
    lqv vids3, 24*2,s1

    veq v___  vids0, vsearch.e0
    vmrg voffs0, voffs0, vzero
    veq v___  vids1, vsearch.e0
    vmrg voffs1, voffs1, vzero
    veq v___  vids2, vsearch.e0
    vmrg voffs2, voffs2, vzero
    veq v___  vids3, vsearch.e0
    vmrg voffs3, voffs3, vzero

    vaddc voffs0, voffs0.q1
    vaddc voffs0, voffs0.h2
    vaddc voffs0, voffs0.e4
    mfc2 t1, voffs0.e0
    addi t1, -2
    bgez t1, cache_hit
    li t2, 1
cache_miss:
    # No match found in the cache, remove oldest entry
    li t1, (SCREEN_VERTEX_CACHE_COUNT*2)-2
    li t2, 0
    sh t0, %lo(SCREEN_VERTEX_CACHE_IDS)(t1)
cache_hit:
    add s0, s1, t1
#ifndef NDEBUG
    lhu t3, (SCREEN_VERTEX_CACHE_COUNT*2*0)(s0)   # Check that we found the correct ID
    assert_eq t0, t3, 0x1234
#endif
    beqz t1, JrRa
    lhu t1, (SCREEN_VERTEX_CACHE_COUNT*2*1)(s0)   # Read slot for ID

move_loop:
    addi s0, -2
    lhu t3, (SCREEN_VERTEX_CACHE_COUNT*2*0) + 0(s0)
    lhu t4, (SCREEN_VERTEX_CACHE_COUNT*2*1) + 0(s0)
    sh  t3, (SCREEN_VERTEX_CACHE_COUNT*2*0) + 2(s0)
    bne s0, s1, move_loop
    sh  t4, (SCREEN_VERTEX_CACHE_COUNT*2*1) + 2(s0)

    # Store ID/Slot at the top of the cache
    sh t0, (SCREEN_VERTEX_CACHE_COUNT*2*0)(s0)
    jr ra
    sh t1, (SCREEN_VERTEX_CACHE_COUNT*2*1)(s0)
    .endfunc

GLCmd_Vtx:
    #define prim_index      s1
    #define prim_func       s2
    #define index           s3
    #define cmd_ptr         s4
    #define cur_attr        s5
    #define vtx_cmd         t5
    #define prim_size       t6
    #define prim_progress   t7

    #define vposition       $v01
    #define vcolor          $v02
    #define vtexcoord       $v03
    #define vnormal         $v04

    #define prim_vtx1       s5
    #define prim_vtx2       s6
    #define prim_vtx3       s7

    lb v0, %lo(TRI_CULL)
    lhu prim_index, %lo(PRIM_NEXT)

    # If TRI_CULL is negative, we're culling all faces.
    # So just quit immediately.
    bltz v0, JrRa
    move ra2, ra

    andi index, a0, 0xFFFF
    srl vtx_cmd, a0, 24

    addi index, 1
    addi a0, prim_index, %lo(PRIM_VERTEX_CACHE)

    addi cmd_ptr, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) + 4
    sub cmd_ptr, rspq_cmd_size

    # TODO: Add fetching from VBOs
    # TODO: Maybe split this into separate commands and inline the branches?

    
    andi t0, vtx_cmd, VTX_CMD_FLAG_POSITION
    beqz t0, gl_vtx_no_position
    li cur_attr, %lo(GL_CUR_COLOR)

    ldv vposition,  0,cmd_ptr
    addi cmd_ptr, VTX_CMD_SIZE_POS

gl_vtx_no_position:
    andi t0, vtx_cmd, VTX_CMD_FLAG_COLOR
    beqz t0, gl_vtx_no_color
    ldv vcolor,  0,cmd_ptr

    addi cmd_ptr, VTX_CMD_SIZE_COL
    sdv vcolor,  0,cur_attr

gl_vtx_no_color:
    andi t0, vtx_cmd, VTX_CMD_FLAG_TEXCOORD
    beqz t0, gl_vtx_no_texcoord
    ldv vtexcoord,  0,cmd_ptr

    addi cmd_ptr, VTX_CMD_SIZE_TEX
    sdv vtexcoord,  8,cur_attr

gl_vtx_no_texcoord:
    andi t0, vtx_cmd, VTX_CMD_FLAG_NORMAL
    beqz t0, gl_vtx_no_normal
    llv vnormal,  0,cmd_ptr

    slv vnormal,  16,cur_attr

gl_vtx_no_normal:

    ldv vcolor,     0 ,cur_attr
    ldv vtexcoord,  8 ,cur_attr
    llv vnormal,    16,cur_attr

    sdv vposition,  PRIM_VTX_X     ,a0
    sdv vcolor,     PRIM_VTX_R     ,a0
    slv vtexcoord,  PRIM_VTX_S     ,a0  # TODO: add R and Q
    slv vnormal,    PRIM_VTX_NORMAL,a0
    jal GL_PreCull
    sh index, PRIM_VTX_ID(a0)

    lhu prim_size, %lo(PRIM_SIZE)
    lhu prim_progress, %lo(PRIM_PROGRESS)

    blt prim_index, PRIM_VTX_SIZE*3, gl_write_prim_next
    addi t0, prim_index, PRIM_VTX_SIZE
    move t0, zero
gl_write_prim_next:
    sh t0, %lo(PRIM_NEXT)

    move ra, ra2
    
    sh prim_index, %lo(PRIM_INDICES)(prim_progress)
    addi prim_progress, 2
    blt prim_progress, prim_size, JrRa
    sh prim_progress, %lo(PRIM_PROGRESS)

    lhu prim_func, %lo(PRIM_FUNC)

    lhu prim_vtx1, %lo(PRIM_INDICES) + 0x0
    lhu prim_vtx2, %lo(PRIM_INDICES) + 0x2
    jr prim_func
    lhu prim_vtx3, %lo(PRIM_INDICES) + 0x4

gl_prim_func_return:
    sh prim_progress, %lo(PRIM_PROGRESS)
    # TODO: points / lines
    j GL_DrawTriangle
    lhu a0, %lo(TRI_CMD)

GL_PrimSimple:
    j gl_prim_func_return
    move prim_progress, zero

GL_PrimLineStrip:
    lhu t0, %lo(PRIM_INDICES) + 0x2
    li prim_progress, 2
    j gl_prim_func_return
    sh t0, %lo(PRIM_INDICES) + 0x0

GL_PrimTriangleStrip:
    lhu t0, %lo(PRIM_INDICES) + 0x4
    lhu t1, %lo(PRIM_COUNTER)
    li prim_progress, 4
    xori t2, t1, 2
    sh t0, %lo(PRIM_INDICES)(t1)
    j gl_prim_func_return
    sh t2, %lo(PRIM_COUNTER)

GL_PrimTriangleFan:
    lhu t0, %lo(PRIM_INDICES) + 0x4
    li prim_progress, 4
    j gl_prim_func_return
    sh t0, %lo(PRIM_INDICES) + 0x2

GL_PrimQuads:
    lhu t0, %lo(PRIM_INDICES) + 0x4
    lhu t1, %lo(PRIM_COUNTER)
    sh t0, %lo(PRIM_INDICES) + 0x2
    xori t1, 2
    sll prim_progress, t1, 1
    j gl_prim_func_return
    sh t1, %lo(PRIM_COUNTER)

    ########################################
    # GLCmd_SetPrimVertex
    #
    # Arguments:
    # * 0x00 (a0): offset within PRIM_VERTEX_CACHE + Vertex ID
    # * 0x04 (a1): object space X, Y (16-bit)
    # * 0x08 (a2): object space Z, W (16-bit)
    # * 0x0C (a3): RGBA (8-bit each one)
    # * 0x10:      S, T (16-bit)
    # * 0x14:      normal X, Y, Z (8-bit each one) (LSB must be 0)
    #
    ########################################

    .func GLCmd_SetPrimVertex
GLCmd_SetPrimVertex:
    #define prim_vtx   a0
    #define in_xy      a1
    #define in_zw      a2
    #define in_rg      a3
    #define vtx_id     v1

    srl vtx_id, prim_vtx, 8
    andi prim_vtx, 0xFF
    addi prim_vtx, %lo(PRIM_VERTEX_CACHE)

    lw t0, CMD_ADDR(16, 28)  # B,A
    lw t1, CMD_ADDR(20, 28)  # S,T
    lw t2, CMD_ADDR(24, 28)  # N

    sw in_xy,    PRIM_VTX_X     (prim_vtx)
    sw in_zw,    PRIM_VTX_Z     (prim_vtx)
    sw in_rg,    PRIM_VTX_R     (prim_vtx)
    sw t0,       PRIM_VTX_B     (prim_vtx)
    sw t1,       PRIM_VTX_S     (prim_vtx)
    sw t2,       PRIM_VTX_NORMAL(prim_vtx)
    sh vtx_id,   PRIM_VTX_ID    (prim_vtx)

GL_PreCull:
    #define v___      $v01

    #define vmtx0_i   $v16       //  m00 m01 m02 m03
    #define vmtx0_f   $v17
    #define vmtx1_i   $v18       //  m00 m01 m02 m03
    #define vmtx1_f   $v19
    #define vmtx2_i   $v20       //  m00 m01 m02 m03
    #define vmtx2_f   $v21
    #define vmtx3_i   $v22       //  m00 m01 m02 m03
    #define vmtx3_f   $v23

    #define vpos       $v24
    #define vcspos_i   $v25
    #define vcspos_f   $v26

    ldv vpos.e0, PRIM_VTX_X     ,prim_vtx

    #define x  e0
    #define y  e1
    #define z  e2
    #define w  e3

    li s0, %lo(FINAL_MATRIX)
    ldv vmtx0_i.e0,  0x00,s0
    ldv vmtx1_i.e0,  0x08,s0
    ldv vmtx2_i.e0,  0x10,s0
    ldv vmtx3_i.e0,  0x18,s0
    ldv vmtx0_f.e0,  0x20,s0
    ldv vmtx1_f.e0,  0x28,s0
    ldv vmtx2_f.e0,  0x30,s0
    ldv vmtx3_f.e0,  0x38,s0

    vmudn v___,     vmtx0_f, vpos.x
    vmadh v___,     vmtx0_i, vpos.x
    vmadn v___,     vmtx1_f, vpos.y
    vmadh v___,     vmtx1_i, vpos.y
    vmadn v___,     vmtx2_f, vpos.z
    vmadh v___,     vmtx2_i, vpos.z
    vmadn vcspos_f, vmtx3_f, vpos.w
    vmadh vcspos_i  vmtx3_i, vpos.w
    vmadn vcspos_f, vzero, vzero

    # 32-bit right shift by 5, to keep the clip space coordinates unscaled
    vmudm vcspos_i, vcspos_i, vshift8.e4
    vmadl vcspos_f, vcspos_f, vshift8.e4

    sdv vcspos_i, PRIM_VTX_CS_POSi,prim_vtx
    sdv vcspos_f, PRIM_VTX_CS_POSf,prim_vtx

    # Calculate and store clipping flags against CS.W. These
    # will be used for trivial rejections.
    vch v___, vcspos_i, vcspos_i.w
    vcl v___, vcspos_f, vcspos_f.w
    cfc2 t0, COP2_CTRL_VCC
    andi t0, 0x707   # Isolate X/Y/Z flags
    sb t0, PRIM_VTX_TRCODE(prim_vtx)

    # FIXME: in immediate mode, we should also cache the per-vertex
    # material, in case it is changed within a glBegin / glEnd pair.

    jr ra
    nop

    #undef pos_x
    #undef pos_y
    #undef pos_z
    #undef pos_w

    #undef prim_vtx
    #undef in_xy   
    #undef in_zw   
    #undef in_rgba 
    #undef vtx_id

    #undef v___     

    #undef vmtx0_i   
    #undef vmtx0_f  
    #undef vmtx1_i   
    #undef vmtx1_f  
    #undef vmtx2_i   
    #undef vmtx2_f  
    #undef vmtx3_i   
    #undef vmtx3_f  

    #undef vpos      
    #undef vcspos_i  
    #undef vcspos_f  

    .endfunc


    ################################################################
    # GL_TnL
    #
    # Args:
    #   s3 = address of the prim vertex in DMEM (usually within PRIM_VERTEX_CACHE)
    #
    # Returns:
    #   s3 = address of the screen vertex in DMEM (within SCREEN_VERTEX_CACHE)
    ################################################################
    .func GL_TnL
GL_TnL:
    #define prim_vtx     s3
    #define screen_vtx   s4
    move ra2, ra

    # Read the ID of the vertex and lookup into the cache.
    jal VertexCacheLookup
    lhu t0, PRIM_VTX_ID(prim_vtx)
    # If the vertex was found in cache, we got nothing to do
    move ra, ra2
    bnez t2, JrRa
    # We need to do TnL.
    # Compute the address in cache where to add the vertex.
    addi screen_vtx, t1, %lo(SCREEN_VERTEX_CACHE)

    #define v___         $v01
    #define vcspos_f     $v02
    #define vcspos_i     $v03
    #define vrgba        $v04
    #define vst          $v05
    #define vguard_f     $v06
    #define vguard_i     $v07
    #define vinvw_f      $v08
    #define vinvw_i      $v09
    #define vviewscale   $v10
    #define vviewoff     $v11
    #define vscreenpos_i $v12
    #define vscreenpos_f $v13
    #define vtexsize     $v14
    #define s            e0
    #define t            e1
    #define z            e2
    #define w            e3
    #define KGUARD       __PPCAT(K, GUARD_BAND_FACTOR)

    ldv vcspos_f, PRIM_VTX_CS_POSf,prim_vtx
    ldv vcspos_i, PRIM_VTX_CS_POSi,prim_vtx
    ldv vrgba,    PRIM_VTX_R,prim_vtx         # R + G + B + A
    llv vst,      PRIM_VTX_S,prim_vtx         # S + T

    sdv vcspos_f, SCREEN_VTX_CS_POSf,screen_vtx
    sdv vcspos_i, SCREEN_VTX_CS_POSi,screen_vtx
    suv vrgba,    SCREEN_VTX_RGBA,screen_vtx

    vmudn vguard_f, vcspos_f, KGUARD
    vmadh vguard_i, vcspos_i, KGUARD
    
    vcl v___, vcspos_f, vguard_f.w
    vch v___, vcspos_i, vguard_i.w
    cfc2 t0, COP2_CTRL_VCC
    lbu t1, PRIM_VTX_TRCODE(prim_vtx) 
    andi t1, 0x404   # Z flag
    andi t0, 0x303   # X/Y flags
    or t0, t1

    # Calculate 32-bit inverse W
    # TODO: NR?
    vrcph vinvw_i.w, vcspos_i.w
    vrcpl vinvw_f.w, vcspos_f.w
    vrcph vinvw_i.w, vzero.e0

    # Calculate screenspace coords
    li s0, %lo(GL_VIEWPORT_SCALE)
    ldv vviewscale, 0,s0
    ldv vviewoff,   8,s0

    vmudl v___,         vcspos_f, vinvw_f.w
    vmadm v___,         vcspos_i, vinvw_f.w
    vmadn vscreenpos_f, vcspos_f, vinvw_i.w
    vmadh vscreenpos_i, vcspos_i, vinvw_i.w

    vmudn vscreenpos_f, vscreenpos_f, vviewscale
    vmadh vscreenpos_i, vscreenpos_i, vviewscale
    vadd vscreenpos_i, vviewoff

    # Scale texcoord by texsize
    li s0, %lo(GL_STATE_TEX_SIZE)
    llv vtexsize.s, 0,s0
    vmudh vst, vtexsize
    
    sdv vscreenpos_i, SCREEN_VTX_X        ,screen_vtx
    ssv vcspos_i.w,   SCREEN_VTX_W+0      ,screen_vtx 
    ssv vcspos_f.w,   SCREEN_VTX_W+2      ,screen_vtx 
    ssv vinvw_i.w,    SCREEN_VTX_INVW+0   ,screen_vtx
    ssv vinvw_f.w,    SCREEN_VTX_INVW+2   ,screen_vtx
    sdv vcspos_i,     SCREEN_VTX_CS_POSi  ,screen_vtx 
    sdv vcspos_f,     SCREEN_VTX_CS_POSf  ,screen_vtx 
    slv vst.s         SCREEN_VTX_S        ,screen_vtx
    sb t0,            SCREEN_VTX_CLIP_CODE(screen_vtx)

    jr ra
    nop

    #undef v___        
    #undef vcspos_f    
    #undef vcspos_i    
    #undef vrgbast     
    #undef vguard_f    
    #undef vguard_i    
    #undef vinvw_f     
    #undef vinvw_i     
    #undef vviewscale  
    #undef vviewoff    
    #undef vscreenpos_i
    #undef vscreenpos_f
    #undef z         
    #undef w         
    #undef KGUARD

    .endfunc


    ################################################################
    # GLCmd_DrawTriangle
    #
    ################################################################

    .func GLCmd_DrawTriangle
GLCmd_DrawTriangle:
    #define tri_cmd     a0
    #define prim_verts  a1

    #define prim_vtx1   s5
    #define prim_vtx2   s6
    #define prim_vtx3   s7
    #define ra3         s8

    andi prim_vtx3, prim_verts, 0xFF
    srl  prim_vtx2, prim_verts, 8
    and  prim_vtx2, 0xFF
    srl  prim_vtx1, prim_verts, 16
    and  prim_vtx1, 0xFF
    lb v0, %lo(TRI_CULL)

GL_DrawTriangle:
    addi prim_vtx3, %lo(PRIM_VERTEX_CACHE)
    addi prim_vtx2, %lo(PRIM_VERTEX_CACHE)
    addi prim_vtx1, %lo(PRIM_VERTEX_CACHE)

    # Trivial reject: if all the vertices are out of the same plane (at least one),
    # the triangle is out of the viewport.
    lbu t0, PRIM_VTX_TRCODE(prim_vtx1)
    lbu t1, PRIM_VTX_TRCODE(prim_vtx2)
    lbu t2, PRIM_VTX_TRCODE(prim_vtx3)
    and t0, t1
    and t0, t2
    bnez t0, JrRa
    move ra3, ra

    jal GL_TnL
    move s3, prim_vtx1
    addi a1, s4, SCREEN_VTX_X

    jal GL_TnL
    move s3, prim_vtx2
    addi a2, s4, SCREEN_VTX_X

    jal GL_TnL
    move s3, prim_vtx3
    addi a3, s4, SCREEN_VTX_X

    jal RDPQ_Triangle
    li s3, %lo(RDPQ_CMD_STAGING)

    move ra, ra3
    j RDPQ_Send
    li s4, %lo(RDPQ_CMD_STAGING)

    #undef prim_vtx1
    #undef prim_vtx2
    #undef prim_vtx3

    .endfunc

GL_DrawLine:
    # TODO
    jr ra
    nop

GL_DrawPoint:
    # TODO
    jr ra
    nop

GLCmd_SendIndex:
    jr ra
    nop

GLCmd_DrawBuffers:
    jr ra
    nop

#include "rsp_gl_common.inc"
#include <rsp_rdpq.inc>
