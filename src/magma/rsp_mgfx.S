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
#define MGFX_VTX_POS_SHIFT 5
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
    MgVertexAttributeLoaders LOAD_POSITION304, LOAD_POSITION305, LOAD_POSITION573, LOAD_POSITION574
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL302, LOAD_NORMAL303, LOAD_NORMAL571, LOAD_NORMAL572
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR306, LOAD_COLOR307, LOAD_COLOR576, LOAD_COLOR575
    MgBeginVertexAttributePatch PATCH_COLOR500
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD334, LOAD_TEXCOORD337
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:216  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:215  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v01, 0, 0, $at                                ## L:216  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:217  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:233  |   ***8 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vmudh $v04, $v02, $v01.v                           ## L:219  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:241  |      9 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vsar $v03, COP2_ACC_HI                             ## L:219  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  vsar $v04, COP2_ACC_MD                             ## L:219  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:220  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v03, 0, 8, $at                                ## L:221  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:221  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:220  |     15 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:231  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:227  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:229  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:234  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:236  |     20 | u32 misalignment = buffer_offset & 0x7;
  addiu $t2, $zero, 12                               ## L:244  |     21 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addu $t0, $t1, $t3                                 ## L:237  |     22 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:238  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:239  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:242  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:244  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:244  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:263  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:258  |     30 | u16 mvpaddr = MATRICES_MVP;
  ldv $v04, 0, 40, $t0                               ## L:199  |     31 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:263  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:197  |     33 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:264  |     34 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:263  |     35 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:265  |     36 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:264  |     37 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:246  |     38 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v04, 8, 40, $t0                               ## L:199  |     39 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:265  |     40 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:267  |     41 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:267  |     42 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:268  |     43 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:267  |     44 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:268  |     45 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:271  |     46 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  LOAD_NORMAL302: lsv $v20, 0, 0, $s4                ## L:302  |     47 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  LOAD_COLOR306: llv $v23, 0, 0, $s4                 ## L:306  |     48 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v15, 0, 8, $at                                ## L:272  |     49 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v14, 0, 0, $at                                ## L:271  |     50 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v15, 8, 8, $at                                ## L:272  |     51 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:202  |     52 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:203  |     53 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:201  |     54 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v14, 8, 0, $at                                ## L:271  |     55 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:196  |     56 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LOAD_POSITION304: ldv $v19, 0, 0, $s4              ## L:304  |     57 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v01, 8, 0, $t0                                ## L:196  |     58 | m0:sint   = load(address, 0x00).xyzwxyzw;
  addu $t2, $s4, $t8                                 ## L:287  |     59 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ldv $v02, 8, 32, $t0                               ## L:197  |     60 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ori $t5, $zero, %lo(MG_SCRATCH_MEM)                ## L:291  |     61 | u16 prev_out1 = MG_SCRATCH_MEM;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:275  |     62 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  LOAD_POSITION305: ldv $v19, 8, 0, $t2              ## L:305  |     63 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  llv $v16, 0, 0, $at                                ## L:275  |     64 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  ldv $v03, 0, 8, $t0                                ## L:198  |     65 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:198  |     66 | m1:sint   = load(address, 0x08).xyzwxyzw;
  llv $v16, 8, 0, $at                                ## L:276  |     67 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  ldv $v08, 8, 56, $t0                               ## L:203  |     68 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:277  |     69 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  LOAD_COLOR307: llv $v23, 4, 0, $t2                 ## L:307  |     70 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v06, 8, 48, $t0                               ## L:201  |     71 | m2:ufract = load(address, 0x30).xyzwxyzw;
  llv $v17, 0, 0, $at                                ## L:277  |     72 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  ldv $v07, 8, 24, $t0                               ## L:202  |     73 | m3:sint   = load(address, 0x18).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:249  |     74 | vtx_in_end = vtx_in0_ptr + vertices_size;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:250  |     75 | vtx_out0_ptr += MG_VERTEX_CACHE;
  LOAD_NORMAL303: lsv $v20, 8, 0, $t2                ## L:303  |     76 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  addiu $t3, $s7, 40                                 ## L:288  |     77 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  lbu $t1, %lo(FOG_MASK + 0)                         ## L:285  |     78 | u8 fog_mask = load(FOG_MASK);
  ldv $v05, 0, 16, $t0                               ## L:200  |     79 | m2:sint   = load(address, 0x10).xyzwxyzw;
  llv $v17, 8, 0, $at                                ## L:278  |     80 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:284  |     81 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:247  |     82 | vtx_size2 = vtx_size * 2;
  ldv $v05, 8, 16, $t0                               ## L:200  |     83 | m2:sint   = load(address, 0x10).xyzwxyzw;
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:280  |     84 | u8 has_points = load(LIGHTING_HAS_POINTS);
  ldv $v18, 0, 0, $at                                ## L:284  |     85 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:290  |     86 | u16 prev_out0 = MG_SCRATCH_MEM;
  ldv $v18, 8, 0, $at                                ## L:284  |     87 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  VERTEX_LOOP:
  vand $v20, $v12, $v20.h0                           ## L:341  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  vmov $v19.e3, $v30.e2                              ## L:343  |     88 | vpos.w = 0x20;
  suv $v22, 0, 8, $t4                                ## L:323  |      ^ | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmov $v19.e7, $v30.e2                              ## L:344  |     89 | vpos.W = 0x20;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:329  |      ^ | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v24, 8, 0, $t5                                ## L:316  |     90 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v22, 4, 8, $t5                                ## L:324  |     91 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmudn $v20, $v20, $v13.v                           ## L:346  |      ^ | vnorm *= vnormfactor;
  sdv $v24, 0, 0, $t4                                ## L:315  |     92 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sdv $v23, 0, 0, $at                                ## L:329  |     93 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v29, $v02, $v19.h0                          ## L:349  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  luv $v23, 0, 0, $at                                ## L:330  |     94 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadh $v29, $v01, $v19.h0                          ## L:349  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:358  |     95 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v06, $v19.h2                          ## L:350  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v19.h2                          ## L:350  |     96 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ldv $v22, 0, 0, $at                                ## L:358  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v04, $v19.h1                          ## L:351  |     97 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v22, 8, 0, $at                                ## L:358  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v03, $v19.h1                          ## L:351  |     98 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:377  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v26, 0, 0, $at                                ## L:377  |     99 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v25, $v08, $v19.h3                          ## L:352  |      ^ | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  vmadh $v24, $v07, $v19.h3                          ## L:352  |    100 | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  ldv $v26, 8, 0, $at                                ## L:377  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmulf $v29, $v09, $v20.h0                          ## L:354  |    101 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  vmacf $v29, $v10, $v20.h1                          ## L:355  |    102 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  lbu $sp, %lo(LIGHTING_COUNT + 0)                   ## L:359  |      ^ | u8 light_count = load(LIGHTING_COUNT);
  slv $v21, 0, 12, $t4                               ## L:326  |    103 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmacf $v20, $v11, $v20.h2                          ## L:356  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudn $v28, $v25, $v26.v                           ## L:378  |    104 | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v27, $v24, $v26.v                           ## L:378  |    105 | vec32 vguard = vpos_clip * vclip_factors;
  sb $t6, 6($t4)                                     ## L:318  |      ^ | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t7, 6($t5)                                     ## L:319  |    106 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $k1, 7($t5)                                     ## L:321  |    107 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  slv $v21, 8, 12, $t5                               ## L:327  |    108 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $k0, 7($t4)                                     ## L:320  |    109 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vch $v29, $v27, $v27.h3                            ## L:379  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  LOAD_TEXCOORD334: llv $v21, 0, 0, $s4              ## L:334  |    110 | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  vcl $v29, $v28, $v28.h3                            ## L:379  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $fp, $vcc                                     ## L:379  |    111 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v24, $v24.h3                            ## L:383  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v25, $v25.h3                            ## L:383  |    112 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vaddc $v27, $v25, $v18.h1                          ## L:392  |    113 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  LOAD_TEXCOORD337: llv $v21, 8, 0, $t2              ## L:337  |      ^ | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  vadd $v26, $v24, $v18.h0                           ## L:393  |    114 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s1, $fp, 1799                                ## L:162  |      ^ | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:163  |    115 | t1 = t0 >> 5;
  sdv $v24, 8, 24, $t3                               ## L:388  |    116 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  srl $s0, $fp, 4                                    ## L:182  |    117 | u16 codes2 = codes >> 4;
  vmudn $v29, $v27, $v18.h2                          ## L:395  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v26, $v26, $v18.h2                          ## L:396  |    118 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  sdv $v24, 0, 24, $s7                               ## L:387  |      ^ | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  or $t6, $s1, $s2                                   ## L:170  |    119 | result = t0 | t1;
  andi $s1, $s0, 1799                                ## L:162  |    120 | t0 = codes & 0x707;
  sdv $v25, 8, 32, $t3                               ## L:388  |    121 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  cfc2 $s0, $vcc                                     ## L:383  |    122 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v26, $v00, $v26.h2                            ## L:397  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  sdv $v25, 0, 32, $s7                               ## L:387  |    123 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vmudl $v25, $v25, $v14.h3                          ## L:405  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v24, $v24, $v14.h3                          ## L:405  |    124 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  srl $s2, $s1, 5                                    ## L:163  |      ^ | t1 = t0 >> 5;
  vmadn $v25, $v00, $v00                             ## L:405  |    125 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  or $t7, $s1, $s2                                   ## L:170  |      ^ | result = t0 | t1;
  andi $s2, $s0, 1799                                ## L:162  |    126 | t0 = codes & 0x707;
  srl $s3, $s2, 5                                    ## L:163  |    127 | t1 = t0 >> 5;
  nor $k0, $s2, $s3                                  ## L:177  |    128 | result = t0 ~| t1;
  vrcph $v27.e3, $v24.e3                             ## L:412  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ssv $v24, 6, 16, $s7                               ## L:408  |    129 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcpl $v28.e3, $v25.e3                             ## L:412  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  srl $s1, $s0, 4                                    ## L:189  |    130 | u16 codes2 = codes >> 4;
  vrcph $v27.e3, $v24.e7                             ## L:413  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  andi $s2, $s1, 1799                                ## L:162  |    131 | t0 = codes & 0x707;
  vrcpl $v28.e7, $v25.e7                             ## L:413  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v27.e7, $v00.e7                             ## L:413  |    132 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v24, 14, 16, $t3                              ## L:409  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  srl $s3, $s2, 5                                    ## L:163  |    133 | t1 = t0 >> 5;
  ssv $v25, 14, 18, $t3                              ## L:409  |    134 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  nor $k1, $s2, $s3                                  ## L:177  |    135 | result = t0 ~| t1;
  vmudl $v29, $v25, $v28.h3                          ## L:418  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v24, $v28.h3                          ## L:418  |    136 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 6, 18, $s7                               ## L:408  |      ^ | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vmadn $v25, $v25, $v27.h3                          ## L:418  |    137 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v27, 14, 20, $t3                              ## L:416  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v24, $v24, $v27.h3                          ## L:418  |    138 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v27, 6, 20, $s7                               ## L:415  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v28, 6, 22, $s7                               ## L:415  |    139 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v28, 14, 22, $t3                              ## L:416  |    140 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmudh $v29, $v15, $v30.e7                          ## L:422  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v25, $v25, $v14.v                           ## L:423  |    141 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_mgfx_0001                    ## L:431  |      ^ | if (has_points != 0)
  vmadh $v24, $v24, $v14.v                           ## L:423  |   *143 | vpos_clip = vpos_clip +* vviewscale;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:434  |    144 | u16 addr = MATRICES_MV;
  ldv $v02, 0, 32, $fp                               ## L:197  |    145 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v01, 0, 0, $fp                                ## L:196  |    146 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 8, 32, $fp                               ## L:197  |    147 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $fp                                ## L:198  |    148 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:201  |    149 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 0, 56, $fp                               ## L:203  |    150 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v01, 8, 0, $fp                                ## L:196  |    151 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v04, 0, 40, $fp                               ## L:199  |    152 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmudn $v29, $v02, $v19.h0                          ## L:437  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v04, 8, 40, $fp                               ## L:199  |    153 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v03, 8, 8, $fp                                ## L:198  |    154 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 8, 48, $fp                               ## L:201  |    155 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 8, 56, $fp                               ## L:203  |    156 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v29, $v01, $v19.h0                          ## L:437  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v19.h1                          ## L:438  |    157 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v05, 0, 16, $fp                               ## L:200  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 8, 16, $fp                               ## L:200  |    158 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v07, 0, 24, $fp                               ## L:202  |    159 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v07, 8, 24, $fp                               ## L:202  |    160 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadh $v29, $v03, $v19.h1                          ## L:438  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v29, $v06, $v19.h2                          ## L:439  |    161 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v19.h2                          ## L:439  |    162 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v19.h3                          ## L:440  |    163 | vpos = vmvp3 +* vpos.wwwwWWWW;
  vmadh $v19, $v07, $v19.h3                          ## L:440  |    164 | vpos = vmvp3 +* vpos.wwwwWWWW;
  LABEL_mgfx_0001:
  beq $sp, $zero, LIGHT_END                          ## L:444  |      ^ | if (light_count == 0) goto LIGHT_END;
  ori $fp, $zero, %lo(LIGHTING_LIGHTS)               ## L:443  |   *166 | u16 light_ptr = LIGHTING_LIGHTS;
  LIGHT_LOOP:
  lh $s0, 6($fp)                                     ## L:450  |    167 | s16 light_w = load(light_ptr, 6);
  ldv $v28, 0, 0, $fp                                ## L:448  |    168 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v27, 0, 8, $fp                                ## L:447  |    169 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v28, 8, 0, $fp                                ## L:448  |    170 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $s0, $zero, LABEL_mgfx_0003                    ## L:451  |    171 | if (light_w != 0)
  ldv $v27, 8, 8, $fp                                ## L:447  |   *173 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  vsubc $v28, $v28, $v19.v                           ## L:459  |    174 | vlpos -= vpos;
  vmudh $v02, $v28, $v28.v                           ## L:460  | ***178 | vec32 vsqdist = vlpos * vlpos;
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:479  |      ^ | u16 addr = MATRICES_MVP;
  vsar $v02, COP2_ACC_MD                             ## L:460  |    179 | vec32 vsqdist = vlpos * vlpos;
  ldv $v07, 8, 24, $s1                               ## L:202  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vsar $v01, COP2_ACC_HI                             ## L:460  |    180 | vec32 vsqdist = vlpos * vlpos;
  ldv $v06, 8, 48, $s1                               ## L:201  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vaddc $v04, $v02, $v02.h1                          ## L:461  |  **183 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v08, 8, 56, $s1                               ## L:203  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vadd $v03, $v01, $v01.h1                           ## L:461  |    184 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v08, 0, 56, $s1                               ## L:203  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vaddc $v02, $v04, $v02.h2                          ## L:462  |  **187 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v01, $v03, $v01.h2                           ## L:462  |    188 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v03.e0, $v01.e0                             ## L:464  | ***192 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v04.e0, $v02.e0                             ## L:464  |    193 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v03.e0, $v01.e4                             ## L:465  |    194 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v04.e4, $v02.e4                             ## L:465  |    195 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v07, 0, 24, $s1                               ## L:202  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsqh $v03.e4, $v00.e0                             ## L:465  |    196 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v02, 8, 32, $s1                               ## L:197  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmudm $v29, $v28, $v04.h0                          ## L:467  |  **199 | vlpos *= vtmp.xxxxXXXX;
  ldv $v06, 0, 48, $s1                               ## L:201  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v01, 8, 0, $s1                                ## L:196  |    200 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadh $v28, $v28, $v03.h0                          ## L:467  |      ^ | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v04, $v27.h3                          ## L:468  |    201 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  ldv $v02, 0, 32, $s1                               ## L:197  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v05, $v03, $v27.h3                          ## L:468  |    202 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  ldv $v04, 0, 40, $s1                               ## L:199  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v04, 8, 40, $s1                               ## L:199  |    203 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 0, 0, $s1                                ## L:196  |    204 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v03, 8, 8, $s1                                ## L:198  |    205 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v03, 0, 8, $s1                                ## L:198  |    206 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmulf $v05, $v05, $v05.v                           ## L:469  |      ^ | vatten:sfract *= vatten:sfract;
  vmulf $v27, $v27, $v05.h0                          ## L:470  | ***210 | vlcol:sfract *= vatten:sfract.xxxxXXXX;
  ldv $v05, 8, 16, $s1                               ## L:200  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 0, 16, $s1                               ## L:200  |    211 | m2:sint   = load(address, 0x10).xyzwxyzw;
  LABEL_mgfx_0003:
  vmulf $v28, $v28, $v20.v                           ## L:484  |      ^ | vlpos *= vnorm:sfract;
  addiu $sp, $sp, 65535                              ## L:493  | ***215 | light_count -= 1;
  vmulf $v29, $v27, $v28.h0                          ## L:485  |      ^ | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v27, $v28.h1                          ## L:486  |    216 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $fp, $fp, 16                                 ## L:492  |      ^ | light_ptr += 16;
  vmacf $v27, $v27, $v28.h2                          ## L:487  |    217 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v27, $v27, $v00                               ## L:488  | ***221 | vlcol = max(vlcol, VZERO);
  bne $sp, $zero, LIGHT_LOOP                         ## L:494  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v22, $v22, $v27.v                            ## L:490  | ****226 | vrgba:sint += vlcol;
  LIGHT_END:
  vmov $v22.e3, $v12.e3                              ## L:498  |    227 | vrgba.w = vnormmask.w;
  addu $t2, $t2, $t9                                 ## L:567  |      ^ | vtx_in1_ptr += vtx_size2;
  addu $s4, $s4, $t9                                 ## L:566  |    228 | vtx_in0_ptr += vtx_size2;
  ctc2 $t1, $vcc                                     ## L:502  |    229 | set_vcc(fog_mask);
  vmov $v22.e7, $v12.e3                              ## L:499  |      ^ | vrgba.W = vnormmask.w;
  LOAD_NORMAL571: lsv $v20, 0, 0, $s4                ## L:571  |    230 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  LOAD_POSITION573: ldv $v19, 0, 0, $s4              ## L:573  |    231 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  or $t5, $zero, $t3                                 ## L:565  |    232 | prev_out1 = vtx_out1_ptr;
  LOAD_POSITION574: ldv $v19, 8, 0, $t2              ## L:574  |    233 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  PATCH_COLOR500: vmulf $v22, $v22, $v23.v           ## L:500  |      ^ | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  sltu $at, $s4, $s6                                 ## L:578  |    234 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  or $t4, $zero, $s7                                 ## L:564  |    235 | prev_out0 = vtx_out0_ptr;
  LOAD_COLOR576: llv $v23, 4, 0, $t2                 ## L:576  |    236 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  addiu $t3, $t3, 80                                 ## L:569  |    237 | vtx_out1_ptr += 80;
  vmrg $v22, $v22, $v26.h2                           ## L:503  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v29, $v17, $v30.e7                          ## L:561  |    238 | VTEMP = vtexoffset:sint * 1;
  LOAD_COLOR575: llv $v23, 0, 0, $s4                 ## L:575  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmadh $v21, $v21, $v16.v                           ## L:562  |    239 | vtex:sint = vtex:sint +* vtexscale;
  LOAD_NORMAL572: lsv $v20, 8, 0, $t2                ## L:572  |      ^ | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  bne $at, $zero, VERTEX_LOOP                        ## L:578  |    240 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  addiu $s7, $s7, 80                                 ## L:568  |   *242 | vtx_out0_ptr += 80;
  sdv $v24, 8, 0, $t5                                ## L:585  |    243 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v22, 4, 8, $t5                                ## L:593  |    244 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  slv $v21, 8, 12, $t5                               ## L:596  |    245 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  suv $v22, 0, 8, $t4                                ## L:592  |    246 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v24, 0, 0, $t4                                ## L:584  |    247 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t7, 6($t5)                                     ## L:588  |    248 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $t6, 6($t4)                                     ## L:587  |    249 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $k0, 7($t4)                                     ## L:589  |    250 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sb $k1, 7($t5)                                     ## L:590  |    251 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  j RSPQ_Loop                                        ## L:597  |    252 | }
  slv $v21, 0, 12, $t4                               ## L:595  |   *254 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40

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