#include <rsp_queue.inc>
#include <rdpq_macros.h>
#include "gl_constants.h"
#include "GL/gl_enums.h"

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_InitPipe,      4
        RSPQ_DefineCommand GLCmd_SetVtxLoader,  12 + VTX_LOADER_MAX_SIZE
        RSPQ_DefineCommand GLCmd_SetVtxCmdSize, 8
        RSPQ_DefineCommand GLCmd_DrawTriangle,  8
        RSPQ_DefineCommand GLCmd_SetPrimVertex, 32
        RSPQ_DefineCommand GLCmd_SetByte,       8
        RSPQ_DefineCommand GLCmd_SetWord,       8
        RSPQ_DefineCommand GLCmd_SetLong,       12
    RSPQ_EndOverlayHeader

    .align 4
BANNER0: .ascii " RSP OpenGL T&L "
BANNER1: .ascii "Rasky & Snacchus"

    RSPQ_BeginSavedState
    #include "rsp_gl_state.inc"

#define SCREEN_VTX_CS_POSi          0     // X, Y, Z, W (all 32-bit)
#define SCREEN_VTX_CS_POSf          8     // X, Y, Z, W (all 32-bit)
#define SCREEN_VTX_X               16
#define SCREEN_VTX_Y               18
#define SCREEN_VTX_Z               20
#define SCREEN_VTX_CLIP_CODE       22
#define SCREEN_VTX_PADDING         23
#define SCREEN_VTX_RGBA            24
#define SCREEN_VTX_S               28
#define SCREEN_VTX_T               30
#define SCREEN_VTX_W               32     // FIXME: this is duplicated in CS_POS
#define SCREEN_VTX_INVW            36     // 32-bit
#define SCREEN_VTX_SIZE            40

    .align 3
VERTEX_CACHE:   .dcb.b      PRIM_VTX_SIZE * VERTEX_CACHE_SIZE

    RSPQ_EndSavedState

    .align 4
DEFAULT_ATTRIBUTES:     .half 0, 0, 0, 1<<5, 0, 0, 0, 0x7FFF, 0, 0, 0, 1<<8

    .align 4
CACHE_OFFSETS:          .half 2,4,6,8,10,12,14,16,18

CLIP_CODE_FACTORS:      .half 1, 1, GUARD_BAND_FACTOR, GUARD_BAND_FACTOR


    .text

    .func GLCmd_InitPipe
GLCmd_InitPipe:
    move s0, a0
    li s4, %lo(GL_STATE)
    j DMAIn
    li t0, DMA_SIZE(GL_STATE_SIZE, 1)
    .endfunc

    .func GLCmd_SetVtxLoader
GLCmd_SetVtxLoader:
    move s0, a1
    li s4, %lo(gl_vtx_loader)
    jal DMAInAsync
    li t0, DMA_SIZE(VTX_LOADER_MAX_SIZE, 1)
    add s0, a0, s4
    jal_and_j DMAOutAsync, RSPQ_Loop
    .endfunc

    .func GLCmd_SetVtxCmdSize
GLCmd_SetVtxCmdSize:
    li s4, %lo(_RSPQ_OVERLAY_COMMAND_TABLE) + 8
    sh a0, 0(s4)
    move s0, a1
    j DMAOutAsync
    li t0, DMA_SIZE(8, 1)
    .endfunc

    .func GLCmd_SetByte
GLCmd_SetByte:
    jr ra
    sb a1, %lo(GL_STATE)(a0)
    .endfunc

    .func GLCmd_SetLong
GLCmd_SetLong:
    sw a2, %lo(GL_STATE) + 4(a0)
GLCmd_SetWord:
    jr ra
    sw a1, %lo(GL_STATE)(a0)
    .endfunc

    ########################################
    # GL_HandleMatrixPalette
    #
    # Args:
    #   v1 = Matrix index
    #   t5 = State flags
    #   s1 = Matrix pointer offset
    #
    ########################################
    .func GL_HandleMatrixPalette
