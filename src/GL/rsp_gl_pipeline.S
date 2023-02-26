#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_InitPipe,      4
        RSPQ_DefineCommand GLCmd_DrawTriangle,  4
        RSPQ_DefineCommand GLCmd_SetPrimVertex, 32
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
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
        RSPQ_DefineCommand RSPQCmd_Noop,        4
    RSPQ_EndOverlayHeader

    .align 4
BANNER0: .ascii " RSP OpenGL T&L "
BANNER1: .ascii "Rasky & Snacchus"

    RSPQ_BeginSavedState
    #include "rsp_gl_state.inc"

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
#define PRIM_VTX_TEX_S             32
#define PRIM_VTX_TEX_T             34
#define PRIM_VTX_TEX_R             36
#define PRIM_VTX_TEX_Q             38
#define PRIM_VTX_NORMAL            40    // Normal X,Y,Z (8 bit)
#define PRIM_VTX_TRCODE            43    // trivial-reject clipping flags (against -w/+w)
#define PRIM_VTX_ID                44    // 16-bit unique ID for this vertex
#define PRIM_VTX_SIZE              46

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
    RSPQ_EndSavedState

    .align 4
CACHE_OFFSETS:               .half 2,4,6,8,10,12,14,16,18

CLIP_CODE_FACTORS: .half 1, 1, GUARD_BAND_FACTOR, GUARD_BAND_FACTOR

    .text

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
    j RSPQ_Loop
    sqv vzero, 0x30,s0

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

    lw t0, CMD_ADDR(16, 32)  # B,A
    lw t1, CMD_ADDR(20, 32)  # S,T
    lw t2, CMD_ADDR(24, 32)  # R,Q
    lw t3, CMD_ADDR(28, 32)  # N

    sw in_xy,    PRIM_VTX_X     (prim_vtx)
    sw in_zw,    PRIM_VTX_Z     (prim_vtx)
    sw in_rg,    PRIM_VTX_R     (prim_vtx)
    sw t0,       PRIM_VTX_B     (prim_vtx)
    sw t1,       PRIM_VTX_TEX_S (prim_vtx)
    sw t2,       PRIM_VTX_TEX_R (prim_vtx)
    sw t3,       PRIM_VTX_NORMAL(prim_vtx)
    sh vtx_id,   PRIM_VTX_ID    (prim_vtx)

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

    li s0, %lo(GL_MATRIX_FINAL)
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
    # Compress flags to 8 bit
    srl t1, t0, 5
    andi t0, 0x7
    or t0, t1
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

    #undef x
    #undef y
    #undef z
    #undef w

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
    # GL_CalcScreenSpace
    #
    # Args:
    #   s4   = Destination vertex address
    #   $v02 = Clip space position (fractional part)
    #   $v03 = Clip space position (integer part)
    #
    ################################################################
    .func GL_CalcScreenSpace
GL_CalcScreenSpace:
    #define dst          s4
    #define vcspos_f     $v02
    #define vcspos_i     $v03
    #define vinvw_f      $v23
    #define vinvw_i      $v24
    #define vviewscale   $v25
    #define vviewoff     $v26
    #define vscreenpos_i $v27
    #define vscreenpos_f $v28
    #define v___         $v29
    #define w            e3

    # Calculate 32-bit inverse W
    # TODO: NR?
    vrcph vinvw_i.w, vcspos_i.w
    vrcpl vinvw_f.w, vcspos_f.w
    vrcph vinvw_i.w, vzero.e0

    # Calculate screenspace coords
    li t0, %lo(GL_VIEWPORT_SCALE)
    ldv vviewscale, 0,t0
    ldv vviewoff,   8,t0

    vmudl v___,         vcspos_f, vinvw_f.w
    vmadm v___,         vcspos_i, vinvw_f.w
    vmadn vscreenpos_f, vcspos_f, vinvw_i.w
    vmadh vscreenpos_i, vcspos_i, vinvw_i.w

    vmudn vscreenpos_f, vscreenpos_f, vviewscale
    vmadh vscreenpos_i, vscreenpos_i, vviewscale
    vadd vscreenpos_i, vviewoff

    sdv vscreenpos_i, SCREEN_VTX_X     ,dst
    ssv vcspos_i.w,   SCREEN_VTX_W+0   ,dst 
    ssv vcspos_f.w,   SCREEN_VTX_W+2   ,dst 
    ssv vinvw_i.w,    SCREEN_VTX_INVW+0,dst
    ssv vinvw_f.w,    SCREEN_VTX_INVW+2,dst
    jr ra
    sb zero,          SCREEN_VTX_PADDING(dst)

    #undef dst
    #undef vcspos_f
    #undef vcspos_i
    #undef vinvw_f
    #undef vinvw_i
    #undef vviewscale
    #undef vviewoff
    #undef vscreenpos_i
    #undef vscreenpos_f
    #undef v___
    #undef w

    .endfunc

    ################################################################
    # GL_CalcClipCodes
    #
    # Args:
    #   s4   = Destination vertex address
    #   $v02 = Clip space position (fractional part)
    #   $v03 = Clip space position (integer part)
    #
    ################################################################
    .func GL_CalcClipCodes
