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
#define TEX_SIZE_SHIFT 6
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
#define GLP_MATERIAL_FLAG_DIFFUSE 0x1
#define GLP_MATERIAL_FLAG_AMBIENT 0x2
#define GLP_MATERIAL_FLAG_EMISSIVE 0x4
#define SCRATCH_MAT_DIFFUSE 0x30
#define SCRATCH_MAT_AMBIENT 0x40
#define SCRATCH_MAT_EMISSIVE 0x50
#define SCRATCH_RGBA_IN 0x60
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
    .align 4
    TEXTURE_MATRIX: .ds.b 48
    .align 1
    TEXTURE_SIZE: .ds.b 4
    .align 1
    TEXTURE_OFFSET: .ds.b 4
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
    MgVertexAttributeLoaders LOAD_POS_NORM516, LOAD_POS_NORM517, LOAD_POS_NORM518, LOAD_POS_NORM519, LOAD_POS_NORM840, LOAD_POS_NORM843, LOAD_POS_NORM841, LOAD_POS_NORM842
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR520, LOAD_COLOR521, LOAD_COLOR844, LOAD_COLOR845
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:414  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:415  |      2 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:416  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:415  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:440  |   ***8 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vmudh $v04, $v02, $v01.v                           ## L:418  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:432  |      9 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v03, COP2_ACC_HI                             ## L:418  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:419  |     10 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v04, COP2_ACC_MD                             ## L:418  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ssv $v03, 0, 8, $at                                ## L:420  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:420  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:430  |     15 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:419  |     16 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:426  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:428  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:435  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:433  |     20 | vtx_buffer += buffer_offset;
  addiu $t2, $zero, 12                               ## L:443  |     21 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addu $t0, $t1, $t3                                 ## L:436  |     22 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:437  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:438  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:441  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:443  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:443  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:457  |     29 | u16 mvpaddr = MATRICES_MVP;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:462  |     30 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $t2, $zero, %lo(MG_SCRATCH_MEM)                ## L:476  |     31 | u16 prev_out0 = MG_SCRATCH_MEM;
  ldv $v02, 0, 32, $t0                               ## L:396  |     32 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:397  |     33 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:398  |     34 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:462  |     35 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:395  |     36 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:464  |     37 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:402  |     38 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:395  |     39 | m0:sint   = load(address, 0x00).xyzwxyzw;
  addiu $k0, $zero, 48                               ## L:488  |     40 | u16 mat_ptr_diffuse  = 0x30;
  ldv $v10, 0, 8, $at                                ## L:463  |     41 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:398  |     42 | m1:ufract = load(address, 0x28).xyzwxyzw;
  lhu $fp, %lo(MATERIAL_FLAGS + 0)                   ## L:501  |     43 | u16 mat_flags = load(MATERIAL_FLAGS);
  ldv $v10, 8, 8, $at                                ## L:463  |     44 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:477  |     45 | u16 prev_out1 = MG_SCRATCH_MEM;
  ldv $v03, 8, 8, $t0                                ## L:397  |     46 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:464  |     47 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:462  |     48 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:466  |     49 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:445  |     50 | vtx_size = load(MG_VERTEX_SIZE);
  addu $s6, $s4, $t1                                 ## L:448  |     51 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ldv $v12, 0, 0, $at                                ## L:466  |     52 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:467  |     53 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:467  |     54 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:401  |     55 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:402  |     56 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:449  |     57 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v12, 8, 0, $at                                ## L:466  |     58 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:396  |     59 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:493  |     60 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v21, 0, 0, $at                                ## L:493  |     61 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v21, 8, 0, $at                                ## L:493  |     62 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  andi $s0, $fp, 1                                   ## L:502  |     63 | u16 tmp = mat_flags & 0x1;
  ldv $v07, 8, 24, $t0                               ## L:401  |     64 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:494  |     65 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:494  |     66 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:400  |     67 | m2:ufract = load(address, 0x30).xyzwxyzw;
  addiu $k1, $zero, 64                               ## L:489  |     68 | u16 mat_ptr_ambient  = 0x40;
  ldv $v06, 8, 48, $t0                               ## L:400  |     69 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:494  |     70 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:495  |     71 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:399  |     72 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:399  |     73 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:495  |     74 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:495  |     75 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:497  |     76 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  sll $t9, $t8, 1                                    ## L:446  |     77 | vtx_size2 = vtx_size * 2;
  addiu $sp, $zero, 80                               ## L:490  |     78 | u16 mat_ptr_emissive = 0x50;
  addiu $t1, $s7, 40                                 ## L:474  |     79 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  sqv $v22, 0, 64, $at                               ## L:498  |     80 | store(vambient, MG_SCRATCH_MEM, 0x40);
  sqv $v21, 0, 48, $at                               ## L:497  |     81 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  addu $t0, $s4, $t8                                 ## L:473  |     82 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  beq $s0, $zero, LABEL_gl_pipeline_0001             ## L:503  |     83 | if (tmp != 0) {
  sqv $v23, 0, 80, $at                               ## L:499  |    *85 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  addiu $k0, $zero, 96                               ## L:504  |     86 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s0, $fp, 2                                   ## L:506  |     87 | tmp = mat_flags & 0x2;
  beq $s0, $zero, LABEL_gl_pipeline_0002             ## L:507  |     88 | if (tmp != 0 ) {
  nop                                                ## L:507  |    *90 | if (tmp != 0 ) {
  addiu $k1, $zero, 96                               ## L:508  |     91 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s0, $fp, 4                                   ## L:510  |     92 | tmp = mat_flags & 0x4;
  beq $s0, $zero, LABEL_gl_pipeline_0003             ## L:511  |     93 | if (tmp != 0) {
  nop                                                ## L:511  |    *95 | if (tmp != 0) {
  addiu $sp, $zero, 96                               ## L:512  |     96 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_COLOR520: llv $v18, 0, 0, $s4                 ## L:520  |     97 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_POS_NORM516: ldv $v15, 0, 0, $s4              ## L:516  |     98 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR521: llv $v18, 4, 0, $t0                 ## L:521  |     99 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  LOAD_POS_NORM517: ldv $v15, 8, 0, $t0              ## L:517  |    100 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_POS_NORM518: ldv $v14, 0, 0, $s4              ## L:518  |    101 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM519: ldv $v14, 8, 0, $t0              ## L:519  |    102 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:543  |   *104 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vand $v15, $v12, $v15.h3                           ## L:556  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  sdv $v18, 0, 0, $at                                ## L:543  |    105 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  luv $v18, 0, 0, $at                                ## L:544  |    106 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  suv $v17, 0, 8, $t2                                ## L:537  |    107 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:545  |    108 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmudn $v15, $v15, $v13.v                           ## L:557  |      ^ | vnorm *= vnormfactor;
  vmudn $v29, $v02, $v14.h0                          ## L:560  |    109 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v19, 0, 0, $t2                                ## L:529  |      ^ | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmadh $v29, $v01, $v14.h0                          ## L:560  |    110 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sqv $v18, 0, 96, $at                               ## L:545  |      ^ | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  suv $v17, 4, 8, $t3                                ## L:538  |    111 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadn $v29, $v04, $v14.h1                          ## L:561  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadh $v29, $v03, $v14.h1                          ## L:561  |    112 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:585  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v29, $v06, $v14.h2                          ## L:562  |    113 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ldv $v21, 0, 0, $at                                ## L:585  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v05, $v14.h2                          ## L:562  |    114 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  slv $v16, 8, 12, $t3                               ## L:541  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  ldv $v21, 8, 0, $at                                ## L:585  |    115 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v20, $v08, $v30.e2                          ## L:563  |      ^ | vpos_clip = vmvp3 +* 0x20;
  sdv $v19, 8, 0, $t3                                ## L:530  |    116 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmadh $v19, $v07, $v30.e2                          ## L:563  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmulf $v29, $v09, $v15.h0                          ## L:565  |    117 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:600  |      ^ | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  sb $t4, 6($t2)                                     ## L:532  |    118 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vmacf $v29, $v10, $v15.h1                          ## L:566  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  slv $v16, 0, 12, $t2                               ## L:540  |    119 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmacf $v15, $v11, $v15.h2                          ## L:567  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudn $v23, $v20, $v21.v                           ## L:586  |    120 | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v19, 8, 24, $t1                               ## L:596  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vmadh $v22, $v19, $v21.v                           ## L:586  |    121 | vec32 vguard = vpos_clip * vclip_factors;
  sb $t5, 6($t3)                                     ## L:533  |      ^ | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $t7, 7($t3)                                     ## L:535  |    122 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sdv $v19, 0, 24, $s7                               ## L:595  |    123 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v20, 0, 32, $s7                               ## L:595  |    124 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sb $t6, 7($t2)                                     ## L:534  |    125 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vch $v29, $v22, $v22.h3                            ## L:587  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vcl $v29, $v23, $v23.h3                            ## L:587  |    126 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v20, 8, 32, $t1                               ## L:596  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  cfc2 $fp, $vcc                                     ## L:587  |    127 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  ldv $v22, 0, 0, $at                                ## L:600  |    128 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  vch $v29, $v19, $v19.h3                            ## L:591  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v22, 8, 0, $at                                ## L:600  |    129 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  andi $s1, $fp, 1799                                ## L:361  |    130 | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:362  |    131 | t1 = t0 >> 5;
  or $t4, $s1, $s2                                   ## L:369  |    132 | result = t0 | t1;
  vcl $v29, $v20, $v20.h3                            ## L:591  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vaddc $v23, $v20, $v22.h1                          ## L:601  |    133 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  srl $s0, $fp, 4                                    ## L:381  |      ^ | u16 codes2 = codes >> 4;
  vadd $v21, $v19, $v22.h0                           ## L:602  |    134 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s1, $s0, 1799                                ## L:361  |      ^ | t0 = codes & 0x707;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:639  |    135 | u16 addr = MATRICES_MV;
  ldv $v03, 0, 8, $fp                                ## L:397  |    136 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v07, 0, 24, $fp                               ## L:401  |    137 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:609  |    138 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v07, 8, 24, $fp                               ## L:401  |    139 | m3:sint   = load(address, 0x18).xyzwxyzw;
  cfc2 $s0, $vcc                                     ## L:591  |    140 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vmudn $v29, $v23, $v22.h2                          ## L:604  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v21, $v21, $v22.h2                          ## L:605  |    141 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  srl $s2, $s1, 5                                    ## L:362  |      ^ | t1 = t0 >> 5;
  ldv $v23, 0, 8, $at                                ## L:610  |    142 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:609  |    143 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:609  |    144 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v23, 8, 8, $at                                ## L:610  |    145 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ldv $v03, 8, 8, $fp                                ## L:397  |    146 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v05, 0, 16, $fp                               ## L:399  |    147 | m2:sint   = load(address, 0x10).xyzwxyzw;
  or $t5, $s1, $s2                                   ## L:369  |    148 | result = t0 | t1;
  vge $v21, $v00, $v21.h2                            ## L:606  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  andi $s2, $s0, 1799                                ## L:361  |    149 | t0 = codes & 0x707;
  vmudl $v20, $v20, $v22.h3                          ## L:612  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v19, $v19, $v22.h3                          ## L:612  |    150 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v08, 0, 56, $fp                               ## L:402  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:400  |    151 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadn $v20, $v00, $v00                             ## L:612  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v02, 0, 32, $fp                               ## L:396  |    152 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 0, 40, $fp                               ## L:398  |    153 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v06, 8, 48, $fp                               ## L:400  |    154 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v04, 8, 40, $fp                               ## L:398  |    155 | m1:ufract = load(address, 0x28).xyzwxyzw;
  srl $s1, $s0, 4                                    ## L:388  |    156 | u16 codes2 = codes >> 4;
  srl $s3, $s2, 5                                    ## L:362  |    157 | t1 = t0 >> 5;
  ldv $v02, 8, 32, $fp                               ## L:396  |    158 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v05, 8, 16, $fp                               ## L:399  |    159 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:648  |    160 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vrcph $v24.e3, $v19.e3                             ## L:619  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ldv $v17, 0, 0, $at                                ## L:648  |    161 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vrcpl $v25.e3, $v20.e3                             ## L:619  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcph $v24.e3, $v19.e7                             ## L:620  |    162 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v19, 14, 16, $t1                              ## L:616  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcpl $v25.e7, $v20.e7                             ## L:620  |    163 | vinvw.W = invert_half(vpos_clip).W;
  ldv $v17, 8, 0, $at                                ## L:648  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ssv $v19, 6, 16, $s7                               ## L:615  |    164 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcph $v24.e7, $v00.e7                             ## L:620  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 6, 18, $s7                               ## L:615  |   *166 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ssv $v20, 14, 18, $t1                              ## L:616  |    167 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  nor $t6, $s2, $s3                                  ## L:376  |    168 | result = t0 ~| t1;
  vmudl $v29, $v20, $v25.h3                          ## L:625  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v19, $v25.h3                          ## L:625  |    169 | vpos_clip *= vinvw.wwwwWWWW;
  andi $s2, $s1, 1799                                ## L:361  |      ^ | t0 = codes & 0x707;
  ldv $v01, 0, 0, $fp                                ## L:395  |    170 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ssv $v24, 14, 20, $t1                              ## L:623  |    171 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v20, $v20, $v24.h3                          ## L:625  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v01, 8, 0, $fp                                ## L:395  |    172 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadh $v19, $v19, $v24.h3                          ## L:625  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmudh $v29, $v23, $v30.e7                          ## L:629  |    173 | VTEMP = vviewoff:sint * 1;
  addiu $at, $k0, MG_SCRATCH_MEM                     ## L:650  |      ^ | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:661  |    174 | u16 light_ptr = LIGHTING_LIGHTS;
  ssv $v24, 6, 20, $s7                               ## L:622  |    175 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadn $v20, $v20, $v22.v                           ## L:630  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  srl $s3, $s2, 5                                    ## L:362  |    176 | t1 = t0 >> 5;
  vmadh $v19, $v19, $v22.v                           ## L:630  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  vmudn $v29, $v02, $v14.h0                          ## L:642  |    177 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v08, 8, 56, $fp                               ## L:402  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  lqv $v22, 0, 0, $at                                ## L:650  |    178 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  vmadh $v29, $v01, $v14.h0                          ## L:642  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:651  |    179 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  vmadn $v29, $v04, $v14.h1                          ## L:643  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  lbu $fp, %lo(LIGHTING_COUNT + 0)                   ## L:659  |    180 | u8 light_count = load(LIGHTING_COUNT);
  vmadh $v29, $v03, $v14.h1                          ## L:643  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v29, $v06, $v14.h2                          ## L:644  |    181 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  lqv $v23, 0, 0, $at                                ## L:651  |      ^ | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:652  |    182 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lqv $v24, 0, 0, $at                                ## L:652  |    183 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  ssv $v25, 14, 22, $t1                              ## L:623  |    184 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v29, $v05, $v14.h2                          ## L:644  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  nor $t7, $s2, $s3                                  ## L:376  |    185 | result = t0 ~| t1;
  vmadn $v29, $v08, $v30.e2                          ## L:645  |      ^ | vpos = vmvp3 +* 0x20;
  ssv $v25, 6, 22, $s7                               ## L:622  |    186 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadh $v14, $v07, $v30.e2                          ## L:645  |      ^ | vpos = vmvp3 +* 0x20;
  vmudh $v29, $v24, $v30.e7                          ## L:654  |    187 | VTEMP = vemissive:sint * 1;
  beq $fp, $zero, LIGHT_END                          ## L:662  |      ^ | if (light_count == 0) goto LIGHT_END;
  vmacf $v17, $v17, $v23.v                           ## L:655  |   *189 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  addiu $s1, $s0, 14                                 ## L:665  |    190 | u16 amb_ptr = light_ptr + 14;
  ldv $v25, 0, 8, $s0                                ## L:667  |    191 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v25, 8, 8, $s0                                ## L:667  |    192 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v24, 0, 0, $s1                                ## L:666  |    193 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $s1                                ## L:666  |    194 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  lh $s2, 6($s0)                                     ## L:672  |    195 | s16 light_w = load(light_ptr, 6);
  ldv $v26, 0, 0, $s0                                ## L:668  |    196 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  vmulf $v25, $v25, $v22.v                           ## L:670  |      ^ | vlcol:sfract *= vdiffuse;
  ldv $v26, 8, 0, $s0                                ## L:668  |    197 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $s2, $zero, LABEL_gl_pipeline_0005             ## L:673  |    198 | if (light_w != 0)
  nop                                                ## L:779  |   *200 | VTEMP = vzunit:sint +* 1;
  addiu $s3, $s0, 20                                 ## L:681  |    201 | u16 att_ptr = light_ptr + 20;
  vsubc $v26, $v26, $v14.v                           ## L:687  |      ^ | vlpos -= vpos;
  vmudh $v04, $v26, $v26.v                           ## L:688  | ***205 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 0, 0, $s3                                ## L:683  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  ldv $v01, 8, 0, $s3                                ## L:683  |    206 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v04, COP2_ACC_MD                             ## L:688  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vsar $v03, COP2_ACC_HI                             ## L:688  |    207 | vec32 vsqdist = vlpos * vlpos;
  addiu $s3, $s3, 6                                  ## L:684  |      ^ | att_ptr += 6;
  ldv $v02, 0, 0, $s3                                ## L:685  |    208 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  ldv $v02, 8, 0, $s3                                ## L:685  |   *210 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vaddc $v06, $v04, $v04.h1                          ## L:689  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v05, $v03, $v03.h1                           ## L:689  |    211 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:690  |  **214 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:690  |    215 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:692  | ***219 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:692  |    220 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:693  |    221 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:693  |    222 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:693  |    223 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:695  |  **226 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:695  |    227 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:695  |    228 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:695  |    229 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v26, $v06.h0                          ## L:697  |    230 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v26, $v26, $v05.h0                          ## L:697  |    231 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:699  |    232 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:699  |    233 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:700  |    234 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:700  |    235 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:700  |    236 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:700  |    237 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:701  |    238 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:701  |    239 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:701  |    240 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:701  |    241 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vrcph $v01.e0, $v01.e0                             ## L:703  | ***245 | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:703  |    246 | vatten.x = invert_half(vatten).x;
  vrcph $v01.e0, $v01.e4                             ## L:704  |  **249 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:704  |    250 | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:704  |    251 | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v25, $v02.h0                          ## L:706  |  **254 | vlcol *= vatten.xxxxXXXX;
  vmadh $v25, $v25, $v01.h0                          ## L:706  |    255 | vlcol *= vatten.xxxxXXXX;
  LABEL_gl_pipeline_0005:
  addiu $fp, $fp, 65535                              ## L:730  |      ^ | light_count -= 1;
  addiu $s0, $s0, 32                                 ## L:729  |    256 | light_ptr += 32;
  vmulf $v26, $v26, $v15.v                           ## L:720  |      ^ | vlpos *= vnorm:sfract;
  vmudn $v29, $v24, $v23.v                           ## L:721  |    257 | VTEMP = vlambient * vambient;
  vmacf $v29, $v25, $v26.h0                          ## L:722  |  **260 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v25, $v26.h1                          ## L:723  |    261 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v25, $v25, $v26.h2                          ## L:724  |    262 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v25, $v25, $v00                               ## L:725  | ***266 | vlcol = max(vlcol, VZERO);
  bne $fp, $zero, LIGHT_LOOP                         ## L:731  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v17, $v17, $v25.v                            ## L:727  | ****271 | vrgba:sint += vlcol;
  LIGHT_END:
  vmov $v17.e3, $v22.e3                              ## L:735  |    272 | vrgba.w = vdiffuse.w;
  ori $at, $zero, %lo(TEXTURE_MATRIX)                ## L:804  |      ^ | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  lbu $s1, %lo(FOG_MASK + 0)                         ## L:742  |    273 | u8 fog_mask = load(FOG_MASK);
  vmov $v17.e7, $v22.e7                              ## L:736  |      ^ | vrgba.W = vdiffuse.W;
  ctc2 $s1, $vcc                                     ## L:743  |  **276 | set_vcc(fog_mask);
  ldv $v28, 0, 40, $at                               ## L:809  |    277 | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  vmrg $v17, $v17, $v21.h2                           ## L:744  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v22, $v14, $v14.v                           ## L:755  |    278 | vec32 vsqdist = vpos * vpos;
  vsar $v22, COP2_ACC_MD                             ## L:755  |    279 | vec32 vsqdist = vpos * vpos;
  vsar $v21, COP2_ACC_HI                             ## L:755  |    280 | vec32 vsqdist = vpos * vpos;
  or $t3, $zero, $t1                                 ## L:834  |      ^ | prev_out1 = vtx_out1_ptr;
  vaddc $v24, $v22, $v22.h1                          ## L:756  |  **283 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:751  |      ^ | u16 addr = MATRICES_MVP;
  addu $t0, $t0, $t9                                 ## L:836  |    284 | vtx_in1_ptr += vtx_size2;
  vadd $v23, $v21, $v21.h1                           ## L:756  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v22, $v24, $v22.h2                          ## L:757  |  **287 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v04, 0, 40, $s1                               ## L:398  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v08, 0, 56, $s1                               ## L:402  |    288 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vadd $v21, $v23, $v21.h2                           ## L:757  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v05, 0, 16, $s1                               ## L:399  | ***292 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrsqh $v23.e0, $v21.e0                             ## L:759  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v08, 8, 56, $s1                               ## L:402  |    293 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrsql $v24.e0, $v22.e0                             ## L:759  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v05, 8, 16, $s1                               ## L:399  |    294 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrsqh $v23.e0, $v21.e4                             ## L:760  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v24.e4, $v22.e4                             ## L:760  |    295 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v23.e4, $v00.e0                             ## L:760  |    296 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v04, 8, 40, $s1                               ## L:398  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmudm $v29, $v14, $v24.h0                          ## L:762  |  **299 | vpos *= vtmp.xxxxXXXX;
  ldv $v07, 8, 24, $s1                               ## L:401  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  addiu $t1, $t1, 80                                 ## L:838  |    300 | vtx_out1_ptr += 80;
  vmadh $v14, $v14, $v23.h0                          ## L:762  |      ^ | vpos *= vtmp.xxxxXXXX;
  vmulf $v23, $v14, $v15.v                           ## L:765  | ***304 | vec16 vdot = vpos:sfract * vnorm:sfract;
  ldv $v07, 0, 24, $s1                               ## L:401  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vadd $v24, $v23, $v23.h1                           ## L:766  | ***308 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  ldv $v02, 8, 32, $s1                               ## L:396  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vadd $v23, $v24, $v23.h2                           ## L:767  | ***312 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  vor $v24, $v00, $v00                               ## L:772  |    313 | vec16 vzunit = VZERO;
  vlt $v23, $v23, $v00                               ## L:769  |  **316 | vdot = min(vdot, VZERO);
  vmov $v24.e6, $v12.e3                              ## L:774  |    317 | vzunit.Z = vnormmask.w;
  vsubc $v23, $v00, $v23.v                           ## L:770  |  **320 | vdot = VZERO - vdot;
  vmov $v24.e2, $v12.e3                              ## L:773  |    321 | vzunit.z = vnormmask.w;
  vmulf $v29, $v15, $v23.h0                          ## L:776  |  **324 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v15, $v23.h0                          ## L:777  |    325 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  addu $s4, $s4, $t9                                 ## L:835  |      ^ | vtx_in0_ptr += vtx_size2;
  LOAD_POS_NORM840: ldv $v15, 0, 0, $s4              ## L:840  |    326 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  vmadh $v29, $v14, $v30.e7                          ## L:778  |      ^ | VTEMP = vpos:sint +* 1;
  vmadh $v29, $v24, $v30.e7                          ## L:779  |    327 | VTEMP = vzunit:sint +* 1;
  LOAD_COLOR844: llv $v18, 0, 0, $s4                 ## L:844  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vsar $v24, COP2_ACC_MD                             ## L:782  |    328 | vec32 vrefl = get_acc();
  vsar $v23, COP2_ACC_HI                             ## L:782  |    329 | vec32 vrefl = get_acc();
  vmudl $v29, $v24, $v24.v                           ## L:784  |  **332 | vsqdist = vrefl * vrefl;
  vmadm $v29, $v23, $v24.v                           ## L:784  |    333 | vsqdist = vrefl * vrefl;
  LOAD_POS_NORM843: ldv $v14, 8, 0, $t0              ## L:843  |      ^ | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadn $v22, $v24, $v23.v                           ## L:784  |    334 | vsqdist = vrefl * vrefl;
  vmadh $v21, $v23, $v23.v                           ## L:784  |    335 | vsqdist = vrefl * vrefl;
  vaddc $v26, $v22, $v22.h1                          ## L:785  |  **338 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v27, 0, 16, $at                               ## L:808  |      ^ | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  ldv $v27, 8, 16, $at                               ## L:808  |    339 | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  vadd $v25, $v21, $v21.h1                           ## L:785  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vxor $v16, $v00, $v30.e0                           ## L:791  |    340 | vtex = 0x80;
  vaddc $v22, $v26, $v22.h2                          ## L:786  |   *342 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v21, $v25, $v21.h2                           ## L:786  |    343 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v25.e0, $v21.e0                             ## L:788  | ***347 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v26.e0, $v22.e0                             ## L:788  |    348 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v25.e0, $v21.e4                             ## L:789  |    349 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v26.e4, $v22.e4                             ## L:789  |    350 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v03, 0, 8, $s1                                ## L:397  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrsqh $v25.e4, $v00.e0                             ## L:789  |    351 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudh $v29, $v16, $v30.e7                          ## L:792  |    352 | VTEMP = vtex:sint * 1;
  ldv $v03, 8, 8, $s1                                ## L:397  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmadl $v29, $v24, $v26.h0                          ## L:793  |   *354 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v28, 8, 40, $at                               ## L:809  |      ^ | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  vmadm $v29, $v23, $v26.h0                          ## L:793  |    355 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v26, 0, 32, $at                               ## L:807  |      ^ | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  vmadn $v29, $v24, $v25.h0                          ## L:793  |    356 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v24, 0, 24, $at                               ## L:805  |      ^ | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  ldv $v24, 8, 24, $at                               ## L:805  |    357 | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  ldv $v26, 8, 32, $at                               ## L:807  |    358 | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  vmadh $v16, $v23, $v25.h0                          ## L:793  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v23, 0, 0, $at                                ## L:804  |    359 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:804  |    360 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v25, 0, 8, $at                                ## L:806  |    361 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  ldv $v25, 8, 8, $at                                ## L:806  |    362 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  vmudn $v29, $v24, $v16.h0                          ## L:811  |      ^ | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadh $v29, $v23, $v16.h0                          ## L:811  |   *364 | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadn $v29, $v26, $v16.h1                          ## L:812  |    365 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  ori $at, $zero, %lo(TEXTURE_SIZE)                  ## L:825  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  vmadh $v29, $v25, $v16.h1                          ## L:812  |    366 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  or $t2, $zero, $s7                                 ## L:833  |      ^ | prev_out0 = vtx_out0_ptr;
  addiu $s7, $s7, 80                                 ## L:837  |    367 | vtx_out0_ptr += 80;
  vmadn $v22, $v28, $v31.e7                          ## L:813  |      ^ | vtex_transformed = vmt3 +* 0x100;
  vmadh $v21, $v27, $v31.e7                          ## L:813  |    368 | vtex_transformed = vmt3 +* 0x100;
  ldv $v06, 8, 48, $s1                               ## L:400  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v01, 0, 0, $s1                                ## L:395  | ***372 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vrcph $v23.e0, $v21.e3                             ## L:818  |      ^ | vinv_tex.x = invert_half(vtex_transformed).w;
  vrcpl $v24.e0, $v22.e3                             ## L:818  |    373 | vinv_tex.x = invert_half(vtex_transformed).w;
  LOAD_COLOR845: llv $v18, 4, 0, $t0                 ## L:845  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vrcph $v23.e0, $v21.e7                             ## L:819  |    374 | vinv_tex.X = invert_half(vtex_transformed).W;
  LOAD_POS_NORM841: ldv $v15, 8, 0, $t0              ## L:841  |      ^ | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  vrcpl $v24.e4, $v22.e7                             ## L:819  |    375 | vinv_tex.X = invert_half(vtex_transformed).W;
  vrcph $v23.e4, $v00.e7                             ## L:819  |    376 | vinv_tex.X = invert_half(vtex_transformed).W;
  ldv $v02, 0, 32, $s1                               ## L:396  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmudl $v29, $v22, $v24.h0                          ## L:821  |  **379 | vtex_transformed *= vinv_tex.xxxxXXXX;
  ldv $v06, 0, 48, $s1                               ## L:400  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadm $v29, $v21, $v24.h0                          ## L:821  |    380 | vtex_transformed *= vinv_tex.xxxxXXXX;
  LOAD_POS_NORM842: ldv $v14, 0, 0, $s4              ## L:842  |      ^ | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vmadn $v22, $v22, $v23.h0                          ## L:821  |    381 | vtex_transformed *= vinv_tex.xxxxXXXX;
  ldv $v01, 8, 0, $s1                                ## L:395  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadh $v21, $v21, $v23.h0                          ## L:821  |    382 | vtex_transformed *= vinv_tex.xxxxXXXX;
  llv $v23, 0, 0, $at                                ## L:825  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  llv $v23, 8, 0, $at                                ## L:826  |    383 | vtex_size.XY = load(TEXTURE_SIZE).xy;
  ori $at, $zero, %lo(TEXTURE_OFFSET)                ## L:827  |    384 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 0, 0, $at                                ## L:827  |    385 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:828  |    386 | vtex_offset.XY = load(TEXTURE_OFFSET).xy;
  vmudh $v29, $v24, $v30.e7                          ## L:830  | ***390 | VTEMP = vtex_offset:sint * 1;
  sltu $at, $s4, $s6                                 ## L:847  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadn $v29, $v22, $v23.v                           ## L:831  |    391 | vtex = vtex_transformed +* vtex_size;
  bne $at, $zero, VERTEX_LOOP                        ## L:847  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v16, $v21, $v23.v                           ## L:831  |   *393 | vtex = vtex_transformed +* vtex_size;
  sdv $v19, 0, 0, $t2                                ## L:853  |    394 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v17, 4, 8, $t3                                ## L:862  |    395 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sb $t4, 6($t2)                                     ## L:856  |    396 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t6, 7($t2)                                     ## L:858  |    397 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sdv $v19, 8, 0, $t3                                ## L:854  |    398 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  slv $v16, 8, 12, $t3                               ## L:865  |    399 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  suv $v17, 0, 8, $t2                                ## L:861  |    400 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sb $t5, 6($t3)                                     ## L:857  |    401 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $t7, 7($t3)                                     ## L:859  |    402 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  j RSPQ_Loop                                        ## L:866  |    403 | }
  slv $v16, 0, 12, $t2                               ## L:864  |   *405 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40

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