GL_HandleMatrixPalette:
    #define mtx_index   v1
    #define state_flags t5
    #define mtx_pointer s1
    andi t0, state_flags, FLAG_MATRIX_PALETTE
    beqz t0, JrRa
    lw t0, %lo(GL_LOADED_MTX_INDEX) - 0xC(mtx_pointer)

    lw s0, %lo(GL_MATRIX_POINTERS)(mtx_pointer)

    # Check if this matrix is already loaded
    beq t0, mtx_index, JrRa
    sw mtx_index, %lo(GL_LOADED_MTX_INDEX) - 0xC(mtx_pointer)

    sll t0, mtx_index, 6
    add s0, t0
    j DMAIn
    li t0, DMA_SIZE(MATRIX_SIZE, 1)
    #undef mtx_index
    #undef state_flags
    #undef mtx_pointer
    .endfunc

    ########################################
    # GLCmd_SetPrimVertex
    #
    # Arguments:
    # * 0x00 (a0): offset within VERTEX_CACHE
    # * 0x04 (a1): object space X, Y (16-bit)
    # * 0x08 (a2): object space Z, W (16-bit)
    # * 0x0C (a3): RGBA (8-bit each one)
    # * 0x10:      S, T (16-bit)
    # * 0x14:      normal X, Y, Z (8-bit each one) (LSB must be 0)
    #
    ########################################
    .align 3
    .func GLCmd_SetPrimVertex
GLCmd_SetPrimVertex:
    #define vtx         a0
    #define default     s0
    #define current     s1
    #define cmd_ptr     s4
    #define norm        v0
    #define mtxi        v1
    #define state_flags t5

    #define v___        $v01

    #define vmtx0_i     $v16       //  m00 m01 m02 m03
    #define vmtx0_f     $v17
    #define vmtx1_i     $v18       //  m10 m11 m12 m13
    #define vmtx1_f     $v19
    #define vmtx2_i     $v20       //  m20 m21 m22 m23
    #define vmtx2_f     $v21
    #define vmtx3_i     $v22       //  m30 m31 m32 m03
    #define vmtx3_f     $v23

    #define vpos        $v24
    #define vcol        $v25
    #define vtex        $v26
    #define vdefault    $v27
    #define vcspos_i    $v28
    #define vcspos_f    $v29

    #define x  e0
    #define y  e1
    #define z  e2
    #define w  e3

    addi cmd_ptr, rspq_dmem_buf_ptr, %lo(RSPQ_DMEM_BUFFER) + 4
    sub cmd_ptr, rspq_cmd_size

    li default, %lo(DEFAULT_ATTRIBUTES)
    li current, %lo(GL_CURRENT_ATTRIBUTES)

    .align 3
gl_vtx_loader:
    .ds.l VTX_LOADER_MAX_COMMANDS
    .align 3

    addi vtx, %lo(VERTEX_CACHE)
    sdv vpos, PRIM_VTX_X     ,vtx
    sdv vcol, PRIM_VTX_R     ,vtx
    sdv vtex, PRIM_VTX_TEX_S ,vtx
    sw  norm, PRIM_VTX_NORMAL(vtx)

    move ra2, ra
    lw state_flags, %lo(GL_STATE_FLAGS)
    
    li s4, %lo(GL_MATRIX_MVP)
    sb mtxi, PRIM_VTX_MTX_INDEX(vtx)
    jal GL_HandleMatrixPalette
    li s1, 0x10

    jal GL_MtxTransformSingle
    vcopy $v27, vpos

    # 32-bit right shift by 5, to keep the clip space coordinates unscaled
    vmudm vcspos_i, vcspos_i, vshift8.e4
    vmadl vcspos_f, vcspos_f, vshift8.e4

    sdv vcspos_i, PRIM_VTX_CS_POSi,vtx
    sdv vcspos_f, PRIM_VTX_CS_POSf,vtx

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
    jr ra2
    sb t0, PRIM_VTX_TRCODE(vtx)

    # FIXME: in a glBegin/glEnd pair, we should also cache the per-vertex material.

    #undef cmd_ptr
    #undef vtx
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
    #   s3   = Destination vertex address
    #   $v02 = Clip space position (fractional part)
    #   $v03 = Clip space position (integer part)
    #
    ################################################################
    .func GL_CalcScreenSpace
