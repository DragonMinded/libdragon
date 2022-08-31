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
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
GL_STATE:
    GL_STATE_SCISSOR:         .quad   0
    GL_STATE_FLAGS:           .word   0
    GL_STATE_DEPTH_FUNC:      .word   0
    GL_STATE_ALPHA_FUNC:      .word   0
    GL_STATE_BLEND_SRC:       .word   0
    GL_STATE_BLEND_DST:       .word   0
    GL_STATE_BLEND_CYCLE:     .word   0
    GL_STATE_TEX_ENV_MODE:    .word   0
    GL_STATE_POLYGON_MODE:    .word   0
    GL_STATE_PRIM_TYPE:       .word   0
    GL_STATE_FOG_COLOR:       .word   0
    GL_STATE_FB_SIZE:         .short  0, 0
    GL_STATE_SCISSOR_RECT:    .short  0, 0, 0, 0
    GL_STATE_ALPHA_REF:       .byte   0
    RSPQ_EndSavedState

CONVERT_CONST:  .short 1, 2, 0x8000, 0x200, 0xFF, 134, 0x7F, 0x80

UPDATE_FUNCTIONS:
    .short RSPQ_Loop            - _start  # Do nothing
    .short GL_UpdateDepthTest   - _start
    .short GL_UpdateDepthMask   - _start
    .short GL_UpdateBlend       - _start
    .short GL_UpdateDither      - _start
    .short GL_UpdatePoints      - _start
    .short GL_UpdateAlphaTest   - _start
    .short GL_UpdateBlendCycle  - _start
    .short GL_UpdateFogCycle    - _start
    .short GL_UpdateScissor     - _start

    .text

    #############################################################
    # GLCmd_SetFlag
    #
    # Sets or clears a flag
    #
    # ARGS:
    #   a0: Bit 31..24: Command id
    #       Bit 23..12: Offset into UPDATE_FUNCTIONS jump table
    #       Bit 11:     If 1, set the flag, otherwise clear it
    #   a1: flag mask (inverted if clearing)
    #############################################################
    .func GLCmd_SetFlag
GLCmd_SetFlag:
    sll t0, a0, 20
    lw t1, %lo(GL_STATE_FLAGS)
    bgez t0, flag_clear
    and t2, t1, a1
    or t2, t1, a1

flag_clear:
    j GLCmd_Update
    sw t2, %lo(GL_STATE_FLAGS)
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

GL_UpdateDepthTest:
    lw t0, %lo(GL_STATE_FLAGS)
    lw t1, %lo(GL_STATE_DEPTH_FUNC)

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
    lw t1, %lo(GL_STATE_ALPHA_FUNC)

    andi t0, FLAG_ALPHA_TEST            
    beqz t0, alpha_test_disable         
    lw t2, %lo(RDPQ_OTHER_MODES) + 0x4
    xori t1, GL_GREATER                 
    sltu t1, 1                          
    sll t0, t1, 1  
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

GL_UpdatePoints:
    lw t2, %lo(GL_STATE_PRIM_TYPE)
    beqz t2, is_points                  # prim_type == GL_POINTS
    li t3, SOM_ZSOURCE_PRIM

    addi t2, -1
    sltu t2, (GL_TRIANGLES - 1)
    bne t2, zero, is_not_points         # prim_type == GL_LINES || prim_type == GL_LINE_LOOP || prim_type == GL_LINE_STRIP
    lw t0, %lo(GL_STATE_POLYGON_MODE)

    li t1, GL_POINT                     # polygon_mode == GL_POINT
    beq t0, t1, is_points
    nop

is_not_points:
    li t2, SOM_TEXTURE_PERSP >> 32
    li t3, SOM_ZSOURCE_PIXEL

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
    jr ra
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
    # TODO RDPQ_SCISSOR_RECT
    jr ra
    nop

/*
GLCmd_Begin:
    jr ra
    sb a0, %lo(GL_PRIM_MODE)

GLCmd_End:
    jr ra
    nop

GLCmd_Vertex:
    jr ra
    nop

GLCmd_Color:
    j gl_save_vector
    li s2, %lo(GL_CURRENT_COLOR)
GLCmd_TexCoord:
    j gl_save_vector
    li s2, %lo(GL_CURRENT_TEXCOORD)
GLCmd_Normal:
    li s2, %lo(GL_CURRENT_NORMAL)
gl_save_vector:
    sub s1, rspq_dmem_buf_ptr, rspq_cmd_size
    addi s1, %lo(RSPQ_DMEM_BUFFER) + 4
    lqv $v00,0, 0x00,s1
    lrv $v00,0, 0x10,s1
    jr ra
    sqv $v00,0, 0x00,s2


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
    #define vzero  $v00

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
