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
    MgVertexAttributeLoaders LOAD_POS_NORM482, LOAD_POS_NORM483, LOAD_POS_NORM484, LOAD_POS_NORM485, LOAD_POS_NORM777, LOAD_POS_NORM779, LOAD_POS_NORM778, LOAD_POS_NORM776
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR486, LOAD_COLOR487, LOAD_COLOR781, LOAD_COLOR780
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD514, LOAD_TEXCOORD517
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:405  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v01, 0, 0, $at                                ## L:405  |      2 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:404  |      3 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:406  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:430  |   ***8 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vmudh $v04, $v02, $v01.v                           ## L:408  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:409  |      9 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:408  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:422  |     10 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:408  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ssv $v03, 0, 8, $at                                ## L:410  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:410  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:409  |     15 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:420  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:416  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:418  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:423  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:425  |     20 | u32 misalignment = buffer_offset & 0x7;
  addu $t0, $t1, $t3                                 ## L:426  |     21 | dma_size = vertices_size + misalignment;
  addiu $t2, $zero, 12                               ## L:433  |     22 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addiu $t0, $t0, 7                                  ## L:427  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:428  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:431  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:433  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:433  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:447  |     29 | u16 mvpaddr = MATRICES_MVP;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:435  |     30 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v02, 0, 32, $t0                               ## L:386  |     31 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:452  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:452  |     33 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  LOAD_COLOR486: llv $v19, 0, 0, $s4                 ## L:486  |     34 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  sll $t9, $t8, 1                                    ## L:436  |     35 | vtx_size2 = vtx_size * 2;
  ldv $v11, 0, 16, $at                               ## L:454  |     36 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:452  |     37 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  LOAD_POS_NORM482: ldv $v16, 0, 0, $s4              ## L:482  |     38 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v10, 0, 8, $at                                ## L:453  |     39 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:390  |     40 | m2:ufract = load(address, 0x30).xyzwxyzw;
  addu $t2, $s4, $t8                                 ## L:467  |     41 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  LOAD_COLOR487: llv $v19, 4, 0, $t2                 ## L:487  |     42 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v11, 8, 16, $at                               ## L:454  |     43 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:453  |     44 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:387  |     45 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:456  |     46 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ori $t4, $zero, %lo(MG_CLIP_CACHE)                 ## L:470  |     47 | u16 prev_out0 = MG_CLIP_CACHE;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:439  |     48 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v08, 0, 56, $t0                               ## L:392  |     49 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:457  |     50 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:456  |     51 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:388  |     52 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:456  |     53 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:392  |     54 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:389  |     55 | m2:sint   = load(address, 0x10).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:438  |     56 | vtx_in_end = vtx_in0_ptr + vertices_size;
  LOAD_POS_NORM483: ldv $v16, 8, 0, $t2              ## L:483  |     57 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  ldv $v03, 8, 8, $t0                                ## L:387  |     58 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:385  |     59 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:457  |     60 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:389  |     61 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:464  |     62 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  lbu $t1, %lo(FOG_MASK + 0)                         ## L:465  |     63 | u8 fog_mask = load(FOG_MASK);
  ldv $v07, 0, 24, $t0                               ## L:391  |     64 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:385  |     65 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:390  |     66 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:386  |     67 | m0:ufract = load(address, 0x20).xyzwxyzw;
  LOAD_POS_NORM484: ldv $v15, 0, 0, $s4              ## L:484  |     68 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v14, 0, 0, $at                                ## L:464  |     69 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:391  |     70 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:388  |     71 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v14, 8, 0, $at                                ## L:464  |     72 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:460  |     73 | u8 has_points = load(LIGHTING_HAS_POINTS);
  ori $t5, $zero, %lo(MG_CLIP_CACHE)                 ## L:471  |     74 | u16 prev_out1 = MG_CLIP_CACHE;
  LOAD_POS_NORM485: ldv $v15, 8, 0, $t2              ## L:485  |     75 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  addiu $t3, $s7, 40                                 ## L:468  |     76 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h3                           ## L:521  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:509  |     77 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v20, 0, 0, $t4                                ## L:495  |     78 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v18, 0, 8, $t4                                ## L:503  |     79 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  suv $v18, 4, 8, $t5                                ## L:504  |     80 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmudn $v16, $v16, $v13.v                           ## L:522  |      ^ | vnorm *= vnormfactor;
  slv $v17, 8, 12, $t5                               ## L:507  |     81 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmudn $v29, $v02, $v15.h0                          ## L:525  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v20, 8, 0, $t5                                ## L:496  |     82 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmadh $v29, $v01, $v15.h0                          ## L:525  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v19, 0, 0, $at                                ## L:509  |     83 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadn $v29, $v06, $v15.h2                          ## L:526  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  slv $v17, 0, 12, $t4                               ## L:506  |     84 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmadh $v29, $v05, $v15.h2                          ## L:526  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v04, $v15.h1                          ## L:527  |     85 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  luv $v19, 0, 0, $at                                ## L:510  |      ^ | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:550  |     86 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:527  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v22, 0, 0, $at                                ## L:550  |     87 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v21, $v08, $v30.e2                          ## L:528  |      ^ | vpos_clip = vmvp3 +* 0x20;
  ldv $v22, 8, 0, $at                                ## L:550  |     88 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v20, $v07, $v30.e2                          ## L:528  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmulf $v29, $v09, $v16.h0                          ## L:530  |     89 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:573  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmacf $v29, $v10, $v16.h1                          ## L:531  |     90 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  LOAD_TEXCOORD514: llv $v17, 0, 0, $s4              ## L:514  |      ^ | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  sb $t6, 6($t4)                                     ## L:498  |     91 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vmacf $v16, $v11, $v16.h2                          ## L:532  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudn $v24, $v21, $v22.v                           ## L:551  |     92 | vec32 vguard = vpos_clip * vclip_factors;
  LOAD_TEXCOORD517: llv $v17, 8, 0, $t2              ## L:517  |      ^ | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  vmadh $v23, $v20, $v22.v                           ## L:551  |     93 | vec32 vguard = vpos_clip * vclip_factors;
  sb $k1, 7($t5)                                     ## L:501  |      ^ | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sdv $v20, 8, 24, $t3                               ## L:561  |    *95 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v21, 8, 32, $t3                               ## L:561  |     96 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v20, 0, 24, $s7                               ## L:560  |     97 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vch $v29, $v23, $v23.h3                            ## L:552  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v24, $v24.h3                            ## L:552  |     98 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $t7, 6($t5)                                     ## L:499  |      ^ | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  cfc2 $sp, $vcc                                     ## L:552  |     99 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:556  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sdv $v21, 0, 32, $s7                               ## L:560  |    100 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vcl $v29, $v21, $v21.h3                            ## L:556  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vaddc $v23, $v21, $v14.h1                          ## L:565  |    101 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  ldv $v24, 0, 8, $at                                ## L:574  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vadd $v22, $v20, $v14.h0                           ## L:566  |    102 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  srl $fp, $sp, 4                                    ## L:371  |      ^ | u16 codes2 = codes >> 4;
  andi $s0, $sp, 1799                                ## L:351  |    103 | t0 = codes & 0x707;
  srl $s1, $s0, 5                                    ## L:352  |    104 | t1 = t0 >> 5;
  or $t6, $s0, $s1                                   ## L:359  |    105 | result = t0 | t1;
  vmudn $v29, $v23, $v14.h2                          ## L:568  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v22, $v22, $v14.h2                          ## L:569  |    106 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:573  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:573  |    107 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  andi $s0, $fp, 1799                                ## L:351  |    108 | t0 = codes & 0x707;
  srl $s1, $s0, 5                                    ## L:352  |    109 | t1 = t0 >> 5;
  cfc2 $fp, $vcc                                     ## L:556  |    110 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v22, $v00, $v22.h2                            ## L:570  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  or $t7, $s0, $s1                                   ## L:359  |    111 | result = t0 | t1;
  vmudl $v21, $v21, $v23.h3                          ## L:576  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v24, 8, 8, $at                                ## L:574  |    112 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vmadm $v20, $v20, $v23.h3                          ## L:576  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  sb $k0, 7($t4)                                     ## L:500  |    113 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vmadn $v21, $v00, $v00                             ## L:576  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  andi $s1, $fp, 1799                                ## L:351  |    114 | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:352  |    115 | t1 = t0 >> 5;
  srl $s0, $fp, 4                                    ## L:378  |    116 | u16 codes2 = codes >> 4;
  vrcph $v25.e3, $v20.e3                             ## L:583  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcpl $v26.e3, $v21.e3                             ## L:583  |    117 | vinvw.w = invert_half(vpos_clip).w;
  nor $k0, $s1, $s2                                  ## L:366  |      ^ | result = t0 ~| t1;
  ssv $v20, 6, 16, $s7                               ## L:579  |    118 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcph $v25.e3, $v20.e7                             ## L:584  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcpl $v26.e7, $v21.e7                             ## L:584  |    119 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 14, 16, $t3                              ## L:580  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v21, 14, 18, $t3                              ## L:580  |    120 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v25.e7, $v00.e7                             ## L:584  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ssv $v21, 6, 18, $s7                               ## L:579  |    121 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  andi $s1, $s0, 1799                                ## L:351  |    122 | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:352  |    123 | t1 = t0 >> 5;
  vmudl $v29, $v21, $v26.h3                          ## L:589  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 14, 20, $t3                              ## L:587  |    124 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadm $v29, $v20, $v26.h3                          ## L:589  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadn $v21, $v21, $v25.h3                          ## L:589  |    125 | vpos_clip *= vinvw.wwwwWWWW;
  nor $k1, $s1, $s2                                  ## L:366  |      ^ | result = t0 ~| t1;
  ssv $v26, 14, 22, $t3                              ## L:587  |    126 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v20, $v20, $v25.h3                          ## L:589  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 6, 20, $s7                               ## L:586  |    127 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v26, 6, 22, $s7                               ## L:586  |    128 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmudh $v29, $v24, $v30.e7                          ## L:593  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v21, $v21, $v23.v                           ## L:594  |    129 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_gl_pipeline_0001             ## L:600  |      ^ | if (has_points != 0)
  vmadh $v20, $v20, $v23.v                           ## L:594  |   *131 | vpos_clip = vpos_clip +* vviewscale;
  ori $sp, $zero, %lo(MATRICES_MV)                   ## L:603  |    132 | u16 addr = MATRICES_MV;
  ldv $v06, 0, 48, $sp                               ## L:390  |    133 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v01, 0, 0, $sp                                ## L:385  |    134 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 0, 32, $sp                               ## L:386  |    135 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v02, 8, 32, $sp                               ## L:386  |    136 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $sp                                ## L:387  |    137 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v01, 8, 0, $sp                                ## L:385  |    138 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v04, 0, 40, $sp                               ## L:388  |    139 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v03, 8, 8, $sp                                ## L:387  |    140 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v05, 0, 16, $sp                               ## L:389  |    141 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmudn $v29, $v02, $v15.h0                          ## L:606  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v04, 8, 40, $sp                               ## L:388  |    142 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v06, 8, 48, $sp                               ## L:390  |    143 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v08, 0, 56, $sp                               ## L:392  |    144 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v07, 0, 24, $sp                               ## L:391  |    145 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v05, 8, 16, $sp                               ## L:389  |    146 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v29, $v01, $v15.h0                          ## L:606  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v15.h1                          ## L:607  |    147 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v08, 8, 56, $sp                               ## L:392  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:607  |    148 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v07, 8, 24, $sp                               ## L:391  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v29, $v06, $v15.h2                          ## L:608  |    149 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:608  |    150 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e2                          ## L:609  |    151 | vpos = vmvp3 +* 0x20;
  vmadh $v15, $v07, $v30.e2                          ## L:609  |    152 | vpos = vmvp3 +* 0x20;
  LABEL_gl_pipeline_0001:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:612  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v18, 0, 0, $at                                ## L:612  |    153 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v18, 8, 0, $at                                ## L:612  |    154 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:616  |    155 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:616  |    156 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:616  |    157 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:617  |    158 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v24, 0, 0, $at                                ## L:617  |    159 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v24, 8, 0, $at                                ## L:617  |    160 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:618  |    161 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v25, 0, 0, $at                                ## L:618  |    162 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v25, 8, 0, $at                                ## L:618  |    163 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  lbu $sp, %lo(LIGHTING_COUNT + 0)                   ## L:625  |    164 | u8 light_count = load(LIGHTING_COUNT);
  ori $fp, $zero, %lo(LIGHTING_LIGHTS)               ## L:627  |  **167 | u16 light_ptr = LIGHTING_LIGHTS;
  vmudh $v29, $v25, $v30.e7                          ## L:620  |      ^ | VTEMP = vemissive:sint * 1;
  beq $sp, $zero, LIGHT_END                          ## L:628  |    168 | if (light_count == 0) goto LIGHT_END;
  vmacf $v18, $v18, $v24.v                           ## L:621  |   *170 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  ldv $v26, 0, 8, $fp                                ## L:633  |    171 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  addiu $s0, $fp, 14                                 ## L:631  |    172 | u16 amb_ptr = light_ptr + 14;
  ldv $v27, 0, 0, $fp                                ## L:634  |    173 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v26, 8, 8, $fp                                ## L:633  |    174 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  lh $s1, 6($fp)                                     ## L:638  |    175 | s16 light_w = load(light_ptr, 6);
  ldv $v25, 0, 0, $s0                                ## L:632  |    176 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v27, 8, 0, $fp                                ## L:634  |    177 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v25, 8, 0, $s0                                ## L:632  |    178 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  beq $s1, $zero, LABEL_gl_pipeline_0003             ## L:639  |    179 | if (light_w != 0)
  vmulf $v26, $v26, $v23.v                           ## L:636  |   *181 | vlcol:sfract *= vdiffuse;
  vsubc $v27, $v27, $v15.v                           ## L:653  |    182 | vlpos -= vpos;
  addiu $s2, $fp, 20                                 ## L:647  |      ^ | u16 att_ptr = light_ptr + 20;
  vmudh $v04, $v27, $v27.v                           ## L:654  | ***186 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 0, 0, $s2                                ## L:649  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  ldv $v01, 8, 0, $s2                                ## L:649  |    187 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v04, COP2_ACC_MD                             ## L:654  |      ^ | vec32 vsqdist = vlpos * vlpos;
  addiu $s2, $s2, 6                                  ## L:650  |    188 | att_ptr += 6;
  vsar $v03, COP2_ACC_HI                             ## L:654  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vaddc $v06, $v04, $v04.h1                          ## L:655  |  **191 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v05, $v03, $v03.h1                           ## L:655  |    192 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:656  |  **195 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:656  |    196 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:658  | ***200 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:658  |    201 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:659  |    202 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v02, 0, 0, $s2                                ## L:651  |      ^ | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  ldv $v02, 8, 0, $s2                                ## L:651  |    203 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vrsql $v06.e4, $v04.e4                             ## L:659  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:659  |    204 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:661  |  **207 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:661  |    208 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:661  |    209 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:661  |    210 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v27, $v06.h0                          ## L:663  |    211 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v27, $v27, $v05.h0                          ## L:663  |    212 | vlpos *= vtmp.xxxxXXXX;
  ori $s2, $zero, %lo(MATRICES_MVP)                  ## L:681  |      ^ | u16 addr = MATRICES_MVP;
  ldv $v06, 8, 48, $s2                               ## L:390  |    213 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudn $v29, $v02, $v30.e7                          ## L:665  |      ^ | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:665  |    214 | VTEMP = vatten * 1;
  ldv $v06, 0, 48, $s2                               ## L:390  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadl $v29, $v08, $v02.h1                          ## L:666  |    215 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:666  |    216 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:666  |    217 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:666  |    218 | VTEMP = vdist +* vatten.yyyyYYYY;
  ldv $v07, 0, 24, $s2                               ## L:391  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v07, 8, 24, $s2                               ## L:391  |    219 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadl $v29, $v04, $v02.h2                          ## L:667  |      ^ | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v05, 8, 16, $s2                               ## L:389  |    220 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadm $v29, $v03, $v02.h2                          ## L:667  |      ^ | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:667  |    221 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v04, 8, 40, $s2                               ## L:388  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadh $v01, $v03, $v01.h2                          ## L:667  |    222 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v05, 0, 16, $s2                               ## L:389  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrcph $v01.e0, $v01.e0                             ## L:669  | ***226 | vatten.x = invert_half(vatten).x;
  ldv $v08, 8, 56, $s2                               ## L:392  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcpl $v02.e0, $v02.e0                             ## L:669  |    227 | vatten.x = invert_half(vatten).x;
  ldv $v03, 0, 8, $s2                                ## L:387  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v03, 8, 8, $s2                                ## L:387  |  **230 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrcph $v01.e0, $v01.e4                             ## L:670  |      ^ | vatten.X = invert_half(vatten).X;
  ldv $v08, 0, 56, $s2                               ## L:392  |    231 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcpl $v02.e4, $v02.e4                             ## L:670  |      ^ | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:670  |    232 | vatten.X = invert_half(vatten).X;
  ldv $v04, 0, 40, $s2                               ## L:388  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmudm $v29, $v26, $v02.h0                          ## L:672  |  **235 | vlcol *= vatten.xxxxXXXX;
  ldv $v02, 0, 32, $s2                               ## L:386  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v02, 8, 32, $s2                               ## L:386  |    236 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v26, $v26, $v01.h0                          ## L:672  |      ^ | vlcol *= vatten.xxxxXXXX;
  ldv $v01, 8, 0, $s2                                ## L:385  |    237 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 0, 0, $s2                                ## L:385  |    238 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LABEL_gl_pipeline_0003:
  vmulf $v27, $v27, $v16.v                           ## L:686  |      ^ | vlpos *= vnorm:sfract;
  vmudn $v29, $v25, $v24.v                           ## L:687  |    239 | VTEMP = vlambient * vambient;
  vmacf $v29, $v26, $v27.h0                          ## L:688  |  **242 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v26, $v27.h1                          ## L:689  |    243 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v26, $v26, $v27.h2                          ## L:690  |    244 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $sp, $sp, 65535                              ## L:696  |      ^ | light_count -= 1;
  addiu $fp, $fp, 32                                 ## L:695  | ***248 | light_ptr += 32;
  vge $v26, $v26, $v00                               ## L:691  |      ^ | vlcol = max(vlcol, VZERO);
  bne $sp, $zero, LIGHT_LOOP                         ## L:697  |    249 | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v26.v                            ## L:693  | ***253 | vrgba:sint += vlcol;
  LIGHT_END:
  vmov $v18.e3, $v23.e3                              ## L:701  |    254 | vrgba.w = vdiffuse.w;
  or $t5, $zero, $t3                                 ## L:770  |      ^ | prev_out1 = vtx_out1_ptr;
  vmov $v18.e7, $v23.e7                              ## L:702  |    255 | vrgba.W = vdiffuse.W;
  addu $t2, $t2, $t9                                 ## L:772  |      ^ | vtx_in1_ptr += vtx_size2;
  LOAD_COLOR781: llv $v19, 4, 0, $t2                 ## L:781  |    256 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ctc2 $t1, $vcc                                     ## L:707  |    257 | set_vcc(fog_mask);
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:760  |    258 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  addiu $t3, $t3, 80                                 ## L:774  |    259 | vtx_out1_ptr += 80;
  vmrg $v18, $v18, $v22.h2                           ## L:708  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  llv $v22, 0, 0, $at                                ## L:760  |    260 | vtexscale.xy = load(TEXTURING_SCALE).xy;
  llv $v22, 8, 0, $at                                ## L:761  |    261 | vtexscale.XY = load(TEXTURING_SCALE).xy;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:762  |    262 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 0, 0, $at                                ## L:762  |    263 | vtexoffset.xy = load(TEXTURING_OFFSET).xy;
  llv $v23, 8, 0, $at                                ## L:763  |    264 | vtexoffset.XY = load(TEXTURING_OFFSET).xy;
  addu $s4, $s4, $t9                                 ## L:771  |    265 | vtx_in0_ptr += vtx_size2;
  LOAD_POS_NORM777: ldv $v16, 8, 0, $t2              ## L:777  |    266 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  or $t4, $zero, $s7                                 ## L:769  |    267 | prev_out0 = vtx_out0_ptr;
  LOAD_COLOR780: llv $v19, 0, 0, $s4                 ## L:780  |    268 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_POS_NORM779: ldv $v15, 8, 0, $t2              ## L:779  |    269 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_POS_NORM778: ldv $v15, 0, 0, $s4              ## L:778  |    270 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vmudh $v29, $v23, $v30.e7                          ## L:766  |      ^ | VTEMP = vtexoffset:sint * 1;
  vmadh $v17, $v17, $v22.v                           ## L:767  |    271 | vtex:sint = vtex:sint +* vtexscale;
  sltu $at, $s4, $s6                                 ## L:783  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  LOAD_POS_NORM776: ldv $v16, 0, 0, $s4              ## L:776  |    272 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  bne $at, $zero, VERTEX_LOOP                        ## L:783  |    273 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  addiu $s7, $s7, 80                                 ## L:773  |   *275 | vtx_out0_ptr += 80;
  sdv $v20, 0, 0, $t4                                ## L:789  |    276 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v18, 0, 8, $t4                                ## L:797  |    277 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v20, 8, 0, $t5                                ## L:790  |    278 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sb $k1, 7($t5)                                     ## L:795  |    279 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  slv $v17, 0, 12, $t4                               ## L:800  |    280 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t6, 6($t4)                                     ## L:792  |    281 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t7, 6($t5)                                     ## L:793  |    282 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  suv $v18, 4, 8, $t5                                ## L:798  |    283 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sb $k0, 7($t4)                                     ## L:794  |    284 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  j RSPQ_Loop                                        ## L:802  |    285 | }
  slv $v17, 8, 12, $t5                               ## L:801  |   *287 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80

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