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
    MgVertexAttributeLoaders LOAD_POSITION304, LOAD_POSITION305, LOAD_POSITION574, LOAD_POSITION573
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL302, LOAD_NORMAL303, LOAD_NORMAL572, LOAD_NORMAL571
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR306, LOAD_COLOR307, LOAD_COLOR576, LOAD_COLOR575
    MgBeginVertexAttributePatch PATCH_COLOR500
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:215  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:217  |      2 | vec16 vinputs = load(inputs_ptr).xyzw;
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:216  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v01, 0, 0, $at                                ## L:216  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:219  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:233  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v03, COP2_ACC_HI                             ## L:219  |      9 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:220  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v04, COP2_ACC_MD                             ## L:219  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:241  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ssv $v03, 0, 8, $at                                ## L:221  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:221  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:231  |     15 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:220  |     16 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:227  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:229  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:236  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:234  |     20 | vtx_buffer += buffer_offset;
  addu $t0, $t1, $t3                                 ## L:237  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:238  |     22 | dma_size += 0x7;
  addiu $t2, $zero, 12                               ## L:244  |     23 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  andi $t0, $t0, 4088                                ## L:239  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:242  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:244  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:244  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:258  |     29 | u16 mvpaddr = MATRICES_MVP;
  addu $s6, $s4, $t1                                 ## L:249  |     30 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:263  |     31 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:263  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:203  |     33 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:265  |     34 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:265  |     35 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:196  |     36 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LOAD_POSITION304: ldv $v15, 0, 0, $s4              ## L:304  |     37 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v09, 8, 0, $at                                ## L:263  |     38 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:198  |     39 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:264  |     40 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:264  |     41 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:267  |     42 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:201  |     43 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:201  |     44 | m2:ufract = load(address, 0x30).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:246  |     45 | vtx_size = load(MG_VERTEX_SIZE);
  LOAD_NORMAL302: lsv $v16, 0, 0, $s4                ## L:302  |     46 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  ldv $v04, 0, 40, $t0                               ## L:199  |     47 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_COLOR306: llv $v19, 0, 0, $s4                 ## L:306  |     48 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  addu $t1, $s4, $t8                                 ## L:287  |     49 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ldv $v13, 0, 8, $at                                ## L:268  |     50 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:197  |     51 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:198  |     52 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:267  |     53 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:267  |     54 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:268  |     55 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:284  |     56 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:200  |     57 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:291  |     58 | u16 prev_out1 = MG_SCRATCH_MEM;
  ldv $v07, 0, 24, $t0                               ## L:202  |     59 | m3:sint   = load(address, 0x18).xyzwxyzw;
  LOAD_NORMAL303: lsv $v16, 8, 0, $t1                ## L:303  |     60 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  ldv $v14, 0, 0, $at                                ## L:284  |     61 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:199  |     62 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:196  |     63 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LOAD_POSITION305: ldv $v15, 8, 0, $t1              ## L:305  |     64 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v05, 8, 16, $t0                               ## L:200  |     65 | m2:sint   = load(address, 0x10).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:250  |     66 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v02, 8, 32, $t0                               ## L:197  |     67 | m0:ufract = load(address, 0x20).xyzwxyzw;
  addiu $t2, $s7, 40                                 ## L:288  |     68 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  ldv $v08, 8, 56, $t0                               ## L:203  |     69 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:202  |     70 | m3:sint   = load(address, 0x18).xyzwxyzw;
  LOAD_COLOR307: llv $v19, 4, 0, $t1                 ## L:307  |     71 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  lbu $t0, %lo(FOG_MASK + 0)                         ## L:285  |     72 | u8 fog_mask = load(FOG_MASK);
  sll $t9, $t8, 1                                    ## L:247  |     73 | vtx_size2 = vtx_size * 2;
  ldv $v14, 8, 0, $at                                ## L:284  |     74 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:290  |     75 | u16 prev_out0 = MG_SCRATCH_MEM;
  VERTEX_LOOP:
  vmov $v15.e7, $v30.e2                              ## L:344  |      ^ | vpos.W = 0x20;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:329  |     76 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vand $v16, $v12, $v16.h0                           ## L:341  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  vmov $v15.e3, $v30.e2                              ## L:343  |     77 | vpos.w = 0x20;
  sdv $v20, 0, 0, $t3                                ## L:315  |      ^ | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v18, 0, 8, $t3                                ## L:323  |     78 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v19, 0, 0, $at                                ## L:329  |     79 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  luv $v19, 0, 0, $at                                ## L:330  |     80 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v16, $v16, $v13.v                           ## L:346  |      ^ | vnorm *= vnormfactor;
  vmudn $v29, $v02, $v15.h0                          ## L:349  |     81 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  suv $v18, 4, 8, $t4                                ## L:324  |      ^ | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadh $v29, $v01, $v15.h0                          ## L:349  |     82 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:358  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v06, $v15.h2                          ## L:350  |     83 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  sdv $v20, 8, 0, $t4                                ## L:316  |      ^ | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  ldv $v18, 0, 0, $at                                ## L:358  |     84 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v05, $v15.h2                          ## L:350  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v04, $v15.h1                          ## L:351  |     85 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v18, 8, 0, $at                                ## L:358  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:351  |     86 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:377  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v21, $v08, $v15.h3                          ## L:352  |     87 | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  ldv $v22, 0, 0, $at                                ## L:377  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  sb $t7, 7($t3)                                     ## L:320  |     88 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  ldv $v22, 8, 0, $at                                ## L:377  |     89 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v20, $v07, $v15.h3                          ## L:352  |      ^ | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  sb $t6, 6($t4)                                     ## L:319  |     90 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vmulf $v29, $v09, $v16.h0                          ## L:354  |      ^ | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  vmacf $v29, $v10, $v16.h1                          ## L:355  |     91 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:401  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmacf $v16, $v11, $v16.h2                          ## L:356  |     92 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudn $v24, $v21, $v22.v                           ## L:378  |     93 | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v20, 8, 24, $t2                               ## L:388  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vmadh $v23, $v20, $v22.v                           ## L:378  |     94 | vec32 vguard = vpos_clip * vclip_factors;
  slv $v17, 0, 12, $t3                               ## L:326  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $k0, 7($t4)                                     ## L:321  |     95 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sdv $v20, 0, 24, $s7                               ## L:387  |     96 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v21, 0, 32, $s7                               ## L:387  |     97 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sb $t5, 6($t3)                                     ## L:318  |     98 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vch $v29, $v23, $v23.h3                            ## L:379  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v24, $v24.h3                            ## L:379  |     99 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v21, 8, 32, $t2                               ## L:388  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  cfc2 $sp, $vcc                                     ## L:379  |    100 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:383  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  lbu $k1, %lo(LIGHTING_COUNT + 0)                   ## L:359  |    101 | u8 light_count = load(LIGHTING_COUNT);
  vcl $v29, $v21, $v21.h3                            ## L:383  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vaddc $v23, $v21, $v14.h1                          ## L:392  |    102 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  ldv $v24, 0, 8, $at                                ## L:402  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vadd $v22, $v20, $v14.h0                           ## L:393  |    103 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  srl $fp, $sp, 4                                    ## L:182  |      ^ | u16 codes2 = codes >> 4;
  andi $s0, $sp, 1799                                ## L:162  |    104 | t0 = codes & 0x707;
  srl $s1, $s0, 5                                    ## L:163  |    105 | t1 = t0 >> 5;
  or $t5, $s0, $s1                                   ## L:170  |    106 | result = t0 | t1;
  vmudn $v29, $v23, $v14.h2                          ## L:395  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v22, $v22, $v14.h2                          ## L:396  |    107 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:401  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:401  |    108 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ori $sp, $zero, %lo(MATRICES_MV)                   ## L:434  |    109 | u16 addr = MATRICES_MV;
  andi $s0, $fp, 1799                                ## L:162  |    110 | t0 = codes & 0x707;
  cfc2 $fp, $vcc                                     ## L:383  |    111 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v22, $v00, $v22.h2                            ## L:397  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  ldv $v01, 0, 0, $sp                                ## L:196  |    112 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmudl $v21, $v21, $v23.h3                          ## L:405  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  srl $s1, $s0, 5                                    ## L:163  |    113 | t1 = t0 >> 5;
  vmadm $v20, $v20, $v23.h3                          ## L:405  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v07, 0, 24, $sp                               ## L:202  |    114 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v02, 0, 32, $sp                               ## L:197  |    115 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadn $v21, $v00, $v00                             ## L:405  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v05, 0, 16, $sp                               ## L:200  |    116 | m2:sint   = load(address, 0x10).xyzwxyzw;
  or $t6, $s0, $s1                                   ## L:170  |    117 | result = t0 | t1;
  ldv $v24, 8, 8, $at                                ## L:402  |    118 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vrcph $v25.e3, $v20.e3                             ## L:412  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcpl $v26.e3, $v21.e3                             ## L:412  |    119 | vinvw.w = invert_half(vpos_clip).w;
  ssv $v20, 14, 16, $t2                              ## L:409  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  andi $s1, $fp, 1799                                ## L:162  |    120 | t0 = codes & 0x707;
  vrcph $v25.e3, $v20.e7                             ## L:413  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v01, 8, 0, $sp                                ## L:196  |    121 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrcpl $v26.e7, $v21.e7                             ## L:413  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v25.e7, $v00.e7                             ## L:413  |    122 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 6, 16, $s7                               ## L:408  |      ^ | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  srl $s2, $s1, 5                                    ## L:163  |    123 | t1 = t0 >> 5;
  srl $s0, $fp, 4                                    ## L:189  |    124 | u16 codes2 = codes >> 4;
  ldv $v06, 0, 48, $sp                               ## L:201  |    125 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ssv $v21, 6, 18, $s7                               ## L:408  |    126 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v03, 0, 8, $sp                                ## L:198  |    127 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudl $v29, $v21, $v26.h3                          ## L:418  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v20, $v26.h3                          ## L:418  |    128 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v21, 14, 18, $t2                              ## L:409  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vmadn $v21, $v21, $v25.h3                          ## L:418  |    129 | vpos_clip *= vinvw.wwwwWWWW;
  nor $t7, $s1, $s2                                  ## L:177  |      ^ | result = t0 ~| t1;
  vmadh $v20, $v20, $v25.h3                          ## L:418  |    130 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v04, 0, 40, $sp                               ## L:199  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v04, 8, 40, $sp                               ## L:199  |    131 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v02, 8, 32, $sp                               ## L:197  |    132 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v06, 8, 48, $sp                               ## L:201  |    133 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 0, 56, $sp                               ## L:203  |    134 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:422  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v21, $v21, $v23.v                           ## L:423  |    135 | vpos_clip = vpos_clip +* vviewscale;
  ldv $v05, 8, 16, $sp                               ## L:200  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v20, $v20, $v23.v                           ## L:423  |    136 | vpos_clip = vpos_clip +* vviewscale;
  ldv $v08, 8, 56, $sp                               ## L:203  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v03, 8, 8, $sp                                ## L:198  |    137 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudn $v29, $v02, $v15.h0                          ## L:437  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v15.h0                          ## L:437  |    138 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  andi $s1, $s0, 1799                                ## L:162  |      ^ | t0 = codes & 0x707;
  ldv $v07, 8, 24, $sp                               ## L:202  |    139 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)               ## L:443  |    140 | u16 light_ptr = LIGHTING_LIGHTS;
  srl $s2, $s1, 5                                    ## L:163  |    141 | t1 = t0 >> 5;
  vmadn $v29, $v04, $v15.h1                          ## L:438  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  nor $k0, $s1, $s2                                  ## L:177  |    142 | result = t0 ~| t1;
  vmadh $v29, $v03, $v15.h1                          ## L:438  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  slv $v17, 8, 12, $t4                               ## L:327  |    143 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmadn $v29, $v06, $v15.h2                          ## L:439  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:439  |    144 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v25, 14, 20, $t2                              ## L:416  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v29, $v08, $v15.h3                          ## L:440  |    145 | vpos = vmvp3 +* vpos.wwwwWWWW;
  ssv $v26, 14, 22, $t2                              ## L:416  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v15, $v07, $v15.h3                          ## L:440  |    146 | vpos = vmvp3 +* vpos.wwwwWWWW;
  ssv $v25, 6, 20, $s7                               ## L:415  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  beq $k1, $zero, LIGHT_END                          ## L:444  |    147 | if (light_count == 0) goto LIGHT_END;
  ssv $v26, 6, 22, $s7                               ## L:415  |   *149 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  LIGHT_LOOP:
  lh $fp, 6($sp)                                     ## L:450  |    150 | s16 light_w = load(light_ptr, 6);
  ldv $v24, 0, 0, $sp                                ## L:448  |    151 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $sp                                ## L:448  |    152 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v23, 0, 8, $sp                                ## L:447  |    153 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  beq $fp, $zero, LABEL_mgfx_0002                    ## L:451  |    154 | if (light_w != 0)
  ldv $v23, 8, 8, $sp                                ## L:447  |   *156 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  vsubc $v24, $v24, $v15.v                           ## L:459  |    157 | vlpos -= vpos;
  vmudh $v02, $v24, $v24.v                           ## L:460  | ***161 | vec32 vsqdist = vlpos * vlpos;
  vsar $v02, COP2_ACC_MD                             ## L:460  |    162 | vec32 vsqdist = vlpos * vlpos;
  vsar $v01, COP2_ACC_HI                             ## L:460  |    163 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v04, $v02, $v02.h1                          ## L:461  |  **166 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v03, $v01, $v01.h1                           ## L:461  |    167 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v02, $v04, $v02.h2                          ## L:462  |  **170 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v01, $v03, $v01.h2                           ## L:462  |    171 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v03.e0, $v01.e0                             ## L:464  | ***175 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v04.e0, $v02.e0                             ## L:464  |    176 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v03.e0, $v01.e4                             ## L:465  |    177 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v04.e4, $v02.e4                             ## L:465  |    178 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v03.e4, $v00.e0                             ## L:465  |    179 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v24, $v04.h0                          ## L:467  |  **182 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v24, $v24, $v03.h0                          ## L:467  |    183 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v04, $v23.h3                          ## L:468  |    184 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmadh $v05, $v03, $v23.h3                          ## L:468  |    185 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmulf $v05, $v05, $v05.v                           ## L:469  | ***189 | vatten:sfract *= vatten:sfract;
  vmulf $v23, $v23, $v05.h0                          ## L:470  | ***193 | vlcol:sfract *= vatten:sfract.xxxxXXXX;
  LABEL_mgfx_0002:
  addiu $sp, $sp, 16                                 ## L:492  |      ^ | light_ptr += 16;
  vmulf $v24, $v24, $v16.v                           ## L:484  |    194 | vlpos *= vnorm:sfract;
  vmulf $v29, $v23, $v24.h0                          ## L:485  | ***198 | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v23, $v24.h1                          ## L:486  |    199 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $k1, $k1, 65535                              ## L:493  |      ^ | light_count -= 1;
  vmacf $v23, $v23, $v24.h2                          ## L:487  |    200 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v23, $v23, $v00                               ## L:488  | ***204 | vlcol = max(vlcol, VZERO);
  bne $k1, $zero, LIGHT_LOOP                         ## L:494  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v23.v                            ## L:490  | ****209 | vrgba:sint += vlcol;
  LIGHT_END:
  vmov $v18.e3, $v12.e3                              ## L:498  |    210 | vrgba.w = vnormmask.w;
  ctc2 $t0, $vcc                                     ## L:502  |      ^ | set_vcc(fog_mask);
  vmov $v18.e7, $v12.e3                              ## L:499  |    211 | vrgba.W = vnormmask.w;
  ori $fp, $zero, %lo(MATRICES_MVP)                  ## L:508  |      ^ | u16 addr = MATRICES_MVP;
  PATCH_COLOR500: vmulf $v18, $v18, $v19.v           ## L:500  | ***215 | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  ldv $v03, 8, 8, $fp                                ## L:198  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmrg $v18, $v18, $v22.h2                           ## L:503  | ***219 | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v23, $v15, $v15.v                           ## L:512  |    220 | vec32 vsqdist = vpos * vpos;
  or $t3, $zero, $s7                                 ## L:564  |      ^ | prev_out0 = vtx_out0_ptr;
  vsar $v23, COP2_ACC_MD                             ## L:512  |    221 | vec32 vsqdist = vpos * vpos;
  addu $t1, $t1, $t9                                 ## L:567  |      ^ | vtx_in1_ptr += vtx_size2;
  vsar $v22, COP2_ACC_HI                             ## L:512  |    222 | vec32 vsqdist = vpos * vpos;
  ldv $v04, 8, 40, $fp                               ## L:199  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:201  |    223 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v02, 0, 32, $fp                               ## L:197  |   *225 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vaddc $v25, $v23, $v23.h1                          ## L:513  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v24, $v22, $v22.h1                           ## L:513  |    226 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  or $t4, $zero, $t2                                 ## L:565  |      ^ | prev_out1 = vtx_out1_ptr;
  vaddc $v23, $v25, $v23.h2                          ## L:514  |  **229 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v24, $v22.h2                           ## L:514  |    230 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  addiu $s7, $s7, 80                                 ## L:568  |      ^ | vtx_out0_ptr += 80;
  ldv $v06, 8, 48, $fp                               ## L:201  |    231 | m2:ufract = load(address, 0x30).xyzwxyzw;
  addiu $t2, $t2, 80                                 ## L:569  |  **234 | vtx_out1_ptr += 80;
  vrsqh $v24.e0, $v22.e0                             ## L:516  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v25.e0, $v23.e0                             ## L:516  |    235 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v04, 0, 40, $fp                               ## L:199  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrsqh $v24.e0, $v22.e4                             ## L:517  |    236 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v25.e4, $v23.e4                             ## L:517  |    237 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v24.e4, $v00.e0                             ## L:517  |    238 | vtmp.X = invert_half_sqrt(vsqdist).X;
  LOAD_COLOR576: llv $v19, 4, 0, $t1                 ## L:576  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmudm $v29, $v15, $v25.h0                          ## L:519  |  **241 | vpos *= vtmp.xxxxXXXX;
  vmadh $v15, $v15, $v24.h0                          ## L:519  |    242 | vpos *= vtmp.xxxxXXXX;
  vmulf $v24, $v15, $v16.v                           ## L:522  | ***246 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:523  | ***250 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:524  | ***254 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  ldv $v01, 0, 0, $fp                                ## L:196  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vlt $v24, $v24, $v00                               ## L:526  | ***258 | vdot = min(vdot, VZERO);
  vor $v25, $v00, $v00                               ## L:529  |    259 | vec16 vzunit = VZERO;
  vmov $v25.e6, $v12.e3                              ## L:531  |    260 | vzunit.Z = vnormmask.w;
  vmov $v25.e2, $v12.e3                              ## L:530  |    261 | vzunit.z = vnormmask.w;
  ldv $v01, 8, 0, $fp                                ## L:196  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vsubc $v24, $v00, $v24.v                           ## L:527  |    262 | vdot = VZERO - vdot;
  vmulf $v29, $v16, $v24.h0                          ## L:533  | ***266 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v16, $v24.h0                          ## L:534  |    267 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  LOAD_NORMAL572: lsv $v16, 8, 0, $t1                ## L:572  |      ^ | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  vmadh $v29, $v15, $v30.e7                          ## L:535  |    268 | VTEMP = vpos:sint +* 1;
  vmadh $v29, $v25, $v30.e7                          ## L:536  |    269 | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:539  |    270 | vec32 vrefl = get_acc();
  vsar $v24, COP2_ACC_HI                             ## L:539  |    271 | vec32 vrefl = get_acc();
  addu $s4, $s4, $t9                                 ## L:566  |      ^ | vtx_in0_ptr += vtx_size2;
  vmudl $v29, $v25, $v25.v                           ## L:541  |  **274 | vsqdist = vrefl * vrefl;
  ldv $v07, 8, 24, $fp                               ## L:202  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadm $v29, $v24, $v25.v                           ## L:541  |    275 | vsqdist = vrefl * vrefl;
  ldv $v07, 0, 24, $fp                               ## L:202  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v23, $v25, $v24.v                           ## L:541  |    276 | vsqdist = vrefl * vrefl;
  vmadh $v22, $v24, $v24.v                           ## L:541  |    277 | vsqdist = vrefl * vrefl;
  vaddc $v27, $v23, $v23.h1                          ## L:542  |  **280 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v26, $v22, $v22.h1                           ## L:542  |    281 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  LOAD_POSITION574: ldv $v15, 8, 0, $t1              ## L:574  |      ^ | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vaddc $v23, $v27, $v23.h2                          ## L:543  |  **284 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v02, 8, 32, $fp                               ## L:197  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vadd $v22, $v26, $v22.h2                           ## L:543  |    285 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:554  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  ldv $v05, 0, 16, $fp                               ## L:200  |    286 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vxor $v17, $v00, $v30.e0                           ## L:548  |      ^ | vtex = 0x80;
  ldv $v08, 0, 56, $fp                               ## L:203  |  **289 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrsqh $v26.e0, $v22.e0                             ## L:545  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v27.e0, $v23.e0                             ## L:545  |    290 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v05, 8, 16, $fp                               ## L:200  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  LOAD_NORMAL571: lsv $v16, 0, 0, $s4                ## L:571  |    291 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  vrsqh $v26.e0, $v22.e4                             ## L:546  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v27.e4, $v23.e4                             ## L:546  |    292 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 0, 0, $at                                ## L:554  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  vrsqh $v26.e4, $v00.e0                             ## L:546  |    293 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 8, 0, $at                                ## L:555  |      ^ | vtexscale.XY = load(TEXTURING_SCALE).xy;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:556  |    294 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 0, 0, $at                                ## L:556  |    295 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  vmudh $v29, $v17, $v30.e7                          ## L:549  |      ^ | VTEMP = vtex:sint * 1;
  vmadl $v29, $v25, $v27.h0                          ## L:550  |    296 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v08, 8, 56, $fp                               ## L:203  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  llv $v23, 8, 0, $at                                ## L:557  |    297 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  vmadm $v29, $v24, $v27.h0                          ## L:550  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v03, 0, 8, $fp                                ## L:198  |    298 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmadn $v29, $v25, $v26.h0                          ## L:550  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_COLOR575: llv $v19, 0, 0, $s4                 ## L:575  |    299 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmadh $v17, $v24, $v26.h0                          ## L:550  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_POSITION573: ldv $v15, 0, 0, $s4              ## L:573  |    300 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  sltu $at, $s4, $s6                                 ## L:578  |    301 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmudh $v29, $v23, $v30.e7                          ## L:561  |      ^ | VTEMP = vtexoffset:sint * 1;
  bne $at, $zero, VERTEX_LOOP                        ## L:578  |    302 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:562  |   *304 | vtex:sint = vtex:sint +* vtexscale;
  sdv $v20, 8, 0, $t4                                ## L:585  |    305 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sdv $v20, 0, 0, $t3                                ## L:584  |    306 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v18, 4, 8, $t4                                ## L:593  |    307 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  suv $v18, 0, 8, $t3                                ## L:592  |    308 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  slv $v17, 8, 12, $t4                               ## L:596  |    309 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $t5, 6($t3)                                     ## L:587  |    310 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $k0, 7($t4)                                     ## L:590  |    311 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sb $t7, 7($t3)                                     ## L:589  |    312 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sb $t6, 6($t4)                                     ## L:588  |    313 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  j RSPQ_Loop                                        ## L:597  |    314 | }
  slv $v17, 0, 12, $t3                               ## L:595  |   *316 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40

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