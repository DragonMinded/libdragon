## Auto-generated file, transpiled with RSPL
#define ENABLE_ENV_MAP 
#define ENABLE_NORMALIZE 
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
#define PIPELINE_COUNT (1<<1)
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
    MgVertexAttributeLoaders LOAD_POS_NORM515, LOAD_POS_NORM516, LOAD_POS_NORM517, LOAD_POS_NORM518, LOAD_POS_NORM842, LOAD_POS_NORM841, LOAD_POS_NORM839, LOAD_POS_NORM840
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR519, LOAD_COLOR520, LOAD_COLOR844, LOAD_COLOR843
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:414  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:413  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:415  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:414  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:417  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:418  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:417  |      9 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:431  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:417  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:439  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ssv $v03, 0, 8, $at                                ## L:419  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:419  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:418  |     15 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:429  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:425  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:427  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  andi $t3, $t2, 7                                   ## L:434  |     19 | u32 misalignment = buffer_offset & 0x7;
  addu $s0, $s0, $t2                                 ## L:432  |     20 | vtx_buffer += buffer_offset;
  addu $t0, $t1, $t3                                 ## L:435  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:436  |     22 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:437  |     23 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:440  |     24 | vtx_in0_ptr -= dma_size;
  addiu $t0, $t0, -1                                 ## L:442  |     25 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  jal DMAExec                                        ## L:442  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t2, $zero, 12                               ## L:442  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:461  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:461  |     30 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:456  |     31 | u16 mvpaddr = MATRICES_MVP;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:444  |     32 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v11, 0, 16, $at                               ## L:463  |     33 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:399  |     34 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:461  |     35 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:395  |     36 | m0:ufract = load(address, 0x20).xyzwxyzw;
  lhu $fp, %lo(MATERIAL_FLAGS + 0)                   ## L:500  |     37 | u16 mat_flags = load(MATERIAL_FLAGS);
  ldv $v10, 0, 8, $at                                ## L:462  |     38 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:463  |     39 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:462  |     40 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:448  |     41 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:465  |     42 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:394  |     43 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:465  |     44 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:466  |     45 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:397  |     46 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:396  |     47 | m1:sint   = load(address, 0x08).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:447  |     48 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ldv $v13, 8, 8, $at                                ## L:466  |     49 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:397  |     50 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:465  |     51 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:399  |     52 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:394  |     53 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:492  |     54 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:476  |     55 | u16 prev_out1 = MG_SCRATCH_MEM;
  addiu $sp, $zero, 80                               ## L:489  |     56 | u16 mat_ptr_emissive = 0x50;
  ldv $v07, 0, 24, $t0                               ## L:400  |     57 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v21, 0, 0, $at                                ## L:492  |     58 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v21, 8, 0, $at                                ## L:492  |     59 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:493  |     60 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  andi $s0, $fp, 1                                   ## L:501  |     61 | u16 tmp = mat_flags & 0x1;
  ldv $v07, 8, 24, $t0                               ## L:400  |     62 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:493  |     63 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:493  |     64 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:494  |     65 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:494  |     66 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:398  |     67 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:401  |     68 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:494  |     69 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:401  |     70 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:496  |     71 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  addiu $t1, $s7, 40                                 ## L:473  |     72 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  sqv $v23, 0, 80, $at                               ## L:498  |     73 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  ldv $v02, 8, 32, $t0                               ## L:395  |     74 | m0:ufract = load(address, 0x20).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:445  |     75 | vtx_size2 = vtx_size * 2;
  ori $t2, $zero, %lo(MG_SCRATCH_MEM)                ## L:475  |     76 | u16 prev_out0 = MG_SCRATCH_MEM;
  ldv $v03, 8, 8, $t0                                ## L:396  |     77 | m1:sint   = load(address, 0x08).xyzwxyzw;
  sqv $v22, 0, 64, $at                               ## L:497  |     78 | store(vambient, MG_SCRATCH_MEM, 0x40);
  ldv $v05, 8, 16, $t0                               ## L:398  |     79 | m2:sint   = load(address, 0x10).xyzwxyzw;
  addiu $k0, $zero, 48                               ## L:487  |     80 | u16 mat_ptr_diffuse  = 0x30;
  addu $t0, $s4, $t8                                 ## L:472  |     81 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  addiu $k1, $zero, 64                               ## L:488  |     82 | u16 mat_ptr_ambient  = 0x40;
  beq $s0, $zero, LABEL_gl_pipeline_0001             ## L:502  |     83 | if (tmp != 0) {
  sqv $v21, 0, 48, $at                               ## L:496  |    *85 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  addiu $k0, $zero, 96                               ## L:503  |     86 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s0, $fp, 2                                   ## L:505  |     87 | tmp = mat_flags & 0x2;
  beq $s0, $zero, LABEL_gl_pipeline_0002             ## L:506  |     88 | if (tmp != 0 ) {
  nop                                                ## L:506  |    *90 | if (tmp != 0 ) {
  addiu $k1, $zero, 96                               ## L:507  |     91 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s0, $fp, 4                                   ## L:509  |     92 | tmp = mat_flags & 0x4;
  beq $s0, $zero, LABEL_gl_pipeline_0003             ## L:510  |     93 | if (tmp != 0) {
  nop                                                ## L:510  |    *95 | if (tmp != 0) {
  addiu $sp, $zero, 96                               ## L:511  |     96 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_POS_NORM515: ldv $v15, 0, 0, $s4              ## L:515  |     97 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM516: ldv $v15, 8, 0, $t0              ## L:516  |     98 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_POS_NORM517: ldv $v14, 0, 0, $s4              ## L:517  |     99 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR519: llv $v18, 0, 0, $s4                 ## L:519  |    100 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_COLOR520: llv $v18, 4, 0, $t0                 ## L:520  |    101 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  LOAD_POS_NORM518: ldv $v14, 8, 0, $t0              ## L:518  |    102 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  vand $v15, $v12, $v15.h3                           ## L:555  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  sdv $v19, 0, 0, $t2                                ## L:528  | ***106 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmudn $v15, $v15, $v13.v                           ## L:556  |      ^ | vnorm *= vnormfactor;
  sb $t4, 6($t2)                                     ## L:531  |    107 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vmudn $v29, $v02, $v14.h0                          ## L:559  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v14.h0                          ## L:559  |    108 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  suv $v17, 4, 8, $t3                                ## L:537  |      ^ | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadn $v29, $v04, $v14.h1                          ## L:560  |    109 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  sdv $v19, 8, 0, $t3                                ## L:529  |      ^ | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmadh $v29, $v03, $v14.h1                          ## L:560  |    110 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:542  |      ^ | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v18, 0, 0, $at                                ## L:542  |    111 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadn $v29, $v06, $v14.h2                          ## L:561  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v14.h2                          ## L:561  |    112 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  suv $v17, 0, 8, $t2                                ## L:536  |      ^ | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  slv $v16, 0, 12, $t2                               ## L:539  |    113 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmadn $v20, $v08, $v30.e2                          ## L:562  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmadh $v19, $v07, $v30.e2                          ## L:562  |    114 | vpos_clip = vmvp3 +* 0x20;
  vmulf $v29, $v09, $v15.h0                          ## L:564  |    115 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  vmacf $v29, $v10, $v15.h1                          ## L:565  |    116 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  sb $t7, 7($t3)                                     ## L:534  |      ^ | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmacf $v15, $v11, $v15.h2                          ## L:566  |    117 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  vmudh $v22, $v15, $v15.v                           ## L:570  | ***121 | vec32 vsqdist = vnorm * vnorm;
  luv $v18, 0, 0, $at                                ## L:543  |      ^ | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vsar $v22, COP2_ACC_MD                             ## L:570  |    122 | vec32 vsqdist = vnorm * vnorm;
  sdv $v19, 0, 24, $s7                               ## L:594  |      ^ | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vsar $v21, COP2_ACC_HI                             ## L:570  |    123 | vec32 vsqdist = vnorm * vnorm;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:544  |      ^ | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vaddc $v24, $v22, $v22.h1                          ## L:571  |  **126 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  sdv $v20, 0, 32, $s7                               ## L:594  |      ^ | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vadd $v23, $v21, $v21.h1                           ## L:571  |    127 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v22, $v24, $v22.h2                          ## L:572  |  **130 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  sdv $v19, 8, 24, $t1                               ## L:595  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sqv $v18, 0, 96, $at                               ## L:544  |    131 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vadd $v21, $v23, $v21.h2                           ## L:572  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v25.e0, $v21.e0                             ## L:575  | ***135 | vinvdist.x = invert_half_sqrt(vsqdist).x;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:584  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  slv $v16, 8, 12, $t3                               ## L:540  |    136 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vrsql $v26.e0, $v22.e0                             ## L:575  |      ^ | vinvdist.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v25.e0, $v21.e4                             ## L:576  |    137 | vinvdist.X = invert_half_sqrt(vsqdist).X;
  ldv $v21, 0, 0, $at                                ## L:584  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v21, 8, 0, $at                                ## L:584  |    138 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vrsql $v26.e4, $v22.e4                             ## L:576  |      ^ | vinvdist.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v25.e4, $v00.e0                             ## L:576  |    139 | vinvdist.X = invert_half_sqrt(vsqdist).X;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:599  |      ^ | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  sb $t5, 6($t3)                                     ## L:532  |  **142 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vmudm $v29, $v15, $v26.h0                          ## L:578  |      ^ | vnorm *= vinvdist.xxxxXXXX;
  vmadh $v15, $v15, $v25.h0                          ## L:578  |    143 | vnorm *= vinvdist.xxxxXXXX;
  vmudn $v23, $v20, $v21.v                           ## L:585  |    144 | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v22, $v19, $v21.v                           ## L:585  |    145 | vec32 vguard = vpos_clip * vclip_factors;
  sb $t6, 7($t2)                                     ## L:533  |      ^ | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vch $v29, $v22, $v22.h3                            ## L:586  | ***149 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v20, 8, 32, $t1                               ## L:595  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  ldv $v22, 0, 0, $at                                ## L:599  |    150 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  vcl $v29, $v23, $v23.h3                            ## L:586  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $fp, $vcc                                     ## L:586  |    151 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v19, $v19.h3                            ## L:590  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v20, $v20.h3                            ## L:590  |    152 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v22, 8, 0, $at                                ## L:599  |      ^ | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:608  |    153 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  andi $s1, $fp, 1799                                ## L:360  |    154 | t0 = codes & 0x707;
  srl $s0, $fp, 4                                    ## L:380  |    155 | u16 codes2 = codes >> 4;
  srl $s2, $s1, 5                                    ## L:361  |    156 | t1 = t0 >> 5;
  vaddc $v23, $v20, $v22.h1                          ## L:600  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v21, $v19, $v22.h0                           ## L:601  |    157 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  or $t4, $s1, $s2                                   ## L:368  |      ^ | result = t0 | t1;
  andi $s1, $s0, 1799                                ## L:360  |    158 | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:361  |    159 | t1 = t0 >> 5;
  cfc2 $s0, $vcc                                     ## L:590  |    160 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vmudn $v29, $v23, $v22.h2                          ## L:603  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v21, $v21, $v22.h2                          ## L:604  |    161 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:638  |      ^ | u16 addr = MATRICES_MV;
  ldv $v22, 0, 0, $at                                ## L:608  |    162 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  or $t5, $s1, $s2                                   ## L:368  |    163 | result = t0 | t1;
  ldv $v22, 8, 0, $at                                ## L:608  |    164 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  andi $s2, $s0, 1799                                ## L:360  |    165 | t0 = codes & 0x707;
  srl $s3, $s2, 5                                    ## L:361  |    166 | t1 = t0 >> 5;
  nor $t6, $s2, $s3                                  ## L:375  |    167 | result = t0 ~| t1;
  vge $v21, $v00, $v21.h2                            ## L:605  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  ldv $v07, 0, 24, $fp                               ## L:400  |    168 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 0, 56, $fp                               ## L:401  |    169 | m3:ufract = load(address, 0x38).xyzwxyzw;
  srl $s1, $s0, 4                                    ## L:387  |    170 | u16 codes2 = codes >> 4;
  ldv $v03, 0, 8, $fp                                ## L:396  |    171 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudl $v20, $v20, $v22.h3                          ## L:611  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v05, 0, 16, $fp                               ## L:398  |    172 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadm $v19, $v19, $v22.h3                          ## L:611  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadn $v20, $v00, $v00                             ## L:611  |    173 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v03, 8, 8, $fp                                ## L:396  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:399  |    174 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v07, 8, 24, $fp                               ## L:400  |    175 | m3:sint   = load(address, 0x18).xyzwxyzw;
  andi $s2, $s1, 1799                                ## L:360  |    176 | t0 = codes & 0x707;
  vrcph $v24.e3, $v19.e3                             ## L:618  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcpl $v25.e3, $v20.e3                             ## L:618  |    177 | vinvw.w = invert_half(vpos_clip).w;
  srl $s3, $s2, 5                                    ## L:361  |      ^ | t1 = t0 >> 5;
  ssv $v19, 14, 16, $t1                              ## L:615  |    178 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v23, 0, 8, $at                                ## L:609  |    179 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vrcph $v24.e3, $v19.e7                             ## L:619  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcpl $v25.e7, $v20.e7                             ## L:619  |    180 | vinvw.W = invert_half(vpos_clip).W;
  ldv $v06, 8, 48, $fp                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrcph $v24.e7, $v00.e7                             ## L:619  |    181 | vinvw.W = invert_half(vpos_clip).W;
  ldv $v05, 8, 16, $fp                               ## L:398  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  nor $t7, $s2, $s3                                  ## L:375  |    182 | result = t0 ~| t1;
  ldv $v08, 8, 56, $fp                               ## L:401  |    183 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ssv $v19, 6, 16, $s7                               ## L:614  |    184 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v04, 0, 40, $fp                               ## L:397  |    185 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ssv $v20, 14, 18, $t1                              ## L:615  |    186 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v02, 0, 32, $fp                               ## L:395  |    187 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ssv $v20, 6, 18, $s7                               ## L:614  |    188 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v01, 0, 0, $fp                                ## L:394  |    189 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmudl $v29, $v20, $v25.h3                          ## L:624  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v19, $v25.h3                          ## L:624  |    190 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v23, 8, 8, $at                                ## L:609  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vmadn $v20, $v20, $v24.h3                          ## L:624  |    191 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v01, 8, 0, $fp                                ## L:394  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:647  |    192 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v02, 8, 32, $fp                               ## L:395  |    193 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ssv $v24, 6, 20, $s7                               ## L:621  |    194 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ldv $v17, 0, 0, $at                                ## L:647  |    195 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v19, $v19, $v24.h3                          ## L:624  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v17, 8, 0, $at                                ## L:647  |    196 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmudh $v29, $v23, $v30.e7                          ## L:628  |      ^ | VTEMP = vviewoff:sint * 1;
  addiu $at, $k0, MG_SCRATCH_MEM                     ## L:649  |    197 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  vmadn $v20, $v20, $v22.v                           ## L:629  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  ldv $v04, 8, 40, $fp                               ## L:397  |    198 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ssv $v24, 14, 20, $t1                              ## L:622  |    199 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v19, $v19, $v22.v                           ## L:629  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  lqv $v22, 0, 0, $at                                ## L:649  |    200 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:650  |    201 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  vmudn $v29, $v02, $v14.h0                          ## L:641  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v14.h0                          ## L:641  |    202 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  lqv $v23, 0, 0, $at                                ## L:650  |      ^ | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:651  |    203 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  vmadn $v29, $v04, $v14.h1                          ## L:642  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadh $v29, $v03, $v14.h1                          ## L:642  |    204 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  lqv $v24, 0, 0, $at                                ## L:651  |      ^ | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  vmadn $v29, $v06, $v14.h2                          ## L:643  |    205 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v25, 6, 22, $s7                               ## L:621  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  lbu $fp, %lo(LIGHTING_COUNT + 0)                   ## L:658  |    206 | u8 light_count = load(LIGHTING_COUNT);
  vmadh $v29, $v05, $v14.h2                          ## L:643  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v25, 14, 22, $t1                              ## L:622  |    207 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v29, $v08, $v30.e2                          ## L:644  |      ^ | vpos = vmvp3 +* 0x20;
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:660  |    208 | u16 light_ptr = LIGHTING_LIGHTS;
  vmadh $v14, $v07, $v30.e2                          ## L:644  |      ^ | vpos = vmvp3 +* 0x20;
  vmudh $v29, $v24, $v30.e7                          ## L:653  |    209 | VTEMP = vemissive:sint * 1;
  beq $fp, $zero, LIGHT_END                          ## L:661  |      ^ | if (light_count == 0) goto LIGHT_END;
  vmacf $v17, $v17, $v23.v                           ## L:654  |   *211 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  addiu $s1, $s0, 14                                 ## L:664  |    212 | u16 amb_ptr = light_ptr + 14;
  ldv $v24, 0, 0, $s1                                ## L:665  |    213 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $s1                                ## L:665  |    214 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  lh $s2, 6($s0)                                     ## L:671  |    215 | s16 light_w = load(light_ptr, 6);
  ldv $v25, 0, 8, $s0                                ## L:666  |    216 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v25, 8, 8, $s0                                ## L:666  |    217 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v26, 0, 0, $s0                                ## L:667  |    218 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v26, 8, 0, $s0                                ## L:667  |    219 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $s2, $zero, LABEL_gl_pipeline_0005             ## L:672  |    220 | if (light_w != 0)
  vmulf $v25, $v25, $v22.v                           ## L:669  |   *222 | vlcol:sfract *= vdiffuse;
  vsubc $v26, $v26, $v14.v                           ## L:686  |    223 | vlpos -= vpos;
  addiu $s3, $s0, 20                                 ## L:680  |      ^ | u16 att_ptr = light_ptr + 20;
  ldv $v01, 0, 0, $s3                                ## L:682  | ***227 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vmudh $v04, $v26, $v26.v                           ## L:687  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vsar $v04, COP2_ACC_MD                             ## L:687  |    228 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 8, 0, $s3                                ## L:682  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v03, COP2_ACC_HI                             ## L:687  |    229 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v06, $v04, $v04.h1                          ## L:688  |  **232 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v05, $v03, $v03.h1                           ## L:688  |    233 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:689  |  **236 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:689  |    237 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:691  | ***241 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:691  |    242 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:692  |    243 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:692  |    244 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:692  |    245 | vtmp.X = invert_half_sqrt(vsqdist).X;
  addiu $s3, $s3, 6                                  ## L:683  |      ^ | att_ptr += 6;
  ldv $v02, 0, 0, $s3                                ## L:684  |  **248 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vmudl $v29, $v06, $v04.v                           ## L:694  |      ^ | vec32 vdist = vtmp * vsqdist;
  ldv $v02, 8, 0, $s3                                ## L:684  |    249 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vmadm $v29, $v05, $v04.v                           ## L:694  |      ^ | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:694  |    250 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:694  |    251 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v26, $v06.h0                          ## L:696  |    252 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v26, $v26, $v05.h0                          ## L:696  |    253 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:698  |    254 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:698  |    255 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:699  |    256 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:699  |    257 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:699  |    258 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:699  |    259 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:700  |    260 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:700  |    261 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:700  |    262 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:700  |    263 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vrcph $v01.e0, $v01.e0                             ## L:702  | ***267 | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:702  |    268 | vatten.x = invert_half(vatten).x;
  vrcph $v01.e0, $v01.e4                             ## L:703  |  **271 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:703  |    272 | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:703  |    273 | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v25, $v02.h0                          ## L:705  |  **276 | vlcol *= vatten.xxxxXXXX;
  vmadh $v25, $v25, $v01.h0                          ## L:705  |    277 | vlcol *= vatten.xxxxXXXX;
  LABEL_gl_pipeline_0005:
  vmulf $v26, $v26, $v15.v                           ## L:719  |    278 | vlpos *= vnorm:sfract;
  addiu $s0, $s0, 32                                 ## L:728  |      ^ | light_ptr += 32;
  vmudn $v29, $v24, $v23.v                           ## L:720  |    279 | VTEMP = vlambient * vambient;
  addiu $fp, $fp, 65535                              ## L:729  |      ^ | light_count -= 1;
  vmacf $v29, $v25, $v26.h0                          ## L:721  |  **282 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v25, $v26.h1                          ## L:722  |    283 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v25, $v25, $v26.h2                          ## L:723  |    284 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v25, $v25, $v00                               ## L:724  | ***288 | vlcol = max(vlcol, VZERO);
  bne $fp, $zero, LIGHT_LOOP                         ## L:730  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v17, $v17, $v25.v                            ## L:726  | ****293 | vrgba:sint += vlcol;
  LIGHT_END:
  lbu $s1, %lo(FOG_MASK + 0)                         ## L:741  |    294 | u8 fog_mask = load(FOG_MASK);
  vmov $v17.e3, $v22.e3                              ## L:734  |      ^ | vrgba.w = vdiffuse.w;
  vmov $v17.e7, $v22.e7                              ## L:735  |    295 | vrgba.W = vdiffuse.W;
  or $t2, $zero, $s7                                 ## L:832  |      ^ | prev_out0 = vtx_out0_ptr;
  ctc2 $s1, $vcc                                     ## L:742  | ***299 | set_vcc(fog_mask);
  vmrg $v17, $v17, $v21.h2                           ## L:743  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v22, $v14, $v14.v                           ## L:754  |    300 | vec32 vsqdist = vpos * vpos;
  addu $t0, $t0, $t9                                 ## L:835  |      ^ | vtx_in1_ptr += vtx_size2;
  vsar $v22, COP2_ACC_MD                             ## L:754  |    301 | vec32 vsqdist = vpos * vpos;
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:750  |      ^ | u16 addr = MATRICES_MVP;
  ldv $v02, 8, 32, $s1                               ## L:395  |    302 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vsar $v21, COP2_ACC_HI                             ## L:754  |      ^ | vec32 vsqdist = vpos * vpos;
  ldv $v07, 0, 24, $s1                               ## L:400  |  **305 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vaddc $v24, $v22, $v22.h1                          ## L:755  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v04, 8, 40, $s1                               ## L:397  |    306 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vadd $v23, $v21, $v21.h1                           ## L:755  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ori $at, $zero, %lo(TEXTURE_MATRIX)                ## L:803  |  **309 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  vaddc $v22, $v24, $v22.h2                          ## L:756  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v21, $v23, $v21.h2                           ## L:756  |    310 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v06, 8, 48, $s1                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  LOAD_COLOR844: llv $v18, 4, 0, $t0                 ## L:844  | ***314 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vrsqh $v23.e0, $v21.e0                             ## L:758  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v24.e0, $v22.e0                             ## L:758  |    315 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v27, 0, 16, $at                               ## L:807  |      ^ | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  vrsqh $v23.e0, $v21.e4                             ## L:759  |    316 | vtmp.X = invert_half_sqrt(vsqdist).X;
  addiu $s7, $s7, 80                                 ## L:836  |      ^ | vtx_out0_ptr += 80;
  vrsql $v24.e4, $v22.e4                             ## L:759  |    317 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v27, 8, 16, $at                               ## L:807  |      ^ | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  ldv $v03, 8, 8, $s1                                ## L:396  |    318 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrsqh $v23.e4, $v00.e0                             ## L:759  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudm $v29, $v14, $v24.h0                          ## L:761  |  **321 | vpos *= vtmp.xxxxXXXX;
  ldv $v08, 0, 56, $s1                               ## L:401  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v03, 0, 8, $s1                                ## L:396  |    322 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmadh $v14, $v14, $v23.h0                          ## L:761  |      ^ | vpos *= vtmp.xxxxXXXX;
  vmulf $v23, $v14, $v15.v                           ## L:764  | ***326 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v24, $v23, $v23.h1                           ## L:765  | ***330 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v23, $v24, $v23.h2                           ## L:766  | ***334 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  ldv $v28, 0, 40, $at                               ## L:808  |      ^ | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  vor $v24, $v00, $v00                               ## L:771  |    335 | vec16 vzunit = VZERO;
  vmov $v24.e2, $v12.e3                              ## L:772  |    336 | vzunit.z = vnormmask.w;
  vmov $v24.e6, $v12.e3                              ## L:773  |    337 | vzunit.Z = vnormmask.w;
  ldv $v28, 8, 40, $at                               ## L:808  |      ^ | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  vlt $v23, $v23, $v00                               ## L:768  |    338 | vdot = min(vdot, VZERO);
  vsubc $v23, $v00, $v23.v                           ## L:769  | ***342 | vdot = VZERO - vdot;
  vmulf $v29, $v15, $v23.h0                          ## L:775  | ***346 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v15, $v23.h0                          ## L:776  |    347 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  vmadh $v29, $v14, $v30.e7                          ## L:777  |    348 | VTEMP = vpos:sint +* 1;
  LOAD_POS_NORM842: ldv $v14, 8, 0, $t0              ## L:842  |      ^ | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v29, $v24, $v30.e7                          ## L:778  |    349 | VTEMP = vzunit:sint +* 1;
  vsar $v24, COP2_ACC_MD                             ## L:781  |    350 | vec32 vrefl = get_acc();
  vsar $v23, COP2_ACC_HI                             ## L:781  |    351 | vec32 vrefl = get_acc();
  vmudl $v29, $v24, $v24.v                           ## L:783  |  **354 | vsqdist = vrefl * vrefl;
  vmadm $v29, $v23, $v24.v                           ## L:783  |    355 | vsqdist = vrefl * vrefl;
  vmadn $v22, $v24, $v23.v                           ## L:783  |    356 | vsqdist = vrefl * vrefl;
  vmadh $v21, $v23, $v23.v                           ## L:783  |    357 | vsqdist = vrefl * vrefl;
  vaddc $v26, $v22, $v22.h1                          ## L:784  |  **360 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v25, $v21, $v21.h1                           ## L:784  |    361 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v22, $v26, $v22.h2                          ## L:785  |  **364 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v21, $v25, $v21.h2                           ## L:785  |    365 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vxor $v16, $v00, $v30.e0                           ## L:790  |    366 | vtex = 0x80;
  vrsqh $v25.e0, $v21.e0                             ## L:787  |  **369 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v26.e0, $v22.e0                             ## L:787  |    370 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v25.e0, $v21.e4                             ## L:788  |    371 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v06, 0, 48, $s1                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrsql $v26.e4, $v22.e4                             ## L:788  |    372 | vtmp.X = invert_half_sqrt(vsqdist).X;
  addu $s4, $s4, $t9                                 ## L:834  |      ^ | vtx_in0_ptr += vtx_size2;
  vrsqh $v25.e4, $v00.e0                             ## L:788  |    373 | vtmp.X = invert_half_sqrt(vsqdist).X;
  LOAD_COLOR843: llv $v18, 0, 0, $s4                 ## L:843  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmudh $v29, $v16, $v30.e7                          ## L:791  |    374 | VTEMP = vtex:sint * 1;
  or $t3, $zero, $t1                                 ## L:833  |      ^ | prev_out1 = vtx_out1_ptr;
  addiu $t1, $t1, 80                                 ## L:837  |   *376 | vtx_out1_ptr += 80;
  vmadl $v29, $v24, $v26.h0                          ## L:792  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  vmadm $v29, $v23, $v26.h0                          ## L:792  |    377 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v26, 0, 32, $at                               ## L:806  |      ^ | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  vmadn $v29, $v24, $v25.h0                          ## L:792  |    378 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v24, 0, 24, $at                               ## L:804  |      ^ | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  vmadh $v16, $v23, $v25.h0                          ## L:792  |    379 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v26, 8, 32, $at                               ## L:806  |      ^ | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  ldv $v24, 8, 24, $at                               ## L:804  |    380 | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:803  |    381 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:803  |    382 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v25, 0, 8, $at                                ## L:805  |    383 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  ldv $v25, 8, 8, $at                                ## L:805  |    384 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  vmudn $v29, $v24, $v16.h0                          ## L:810  |      ^ | VTEMP = vmt0  * vtex.xxxxXXXX;
  ori $at, $zero, %lo(TEXTURE_SIZE)                  ## L:824  |    385 | vtex_size.xy = load(TEXTURE_SIZE).xy;
  ldv $v01, 8, 0, $s1                                ## L:394  |    386 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadh $v29, $v23, $v16.h0                          ## L:810  |      ^ | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadn $v29, $v26, $v16.h1                          ## L:811  |    387 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  vmadh $v29, $v25, $v16.h1                          ## L:811  |    388 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  LOAD_POS_NORM841: ldv $v14, 0, 0, $s4              ## L:841  |      ^ | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vmadn $v22, $v28, $v31.e7                          ## L:812  |    389 | vtex_transformed = vmt3 +* 0x100;
  ldv $v01, 0, 0, $s1                                ## L:394  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadh $v21, $v27, $v31.e7                          ## L:812  |    390 | vtex_transformed = vmt3 +* 0x100;
  LOAD_POS_NORM839: ldv $v15, 0, 0, $s4              ## L:839  |      ^ | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  vrcph $v23.e0, $v21.e3                             ## L:817  | ***394 | vinv_tex.x = invert_half(vtex_transformed).w;
  ldv $v08, 8, 56, $s1                               ## L:401  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcpl $v24.e0, $v22.e3                             ## L:817  |    395 | vinv_tex.x = invert_half(vtex_transformed).w;
  ldv $v02, 0, 32, $s1                               ## L:395  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrcph $v23.e0, $v21.e7                             ## L:818  |    396 | vinv_tex.X = invert_half(vtex_transformed).W;
  ldv $v05, 0, 16, $s1                               ## L:398  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrcpl $v24.e4, $v22.e7                             ## L:818  |    397 | vinv_tex.X = invert_half(vtex_transformed).W;
  vrcph $v23.e4, $v00.e7                             ## L:818  |    398 | vinv_tex.X = invert_half(vtex_transformed).W;
  vmudl $v29, $v22, $v24.h0                          ## L:820  |  **401 | vtex_transformed *= vinv_tex.xxxxXXXX;
  LOAD_POS_NORM840: ldv $v15, 8, 0, $t0              ## L:840  |      ^ | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  vmadm $v29, $v21, $v24.h0                          ## L:820  |    402 | vtex_transformed *= vinv_tex.xxxxXXXX;
  ldv $v04, 0, 40, $s1                               ## L:397  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadn $v22, $v22, $v23.h0                          ## L:820  |    403 | vtex_transformed *= vinv_tex.xxxxXXXX;
  ldv $v05, 8, 16, $s1                               ## L:398  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v21, $v21, $v23.h0                          ## L:820  |    404 | vtex_transformed *= vinv_tex.xxxxXXXX;
  llv $v23, 0, 0, $at                                ## L:824  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  llv $v23, 8, 0, $at                                ## L:825  |    405 | vtex_size.XY = load(TEXTURE_SIZE).xy;
  ori $at, $zero, %lo(TEXTURE_OFFSET)                ## L:826  |    406 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 0, 0, $at                                ## L:826  |    407 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:827  |    408 | vtex_offset.XY = load(TEXTURE_OFFSET).xy;
  sltu $at, $s4, $s6                                 ## L:846  |    409 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  ldv $v07, 8, 24, $s1                               ## L:400  |  **412 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:829  |      ^ | VTEMP = vtex_offset:sint * 1;
  vmadn $v29, $v22, $v23.v                           ## L:830  |    413 | vtex = vtex_transformed +* vtex_size;
  bne $at, $zero, VERTEX_LOOP                        ## L:846  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v16, $v21, $v23.v                           ## L:830  |   *415 | vtex = vtex_transformed +* vtex_size;
  suv $v17, 0, 8, $t2                                ## L:860  |    416 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v19, 0, 0, $t2                                ## L:852  |    417 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sdv $v19, 8, 0, $t3                                ## L:853  |    418 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sb $t5, 6($t3)                                     ## L:856  |    419 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $t7, 7($t3)                                     ## L:858  |    420 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  suv $v17, 4, 8, $t3                                ## L:861  |    421 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sb $t6, 7($t2)                                     ## L:857  |    422 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  slv $v16, 0, 12, $t2                               ## L:863  |    423 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t4, 6($t2)                                     ## L:855  |    424 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  j RSPQ_Loop                                        ## L:865  |    425 | }
  slv $v16, 8, 12, $t3                               ## L:864  |   *427 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80

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