GL_CalcScreenSpace:
    #define dst          s3
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
    #   s3   = Destination vertex address
    #   $v02 = Clip space position (fractional part)
    #   $v03 = Clip space position (integer part)
    #
    ################################################################
    .func GL_CalcClipCodes
GL_CalcClipCodes:
    #define dst          s3
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
    #   s3 = address of the vertex in DMEM (usually within VERTEX_CACHE)
    #
    ################################################################
    .func GL_TnL
GL_TnL:
    #define tmp_ptr      s2
    #define vtx          s3
    #define state_flags  t5
    #define s            e0
    move ra2, ra

    lw state_flags, %lo(GL_STATE_FLAGS)

    #define v___         $v01
    #define veyenormal   $v03
    #define vrgba        $v04
    #define vobjpos      $v27
    #define veyepos      $v28

    ldv vrgba.e0, PRIM_VTX_R, vtx     # R + G + B + A
    ldv vrgba.e4, PRIM_VTX_R, vtx     # R + G + B + A

    andi t0, state_flags, FLAG_NEED_EYE_SPACE
    beqz t0, 2f
    li s4, %lo(GL_MATRIX_MODELVIEW)

    lbu v1, PRIM_VTX_MTX_INDEX(vtx)
    jal GL_HandleMatrixPalette
    li s1, 0xC

    addi s5, vtx, PRIM_VTX_NORMAL-4
    lpv vobjpos.e0,  0,s5              # loads NX + NY + NZ into lanes 4-7
    ldv vobjpos.e0,  PRIM_VTX_X,vtx    # loads X + Y + Z + W into lanes 0-3

    # TODO: technically we need the inverse transpose matrix,
    #       but for rigid matrices (translation, rotation, uniform scale) this is fine

    # Transform vertex pos and normal into eye space
    jal GL_MtxTransformDouble
    vmov vobjpos.e7, vzero.e0

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

    andi t0, state_flags, FLAG_LIGHTING
    bnez t0, GL_VtxLighting
    li ra, %lo(1f)
1:

    #define vtmp    $v06
    #define vfog_i  $v07
    #define vfog_f  $v08
    andi t0, state_flags, FLAG_FOG
    beqz t0, 1f
    li s1, %lo(GL_STATE_FOG_PARAMS)
    llv vfog_i, 0,s1
    llv vfog_f, 4,s1

    # Note that the GL spec says the correct formula is
    #   f = (fog_end - abs(veyepos.z)) / (fog_end - fog_start)
    # And the final fogged color is calculated as
    #   C = f * Ci + (1 - f) * Cf
    # Where Ci is the incoming fragment color and Cf is the fog color.
    
    # However, there is a problem with translating this formula directly to a blender configuration:
    #   RDPQ_BLENDER((IN_RGB, SHADE_ALPHA, FOG_RGB, INV_MUX_ALPHA))
    # This would mean that a shade alpha of 1 represents "no fog" and a value of 0 is "full fog".
    # Because shade alpha is internally an 8 bit fixed point value (0-255), it's not possible to represent
    # a value of 1.0 exactly, only just below it. This leads to triangles that are completely outside of
    # the fog area to still be tinted slightly with the fog color.

    # To avoid this, we turn the formula around, so that shade alpha 0 (which is exactly representable)
    # corresponds to "no fog".
    # This means we have to compute 1-f instead of f. If we rearrange the formula, we get
    #   (abs(veyepos.z) - fog_start) / (fog_end - fog_start)
    # instead.

    # Compute abs(veyepos.z).
    # abs(veyepos.z) is an approximation for the distance between the
    # vertex and the origin in eye space, as recommended by the GL spec.
    vsubc vtmp, vzero, veyepos.e2
    vge vtmp, veyepos.e2

    # vtmp.e0 = abs(veyepos.z) - fog_start
    # Note that fog_start might be negative. In practice this would
    # rarely be the case, but it is not forbidden by the GL spec.
    vsubc vtmp, vfog_i.e1

    # vtmp.e0 = (abs(veyepos.z) - fog_start) / (fog_end - fog_start)
    # The factor is premultiplied so that combined with VTX_SHIFT
    # the product will be in 1.15 precision and saturated to 0x7FFF.
    vmudm v___, vtmp, vfog_f.e0
    vmadh vtmp, vtmp, vfog_i.e0

    # Clamp negative values to 0
    vge vtmp, vzero

    # Save the alpha factor in the vertex color, overwriting the alpha component.
    vmov vrgba.e3, vtmp.e0
    #undef vtmp
    #undef vfog
