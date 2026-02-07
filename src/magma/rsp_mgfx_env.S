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
#define MGFX_ATTRIBUTE_POS_NORM 0
#define MGFX_ATTRIBUTE_COLOR 1
#define MGFX_ATTRIBUTE_TEXCOORD 2
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
#define MGFX_VTX_TEX_SHIFT 8
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
    MgVertexAttributeLoaders LOAD_POS_NORM298, LOAD_POS_NORM300, LOAD_POS_NORM299, LOAD_POS_NORM301, LOAD_POS_NORM569, LOAD_POS_NORM570, LOAD_POS_NORM568, LOAD_POS_NORM567
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR302, LOAD_COLOR303, LOAD_COLOR571, LOAD_COLOR572
    MgBeginVertexAttributePatch PATCH_COLOR496
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:212  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:211  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:213  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:212  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:215  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:229  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:215  |      9 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:237  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:216  |     10 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:215  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  sdv $v04, 0, 0, $at                                ## L:216  |   **13 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v03, 0, 8, $at                                ## L:217  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:217  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:223  |     16 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:227  |     17 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:225  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:230  |    *20 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:232  |     21 | u32 misalignment = buffer_offset & 0x7;
  addu $t0, $t1, $t3                                 ## L:233  |     22 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:234  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:235  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:238  |     25 | vtx_in0_ptr -= dma_size;
  addiu $t0, $t0, -1                                 ## L:240  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  jal DMAExec                                        ## L:240  |     27 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t2, $zero, 12                               ## L:240  |    *29 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:242  |     30 | vtx_size = load(MG_VERTEX_SIZE);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:259  |     31 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:254  |     32 | u16 mvpaddr = MATRICES_MVP;
  LOAD_POS_NORM298: ldv $v16, 0, 0, $s4              ## L:298  |     33 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v10, 0, 8, $at                                ## L:260  |     34 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:245  |     35 | vtx_in_end = vtx_in0_ptr + vertices_size;
  addu $t1, $s4, $t8                                 ## L:283  |     36 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:246  |     37 | vtx_out0_ptr += MG_VERTEX_CACHE;
  addiu $t2, $s7, 40                                 ## L:284  |     38 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  LOAD_POS_NORM300: ldv $v15, 0, 0, $s4              ## L:300  |     39 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v10, 8, 8, $at                                ## L:260  |     40 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  LOAD_POS_NORM299: ldv $v16, 8, 0, $t1              ## L:299  |     41 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v04, 0, 40, $t0                               ## L:195  |     42 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:192  |     43 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:193  |     44 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:194  |     45 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:259  |     46 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:261  |     47 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:195  |     48 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:259  |     49 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:261  |     50 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:263  |     51 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:193  |     52 | m0:ufract = load(address, 0x20).xyzwxyzw;
  LOAD_POS_NORM301: ldv $v15, 8, 0, $t1              ## L:301  |     53 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  sll $t9, $t8, 1                                    ## L:243  |     54 | vtx_size2 = vtx_size * 2;
  ldv $v12, 0, 0, $at                                ## L:263  |     55 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  LOAD_COLOR302: llv $v19, 0, 0, $s4                 ## L:302  |     56 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v13, 0, 8, $at                                ## L:264  |     57 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:194  |     58 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:263  |     59 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:198  |     60 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:197  |     61 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:199  |     62 | m3:ufract = load(address, 0x38).xyzwxyzw;
  LOAD_COLOR303: llv $v19, 4, 0, $t1                 ## L:303  |     63 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v01, 8, 0, $t0                                ## L:192  |     64 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:286  |     65 | u16 prev_out0 = MG_SCRATCH_MEM;
  ldv $v05, 0, 16, $t0                               ## L:196  |     66 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:196  |     67 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:287  |     68 | u16 prev_out1 = MG_SCRATCH_MEM;
  ldv $v08, 8, 56, $t0                               ## L:199  |     69 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:197  |     70 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:264  |     71 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:280  |     72 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v14, 0, 0, $at                                ## L:280  |     73 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:198  |     74 | m3:sint   = load(address, 0x18).xyzwxyzw;
  lbu $t0, %lo(FOG_MASK + 0)                         ## L:281  |     75 | u8 fog_mask = load(FOG_MASK);
  ldv $v14, 8, 0, $at                                ## L:280  |     76 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h3                           ## L:337  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  lbu $k1, %lo(LIGHTING_COUNT + 0)                   ## L:355  |     77 | u8 light_count = load(LIGHTING_COUNT);
  vmov $v15.e3, $v30.e2                              ## L:339  |      ^ | vpos.w = 0x20;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:325  |     78 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmov $v15.e7, $v30.e2                              ## L:340  |      ^ | vpos.W = 0x20;
  sdv $v20, 8, 0, $t4                                ## L:312  |    *80 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sdv $v20, 0, 0, $t3                                ## L:311  |     81 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t6, 6($t4)                                     ## L:315  |     82 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vmudn $v16, $v16, $v13.v                           ## L:342  |      ^ | vnorm *= vnormfactor;
  sdv $v19, 0, 0, $at                                ## L:325  |     83 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v29, $v02, $v15.h0                          ## L:345  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  suv $v18, 0, 8, $t3                                ## L:319  |     84 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmadh $v29, $v01, $v15.h0                          ## L:345  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  luv $v19, 0, 0, $at                                ## L:326  |     85 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadn $v29, $v06, $v15.h2                          ## L:346  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  suv $v18, 4, 8, $t4                                ## L:320  |     86 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadh $v29, $v05, $v15.h2                          ## L:346  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v04, $v15.h1                          ## L:347  |     87 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:354  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  sb $k0, 7($t4)                                     ## L:317  |     88 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmadh $v29, $v03, $v15.h1                          ## L:347  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v18, 0, 0, $at                                ## L:354  |     89 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v21, $v08, $v15.h3                          ## L:348  |      ^ | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  vmadh $v20, $v07, $v15.h3                          ## L:348  |     90 | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  ldv $v18, 8, 0, $at                                ## L:354  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmulf $v29, $v09, $v16.h0                          ## L:350  |     91 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:373  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmacf $v29, $v10, $v16.h1                          ## L:351  |     92 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  ldv $v22, 0, 0, $at                                ## L:373  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:373  |     93 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmacf $v16, $v11, $v16.h2                          ## L:352  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudn $v24, $v21, $v22.v                           ## L:374  |  ***97 | vec32 vguard = vpos_clip * vclip_factors;
  slv $v17, 0, 12, $t3                               ## L:322  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmadh $v23, $v20, $v22.v                           ## L:374  |     98 | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v20, 8, 24, $t2                               ## L:384  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sb $t5, 6($t3)                                     ## L:314  |     99 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sdv $v21, 8, 32, $t2                               ## L:384  |    100 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v20, 0, 24, $s7                               ## L:383  |    101 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v21, 0, 32, $s7                               ## L:383  |    102 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vch $v29, $v23, $v23.h3                            ## L:375  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v24, $v24.h3                            ## L:375  |    103 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:397  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  cfc2 $sp, $vcc                                     ## L:375  |    104 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:379  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sb $t7, 7($t3)                                     ## L:316  |    105 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  andi $s0, $sp, 1799                                ## L:158  |   *107 | t0 = codes & 0x707;
  srl $s1, $s0, 5                                    ## L:159  |    108 | t1 = t0 >> 5;
  or $t5, $s0, $s1                                   ## L:166  |    109 | result = t0 | t1;
  vcl $v29, $v21, $v21.h3                            ## L:379  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  srl $fp, $sp, 4                                    ## L:178  |    110 | u16 codes2 = codes >> 4;
  vaddc $v23, $v21, $v14.h1                          ## L:388  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v22, $v20, $v14.h0                           ## L:389  |    111 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s0, $fp, 1799                                ## L:158  |      ^ | t0 = codes & 0x707;
  cfc2 $fp, $vcc                                     ## L:379  |    112 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  srl $s1, $s0, 5                                    ## L:159  |    113 | t1 = t0 >> 5;
  or $t6, $s0, $s1                                   ## L:166  |    114 | result = t0 | t1;
  andi $s1, $fp, 1799                                ## L:158  |    115 | t0 = codes & 0x707;
  vmudn $v29, $v23, $v14.h2                          ## L:391  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  srl $s2, $s1, 5                                    ## L:159  |    116 | t1 = t0 >> 5;
  vmadh $v22, $v22, $v14.h2                          ## L:392  |      ^ | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:397  |    117 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ori $sp, $zero, %lo(MATRICES_MV)                   ## L:430  |    118 | u16 addr = MATRICES_MV;
  ldv $v23, 8, 0, $at                                ## L:397  |    119 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v05, 0, 16, $sp                               ## L:196  |    120 | m2:sint   = load(address, 0x10).xyzwxyzw;
  nor $t7, $s1, $s2                                  ## L:173  |    121 | result = t0 ~| t1;
  ldv $v03, 0, 8, $sp                                ## L:194  |    122 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vge $v22, $v00, $v22.h2                            ## L:393  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v21, $v21, $v23.h3                          ## L:401  |    123 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v24, 0, 8, $at                                ## L:398  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vmadm $v20, $v20, $v23.h3                          ## L:401  |    124 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v01, 0, 0, $sp                                ## L:192  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadn $v21, $v00, $v00                             ## L:401  |    125 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v03, 8, 8, $sp                                ## L:194  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  srl $s0, $fp, 4                                    ## L:185  |    126 | u16 codes2 = codes >> 4;
  ssv $v20, 14, 16, $t2                              ## L:405  |   *128 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v25.e3, $v20.e3                             ## L:408  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ssv $v21, 14, 18, $t2                              ## L:405  |    129 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v20, 6, 16, $s7                               ## L:404  |    130 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcpl $v26.e3, $v21.e3                             ## L:408  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v04, 0, 40, $sp                               ## L:195  |    131 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrcph $v25.e3, $v20.e7                             ## L:409  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v05, 8, 16, $sp                               ## L:196  |    132 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v02, 0, 32, $sp                               ## L:193  |    133 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrcpl $v26.e7, $v21.e7                             ## L:409  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v25.e7, $v00.e7                             ## L:409  |    134 | vinvw.W = invert_half(vpos_clip).W;
  andi $s1, $s0, 1799                                ## L:158  |      ^ | t0 = codes & 0x707;
  ldv $v01, 8, 0, $sp                                ## L:192  |    135 | m0:sint   = load(address, 0x00).xyzwxyzw;
  srl $s2, $s1, 5                                    ## L:159  |    136 | t1 = t0 >> 5;
  ldv $v24, 8, 8, $at                                ## L:398  |    137 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vmudl $v29, $v21, $v26.h3                          ## L:414  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v20, $v26.h3                          ## L:414  |    138 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v21, 6, 18, $s7                               ## L:404  |      ^ | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v06, 0, 48, $sp                               ## L:197  |    139 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadn $v21, $v21, $v25.h3                          ## L:414  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadh $v20, $v20, $v25.h3                          ## L:414  |    140 | vpos_clip *= vinvw.wwwwWWWW;
  slv $v17, 8, 12, $t4                               ## L:323  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  ldv $v07, 0, 24, $sp                               ## L:198  |    141 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ssv $v25, 14, 20, $t2                              ## L:412  |    142 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  ldv $v06, 8, 48, $sp                               ## L:197  |    143 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v02, 8, 32, $sp                               ## L:193  |    144 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v08, 0, 56, $sp                               ## L:199  |    145 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:418  |      ^ | VTEMP = vviewoff:sint * 1;
  ldv $v04, 8, 40, $sp                               ## L:195  |    146 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadn $v21, $v21, $v23.v                           ## L:419  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  ldv $v07, 8, 24, $sp                               ## L:198  |    147 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadh $v20, $v20, $v23.v                           ## L:419  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  ldv $v08, 8, 56, $sp                               ## L:199  |    148 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudn $v29, $v02, $v15.h0                          ## L:433  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)               ## L:439  |    149 | u16 light_ptr = LIGHTING_LIGHTS;
  vmadh $v29, $v01, $v15.h0                          ## L:433  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v15.h1                          ## L:434  |    150 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  nor $k0, $s1, $s2                                  ## L:173  |      ^ | result = t0 ~| t1;
  ssv $v26, 14, 22, $t2                              ## L:412  |    151 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v29, $v03, $v15.h1                          ## L:434  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v29, $v06, $v15.h2                          ## L:435  |    152 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v25, 6, 20, $s7                               ## L:411  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v26, 6, 22, $s7                               ## L:411  |    153 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadh $v29, $v05, $v15.h2                          ## L:435  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v15.h3                          ## L:436  |    154 | vpos = vmvp3 +* vpos.wwwwWWWW;
  beq $k1, $zero, LIGHT_END                          ## L:440  |      ^ | if (light_count == 0) goto LIGHT_END;
  vmadh $v15, $v07, $v15.h3                          ## L:436  |   *156 | vpos = vmvp3 +* vpos.wwwwWWWW;
  LIGHT_LOOP:
  ldv $v24, 0, 0, $sp                                ## L:444  |    157 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  lh $fp, 6($sp)                                     ## L:446  |    158 | s16 light_w = load(light_ptr, 6);
  ldv $v24, 8, 0, $sp                                ## L:444  |    159 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v23, 0, 8, $sp                                ## L:443  |    160 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v23, 8, 8, $sp                                ## L:443  |    161 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  beq $fp, $zero, LABEL_mgfx_0002                    ## L:447  |    162 | if (light_w != 0)
  nop                                                ## L:536  |   *164 | 
  vsubc $v24, $v24, $v15.v                           ## L:455  |    165 | vlpos -= vpos;
  vmudh $v02, $v24, $v24.v                           ## L:456  | ***169 | vec32 vsqdist = vlpos * vlpos;
  vsar $v02, COP2_ACC_MD                             ## L:456  |    170 | vec32 vsqdist = vlpos * vlpos;
  vsar $v01, COP2_ACC_HI                             ## L:456  |    171 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v04, $v02, $v02.h1                          ## L:457  |  **174 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v03, $v01, $v01.h1                           ## L:457  |    175 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v02, $v04, $v02.h2                          ## L:458  |  **178 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v01, $v03, $v01.h2                           ## L:458  |    179 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v03.e0, $v01.e0                             ## L:460  | ***183 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v04.e0, $v02.e0                             ## L:460  |    184 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v03.e0, $v01.e4                             ## L:461  |    185 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v04.e4, $v02.e4                             ## L:461  |    186 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v03.e4, $v00.e0                             ## L:461  |    187 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v24, $v04.h0                          ## L:463  |  **190 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v24, $v24, $v03.h0                          ## L:463  |    191 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v04, $v23.h3                          ## L:464  |    192 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmadh $v05, $v03, $v23.h3                          ## L:464  |    193 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmulf $v05, $v05, $v05.v                           ## L:465  | ***197 | vatten:sfract *= vatten:sfract;
  vmulf $v23, $v23, $v05.h0                          ## L:466  | ***201 | vlcol:sfract *= vatten:sfract.xxxxXXXX;
  LABEL_mgfx_0002:
  vmulf $v24, $v24, $v16.v                           ## L:480  |    202 | vlpos *= vnorm:sfract;
  vmulf $v29, $v23, $v24.h0                          ## L:481  | ***206 | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v23, $v24.h1                          ## L:482  |    207 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $k1, $k1, 65535                              ## L:489  |      ^ | light_count -= 1;
  vmacf $v23, $v23, $v24.h2                          ## L:483  |    208 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $sp, $sp, 16                                 ## L:488  |      ^ | light_ptr += 16;
  vge $v23, $v23, $v00                               ## L:484  | ***212 | vlcol = max(vlcol, VZERO);
  bne $k1, $zero, LIGHT_LOOP                         ## L:490  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v23.v                            ## L:486  | ****217 | vrgba:sint += vlcol;
  LIGHT_END:
  vmov $v18.e3, $v12.e3                              ## L:494  |    218 | vrgba.w = vnormmask.w;
  ori $fp, $zero, %lo(MATRICES_MVP)                  ## L:504  |      ^ | u16 addr = MATRICES_MVP;
  vmov $v18.e7, $v12.e3                              ## L:495  |    219 | vrgba.W = vnormmask.w;
  ldv $v03, 0, 8, $fp                                ## L:194  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  PATCH_COLOR496: vmulf $v18, $v18, $v19.v           ## L:496  | ***223 | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  ctc2 $t0, $vcc                                     ## L:498  |      ^ | set_vcc(fog_mask);
  ldv $v02, 0, 32, $fp                               ## L:193  | ***227 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmrg $v18, $v18, $v22.h2                           ## L:499  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  ldv $v01, 0, 0, $fp                                ## L:192  |    228 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmudh $v23, $v15, $v15.v                           ## L:508  |      ^ | vec32 vsqdist = vpos * vpos;
  ldv $v03, 8, 8, $fp                                ## L:194  |    229 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vsar $v23, COP2_ACC_MD                             ## L:508  |      ^ | vec32 vsqdist = vpos * vpos;
  vsar $v22, COP2_ACC_HI                             ## L:508  |    230 | vec32 vsqdist = vpos * vpos;
  ldv $v08, 0, 56, $fp                               ## L:199  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vaddc $v25, $v23, $v23.h1                          ## L:509  |  **233 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v24, $v22, $v22.h1                           ## L:509  |    234 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v23, $v25, $v23.h2                          ## L:510  |  **237 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v06, 8, 48, $fp                               ## L:197  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v04, 0, 40, $fp                               ## L:195  |    238 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vadd $v22, $v24, $v22.h2                           ## L:510  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v07, 8, 24, $fp                               ## L:198  | ***242 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsqh $v24.e0, $v22.e0                             ## L:512  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:550  |    243 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  vrsql $v25.e0, $v23.e0                             ## L:512  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v24.e0, $v22.e4                             ## L:513  |    244 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v08, 8, 56, $fp                               ## L:199  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:197  |    245 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrsql $v25.e4, $v23.e4                             ## L:513  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v04, 8, 40, $fp                               ## L:195  |    246 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrsqh $v24.e4, $v00.e0                             ## L:513  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v15, $v25.h0                          ## L:515  |  **249 | vpos *= vtmp.xxxxXXXX;
  addu $t1, $t1, $t9                                 ## L:563  |      ^ | vtx_in1_ptr += vtx_size2;
  vmadh $v15, $v15, $v24.h0                          ## L:515  |    250 | vpos *= vtmp.xxxxXXXX;
  vmulf $v24, $v15, $v16.v                           ## L:518  | ***254 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:519  | ***258 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:520  | ***262 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  vor $v25, $v00, $v00                               ## L:525  |    263 | vec16 vzunit = VZERO;
  vmov $v25.e2, $v12.e3                              ## L:526  |    264 | vzunit.z = vnormmask.w;
  vlt $v24, $v24, $v00                               ## L:522  |   *266 | vdot = min(vdot, VZERO);
  vmov $v25.e6, $v12.e3                              ## L:527  |    267 | vzunit.Z = vnormmask.w;
  ldv $v05, 0, 16, $fp                               ## L:196  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 8, 16, $fp                               ## L:196  |  **270 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vsubc $v24, $v00, $v24.v                           ## L:523  |      ^ | vdot = VZERO - vdot;
  vmulf $v29, $v16, $v24.h0                          ## L:529  | ***274 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  addu $s4, $s4, $t9                                 ## L:562  |      ^ | vtx_in0_ptr += vtx_size2;
  vmacf $v29, $v16, $v24.h0                          ## L:530  |    275 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  or $t4, $zero, $t2                                 ## L:561  |      ^ | prev_out1 = vtx_out1_ptr;
  vmadh $v29, $v15, $v30.e7                          ## L:531  |    276 | VTEMP = vpos:sint +* 1;
  LOAD_POS_NORM569: ldv $v15, 0, 0, $s4              ## L:569  |      ^ | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM570: ldv $v15, 8, 0, $t1              ## L:570  |    277 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v29, $v25, $v30.e7                          ## L:532  |      ^ | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:535  |    278 | vec32 vrefl = get_acc();
  LOAD_COLOR571: llv $v19, 0, 0, $s4                 ## L:571  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vsar $v24, COP2_ACC_HI                             ## L:535  |    279 | vec32 vrefl = get_acc();
  vmudl $v29, $v25, $v25.v                           ## L:537  |  **282 | vsqdist = vrefl * vrefl;
  vmadm $v29, $v24, $v25.v                           ## L:537  |    283 | vsqdist = vrefl * vrefl;
  vmadn $v23, $v25, $v24.v                           ## L:537  |    284 | vsqdist = vrefl * vrefl;
  addiu $t2, $t2, 80                                 ## L:565  |      ^ | vtx_out1_ptr += 80;
  ldv $v02, 8, 32, $fp                               ## L:193  |    285 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v22, $v24, $v24.v                           ## L:537  |      ^ | vsqdist = vrefl * vrefl;
  vaddc $v27, $v23, $v23.h1                          ## L:538  |  **288 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v26, $v22, $v22.h1                           ## L:538  |    289 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v07, 0, 24, $fp                               ## L:198  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vaddc $v23, $v27, $v23.h2                          ## L:539  |  **292 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v26, $v22.h2                           ## L:539  |    293 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  or $t3, $zero, $s7                                 ## L:560  |      ^ | prev_out0 = vtx_out0_ptr;
  vxor $v17, $v00, $v30.e0                           ## L:544  |    294 | vtex = 0x80;
  ldv $v01, 8, 0, $fp                                ## L:192  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrsqh $v26.e0, $v22.e0                             ## L:541  |  **297 | vtmp.x = invert_half_sqrt(vsqdist).x;
  LOAD_POS_NORM568: ldv $v16, 8, 0, $t1              ## L:568  |      ^ | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  vrsql $v27.e0, $v23.e0                             ## L:541  |    298 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v26.e0, $v22.e4                             ## L:542  |    299 | vtmp.X = invert_half_sqrt(vsqdist).X;
  addiu $s7, $s7, 80                                 ## L:564  |      ^ | vtx_out0_ptr += 80;
  vrsql $v27.e4, $v23.e4                             ## L:542  |    300 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 0, 0, $at                                ## L:550  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v22, 8, 0, $at                                ## L:551  |    301 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  vrsqh $v26.e4, $v00.e0                             ## L:542  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:552  |    302 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 0, 0, $at                                ## L:552  |    303 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  vmudh $v29, $v17, $v30.e7                          ## L:545  |      ^ | VTEMP = vtex:sint * 1;
  vmadl $v29, $v25, $v27.h0                          ## L:546  |    304 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  llv $v23, 8, 0, $at                                ## L:553  |      ^ | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  vmadm $v29, $v24, $v27.h0                          ## L:546  |    305 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_COLOR572: llv $v19, 4, 0, $t1                 ## L:572  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmadn $v29, $v25, $v26.h0                          ## L:546  |    306 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  sltu $at, $s4, $s6                                 ## L:574  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  LOAD_POS_NORM567: ldv $v16, 0, 0, $s4              ## L:567  |    307 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  vmadh $v17, $v24, $v26.h0                          ## L:546  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  vmudh $v29, $v23, $v30.e7                          ## L:557  |    308 | VTEMP = vtexoffset:sint * 1;
  bne $at, $zero, VERTEX_LOOP                        ## L:574  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:558  | ***312 | vtex:sint = vtex:sint +* vtexscale;
  sdv $v20, 0, 0, $t3                                ## L:580  |    313 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sdv $v20, 8, 0, $t4                                ## L:581  |    314 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v18, 0, 8, $t3                                ## L:588  |    315 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sb $t6, 6($t4)                                     ## L:584  |    316 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $k0, 7($t4)                                     ## L:586  |    317 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sb $t7, 7($t3)                                     ## L:585  |    318 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  suv $v18, 4, 8, $t4                                ## L:589  |    319 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sb $t5, 6($t3)                                     ## L:583  |    320 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  slv $v17, 0, 12, $t3                               ## L:591  |    321 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  j RSPQ_Loop                                        ## L:593  |    322 | }
  slv $v17, 8, 12, $t4                               ## L:592  |   *324 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80

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