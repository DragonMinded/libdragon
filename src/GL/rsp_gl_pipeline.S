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
    MgVertexAttributeLoaders LOAD_POS_NORM431, LOAD_POS_NORM432, LOAD_POS_NORM433, LOAD_POS_NORM434, LOAD_POS_NORM717, LOAD_POS_NORM714, LOAD_POS_NORM716, LOAD_POS_NORM715
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR435, LOAD_COLOR436, LOAD_COLOR719, LOAD_COLOR718
    MgBeginVertexAttributePatch PATCH_COLOR643
      vnop
    MgEndVertexAttributePatch
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD463, LOAD_TEXCOORD466
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:345  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:344  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v01, 0, 0, $at                                ## L:345  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:346  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:348  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:362  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v03, COP2_ACC_HI                             ## L:348  |      9 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:349  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v04, COP2_ACC_MD                             ## L:348  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:370  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ssv $v03, 0, 8, $at                                ## L:350  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:350  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:360  |     15 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:349  |     16 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:356  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:358  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:365  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:363  |     20 | vtx_buffer += buffer_offset;
  addu $t0, $t1, $t3                                 ## L:366  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:367  |     22 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:368  |     23 | dma_size &= 0xFF8;
  addiu $t2, $zero, 12                               ## L:373  |     24 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  subu $s4, $s4, $t0                                 ## L:371  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:373  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:373  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:392  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:378  |     30 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:387  |     31 | u16 mvpaddr = MATRICES_MVP;
  ldv $v09, 0, 0, $at                                ## L:392  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:393  |     33 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:375  |     34 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v09, 8, 0, $at                                ## L:392  |     35 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:394  |     36 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  LOAD_POS_NORM431: ldv $v20, 0, 0, $s4              ## L:431  |     37 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  addu $t2, $s4, $t8                                 ## L:416  |     38 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ldv $v11, 8, 16, $at                               ## L:394  |     39 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  LOAD_POS_NORM432: ldv $v20, 8, 0, $t2              ## L:432  |     40 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  sll $t9, $t8, 1                                    ## L:376  |     41 | vtx_size2 = vtx_size * 2;
  ldv $v03, 0, 8, $t0                                ## L:327  |     42 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:393  |     43 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:396  |     44 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:396  |     45 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:330  |     46 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:329  |     47 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:328  |     48 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_COLOR435: llv $v23, 0, 0, $s4                 ## L:435  |     49 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v05, 8, 16, $t0                               ## L:329  |     50 | m2:sint   = load(address, 0x10).xyzwxyzw;
  LOAD_COLOR436: llv $v23, 4, 0, $t2                 ## L:436  |     51 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v13, 0, 8, $at                                ## L:397  |     52 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  LOAD_POS_NORM433: ldv $v19, 0, 0, $s4              ## L:433  |     53 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v12, 8, 0, $at                                ## L:396  |     54 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:397  |     55 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $t5, $zero, %lo(MG_CLIP_CACHE)                 ## L:420  |     56 | u16 prev_out1 = MG_CLIP_CACHE;
  ldv $v04, 8, 40, $t0                               ## L:328  |     57 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_POS_NORM434: ldv $v19, 8, 0, $t2              ## L:434  |     58 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:400  |     59 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:327  |     60 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:331  |     61 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v15, 0, 8, $at                                ## L:401  |     62 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v14, 0, 0, $at                                ## L:400  |     63 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:331  |     64 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:330  |     65 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v14, 8, 0, $at                                ## L:400  |     66 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:326  |     67 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v15, 8, 8, $at                                ## L:401  |     68 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:404  |     69 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  ldv $v01, 0, 0, $t0                                ## L:325  |     70 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:332  |     71 | m3:ufract = load(address, 0x38).xyzwxyzw;
  llv $v16, 0, 0, $at                                ## L:404  |     72 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v16, 8, 0, $at                                ## L:405  |     73 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  ldv $v08, 8, 56, $t0                               ## L:332  |     74 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:326  |     75 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:406  |     76 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v17, 0, 0, $at                                ## L:406  |     77 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  ldv $v01, 8, 0, $t0                                ## L:325  |     78 | m0:sint   = load(address, 0x00).xyzwxyzw;
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:409  |     79 | u8 has_points = load(LIGHTING_HAS_POINTS);
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:379  |     80 | vtx_out0_ptr += MG_VERTEX_CACHE;
  lbu $t1, %lo(FOG_MASK + 0)                         ## L:414  |     81 | u8 fog_mask = load(FOG_MASK);
  llv $v17, 8, 0, $at                                ## L:407  |     82 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  addiu $t3, $s7, 40                                 ## L:417  |     83 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:413  |     84 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v18, 0, 0, $at                                ## L:413  |     85 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v18, 8, 0, $at                                ## L:413  |     86 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $t4, $zero, %lo(MG_CLIP_CACHE)                 ## L:419  |     87 | u16 prev_out0 = MG_CLIP_CACHE;
  VERTEX_LOOP:
  vand $v20, $v12, $v20.h3                           ## L:470  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:458  |     88 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmov $v19.e3, $v30.e2                              ## L:472  |      ^ | vpos.w = 0x20;
  suv $v22, 4, 8, $t5                                ## L:453  |     89 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmov $v19.e7, $v30.e2                              ## L:473  |      ^ | vpos.W = 0x20;
  sdv $v23, 0, 0, $at                                ## L:458  |     90 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  luv $v23, 0, 0, $at                                ## L:459  |     91 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  suv $v22, 0, 8, $t4                                ## L:452  |     92 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmudn $v20, $v20, $v13.v                           ## L:475  |      ^ | vnorm *= vnormfactor;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:487  |     93 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmudn $v29, $v02, $v19.h0                          ## L:478  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v19.h0                          ## L:478  |     94 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v22, 0, 0, $at                                ## L:487  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:487  |     95 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v06, $v19.h2                          ## L:479  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:506  |     96 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v26, 0, 0, $at                                ## L:506  |     97 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v05, $v19.h2                          ## L:479  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  sdv $v24, 0, 0, $t4                                ## L:444  |     98 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmadn $v29, $v04, $v19.h1                          ## L:480  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v26, 8, 0, $at                                ## L:506  |     99 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v03, $v19.h1                          ## L:480  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v25, $v08, $v19.h3                          ## L:481  |    100 | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  sdv $v24, 8, 0, $t5                                ## L:445  |      ^ | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmadh $v24, $v07, $v19.h3                          ## L:481  |    101 | vpos_clip = vmvp3 +* vpos.wwwwWWWW;
  vmulf $v29, $v09, $v20.h0                          ## L:483  |    102 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  slv $v21, 8, 12, $t5                               ## L:456  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmacf $v29, $v10, $v20.h1                          ## L:484  |    103 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  sb $k0, 7($t4)                                     ## L:449  |      ^ | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vmacf $v20, $v11, $v20.h2                          ## L:485  |    104 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  slv $v21, 0, 12, $t4                               ## L:455  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  LOAD_TEXCOORD463: llv $v21, 0, 0, $s4              ## L:463  |    105 | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  vmudn $v28, $v25, $v26.v                           ## L:507  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v27, $v24, $v26.v                           ## L:507  |    106 | vec32 vguard = vpos_clip * vclip_factors;
  LOAD_TEXCOORD466: llv $v21, 8, 0, $t2              ## L:466  |      ^ | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  sdv $v24, 0, 24, $s7                               ## L:516  |  **109 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v24, 8, 24, $t3                               ## L:517  |    110 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vch $v29, $v27, $v27.h3                            ## L:508  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $t6, 6($t4)                                     ## L:447  |    111 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vcl $v29, $v28, $v28.h3                            ## L:508  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $fp, $vcc                                     ## L:508  |    112 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v24, $v24.h3                            ## L:512  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sb $t7, 6($t5)                                     ## L:448  |    113 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vcl $v29, $v25, $v25.h3                            ## L:512  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vaddc $v27, $v25, $v18.h1                          ## L:521  |    114 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  lbu $sp, %lo(LIGHTING_COUNT + 0)                   ## L:488  |      ^ | u8 light_count = load(LIGHTING_COUNT);
  vadd $v26, $v24, $v18.h0                           ## L:522  |    115 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s1, $fp, 1799                                ## L:291  |      ^ | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:292  |    116 | t1 = t0 >> 5;
  srl $s0, $fp, 4                                    ## L:311  |   *118 | u16 codes2 = codes >> 4;
  vmudn $v29, $v27, $v18.h2                          ## L:524  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  or $t6, $s1, $s2                                   ## L:299  |    119 | result = t0 | t1;
  vmadh $v26, $v26, $v18.h2                          ## L:525  |      ^ | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  sdv $v25, 0, 32, $s7                               ## L:516  |    120 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  andi $s1, $s0, 1799                                ## L:291  |    121 | t0 = codes & 0x707;
  sdv $v25, 8, 32, $t3                               ## L:517  |    122 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  cfc2 $s0, $vcc                                     ## L:512  |    123 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v26, $v00, $v26.h2                            ## L:526  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  srl $s2, $s1, 5                                    ## L:292  |    124 | t1 = t0 >> 5;
  vmudl $v25, $v25, $v14.h3                          ## L:534  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v24, $v24, $v14.h3                          ## L:534  |    125 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  or $t7, $s1, $s2                                   ## L:299  |      ^ | result = t0 | t1;
  andi $s2, $s0, 1799                                ## L:291  |    126 | t0 = codes & 0x707;
  vmadn $v25, $v00, $v00                             ## L:534  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  sb $k1, 7($t5)                                     ## L:450  |    127 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  srl $s1, $s0, 4                                    ## L:318  |    128 | u16 codes2 = codes >> 4;
  srl $s3, $s2, 5                                    ## L:292  |    129 | t1 = t0 >> 5;
  vrcph $v27.e3, $v24.e3                             ## L:541  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  nor $k0, $s2, $s3                                  ## L:306  |    130 | result = t0 ~| t1;
  vrcpl $v28.e3, $v25.e3                             ## L:541  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcph $v27.e3, $v24.e7                             ## L:542  |    131 | vinvw.W = invert_half(vpos_clip).W;
  andi $s2, $s1, 1799                                ## L:291  |      ^ | t0 = codes & 0x707;
  vrcpl $v28.e7, $v25.e7                             ## L:542  |    132 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v24, 6, 16, $s7                               ## L:537  |      ^ | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ssv $v24, 14, 16, $t3                              ## L:538  |    133 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v27.e7, $v00.e7                             ## L:542  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  srl $s3, $s2, 5                                    ## L:292  |    134 | t1 = t0 >> 5;
  ssv $v25, 14, 18, $t3                              ## L:538  |    135 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  nor $k1, $s2, $s3                                  ## L:306  |    136 | result = t0 ~| t1;
  vmudl $v29, $v25, $v28.h3                          ## L:547  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 6, 18, $s7                               ## L:537  |    137 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vmadm $v29, $v24, $v28.h3                          ## L:547  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v27, 14, 20, $t3                              ## L:545  |    138 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v25, $v25, $v27.h3                          ## L:547  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadh $v24, $v24, $v27.h3                          ## L:547  |    139 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v28, 14, 22, $t3                              ## L:545  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  ssv $v27, 6, 20, $s7                               ## L:544  |    140 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v28, 6, 22, $s7                               ## L:544  |    141 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmudh $v29, $v15, $v30.e7                          ## L:551  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v25, $v25, $v14.v                           ## L:552  |    142 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_gl_pipeline_0001             ## L:560  |      ^ | if (has_points != 0)
  vmadh $v24, $v24, $v14.v                           ## L:552  |   *144 | vpos_clip = vpos_clip +* vviewscale;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:563  |    145 | u16 addr = MATRICES_MV;
  ldv $v04, 0, 40, $fp                               ## L:328  |    146 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v02, 0, 32, $fp                               ## L:326  |    147 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $fp                                ## L:327  |    148 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v02, 8, 32, $fp                               ## L:326  |    149 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 8, 8, $fp                                ## L:327  |    150 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:330  |    151 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v01, 0, 0, $fp                                ## L:325  |    152 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 8, 0, $fp                                ## L:325  |    153 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v07, 0, 24, $fp                               ## L:331  |    154 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v04, 8, 40, $fp                               ## L:328  |    155 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v06, 8, 48, $fp                               ## L:330  |    156 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudn $v29, $v02, $v19.h0                          ## L:566  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v05, 0, 16, $fp                               ## L:329  |    157 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 0, 56, $fp                               ## L:332  |    158 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v29, $v01, $v19.h0                          ## L:566  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v19.h1                          ## L:567  |    159 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v05, 8, 16, $fp                               ## L:329  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 8, 56, $fp                               ## L:332  |    160 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v29, $v03, $v19.h1                          ## L:567  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v07, 8, 24, $fp                               ## L:331  |    161 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v29, $v06, $v19.h2                          ## L:568  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v19.h2                          ## L:568  |   *163 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v19.h3                          ## L:569  |    164 | vpos = vmvp3 +* vpos.wwwwWWWW;
  vmadh $v19, $v07, $v19.h3                          ## L:569  |    165 | vpos = vmvp3 +* vpos.wwwwWWWW;
  LABEL_gl_pipeline_0001:
  ori $fp, $zero, %lo(LIGHTING_LIGHTS)               ## L:572  |      ^ | u16 light_ptr = LIGHTING_LIGHTS;
  beq $sp, $zero, LIGHT_END                          ## L:573  |    166 | if (light_count == 0) goto LIGHT_END;
  nop                                                ## L:573  |   *168 | if (light_count == 0) goto LIGHT_END;
  LIGHT_LOOP:
  lh $s0, 6($fp)                                     ## L:579  |    169 | s16 light_w = load(light_ptr, 6);
  ldv $v28, 0, 0, $fp                                ## L:577  |    170 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v28, 8, 0, $fp                                ## L:577  |    171 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v27, 0, 8, $fp                                ## L:576  |    172 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  beq $s0, $zero, LABEL_gl_pipeline_0003             ## L:580  |    173 | if (light_w != 0)
  ldv $v27, 8, 8, $fp                                ## L:576  |   *175 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  vsubc $v28, $v28, $v19.v                           ## L:594  |    176 | vlpos -= vpos;
  vmudh $v04, $v28, $v28.v                           ## L:595  | ***180 | vec32 vsqdist = vlpos * vlpos;
  addiu $s1, $fp, 14                                 ## L:588  |      ^ | u16 att_ptr = light_ptr + 14;
  vsar $v04, COP2_ACC_MD                             ## L:595  |    181 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 0, 0, $s1                                ## L:590  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v03, COP2_ACC_HI                             ## L:595  |    182 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 8, 0, $s1                                ## L:590  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  addiu $s1, $s1, 6                                  ## L:591  |  **185 | att_ptr += 6;
  vaddc $v06, $v04, $v04.h1                          ## L:596  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v02, 0, 0, $s1                                ## L:592  |    186 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vadd $v05, $v03, $v03.h1                           ## L:596  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:597  |  **189 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v02, 8, 0, $s1                                ## L:592  |      ^ | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vadd $v03, $v05, $v03.h2                           ## L:597  |    190 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:622  |      ^ | u16 addr = MATRICES_MVP;
  vrsqh $v05.e0, $v03.e0                             ## L:599  | ***194 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:599  |    195 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:600  |    196 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:600  |    197 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:600  |    198 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:602  |  **201 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:602  |    202 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:602  |    203 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:602  |    204 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v28, $v06.h0                          ## L:604  |    205 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v28, $v28, $v05.h0                          ## L:604  |    206 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:606  |    207 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:606  |    208 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:607  |    209 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:607  |    210 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:607  |    211 | VTEMP = vdist +* vatten.yyyyYYYY;
  ldv $v08, 8, 56, $s1                               ## L:332  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v29, $v07, $v01.h1                          ## L:607  |    212 | VTEMP = vdist +* vatten.yyyyYYYY;
  ldv $v07, 8, 24, $s1                               ## L:331  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v07, 0, 24, $s1                               ## L:331  |    213 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadl $v29, $v04, $v02.h2                          ## L:608  |      ^ | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:608  |    214 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:608  |    215 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v05, 8, 16, $s1                               ## L:329  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v06, 0, 48, $s1                               ## L:330  |    216 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v01, $v03, $v01.h2                          ## L:608  |      ^ | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v03, 0, 8, $s1                                ## L:327  |    217 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v05, 0, 16, $s1                               ## L:329  |    218 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v04, 0, 40, $s1                               ## L:328  |   *220 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrcph $v01.e0, $v01.e0                             ## L:610  |      ^ | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:610  |    221 | vatten.x = invert_half(vatten).x;
  ldv $v08, 0, 56, $s1                               ## L:332  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v04, 8, 40, $s1                               ## L:328  |  **224 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrcph $v01.e0, $v01.e4                             ## L:611  |      ^ | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:611  |    225 | vatten.X = invert_half(vatten).X;
  ldv $v03, 8, 8, $s1                                ## L:327  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 8, 48, $s1                               ## L:330  |    226 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrcph $v01.e4, $v00.e4                             ## L:611  |      ^ | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v27, $v02.h0                          ## L:613  |  **229 | vlcol *= vatten.xxxxXXXX;
  ldv $v02, 0, 32, $s1                               ## L:326  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v02, 8, 32, $s1                               ## L:326  |    230 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v27, $v27, $v01.h0                          ## L:613  |      ^ | vlcol *= vatten.xxxxXXXX;
  ldv $v01, 0, 0, $s1                                ## L:325  |    231 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 8, 0, $s1                                ## L:325  |    232 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LABEL_gl_pipeline_0003:
  vmulf $v28, $v28, $v20.v                           ## L:627  |      ^ | vlpos *= vnorm:sfract;
  vmulf $v29, $v27, $v28.h0                          ## L:628  | ***236 | VTEMP = vlcol:sfract  * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v27, $v28.h1                          ## L:629  |    237 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $fp, $fp, 26                                 ## L:635  |      ^ | light_ptr += 26;
  vmacf $v27, $v27, $v28.h2                          ## L:630  |    238 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $sp, $sp, 65535                              ## L:636  |      ^ | light_count -= 1;
  vge $v27, $v27, $v00                               ## L:631  | ***242 | vlcol = max(vlcol, VZERO);
  bne $sp, $zero, LIGHT_LOOP                         ## L:637  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v22, $v22, $v27.v                            ## L:633  | ****247 | vrgba:sint += vlcol;
  LIGHT_END:
  addu $t2, $t2, $t9                                 ## L:710  |    248 | vtx_in1_ptr += vtx_size2;
  vmov $v22.e3, $v12.e3                              ## L:641  |      ^ | vrgba.w = vnormmask.w;
  vmov $v22.e7, $v12.e3                              ## L:642  |    249 | vrgba.W = vnormmask.w;
  or $t5, $zero, $t3                                 ## L:708  |      ^ | prev_out1 = vtx_out1_ptr;
  LOAD_POS_NORM717: ldv $v19, 8, 0, $t2              ## L:717  |    250 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  or $t4, $zero, $s7                                 ## L:707  |    251 | prev_out0 = vtx_out0_ptr;
  addu $s4, $s4, $t9                                 ## L:709  |    252 | vtx_in0_ptr += vtx_size2;
  ctc2 $t1, $vcc                                     ## L:645  |    253 | set_vcc(fog_mask);
  PATCH_COLOR643: vmulf $v22, $v22, $v23.v           ## L:643  |      ^ | @AttrPatch("COLOR:vnop") vrgba:sfract *= vrgba_in:sfract;
  addiu $t3, $t3, 80                                 ## L:712  |    254 | vtx_out1_ptr += 80;
  LOAD_POS_NORM714: ldv $v20, 0, 0, $s4              ## L:714  |    255 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM716: ldv $v19, 0, 0, $s4              ## L:716  |    256 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR719: llv $v23, 4, 0, $t2                 ## L:719  |    257 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmrg $v22, $v22, $v26.h2                           ## L:646  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v29, $v17, $v30.e7                          ## L:704  |    258 | VTEMP = vtexoffset:sint * 1;
  LOAD_COLOR718: llv $v23, 0, 0, $s4                 ## L:718  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  sltu $at, $s4, $s6                                 ## L:721  |    259 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  LOAD_POS_NORM715: ldv $v20, 8, 0, $t2              ## L:715  |    260 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v21, $v21, $v16.v                           ## L:705  |      ^ | vtex:sint = vtex:sint +* vtexscale;
  bne $at, $zero, VERTEX_LOOP                        ## L:721  |    261 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  addiu $s7, $s7, 80                                 ## L:711  |   *263 | vtx_out0_ptr += 80;
  sdv $v24, 0, 0, $t4                                ## L:727  |    264 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sdv $v24, 8, 0, $t5                                ## L:728  |    265 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sb $k1, 7($t5)                                     ## L:733  |    266 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  suv $v22, 4, 8, $t5                                ## L:736  |    267 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  suv $v22, 0, 8, $t4                                ## L:735  |    268 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  slv $v21, 0, 12, $t4                               ## L:738  |    269 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t6, 6($t4)                                     ## L:730  |    270 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  slv $v21, 8, 12, $t5                               ## L:739  |    271 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $k0, 7($t4)                                     ## L:732  |    272 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  j RSPQ_Loop                                        ## L:740  |    273 | }
  sb $t7, 6($t5)                                     ## L:731  |   *275 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10

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