1:

    #define vtexsize     $v06
    #define vtexoffset   $v07
    #define vstrq        $v08

    ldv vstrq,  PRIM_VTX_TEX_S,vtx     # S + T + R + Q
    suv vrgba,  SCREEN_VTX_RGBA,vtx

    li s1, %lo(GL_STATE_TEX_SIZE)
    llv vtexsize.s,   0,s1
    llv vtexoffset.s, 4,s1

    #define vmodes      $v09
    #define vobjlin     $v10
    #define veyelin     $v11
    #define vsphmap     $v12

    srl t0, state_flags, TEX_GEN_S_SHIFT
    andi t0, 0xF
    beqz t0, 2f
    ldv vobjpos.e4,  0,tmp_ptr

    # Fill VCC with enable-flags
    # We need to OR with 0xF0 so that the mode constants stay in the vmodes register
    ori t0, 0x70
    ctc2 t0, COP2_CTRL_VCC

    li s1, %lo(GL_TEX_GEN)
    
    #define vplanes_s_i $v13
    #define vplanes_s_f $v14
    #define vplanes_t_i $v15
    #define vplanes_t_f $v16
    #define vplanes_r_i $v17
    #define vplanes_r_f $v18
    #define vplanes_q_i $v19
    #define vplanes_q_f $v20

    lqv vmodes,       TEX_GEN_MODE_OFFSET,         s1
    lqv vplanes_s_i,  TEX_GEN_INTEGER_OFFSET+0x00, s1

    # If profiling is enabled, this ucode won't fit into IMEM in its entirety.
    # In that case, sacrifice linear tex coord generation, a rarely used feature.
#if !RSPQ_PROFILE
    # Check if linear tex gen is active for any coordinate
    andi t0, state_flags, FLAG_TEX_GEN_LINEAR
    beqz t0, 1f
    # Select which coords to enable based on the enable-flags that were loaded into VCC above
    vmrg vmodes, vmodes, vzero
    lqv vplanes_s_f,  TEX_GEN_FRACTION_OFFSET+0x00,s1
    lqv vplanes_t_i,  TEX_GEN_INTEGER_OFFSET+0x10, s1
    lqv vplanes_t_f,  TEX_GEN_FRACTION_OFFSET+0x10,s1
    lqv vplanes_r_i,  TEX_GEN_INTEGER_OFFSET+0x20, s1
    lqv vplanes_r_f,  TEX_GEN_FRACTION_OFFSET+0x20,s1
    lqv vplanes_q_i,  TEX_GEN_INTEGER_OFFSET+0x30, s1
    lqv vplanes_q_f,  TEX_GEN_FRACTION_OFFSET+0x30,s1

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
    
    #undef vplanes_s_i
    #undef vplanes_s_f
    #undef vplanes_t_i
    #undef vplanes_t_f
    #undef vplanes_r_i
    #undef vplanes_r_f
    #undef vplanes_q_i
    #undef vplanes_q_f

