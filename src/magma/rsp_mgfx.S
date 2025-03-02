## Auto-generated file, transpiled with RSPL
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
  MgBeginUniform MATRICES, 0
    .align 4
    MATRICES_MVP: .ds.b 64
    .align 4
    MATRICES_MV: .ds.b 64
    .align 4
    MATRICES_NORMAL: .ds.b 32
  MgEndUniform

  MgBeginUniform TEXTURING, 1
    .align 1
    TEXTURING_SCALE: .ds.b 4
    .align 1
    TEXTURING_OFFSET: .ds.b 4
  MgEndUniform

  MgBeginUniform LIGHTING, 2
    LIGHTING_LIGHTS: .ds.b 128
    .align 1
    LIGHTING_AMBIENT: .ds.b 6
    LIGHTING_HAS_POINTS: .ds.b 1
    LIGHTING_COUNT: .ds.b 1
  MgEndUniform

  MgBeginUniform FOG, 3
    .align 1
    FOG_PARAMS: .ds.b 6
    FOG_MASK: .ds.b 1
  MgEndUniform

MgEndShaderUniforms

MgBeginVertexInput
  MgBeginVertexAttribute 0, 0
    MgVertexAttributeLoaders LOAD_POS_NORM298, LOAD_POS_NORM299, LOAD_POS_NORM300, LOAD_POS_NORM301, LOAD_POS_NORM568, LOAD_POS_NORM570, LOAD_POS_NORM571, LOAD_POS_NORM569
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR325, LOAD_COLOR328
    MgBeginVertexAttributePatch PATCH_COLOR499
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD335, LOAD_TEXCOORD338
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
T3DCmd_VertLoad:
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6
  ori $at, $zero, %lo(MG_VERTEX_SIZE)
  ldv $v02, 0, 0, $t0
  lsv $v02, 6, 4, $t0
  ldv $v01, 0, 0, $at
  vmudh $v04, $v02, $v01.v
  vsar $v04, COP2_ACC_MD
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)
  vsar $v03, COP2_ACC_HI
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x1
  sdv $v04, 0, 0, $at ## Barrier: 0x1
  ssv $v03, 0, 8, $at ## Barrier: 0x2
  ssv $v04, 0, 10, $at ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8) ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2) ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 6) ## Barrier: 0x1
  lh $t4, %lo(RSPQ_SCRATCH_MEM + 4) ## Barrier: 0x1
  andi $t3, $t2, 7
  addu $t0, $t1, $t3
  addiu $t0, $t0, 7
  addiu $s4, $t4, %lo(MG_VERTEX_CACHE_END)
  andi $t0, $t0, 4088
  addiu $t0, $t0, -1
  addu $s0, $s0, $t2
  addiu $t2, $zero, 12
  jal DMAExec ## Args: $t0, $t1, $s0, $s4, $t2
  andi $s4, $s4, 4088
  ori $at, $zero, %lo(MATRICES_NORMAL)
  ldv $v09, 0, 0, $at
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)
  ori $t0, $zero, %lo(MATRICES_MVP)
  ldv $v08, 0, 56, $t0
  addu $t2, $s4, $t8
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)
  ldv $v06, 0, 48, $t0
  ldv $v01, 0, 0, $t0
  ldv $v09, 8, 0, $at
  LOAD_POS_NORM298: ldv $v22, 0, 0, $s4
  ldv $v11, 0, 16, $at
  ldv $v10, 0, 8, $at
  ldv $v10, 8, 8, $at
  ldv $v11, 8, 16, $at
  ori $at, $zero, %lo(MG_NORMAL_MASK)
  addiu $t3, $s7, 40
  ldv $v07, 0, 24, $t0
  ldv $v12, 0, 0, $at
  sll $t9, $t8, 1
  ldv $v13, 0, 8, $at
  LOAD_POS_NORM299: ldv $v22, 8, 0, $t2
  addu $s6, $s4, $t1
  ldv $v13, 8, 8, $at
  ldv $v12, 8, 0, $at
  ori $at, $zero, %lo(MG_VIEWPORT)
  ldv $v04, 0, 40, $t0
  ldv $v08, 8, 56, $t0
  ldv $v05, 0, 16, $t0
  ldv $v03, 0, 8, $t0
  ldv $v14, 0, 0, $at
  LOAD_POS_NORM300: ldv $v21, 0, 0, $s4
  lbu $t1, %lo(FOG_MASK + 0)
  ldv $v14, 8, 0, $at
  ori $t4, $zero, %lo(MG_CLIP_CACHE)
  ldv $v15, 0, 8, $at
  LOAD_POS_NORM301: ldv $v21, 8, 0, $t2
  ldv $v05, 8, 16, $t0
  ldv $v15, 8, 8, $at
  ori $at, $zero, %lo(TEXTURING_SCALE)
  llv $v16, 0, 0, $at
  ldv $v03, 8, 8, $t0
  llv $v16, 8, 0, $at
  ldv $v04, 8, 40, $t0
  ori $at, $zero, %lo(TEXTURING_OFFSET)
  ldv $v02, 0, 32, $t0
  llv $v17, 0, 0, $at
  llv $v17, 8, 0, $at
  ldv $v02, 8, 32, $t0
  ldv $v06, 8, 48, $t0
  ldv $v07, 8, 24, $t0
  ori $at, $zero, %lo(FOG_PARAMS)
  ldv $v01, 8, 0, $t0
  ldv $v18, 0, 0, $at
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)
  ldv $v18, 8, 0, $at
  ori $t5, $zero, %lo(MG_CLIP_CACHE)
  VERTEX_LOOP:
  vand $v22, $v12, $v22.h3
  vmov $v21.e3, $v30.e2
  LOAD_COLOR325: llv $v25, 0, 0, $s4 ## Barrier: 0x3F00
  vmov $v21.e7, $v30.e2
  suv $v20, 0, 8, $t4 ## Barrier: 0x40
  suv $v20, 4, 8, $t5 ## Barrier: 0x80
  slv $v19, 0, 12, $t4 ## Barrier: 0x40
  vmudn $v22, $v22, $v13.v
  vmudn $v29, $v02, $v21.h0
  LOAD_COLOR328: llv $v25, 4, 0, $t2 ## Barrier: 0x3F00
  sdv $v23, 0, 0, $t4 ## Barrier: 0xC
  vmadh $v29, $v01, $v21.h0
  vmadn $v29, $v06, $v21.h2
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x4000
  vmadh $v29, $v05, $v21.h2
  sdv $v23, 8, 0, $t5 ## Barrier: 0x30
  slv $v19, 8, 12, $t5 ## Barrier: 0x80
  vmadn $v29, $v04, $v21.h1
  vmadh $v29, $v03, $v21.h1
  sdv $v25, 0, 0, $at ## Barrier: 0x4000
  luv $v25, 0, 0, $at ## Barrier: 0x4000
  vmadn $v24, $v08, $v21.h3
  vmadh $v23, $v07, $v21.h3
  ori $at, $zero, %lo(MG_CLIP_FACTORS)
  vmulf $v29, $v09, $v22.h0
  LOAD_TEXCOORD335: llv $v19, 0, 0, $s4 ## Barrier: 0x3F00
  ldv $v26, 0, 0, $at
  vmacf $v29, $v10, $v22.h1
  ldv $v26, 8, 0, $at
  sb $k0, 7($t4) ## Barrier: 0x8
  vmacf $v22, $v11, $v22.h2
  sb $t6, 6($t4) ## Barrier: 0x4
  vmudn $v28, $v24, $v26.v
  LOAD_TEXCOORD338: llv $v19, 8, 0, $t2 ## Barrier: 0x3F00
  vmadh $v27, $v23, $v26.v
  sdv $v23, 8, 24, $t3 ## Barrier: 0x200
  sdv $v23, 0, 24, $s7 ## Barrier: 0x100
  sb $t7, 6($t5) ## Barrier: 0x10
  vch $v29, $v27, $v27.h3
  vcl $v29, $v28, $v28.h3
  sdv $v24, 0, 32, $s7 ## Barrier: 0x100
  cfc2 $sp, $vcc
  vch $v29, $v23, $v23.h3
  vcl $v29, $v24, $v24.h3
  sdv $v24, 8, 32, $t3 ## Barrier: 0x200
  vaddc $v27, $v24, $v18.h1
  vadd $v26, $v23, $v18.h0
  srl $fp, $sp, 4
  andi $s0, $sp, 1799
  srl $s1, $s0, 5
  or $t6, $s0, $s1
  vmudn $v29, $v27, $v18.h2
  vmadh $v26, $v26, $v18.h2
  andi $s0, $fp, 1799
  cfc2 $fp, $vcc
  srl $s1, $s0, 5
  or $t7, $s0, $s1
  andi $s1, $fp, 1799
  vge $v26, $v00, $v26.h2
  srl $s2, $s1, 5
  vmudl $v24, $v24, $v14.h3
  srl $s0, $fp, 4
  vmadm $v23, $v23, $v14.h3
  nor $k0, $s1, $s2
  vmadn $v24, $v00, $v00
  vrcph $v27.e3, $v23.e3
  sb $k1, 7($t5) ## Barrier: 0x20
  vrcpl $v28.e3, $v24.e3
  vrcph $v27.e3, $v23.e7
  ssv $v23, 6, 16, $s7 ## Barrier: 0x400
  andi $s1, $s0, 1799
  vrcpl $v28.e7, $v24.e7
  vrcph $v27.e7, $v00.e7
  ssv $v23, 14, 16, $t3 ## Barrier: 0x800
  ssv $v24, 14, 18, $t3 ## Barrier: 0x800
  ssv $v24, 6, 18, $s7 ## Barrier: 0x400
  srl $s2, $s1, 5
  vmudl $v29, $v24, $v28.h3
  ssv $v27, 14, 20, $t3 ## Barrier: 0x2000
  vmadm $v29, $v23, $v28.h3
  ssv $v27, 6, 20, $s7 ## Barrier: 0x1000
  vmadn $v24, $v24, $v27.h3
  vmadh $v23, $v23, $v27.h3
  ssv $v28, 6, 22, $s7 ## Barrier: 0x1000
  ssv $v28, 14, 22, $t3 ## Barrier: 0x2000
  nor $k1, $s1, $s2
  vmudh $v29, $v15, $v30.e7
  vmadn $v24, $v24, $v14.v
  beq $t0, $zero, LABEL_0001
  vmadh $v23, $v23, $v14.v
  ori $sp, $zero, %lo(MATRICES_MV)
  ldv $v02, 0, 32, $sp
  ldv $v01, 0, 0, $sp
  ldv $v08, 0, 56, $sp
  ldv $v02, 8, 32, $sp
  ldv $v03, 0, 8, $sp
  ldv $v01, 8, 0, $sp
  ldv $v06, 0, 48, $sp
  ldv $v04, 0, 40, $sp
  ldv $v04, 8, 40, $sp
  ldv $v03, 8, 8, $sp
  vmudn $v29, $v02, $v21.h0
  ldv $v06, 8, 48, $sp
  ldv $v05, 0, 16, $sp
  ldv $v05, 8, 16, $sp
  ldv $v07, 0, 24, $sp
  vmadh $v29, $v01, $v21.h0
  ldv $v08, 8, 56, $sp
  vmadn $v29, $v04, $v21.h1
  vmadh $v29, $v03, $v21.h1
  ldv $v07, 8, 24, $sp
  vmadn $v29, $v06, $v21.h2
  vmadh $v29, $v05, $v21.h2
  vmadn $v29, $v08, $v21.h3
  vmadh $v21, $v07, $v21.h3
  LABEL_0001:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)
  lbu $sp, %lo(LIGHTING_COUNT + 0)
  ldv $v20, 0, 0, $at
  ori $fp, $zero, %lo(LIGHTING_LIGHTS)
  beq $sp, $zero, LIGHT_END
  ldv $v20, 8, 0, $at
  LIGHT_LOOP:
  ldv $v27, 0, 8, $fp
  lh $s0, 6($fp)
  ldv $v28, 0, 0, $fp
  ldv $v28, 8, 0, $fp
  beq $s0, $zero, LABEL_0003
  ldv $v27, 8, 8, $fp
  vsubc $v28, $v28, $v21.v
  vmudh $v02, $v28, $v28.v
  vsar $v02, COP2_ACC_MD
  vsar $v01, COP2_ACC_HI
  ori $s1, $zero, %lo(MATRICES_MVP)
  vaddc $v04, $v02, $v02.h1
  vadd $v03, $v01, $v01.h1
  ldv $v06, 8, 48, $s1
  vaddc $v02, $v04, $v02.h2
  vadd $v01, $v03, $v01.h2
  vrsqh $v03.e0, $v01.e0
  vrsql $v04.e0, $v02.e0
  vrsqh $v03.e0, $v01.e4
  ldv $v07, 8, 24, $s1
  vrsql $v04.e4, $v02.e4
  ldv $v08, 8, 56, $s1
  vrsqh $v03.e4, $v00.e0
  ldv $v02, 0, 32, $s1
  ldv $v01, 0, 0, $s1
  ldv $v07, 0, 24, $s1
  ldv $v06, 0, 48, $s1
  vmudm $v29, $v28, $v04.h0
  vmadh $v28, $v28, $v03.h0
  vmudn $v29, $v04, $v27.h3
  ldv $v02, 8, 32, $s1
  vmadh $v05, $v03, $v27.h3
  ldv $v08, 0, 56, $s1
  ldv $v03, 8, 8, $s1
  ldv $v04, 0, 40, $s1
  vmulf $v05, $v05, $v05.v
  ldv $v01, 8, 0, $s1
  ldv $v03, 0, 8, $s1
  ldv $v04, 8, 40, $s1
  vmulf $v27, $v27, $v05.h0
  ldv $v05, 0, 16, $s1
  ldv $v05, 8, 16, $s1
  LABEL_0003:
  vmulf $v28, $v28, $v22.v
  addiu $fp, $fp, 16
  vmulf $v29, $v27, $v28.h0
  vmacf $v29, $v27, $v28.h1
  vmacf $v27, $v27, $v28.h2
  addiu $sp, $sp, 65535
  vge $v27, $v27, $v00
  bne $sp, $zero, LIGHT_LOOP
  vadd $v20, $v20, $v27.v
  LIGHT_END:
  addu $t2, $t2, $t9
  vmov $v20.e3, $v12.e3
  vmov $v20.e7, $v12.e3
  or $t4, $zero, $s7
  addu $s4, $s4, $t9
  sltu $at, $s4, $s6
  ctc2 $t1, $vcc
  or $t5, $zero, $t3
  PATCH_COLOR499: vmulf $v20, $v20, $v25.v
  LOAD_POS_NORM568: ldv $v22, 0, 0, $s4
  LOAD_POS_NORM570: ldv $v21, 0, 0, $s4
  LOAD_POS_NORM571: ldv $v21, 8, 0, $t2
  addiu $t3, $t3, 80
  vmrg $v20, $v20, $v26.h2
  vmudh $v29, $v17, $v30.e7
  LOAD_POS_NORM569: ldv $v22, 8, 0, $t2
  vmadh $v19, $v19, $v16.v
  bne $at, $zero, VERTEX_LOOP
  addiu $s7, $s7, 80
  sdv $v23, 0, 0, $t4 ## Barrier: 0xC
  suv $v20, 4, 8, $t5 ## Barrier: 0x80
  sdv $v23, 8, 0, $t5 ## Barrier: 0x30
  slv $v19, 8, 12, $t5 ## Barrier: 0x80
  suv $v20, 0, 8, $t4 ## Barrier: 0x40
  sb $k0, 7($t4) ## Barrier: 0x8
  slv $v19, 0, 12, $t4 ## Barrier: 0x40
  sb $t6, 6($t4) ## Barrier: 0x4
  sb $t7, 6($t5) ## Barrier: 0x10
  j RSPQ_Loop
  sb $k1, 7($t5) ## Barrier: 0x20

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