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
    MgVertexAttributeLoaders LOAD_POS_NORM300, LOAD_POS_NORM298, LOAD_POS_NORM301, LOAD_POS_NORM299, LOAD_POS_NORM565, LOAD_POS_NORM563, LOAD_POS_NORM562, LOAD_POS_NORM564
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR302, LOAD_COLOR303, LOAD_COLOR567, LOAD_COLOR566
    MgBeginVertexAttributePatch PATCH_COLOR494
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
T3DCmd_VertLoad:
  ori $at, $zero, %lo(MG_VERTEX_SIZE)
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6
  ldv $v02, 0, 0, $t0
  ldv $v01, 0, 0, $at
  lsv $v02, 6, 4, $t0
  vmudh $v04, $v02, $v01.v
  vsar $v04, COP2_ACC_MD
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x1
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)
  vsar $v03, COP2_ACC_HI
  sdv $v04, 0, 0, $at ## Barrier: 0x1
  ssv $v03, 0, 8, $at ## Barrier: 0x2
  ssv $v04, 0, 10, $at ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2) ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8) ## Barrier: 0x2
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 6) ## Barrier: 0x1
  lh $t4, %lo(RSPQ_SCRATCH_MEM + 4) ## Barrier: 0x1
  andi $t3, $t2, 7
  addu $s0, $s0, $t2
  addu $t0, $t1, $t3
  addiu $t0, $t0, 7
  addiu $s4, $t4, %lo(MG_VERTEX_CACHE_END)
  andi $t0, $t0, 4088
  addiu $t2, $zero, 12
  andi $s4, $s4, 4088
  jal DMAExec ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1
  ori $t3, $zero, %lo(MG_CLIP_CACHE)
  ori $t4, $zero, %lo(MG_CLIP_CACHE)
  ori $at, $zero, %lo(MATRICES_NORMAL)
  ldv $v09, 0, 0, $at
  ori $t0, $zero, %lo(MATRICES_MVP)
  ldv $v09, 8, 0, $at
  ldv $v11, 0, 16, $at
  ldv $v10, 0, 8, $at
  LOAD_POS_NORM300: ldv $v15, 0, 0, $s4
  ldv $v03, 0, 8, $t0
  ldv $v07, 0, 24, $t0
  ldv $v11, 8, 16, $at
  ldv $v04, 0, 40, $t0
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)
  ldv $v02, 0, 32, $t0
  addu $s6, $s4, $t1
  LOAD_COLOR302: llv $v19, 0, 0, $s4
  LOAD_POS_NORM298: ldv $v16, 0, 0, $s4
  ldv $v10, 8, 8, $at
  ldv $v06, 0, 48, $t0
  ldv $v05, 0, 16, $t0
  ori $at, $zero, %lo(MG_NORMAL_MASK)
  addu $t1, $s4, $t8
  ldv $v13, 0, 8, $at
  LOAD_POS_NORM301: ldv $v15, 8, 0, $t1
  ldv $v02, 8, 32, $t0
  ldv $v12, 0, 0, $at
  ldv $v13, 8, 8, $at
  ldv $v07, 8, 24, $t0
  ldv $v05, 8, 16, $t0
  LOAD_POS_NORM299: ldv $v16, 8, 0, $t1
  ldv $v03, 8, 8, $t0
  ldv $v12, 8, 0, $at
  ldv $v01, 0, 0, $t0
  ldv $v06, 8, 48, $t0
  ori $at, $zero, %lo(FOG_PARAMS)
  ldv $v01, 8, 0, $t0
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)
  ldv $v08, 0, 56, $t0
  addiu $t2, $s7, 40
  ldv $v08, 8, 56, $t0
  ldv $v14, 0, 0, $at
  sll $t9, $t8, 1
  ldv $v14, 8, 0, $at
  ldv $v04, 8, 40, $t0
  lbu $t0, %lo(FOG_MASK + 0)
  LOAD_COLOR303: llv $v19, 4, 0, $t1
  VERTEX_LOOP:
  vmov $v15.e3, $v30.e2
  vand $v16, $v12, $v16.h3
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x100
  vmov $v15.e7, $v30.e2
  vmudn $v16, $v16, $v13.v
  sdv $v20, 0, 0, $t3 ## Barrier: 0xC
  sdv $v19, 0, 0, $at ## Barrier: 0x100
  vmudn $v29, $v02, $v15.h0
  vmadh $v29, $v01, $v15.h0
  sdv $v20, 8, 0, $t4 ## Barrier: 0x30
  vmadn $v29, $v06, $v15.h2
  sb $t5, 6($t3) ## Barrier: 0x4
  luv $v19, 0, 0, $at ## Barrier: 0x100
  vmadh $v29, $v05, $v15.h2
  vmadn $v29, $v04, $v15.h1
  ori $at, $zero, %lo(MG_CLIP_FACTORS)
  vmadh $v29, $v03, $v15.h1
  ldv $v22, 0, 0, $at
  sb $t7, 7($t3) ## Barrier: 0x8
  vmadn $v21, $v08, $v15.h3
  ldv $v22, 8, 0, $at
  vmadh $v20, $v07, $v15.h3
  vmulf $v29, $v09, $v16.h0
  vmacf $v29, $v10, $v16.h1
  ori $at, $zero, %lo(MG_VIEWPORT)
  vmacf $v16, $v11, $v16.h2
  suv $v18, 0, 8, $t3 ## Barrier: 0x40
  vmudn $v24, $v21, $v22.v
  suv $v18, 4, 8, $t4 ## Barrier: 0x80
  slv $v17, 0, 12, $t3 ## Barrier: 0x40
  vmadh $v23, $v20, $v22.v
  sb $t6, 6($t4) ## Barrier: 0x10
  sdv $v20, 8, 24, $t2 ## Barrier: 0x400
  vch $v29, $v23, $v23.h3
  vcl $v29, $v24, $v24.h3
  sdv $v20, 0, 24, $s7 ## Barrier: 0x200
  cfc2 $k1, $vcc
  vch $v29, $v20, $v20.h3
  sdv $v21, 0, 32, $s7 ## Barrier: 0x200
  andi $fp, $k1, 1799
  vcl $v29, $v21, $v21.h3
  srl $s0, $fp, 5
  sdv $v21, 8, 32, $t2 ## Barrier: 0x400
  vaddc $v23, $v21, $v14.h1
  srl $sp, $k1, 4
  ldv $v24, 0, 8, $at
  or $t5, $fp, $s0
  vadd $v22, $v20, $v14.h0
  andi $fp, $sp, 1799
  cfc2 $sp, $vcc
  srl $s0, $fp, 5
  vmudn $v29, $v23, $v14.h2
  or $t6, $fp, $s0
  vmadh $v22, $v22, $v14.h2
  ldv $v23, 0, 0, $at
  andi $s0, $sp, 1799
  ldv $v23, 8, 0, $at
  srl $s1, $s0, 5
  ldv $v24, 8, 8, $at
  nor $t7, $s0, $s1
  ori $k1, $zero, %lo(MATRICES_MV)
  vge $v22, $v00, $v22.h2
  ldv $v03, 0, 8, $k1
  vmudl $v21, $v21, $v23.h3
  ldv $v02, 0, 32, $k1
  ldv $v01, 0, 0, $k1
  vmadm $v20, $v20, $v23.h3
  vmadn $v21, $v00, $v00
  srl $fp, $sp, 4
  ldv $v07, 0, 24, $k1
  ori $at, $zero, %lo(LIGHTING_AMBIENT)
  ldv $v02, 8, 32, $k1
  ssv $v20, 14, 16, $t2 ## Barrier: 0x1000
  ldv $v01, 8, 0, $k1
  andi $s0, $fp, 1799
  ldv $v06, 0, 48, $k1
  vrcph $v25.e3, $v20.e3
  ssv $v20, 6, 16, $s7 ## Barrier: 0x800
  vrcpl $v26.e3, $v21.e3
  vrcph $v25.e3, $v20.e7
  ldv $v03, 8, 8, $k1
  ldv $v07, 8, 24, $k1
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)
  vrcpl $v26.e7, $v21.e7
  ldv $v04, 0, 40, $k1
  vrcph $v25.e7, $v00.e7
  ssv $v21, 6, 18, $s7 ## Barrier: 0x800
  ldv $v05, 0, 16, $k1
  sb $k0, 7($t4) ## Barrier: 0x20
  vmudl $v29, $v21, $v26.h3
  ssv $v21, 14, 18, $t2 ## Barrier: 0x1000
  vmadm $v29, $v20, $v26.h3
  slv $v17, 8, 12, $t4 ## Barrier: 0x80
  vmadn $v21, $v21, $v25.h3
  vmadh $v20, $v20, $v25.h3
  ssv $v25, 6, 20, $s7 ## Barrier: 0x2000
  ldv $v05, 8, 16, $k1
  vmudh $v29, $v24, $v30.e7
  ldv $v08, 0, 56, $k1
  ldv $v04, 8, 40, $k1
  vmadn $v21, $v21, $v23.v
  ldv $v06, 8, 48, $k1
  vmadh $v20, $v20, $v23.v
  vmudn $v29, $v02, $v15.h0
  ldv $v18, 0, 0, $at
  srl $s1, $s0, 5
  vmadh $v29, $v01, $v15.h0
  vmadn $v29, $v04, $v15.h1
  ldv $v08, 8, 56, $k1
  vmadh $v29, $v03, $v15.h1
  ssv $v25, 14, 20, $t2 ## Barrier: 0x4000
  vmadn $v29, $v06, $v15.h2
  lbu $k1, %lo(LIGHTING_COUNT + 0)
  vmadh $v29, $v05, $v15.h2
  ssv $v26, 6, 22, $s7 ## Barrier: 0x2000
  nor $k0, $s0, $s1
  vmadn $v29, $v08, $v15.h3
  vmadh $v15, $v07, $v15.h3
  ssv $v26, 14, 22, $t2 ## Barrier: 0x4000
  beq $k1, $zero, LIGHT_END
  ldv $v18, 8, 0, $at
  LIGHT_LOOP:
  lh $fp, 6($sp)
  ldv $v24, 0, 0, $sp
  ldv $v24, 8, 0, $sp
  ldv $v23, 0, 8, $sp
  beq $fp, $zero, LABEL_0002
  ldv $v23, 8, 8, $sp
  vsubc $v24, $v24, $v15.v
  vmudh $v02, $v24, $v24.v
  vsar $v02, COP2_ACC_MD
  vsar $v01, COP2_ACC_HI
  vaddc $v04, $v02, $v02.h1
  ori $s0, $zero, %lo(MATRICES_MVP)
  vadd $v03, $v01, $v01.h1
  vaddc $v02, $v04, $v02.h2
  ldv $v06, 0, 48, $s0
  vadd $v01, $v03, $v01.h2
  vrsqh $v03.e0, $v01.e0
  vrsql $v04.e0, $v02.e0
  vrsqh $v03.e0, $v01.e4
  vrsql $v04.e4, $v02.e4
  ldv $v07, 0, 24, $s0
  ldv $v08, 8, 56, $s0
  vrsqh $v03.e4, $v00.e0
  vmudm $v29, $v24, $v04.h0
  ldv $v01, 8, 0, $s0
  vmadh $v24, $v24, $v03.h0
  vmudn $v29, $v04, $v23.h3
  ldv $v06, 8, 48, $s0
  vmadh $v05, $v03, $v23.h3
  ldv $v07, 8, 24, $s0
  ldv $v08, 0, 56, $s0
  ldv $v02, 0, 32, $s0
  ldv $v04, 8, 40, $s0
  ldv $v03, 0, 8, $s0
  vmulf $v05, $v05, $v05.v
  ldv $v04, 0, 40, $s0
  ldv $v02, 8, 32, $s0
  ldv $v01, 0, 0, $s0
  ldv $v03, 8, 8, $s0
  vmulf $v23, $v23, $v05.h0
  ldv $v05, 8, 16, $s0
  ldv $v05, 0, 16, $s0
  LABEL_0002:
  vmulf $v24, $v24, $v16.v
  vmulf $v29, $v23, $v24.h0
  vmacf $v29, $v23, $v24.h1
  addiu $sp, $sp, 16
  vmacf $v23, $v23, $v24.h2
  addiu $k1, $k1, 65535
  vge $v23, $v23, $v00
  bne $k1, $zero, LIGHT_LOOP
  vadd $v18, $v18, $v23.v
  LIGHT_END:
  vmov $v18.e3, $v12.e3
  ctc2 $t0, $vcc
  vmov $v18.e7, $v12.e3
  PATCH_COLOR494: vmulf $v18, $v18, $v19.v
  vmrg $v18, $v18, $v22.h2
  vmudh $v23, $v15, $v15.v
  or $t3, $zero, $s7
  vsar $v23, COP2_ACC_MD
  vsar $v22, COP2_ACC_HI
  vaddc $v25, $v23, $v23.h1
  vadd $v24, $v22, $v22.h1
  vaddc $v23, $v25, $v23.h2
  vadd $v22, $v24, $v22.h2
  vrsqh $v24.e0, $v22.e0
  or $t4, $zero, $t2
  vrsql $v25.e0, $v23.e0
  vrsqh $v24.e0, $v22.e4
  vrsql $v25.e4, $v23.e4
  vrsqh $v24.e4, $v00.e0
  vmudm $v29, $v15, $v25.h0
  vmadh $v15, $v15, $v24.h0
  vmulf $v24, $v15, $v16.v
  ori $at, $zero, %lo(TEXTURING_SCALE)
  vadd $v25, $v24, $v24.h1
  vadd $v24, $v25, $v24.h2
  vlt $v24, $v24, $v00
  addiu $s7, $s7, 80
  vsubc $v24, $v00, $v24.v
  vor $v25, $v00, $v00
  vmov $v25.e2, $v12.e3
  addiu $t2, $t2, 80
  addu $t1, $t1, $t9
  vmov $v25.e6, $v12.e3
  vmulf $v29, $v16, $v24.h0
  vmacf $v29, $v16, $v24.h0
  vmadh $v29, $v15, $v30.e7
  vmadh $v29, $v25, $v30.e7
  vsar $v25, COP2_ACC_MD
  vsar $v24, COP2_ACC_HI
  vmudl $v29, $v25, $v25.v
  LOAD_COLOR567: llv $v19, 4, 0, $t1
  vmadm $v29, $v24, $v25.v
  vmadn $v23, $v25, $v24.v
  vmadh $v22, $v24, $v24.v
  vaddc $v27, $v23, $v23.h1
  vadd $v26, $v22, $v22.h1
  LOAD_POS_NORM565: ldv $v15, 8, 0, $t1
  vaddc $v23, $v27, $v23.h2
  addu $s4, $s4, $t9
  vadd $v22, $v26, $v22.h2
  vrsqh $v26.e0, $v22.e0
  vrsql $v27.e0, $v23.e0
  vrsqh $v26.e0, $v22.e4
  llv $v22, 0, 0, $at
  vrsql $v27.e4, $v23.e4
  LOAD_COLOR566: llv $v19, 0, 0, $s4
  vrsqh $v26.e4, $v00.e0
  llv $v22, 8, 0, $at
  LOAD_POS_NORM563: ldv $v16, 8, 0, $t1
  vxor $v17, $v00, $v30.e0
  ori $at, $zero, %lo(TEXTURING_OFFSET)
  vmudh $v29, $v17, $v30.e7
  llv $v23, 0, 0, $at
  vmadl $v29, $v25, $v27.h0
  vmadm $v29, $v24, $v27.h0
  llv $v23, 8, 0, $at
  LOAD_POS_NORM562: ldv $v16, 0, 0, $s4
  vmadn $v29, $v25, $v26.h0
  sltu $at, $s4, $s6
  vmadh $v17, $v24, $v26.h0
  vmudh $v29, $v23, $v30.e7
  LOAD_POS_NORM564: ldv $v15, 0, 0, $s4
  bne $at, $zero, VERTEX_LOOP
  vmadh $v17, $v17, $v22.v
  sdv $v20, 8, 0, $t4 ## Barrier: 0x30
  sb $t6, 6($t4) ## Barrier: 0x10
  suv $v18, 0, 8, $t3 ## Barrier: 0x40
  sdv $v20, 0, 0, $t3 ## Barrier: 0xC
  sb $k0, 7($t4) ## Barrier: 0x20
  suv $v18, 4, 8, $t4 ## Barrier: 0x80
  slv $v17, 8, 12, $t4 ## Barrier: 0x80
  sb $t5, 6($t3) ## Barrier: 0x4
  sb $t7, 7($t3) ## Barrier: 0x8
  j RSPQ_Loop
  slv $v17, 0, 12, $t3 ## Barrier: 0x40

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