GL_CalcClipCodes:
    #define dst          s4
    #define vcspos_f     $v02
    #define vcspos_i     $v03
    #define vguard_f     $v27
    #define vguard_i     $v28
    #define v___         $v29
    #define w            e3

    li t0, %lo(CLIP_CODE_FACTORS)
    ldv vguard_i,  0,t0

    vmudn vguard_f, vcspos_f, vguard_i
    vmadh vguard_i, vcspos_i, vguard_i
    
    vch v___, vguard_i, vguard_i.w
    vcl v___, vguard_f, vguard_f.w
    cfc2 t0, COP2_CTRL_VCC
    andi t0, 0x707
    srl t1, t0, 5
    andi t0, 0x7
    or t0, t1
    jr ra
    sb t0,  SCREEN_VTX_CLIP_CODE(dst)

    #undef dst
    #undef vcspos_i
    #undef vcspos_f
    #undef vguard_i
    #undef vguard_f
    #undef v___
    #undef w

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
    #define tmp_ptr      s2
    #define prim_vtx     s3
    #define screen_vtx   s4
    #define state_flags  t5
    #define s            e0
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

    lw state_flags, %lo(GL_STATE_FLAGS)

    #define v___         $v01
    #define veyepos      $v02
    #define veyenormal   $v03
    #define vrgba        $v04
    #define vobjpos      $v29

    ldv vrgba.e0, PRIM_VTX_R, prim_vtx     # R + G + B + A
    ldv vrgba.e4, PRIM_VTX_R, prim_vtx     # R + G + B + A

    #define vmtx0_i      $v16       //  m00 m01 m02 m03
    #define vmtx0_f      $v17
    #define vmtx1_i      $v18       //  m00 m01 m02 m03
    #define vmtx1_f      $v19
    #define vmtx2_i      $v20       //  m00 m01 m02 m03
    #define vmtx2_f      $v21
    #define vmtx3_i      $v22       //  m00 m01 m02 m03
    #define vmtx3_f      $v23

    and t0, state_flags, FLAG_NEED_EYE_SPACE
    beqz t0, 2f
    li tmp_ptr, %lo(GL_MATRIX_MODELVIEW)

    addi s5, prim_vtx, PRIM_VTX_NORMAL-4
    lpv vobjpos.e0,  0,s5                   # loads NX + NY + NZ into lanes 4-7
    ldv vobjpos.e0,  PRIM_VTX_X,prim_vtx    # loads X + Y + Z + W into lanes 0-3

    # TODO: factor out this style of matrix loading?
    # TODO: technically we need the inverse transpose matrix,
    #       but for rigid matrices (translation, rotation, uniform scale) this is fine
    ldv vmtx0_i.e0,  0x00,tmp_ptr
    ldv vmtx1_i.e0,  0x08,tmp_ptr
    ldv vmtx2_i.e0,  0x10,tmp_ptr
    ldv vmtx3_i.e0,  0x18,tmp_ptr
    ldv vmtx0_f.e0,  0x20,tmp_ptr
    ldv vmtx1_f.e0,  0x28,tmp_ptr
    ldv vmtx2_f.e0,  0x30,tmp_ptr
    ldv vmtx3_f.e0,  0x38,tmp_ptr
    ldv vmtx0_i.e4,  0x00,tmp_ptr
    ldv vmtx1_i.e4,  0x08,tmp_ptr
    ldv vmtx2_i.e4,  0x10,tmp_ptr
    ldv vmtx3_i.e4,  0x18,tmp_ptr
    ldv vmtx0_f.e4,  0x20,tmp_ptr
    ldv vmtx1_f.e4,  0x28,tmp_ptr
    ldv vmtx2_f.e4,  0x30,tmp_ptr
    ldv vmtx3_f.e4,  0x38,tmp_ptr

    vmov vobjpos.e7, vzero.e0
    # Transform vertex pos and normal into eye space
    vmudn v___,    vmtx0_f, vobjpos.h0
    vmadh v___,    vmtx0_i, vobjpos.h0
    vmadn v___,    vmtx1_f, vobjpos.h1
    vmadh v___,    vmtx1_i, vobjpos.h1
    vmadn v___,    vmtx2_f, vobjpos.h2
    vmadh v___,    vmtx2_i, vobjpos.h2
    vmadn v___,    vmtx3_f, vobjpos.h3
    vmadh veyepos, vmtx3_i, vobjpos.h3

    # TODO: normalize normal if GL_NORMALIZE is enabled

    li tmp_ptr, %lo(RDPQ_CMD_STAGING)
    sqv veyepos,        0,tmp_ptr
    ldv veyenormal.e0,  8,tmp_ptr
    ldv veyenormal.e4,  8,tmp_ptr

    #define vsqdist_f   $v10
    #define vsqdist_i   $v11
    #define vtmp_f      $v12
    #define vtmp_i      $v13
    #define vinvdist_f  $v14
    #define vinvdist_i  $v15

    # Re-normalize transformed normal

    andi t0, state_flags, FLAG_NORMALIZE
    beqz t0, 1f
    vmudh v___, veyenormal, veyenormal
    vsar  vsqdist_f, COP2_ACC_MD
    vsar  vsqdist_i, COP2_ACC_HI
    vaddc vtmp_f, vsqdist_f, vsqdist_f.h1
    vadd  vtmp_i, vsqdist_i, vsqdist_i.h1
    vaddc vsqdist_f, vtmp_f, vsqdist_f.h2
    vadd  vsqdist_i, vtmp_i, vsqdist_i.h2

    vrsqh v___.e0,       vsqdist_i.e0
    vrsql vinvdist_f.e0, vsqdist_f.e0
    vrsqh vinvdist_i.e0, vzero.e0

    vmudm v___,       veyenormal, vinvdist_f.e0
    vmadh veyenormal, veyenormal, vinvdist_i.e0

    #undef vsqdist_f
    #undef vsqdist_i
    #undef vtmp_f
    #undef vtmp_i
    #undef vinvdist_f
    #undef vinvdist_i

