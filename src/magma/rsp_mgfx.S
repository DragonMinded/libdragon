## Auto-generated file, transpiled with RSPL
#define __MAGMA_CONSTANTS 
#define MG_VERTEX_CACHE_COUNT 48
#define MG_DEFAULT_GUARD_BAND 2
#define MG_INLINE_UNIFORM_HEADER 12
#define MG_MAX_UNIFORM_PAYLOAD_SIZE (RSPQ_MAX_COMMAND_SIZE*4 - 12)
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
#define MG_MAX_UNIFORMS_SIZE 0x200
#define __MGFX_CONSTANTS 
#define MGFX_ATTRIBUTE_POSITION 0
#define MGFX_ATTRIBUTE_NORMAL 1
#define MGFX_ATTRIBUTE_COLOR 2
#define MGFX_ATTRIBUTE_TEXCOORD 3
#define MGFX_BINDING_MATRICES 0
#define MGFX_BINDING_TEXTURING 1
#define MGFX_BINDING_LIGHTING 2
#define MGFX_BINDING_FOG 3
#define MGFX_LIGHT_COUNT_MAX 8
#define MGFX_LIGHT_POSITION 0
#define MGFX_LIGHT_POSITIONW 6
#define MGFX_LIGHT_COLOR 8
#define MGFX_LIGHT_INTENSITY 14
#define MGFX_LIGHT_SIZE 16
#define MGFX_MATRIX_SIZE 64
#define MGFX_VTX_TEX_SHIFT 5
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
    MgVertexAttributeLoaders LOAD_POSITION380, LOAD_POSITION381, LOAD_POSITION645, LOAD_POSITION646
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL378, LOAD_NORMAL379, LOAD_NORMAL643, LOAD_NORMAL644
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR382, LOAD_COLOR383, LOAD_COLOR648, LOAD_COLOR647
    MgBeginVertexAttributePatch PATCH_COLOR572
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD410, LOAD_TEXCOORD413
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:292  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:291  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:293  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:292  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:296  |   ***8 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vmudh $v04, $v02, $v01.v                           ## L:295  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:317  |      9 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vsar $v03, COP2_ACC_HI                             ## L:295  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:309  |     10 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:295  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ssv $v03, 0, 8, $at                                ## L:297  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:297  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:307  |     15 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:296  |     16 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:303  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:305  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:312  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:310  |     20 | vtx_buffer += buffer_offset;
  addu $t0, $t1, $t3                                 ## L:313  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:314  |     22 | dma_size += 0x7;
  addiu $t2, $zero, 12                               ## L:320  |     23 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  andi $t0, $t0, 4088                                ## L:315  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:318  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:320  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:320  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:339  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:325  |     30 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:334  |     31 | u16 mvpaddr = MATRICES_MVP;
  ldv $v02, 0, 32, $t0                               ## L:273  |     32 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:274  |     33 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:272  |     34 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:278  |     35 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:277  |     36 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:340  |     37 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:322  |     38 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v06, 8, 48, $t0                               ## L:277  |     39 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:279  |     40 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:276  |     41 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:366  |     42 | u16 prev_out0 = MG_SCRATCH_MEM;
  ldv $v11, 0, 16, $at                               ## L:341  |     43 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:279  |     44 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addu $t2, $s4, $t8                                 ## L:363  |     45 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ldv $v01, 8, 0, $t0                                ## L:272  |     46 | m0:sint   = load(address, 0x00).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:326  |     47 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v02, 8, 32, $t0                               ## L:273  |     48 | m0:ufract = load(address, 0x20).xyzwxyzw;
  LOAD_POSITION380: ldv $v19, 0, 0, $s4              ## L:380  |     49 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR382: llv $v23, 0, 0, $s4                 ## L:382  |     50 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v04, 0, 40, $t0                               ## L:275  |     51 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_NORMAL378: lsv $v20, 0, 0, $s4                ## L:378  |     52 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  ldv $v10, 8, 8, $at                                ## L:340  |     53 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:276  |     54 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $t5, $zero, %lo(MG_SCRATCH_MEM)                ## L:367  |     55 | u16 prev_out1 = MG_SCRATCH_MEM;
  ldv $v07, 8, 24, $t0                               ## L:278  |     56 | m3:sint   = load(address, 0x18).xyzwxyzw;
  lbu $t1, %lo(FOG_MASK + 0)                         ## L:361  |     57 | u8 fog_mask = load(FOG_MASK);
  ldv $v11, 8, 16, $at                               ## L:341  |     58 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:275  |     59 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:339  |     60 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:274  |     61 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:339  |     62 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:323  |     63 | vtx_size2 = vtx_size * 2;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:343  |     64 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:343  |     65 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:344  |     66 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:344  |     67 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:343  |     68 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:347  |     69 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v15, 0, 8, $at                                ## L:348  |     70 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v14, 0, 0, $at                                ## L:347  |     71 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v15, 8, 8, $at                                ## L:348  |     72 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  LOAD_NORMAL379: lsv $v20, 8, 0, $t2                ## L:379  |     73 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  ldv $v14, 8, 0, $at                                ## L:347  |     74 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:351  |     75 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v16, 0, 0, $at                                ## L:351  |     76 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v16, 8, 0, $at                                ## L:352  |     77 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:353  |     78 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  LOAD_POSITION381: ldv $v19, 8, 0, $t2              ## L:381  |     79 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:356  |     80 | u8 has_points = load(LIGHTING_HAS_POINTS);
  LOAD_COLOR383: llv $v23, 4, 0, $t2                 ## L:383  |     81 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  llv $v17, 0, 0, $at                                ## L:353  |     82 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v17, 8, 0, $at                                ## L:354  |     83 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:360  |     84 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v18, 0, 0, $at                                ## L:360  |     85 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v18, 8, 0, $at                                ## L:360  |     86 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  addiu $t3, $s7, 40                                 ## L:364  |     87 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  VERTEX_LOOP:
  vand $v20, $v12, $v20.h0                           ## L:417  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:405  |     88 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v23, 0, 0, $at                                ## L:405  |     89 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  suv $v22, 4, 8, $t5                                ## L:400  |     90 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  suv $v22, 0, 8, $t4                                ## L:399  |     91 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v24, 0, 0, $t4                                ## L:391  |     92 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmudn $v20, $v20, $v13.v                           ## L:418  |      ^ | vnorm *= vnormfactor;
  luv $v23, 0, 0, $at                                ## L:406  |     93 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v29, $v02, $v19.h0                          ## L:421  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v19.h0                          ## L:421  |     94 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:430  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:430  |     95 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v06, $v19.h2                          ## L:422  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ldv $v22, 8, 0, $at                                ## L:430  |     96 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v05, $v19.h2                          ## L:422  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v04, $v19.h1                          ## L:423  |     97 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:449  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v03, $v19.h1                          ## L:423  |     98 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v26, 0, 0, $at                                ## L:449  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v25, $v08, $v30.e7                          ## L:424  |     99 | vpos_clip = vmvp3 +* 1;
  sdv $v24, 8, 0, $t5                                ## L:392  |      ^ | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmadh $v24, $v07, $v30.e7                          ## L:424  |    100 | vpos_clip = vmvp3 +* 1;
  ldv $v26, 8, 0, $at                                ## L:449  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  slv $v21, 8, 12, $t5                               ## L:403  |    101 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmulf $v29, $v09, $v20.h0                          ## L:426  |      ^ | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  lbu $sp, %lo(LIGHTING_COUNT + 0)                   ## L:431  |    102 | u8 light_count = load(LIGHTING_COUNT);
  vmacf $v29, $v10, $v20.h1                          ## L:427  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  vmacf $v20, $v11, $v20.h2                          ## L:428  |    103 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  slv $v21, 0, 12, $t4                               ## L:402  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  LOAD_TEXCOORD410: llv $v21, 0, 0, $s4              ## L:410  |    104 | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  vmudn $v28, $v25, $v26.v                           ## L:450  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  sb $k1, 7($t5)                                     ## L:397  |    105 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmadh $v27, $v24, $v26.v                           ## L:450  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  LOAD_TEXCOORD413: llv $v21, 8, 0, $t2              ## L:413  |    106 | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  sdv $v24, 0, 24, $s7                               ## L:459  |    107 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v25, 0, 32, $s7                               ## L:459  |   *109 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vch $v29, $v27, $v27.h3                            ## L:451  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $t6, 6($t4)                                     ## L:394  |    110 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vcl $v29, $v28, $v28.h3                            ## L:451  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $fp, $vcc                                     ## L:451  |    111 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v24, $v24.h3                            ## L:455  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v25, $v25.h3                            ## L:455  |    112 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sdv $v24, 8, 24, $t3                               ## L:460  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vaddc $v27, $v25, $v18.h1                          ## L:464  |    113 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v26, $v24, $v18.h0                           ## L:465  |    114 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s1, $fp, 1799                                ## L:238  |      ^ | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:239  |    115 | t1 = t0 >> 5;
  srl $s0, $fp, 4                                    ## L:258  |   *117 | u16 codes2 = codes >> 4;
  vmudn $v29, $v27, $v18.h2                          ## L:467  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v26, $v26, $v18.h2                          ## L:468  |    118 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  sb $k0, 7($t4)                                     ## L:396  |      ^ | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  or $t6, $s1, $s2                                   ## L:246  |    119 | result = t0 | t1;
  andi $s1, $s0, 1799                                ## L:238  |    120 | t0 = codes & 0x707;
  cfc2 $s0, $vcc                                     ## L:455  |    121 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sdv $v25, 8, 32, $t3                               ## L:460  |    122 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vge $v26, $v00, $v26.h2                            ## L:469  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  srl $s2, $s1, 5                                    ## L:239  |    123 | t1 = t0 >> 5;
  vmudl $v25, $v25, $v14.h3                          ## L:477  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v24, $v24, $v14.h3                          ## L:477  |    124 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  sb $t7, 6($t5)                                     ## L:395  |      ^ | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  or $t7, $s1, $s2                                   ## L:246  |    125 | result = t0 | t1;
  vmadn $v25, $v00, $v00                             ## L:477  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  andi $s2, $s0, 1799                                ## L:238  |    126 | t0 = codes & 0x707;
  srl $s1, $s0, 4                                    ## L:265  |    127 | u16 codes2 = codes >> 4;
  srl $s3, $s2, 5                                    ## L:239  |    128 | t1 = t0 >> 5;
  vrcph $v27.e3, $v24.e3                             ## L:484  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcpl $v28.e3, $v25.e3                             ## L:484  |    129 | vinvw.w = invert_half(vpos_clip).w;
  ssv $v24, 14, 16, $t3                              ## L:481  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v25, 14, 18, $t3                              ## L:481  |    130 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v27.e3, $v24.e7                             ## L:485  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcpl $v28.e7, $v25.e7                             ## L:485  |    131 | vinvw.W = invert_half(vpos_clip).W;
  nor $k0, $s2, $s3                                  ## L:253  |      ^ | result = t0 ~| t1;
  andi $s2, $s1, 1799                                ## L:238  |    132 | t0 = codes & 0x707;
  vrcph $v27.e7, $v00.e7                             ## L:485  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ssv $v24, 6, 16, $s7                               ## L:480  |    133 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ssv $v25, 6, 18, $s7                               ## L:480  |    134 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  srl $s3, $s2, 5                                    ## L:239  |    135 | t1 = t0 >> 5;
  vmudl $v29, $v25, $v28.h3                          ## L:490  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v24, $v28.h3                          ## L:490  |    136 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v27, 14, 20, $t3                              ## L:488  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v25, $v25, $v27.h3                          ## L:490  |    137 | vpos_clip *= vinvw.wwwwWWWW;
  nor $k1, $s2, $s3                                  ## L:253  |      ^ | result = t0 ~| t1;
  vmadh $v24, $v24, $v27.h3                          ## L:490  |    138 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v27, 6, 20, $s7                               ## L:487  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v28, 6, 22, $s7                               ## L:487  |    139 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v28, 14, 22, $t3                              ## L:488  |    140 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmudh $v29, $v15, $v30.e7                          ## L:494  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v25, $v25, $v14.v                           ## L:495  |    141 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_mgfx_0001                    ## L:503  |      ^ | if (has_points != 0)
  vmadh $v24, $v24, $v14.v                           ## L:495  |   *143 | vpos_clip = vpos_clip +* vviewscale;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:506  |    144 | u16 addr = MATRICES_MV;
  ldv $v03, 0, 8, $fp                                ## L:274  |    145 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v02, 0, 32, $fp                               ## L:273  |    146 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v01, 0, 0, $fp                                ## L:272  |    147 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 8, 0, $fp                                ## L:272  |    148 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v08, 0, 56, $fp                               ## L:279  |    149 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v03, 8, 8, $fp                                ## L:274  |    150 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v04, 0, 40, $fp                               ## L:275  |    151 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v02, 8, 32, $fp                               ## L:273  |    152 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 8, 40, $fp                               ## L:275  |    153 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v05, 0, 16, $fp                               ## L:276  |    154 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:277  |    155 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v06, 8, 48, $fp                               ## L:277  |    156 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudn $v29, $v02, $v19.h0                          ## L:509  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v19.h0                          ## L:509  |    157 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v08, 8, 56, $fp                               ## L:279  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadn $v29, $v04, $v19.h1                          ## L:510  |    158 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v05, 8, 16, $fp                               ## L:276  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v07, 0, 24, $fp                               ## L:278  |    159 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadh $v29, $v03, $v19.h1                          ## L:510  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v07, 8, 24, $fp                               ## L:278  |    160 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v29, $v06, $v19.h2                          ## L:511  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v19.h2                          ## L:511  |   *162 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e7                          ## L:512  |    163 | vpos = vmvp3 +* 1;
  vmadh $v19, $v07, $v30.e7                          ## L:512  |    164 | vpos = vmvp3 +* 1;
  LABEL_mgfx_0001:
  beq $sp, $zero, LIGHT_END                          ## L:516  |      ^ | if (light_count == 0) goto LIGHT_END;
  ori $fp, $zero, %lo(LIGHTING_LIGHTS)               ## L:515  |   *166 | u16 light_ptr = LIGHTING_LIGHTS;
  LIGHT_LOOP:
  ldv $v28, 0, 0, $fp                                ## L:520  |    167 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  lh $s0, 6($fp)                                     ## L:522  |    168 | s16 light_w = load(light_ptr, 6);
  ldv $v28, 8, 0, $fp                                ## L:520  |    169 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v27, 0, 8, $fp                                ## L:519  |    170 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  beq $s0, $zero, LABEL_mgfx_0003                    ## L:523  |    171 | if (light_w != 0)
  ldv $v27, 8, 8, $fp                                ## L:519  |   *173 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:551  |    174 | u16 addr = MATRICES_MVP;
  vsubc $v28, $v28, $v19.v                           ## L:531  |      ^ | vlpos -= vpos;
  ldv $v06, 8, 48, $s1                               ## L:277  | ***178 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudh $v02, $v28, $v28.v                           ## L:532  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vsar $v02, COP2_ACC_MD                             ## L:532  |    179 | vec32 vsqdist = vlpos * vlpos;
  vsar $v01, COP2_ACC_HI                             ## L:532  |    180 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v04, $v02, $v02.h1                          ## L:533  |  **183 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v03, $v01, $v01.h1                           ## L:533  |    184 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v02, $v04, $v02.h2                          ## L:534  |  **187 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v01, $v03, $v01.h2                           ## L:534  |    188 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v07, 8, 24, $s1                               ## L:278  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v07, 0, 24, $s1                               ## L:278  |    189 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 0, 56, $s1                               ## L:279  |  **192 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrsqh $v03.e0, $v01.e0                             ## L:536  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v04.e0, $v02.e0                             ## L:536  |    193 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v03.e0, $v01.e4                             ## L:537  |    194 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v08, 8, 56, $s1                               ## L:279  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v01, 8, 0, $s1                                ## L:272  |    195 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrsql $v04.e4, $v02.e4                             ## L:537  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v03.e4, $v00.e0                             ## L:537  |    196 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v02, 0, 32, $s1                               ## L:273  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmudm $v29, $v28, $v04.h0                          ## L:539  |  **199 | vlpos *= vtmp.xxxxXXXX;
  ldv $v02, 8, 32, $s1                               ## L:273  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v28, $v28, $v03.h0                          ## L:539  |    200 | vlpos *= vtmp.xxxxXXXX;
  ldv $v01, 0, 0, $s1                                ## L:272  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v06, 0, 48, $s1                               ## L:277  |    201 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudn $v29, $v04, $v27.h3                          ## L:540  |      ^ | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  ldv $v04, 8, 40, $s1                               ## L:275  |    202 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadh $v05, $v03, $v27.h3                          ## L:540  |      ^ | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  ldv $v03, 0, 8, $s1                                ## L:274  |    203 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v03, 8, 8, $s1                                ## L:274  |    204 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v04, 0, 40, $s1                               ## L:275  |   *206 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmulf $v05, $v05, $v05.v                           ## L:541  |      ^ | vatten:sfract *= vatten:sfract;
  vmulf $v27, $v27, $v05.h0                          ## L:542  | ***210 | vlcol:sfract *= vatten:sfract.xxxxXXXX;
  ldv $v05, 0, 16, $s1                               ## L:276  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 8, 16, $s1                               ## L:276  |    211 | m2:sint   = load(address, 0x10).xyzwxyzw;
  LABEL_mgfx_0003:
  vmulf $v28, $v28, $v20.v                           ## L:556  |      ^ | vlpos *= vnorm:sfract;
  addiu $fp, $fp, 16                                 ## L:564  | ***215 | light_ptr += 16;
  vmulf $v29, $v27, $v28.h0                          ## L:557  |      ^ | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  addiu $sp, $sp, 65535                              ## L:565  |    216 | light_count -= 1;
  vmacf $v29, $v27, $v28.h1                          ## L:558  |      ^ | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v27, $v27, $v28.h2                          ## L:559  |    217 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v27, $v27, $v00                               ## L:560  | ***221 | vlcol = max(vlcol, VZERO);
  bne $sp, $zero, LIGHT_LOOP                         ## L:566  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v22, $v22, $v27.v                            ## L:562  | ****226 | vrgba:sint += vlcol;
  LIGHT_END:
  addu $s4, $s4, $t9                                 ## L:638  |    227 | vtx_in0_ptr += vtx_size2;
  vmov $v22.e3, $v12.e3                              ## L:570  |      ^ | vrgba.w = vnormmask.w;
  LOAD_NORMAL643: lsv $v20, 0, 0, $s4                ## L:643  |    228 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  vmov $v22.e7, $v12.e3                              ## L:571  |      ^ | vrgba.W = vnormmask.w;
  sltu $at, $s4, $s6                                 ## L:650  |    229 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  or $t5, $zero, $t3                                 ## L:637  |    230 | prev_out1 = vtx_out1_ptr;
  ctc2 $t1, $vcc                                     ## L:574  |    231 | set_vcc(fog_mask);
  or $t4, $zero, $s7                                 ## L:636  |    232 | prev_out0 = vtx_out0_ptr;
  addu $t2, $t2, $t9                                 ## L:639  |    233 | vtx_in1_ptr += vtx_size2;
  PATCH_COLOR572: vmulf $v22, $v22, $v23.v           ## L:572  |      ^ | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  LOAD_NORMAL644: lsv $v20, 8, 0, $t2                ## L:644  |    234 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  addiu $s7, $s7, 80                                 ## L:640  |    235 | vtx_out0_ptr += 80;
  LOAD_POSITION645: ldv $v19, 0, 0, $s4              ## L:645  |    236 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR648: llv $v23, 4, 0, $t2                 ## L:648  |    237 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmrg $v22, $v22, $v26.h2                           ## L:575  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  LOAD_POSITION646: ldv $v19, 8, 0, $t2              ## L:646  |    238 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmudh $v29, $v17, $v30.e7                          ## L:633  |      ^ | VTEMP = vtexoffset:sint * 1;
  LOAD_COLOR647: llv $v23, 0, 0, $s4                 ## L:647  |    239 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmadh $v21, $v21, $v16.v                           ## L:634  |      ^ | vtex:sint = vtex:sint +* vtexscale;
  bne $at, $zero, VERTEX_LOOP                        ## L:650  |    240 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  addiu $t3, $t3, 80                                 ## L:641  |   *242 | vtx_out1_ptr += 80;
  sdv $v24, 0, 0, $t4                                ## L:656  |    243 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sdv $v24, 8, 0, $t5                                ## L:657  |    244 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v22, 0, 8, $t4                                ## L:664  |    245 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sb $t7, 6($t5)                                     ## L:660  |    246 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $k1, 7($t5)                                     ## L:662  |    247 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sb $k0, 7($t4)                                     ## L:661  |    248 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  suv $v22, 4, 8, $t5                                ## L:665  |    249 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  slv $v21, 0, 12, $t4                               ## L:667  |    250 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t6, 6($t4)                                     ## L:659  |    251 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  j RSPQ_Loop                                        ## L:669  |    252 | }
  slv $v21, 8, 12, $t5                               ## L:668  |   *254 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80

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