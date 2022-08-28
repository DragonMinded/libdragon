#include <rsp_queue.inc>

#define VERTEX_SIZE (16 * 4)

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand GLCmd_Begin,     4
        RSPQ_DefineCommand GLCmd_End,       4
        RSPQ_DefineCommand GLCmd_Vertex,    20
        RSPQ_DefineCommand GLCmd_Color,     20
        RSPQ_DefineCommand GLCmd_TexCoord,  20
        RSPQ_DefineCommand GLCmd_Normal,    16
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
GL_VERTEX_CACHE:        .ds.b 4 * VERTEX_SIZE
    .align 4
GL_CURRENT_COLOR:       .ds.b 16
    .align 4
GL_CURRENT_TEXCOORD:    .ds.b 16
    .align 4
GL_CURRENT_NORMAL:      .ds.b 16
GL_PRIM_MODE:           .byte 0
GL_PRIM_INDICES:        .byte 0
GL_PRIM_PROGRESS:       .byte 0
GL_TRIANGLE_COUNTER:    .byte 0
GL_NEXT_VERTEX:         .byte 0
GL_CACHE_LOCKED:        .byte 0
    RSPQ_EndSavedState

    .bss

    .text

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

    li t1, %lo(VEC_CONST)
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
