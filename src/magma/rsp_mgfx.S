## Auto-generated file, transpiled with RSPL
#define __MAGMA_CONSTANTS 
#define MG_VERTEX_CACHE_COUNT 32
#define MG_DEFAULT_GUARD_BAND 2
#define MG_INLINE_UNIFORM_HEADER 12
#define MG_MAX_UNIFORM_PAYLOAD_SIZE (RSPQ_MAX_COMMAND_SIZE*4 - MG_INLINE_UNIFORM_HEADER)
#define MG_CLIP_PLANE_COUNT 6
#define MG_CLIP_PLANE_SIZE 8
#define MG_CLIP_CACHE_SIZE 9
#define MG_VTX_XYZ 0
#define MG_VTX_CLIP_CODE 6
#define MG_VTX_TR_CODE 7
#define MG_VTX_RGBA 8
#define MG_VTX_ST 12
#define MG_VTX_Wi 16
#define MG_VTX_Wf 18
#define MG_VTX_INVWi 20
#define MG_VTX_INVWf 22
#define MG_VTX_CS_POSi 24
#define MG_VTX_CS_POSf 32
#define MG_VTX_SIZE 40
#define MG_VTX_SIZE2 80
#define __MGFX_CONSTANTS 
#define MGFX_ATTRIBUTE_POS_NORM 0
#define MGFX_ATTRIBUTE_COLOR 1
#define MGFX_ATTRIBUTE_TEXCOORD 2
#define MGFX_BINDING_FOG 0
#define MGFX_BINDING_LIGHTING 1
#define MGFX_BINDING_TEXTURING 2
#define MGFX_BINDING_MATRICES 3
#define MGFX_LIGHT_COUNT_MAX 8
#define MGFX_LIGHT_POSITION 0
#define MGFX_LIGHT_COLOR 8
#define MGFX_LIGHT_ATT_INT 16
#define MGFX_LIGHT_ATT_FRAC 24
#define MGFX_LIGHT_SIZE 32
#define MGFX_MATRIX_SIZE 64
#define MGFX_VTX_POS_SHIFT 5
#include <rsp_magma.inc>

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

MgBeginShaderUniforms
  MgBeginUniform FOG, 0
    .align 1
    FOG_PARAMS: .ds.b 6
    FOG_MASK: .ds.b 1
  MgEndUniform

  MgBeginUniform LIGHTING, 1
    LIGHTING_LIGHTS: .ds.b 256
    .align 1
    LIGHTING_AMBIENT: .ds.b 8
    .align 2
    LIGHTING_COUNT: .ds.b 4
  MgEndUniform

  MgBeginUniform TEXTURING, 2
    .align 1
    TEXTURING_SCALE: .ds.b 4
    .align 1
    TEXTURING_OFFSET: .ds.b 4
  MgEndUniform

  MgBeginUniform MATRICES, 3
    .align 4
    MATRICES_MVP: .ds.b 64
    .align 4
    MATRICES_MV: .ds.b 64
    .align 4
    MATRICES_NORMAL: .ds.b 32
  MgEndUniform

MgEndShaderUniforms

