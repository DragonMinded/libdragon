## Auto-generated file, transpiled with RSPL
#define ENABLE_ENV_MAP 
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
#define NEED_EYE_SPACE 1
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
    FOG_PARAMS: .ds.b 8
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
    MgVertexAttributeLoaders LOAD_POS_NORM308, LOAD_POS_NORM310, LOAD_POS_NORM311, LOAD_POS_NORM309, LOAD_POS_NORM596, LOAD_POS_NORM594, LOAD_POS_NORM597, LOAD_POS_NORM595
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR335, LOAD_COLOR338
    MgBeginVertexAttributePatch PATCH_COLOR510
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
T3DCmd_VertLoad:
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6
  ldv $v02, 0, 0, $t0
  ori $at, $zero, %lo(MG_VERTEX_SIZE)
  lsv $v02, 6, 4, $t0
  ldv $v01, 0, 0, $at
  vmudh $v04, $v02, $v01.v
  vsar $v04, COP2_ACC_MD
  vsar $v03, COP2_ACC_HI
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x1
  sdv $v04, 0, 0, $at ## Barrier: 0x1
  ssv $v03, 0, 8, $at ## Barrier: 0x2
  ssv $v04, 0, 10, $at ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8) ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2) ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 6) ## Barrier: 0x1
  lh $t4, %lo(RSPQ_SCRATCH_MEM + 4) ## Barrier: 0x1
  addu $s0, $s0, $t2
  andi $t3, $t2, 7
  addiu $t2, $zero, 12
  addiu $s4, $t4, %lo(MG_VERTEX_OVERFLOW)
  addu $t0, $t1, $t3
  addiu $t0, $t0, 7
  andi $t0, $t0, 4088
  andi $s4, $s4, 4088
  jal DMAExec ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1
  addu $s6, $s4, $t1
  ori $t0, $zero, %lo(MATRICES_MVP)
  ori $at, $zero, %lo(MG_NORMAL_MASK)
  ldv $v07, 0, 16, $t0
  ldv $v03, 0, 0, $t0
  ldv $v09, 0, 24, $t0
  LOAD_POS_NORM308: ldv $v16, 0, 0, $s4
  ldv $v08, 0, 48, $t0
  ldv $v01, 0, 0, $at
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)
  ldv $v06, 0, 40, $t0
  ldv $v02, 0, 8, $at
  ldv $v10, 0, 56, $t0
  ldv $v01, 8, 0, $at
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)
  ldv $v04, 0, 32, $t0
  ldv $v02, 8, 8, $at
  addu $t1, $s4, $t8
  ldv $v05, 0, 8, $t0
  ldv $v10, 8, 56, $t0
  ldv $v04, 8, 32, $t0
  LOAD_POS_NORM310: ldv $v28, 0, 0, $s4
  ori $at, $zero, %lo(MATRICES_NORMAL)
  ldv $v09, 8, 24, $t0
  ori $t4, $zero, %lo(MG_CLIP_CACHE)
  ldv $v05, 8, 8, $t0
  ori $t3, $zero, %lo(MG_CLIP_CACHE)
  LOAD_POS_NORM311: ldv $v28, 8, 0, $t1
  ldv $v13, 0, 16, $at
  ldv $v08, 8, 48, $t0
  ldv $v07, 8, 16, $t0
  ldv $v13, 8, 16, $at
  ldv $v06, 8, 40, $t0
  ldv $v03, 8, 0, $t0
  ldv $v12, 0, 8, $at
  lbu $t0, %lo(FOG_MASK + 0)
  ldv $v12, 8, 8, $at
  addiu $t2, $s7, 40
  LOAD_POS_NORM309: ldv $v16, 8, 0, $t1
  ldv $v11, 0, 0, $at
  ldv $v11, 8, 0, $at
  sll $t9, $t8, 1
  VERTEX_LOOP:
  vmov $v28.e3, $v30.e2
  LOAD_COLOR335: llv $v19, 0, 0, $s4 ## Barrier: 0x3F00
  vand $v16, $v01, $v16.h3
  vmov $v28.e7, $v30.e2
  suv $v15, 0, 8, $t3 ## Barrier: 0x40
  slv $v14, 0, 12, $t3 ## Barrier: 0x40
  LOAD_COLOR338: llv $v19, 4, 0, $t1 ## Barrier: 0x3F00
  vmudn $v16, $v16, $v02.v
  vmudn $v29, $v04, $v28.h0
  sdv $v17, 0, 0, $t3 ## Barrier: 0xC
  vmadh $v29, $v03, $v28.h0
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x4000
  sb $t7, 7($t3) ## Barrier: 0x8
  vmadn $v29, $v08, $v28.h2
  sdv $v19, 0, 0, $at ## Barrier: 0x4000
  vmadh $v29, $v07, $v28.h2
  vmadn $v29, $v06, $v28.h1
  luv $v19, 0, 0, $at ## Barrier: 0x4000
  ori $at, $zero, %lo(MG_CLIP_FACTORS)
  ldv $v20, 0, 0, $at
  vmadh $v29, $v05, $v28.h1
  vmadn $v18, $v10, $v28.h3
  sdv $v17, 8, 0, $t4 ## Barrier: 0x30
  ldv $v20, 8, 0, $at
  vmadh $v17, $v09, $v28.h3
  sb $t5, 6($t3) ## Barrier: 0x4
  vmulf $v29, $v11, $v16.h0
  vmacf $v29, $v12, $v16.h1
  vmacf $v16, $v13, $v16.h2
  suv $v15, 4, 8, $t4 ## Barrier: 0x80
  vmudn $v22, $v18, $v20.v
  sb $t6, 6($t4) ## Barrier: 0x10
  vmadh $v21, $v17, $v20.v
  ori $at, $zero, %lo(MG_VIEWPORT)
  sdv $v17, 0, 24, $s7 ## Barrier: 0x100
  sdv $v17, 8, 24, $t2 ## Barrier: 0x200
  ldv $v20, 0, 0, $at
  sdv $v18, 8, 32, $t2 ## Barrier: 0x200
  vch $v29, $v21, $v21.h3
  vcl $v29, $v22, $v22.h3
  cfc2 $k1, $vcc
  vch $v29, $v17, $v17.h3
  ldv $v21, 0, 8, $at
  ldv $v20, 8, 0, $at
  andi $fp, $k1, 1799
  srl $s0, $fp, 5
  vcl $v29, $v18, $v18.h3
  srl $sp, $k1, 4
  or $t5, $fp, $s0
  andi $fp, $sp, 1799
  sdv $v18, 0, 32, $s7 ## Barrier: 0x100
  vmudl $v18, $v18, $v20.h3
  ldv $v21, 8, 8, $at
  cfc2 $sp, $vcc
  vmadm $v17, $v17, $v20.h3
  srl $s0, $fp, 5
  or $t6, $fp, $s0
  sb $k0, 7($t4) ## Barrier: 0x20
  vmadn $v18, $v00, $v00
  ssv $v17, 14, 16, $t2 ## Barrier: 0x800
  ssv $v17, 6, 16, $s7 ## Barrier: 0x400
  ori $at, $zero, %lo(FOG_PARAMS)
  vrcph $v17.e3, $v17.e3
  ssv $v18, 6, 18, $s7 ## Barrier: 0x400
  slv $v14, 8, 12, $t4 ## Barrier: 0x80
  ssv $v18, 14, 18, $t2 ## Barrier: 0x800
  vrcpl $v18.e3, $v18.e3
  llv $v24, 0, 4, $at
  andi $s0, $sp, 1799
  vsubc $v22, $v00, $v17.h2
  srl $s1, $s0, 5
  vrcph $v17.e3, $v17.e7
  vrcpl $v18.e7, $v18.e7
  llv $v24, 8, 4, $at
  llv $v23, 0, 0, $at
  ori $k1, $zero, %lo(MATRICES_MV)
  vrcph $v17.e7, $v00.e7
  nor $t7, $s0, $s1
  ldv $v03, 0, 0, $k1
  srl $fp, $sp, 4
  llv $v23, 8, 0, $at
  vge $v22, $v22, $v17.h2
  ssv $v17, 6, 20, $s7 ## Barrier: 0x1000
  ldv $v04, 0, 32, $k1
  ldv $v05, 0, 8, $k1
  andi $s0, $fp, 1799
  vsubc $v22, $v22, $v23.h1
  ldv $v09, 0, 24, $k1
  ssv $v17, 14, 20, $t2 ## Barrier: 0x2000
  ssv $v18, 6, 22, $s7 ## Barrier: 0x1000
  ssv $v18, 14, 22, $t2 ## Barrier: 0x2000
  vmudm $v29, $v22, $v24.h0
  ldv $v04, 8, 32, $k1
  vmadh $v22, $v22, $v23.h0
  ldv $v08, 0, 48, $k1
  ldv $v07, 0, 16, $k1
  ldv $v08, 8, 48, $k1
  vge $v22, $v00, $v22.h0
  ori $at, $zero, %lo(LIGHTING_AMBIENT)
  vmudl $v29, $v18, $v18.h3
  ldv $v06, 0, 40, $k1
  vmadm $v29, $v17, $v18.h3
  srl $s1, $s0, 5
  vmadn $v18, $v18, $v17.h3
  vmadh $v17, $v17, $v17.h3
  ldv $v03, 8, 0, $k1
  ldv $v05, 8, 8, $k1
  ldv $v06, 8, 40, $k1
  ldv $v07, 8, 16, $k1
  vmudh $v29, $v21, $v30.e7
  ldv $v10, 0, 56, $k1
  vmadn $v18, $v18, $v20.v
  vmadh $v17, $v17, $v20.v
  ldv $v09, 8, 24, $k1
  ldv $v15, 0, 0, $at
  vmudn $v29, $v04, $v28.h0
  vmadh $v29, $v03, $v28.h0
  nor $k0, $s0, $s1
  ldv $v10, 8, 56, $k1
  vmadn $v29, $v06, $v28.h1
  vmadh $v29, $v05, $v28.h1
  vmadn $v29, $v08, $v28.h2
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)
  ldv $v15, 8, 0, $at
  vmadh $v29, $v07, $v28.h2
  vmadn $v29, $v10, $v28.h3
  lw $k1, %lo(LIGHTING_COUNT + 0)
  vmadh $v20, $v09, $v28.h3
  LIGHT_LOOP:
  ldv $v23, 0, 0, $sp
  ldv $v23, 8, 0, $sp
  ldv $v21, 0, 8, $sp
  ldv $v21, 8, 8, $sp
  vmulf $v23, $v23, $v16.v
  vmulf $v29, $v21, $v23.h0
  vmacf $v29, $v21, $v23.h1
  addiu $sp, $sp, 32
  addiu $k1, $k1, 65535
  vmacf $v21, $v21, $v23.h2
  vge $v21, $v21, $v00
  bne $k1, $zero, LIGHT_LOOP
  vadd $v15, $v15, $v21.v
  ori $k1, $zero, %lo(MATRICES_MVP)
  ldv $v06, 0, 40, $k1
  ctc2 $t0, $vcc
  ldv $v04, 8, 32, $k1
  PATCH_COLOR510: vmulf $v15, $v15, $v19.v
  vmrg $v15, $v22, $v15
  vmudh $v24, $v20, $v20.v
  vsar $v24, COP2_ACC_MD
  or $t4, $zero, $t2
  vsar $v23, COP2_ACC_HI
  vaddc $v26, $v24, $v24.h1
  ldv $v06, 8, 40, $k1
  vadd $v25, $v23, $v23.h1
  ldv $v05, 0, 8, $k1
  ldv $v03, 8, 0, $k1
  ldv $v07, 8, 16, $k1
  vaddc $v24, $v26, $v24.h2
  addu $t1, $t1, $t9
  vadd $v23, $v25, $v23.h2
  vrsqh $v23.e0, $v23.e0
  ldv $v07, 0, 16, $k1
  ldv $v09, 0, 24, $k1
  vrsql $v24.e0, $v24.e0
  vrsqh $v23.e0, $v23.e4
  ldv $v09, 8, 24, $k1
  ldv $v08, 0, 48, $k1
  vrsql $v24.e4, $v24.e4
  vrsqh $v23.e4, $v00.e0
  vmudm $v29, $v20, $v24.h0
  vmadh $v20, $v20, $v23.h0
  ldv $v03, 0, 0, $k1
  ldv $v10, 0, 56, $k1
  vmulf $v21, $v20, $v16.v
  ldv $v08, 8, 48, $k1
  ldv $v04, 0, 32, $k1
  ldv $v05, 8, 8, $k1
  vadd $v25, $v21, $v21.h1
  ldv $v10, 8, 56, $k1
  vadd $v21, $v25, $v21.h2
  addiu $k1, $zero, 32767
  vlt $v21, $v21, $v00
  sh $k1, %lo(RSPQ_SCRATCH_MEM)($zero) ## Barrier: 0x1C000
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x8000
  vor $v25, $v00, $v00
  lsv $v25, 4, 0, $at ## Barrier: 0x8000
  lsv $v25, 12, 0, $at ## Barrier: 0x10000
  vsubc $v21, $v00, $v21.v
  vmulf $v29, $v16, $v21.h0
  vmacf $v29, $v16, $v21.h0
  vmadh $v29, $v20, $v30.e7
  ori $at, $zero, %lo(TEXTURING_SCALE)
  vmadh $v29, $v25, $v30.e7
  vsar $v26, COP2_ACC_MD
  or $t3, $zero, $s7
  vsar $v25, COP2_ACC_HI
  vmudl $v29, $v26, $v26.v
  llv $v21, 0, 0, $at
  addu $s4, $s4, $t9
  vmadm $v29, $v25, $v26.v
  vmadn $v24, $v26, $v25.v
  vmadh $v23, $v25, $v25.v
  vaddc $v28, $v24, $v24.h1
  llv $v21, 8, 0, $at
  vadd $v27, $v23, $v23.h1
  addiu $s7, $s7, 80
  vaddc $v24, $v28, $v24.h2
  vadd $v23, $v27, $v23.h2
  vrsqh $v23.e0, $v23.e0
  vrsql $v24.e0, $v24.e0
  vrsqh $v23.e0, $v23.e4
  vrsql $v24.e4, $v24.e4
  vrsqh $v23.e4, $v00.e0
  LOAD_POS_NORM596: ldv $v28, 0, 0, $s4
  LOAD_POS_NORM594: ldv $v16, 0, 0, $s4
  vxor $v14, $v00, $v30.e0
  LOAD_POS_NORM597: ldv $v28, 8, 0, $t1
  addiu $t2, $t2, 80
  vmudh $v29, $v14, $v30.e7
  LOAD_POS_NORM595: ldv $v16, 8, 0, $t1
  vmadl $v29, $v26, $v24.h0
  vmadm $v29, $v25, $v24.h0
  ori $at, $zero, %lo(TEXTURING_OFFSET)
  vmadn $v29, $v26, $v23.h0
  vmadh $v14, $v25, $v23.h0
  llv $v23, 0, 0, $at
  llv $v23, 8, 0, $at
  vmudh $v29, $v23, $v30.e7
  sltu $at, $s4, $s6
  bne $at, $zero, VERTEX_LOOP
  vmadh $v14, $v14, $v21.v
  sdv $v17, 8, 0, $t4 ## Barrier: 0x30
  sb $t6, 6($t4) ## Barrier: 0x10
  sdv $v17, 0, 0, $t3 ## Barrier: 0xC
  suv $v15, 4, 8, $t4 ## Barrier: 0x80
  suv $v15, 0, 8, $t3 ## Barrier: 0x40
  sb $t5, 6($t3) ## Barrier: 0x4
  slv $v14, 0, 12, $t3 ## Barrier: 0x40
  sb $k0, 7($t4) ## Barrier: 0x20
  sb $t7, 7($t3) ## Barrier: 0x8
  j RSPQ_Loop
  slv $v14, 8, 12, $t4 ## Barrier: 0x80

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