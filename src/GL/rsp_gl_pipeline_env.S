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
#define GLP_LIGHT_POSITION 0
#define GLP_LIGHT_POSITIONW 6
#define GLP_LIGHT_DIFFUSE 8
#define GLP_LIGHT_AMBIENT 14
#define GLP_LIGHT_ATT_INT 20
#define GLP_LIGHT_ATT_FRAC 26
#define GLP_LIGHT_SIZE 32
#define GLP_ATTRIBUTE_POS_NORM 0
#define GLP_ATTRIBUTE_COLOR 1
#define GLP_ATTRIBUTE_TEXCOORD 2
#define GLP_BINDING_MATRICES 0
#define GLP_BINDING_TEXTURING 1
#define GLP_BINDING_LIGHTING 2
#define GLP_BINDING_FOG 3
#define GLP_VTX_POS_SHIFT 5
#define GLP_VTX_TEX_SHIFT 8
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
    LIGHTING_AMBIENT: .ds.b 6
    LIGHTING_HAS_POINTS: .ds.b 1
    LIGHTING_COUNT: .ds.b 1
    .align 1
    MATERIAL_DIFFUSE: .ds.b 8
    .align 1
    MATERIAL_AMBIENT: .ds.b 6
    .align 1
    MATERIAL_FLAGS: .ds.b 2
    .align 1
    MATERIAL_EMISSIVE: .ds.b 6
  MgEndUniform

  MgBeginUniform FOG, 3
    .align 1
    FOG_PARAMS: .ds.b 6
    FOG_MASK: .ds.b 1
  MgEndUniform

MgEndShaderUniforms

