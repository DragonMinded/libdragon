## Auto-generated file, transpiled with RSPL
#define ENABLE_ENV_MAP 
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
    MgVertexAttributeLoaders LOAD_POSITION380, LOAD_POSITION381, LOAD_POSITION646, LOAD_POSITION645
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL378, LOAD_NORMAL379, LOAD_NORMAL643, LOAD_NORMAL644
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR382, LOAD_COLOR383, LOAD_COLOR647, LOAD_COLOR648
    MgBeginVertexAttributePatch PATCH_COLOR572
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:292  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:291  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v01, 0, 0, $at                                ## L:292  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:293  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:317  |   ***8 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vmudh $v04, $v02, $v01.v                           ## L:295  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  vsar $v03, COP2_ACC_HI                             ## L:295  |      9 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:309  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:295  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:296  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
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
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:326  |     29 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:334  |     30 | u16 mvpaddr = MATRICES_MVP;
  LOAD_POSITION380: ldv $v15, 0, 0, $s4              ## L:380  |     31 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:339  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:322  |     33 | vtx_size = load(MG_VERTEX_SIZE);
  addiu $t2, $s7, 40                                 ## L:364  |     34 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  ldv $v08, 0, 56, $t0                               ## L:279  |     35 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:325  |     36 | vtx_in_end = vtx_in0_ptr + vertices_size;
  sll $t9, $t8, 1                                    ## L:323  |     37 | vtx_size2 = vtx_size * 2;
  ldv $v04, 0, 40, $t0                               ## L:275  |     38 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:278  |     39 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:339  |     40 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:341  |     41 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:340  |     42 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:277  |     43 | m2:ufract = load(address, 0x30).xyzwxyzw;
  LOAD_NORMAL378: lsv $v16, 0, 0, $s4                ## L:378  |     44 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  ldv $v02, 0, 32, $t0                               ## L:273  |     45 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:274  |     46 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:339  |     47 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:277  |     48 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:341  |     49 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  addu $t1, $s4, $t8                                 ## L:363  |     50 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  LOAD_NORMAL379: lsv $v16, 8, 0, $t1                ## L:379  |     51 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  ldv $v04, 8, 40, $t0                               ## L:275  |     52 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_POSITION381: ldv $v15, 8, 0, $t1              ## L:381  |     53 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_COLOR382: llv $v19, 0, 0, $s4                 ## L:382  |     54 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v02, 8, 32, $t0                               ## L:273  |     55 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:278  |     56 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:340  |     57 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:343  |     58 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:274  |     59 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:343  |     60 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:276  |     61 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:279  |     62 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:344  |     63 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:272  |     64 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:343  |     65 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:344  |     66 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:360  |     67 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:276  |     68 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:272  |     69 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v14, 0, 0, $at                                ## L:360  |     70 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  lbu $t0, %lo(FOG_MASK + 0)                         ## L:361  |     71 | u8 fog_mask = load(FOG_MASK);
  ldv $v14, 8, 0, $at                                ## L:360  |     72 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  LOAD_COLOR383: llv $v19, 4, 0, $t1                 ## L:383  |     73 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:367  |     74 | u16 prev_out1 = MG_SCRATCH_MEM;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:366  |     75 | u16 prev_out0 = MG_SCRATCH_MEM;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h0                           ## L:417  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  suv $v18, 0, 8, $t3                                ## L:399  |     76 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v20, 8, 0, $t4                                ## L:392  |     77 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:405  |     78 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v19, 0, 0, $at                                ## L:405  |     79 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v16, $v16, $v13.v                           ## L:418  |      ^ | vnorm *= vnormfactor;
  vmudn $v29, $v02, $v15.h0                          ## L:421  |     80 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  suv $v18, 4, 8, $t4                                ## L:400  |      ^ | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  luv $v19, 0, 0, $at                                ## L:406  |     81 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadh $v29, $v01, $v15.h0                          ## L:421  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:430  |     82 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v06, $v15.h2                          ## L:422  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:422  |     83 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ldv $v18, 0, 0, $at                                ## L:430  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v04, $v15.h1                          ## L:423  |     84 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v18, 8, 0, $at                                ## L:430  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:423  |     85 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:449  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:449  |     86 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  sdv $v20, 0, 0, $t3                                ## L:391  |     87 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmadn $v21, $v08, $v30.e7                          ## L:424  |      ^ | vpos_clip = vmvp3 +* 1;
  ldv $v22, 8, 0, $at                                ## L:449  |     88 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v20, $v07, $v30.e7                          ## L:424  |      ^ | vpos_clip = vmvp3 +* 1;
  vmulf $v29, $v09, $v16.h0                          ## L:426  |     89 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  sb $t6, 6($t4)                                     ## L:395  |      ^ | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  lbu $k1, %lo(LIGHTING_COUNT + 0)                   ## L:431  |     90 | u8 light_count = load(LIGHTING_COUNT);
  vmacf $v29, $v10, $v16.h1                          ## L:427  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  vmacf $v16, $v11, $v16.h2                          ## L:428  |     91 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  slv $v17, 0, 12, $t3                               ## L:402  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmudn $v24, $v21, $v22.v                           ## L:450  |     92 | vec32 vguard = vpos_clip * vclip_factors;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:473  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmadh $v23, $v20, $v22.v                           ## L:450  |     93 | vec32 vguard = vpos_clip * vclip_factors;
  sb $t7, 7($t3)                                     ## L:396  |      ^ | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sb $k0, 7($t4)                                     ## L:397  |     94 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sdv $v20, 8, 24, $t2                               ## L:460  |     95 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v20, 0, 24, $s7                               ## L:459  |     96 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v21, 8, 32, $t2                               ## L:460  |     97 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vch $v29, $v23, $v23.h3                            ## L:451  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v24, $v24.h3                            ## L:451  |     98 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v21, 0, 32, $s7                               ## L:459  |      ^ | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  cfc2 $sp, $vcc                                     ## L:451  |     99 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:455  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sb $t5, 6($t3)                                     ## L:394  |    100 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vcl $v29, $v21, $v21.h3                            ## L:455  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v24, 0, 8, $at                                ## L:474  |    101 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vaddc $v23, $v21, $v14.h1                          ## L:464  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v22, $v20, $v14.h0                           ## L:465  |    102 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  srl $fp, $sp, 4                                    ## L:258  |      ^ | u16 codes2 = codes >> 4;
  andi $s0, $sp, 1799                                ## L:238  |    103 | t0 = codes & 0x707;
  srl $s1, $s0, 5                                    ## L:239  |    104 | t1 = t0 >> 5;
  or $t5, $s0, $s1                                   ## L:246  |    105 | result = t0 | t1;
  vmudn $v29, $v23, $v14.h2                          ## L:467  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  andi $s0, $fp, 1799                                ## L:238  |    106 | t0 = codes & 0x707;
  vmadh $v22, $v22, $v14.h2                          ## L:468  |      ^ | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:473  |    107 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  cfc2 $fp, $vcc                                     ## L:455  |    108 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v23, 8, 0, $at                                ## L:473  |    109 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  srl $s1, $s0, 5                                    ## L:239  |    110 | t1 = t0 >> 5;
  or $t6, $s0, $s1                                   ## L:246  |    111 | result = t0 | t1;
  andi $s1, $fp, 1799                                ## L:238  |    112 | t0 = codes & 0x707;
  vge $v22, $v00, $v22.h2                            ## L:469  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  srl $s2, $s1, 5                                    ## L:239  |    113 | t1 = t0 >> 5;
  vmudl $v21, $v21, $v23.h3                          ## L:477  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v20, $v20, $v23.h3                          ## L:477  |    114 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  nor $t7, $s1, $s2                                  ## L:253  |      ^ | result = t0 ~| t1;
  srl $s0, $fp, 4                                    ## L:265  |    115 | u16 codes2 = codes >> 4;
  vmadn $v21, $v00, $v00                             ## L:477  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ori $sp, $zero, %lo(MATRICES_MV)                   ## L:506  |    116 | u16 addr = MATRICES_MV;
  ldv $v04, 0, 40, $sp                               ## L:275  |    117 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v04, 8, 40, $sp                               ## L:275  |    118 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v07, 0, 24, $sp                               ## L:278  |    119 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 0, 56, $sp                               ## L:279  |    120 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcph $v25.e3, $v20.e3                             ## L:484  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v07, 8, 24, $sp                               ## L:278  |    121 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrcpl $v26.e3, $v21.e3                             ## L:484  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v05, 0, 16, $sp                               ## L:276  |    122 | m2:sint   = load(address, 0x10).xyzwxyzw;
  andi $s1, $s0, 1799                                ## L:238  |    123 | t0 = codes & 0x707;
  vrcph $v25.e3, $v20.e7                             ## L:485  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v01, 0, 0, $sp                                ## L:272  |    124 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ssv $v20, 6, 16, $s7                               ## L:480  |    125 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcpl $v26.e7, $v21.e7                             ## L:485  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v02, 0, 32, $sp                               ## L:273  |    126 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrcph $v25.e7, $v00.e7                             ## L:485  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 14, 16, $t2                              ## L:481  |    127 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v24, 8, 8, $at                                ## L:474  |    128 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ssv $v21, 14, 18, $t2                              ## L:481  |    129 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v06, 0, 48, $sp                               ## L:277  |    130 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ssv $v21, 6, 18, $s7                               ## L:480  |    131 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v01, 8, 0, $sp                                ## L:272  |    132 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmudl $v29, $v21, $v26.h3                          ## L:490  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 6, 20, $s7                               ## L:487  |    133 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ldv $v03, 0, 8, $sp                                ## L:274  |    134 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmadm $v29, $v20, $v26.h3                          ## L:490  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  srl $s2, $s1, 5                                    ## L:239  |    135 | t1 = t0 >> 5;
  vmadn $v21, $v21, $v25.h3                          ## L:490  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v06, 8, 48, $sp                               ## L:277  |    136 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v20, $v20, $v25.h3                          ## L:490  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v02, 8, 32, $sp                               ## L:273  |    137 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 8, 8, $sp                                ## L:274  |    138 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:494  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v21, $v21, $v23.v                           ## L:495  |    139 | vpos_clip = vpos_clip +* vviewscale;
  nor $k0, $s1, $s2                                  ## L:253  |      ^ | result = t0 ~| t1;
  ldv $v08, 8, 56, $sp                               ## L:279  |    140 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v20, $v20, $v23.v                           ## L:495  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  ssv $v25, 14, 20, $t2                              ## L:488  |    141 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmudn $v29, $v02, $v15.h0                          ## L:509  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v15.h0                          ## L:509  |    142 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v05, 8, 16, $sp                               ## L:276  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  slv $v17, 8, 12, $t4                               ## L:403  |    143 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmadn $v29, $v04, $v15.h1                          ## L:510  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadh $v29, $v03, $v15.h1                          ## L:510  |    144 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)               ## L:515  |      ^ | u16 light_ptr = LIGHTING_LIGHTS;
  vmadn $v29, $v06, $v15.h2                          ## L:511  |    145 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v26, 6, 22, $s7                               ## L:487  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v26, 14, 22, $t2                              ## L:488  |    146 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v29, $v05, $v15.h2                          ## L:511  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e7                          ## L:512  |    147 | vpos = vmvp3 +* 1;
  beq $k1, $zero, LIGHT_END                          ## L:516  |      ^ | if (light_count == 0) goto LIGHT_END;
  vmadh $v15, $v07, $v30.e7                          ## L:512  |   *149 | vpos = vmvp3 +* 1;
  LIGHT_LOOP:
  ldv $v23, 0, 8, $sp                                ## L:519  |    150 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  lh $fp, 6($sp)                                     ## L:522  |    151 | s16 light_w = load(light_ptr, 6);
  ldv $v24, 0, 0, $sp                                ## L:520  |    152 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $sp                                ## L:520  |    153 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $fp, $zero, LABEL_mgfx_0002                    ## L:523  |    154 | if (light_w != 0)
  ldv $v23, 8, 8, $sp                                ## L:519  |   *156 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  vsubc $v24, $v24, $v15.v                           ## L:531  |    157 | vlpos -= vpos;
  vmudh $v02, $v24, $v24.v                           ## L:532  | ***161 | vec32 vsqdist = vlpos * vlpos;
  vsar $v02, COP2_ACC_MD                             ## L:532  |    162 | vec32 vsqdist = vlpos * vlpos;
  vsar $v01, COP2_ACC_HI                             ## L:532  |    163 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v04, $v02, $v02.h1                          ## L:533  |  **166 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v03, $v01, $v01.h1                           ## L:533  |    167 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v02, $v04, $v02.h2                          ## L:534  |  **170 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v01, $v03, $v01.h2                           ## L:534  |    171 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v03.e0, $v01.e0                             ## L:536  | ***175 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v04.e0, $v02.e0                             ## L:536  |    176 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v03.e0, $v01.e4                             ## L:537  |    177 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v04.e4, $v02.e4                             ## L:537  |    178 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v03.e4, $v00.e0                             ## L:537  |    179 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v24, $v04.h0                          ## L:539  |  **182 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v24, $v24, $v03.h0                          ## L:539  |    183 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v04, $v23.h3                          ## L:540  |    184 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmadh $v05, $v03, $v23.h3                          ## L:540  |    185 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmulf $v05, $v05, $v05.v                           ## L:541  | ***189 | vatten:sfract *= vatten:sfract;
  vmulf $v23, $v23, $v05.h0                          ## L:542  | ***193 | vlcol:sfract *= vatten:sfract.xxxxXXXX;
  LABEL_mgfx_0002:
  vmulf $v24, $v24, $v16.v                           ## L:556  |    194 | vlpos *= vnorm:sfract;
  vmulf $v29, $v23, $v24.h0                          ## L:557  | ***198 | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v23, $v24.h1                          ## L:558  |    199 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $sp, $sp, 16                                 ## L:564  |      ^ | light_ptr += 16;
  vmacf $v23, $v23, $v24.h2                          ## L:559  |    200 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $k1, $k1, 65535                              ## L:565  |      ^ | light_count -= 1;
  vge $v23, $v23, $v00                               ## L:560  | ***204 | vlcol = max(vlcol, VZERO);
  bne $k1, $zero, LIGHT_LOOP                         ## L:566  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v23.v                            ## L:562  | ****209 | vrgba:sint += vlcol;
  LIGHT_END:
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:626  |    210 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  vmov $v18.e3, $v12.e3                              ## L:570  |      ^ | vrgba.w = vnormmask.w;
  vmov $v18.e7, $v12.e3                              ## L:571  |    211 | vrgba.W = vnormmask.w;
  ctc2 $t0, $vcc                                     ## L:574  |      ^ | set_vcc(fog_mask);
  PATCH_COLOR572: vmulf $v18, $v18, $v19.v           ## L:572  | ***215 | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  ori $fp, $zero, %lo(MATRICES_MVP)                  ## L:580  |      ^ | u16 addr = MATRICES_MVP;
  ldv $v07, 8, 24, $fp                               ## L:278  | ***219 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmrg $v18, $v18, $v22.h2                           ## L:575  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v23, $v15, $v15.v                           ## L:584  |    220 | vec32 vsqdist = vpos * vpos;
  ldv $v04, 0, 40, $fp                               ## L:275  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vsar $v23, COP2_ACC_MD                             ## L:584  |    221 | vec32 vsqdist = vpos * vpos;
  addu $t1, $t1, $t9                                 ## L:639  |      ^ | vtx_in1_ptr += vtx_size2;
  or $t3, $zero, $s7                                 ## L:636  |    222 | prev_out0 = vtx_out0_ptr;
  vsar $v22, COP2_ACC_HI                             ## L:584  |      ^ | vec32 vsqdist = vpos * vpos;
  ldv $v03, 8, 8, $fp                                ## L:274  |    223 | m1:sint   = load(address, 0x08).xyzwxyzw;
  addiu $s7, $s7, 80                                 ## L:640  |   *225 | vtx_out0_ptr += 80;
  vaddc $v25, $v23, $v23.h1                          ## L:585  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v24, $v22, $v22.h1                           ## L:585  |    226 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v07, 0, 24, $fp                               ## L:278  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vaddc $v23, $v25, $v23.h2                          ## L:586  |  **229 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v24, $v22.h2                           ## L:586  |    230 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v24.e0, $v22.e0                             ## L:588  | ***234 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v25.e0, $v23.e0                             ## L:588  |    235 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v02, 0, 32, $fp                               ## L:273  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrsqh $v24.e0, $v22.e4                             ## L:589  |    236 | vtmp.X = invert_half_sqrt(vsqdist).X;
  addu $s4, $s4, $t9                                 ## L:638  |      ^ | vtx_in0_ptr += vtx_size2;
  ldv $v02, 8, 32, $fp                               ## L:273  |    237 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrsql $v25.e4, $v23.e4                             ## L:589  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v24.e4, $v00.e0                             ## L:589  |    238 | vtmp.X = invert_half_sqrt(vsqdist).X;
  LOAD_COLOR647: llv $v19, 0, 0, $s4                 ## L:647  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmudm $v29, $v15, $v25.h0                          ## L:591  |  **241 | vpos *= vtmp.xxxxXXXX;
  vmadh $v15, $v15, $v24.h0                          ## L:591  |    242 | vpos *= vtmp.xxxxXXXX;
  vmulf $v24, $v15, $v16.v                           ## L:594  | ***246 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:595  | ***250 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:596  | ***254 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  vlt $v24, $v24, $v00                               ## L:598  | ***258 | vdot = min(vdot, VZERO);
  LOAD_COLOR648: llv $v19, 4, 0, $t1                 ## L:648  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vor $v25, $v00, $v00                               ## L:601  |    259 | vec16 vzunit = VZERO;
  ldv $v08, 0, 56, $fp                               ## L:279  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v08, 8, 56, $fp                               ## L:279  |    260 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmov $v25.e2, $v12.e3                              ## L:602  |      ^ | vzunit.z = vnormmask.w;
  vsubc $v24, $v00, $v24.v                           ## L:599  |   *262 | vdot = VZERO - vdot;
  ldv $v06, 8, 48, $fp                               ## L:277  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:277  |    263 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmov $v25.e6, $v12.e3                              ## L:603  |      ^ | vzunit.Z = vnormmask.w;
  ldv $v03, 0, 8, $fp                                ## L:274  |  **266 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmulf $v29, $v16, $v24.h0                          ## L:605  |      ^ | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v16, $v24.h0                          ## L:606  |    267 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  vmadh $v29, $v15, $v30.e7                          ## L:607  |    268 | VTEMP = vpos:sint +* 1;
  LOAD_NORMAL643: lsv $v16, 0, 0, $s4                ## L:643  |      ^ | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  vmadh $v29, $v25, $v30.e7                          ## L:608  |    269 | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:611  |    270 | vec32 vrefl = get_acc();
  ldv $v04, 8, 40, $fp                               ## L:275  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vsar $v24, COP2_ACC_HI                             ## L:611  |    271 | vec32 vrefl = get_acc();
  vmudl $v29, $v25, $v25.v                           ## L:613  |  **274 | vsqdist = vrefl * vrefl;
  vmadm $v29, $v24, $v25.v                           ## L:613  |    275 | vsqdist = vrefl * vrefl;
  vmadn $v23, $v25, $v24.v                           ## L:613  |    276 | vsqdist = vrefl * vrefl;
  vmadh $v22, $v24, $v24.v                           ## L:613  |    277 | vsqdist = vrefl * vrefl;
  vaddc $v27, $v23, $v23.h1                          ## L:614  |  **280 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v05, 8, 16, $fp                               ## L:276  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  or $t4, $zero, $t2                                 ## L:637  |    281 | prev_out1 = vtx_out1_ptr;
  vadd $v26, $v22, $v22.h1                           ## L:614  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vxor $v17, $v00, $v30.e0                           ## L:620  |    282 | vtex = 0x80;
  addiu $t2, $t2, 80                                 ## L:641  |      ^ | vtx_out1_ptr += 80;
  vaddc $v23, $v27, $v23.h2                          ## L:615  |   *284 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v05, 0, 16, $fp                               ## L:276  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vadd $v22, $v26, $v22.h2                           ## L:615  |    285 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v26.e0, $v22.e0                             ## L:617  | ***289 | vtmp.x = invert_half_sqrt(vsqdist).x;
  LOAD_POSITION646: ldv $v15, 8, 0, $t1              ## L:646  |      ^ | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vrsql $v27.e0, $v23.e0                             ## L:617  |    290 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v26.e0, $v22.e4                             ## L:618  |    291 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 0, 0, $at                                ## L:626  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  vrsql $v27.e4, $v23.e4                             ## L:618  |    292 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v01, 0, 0, $fp                                ## L:272  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrsqh $v26.e4, $v00.e0                             ## L:618  |    293 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 8, 0, $at                                ## L:627  |      ^ | vtexscale.XY = load(TEXTURING_SCALE).xy;
  vmudh $v29, $v17, $v30.e7                          ## L:621  |    294 | VTEMP = vtex:sint * 1;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:628  |      ^ | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 0, 0, $at                                ## L:628  |    295 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 8, 0, $at                                ## L:629  |    296 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  vmadl $v29, $v25, $v27.h0                          ## L:622  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v01, 8, 0, $fp                                ## L:272  |    297 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadm $v29, $v24, $v27.h0                          ## L:622  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  sltu $at, $s4, $s6                                 ## L:650  |    298 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadn $v29, $v25, $v26.h0                          ## L:622  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  vmadh $v17, $v24, $v26.h0                          ## L:622  |    299 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_NORMAL644: lsv $v16, 8, 0, $t1                ## L:644  |      ^ | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  LOAD_POSITION645: ldv $v15, 0, 0, $s4              ## L:645  |    300 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vmudh $v29, $v23, $v30.e7                          ## L:633  |      ^ | VTEMP = vtexoffset:sint * 1;
  bne $at, $zero, VERTEX_LOOP                        ## L:650  |    301 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:634  |  **304 | vtex:sint = vtex:sint +* vtexscale;
  sdv $v20, 0, 0, $t3                                ## L:656  |    305 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t7, 7($t3)                                     ## L:661  |    306 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sdv $v20, 8, 0, $t4                                ## L:657  |    307 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v18, 4, 8, $t4                                ## L:665  |    308 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  slv $v17, 8, 12, $t4                               ## L:668  |    309 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  suv $v18, 0, 8, $t3                                ## L:664  |    310 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sb $t6, 6($t4)                                     ## L:660  |    311 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $k0, 7($t4)                                     ## L:662  |    312 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sb $t5, 6($t3)                                     ## L:659  |    313 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  j RSPQ_Loop                                        ## L:669  |    314 | }
  slv $v17, 0, 12, $t3                               ## L:667  |   *316 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40

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