1:
    ldv veyepos.e4,     0,tmp_ptr

2:

    and t0, state_flags, FLAG_LIGHTING
    bnez t0, GL_VtxLighting
    li ra, %lo(1f)
1:

    suv vrgba,    SCREEN_VTX_RGBA,screen_vtx

    #define vtexsize     $v06
    #define vtexoffset   $v07
    #define vstrq        $v08

    li s1, %lo(GL_STATE_TEX_SIZE)
    llv vtexsize.s,   0,s1
    llv vtexoffset.s, 4,s1

    ldv vstrq,      PRIM_VTX_TEX_S,prim_vtx     # S + T + R + Q

    #define vplanes_s_i $v09
    #define vplanes_s_f $v10
    #define vplanes_t_i $v11
    #define vplanes_t_f $v12
    #define vplanes_r_i $v13
    #define vplanes_r_f $v14
    #define vplanes_q_i $v15
    #define vplanes_q_f $v16
    #define vobjlin     $v17
    #define veyelin     $v18
    #define vmodes      $v19

    srl t0, state_flags, TEX_GEN_S_SHIFT
    andi t0, 0xF
    beqz t0, 1f
    ldv vobjpos.e4,  0,tmp_ptr

    # Fill VCC with enable-flags
    # We need to OR with 0xF0 so that the mode constants stay in the vmodes register
    ori t0, 0x70
    ctc2 t0, COP2_CTRL_VCC

    li s1, %lo(GL_TEX_GEN)

    lqv vmodes,       TEX_GEN_MODE_OFFSET,         s1
    lqv vplanes_s_i,  TEX_GEN_INTEGER_OFFSET+0x00, s1
    lqv vplanes_s_f,  TEX_GEN_FRACTION_OFFSET+0x00,s1
    lqv vplanes_t_i,  TEX_GEN_INTEGER_OFFSET+0x10, s1
    lqv vplanes_t_f,  TEX_GEN_FRACTION_OFFSET+0x10,s1
    lqv vplanes_r_i,  TEX_GEN_INTEGER_OFFSET+0x20, s1
    lqv vplanes_r_f,  TEX_GEN_FRACTION_OFFSET+0x20,s1
    lqv vplanes_q_i,  TEX_GEN_INTEGER_OFFSET+0x30, s1
    lqv vplanes_q_f,  TEX_GEN_FRACTION_OFFSET+0x30,s1

    # Select which coords to enable based on the enable-flags that were loaded into VCC above
    vmrg vmodes, vmodes, vzero

    vmudn v___,    vplanes_s_f, vobjpos.h0
    vmadh v___,    vplanes_s_i, vobjpos.h0
    vmadn v___,    vplanes_t_f, vobjpos.h1
    vmadh v___,    vplanes_t_i, vobjpos.h1
    vmadn v___,    vplanes_r_f, vobjpos.h2
    vmadh v___,    vplanes_r_i, vobjpos.h2
    vmadn v___,    vplanes_q_f, vobjpos.h3
    vmadh vobjlin, vplanes_q_i, vobjpos.h3

    sdv vobjlin.e4, 0,tmp_ptr
    ldv veyelin.e0, 0,tmp_ptr

    veq  v___,    vmodes,  vmodes.e4
    vmrg vstrq, vobjlin, vstrq
    veq  v___,    vmodes,  vmodes.e5
    vmrg vstrq, veyelin, vstrq

    #undef vplanes_s_i
    #undef vplanes_s_f
    #undef vplanes_t_i
    #undef vplanes_t_f
    #undef vplanes_r_i
    #undef vplanes_r_f
    #undef vplanes_q_i
    #undef vplanes_q_f
    #undef vobjlin
    #undef veyelin
    #undef vmodes