1:
#endif

    #define vsqdist_i   $v13
    #define vsqdist_f   $v14
    #define vtmp_i      $v15
    #define vtmp_f      $v16
    #define vinvdist_i  $v17
    #define vinvdist_f  $v18
    #define veposnorm   $v19
    #define vdot        $v20
    #define refl_i      $v21
    #define refl_f      $v22

    # Check if spherical tex gen is enabled for any coordinate
    andi t0, state_flags, FLAG_TEX_GEN_SPHERICAL
    beqz t0, 1f
    # veposnorm = normalize(veyepos)
    vmudh v___, veyepos, veyepos
    vsar  vsqdist_f, COP2_ACC_MD
    vsar  vsqdist_i, COP2_ACC_HI
    vaddc vtmp_f, vsqdist_f, vsqdist_f.h1
    vadd  vtmp_i, vsqdist_i, vsqdist_i.h1
    vaddc vsqdist_f, vtmp_f, vsqdist_f.h2
    vadd  vsqdist_i, vtmp_i, vsqdist_i.h2

    vrsqh v___.e0,       vsqdist_i.e0
    vrsql vinvdist_f.e0, vsqdist_f.e0
    vrsqh vinvdist_i.e0, vzero.e0

    vmudm v___,      veyepos, vinvdist_f.e0
    vmadh veposnorm, veyepos, vinvdist_i.e0

    # vdot = dot(veposnorm, veyenormal)
    vmulf vdot, veposnorm, veyenormal
    vaddc vtmp_f, vdot, vdot.h1
    vaddc vdot, vtmp_f, vdot.h2

    # negate
    vsubc vdot, vzero, vdot

    vcopy vtmp_i, vzero
    vmov vtmp_i.e2, K16384

    # refl = veposnorm - 2 * vdot * veyenormal + (0, 0, 1)
    vmulf v___, veyenormal, vdot.e0
    vmacf v___, veyenormal, vdot.e0 # Add twice to account for multiplication by 2
    vmadh v___, veposnorm, K1
    vmadh v___, vtmp_i, K2

    vsar refl_f, COP2_ACC_MD
    vsar refl_i, COP2_ACC_HI

    # m = 1 / 2 * sqrt(dot(refl, refl))
    vmudl v___,   refl_f, refl_f
    vmadm v___,   refl_i, refl_f
    vmadn vsqdist_f, refl_f, refl_i
    vmadh vsqdist_i, refl_i, refl_i

    vaddc vtmp_f, vsqdist_f, vsqdist_f.h1
    vadd  vtmp_i, vsqdist_i, vsqdist_i.h1
    vaddc vsqdist_f, vtmp_f, vsqdist_f.h2
    vadd  vsqdist_i, vtmp_i, vsqdist_i.h2

    vaddc vtmp_i, vzero, K128

    vrsqh v___.e0,       vsqdist_i.e0
    vrsql vinvdist_f.e0, vsqdist_f.e0
    vrsqh vinvdist_i.e0, vzero.e0

    # vshmap = refl * m + 0.5
    vmudh v___,    vtmp_i, K1
    vmadl v___,    refl_f, vinvdist_f.e0
    vmadm v___,    refl_i, vinvdist_f.e0
    vmadn v___,    refl_f, vinvdist_i.e0
    vmadh vsphmap, refl_i, vinvdist_i.e0

1:
    veq  v___,  vmodes,  vmodes.e4
    vmrg vstrq, vobjlin, vstrq
    veq  v___,  vmodes,  vmodes.e5
    vmrg vstrq, veyelin, vstrq
    veq  v___,  vmodes,  vmodes.e6
    vmrg vstrq, vsphmap, vstrq

    #undef vsqdist_i
    #undef vsqdist_f
    #undef vtmp_i
    #undef vtmp_f
    #undef vinvdist_i
    #undef vinvdist_f
    #undef veposnorm
    #undef vdot
    #undef refl_i
    #undef refl_f
    #undef vobjlin
    #undef veyelin
    #undef vmodes

