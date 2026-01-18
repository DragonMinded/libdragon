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
#define __GL_CONSTANTS 
#define PIPELINE_FEATURE_COUNT 1
#define PIPELINE_FEATURES_MASK ((1<<1)-1)
#define PIPELINE_COUNT (1<<(1+1))
#define PIPELINE_PSEUDO_FEATURE_FLAG (1<<1)
#define MODELVIEW_STACK_SIZE 32
#define PROJECTION_STACK_SIZE 2
#define TEXTURE_STACK_SIZE 2
#define PALETTE_STACK_SIZE 1
#define VERTEX_UNIT_COUNT 1
#define MATRIX_PALETTE_SIZE 32
#define MATRICES_SIZE 160
#define VERTEX_CACHE_SIZE 32
#define CLIPPING_PLANE_COUNT 6
#define CLIPPING_CACHE_SIZE 9
#define CLIPPING_PLANE_SIZE 8
#define MATRIX_SIZE 64
#define TEX_COORD_COUNT 4
#define TEX_GEN_COUNT 4
#define TEX_GEN_PLANE_COUNT 2
#define TEX_GEN_SIZE 34
#define TEX_GEN_STRUCT_SIZE 144
#define TEX_GEN_INTEGER_OFFSET 0
#define TEX_GEN_FRACTION_OFFSET 64
#define TEX_GEN_MODE_OFFSET 128
#define TEX_GEN_MODE_T_OFFSET 130
#define TEX_GEN_CONST_SIZE (4*2)
#define RDP_TEX_SHIFT 5
#define LIGHT_COUNT 8
#define MAX_TEXTURE_SIZE 64
#define MAX_TEXTURE_LEVELS 7
#define TEXTURE_IMAGE_SIZE 6
#define TEXTURE_OBJECT_PROPS_OFFSET (6 * 7)
#define TEXTURE_LEVELS_COUNT_OFFSET ((6 * 7) + 0)
#define TEXTURE_TLUT_MODE_OFFSET ((6 * 7) + 1)
#define TEXTURE_LEVELS_BLOCK_OFFSET ((6 * 7) + 2)
#define TEXTURE_FLAGS_OFFSET ((6 * 7) + 62)
#define TEXTURE_MIN_FILTER_OFFSET ((6 * 7) + 66)
#define TEXTURE_MAG_FILTER_OFFSET ((6 * 7) + 68)
#define TEXTURE_OBJECT_SIZE ((6 * 7) + 70)
#define IMAGE_WIDTH_OFFSET 0
#define IMAGE_HEIGHT_OFFSET 2
#define IMAGE_INTERNAL_FORMAT_OFFSET 4
#define TEXTURE_BILINEAR_MASK 0x001
#define TEXTURE_INTERPOLATE_MASK 0x002
#define TEXTURE_MIPMAP_MASK 0x100
#define MAX_PIXEL_MAP_SIZE 32
#define ATTRIB_TYPE_COUNT 10
#define FLAG_DITHER (1 << 0)
#define FLAG_BLEND (1 << 1)
#define FLAG_ALPHA_TEST (1 << 2)
#define FLAG_FOG (1 << 3)
#define FLAG_SCISSOR_TEST (1 << 4)
#define FLAG_TEXTURE_1D (1 << 5)
#define FLAG_TEXTURE_2D (1 << 6)
#define FLAG_CULL_FACE (1 << 7)
#define FLAG_LIGHTING (1 << 10)
#define FLAG_COLOR_MATERIAL (1 << 11)
#define FLAG_NORMALIZE (1 << 12)
#define FLAG_MATRIX_PALETTE (1 << 14)
#define FLAG_DEPTH_TEST (1 << 16)
#define FLAG_TEXTURE_ACTIVE (1 << 17)
#define FLAG_DEPTH_MASK (1 << 18)
#define FLAG_FINAL_MTX_DIRTY (1 << 19)
#define FLAG_LIGHT0 (1 << 20)
#define FLAG_LIGHT1 (1 << 21)
#define FLAG_LIGHT2 (1 << 22)
#define FLAG_LIGHT3 (1 << 23)
#define FLAG_LIGHT4 (1 << 24)
#define FLAG_LIGHT5 (1 << 25)
#define FLAG_LIGHT6 (1 << 26)
#define FLAG_LIGHT7 (1 << 27)
#define FLAG_TEX_GEN_S (1 << 28)
#define FLAG_TEX_GEN_T (1 << 29)
#define FLAG_TEX_GEN_R (1 << 30)
#define FLAG_TEX_GEN_Q (1 << 31)
#define FLAG2_USE_RDPQ_MATERIAL (1 << 0)
#define FLAG2_USE_RDPQ_TEXTURING (1 << 1)
#define FLAG2_REDUCED_ALIASING (1 << 2)
#define FLAG2_MULTISAMPLE (1 << 3)
#define FLAG2_TEX_FLIP_T (1 << 4)
#define CLIENT_FLAG_BEGIN_END (1 << 0)
#define CLIENT_FLAG_COLOR_ARRAY (1 << 1)
#define TEX_FLAG_COMPLETE (1 << 0)
#define TEX_FLAG_UPLOAD_DIRTY (1 << 1)
#define TEX_FLAG_FORCE_COMPLETE (1 << 2)
#define TEX_FLAG_DETAIL (1 << 3)
#define DITHER_MASK (SOM_RGBDITHER_MASK | SOM_ALPHADITHER_MASK)
#define BLEND_MASK SOM_ZMODE_MASK
#define DEPTH_TEST_MASK SOM_Z_COMPARE
#define DEPTH_MASK_MASK SOM_Z_WRITE
#define POINTS_MASK (SOM_ZSOURCE_MASK | SOM_TEXTURE_PERSP)
#define ALPHA_TEST_MASK SOM_ALPHACOMPARE_MASK
#define LOAD_TILE 7
#define GUARD_BAND_FACTOR 2
#define MULTISAMPLE_FLAG_SHIFT 2
#define ZMODE_BLEND_FLAG_SHIFT 10
#define COMBINER_FLAG_CONSTANT (1 << 0)
#define COMBINER_FLAG_REPLACE (1 << 1)
#define COMBINER_FLAG_TEXTURE (1 << 2)
#define TEXTURE_ACTIVE_SHIFT 17
#define TEX_ACTIVE_COMBINER_SHIFT (17 - 2)
#define TEX_COORD_SHIFT 3
#define HALF_TEXEL 0x0010
#define TEX_BILINEAR_SHIFT 13
#define TEX_BILINEAR_OFFSET_SHIFT 4
#define TEX_DETAIL_SHIFT 15
#define BILINEAR_TEX_OFFSET_SHIFT 9
#define TRICMD_ATTR_SHIFT 8
#define TRICMD_ATTR_MASK 0x300
#define LIGHT0_SHIFT 20
#define TEX_GEN_S_SHIFT 28
#define TEX_GEN_LINEAR_FLAG_SHIFT 8
#define TEX_GEN_SPHERICAL_FLAG_SHIFT 9
#define NEED_EYE_SPACE_SHIFT 13
#define RDPQ_TEXTURING_MASK ((SOM_SAMPLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE | SOMX_NUMLODS_MASK | SOM_TLUT_MASK)>>32)
#define PALETTE_MATRIX_INDEX 3
#define PALETTE_DIRTY_FLAGS_SIZE ((32+7)>>3)
#define GL_LIGHT_POSITION 0
#define GL_LIGHT_POSITIONW 6
#define GL_LIGHT_COLOR 8
#define GL_LIGHT_ATT_INT 14
#define GL_LIGHT_ATT_FRAC 20
#define GL_LIGHT_SIZE 26
#define GLP_ATTRIBUTE_POS_NORM 0
#define GLP_ATTRIBUTE_COLOR 1
#define GLP_ATTRIBUTE_TEXCOORD 2
#define GLP_BINDING_MATRICES 0
#define GLP_BINDING_TEXTURING 1
#define GLP_BINDING_LIGHTING 2
#define GLP_BINDING_FOG 3
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
    LIGHTING_LIGHTS: .ds.b 208
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
    MgVertexAttributeLoaders LOAD_POS_NORM433, LOAD_POS_NORM431, LOAD_POS_NORM432, LOAD_POS_NORM434, LOAD_POS_NORM715, LOAD_POS_NORM714, LOAD_POS_NORM717, LOAD_POS_NORM716
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR435, LOAD_COLOR436, LOAD_COLOR719, LOAD_COLOR718
    MgBeginVertexAttributePatch PATCH_COLOR643
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:344  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:345  |      2 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v01, 0, 0, $at                                ## L:345  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:346  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:370  |   ***8 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vmudh $v04, $v02, $v01.v                           ## L:348  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:349  |      9 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:348  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:362  |     10 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:348  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ssv $v03, 0, 8, $at                                ## L:350  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:350  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:349  |     15 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:360  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:356  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:358  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:365  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:363  |     20 | vtx_buffer += buffer_offset;
  addu $t0, $t1, $t3                                 ## L:366  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:367  |     22 | dma_size += 0x7;
  addiu $t2, $zero, 12                               ## L:373  |     23 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  andi $t0, $t0, 4088                                ## L:368  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:371  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:373  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:373  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:392  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:387  |     30 | u16 mvpaddr = MATRICES_MVP;
  ldv $v05, 0, 16, $t0                               ## L:329  |     31 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:393  |     32 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:330  |     33 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:332  |     34 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $t3, $zero, %lo(MG_CLIP_CACHE)                 ## L:419  |     35 | u16 prev_out0 = MG_CLIP_CACHE;
  ldv $v02, 0, 32, $t0                               ## L:326  |     36 | m0:ufract = load(address, 0x20).xyzwxyzw;
  LOAD_COLOR435: llv $v19, 0, 0, $s4                 ## L:435  |     37 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v06, 8, 48, $t0                               ## L:330  |     38 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:331  |     39 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:394  |     40 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:326  |     41 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:325  |     42 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:392  |     43 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:394  |     44 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:392  |     45 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  LOAD_POS_NORM433: ldv $v15, 0, 0, $s4              ## L:433  |     46 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v10, 8, 8, $at                                ## L:393  |     47 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:375  |     48 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v04, 0, 40, $t0                               ## L:328  |     49 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_POS_NORM431: ldv $v16, 0, 0, $s4              ## L:431  |     50 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v04, 8, 40, $t0                               ## L:328  |     51 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:329  |     52 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:332  |     53 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:378  |     54 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:396  |     55 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:396  |     56 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:397  |     57 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:397  |     58 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:396  |     59 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:413  |     60 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:327  |     61 | m1:sint   = load(address, 0x08).xyzwxyzw;
  addu $t1, $s4, $t8                                 ## L:416  |     62 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  LOAD_POS_NORM432: ldv $v16, 8, 0, $t1              ## L:432  |     63 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v14, 0, 0, $at                                ## L:413  |     64 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:325  |     65 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:327  |     66 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:331  |     67 | m3:sint   = load(address, 0x18).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:376  |     68 | vtx_size2 = vtx_size * 2;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:379  |     69 | vtx_out0_ptr += MG_VERTEX_CACHE;
  LOAD_COLOR436: llv $v19, 4, 0, $t1                 ## L:436  |     70 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  lbu $t0, %lo(FOG_MASK + 0)                         ## L:414  |     71 | u8 fog_mask = load(FOG_MASK);
  LOAD_POS_NORM434: ldv $v15, 8, 0, $t1              ## L:434  |     72 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v14, 8, 0, $at                                ## L:413  |     73 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $t4, $zero, %lo(MG_CLIP_CACHE)                 ## L:420  |     74 | u16 prev_out1 = MG_CLIP_CACHE;
  addiu $t2, $s7, 40                                 ## L:417  |     75 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  VERTEX_LOOP:
  vmov $v15.e7, $v30.e2                              ## L:473  |      ^ | vpos.W = 0x20;
  sdv $v20, 0, 0, $t3                                ## L:444  |     76 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vand $v16, $v12, $v16.h3                           ## L:470  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  suv $v18, 4, 8, $t4                                ## L:453  |     77 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmov $v15.e3, $v30.e2                              ## L:472  |      ^ | vpos.w = 0x20;
  sb $t5, 6($t3)                                     ## L:447  |     78 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  suv $v18, 0, 8, $t3                                ## L:452  |     79 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v20, 8, 0, $t4                                ## L:445  |     80 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmudn $v16, $v16, $v13.v                           ## L:475  |      ^ | vnorm *= vnormfactor;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:458  |     81 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v29, $v02, $v15.h0                          ## L:478  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v15.h0                          ## L:478  |     82 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v19, 0, 0, $at                                ## L:458  |      ^ | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  luv $v19, 0, 0, $at                                ## L:459  |     83 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadn $v29, $v06, $v15.h2                          ## L:479  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:479  |     84 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:487  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v04, $v15.h1                          ## L:480  |     85 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v18, 0, 0, $at                                ## L:487  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:480  |     86 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v18, 8, 0, $at                                ## L:487  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:506  |     87 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v21, $v08, $v15.h3                          ## L:481  |      ^ | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  ldv $v22, 0, 0, $at                                ## L:506  |     88 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v20, $v07, $v15.h3                          ## L:481  |      ^ | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  vmulf $v29, $v09, $v16.h0                          ## L:483  |     89 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ldv $v22, 8, 0, $at                                ## L:506  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  lbu $k1, %lo(LIGHTING_COUNT + 0)                   ## L:488  |     90 | u8 light_count = load(LIGHTING_COUNT);
  vmacf $v29, $v10, $v16.h1                          ## L:484  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  vmacf $v16, $v11, $v16.h2                          ## L:485  |     91 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:530  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmudn $v24, $v21, $v22.v                           ## L:507  |    *93 | vec32 vguard = vpos_clip * vclip_factors;
  slv $v17, 8, 12, $t4                               ## L:456  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $t7, 7($t3)                                     ## L:449  |     94 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vmadh $v23, $v20, $v22.v                           ## L:507  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v20, 0, 24, $s7                               ## L:516  |     95 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v21, 0, 32, $s7                               ## L:516  |     96 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  slv $v17, 0, 12, $t3                               ## L:455  |     97 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t6, 6($t4)                                     ## L:448  |     98 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vch $v29, $v23, $v23.h3                            ## L:508  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $k0, 7($t4)                                     ## L:450  |     99 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vcl $v29, $v24, $v24.h3                            ## L:508  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $sp, $vcc                                     ## L:508  |    100 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:512  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v21, $v21.h3                            ## L:512  |    101 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sdv $v20, 8, 24, $t2                               ## L:517  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  ldv $v24, 0, 8, $at                                ## L:531  |    102 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  sdv $v21, 8, 32, $t2                               ## L:517  |    103 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vaddc $v23, $v21, $v14.h1                          ## L:521  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v22, $v20, $v14.h0                           ## L:522  |    104 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s0, $sp, 1799                                ## L:291  |      ^ | t0 = codes & 0x707;
  srl $s1, $s0, 5                                    ## L:292  |    105 | t1 = t0 >> 5;
  srl $fp, $sp, 4                                    ## L:311  |    106 | u16 codes2 = codes >> 4;
  ori $sp, $zero, %lo(MATRICES_MV)                   ## L:563  |    107 | u16 addr = MATRICES_MV;
  vmudn $v29, $v23, $v14.h2                          ## L:524  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v22, $v22, $v14.h2                          ## L:525  |    108 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:530  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:530  |    109 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  or $t5, $s0, $s1                                   ## L:299  |    110 | result = t0 | t1;
  andi $s0, $fp, 1799                                ## L:291  |    111 | t0 = codes & 0x707;
  cfc2 $fp, $vcc                                     ## L:512  |    112 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v22, $v00, $v22.h2                            ## L:526  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v21, $v21, $v23.h3                          ## L:534  |    113 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v07, 0, 24, $sp                               ## L:331  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadm $v20, $v20, $v23.h3                          ## L:534  |    114 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v01, 0, 0, $sp                                ## L:325  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadn $v21, $v00, $v00                             ## L:534  |    115 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v02, 0, 32, $sp                               ## L:326  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $sp                                ## L:327  |    116 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v24, 8, 8, $at                                ## L:531  |    117 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v07, 8, 24, $sp                               ## L:331  |    118 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrcph $v25.e3, $v20.e3                             ## L:541  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  srl $s1, $s0, 5                                    ## L:292  |    119 | t1 = t0 >> 5;
  vrcpl $v26.e3, $v21.e3                             ## L:541  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v08, 0, 56, $sp                               ## L:332  |    120 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcph $v25.e3, $v20.e7                             ## L:542  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v01, 8, 0, $sp                                ## L:325  |    121 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrcpl $v26.e7, $v21.e7                             ## L:542  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v02, 8, 32, $sp                               ## L:326  |    122 | m0:ufract = load(address, 0x20).xyzwxyzw;
  or $t6, $s0, $s1                                   ## L:299  |    123 | result = t0 | t1;
  vrcph $v25.e7, $v00.e7                             ## L:542  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  andi $s1, $fp, 1799                                ## L:291  |    124 | t0 = codes & 0x707;
  ssv $v20, 6, 16, $s7                               ## L:537  |    125 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vmudl $v29, $v21, $v26.h3                          ## L:547  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v20, $v26.h3                          ## L:547  |    126 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v20, 14, 16, $t2                              ## L:538  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v21, 6, 18, $s7                               ## L:537  |    127 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ssv $v21, 14, 18, $t2                              ## L:538  |    128 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vmadn $v21, $v21, $v25.h3                          ## L:547  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v06, 0, 48, $sp                               ## L:330  |    129 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v20, $v20, $v25.h3                          ## L:547  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v04, 0, 40, $sp                               ## L:328  |    130 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v03, 8, 8, $sp                                ## L:327  |    131 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 8, 48, $sp                               ## L:330  |    132 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 8, 56, $sp                               ## L:332  |    133 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:551  |      ^ | VTEMP = vviewoff:sint * 1;
  ldv $v04, 8, 40, $sp                               ## L:328  |    134 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadn $v21, $v21, $v23.v                           ## L:552  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  vmadh $v20, $v20, $v23.v                           ## L:552  |    135 | vpos_clip = vpos_clip +* vviewscale;
  ldv $v05, 0, 16, $sp                               ## L:329  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  srl $s2, $s1, 5                                    ## L:292  |    136 | t1 = t0 >> 5;
  vmudn $v29, $v02, $v15.h0                          ## L:566  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v05, 8, 16, $sp                               ## L:329  |    137 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v29, $v01, $v15.h0                          ## L:566  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v15.h1                          ## L:567  |    138 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ssv $v25, 6, 20, $s7                               ## L:544  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  nor $t7, $s1, $s2                                  ## L:306  |    139 | result = t0 ~| t1;
  vmadh $v29, $v03, $v15.h1                          ## L:567  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v29, $v06, $v15.h2                          ## L:568  |    140 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  srl $s0, $fp, 4                                    ## L:318  |      ^ | u16 codes2 = codes >> 4;
  andi $s1, $s0, 1799                                ## L:291  |    141 | t0 = codes & 0x707;
  ssv $v26, 6, 22, $s7                               ## L:544  |    142 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v25, 14, 20, $t2                              ## L:545  |    143 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  srl $s2, $s1, 5                                    ## L:292  |    144 | t1 = t0 >> 5;
  vmadh $v29, $v05, $v15.h2                          ## L:568  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v26, 14, 22, $t2                              ## L:545  |    145 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v29, $v08, $v15.h3                          ## L:569  |      ^ | vpos = vmvp3 +* vpos.wwwwWWWW;
  nor $k0, $s1, $s2                                  ## L:306  |    146 | result = t0 ~| t1;
  vmadh $v15, $v07, $v15.h3                          ## L:569  |      ^ | vpos = vmvp3 +* vpos.wwwwWWWW;
  beq $k1, $zero, LIGHT_END                          ## L:573  |    147 | if (light_count == 0) goto LIGHT_END;
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)               ## L:572  |   *149 | u16 light_ptr = LIGHTING_LIGHTS;
  LIGHT_LOOP:
  lh $fp, 6($sp)                                     ## L:579  |    150 | s16 light_w = load(light_ptr, 6);
  ldv $v24, 0, 0, $sp                                ## L:577  |    151 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v23, 0, 8, $sp                                ## L:576  |    152 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v24, 8, 0, $sp                                ## L:577  |    153 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $fp, $zero, LABEL_gl_pipeline_0002             ## L:580  |    154 | if (light_w != 0)
  ldv $v23, 8, 8, $sp                                ## L:576  |   *156 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  vsubc $v24, $v24, $v15.v                           ## L:594  |    157 | vlpos -= vpos;
  addiu $s0, $sp, 14                                 ## L:588  |      ^ | u16 att_ptr = light_ptr + 14;
  vmudh $v04, $v24, $v24.v                           ## L:595  | ***161 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 0, 0, $s0                                ## L:590  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  ldv $v01, 8, 0, $s0                                ## L:590  |    162 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v04, COP2_ACC_MD                             ## L:595  |      ^ | vec32 vsqdist = vlpos * vlpos;
  addiu $s0, $s0, 6                                  ## L:591  |    163 | att_ptr += 6;
  vsar $v03, COP2_ACC_HI                             ## L:595  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vaddc $v06, $v04, $v04.h1                          ## L:596  |  **166 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v05, $v03, $v03.h1                           ## L:596  |    167 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v02, 0, 0, $s0                                ## L:592  |      ^ | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  ldv $v02, 8, 0, $s0                                ## L:592  |  **170 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vaddc $v04, $v06, $v04.h2                          ## L:597  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:597  |    171 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:599  | ***175 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:599  |    176 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:600  |    177 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:600  |    178 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:600  |    179 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:602  |  **182 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:602  |    183 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:602  |    184 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:602  |    185 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v24, $v06.h0                          ## L:604  |    186 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v24, $v24, $v05.h0                          ## L:604  |    187 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:606  |    188 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:606  |    189 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:607  |    190 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:607  |    191 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:607  |    192 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:607  |    193 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:608  |    194 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:608  |    195 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:608  |    196 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:608  |    197 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vrcph $v01.e0, $v01.e0                             ## L:610  | ***201 | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:610  |    202 | vatten.x = invert_half(vatten).x;
  vrcph $v01.e0, $v01.e4                             ## L:611  |  **205 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:611  |    206 | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:611  |    207 | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v23, $v02.h0                          ## L:613  |  **210 | vlcol *= vatten.xxxxXXXX;
  vmadh $v23, $v23, $v01.h0                          ## L:613  |    211 | vlcol *= vatten.xxxxXXXX;
  LABEL_gl_pipeline_0002:
  addiu $sp, $sp, 26                                 ## L:635  |      ^ | light_ptr += 26;
  vmulf $v24, $v24, $v16.v                           ## L:627  |    212 | vlpos *= vnorm:sfract;
  vmulf $v29, $v23, $v24.h0                          ## L:628  | ***216 | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v23, $v24.h1                          ## L:629  |    217 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v23, $v23, $v24.h2                          ## L:630  |    218 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $k1, $k1, 65535                              ## L:636  |      ^ | light_count -= 1;
  vge $v23, $v23, $v00                               ## L:631  | ***222 | vlcol = max(vlcol, VZERO);
  bne $k1, $zero, LIGHT_LOOP                         ## L:637  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v23.v                            ## L:633  | ****227 | vrgba:sint += vlcol;
  LIGHT_END:
  ctc2 $t0, $vcc                                     ## L:645  |    228 | set_vcc(fog_mask);
  vmov $v18.e3, $v12.e3                              ## L:641  |      ^ | vrgba.w = vnormmask.w;
  addu $t1, $t1, $t9                                 ## L:710  |    229 | vtx_in1_ptr += vtx_size2;
  vmov $v18.e7, $v12.e3                              ## L:642  |      ^ | vrgba.W = vnormmask.w;
  ori $fp, $zero, %lo(MATRICES_MVP)                  ## L:651  |    230 | u16 addr = MATRICES_MVP;
  or $t3, $zero, $s7                                 ## L:707  |    231 | prev_out0 = vtx_out0_ptr;
  ldv $v08, 8, 56, $fp                               ## L:332  |   *233 | m3:ufract = load(address, 0x38).xyzwxyzw;
  PATCH_COLOR643: vmulf $v18, $v18, $v19.v           ## L:643  |      ^ | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  ldv $v02, 8, 32, $fp                               ## L:326  | ***237 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmrg $v18, $v18, $v22.h2                           ## L:646  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  ldv $v06, 0, 48, $fp                               ## L:330  |    238 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudh $v23, $v15, $v15.v                           ## L:655  |      ^ | vec32 vsqdist = vpos * vpos;
  ldv $v03, 8, 8, $fp                                ## L:327  |    239 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vsar $v23, COP2_ACC_MD                             ## L:655  |      ^ | vec32 vsqdist = vpos * vpos;
  addiu $s7, $s7, 80                                 ## L:711  |    240 | vtx_out0_ptr += 80;
  vsar $v22, COP2_ACC_HI                             ## L:655  |      ^ | vec32 vsqdist = vpos * vpos;
  ldv $v06, 8, 48, $fp                               ## L:330  |  **243 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vaddc $v25, $v23, $v23.h1                          ## L:656  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v02, 0, 32, $fp                               ## L:326  |    244 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vadd $v24, $v22, $v22.h1                           ## L:656  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  addu $s4, $s4, $t9                                 ## L:709  |  **247 | vtx_in0_ptr += vtx_size2;
  vaddc $v23, $v25, $v23.h2                          ## L:657  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:697  |    248 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  vadd $v22, $v24, $v22.h2                           ## L:657  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v24.e0, $v22.e0                             ## L:659  | ***252 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v25.e0, $v23.e0                             ## L:659  |    253 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v04, 8, 40, $fp                               ## L:328  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v03, 0, 8, $fp                                ## L:327  |    254 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrsqh $v24.e0, $v22.e4                             ## L:660  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v25.e4, $v23.e4                             ## L:660  |    255 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v01, 0, 0, $fp                                ## L:325  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrsqh $v24.e4, $v00.e0                             ## L:660  |    256 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v01, 8, 0, $fp                                ## L:325  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmudm $v29, $v15, $v25.h0                          ## L:662  |  **259 | vpos *= vtmp.xxxxXXXX;
  vmadh $v15, $v15, $v24.h0                          ## L:662  |    260 | vpos *= vtmp.xxxxXXXX;
  vmulf $v24, $v15, $v16.v                           ## L:665  | ***264 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:666  | ***268 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:667  | ***272 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  vor $v25, $v00, $v00                               ## L:672  |    273 | vec16 vzunit = VZERO;
  vmov $v25.e6, $v12.e3                              ## L:674  |    274 | vzunit.Z = vnormmask.w;
  vlt $v24, $v24, $v00                               ## L:669  |   *276 | vdot = min(vdot, VZERO);
  vsubc $v24, $v00, $v24.v                           ## L:670  | ***280 | vdot = VZERO - vdot;
  vmov $v25.e2, $v12.e3                              ## L:673  |    281 | vzunit.z = vnormmask.w;
  vmulf $v29, $v16, $v24.h0                          ## L:676  |  **284 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v16, $v24.h0                          ## L:677  |    285 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  vmadh $v29, $v15, $v30.e7                          ## L:678  |    286 | VTEMP = vpos:sint +* 1;
  LOAD_POS_NORM715: ldv $v16, 8, 0, $t1              ## L:715  |      ^ | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_POS_NORM714: ldv $v16, 0, 0, $s4              ## L:714  |    287 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  vmadh $v29, $v25, $v30.e7                          ## L:679  |      ^ | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:682  |    288 | vec32 vrefl = get_acc();
  ldv $v05, 0, 16, $fp                               ## L:329  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vsar $v24, COP2_ACC_HI                             ## L:682  |    289 | vec32 vrefl = get_acc();
  ldv $v05, 8, 16, $fp                               ## L:329  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmudl $v29, $v25, $v25.v                           ## L:684  |  **292 | vsqdist = vrefl * vrefl;
  vmadm $v29, $v24, $v25.v                           ## L:684  |    293 | vsqdist = vrefl * vrefl;
  vmadn $v23, $v25, $v24.v                           ## L:684  |    294 | vsqdist = vrefl * vrefl;
  LOAD_POS_NORM717: ldv $v15, 8, 0, $t1              ## L:717  |      ^ | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  or $t4, $zero, $t2                                 ## L:708  |    295 | prev_out1 = vtx_out1_ptr;
  vmadh $v22, $v24, $v24.v                           ## L:684  |      ^ | vsqdist = vrefl * vrefl;
  addiu $t2, $t2, 80                                 ## L:712  |    296 | vtx_out1_ptr += 80;
  LOAD_POS_NORM716: ldv $v15, 0, 0, $s4              ## L:716  |    297 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vxor $v17, $v00, $v30.e0                           ## L:691  |      ^ | vtex = 0x80;
  vaddc $v27, $v23, $v23.h1                          ## L:685  |    298 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v26, $v22, $v22.h1                           ## L:685  |    299 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v23, $v27, $v23.h2                          ## L:686  |  **302 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v26, $v22.h2                           ## L:686  |    303 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v07, 0, 24, $fp                               ## L:331  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsqh $v26.e0, $v22.e0                             ## L:688  | ***307 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v07, 8, 24, $fp                               ## L:331  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsql $v27.e0, $v23.e0                             ## L:688  |    308 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v04, 0, 40, $fp                               ## L:328  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrsqh $v26.e0, $v22.e4                             ## L:689  |    309 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 0, 0, $at                                ## L:697  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v22, 8, 0, $at                                ## L:698  |    310 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  vrsql $v27.e4, $v23.e4                             ## L:689  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v26.e4, $v00.e0                             ## L:689  |    311 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudh $v29, $v17, $v30.e7                          ## L:692  |    312 | VTEMP = vtex:sint * 1;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:699  |      ^ | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 0, 0, $at                                ## L:699  |    313 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 8, 0, $at                                ## L:700  |    314 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  vmadl $v29, $v25, $v27.h0                          ## L:693  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_COLOR719: llv $v19, 4, 0, $t1                 ## L:719  |    315 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmadm $v29, $v24, $v27.h0                          ## L:693  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  vmadn $v29, $v25, $v26.h0                          ## L:693  |    316 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v08, 0, 56, $fp                               ## L:332  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  LOAD_COLOR718: llv $v19, 0, 0, $s4                 ## L:718  |    317 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmadh $v17, $v24, $v26.h0                          ## L:693  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  sltu $at, $s4, $s6                                 ## L:721  |    318 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmudh $v29, $v23, $v30.e7                          ## L:704  |      ^ | VTEMP = vtexoffset:sint * 1;
  bne $at, $zero, VERTEX_LOOP                        ## L:721  |    319 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:705  |  **322 | vtex:sint = vtex:sint +* vtexscale;
  sdv $v20, 0, 0, $t3                                ## L:727  |    323 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v18, 0, 8, $t3                                ## L:735  |    324 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v20, 8, 0, $t4                                ## L:728  |    325 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sb $t7, 7($t3)                                     ## L:732  |    326 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  suv $v18, 4, 8, $t4                                ## L:736  |    327 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  slv $v17, 0, 12, $t3                               ## L:738  |    328 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  slv $v17, 8, 12, $t4                               ## L:739  |    329 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $t5, 6($t3)                                     ## L:730  |    330 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $k0, 7($t4)                                     ## L:733  |    331 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  j RSPQ_Loop                                        ## L:740  |    332 | }
  sb $t6, 6($t4)                                     ## L:731  |   *334 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10

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