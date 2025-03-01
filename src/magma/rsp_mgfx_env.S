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
    LIGHTING_LIGHTS: .ds.b 256
    .align 1
    LIGHTING_AMBIENT: .ds.b 8
    .align 2
    LIGHTING_COUNT: .ds.b 4
  MgEndUniform

  MgBeginUniform FOG, 3
    .align 1
    FOG_PARAMS: .ds.b 6
    FOG_MASK: .ds.b 1
  MgEndUniform

MgEndShaderUniforms

MgBeginVertexInput
  MgBeginVertexAttribute 0, 0
    MgVertexAttributeLoaders LOAD_POS_NORM308, LOAD_POS_NORM306, LOAD_POS_NORM307, LOAD_POS_NORM309, LOAD_POS_NORM597, LOAD_POS_NORM594, LOAD_POS_NORM596, LOAD_POS_NORM595
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR333, LOAD_COLOR336
    MgBeginVertexAttributePatch PATCH_COLOR509
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
  lsv $v02, 6, 4, $t0
  ldv $v01, 0, 0, $at
  vmudh $v04, $v02, $v01.v
  vsar $v03, COP2_ACC_HI
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)
  vsar $v04, COP2_ACC_MD
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x1
  ssv $v03, 0, 8, $at ## Barrier: 0x2
  ssv $v04, 0, 10, $at ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8) ## Barrier: 0x2
  sdv $v04, 0, 0, $at ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2) ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 6) ## Barrier: 0x1
  andi $t3, $t2, 7
  lh $t4, %lo(RSPQ_SCRATCH_MEM + 4) ## Barrier: 0x1
  addu $t0, $t1, $t3
  addu $s0, $s0, $t2
  addiu $t0, $t0, 7
  addiu $s4, $t4, %lo(MG_VERTEX_OVERFLOW)
  andi $t0, $t0, 4088
  andi $s4, $s4, 4088
  addiu $t0, $t0, -1
  jal DMAExec ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t2, $zero, 12
  ori $t0, $zero, %lo(MATRICES_MVP)
  ldv $v05, 0, 8, $t0
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)
  ori $t3, $zero, %lo(MG_CLIP_CACHE)
  ldv $v03, 0, 0, $t0
  ldv $v08, 0, 48, $t0
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)
  ldv $v07, 0, 16, $t0
  LOAD_POS_NORM308: ldv $v28, 0, 0, $s4
  addiu $t2, $s7, 40
  sll $t9, $t8, 1
  ori $at, $zero, %lo(MG_NORMAL_MASK)
  ldv $v02, 0, 8, $at
  ldv $v02, 8, 8, $at
  ldv $v07, 8, 16, $t0
  ldv $v01, 0, 0, $at
  ldv $v01, 8, 0, $at
  ori $at, $zero, %lo(MATRICES_NORMAL)
  ldv $v09, 0, 24, $t0
  ldv $v06, 0, 40, $t0
  ldv $v06, 8, 40, $t0
  addu $s6, $s4, $t1
  ldv $v11, 0, 0, $at
  LOAD_POS_NORM306: ldv $v16, 0, 0, $s4
  ldv $v03, 8, 0, $t0
  ldv $v05, 8, 8, $t0
  ldv $v13, 0, 16, $at
  ldv $v04, 0, 32, $t0
  addu $t1, $s4, $t8
  ldv $v04, 8, 32, $t0
  ldv $v10, 0, 56, $t0
  ldv $v10, 8, 56, $t0
  LOAD_POS_NORM307: ldv $v16, 8, 0, $t1
  LOAD_POS_NORM309: ldv $v28, 8, 0, $t1
  ori $t4, $zero, %lo(MG_CLIP_CACHE)
  ldv $v09, 8, 24, $t0
  ldv $v12, 0, 8, $at
  ldv $v08, 8, 48, $t0
  ldv $v11, 8, 0, $at
  ldv $v13, 8, 16, $at
  lbu $t0, %lo(FOG_MASK + 0)
  ldv $v12, 8, 8, $at
  VERTEX_LOOP:
  vmov $v28.e3, $v30.e2
  LOAD_COLOR333: llv $v19, 0, 0, $s4 ## Barrier: 0x3F00
  vand $v16, $v01, $v16.h3
  LOAD_COLOR336: llv $v19, 4, 0, $t1 ## Barrier: 0x3F00
  vmov $v28.e7, $v30.e2
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x4000
  vmudn $v16, $v16, $v02.v
  vmudn $v29, $v04, $v28.h0
  sdv $v17, 0, 0, $t3 ## Barrier: 0xC
  vmadh $v29, $v03, $v28.h0
  sdv $v19, 0, 0, $at ## Barrier: 0x4000
  vmadn $v29, $v08, $v28.h2
  luv $v19, 0, 0, $at ## Barrier: 0x4000
  sb $t5, 6($t3) ## Barrier: 0x4
  vmadh $v29, $v07, $v28.h2
  vmadn $v29, $v06, $v28.h1
  ori $at, $zero, %lo(MG_CLIP_FACTORS)
  ldv $v20, 0, 0, $at
  vmadh $v29, $v05, $v28.h1
  vmadn $v18, $v10, $v28.h3
  sdv $v17, 8, 0, $t4 ## Barrier: 0x30
  vmadh $v17, $v09, $v28.h3
  ldv $v20, 8, 0, $at
  vmulf $v29, $v11, $v16.h0
  suv $v15, 0, 8, $t3 ## Barrier: 0x40
  vmacf $v29, $v12, $v16.h1
  ori $at, $zero, %lo(FOG_PARAMS)
  vmacf $v16, $v13, $v16.h2
  sb $k0, 7($t4) ## Barrier: 0x20
  sdv $v17, 8, 24, $t2 ## Barrier: 0x200
  vmudn $v22, $v18, $v20.v
  vmadh $v21, $v17, $v20.v
  sb $t7, 7($t3) ## Barrier: 0x8
  sdv $v17, 0, 24, $s7 ## Barrier: 0x100
  slv $v14, 0, 12, $t3 ## Barrier: 0x40
  sdv $v18, 8, 32, $t2 ## Barrier: 0x200
  sdv $v18, 0, 32, $s7 ## Barrier: 0x100
  vch $v29, $v21, $v21.h3
  sb $t6, 6($t4) ## Barrier: 0x10
  vcl $v29, $v22, $v22.h3
  cfc2 $k1, $vcc
  ldv $v21, 0, 0, $at
  ldv $v21, 8, 0, $at
  andi $fp, $k1, 1799
  vch $v29, $v17, $v17.h3
  vcl $v29, $v18, $v18.h3
  srl $sp, $k1, 4
  ori $k1, $zero, %lo(MATRICES_MV)
  srl $s0, $fp, 5
  or $t5, $fp, $s0
  vaddc $v22, $v18, $v21.h1
  andi $fp, $sp, 1799
  ldv $v06, 0, 40, $k1
  vadd $v20, $v17, $v21.h0
  cfc2 $sp, $vcc
  srl $s0, $fp, 5
  vmudn $v29, $v22, $v21.h2
  or $t6, $fp, $s0
  srl $fp, $sp, 4
  vmadh $v20, $v20, $v21.h2
  ori $at, $zero, %lo(MG_VIEWPORT)
  ldv $v21, 0, 0, $at
  ldv $v21, 8, 0, $at
  ldv $v04, 0, 32, $k1
  andi $s0, $sp, 1799
  vge $v20, $v00, $v20.h2
  ldv $v22, 0, 8, $at
  ldv $v10, 0, 56, $k1
  ldv $v04, 8, 32, $k1
  vmudl $v18, $v18, $v21.h3
  srl $s1, $s0, 5
  vmadm $v17, $v17, $v21.h3
  vmadn $v18, $v00, $v00
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)
  suv $v15, 4, 8, $t4 ## Barrier: 0x80
  nor $t7, $s0, $s1
  ssv $v17, 6, 16, $s7 ## Barrier: 0x400
  ssv $v17, 14, 16, $t2 ## Barrier: 0x800
  vrcph $v17.e3, $v17.e3
  ssv $v18, 14, 18, $t2 ## Barrier: 0x800
  ssv $v18, 6, 18, $s7 ## Barrier: 0x400
  ldv $v08, 0, 48, $k1
  vrcpl $v18.e3, $v18.e3
  ldv $v09, 0, 24, $k1
  ldv $v22, 8, 8, $at
  ldv $v09, 8, 24, $k1
  vrcph $v17.e3, $v17.e7
  vrcpl $v18.e7, $v18.e7
  ldv $v03, 0, 0, $k1
  vrcph $v17.e7, $v00.e7
  andi $s0, $fp, 1799
  ldv $v03, 8, 0, $k1
  ori $at, $zero, %lo(LIGHTING_AMBIENT)
  srl $s1, $s0, 5
  vmudl $v29, $v18, $v18.h3
  vmadm $v29, $v17, $v18.h3
  ssv $v17, 6, 20, $s7 ## Barrier: 0x1000
  ssv $v18, 6, 22, $s7 ## Barrier: 0x1000
  ssv $v17, 14, 20, $t2 ## Barrier: 0x2000
  ssv $v18, 14, 22, $t2 ## Barrier: 0x2000
  vmadn $v18, $v18, $v17.h3
  ldv $v08, 8, 48, $k1
  vmadh $v17, $v17, $v17.h3
  slv $v14, 8, 12, $t4 ## Barrier: 0x80
  ldv $v07, 0, 16, $k1
  vmudh $v29, $v22, $v30.e7
  vmadn $v18, $v18, $v21.v
  ldv $v05, 0, 8, $k1
  vmadh $v17, $v17, $v21.v
  ldv $v06, 8, 40, $k1
  ldv $v05, 8, 8, $k1
  vmudn $v29, $v04, $v28.h0
  vmadh $v29, $v03, $v28.h0
  ldv $v07, 8, 16, $k1
  ldv $v10, 8, 56, $k1
  nor $k0, $s0, $s1
  vmadn $v29, $v06, $v28.h1
  lw $k1, %lo(LIGHTING_COUNT + 0)
  vmadh $v29, $v05, $v28.h1
  vmadn $v29, $v08, $v28.h2
  ldv $v15, 0, 0, $at
  vmadh $v29, $v07, $v28.h2
  ldv $v15, 8, 0, $at
  vmadn $v29, $v10, $v28.h3
  beq $k1, $zero, LIGHT_END
  vmadh $v21, $v09, $v28.h3
  LIGHT_LOOP:
  ldv $v23, 0, 0, $sp
  ldv $v23, 8, 0, $sp
  ldv $v22, 0, 8, $sp
  ldv $v22, 8, 8, $sp
  addiu $sp, $sp, 32
  addiu $k1, $k1, 65535
  vmulf $v23, $v23, $v16.v
  vmulf $v29, $v22, $v23.h0
  vmacf $v29, $v22, $v23.h1
  vmacf $v22, $v22, $v23.h2
  vge $v22, $v22, $v00
  vadd $v15, $v15, $v22.v
  bne $k1, $zero, LIGHT_LOOP
  nop
  LIGHT_END:
  PATCH_COLOR509: vmulf $v15, $v15, $v19.v
  ctc2 $t0, $vcc
  vmrg $v15, $v15, $v20.h2
  ori $k1, $zero, %lo(MATRICES_MVP)
  vmudh $v23, $v21, $v21.v
  ldv $v04, 0, 32, $k1
  ldv $v03, 0, 0, $k1
  vsar $v23, COP2_ACC_MD
  vsar $v22, COP2_ACC_HI
  vaddc $v25, $v23, $v23.h1
  or $t3, $zero, $s7
  vadd $v24, $v22, $v22.h1
  vaddc $v23, $v25, $v23.h2
  addu $s4, $s4, $t9
  vadd $v22, $v24, $v22.h2
  vrsqh $v22.e0, $v22.e0
  ldv $v03, 8, 0, $k1
  vrsql $v23.e0, $v23.e0
  ldv $v09, 8, 24, $k1
  vrsqh $v22.e0, $v22.e4
  ldv $v08, 8, 48, $k1
  ldv $v05, 0, 8, $k1
  vrsql $v23.e4, $v23.e4
  vrsqh $v22.e4, $v00.e0
  ldv $v05, 8, 8, $k1
  ldv $v07, 8, 16, $k1
  ldv $v07, 0, 16, $k1
  vmudm $v29, $v21, $v23.h0
  ldv $v06, 0, 40, $k1
  vmadh $v21, $v21, $v22.h0
  vmulf $v20, $v21, $v16.v
  ldv $v10, 0, 56, $k1
  or $t4, $zero, $t2
  vadd $v24, $v20, $v20.h1
  ldv $v08, 0, 48, $k1
  ldv $v09, 0, 24, $k1
  ldv $v06, 8, 40, $k1
  vadd $v20, $v24, $v20.h2
  ldv $v10, 8, 56, $k1
  addiu $s7, $s7, 80
  vor $v24, $v00, $v00
  ldv $v04, 8, 32, $k1
  addiu $k1, $zero, 32767
  vlt $v20, $v20, $v00
  addu $t1, $t1, $t9
  sh $k1, %lo(RSPQ_SCRATCH_MEM)($zero) ## Barrier: 0x1C000
  vsubc $v20, $v00, $v20.v
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM) ## Barrier: 0x8000
  lsv $v24, 4, 0, $at ## Barrier: 0x8000
  lsv $v24, 12, 0, $at ## Barrier: 0x10000
  LOAD_POS_NORM597: ldv $v28, 8, 0, $t1
  vmulf $v29, $v16, $v20.h0
  vmacf $v29, $v16, $v20.h0
  vmadh $v29, $v21, $v30.e7
  vmadh $v29, $v24, $v30.e7
  ori $at, $zero, %lo(TEXTURING_SCALE)
  llv $v20, 0, 0, $at
  vsar $v25, COP2_ACC_MD
  vsar $v24, COP2_ACC_HI
  llv $v20, 8, 0, $at
  vmudl $v29, $v25, $v25.v
  addiu $t2, $t2, 80
  vmadm $v29, $v24, $v25.v
  vmadn $v23, $v25, $v24.v
  vmadh $v22, $v24, $v24.v
  vaddc $v27, $v23, $v23.h1
  vadd $v26, $v22, $v22.h1
  vaddc $v23, $v27, $v23.h2
  vadd $v22, $v26, $v22.h2
  vrsqh $v22.e0, $v22.e0
  vrsql $v23.e0, $v23.e0
  LOAD_POS_NORM594: ldv $v16, 0, 0, $s4
  vrsqh $v22.e0, $v22.e4
  LOAD_POS_NORM596: ldv $v28, 0, 0, $s4
  vrsql $v23.e4, $v23.e4
  vrsqh $v22.e4, $v00.e0
  ori $at, $zero, %lo(TEXTURING_OFFSET)
  vxor $v14, $v00, $v30.e0
  vmudh $v29, $v14, $v30.e7
  vmadl $v29, $v25, $v23.h0
  vmadm $v29, $v24, $v23.h0
  vmadn $v29, $v25, $v22.h0
  vmadh $v14, $v24, $v22.h0
  llv $v22, 0, 0, $at
  llv $v22, 8, 0, $at
  LOAD_POS_NORM595: ldv $v16, 8, 0, $t1
  sltu $at, $s4, $s6
  vmudh $v29, $v22, $v30.e7
  bne $at, $zero, VERTEX_LOOP
  vmadh $v14, $v14, $v20.v
  suv $v15, 4, 8, $t4 ## Barrier: 0x80
  sdv $v17, 0, 0, $t3 ## Barrier: 0xC
  sb $t7, 7($t3) ## Barrier: 0x8
  slv $v14, 8, 12, $t4 ## Barrier: 0x80
  sdv $v17, 8, 0, $t4 ## Barrier: 0x30
  suv $v15, 0, 8, $t3 ## Barrier: 0x40
  sb $t5, 6($t3) ## Barrier: 0x4
  sb $t6, 6($t4) ## Barrier: 0x10
  slv $v14, 0, 12, $t3 ## Barrier: 0x40
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