MgBeginVertexInput
  MgBeginVertexAttribute 0, 0
    MgVertexAttributeLoaders LOAD_POS_NORM306, LOAD_POS_NORM308, LOAD_POS_NORM307, LOAD_POS_NORM309, LOAD_POS_NORM597, LOAD_POS_NORM596, LOAD_POS_NORM595, LOAD_POS_NORM594
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR333, LOAD_COLOR336
    MgBeginVertexAttributePatch PATCH_COLOR509
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD343, LOAD_TEXCOORD346
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
T3DCmd_VertLoad:
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6
  ori $at, $zero, %lo(MG_VERTEX_SIZE)
  ldv $v02, 0, 0, $t0
  ldv $v01, 0, 0, $at
  lsv $v02, 6, 4, $t0
  vmudh $v04, $v02, $v01.v
  vsar $v03, COP2_ACC_HI
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x1
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)
  vsar $v04, COP2_ACC_MD
  ssv $v03, 0, 8, $at ## Barrier: 0x2
  ssv $v04, 0, 10, $at ## Barrier: 0x2
  sdv $v04, 0, 0, $at ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2) ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8) ## Barrier: 0x2
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 6) ## Barrier: 0x1
  lh $t4, %lo(RSPQ_SCRATCH_MEM + 4) ## Barrier: 0x1
  andi $t3, $t2, 7
  addu $t0, $t1, $t3
  addiu $t0, $t0, 7
  addiu $s4, $t4, %lo(MG_VERTEX_OVERFLOW)
  andi $t0, $t0, 4088
  andi $s4, $s4, 4088
  addiu $t0, $t0, -1
  addu $s0, $s0, $t2
  jal DMAExec ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t2, $zero, 12
  ori $t0, $zero, %lo(MATRICES_MVP)
  ldv $v10, 0, 24, $t0
  ori $at, $zero, %lo(MG_NORMAL_MASK)
  LOAD_POS_NORM306: ldv $v21, 0, 0, $s4
  ldv $v01, 0, 0, $at
  ldv $v02, 0, 8, $at
  ldv $v06, 0, 8, $t0
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)
  ldv $v07, 0, 40, $t0
  ldv $v02, 8, 8, $at
  ldv $v01, 8, 0, $at
  ldv $v10, 8, 24, $t0
  ldv $v04, 0, 0, $t0
  ldv $v11, 0, 56, $t0
  LOAD_POS_NORM308: ldv $v28, 0, 0, $s4
  ori $at, $zero, %lo(MG_CLIP_FACTORS)
  ldv $v05, 0, 32, $t0
  ldv $v04, 8, 0, $t0
  ldv $v03, 0, 0, $at
  ldv $v03, 8, 0, $at
  ldv $v09, 0, 48, $t0
  addu $s6, $s4, $t1
  ori $at, $zero, %lo(MATRICES_NORMAL)
  ldv $v12, 0, 0, $at
  ldv $v13, 0, 8, $at
  ldv $v12, 8, 0, $at
  ldv $v13, 8, 8, $at
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)
  ldv $v14, 0, 16, $at
  ldv $v08, 0, 16, $t0
  ldv $v14, 8, 16, $at
  ori $t4, $zero, %lo(MG_CLIP_CACHE)
  addiu $t2, $s7, 40
  ori $at, $zero, %lo(MG_VIEWPORT)
  ldv $v15, 0, 0, $at
  ldv $v16, 0, 8, $at
  ldv $v15, 8, 0, $at
  ldv $v16, 8, 8, $at
  ori $t3, $zero, %lo(MG_CLIP_CACHE)
  ori $at, $zero, %lo(TEXTURING_SCALE)
  ldv $v11, 8, 56, $t0
  llv $v17, 0, 0, $at
  addu $t1, $s4, $t8
  LOAD_POS_NORM307: ldv $v21, 8, 0, $t1
  ldv $v06, 8, 8, $t0
  ldv $v07, 8, 40, $t0
  llv $v17, 8, 0, $at
  ori $at, $zero, %lo(TEXTURING_OFFSET)
  ldv $v05, 8, 32, $t0
  llv $v18, 0, 0, $at
  llv $v18, 8, 0, $at
  ldv $v08, 8, 16, $t0
  ldv $v09, 8, 48, $t0
  LOAD_POS_NORM309: ldv $v28, 8, 0, $t1
  lbu $t0, %lo(FOG_MASK + 0)
  sll $t9, $t8, 1
  VERTEX_LOOP:
  vand $v21, $v01, $v21.h3
  vmov $v28.e3, $v30.e2
  LOAD_COLOR333: llv $v24, 0, 0, $s4 ## Barrier: 0x3F00
  suv $v20, 0, 8, $t3 ## Barrier: 0x40
  vmov $v28.e7, $v30.e2
  LOAD_COLOR336: llv $v24, 4, 0, $t1 ## Barrier: 0x3F00
  sdv $v22, 8, 0, $t4 ## Barrier: 0x30
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x4000
  vmudn $v21, $v21, $v02.v
  vmudn $v29, $v05, $v28.h0
  vmadh $v29, $v04, $v28.h0
  suv $v20, 4, 8, $t4 ## Barrier: 0x80
  slv $v19, 8, 12, $t4 ## Barrier: 0x80
  vmadn $v29, $v09, $v28.h2
  vmadh $v29, $v08, $v28.h2
  sb $t6, 6($t4) ## Barrier: 0x10
  sdv $v24, 0, 0, $at ## Barrier: 0x4000
  vmadn $v29, $v07, $v28.h1
  sdv $v22, 0, 0, $t3 ## Barrier: 0xC
  vmadh $v29, $v06, $v28.h1
  slv $v19, 0, 12, $t3 ## Barrier: 0x40
  vmadn $v23, $v11, $v28.h3
  vmadh $v22, $v10, $v28.h3
  vmulf $v29, $v12, $v21.h0
  sb $t5, 6($t3) ## Barrier: 0x4
  luv $v24, 0, 0, $at ## Barrier: 0x4000
  vmacf $v29, $v13, $v21.h1
  vmacf $v21, $v14, $v21.h2
  sb $k0, 7($t4) ## Barrier: 0x20
  LOAD_TEXCOORD343: llv $v19, 0, 0, $s4 ## Barrier: 0x3F00
  vmudn $v26, $v23, $v03.v
  ori $at, $zero, %lo(FOG_PARAMS)
  vmadh $v25, $v22, $v03.v
  sb $t7, 7($t3) ## Barrier: 0x8
  LOAD_TEXCOORD346: llv $v19, 8, 0, $t1 ## Barrier: 0x3F00
  sdv $v22, 0, 24, $s7 ## Barrier: 0x100
  vch $v29, $v25, $v25.h3
  vcl $v29, $v26, $v26.h3
  ldv $v26, 0, 0, $at
  cfc2 $k1, $vcc
  vch $v29, $v22, $v22.h3
  ldv $v26, 8, 0, $at
  andi $fp, $k1, 1799
  vcl $v29, $v23, $v23.h3
  srl $s0, $fp, 5
  sdv $v22, 8, 24, $t2 ## Barrier: 0x200
  vaddc $v27, $v23, $v26.h1
  or $t5, $fp, $s0
  vadd $v25, $v22, $v26.h0
  srl $sp, $k1, 4
  andi $fp, $sp, 1799
  cfc2 $sp, $vcc
  vmudn $v29, $v27, $v26.h2
  ori $at, $zero, %lo(LIGHTING_AMBIENT)
  vmadh $v25, $v25, $v26.h2
  srl $s0, $fp, 5
  sdv $v23, 8, 32, $t2 ## Barrier: 0x200
  or $t6, $fp, $s0
  sdv $v23, 0, 32, $s7 ## Barrier: 0x100
  vge $v25, $v00, $v25.h2
  lw $k1, %lo(LIGHTING_COUNT + 0)
  vmudl $v23, $v23, $v15.h3
  vmadm $v22, $v22, $v15.h3
  andi $s0, $sp, 1799
  vmadn $v23, $v00, $v00
  srl $s1, $s0, 5
  nor $t7, $s0, $s1
  ssv $v22, 14, 16, $t2 ## Barrier: 0x800
  ssv $v22, 6, 16, $s7 ## Barrier: 0x400
  vrcph $v22.e3, $v22.e3
  ssv $v23, 14, 18, $t2 ## Barrier: 0x800
  ssv $v23, 6, 18, $s7 ## Barrier: 0x400
  vrcpl $v23.e3, $v23.e3
  srl $fp, $sp, 4
  vrcph $v22.e3, $v22.e7
  vrcpl $v23.e7, $v23.e7
  vrcph $v22.e7, $v00.e7
  andi $s0, $fp, 1799
  ldv $v20, 0, 0, $at
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)
  vmudl $v29, $v23, $v23.h3
  ssv $v22, 14, 20, $t2 ## Barrier: 0x2000
  ssv $v22, 6, 20, $s7 ## Barrier: 0x1000
  vmadm $v29, $v22, $v23.h3
  ssv $v23, 14, 22, $t2 ## Barrier: 0x2000
  ssv $v23, 6, 22, $s7 ## Barrier: 0x1000
  vmadn $v23, $v23, $v22.h3
  vmadh $v22, $v22, $v22.h3
  ldv $v20, 8, 0, $at
  srl $s1, $s0, 5
  nor $k0, $s0, $s1
  vmudh $v29, $v16, $v30.e7
  vmadn $v23, $v23, $v15.v
  beq $k1, $zero, LIGHT_END
  vmadh $v22, $v22, $v15.v
  LIGHT_LOOP:
  ldv $v27, 0, 0, $sp
  ldv $v27, 8, 0, $sp
  ldv $v26, 0, 8, $sp
  ldv $v26, 8, 8, $sp
  vmulf $v27, $v27, $v21.v
  vmulf $v29, $v26, $v27.h0
  vmacf $v29, $v26, $v27.h1
  vmacf $v26, $v26, $v27.h2
  vge $v26, $v26, $v00
  addiu $sp, $sp, 32
  addiu $k1, $k1, 65535
  bne $k1, $zero, LIGHT_LOOP
  vadd $v20, $v20, $v26.v
  LIGHT_END:
  or $t4, $zero, $t2
  addu $t1, $t1, $t9
  LOAD_POS_NORM597: ldv $v28, 8, 0, $t1
  or $t3, $zero, $s7
  addiu $t2, $t2, 80
  addu $s4, $s4, $t9
  PATCH_COLOR509: vmulf $v20, $v20, $v24.v
  ctc2 $t0, $vcc
  LOAD_POS_NORM596: ldv $v28, 0, 0, $s4
  sltu $at, $s4, $s6
  LOAD_POS_NORM595: ldv $v21, 8, 0, $t1
  vmrg $v20, $v20, $v25.h2
  LOAD_POS_NORM594: ldv $v21, 0, 0, $s4
  vmudh $v29, $v18, $v30.e7
  vmadh $v19, $v19, $v17.v
  bne $at, $zero, VERTEX_LOOP
  addiu $s7, $s7, 80
  suv $v20, 0, 8, $t3 ## Barrier: 0x40
  suv $v20, 4, 8, $t4 ## Barrier: 0x80
  sdv $v22, 0, 0, $t3 ## Barrier: 0xC
  sb $t5, 6($t3) ## Barrier: 0x4
  slv $v19, 8, 12, $t4 ## Barrier: 0x80
  slv $v19, 0, 12, $t3 ## Barrier: 0x40
  sdv $v22, 8, 0, $t4 ## Barrier: 0x30
  sb $t6, 6($t4) ## Barrier: 0x10
  sb $t7, 7($t3) ## Barrier: 0x8
  j RSPQ_Loop
  sb $k0, 7($t4) ## Barrier: 0x20

MgEndShader

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