2:
    #define vinvq_i $v26
    #define vinvq_f $v27
    #define vstrq_i $v28
    #define vstrq_f $v29
    #define q       e3

    # Apply texture matrix
    vcopy $v27, vstrq
    jal GL_MtxTransformSingle
    li s4, %lo(GL_MATRIX_TEXTURE)

    # Perform perspective division
    vrcph vinvq_i.q, vstrq_i.q
    vrcpl vinvq_f.q, vstrq_f.q
    vrcph vinvq_i.q, vzero.e0

    vmudl v___,    vstrq_f, vinvq_f.q
    vmadm v___,    vstrq_i, vinvq_f.q
    vmadn vstrq_f, vstrq_f, vinvq_i.q
    vmadh vstrq_i, vstrq_i, vinvq_i.q

    # vmudm vstrq_i, vstrq, K256 # >> 8
    # vmadl vstrq_f, vzero, vzero

    # Scale texcoord by texsize and subtract offset (to correct for bilinear sampling if active)
    vmudn v___,  vstrq_f, vtexsize
    vmadh v___,  vstrq_i, vtexsize
    vmadh vstrq, vtexoffset, K1

    #undef vinvq_i
    #undef vinvq_f
    #undef vstrq_i
    #undef vstrq_f
    #undef q

    lbu t0, PRIM_VTX_TRCODE(vtx)

    #define vcspos_f     $v02
    #define vcspos_i     $v03

    ldv vcspos_f, PRIM_VTX_CS_POSf,vtx
    ldv vcspos_i, PRIM_VTX_CS_POSi,vtx

    # Mark this vertex as having T&L applied
    ori t0, 0x80
    sb t0, PRIM_VTX_TRCODE(vtx)

    jal GL_CalcScreenSpace
    slv vstrq.s,  SCREEN_VTX_S,vtx

    j GL_CalcClipCodes
    move ra, ra2

    #undef vcspos_f
    #undef vcspos_i
    #undef vtexsize
    #undef vtexoffset

    #undef state_flags
    #undef vtx
    #undef vtx

    #undef v___
    #undef vrgba
    #undef vst
    #undef s
    #undef vobjpos
    #undef veyepos
    #undef veyenormal

    .endfunc


    ################################################################
    # GLCmd_DrawTriangle
    #
    # Arguments:
    #   a0: Bit 31..24: Command id
    #       Bit 11..0:  Offset into vertex cache of vtx1
    #   a1: Bit 27..16: Offset into vertex cache of vtx2
    #       Bit 11..0:  Offset into vertex cache of vtx3
    #
    ################################################################
    .func GLCmd_DrawTriangle
GLCmd_DrawTriangle:
    #define vtx1        a1
    #define vtx2        a2
    #define vtx3        a3
    #define trcode1     t6
    #define trcode2     t7
    #define trcode3     t8

    # If GL_FONT_AND_BACK is set (GL_TRI_CULL==-1), there's nothing to do
    lb v0, %lo(GL_TRI_CULL)
    bltz v0, RSPQ_Loop

    addi vtx3, a1, %lo(VERTEX_CACHE)
    srl  vtx2, a1, 16
    addi vtx2, %lo(VERTEX_CACHE)
    addi vtx1, a0, %lo(VERTEX_CACHE)

    # Trivial reject: if all the vertices are out of the same plane (at least one),
    # the triangle is out of the viewport.
    # NOTE: This deliberately uses lb instead of lbu so the sign bit is extended.
    #       The MSB of each TR-code is a bit flag that is set if the vertex has already
    #       had T&L applied once.
    lb trcode1, PRIM_VTX_TRCODE(vtx1)
    lb trcode2, PRIM_VTX_TRCODE(vtx2)
    lb trcode3, PRIM_VTX_TRCODE(vtx3)
    and t0, trcode1, trcode2
    and t0, trcode3
    andi t0, 0x3F
    bnez t0, JrRa
    nop

    # Perform T&L for each vertex if we haven't already
    bgezal trcode1, GL_TnL
    move s3, vtx1

    bgezal trcode2, GL_TnL
    move s3, vtx2

    bgezal trcode3, GL_TnL
    move s3, vtx3

    lbu t0, SCREEN_VTX_CLIP_CODE(vtx1)
    lbu t1, SCREEN_VTX_CLIP_CODE(vtx2)
    lbu t2, SCREEN_VTX_CLIP_CODE(vtx3)
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
    move vtx1, s5
    lhu vtx2, 2(s1)
    lhu vtx3, 4(s1)

gl_draw_single_triangle:
    addi vtx1, SCREEN_VTX_X
    addi vtx2, SCREEN_VTX_X
    addi vtx3, SCREEN_VTX_X
    
    lhu a0, %lo(GL_TRI_CMD)
    lb v0, %lo(GL_TRI_CULL)
    jal RDPQ_Triangle
    li s3, %lo(RDPQ_CMD_STAGING)

    jal RDPQ_Send
    li s4, %lo(RDPQ_CMD_STAGING)

    blt s1, s2, gl_draw_clipped_triangles_loop
    addi s1, 2

gl_draw_triangle_end:
    j RSPQ_Loop
    nop

    #undef vtx1
    #undef vtx2
    #undef vtx3

    .endfunc

#include "rsp_gl_common.inc"
#include "rsp_gl_lighting.inc"
#include "rsp_gl_clipping.inc"
#include <rsp_rdpq.inc>