MgBeginVertexInput
  MgBeginVertexAttribute 0, 0
    MgVertexAttributeLoaders LOAD_POS_NORM482, LOAD_POS_NORM483, LOAD_POS_NORM484, LOAD_POS_NORM485, LOAD_POS_NORM776, LOAD_POS_NORM777, LOAD_POS_NORM779, LOAD_POS_NORM778
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR486, LOAD_COLOR487, LOAD_COLOR780, LOAD_COLOR781
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:405  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:404  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:406  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:405  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:422  |   ***8 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vmudh $v04, $v02, $v01.v                           ## L:408  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:409  |      9 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:408  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  vsar $v04, COP2_ACC_MD                             ## L:408  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:430  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ssv $v03, 0, 8, $at                                ## L:410  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:409  |     14 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v04, 0, 10, $at                               ## L:410  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:420  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:416  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:418  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:425  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:423  |     20 | vtx_buffer += buffer_offset;
  addu $t0, $t1, $t3                                 ## L:426  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:427  |     22 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:428  |     23 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:431  |     24 | vtx_in0_ptr -= dma_size;
  addiu $t2, $zero, 12                               ## L:433  |     25 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  jal DMAExec                                        ## L:433  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:433  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addu $s6, $s4, $t1                                 ## L:438  |     29 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:447  |     30 | u16 mvpaddr = MATRICES_MVP;
  ldv $v08, 0, 56, $t0                               ## L:392  |     31 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:452  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:385  |     33 | m0:sint   = load(address, 0x00).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:435  |     34 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v05, 0, 16, $t0                               ## L:389  |     35 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:454  |     36 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:390  |     37 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:454  |     38 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:453  |     39 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:439  |     40 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v07, 0, 24, $t0                               ## L:391  |     41 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:452  |     42 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:392  |     43 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:453  |     44 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:391  |     45 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:386  |     46 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:452  |     47 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:456  |     48 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:456  |     49 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  LOAD_COLOR486: llv $v19, 0, 0, $s4                 ## L:486  |     50 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v02, 8, 32, $t0                               ## L:386  |     51 | m0:ufract = load(address, 0x20).xyzwxyzw;
  LOAD_POS_NORM482: ldv $v16, 0, 0, $s4              ## L:482  |     52 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  sll $t9, $t8, 1                                    ## L:436  |     53 | vtx_size2 = vtx_size * 2;
  addu $t1, $s4, $t8                                 ## L:467  |     54 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ldv $v01, 8, 0, $t0                                ## L:385  |     55 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ori $t4, $zero, %lo(MG_CLIP_CACHE)                 ## L:471  |     56 | u16 prev_out1 = MG_CLIP_CACHE;
  ldv $v12, 8, 0, $at                                ## L:456  |     57 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:457  |     58 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  LOAD_POS_NORM483: ldv $v16, 8, 0, $t1              ## L:483  |     59 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v04, 0, 40, $t0                               ## L:388  |     60 | m1:ufract = load(address, 0x28).xyzwxyzw;
  LOAD_COLOR487: llv $v19, 4, 0, $t1                 ## L:487  |     61 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v06, 8, 48, $t0                               ## L:390  |     62 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:388  |     63 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:457  |     64 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:387  |     65 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:464  |     66 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  addiu $t2, $s7, 40                                 ## L:468  |     67 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  LOAD_POS_NORM484: ldv $v15, 0, 0, $s4              ## L:484  |     68 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v14, 0, 0, $at                                ## L:464  |     69 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:389  |     70 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $t3, $zero, %lo(MG_CLIP_CACHE)                 ## L:470  |     71 | u16 prev_out0 = MG_CLIP_CACHE;
  ldv $v03, 8, 8, $t0                                ## L:387  |     72 | m1:sint   = load(address, 0x08).xyzwxyzw;
  lbu $t0, %lo(FOG_MASK + 0)                         ## L:465  |     73 | u8 fog_mask = load(FOG_MASK);
  LOAD_POS_NORM485: ldv $v15, 8, 0, $t1              ## L:485  |     74 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v14, 8, 0, $at                                ## L:464  |     75 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h3                           ## L:521  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:509  |     76 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v19, 0, 0, $at                                ## L:509  |    *78 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v20, 8, 0, $t4                                ## L:496  |     79 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmudn $v16, $v16, $v13.v                           ## L:522  |      ^ | vnorm *= vnormfactor;
  luv $v19, 0, 0, $at                                ## L:510  |     80 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v29, $v02, $v15.h0                          ## L:525  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sb $k0, 7($t4)                                     ## L:501  |     81 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmadh $v29, $v01, $v15.h0                          ## L:525  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:550  |     82 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v29, $v06, $v15.h2                          ## L:526  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ldv $v22, 0, 0, $at                                ## L:550  |     83 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v05, $v15.h2                          ## L:526  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  suv $v18, 4, 8, $t4                                ## L:504  |     84 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadn $v29, $v04, $v15.h1                          ## L:527  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v22, 8, 0, $at                                ## L:550  |     85 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:527  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  sdv $v20, 0, 0, $t3                                ## L:495  |     86 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmadn $v21, $v08, $v30.e2                          ## L:528  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmadh $v20, $v07, $v30.e2                          ## L:528  |     87 | vpos_clip = vmvp3 +* 0x20;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:573  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  sb $t6, 6($t4)                                     ## L:499  |     88 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vmulf $v29, $v09, $v16.h0                          ## L:530  |      ^ | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  suv $v18, 0, 8, $t3                                ## L:503  |     89 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmacf $v29, $v10, $v16.h1                          ## L:531  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  sb $t7, 7($t3)                                     ## L:500  |     90 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vmacf $v16, $v11, $v16.h2                          ## L:532  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudn $v24, $v21, $v22.v                           ## L:551  |     91 | vec32 vguard = vpos_clip * vclip_factors;
  slv $v17, 0, 12, $t3                               ## L:506  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmadh $v23, $v20, $v22.v                           ## L:551  |     92 | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v20, 0, 24, $s7                               ## L:560  |      ^ | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sb $t5, 6($t3)                                     ## L:498  |     93 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sdv $v21, 0, 32, $s7                               ## L:560  |     94 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v20, 8, 24, $t2                               ## L:561  |     95 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v21, 8, 32, $t2                               ## L:561  |     96 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vch $v29, $v23, $v23.h3                            ## L:552  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v24, $v24.h3                            ## L:552  |     97 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  slv $v17, 8, 12, $t4                               ## L:507  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  cfc2 $k1, $vcc                                     ## L:552  |     98 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:556  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v24, 0, 8, $at                                ## L:574  |     99 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vcl $v29, $v21, $v21.h3                            ## L:556  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vaddc $v23, $v21, $v14.h1                          ## L:565  |    100 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  ldv $v24, 8, 8, $at                                ## L:574  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  andi $fp, $k1, 1799                                ## L:351  |    101 | t0 = codes & 0x707;
  vadd $v22, $v20, $v14.h0                           ## L:566  |      ^ | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  srl $s0, $fp, 5                                    ## L:352  |    102 | t1 = t0 >> 5;
  srl $sp, $k1, 4                                    ## L:371  |    103 | u16 codes2 = codes >> 4;
  or $t5, $fp, $s0                                   ## L:359  |    104 | result = t0 | t1;
  vmudn $v29, $v23, $v14.h2                          ## L:568  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v22, $v22, $v14.h2                          ## L:569  |    105 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:573  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  andi $fp, $sp, 1799                                ## L:351  |    106 | t0 = codes & 0x707;
  ldv $v23, 8, 0, $at                                ## L:573  |    107 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ori $k1, $zero, %lo(MATRICES_MV)                   ## L:603  |    108 | u16 addr = MATRICES_MV;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:612  |    109 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  cfc2 $sp, $vcc                                     ## L:556  |    110 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v22, $v00, $v22.h2                            ## L:570  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v21, $v21, $v23.h3                          ## L:576  |    111 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  srl $s0, $fp, 5                                    ## L:352  |      ^ | t1 = t0 >> 5;
  ldv $v06, 0, 48, $k1                               ## L:390  |    112 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadm $v20, $v20, $v23.h3                          ## L:576  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadn $v21, $v00, $v00                             ## L:576  |    113 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v01, 0, 0, $k1                                ## L:385  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v07, 0, 24, $k1                               ## L:391  |    114 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v01, 8, 0, $k1                                ## L:385  |    115 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v03, 0, 8, $k1                                ## L:387  |    116 | m1:sint   = load(address, 0x08).xyzwxyzw;
  or $t6, $fp, $s0                                   ## L:359  |    117 | result = t0 | t1;
  vrcph $v25.e3, $v20.e3                             ## L:583  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v02, 0, 32, $k1                               ## L:386  |    118 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v05, 0, 16, $k1                               ## L:389  |    119 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrcpl $v26.e3, $v21.e3                             ## L:583  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  andi $s0, $sp, 1799                                ## L:351  |    120 | t0 = codes & 0x707;
  vrcph $v25.e3, $v20.e7                             ## L:584  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcpl $v26.e7, $v21.e7                             ## L:584  |    121 | vinvw.W = invert_half(vpos_clip).W;
  srl $s1, $s0, 5                                    ## L:352  |      ^ | t1 = t0 >> 5;
  ssv $v20, 6, 16, $s7                               ## L:579  |    122 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v07, 8, 24, $k1                               ## L:391  |    123 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ssv $v21, 6, 18, $s7                               ## L:579  |    124 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcph $v25.e7, $v00.e7                             ## L:584  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v18, 0, 0, $at                                ## L:612  |    125 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmudl $v29, $v21, $v26.h3                          ## L:589  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v20, 14, 16, $t2                              ## L:580  |    126 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v05, 8, 16, $k1                               ## L:389  |    127 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ssv $v21, 14, 18, $t2                              ## L:580  |    128 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vmadm $v29, $v20, $v26.h3                          ## L:589  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v08, 0, 56, $k1                               ## L:392  |    129 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadn $v21, $v21, $v25.h3                          ## L:589  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadh $v20, $v20, $v25.h3                          ## L:589  |    130 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v18, 8, 0, $at                                ## L:612  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:593  |    131 | VTEMP = vviewoff:sint * 1;
  ldv $v04, 0, 40, $k1                               ## L:388  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v06, 8, 48, $k1                               ## L:390  |    132 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v02, 8, 32, $k1                               ## L:386  |    133 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 8, 8, $k1                                ## L:387  |    134 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:616  |    135 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  nor $t7, $s0, $s1                                  ## L:366  |    136 | result = t0 ~| t1;
  ldv $v04, 8, 40, $k1                               ## L:388  |    137 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadn $v21, $v21, $v23.v                           ## L:594  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  ssv $v25, 14, 20, $t2                              ## L:587  |    138 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v20, $v20, $v23.v                           ## L:594  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  vmudn $v29, $v02, $v15.h0                          ## L:606  |    139 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v23, 0, 0, $at                                ## L:616  |      ^ | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  vmadh $v29, $v01, $v15.h0                          ## L:606  |    140 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v23, 8, 0, $at                                ## L:616  |      ^ | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:617  |    141 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  vmadn $v29, $v04, $v15.h1                          ## L:607  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  srl $fp, $sp, 4                                    ## L:378  |    142 | u16 codes2 = codes >> 4;
  ldv $v24, 0, 0, $at                                ## L:617  |    143 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:607  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v24, 8, 0, $at                                ## L:617  |    144 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  vmadn $v29, $v06, $v15.h2                          ## L:608  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ldv $v08, 8, 56, $k1                               ## L:392  |    145 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:618  |    146 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  andi $s0, $fp, 1799                                ## L:351  |    147 | t0 = codes & 0x707;
  ssv $v25, 6, 20, $s7                               ## L:586  |    148 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ldv $v25, 0, 0, $at                                ## L:618  |    149 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  vmadh $v29, $v05, $v15.h2                          ## L:608  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e2                          ## L:609  |    150 | vpos = vmvp3 +* 0x20;
  lbu $k1, %lo(LIGHTING_COUNT + 0)                   ## L:625  |      ^ | u8 light_count = load(LIGHTING_COUNT);
  ldv $v25, 8, 0, $at                                ## L:618  |    151 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  srl $s1, $s0, 5                                    ## L:352  |    152 | t1 = t0 >> 5;
  ori $sp, $zero, %lo(LIGHTING_LIGHTS)               ## L:627  |    153 | u16 light_ptr = LIGHTING_LIGHTS;
  ssv $v26, 6, 22, $s7                               ## L:586  |    154 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadh $v15, $v07, $v30.e2                          ## L:609  |      ^ | vpos = vmvp3 +* 0x20;
  ssv $v26, 14, 22, $t2                              ## L:587  |    155 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmudh $v29, $v25, $v30.e7                          ## L:620  |      ^ | VTEMP = vemissive:sint * 1;
  nor $k0, $s0, $s1                                  ## L:366  |    156 | result = t0 ~| t1;
  beq $k1, $zero, LIGHT_END                          ## L:628  |    157 | if (light_count == 0) goto LIGHT_END;
  vmacf $v18, $v18, $v24.v                           ## L:621  |   *159 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  ldv $v26, 0, 8, $sp                                ## L:633  |    160 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v26, 8, 8, $sp                                ## L:633  |    161 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v27, 0, 0, $sp                                ## L:634  |    162 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  addiu $fp, $sp, 14                                 ## L:631  |    163 | u16 amb_ptr = light_ptr + 14;
  lh $s0, 6($sp)                                     ## L:638  |    164 | s16 light_w = load(light_ptr, 6);
  ldv $v27, 8, 0, $sp                                ## L:634  |    165 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  vmulf $v26, $v26, $v23.v                           ## L:636  |      ^ | vlcol:sfract *= vdiffuse;
  ldv $v25, 0, 0, $fp                                ## L:632  |    166 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  beq $s0, $zero, LABEL_gl_pipeline_0002             ## L:639  |    167 | if (light_w != 0)
  ldv $v25, 8, 0, $fp                                ## L:632  |   *169 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  vsubc $v27, $v27, $v15.v                           ## L:653  |    170 | vlpos -= vpos;
  addiu $s1, $sp, 20                                 ## L:647  |      ^ | u16 att_ptr = light_ptr + 20;
  vmudh $v04, $v27, $v27.v                           ## L:654  | ***174 | vec32 vsqdist = vlpos * vlpos;
  vsar $v04, COP2_ACC_MD                             ## L:654  |    175 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 0, 0, $s1                                ## L:649  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v03, COP2_ACC_HI                             ## L:654  |    176 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v06, $v04, $v04.h1                          ## L:655  |  **179 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v05, $v03, $v03.h1                           ## L:655  |    180 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:656  |  **183 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:656  |    184 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:658  | ***188 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:658  |    189 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v01, 8, 0, $s1                                ## L:649  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vrsqh $v05.e0, $v03.e4                             ## L:659  |    190 | vtmp.X = invert_half_sqrt(vsqdist).X;
  addiu $s1, $s1, 6                                  ## L:650  |      ^ | att_ptr += 6;
  vrsql $v06.e4, $v04.e4                             ## L:659  |    191 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v02, 0, 0, $s1                                ## L:651  |      ^ | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  ldv $v02, 8, 0, $s1                                ## L:651  |    192 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vrsqh $v05.e4, $v00.e0                             ## L:659  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:661  |  **195 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:661  |    196 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:661  |    197 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:661  |    198 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v27, $v06.h0                          ## L:663  |    199 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v27, $v27, $v05.h0                          ## L:663  |    200 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:665  |    201 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:665  |    202 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:666  |    203 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:666  |    204 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:666  |    205 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:666  |    206 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:667  |    207 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:667  |    208 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:667  |    209 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:667  |    210 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vrcph $v01.e0, $v01.e0                             ## L:669  | ***214 | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:669  |    215 | vatten.x = invert_half(vatten).x;
  vrcph $v01.e0, $v01.e4                             ## L:670  |  **218 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:670  |    219 | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:670  |    220 | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v26, $v02.h0                          ## L:672  |  **223 | vlcol *= vatten.xxxxXXXX;
  vmadh $v26, $v26, $v01.h0                          ## L:672  |    224 | vlcol *= vatten.xxxxXXXX;
  LABEL_gl_pipeline_0002:
  addiu $k1, $k1, 65535                              ## L:696  |      ^ | light_count -= 1;
  vmulf $v27, $v27, $v16.v                           ## L:686  |    225 | vlpos *= vnorm:sfract;
  addiu $sp, $sp, 32                                 ## L:695  |      ^ | light_ptr += 32;
  vmudn $v29, $v25, $v24.v                           ## L:687  |    226 | VTEMP = vlambient * vambient;
  vmacf $v29, $v26, $v27.h0                          ## L:688  |  **229 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v26, $v27.h1                          ## L:689  |    230 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v26, $v26, $v27.h2                          ## L:690  |    231 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v26, $v26, $v00                               ## L:691  | ***235 | vlcol = max(vlcol, VZERO);
  bne $k1, $zero, LIGHT_LOOP                         ## L:697  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v26.v                            ## L:693  | ****240 | vrgba:sint += vlcol;
  LIGHT_END:
  ori $fp, $zero, %lo(MATRICES_MVP)                  ## L:713  |    241 | u16 addr = MATRICES_MVP;
  vmov $v18.e3, $v23.e3                              ## L:701  |      ^ | vrgba.w = vdiffuse.w;
  vmov $v18.e7, $v23.e7                              ## L:702  |    242 | vrgba.W = vdiffuse.W;
  ctc2 $t0, $vcc                                     ## L:707  |      ^ | set_vcc(fog_mask);
  or $t3, $zero, $s7                                 ## L:769  |    243 | prev_out0 = vtx_out0_ptr;
  addu $s4, $s4, $t9                                 ## L:771  |    244 | vtx_in0_ptr += vtx_size2;
  ldv $v02, 8, 32, $fp                               ## L:386  |   *246 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmrg $v18, $v18, $v22.h2                           ## L:708  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v23, $v15, $v15.v                           ## L:717  |    247 | vec32 vsqdist = vpos * vpos;
  LOAD_COLOR780: llv $v19, 0, 0, $s4                 ## L:780  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vsar $v23, COP2_ACC_MD                             ## L:717  |    248 | vec32 vsqdist = vpos * vpos;
  or $t4, $zero, $t2                                 ## L:770  |      ^ | prev_out1 = vtx_out1_ptr;
  ldv $v02, 0, 32, $fp                               ## L:386  |    249 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vsar $v22, COP2_ACC_HI                             ## L:717  |      ^ | vec32 vsqdist = vpos * vpos;
  vaddc $v25, $v23, $v23.h1                          ## L:718  |  **252 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v03, 8, 8, $fp                                ## L:387  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vadd $v24, $v22, $v22.h1                           ## L:718  |    253 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v04, 0, 40, $fp                               ## L:388  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v03, 0, 8, $fp                                ## L:387  |    254 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v04, 8, 40, $fp                               ## L:388  |   *256 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vaddc $v23, $v25, $v23.h2                          ## L:719  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v22, $v24, $v22.h2                           ## L:719  |    257 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v24.e0, $v22.e0                             ## L:721  | ***261 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v25.e0, $v23.e0                             ## L:721  |    262 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v24.e0, $v22.e4                             ## L:722  |    263 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v25.e4, $v23.e4                             ## L:722  |    264 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v07, 0, 24, $fp                               ## L:391  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsqh $v24.e4, $v00.e0                             ## L:722  |    265 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v05, 0, 16, $fp                               ## L:389  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  addiu $t2, $t2, 80                                 ## L:774  |  **268 | vtx_out1_ptr += 80;
  vmudm $v29, $v15, $v25.h0                          ## L:724  |      ^ | vpos *= vtmp.xxxxXXXX;
  vmadh $v15, $v15, $v24.h0                          ## L:724  |    269 | vpos *= vtmp.xxxxXXXX;
  vmulf $v24, $v15, $v16.v                           ## L:727  | ***273 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:728  | ***277 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:729  | ***281 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  ldv $v08, 0, 56, $fp                               ## L:392  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vor $v25, $v00, $v00                               ## L:734  |    282 | vec16 vzunit = VZERO;
  vmov $v25.e6, $v12.e3                              ## L:736  |    283 | vzunit.Z = vnormmask.w;
  vlt $v24, $v24, $v00                               ## L:731  |   *285 | vdot = min(vdot, VZERO);
  vmov $v25.e2, $v12.e3                              ## L:735  |    286 | vzunit.z = vnormmask.w;
  vsubc $v24, $v00, $v24.v                           ## L:732  |  **289 | vdot = VZERO - vdot;
  vmulf $v29, $v16, $v24.h0                          ## L:738  | ***293 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v16, $v24.h0                          ## L:739  |    294 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  LOAD_POS_NORM776: ldv $v16, 0, 0, $s4              ## L:776  |      ^ | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  vmadh $v29, $v15, $v30.e7                          ## L:740  |    295 | VTEMP = vpos:sint +* 1;
  vmadh $v29, $v25, $v30.e7                          ## L:741  |    296 | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:744  |    297 | vec32 vrefl = get_acc();
  vsar $v24, COP2_ACC_HI                             ## L:744  |    298 | vec32 vrefl = get_acc();
  vmudl $v29, $v25, $v25.v                           ## L:746  |  **301 | vsqdist = vrefl * vrefl;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:760  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  vmadm $v29, $v24, $v25.v                           ## L:746  |    302 | vsqdist = vrefl * vrefl;
  ldv $v07, 8, 24, $fp                               ## L:391  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v23, $v25, $v24.v                           ## L:746  |    303 | vsqdist = vrefl * vrefl;
  addiu $s7, $s7, 80                                 ## L:773  |      ^ | vtx_out0_ptr += 80;
  vmadh $v22, $v24, $v24.v                           ## L:746  |    304 | vsqdist = vrefl * vrefl;
  ldv $v06, 0, 48, $fp                               ## L:390  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vxor $v17, $v00, $v30.e0                           ## L:753  |    305 | vtex = 0x80;
  vaddc $v27, $v23, $v23.h1                          ## L:747  |   *307 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v26, $v22, $v22.h1                           ## L:747  |    308 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v08, 8, 56, $fp                               ## L:392  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vaddc $v23, $v27, $v23.h2                          ## L:748  |  **311 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  addu $t1, $t1, $t9                                 ## L:772  |      ^ | vtx_in1_ptr += vtx_size2;
  LOAD_POS_NORM777: ldv $v16, 8, 0, $t1              ## L:777  |    312 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  vadd $v22, $v26, $v22.h2                           ## L:748  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v05, 8, 16, $fp                               ## L:389  | ***316 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrsqh $v26.e0, $v22.e0                             ## L:750  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v01, 8, 0, $fp                                ## L:385  |    317 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrsql $v27.e0, $v23.e0                             ## L:750  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  LOAD_COLOR781: llv $v19, 4, 0, $t1                 ## L:781  |    318 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vrsqh $v26.e0, $v22.e4                             ## L:751  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v06, 8, 48, $fp                               ## L:390  |    319 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrsql $v27.e4, $v23.e4                             ## L:751  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v26.e4, $v00.e0                             ## L:751  |    320 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v22, 0, 0, $at                                ## L:760  |      ^ | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v22, 8, 0, $at                                ## L:761  |    321 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  vmudh $v29, $v17, $v30.e7                          ## L:754  |      ^ | VTEMP = vtex:sint * 1;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:762  |    322 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 0, 0, $at                                ## L:762  |    323 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  vmadl $v29, $v25, $v27.h0                          ## L:755  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  llv $v23, 8, 0, $at                                ## L:763  |    324 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  vmadm $v29, $v24, $v27.h0                          ## L:755  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v01, 0, 0, $fp                                ## L:385  |    325 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadn $v29, $v25, $v26.h0                          ## L:755  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_POS_NORM779: ldv $v15, 8, 0, $t1              ## L:779  |    326 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v17, $v24, $v26.h0                          ## L:755  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  LOAD_POS_NORM778: ldv $v15, 0, 0, $s4              ## L:778  |    327 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  sltu $at, $s4, $s6                                 ## L:783  |    328 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmudh $v29, $v23, $v30.e7                          ## L:766  |      ^ | VTEMP = vtexoffset:sint * 1;
  bne $at, $zero, VERTEX_LOOP                        ## L:783  |    329 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:767  |   *331 | vtex:sint = vtex:sint +* vtexscale;
  sdv $v20, 0, 0, $t3                                ## L:789  |    332 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t7, 7($t3)                                     ## L:794  |    333 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sb $t5, 6($t3)                                     ## L:792  |    334 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  suv $v18, 0, 8, $t3                                ## L:797  |    335 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  suv $v18, 4, 8, $t4                                ## L:798  |    336 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  slv $v17, 0, 12, $t3                               ## L:800  |    337 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sdv $v20, 8, 0, $t4                                ## L:790  |    338 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  slv $v17, 8, 12, $t4                               ## L:801  |    339 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $t6, 6($t4)                                     ## L:793  |    340 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  j RSPQ_Loop                                        ## L:802  |    341 | }
  sb $k0, 7($t4)                                     ## L:795  |   *343 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20

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