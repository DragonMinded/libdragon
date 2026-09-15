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
#define SCRATCH_TEXSCALE 0x80
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
    MgVertexAttributeLoaders LOAD_POSITION389, LOAD_POSITION390, LOAD_POSITION655, LOAD_POSITION654
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL387, LOAD_NORMAL388, LOAD_NORMAL652, LOAD_NORMAL653
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR391, LOAD_COLOR392, LOAD_COLOR657, LOAD_COLOR656
    MgBeginVertexAttributePatch PATCH_COLOR581
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:293  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:292  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v01, 0, 0, $at                                ## L:293  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:294  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:296  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:318  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:297  |      9 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v04, COP2_ACC_MD                             ## L:296  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  vsar $v03, COP2_ACC_HI                             ## L:296  |     10 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:310  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  sdv $v04, 0, 0, $at                                ## L:297  |   **13 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v03, 0, 8, $at                                ## L:298  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:298  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:308  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:304  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:306  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:311  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:313  |     20 | u32 misalignment = buffer_offset & 0x7;
  addu $t0, $t1, $t3                                 ## L:314  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:315  |     22 | dma_size += 0x7;
  addiu $t2, $zero, 12                               ## L:321  |     23 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  andi $t0, $t0, 4088                                ## L:316  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:319  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:321  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:321  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:340  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:341  |     30 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:340  |     31 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:342  |     32 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:341  |     33 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:342  |     34 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:340  |     35 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:344  |     36 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:344  |     37 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:344  |     38 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:335  |     39 | u16 mvpaddr = MATRICES_MVP;
  ldv $v03, 0, 8, $t0                                ## L:275  |     40 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:345  |     41 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:345  |     42 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:348  |     43 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  ldv $v07, 0, 24, $t0                               ## L:279  |     44 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:280  |     45 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:376  |     46 | u16 prev_out1 = MG_SCRATCH_MEM;
  llv $v14, 0, 0, $at                                ## L:348  |     47 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v14, 8, 0, $at                                ## L:349  |     48 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  ldv $v04, 0, 40, $t0                               ## L:276  |     49 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:276  |     50 | m1:ufract = load(address, 0x28).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:323  |     51 | vtx_size = load(MG_VERTEX_SIZE);
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:327  |     52 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v05, 0, 16, $t0                               ## L:277  |     53 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:354  |     54 | store(vtexscale2, MG_SCRATCH_MEM, 0x80);
  vmudm $v15, $v14, $v31.e2                          ## L:353  |      ^ | vec32 vtexscale2 = vtexscale >> 3;
  vmadn $v16, $v00, $v00                             ## L:353  |     55 | vec32 vtexscale2 = vtexscale >> 3;
  ldv $v07, 8, 24, $t0                               ## L:279  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:278  |     56 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:274  |     57 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:275  |     58 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:273  |     59 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:274  |     60 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:278  |     61 | m2:ufract = load(address, 0x30).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:326  |     62 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ldv $v01, 8, 0, $t0                                ## L:273  |     63 | m0:sint   = load(address, 0x00).xyzwxyzw;
  sqv $v16, 0, 144, $at                              ## L:354  |     64 | store(vtexscale2, MG_SCRATCH_MEM, 0x80);
  LOAD_NORMAL387: lsv $v16, 0, 0, $s4                ## L:387  |     65 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  sqv $v15, 0, 128, $at                              ## L:354  |     66 | store(vtexscale2, MG_SCRATCH_MEM, 0x80);
  addu $t1, $s4, $t8                                 ## L:372  |     67 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:369  |     68 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:277  |     69 | m2:sint   = load(address, 0x10).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:324  |     70 | vtx_size2 = vtx_size * 2;
  LOAD_POSITION389: ldv $v15, 0, 0, $s4              ## L:389  |     71 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_NORMAL388: lsv $v16, 8, 0, $t1                ## L:388  |     72 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  ldv $v14, 0, 0, $at                                ## L:369  |     73 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:280  |     74 | m3:ufract = load(address, 0x38).xyzwxyzw;
  LOAD_COLOR391: llv $v19, 0, 0, $s4                 ## L:391  |     75 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v14, 8, 0, $at                                ## L:369  |     76 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  lbu $t0, %lo(FOG_MASK + 0)                         ## L:370  |     77 | u8 fog_mask = load(FOG_MASK);
  LOAD_COLOR392: llv $v19, 4, 0, $t1                 ## L:392  |     78 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:375  |     79 | u16 prev_out0 = MG_SCRATCH_MEM;
  addiu $t2, $s7, 40                                 ## L:373  |     80 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  LOAD_POSITION390: ldv $v15, 8, 0, $t1              ## L:390  |     81 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h0                           ## L:426  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:414  |     82 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v20, 8, 0, $t4                                ## L:401  |    *84 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v18, 4, 8, $t4                                ## L:409  |     85 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmudn $v16, $v16, $v13.v                           ## L:427  |      ^ | vnorm *= vnormfactor;
  vmudn $v29, $v02, $v15.h0                          ## L:430  |     86 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v20, 0, 0, $t3                                ## L:400  |      ^ | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sdv $v19, 0, 0, $at                                ## L:414  |     87 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadh $v29, $v01, $v15.h0                          ## L:430  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sb $t7, 7($t3)                                     ## L:405  |     88 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vmadn $v29, $v06, $v15.h2                          ## L:431  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:431  |     89 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  suv $v18, 0, 8, $t3                                ## L:408  |      ^ | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmadn $v29, $v04, $v15.h1                          ## L:432  |     90 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  luv $v19, 0, 0, $at                                ## L:415  |      ^ | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:439  |     91 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:432  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v18, 0, 0, $at                                ## L:439  |     92 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v21, $v08, $v30.e7                          ## L:433  |      ^ | vpos_clip = vmvp3 +* 1;
  ldv $v18, 8, 0, $at                                ## L:439  |     93 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:458  |     94 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v20, $v07, $v30.e7                          ## L:433  |      ^ | vpos_clip = vmvp3 +* 1;
  ldv $v22, 0, 0, $at                                ## L:458  |     95 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:458  |     96 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmulf $v29, $v09, $v16.h0                          ## L:435  |      ^ | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:482  |     97 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmacf $v29, $v10, $v16.h1                          ## L:436  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  vmacf $v16, $v11, $v16.h2                          ## L:437  |    *99 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  slv $v17, 8, 12, $t4                               ## L:412  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $k0, 7($t4)                                     ## L:406  |    100 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmudn $v24, $v21, $v22.v                           ## L:459  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v23, $v20, $v22.v                           ## L:459  |    101 | vec32 vguard = vpos_clip * vclip_factors;
  sb $t6, 6($t4)                                     ## L:404  |      ^ | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sdv $v20, 8, 24, $t2                               ## L:469  |    102 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  slv $v17, 0, 12, $t3                               ## L:411  |    103 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sdv $v21, 8, 32, $t2                               ## L:469  |    104 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v20, 0, 24, $s7                               ## L:468  |    105 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vch $v29, $v23, $v23.h3                            ## L:460  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v24, $v24.h3                            ## L:460  |    106 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $t5, 6($t3)                                     ## L:403  |      ^ | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  cfc2 $sp, $vcc                                     ## L:460  |    107 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v21, 0, 32, $s7                               ## L:468  |    108 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  ldv $v24, 0, 8, $at                                ## L:483  |    109 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vch $v29, $v20, $v20.h3                            ## L:464  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  lbu $k1, %lo(LIGHTING_COUNT + 0)                   ## L:440  |    110 | u8 light_count = load(LIGHTING_COUNT);
  vcl $v29, $v21, $v21.h3                            ## L:464  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  andi $s0, $sp, 1799                                ## L:239  |    111 | t0 = codes & 0x707;
  vaddc $v23, $v21, $v14.h1                          ## L:473  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v22, $v20, $v14.h0                           ## L:474  |    112 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  srl $fp, $sp, 4                                    ## L:259  |      ^ | u16 codes2 = codes >> 4;
  srl $s1, $s0, 5                                    ## L:240  |    113 | t1 = t0 >> 5;
  ori $sp, $zero, %lo(MATRICES_MV)                   ## L:515  |    114 | u16 addr = MATRICES_MV;
  ldv $v03, 0, 8, $sp                                ## L:275  |    115 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudn $v29, $v23, $v14.h2                          ## L:476  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:482  |    116 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmadh $v22, $v22, $v14.h2                          ## L:477  |      ^ | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  or $t5, $s0, $s1                                   ## L:247  |    117 | result = t0 | t1;
  ldv $v23, 8, 0, $at                                ## L:482  |    118 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  andi $s0, $fp, 1799                                ## L:239  |    119 | t0 = codes & 0x707;
  ldv $v08, 0, 56, $sp                               ## L:280  |    120 | m3:ufract = load(address, 0x38).xyzwxyzw;
  cfc2 $fp, $vcc                                     ## L:464  |    121 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v22, $v00, $v22.h2                            ## L:478  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v21, $v21, $v23.h3                          ## L:486  |    122 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v07, 0, 24, $sp                               ## L:279  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadm $v20, $v20, $v23.h3                          ## L:486  |    123 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v04, 0, 40, $sp                               ## L:276  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v04, 8, 40, $sp                               ## L:276  |    124 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadn $v21, $v00, $v00                             ## L:486  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v02, 0, 32, $sp                               ## L:274  |    125 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v08, 8, 56, $sp                               ## L:280  |    126 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v24, 8, 8, $at                                ## L:483  |    127 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vrcph $v25.e3, $v20.e3                             ## L:493  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v05, 0, 16, $sp                               ## L:277  |    128 | m2:sint   = load(address, 0x10).xyzwxyzw;
  srl $s1, $s0, 5                                    ## L:240  |    129 | t1 = t0 >> 5;
  ldv $v02, 8, 32, $sp                               ## L:274  |    130 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrcpl $v26.e3, $v21.e3                             ## L:493  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ssv $v20, 14, 16, $t2                              ## L:490  |    131 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v25.e3, $v20.e7                             ## L:494  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v01, 0, 0, $sp                                ## L:273  |    132 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrcpl $v26.e7, $v21.e7                             ## L:494  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v25.e7, $v00.e7                             ## L:494  |    133 | vinvw.W = invert_half(vpos_clip).W;
  or $t6, $s0, $s1                                   ## L:247  |      ^ | result = t0 | t1;
  andi $s1, $fp, 1799                                ## L:239  |    134 | t0 = codes & 0x707;
  ssv $v20, 6, 16, $s7                               ## L:489  |    135 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ssv $v21, 14, 18, $t2                              ## L:490  |    136 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v21, 6, 18, $s7                               ## L:489  |    137 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vmudl $v29, $v21, $v26.h3                          ## L:499  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v20, $v26.h3                          ## L:499  |    138 | vpos_clip *= vinvw.wwwwWWWW;
  srl $s2, $s1, 5                                    ## L:240  |      ^ | t1 = t0 >> 5;
  vmadn $v21, $v21, $v25.h3                          ## L:499  |    139 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v06, 0, 48, $sp                               ## L:278  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v20, $v20, $v25.h3                          ## L:499  |    140 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v07, 8, 24, $sp                               ## L:279  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v03, 8, 8, $sp                                ## L:275  |    141 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:503  |      ^ | VTEMP = vviewoff:sint * 1;
  srl $s0, $fp, 4                                    ## L:266  |    142 | u16 codes2 = codes >> 4;
  ldv $v01, 8, 0, $sp                                ## L:273  |    143 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadn $v21, $v21, $v23.v                           ## L:504  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  vmadh $v20, $v20, $v23.v                           ## L:504  |    144 | vpos_clip = vpos_clip +* vviewscale;
  nor $t7, $s1, $s2                                  ## L:254  |      ^ | result = t0 ~| t1;
  vmudn $v29, $v02, $v15.h0                          ## L:518  |    145 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v06, 8, 48, $sp                               ## L:278  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v05, 8, 16, $sp                               ## L:277  |    146 | m2:sint   = load(address, 0x10).xyzwxyzw;
  andi $s1, $s0, 1799                                ## L:239  |    147 | t0 = codes & 0x707;
  vmadh $v29, $v01, $v15.h0                          ## L:518  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  srl $s2, $s1, 5                                    ## L:240  |    148 | t1 = t0 >> 5;
  ssv $v25, 14, 20, $t2                              ## L:497  |    149 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v29, $v04, $v15.h1                          ## L:519  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  nor $k0, $s1, $s2                                  ## L:254  |    150 | result = t0 ~| t1;
  vmadh $v29, $v03, $v15.h1                          ## L:519  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)               ## L:524  |    151 | u16 light_ptr = LIGHTING_LIGHTS;
  vmadn $v29, $v06, $v15.h2                          ## L:520  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:520  |    152 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v26, 14, 22, $t2                              ## L:497  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  ssv $v25, 6, 20, $s7                               ## L:496  |    153 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadn $v29, $v08, $v30.e7                          ## L:521  |      ^ | vpos = vmvp3 +* 1;
  vmadh $v15, $v07, $v30.e7                          ## L:521  |    154 | vpos = vmvp3 +* 1;
  beq $k1, $zero, LIGHT_END                          ## L:525  |      ^ | if (light_count == 0) goto LIGHT_END;
  ssv $v26, 6, 22, $s7                               ## L:496  |   *156 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  LIGHT_LOOP:
  lh $fp, 6($sp)                                     ## L:531  |    157 | s16 light_w = load(light_ptr, 6);
  ldv $v24, 0, 0, $sp                                ## L:529  |    158 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $sp                                ## L:529  |    159 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v23, 0, 8, $sp                                ## L:528  |    160 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  beq $fp, $zero, LABEL_mgfx_0002                    ## L:532  |    161 | if (light_w != 0)
  ldv $v23, 8, 8, $sp                                ## L:528  |   *163 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  vsubc $v24, $v24, $v15.v                           ## L:540  |    164 | vlpos -= vpos;
  vmudh $v02, $v24, $v24.v                           ## L:541  | ***168 | vec32 vsqdist = vlpos * vlpos;
  vsar $v02, COP2_ACC_MD                             ## L:541  |    169 | vec32 vsqdist = vlpos * vlpos;
  vsar $v01, COP2_ACC_HI                             ## L:541  |    170 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v04, $v02, $v02.h1                          ## L:542  |  **173 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v03, $v01, $v01.h1                           ## L:542  |    174 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v02, $v04, $v02.h2                          ## L:543  |  **177 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v01, $v03, $v01.h2                           ## L:543  |    178 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v03.e0, $v01.e0                             ## L:545  | ***182 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v04.e0, $v02.e0                             ## L:545  |    183 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v03.e0, $v01.e4                             ## L:546  |    184 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v04.e4, $v02.e4                             ## L:546  |    185 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v03.e4, $v00.e0                             ## L:546  |    186 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v24, $v04.h0                          ## L:548  |  **189 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v24, $v24, $v03.h0                          ## L:548  |    190 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v04, $v23.h3                          ## L:549  |    191 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmadh $v05, $v03, $v23.h3                          ## L:549  |    192 | vec16 vatten = vtmp * vlcol.wwwwWWWW;
  vmulf $v05, $v05, $v05.v                           ## L:550  | ***196 | vatten:sfract *= vatten:sfract;
  vmulf $v23, $v23, $v05.h0                          ## L:551  | ***200 | vlcol:sfract *= vatten:sfract.xxxxXXXX;
  LABEL_mgfx_0002:
  vmulf $v24, $v24, $v16.v                           ## L:565  |    201 | vlpos *= vnorm:sfract;
  vmulf $v29, $v23, $v24.h0                          ## L:566  | ***205 | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v23, $v24.h1                          ## L:567  |    206 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $sp, $sp, 16                                 ## L:573  |      ^ | light_ptr += 16;
  vmacf $v23, $v23, $v24.h2                          ## L:568  |    207 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $k1, $k1, 65535                              ## L:574  |      ^ | light_count -= 1;
  vge $v23, $v23, $v00                               ## L:569  | ***211 | vlcol = max(vlcol, VZERO);
  bne $k1, $zero, LIGHT_LOOP                         ## L:575  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v23.v                            ## L:571  | ****216 | vrgba:sint += vlcol;
  LIGHT_END:
  or $t3, $zero, $s7                                 ## L:645  |    217 | prev_out0 = vtx_out0_ptr;
  vmov $v18.e3, $v12.e3                              ## L:579  |      ^ | vrgba.w = vnormmask.w;
  vmov $v18.e7, $v12.e3                              ## L:580  |    218 | vrgba.W = vnormmask.w;
  addu $s4, $s4, $t9                                 ## L:647  |      ^ | vtx_in0_ptr += vtx_size2;
  ori $fp, $zero, %lo(MATRICES_MVP)                  ## L:589  |    219 | u16 addr = MATRICES_MVP;
  ctc2 $t0, $vcc                                     ## L:583  |  **222 | set_vcc(fog_mask);
  PATCH_COLOR581: vmulf $v18, $v18, $v19.v           ## L:581  |      ^ | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  vmrg $v18, $v18, $v22.h2                           ## L:584  | ***226 | vrgba = select(vrgba, vfog.zzzzZZZZ);
  ldv $v08, 8, 56, $fp                               ## L:280  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudh $v23, $v15, $v15.v                           ## L:593  |    227 | vec32 vsqdist = vpos * vpos;
  addu $t1, $t1, $t9                                 ## L:648  |      ^ | vtx_in1_ptr += vtx_size2;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:636  |    228 | vtexscale = load(MG_SCRATCH_MEM, 0x80);
  vsar $v23, COP2_ACC_MD                             ## L:593  |      ^ | vec32 vsqdist = vpos * vpos;
  addiu $s7, $s7, 80                                 ## L:649  |    229 | vtx_out0_ptr += 80;
  vsar $v22, COP2_ACC_HI                             ## L:593  |      ^ | vec32 vsqdist = vpos * vpos;
  vaddc $v25, $v23, $v23.h1                          ## L:594  |  **232 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  LOAD_COLOR657: llv $v19, 4, 0, $t1                 ## L:657  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v05, 8, 16, $fp                               ## L:277  |    233 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vadd $v24, $v22, $v22.h1                           ## L:594  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v03, 0, 8, $fp                                ## L:275  |  **236 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vaddc $v23, $v25, $v23.h2                          ## L:595  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v24, $v22.h2                           ## L:595  |    237 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v02, 8, 32, $fp                               ## L:274  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v07, 0, 24, $fp                               ## L:279  | ***241 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsqh $v24.e0, $v22.e0                             ## L:597  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v25.e0, $v23.e0                             ## L:597  |    242 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v24.e0, $v22.e4                             ## L:598  |    243 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v25.e4, $v23.e4                             ## L:598  |    244 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v07, 8, 24, $fp                               ## L:279  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  or $t4, $zero, $t2                                 ## L:646  |    245 | prev_out1 = vtx_out1_ptr;
  vrsqh $v24.e4, $v00.e0                             ## L:598  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v15, $v25.h0                          ## L:600  |  **248 | vpos *= vtmp.xxxxXXXX;
  vmadh $v15, $v15, $v24.h0                          ## L:600  |    249 | vpos *= vtmp.xxxxXXXX;
  ldv $v02, 0, 32, $fp                               ## L:274  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmulf $v24, $v15, $v16.v                           ## L:603  | ***253 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:604  | ***257 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:605  | ***261 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  ldv $v01, 0, 0, $fp                                ## L:273  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  LOAD_COLOR656: llv $v19, 0, 0, $s4                 ## L:656  | ***265 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vlt $v24, $v24, $v00                               ## L:607  |      ^ | vdot = min(vdot, VZERO);
  vor $v25, $v00, $v00                               ## L:610  |    266 | vec16 vzunit = VZERO;
  vsubc $v24, $v00, $v24.v                           ## L:608  |  **269 | vdot = VZERO - vdot;
  vmov $v25.e6, $v12.e3                              ## L:612  |    270 | vzunit.Z = vnormmask.w;
  vmov $v25.e2, $v12.e3                              ## L:611  |    271 | vzunit.z = vnormmask.w;
  vmulf $v29, $v16, $v24.h0                          ## L:614  |   *273 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v16, $v24.h0                          ## L:615  |    274 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  vmadh $v29, $v15, $v30.e7                          ## L:616  |    275 | VTEMP = vpos:sint +* 1;
  addiu $t2, $t2, 80                                 ## L:650  |      ^ | vtx_out1_ptr += 80;
  LOAD_POSITION655: ldv $v15, 8, 0, $t1              ## L:655  |    276 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v29, $v25, $v30.e7                          ## L:617  |      ^ | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:620  |    277 | vec32 vrefl = get_acc();
  vsar $v24, COP2_ACC_HI                             ## L:620  |    278 | vec32 vrefl = get_acc();
  vmudl $v29, $v25, $v25.v                           ## L:622  |  **281 | vsqdist = vrefl * vrefl;
  ldv $v05, 0, 16, $fp                               ## L:277  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadm $v29, $v24, $v25.v                           ## L:622  |    282 | vsqdist = vrefl * vrefl;
  LOAD_NORMAL652: lsv $v16, 0, 0, $s4                ## L:652  |      ^ | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  vmadn $v23, $v25, $v24.v                           ## L:622  |    283 | vsqdist = vrefl * vrefl;
  vmadh $v22, $v24, $v24.v                           ## L:622  |    284 | vsqdist = vrefl * vrefl;
  ldv $v08, 0, 56, $fp                               ## L:280  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vaddc $v27, $v23, $v23.h1                          ## L:623  |  **287 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v04, 8, 40, $fp                               ## L:276  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_NORMAL653: lsv $v16, 8, 0, $t1                ## L:653  |    288 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  vadd $v26, $v22, $v22.h1                           ## L:623  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vxor $v17, $v00, $v30.e0                           ## L:629  |    289 | vtex = 0x80;
  ldv $v06, 0, 48, $fp                               ## L:278  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v04, 0, 40, $fp                               ## L:276  |   *291 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vaddc $v23, $v27, $v23.h2                          ## L:624  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v26, $v22.h2                           ## L:624  |    292 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v26.e0, $v22.e0                             ## L:626  | ***296 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v27.e0, $v23.e0                             ## L:626  |    297 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v26.e0, $v22.e4                             ## L:627  |    298 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v01, 8, 0, $fp                                ## L:273  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrsql $v27.e4, $v23.e4                             ## L:627  |    299 | vtmp.X = invert_half_sqrt(vsqdist).X;
  lqv $v23, 0, 144, $at                              ## L:636  |      ^ | vtexscale = load(MG_SCRATCH_MEM, 0x80);
  lqv $v22, 0, 128, $at                              ## L:636  |    300 | vtexscale = load(MG_SCRATCH_MEM, 0x80);
  vrsqh $v26.e4, $v00.e0                             ## L:627  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  LOAD_POSITION654: ldv $v15, 0, 0, $s4              ## L:654  |    301 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vmudh $v29, $v17, $v30.e7                          ## L:630  |      ^ | VTEMP = vtex:sint * 1;
  vmadl $v29, $v25, $v27.h0                          ## L:631  |   *303 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:637  |      ^ | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  vmadm $v29, $v24, $v27.h0                          ## L:631  |    304 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v06, 8, 48, $fp                               ## L:278  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadn $v29, $v25, $v26.h0                          ## L:631  |    305 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v03, 8, 8, $fp                                ## L:275  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmadh $v17, $v24, $v26.h0                          ## L:631  |    306 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  llv $v24, 0, 0, $at                                ## L:637  |      ^ | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:638  |    307 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  sltu $at, $s4, $s6                                 ## L:659  | ***311 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmudh $v29, $v24, $v30.e7                          ## L:642  |      ^ | VTEMP = vtexoffset:sint * 1;
  vmadm $v29, $v17, $v23.v                           ## L:643  |    312 | vtex:sint = vtex:sint +* vtexscale;
  bne $at, $zero, VERTEX_LOOP                        ## L:659  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:643  |   *314 | vtex:sint = vtex:sint +* vtexscale;
  sdv $v20, 8, 0, $t4                                ## L:666  |    315 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v18, 4, 8, $t4                                ## L:674  |    316 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sdv $v20, 0, 0, $t3                                ## L:665  |    317 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v18, 0, 8, $t3                                ## L:673  |    318 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sb $t7, 7($t3)                                     ## L:670  |    319 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  slv $v17, 0, 12, $t3                               ## L:676  |    320 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t5, 6($t3)                                     ## L:668  |    321 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t6, 6($t4)                                     ## L:669  |    322 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  slv $v17, 8, 12, $t4                               ## L:677  |    323 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  j RSPQ_Loop                                        ## L:678  |    324 | }
  sb $k0, 7($t4)                                     ## L:671  |   *326 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20

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