1:
    # Scale texcoord by texsize and subtract offset (to correct for bilinear sampling if active)
    vmudh vstrq, vtexsize
    vsub  vstrq, vtexoffset

    #define vcspos_f     $v02
    #define vcspos_i     $v03

    ldv vcspos_f, PRIM_VTX_CS_POSf,prim_vtx
    ldv vcspos_i, PRIM_VTX_CS_POSi,prim_vtx

    jal GL_CalcScreenSpace
    slv vstrq.s,  SCREEN_VTX_S,screen_vtx

    sdv vcspos_f, SCREEN_VTX_CS_POSf,screen_vtx
    sdv vcspos_i, SCREEN_VTX_CS_POSi,screen_vtx

    j GL_CalcClipCodes
    move ra, ra2

    #undef vcspos_f
    #undef vcspos_i
    #undef vtexsize
    #undef vtexoffset

    #undef state_flags
    #undef prim_vtx
    #undef screen_vtx

    #undef v___
    #undef vrgba
    #undef vst
    #undef s
    #undef vobjpos
    #undef veyepos
    #undef veyenormal
    #undef vmtx0_i
    #undef vmtx0_f
    #undef vmtx1_i
    #undef vmtx1_f
    #undef vmtx2_i
    #undef vmtx2_f
    #undef vmtx3_i
    #undef vmtx3_f

    .endfunc


    ################################################################
    # GLCmd_DrawTriangle
    #
    ################################################################

    .func GLCmd_DrawTriangle
GLCmd_DrawTriangle:
    #define prim_verts  a0

    #define prim_vtx1   s5
    #define prim_vtx2   s6
    #define prim_vtx3   s7
    #define ra3         s8

    andi prim_vtx3, prim_verts, 0xFF
    srl  prim_vtx2, prim_verts, 8
    and  prim_vtx2, 0xFF
    srl  prim_vtx1, prim_verts, 16
    and  prim_vtx1, 0xFF

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

    lbu t0, SCREEN_VTX_CLIP_CODE(a1)
    lbu t1, SCREEN_VTX_CLIP_CODE(a2)
    lbu t2, SCREEN_VTX_CLIP_CODE(a3)
    or t5, t0, t1
    or t5, t2

    move s1, zero
    beqz t5, gl_draw_single_triangle
    move s2, zero

    jal GL_ClipTriangle
    nop

    beqz v1, gl_draw_triangle_end
    addi s2, -6
    lhu s5, 0(s1)
gl_draw_clipped_triangles_loop:
    move a1, s5
    lhu a2, 2(s1)
    lhu a3, 4(s1)

gl_draw_single_triangle:
    lhu a0, %lo(GL_TRI_CMD)
    lb v0, %lo(GL_TRI_CULL)
    jal RDPQ_Triangle
    li s3, %lo(RDPQ_CMD_STAGING)

    jal RDPQ_Send
    li s4, %lo(RDPQ_CMD_STAGING)

    blt s1, s2, gl_draw_clipped_triangles_loop
    addi s1, 2

gl_draw_triangle_end:
    jr ra3
    nop

    #undef prim_vtx1
    #undef prim_vtx2
    #undef prim_vtx3

    .endfunc


#include "rsp_gl_common.inc"
#include "rsp_gl_lighting.inc"
#include "rsp_gl_clipping.inc"
#include <rsp_rdpq.inc>
