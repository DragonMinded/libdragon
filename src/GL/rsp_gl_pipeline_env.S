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
#define PIPELINE_FEATURE_COUNT 2
#define PIPELINE_COUNT (1<<2)
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
#define GLP_ATTRIBUTE_POSITION 0
#define GLP_ATTRIBUTE_NORMAL 1
#define GLP_ATTRIBUTE_COLOR 2
#define GLP_ATTRIBUTE_TEXCOORD 3
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
    MgVertexAttributeLoaders LOAD_POSITION606, LOAD_POSITION607, LOAD_POSITION935, LOAD_POSITION936
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL604, LOAD_NORMAL605, LOAD_NORMAL933, LOAD_NORMAL934
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR608, LOAD_COLOR609, LOAD_COLOR938, LOAD_COLOR937
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:503  |      ^ | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:502  |      2 | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:504  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:503  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:506  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:520  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v03, COP2_ACC_HI                             ## L:506  |      9 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:507  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:528  |     10 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vsar $v04, COP2_ACC_MD                             ## L:506  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ssv $v03, 0, 8, $at                                ## L:508  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  ssv $v04, 0, 10, $at                               ## L:508  |     14 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:518  |     15 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:507  |     16 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:514  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:516  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:521  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:523  |     20 | u32 misalignment = buffer_offset & 0x7;
  addiu $t2, $zero, 12                               ## L:531  |     21 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addu $t0, $t1, $t3                                 ## L:524  |     22 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:525  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:526  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:529  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:531  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:531  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:550  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:551  |     30 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  addiu $sp, $zero, 80                               ## L:578  |     31 | u16 mat_ptr_emissive = 0x50;
  ldv $v09, 0, 0, $at                                ## L:550  |     32 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:545  |     33 | u16 mvpaddr = MATRICES_MVP;
  ldv $v05, 0, 16, $t0                               ## L:487  |     34 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:489  |     35 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:552  |     36 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:550  |     37 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:552  |     38 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:483  |     39 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:486  |     40 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:489  |     41 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:551  |     42 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:554  |     43 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:554  |     44 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:555  |     45 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:488  |     46 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:554  |     47 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:555  |     48 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:581  |     49 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v21, 0, 0, $at                                ## L:581  |     50 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:484  |     51 | m0:ufract = load(address, 0x20).xyzwxyzw;
  lhu $fp, %lo(MATERIAL_FLAGS + 0)                   ## L:589  |     52 | u16 mat_flags = load(MATERIAL_FLAGS);
  ldv $v08, 0, 56, $t0                               ## L:490  |     53 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:487  |     54 | m2:sint   = load(address, 0x10).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:533  |     55 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v21, 8, 0, $at                                ## L:581  |     56 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:582  |     57 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:582  |     58 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  addiu $k0, $zero, 48                               ## L:576  |     59 | u16 mat_ptr_diffuse  = 0x30;
  ldv $v22, 8, 0, $at                                ## L:582  |     60 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:484  |     61 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:486  |     62 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:483  |     63 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:583  |     64 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:485  |     65 | m1:sint   = load(address, 0x08).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:537  |     66 | vtx_out0_ptr += MG_VERTEX_CACHE;
  andi $s0, $fp, 1                                   ## L:590  |     67 | u16 tmp = mat_flags & 0x1;
  addiu $k1, $zero, 64                               ## L:577  |     68 | u16 mat_ptr_ambient  = 0x40;
  addu $s6, $s4, $t1                                 ## L:536  |     69 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ldv $v23, 0, 0, $at                                ## L:583  |     70 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:583  |     71 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:585  |     72 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:565  |     73 | u16 prev_out1 = MG_SCRATCH_MEM;
  ldv $v06, 8, 48, $t0                               ## L:488  |     74 | m2:ufract = load(address, 0x30).xyzwxyzw;
  sqv $v21, 0, 48, $at                               ## L:585  |     75 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  ldv $v08, 8, 56, $t0                               ## L:490  |     76 | m3:ufract = load(address, 0x38).xyzwxyzw;
  sqv $v22, 0, 64, $at                               ## L:586  |     77 | store(vambient, MG_SCRATCH_MEM, 0x40);
  ldv $v03, 8, 8, $t0                                ## L:485  |     78 | m1:sint   = load(address, 0x08).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:534  |     79 | vtx_size2 = vtx_size * 2;
  addiu $t1, $s7, 40                                 ## L:562  |     80 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  addu $t0, $s4, $t8                                 ## L:561  |     81 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  sqv $v23, 0, 80, $at                               ## L:587  |     82 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  beq $s0, $zero, LABEL_gl_pipeline_0001             ## L:591  |     83 | if (tmp != 0) {
  ori $t2, $zero, %lo(MG_SCRATCH_MEM)                ## L:564  |    *85 | u16 prev_out0 = MG_SCRATCH_MEM;
  addiu $k0, $zero, 96                               ## L:592  |     86 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s0, $fp, 2                                   ## L:594  |     87 | tmp = mat_flags & 0x2;
  beq $s0, $zero, LABEL_gl_pipeline_0002             ## L:595  |     88 | if (tmp != 0 ) {
  nop                                                ## L:595  |    *90 | if (tmp != 0 ) {
  addiu $k1, $zero, 96                               ## L:596  |     91 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s0, $fp, 4                                   ## L:598  |     92 | tmp = mat_flags & 0x4;
  beq $s0, $zero, LABEL_gl_pipeline_0003             ## L:599  |     93 | if (tmp != 0) {
  nop                                                ## L:599  |    *95 | if (tmp != 0) {
  addiu $sp, $zero, 96                               ## L:600  |     96 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_NORMAL604: lsv $v15, 0, 0, $s4                ## L:604  |     97 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  LOAD_NORMAL605: lsv $v15, 8, 0, $t0                ## L:605  |     98 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  LOAD_COLOR608: llv $v18, 0, 0, $s4                 ## L:608  |     99 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_COLOR609: llv $v18, 4, 0, $t0                 ## L:609  |    100 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  LOAD_POSITION606: ldv $v14, 0, 0, $s4              ## L:606  |    101 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POSITION607: ldv $v14, 8, 0, $t0              ## L:607  |    102 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  vand $v15, $v12, $v15.h0                           ## L:644  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:631  |    103 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v19, 8, 0, $t3                                ## L:618  |   *105 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sdv $v19, 0, 0, $t2                                ## L:617  |    106 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmudn $v15, $v15, $v13.v                           ## L:645  |      ^ | vnorm *= vnormfactor;
  suv $v17, 0, 8, $t2                                ## L:625  |    107 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmudn $v29, $v02, $v14.h0                          ## L:648  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v14.h0                          ## L:648  |    108 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v18, 0, 0, $at                                ## L:631  |      ^ | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadn $v29, $v04, $v14.h1                          ## L:649  |    109 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  luv $v18, 0, 0, $at                                ## L:632  |      ^ | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  suv $v17, 4, 8, $t3                                ## L:626  |    110 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadh $v29, $v03, $v14.h1                          ## L:649  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:633  |    111 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmadn $v29, $v06, $v14.h2                          ## L:650  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  slv $v16, 0, 12, $t2                               ## L:628  |    112 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sqv $v18, 0, 96, $at                               ## L:633  |    113 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmadh $v29, $v05, $v14.h2                          ## L:650  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v20, $v08, $v30.e2                          ## L:651  |    114 | vpos_clip = vmvp3 +* 0x20;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:673  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v19, $v07, $v30.e2                          ## L:651  |    115 | vpos_clip = vmvp3 +* 0x20;
  ldv $v21, 0, 0, $at                                ## L:673  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v21, 8, 0, $at                                ## L:673  |    116 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmulf $v29, $v09, $v15.h0                          ## L:653  |      ^ | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  vmacf $v29, $v10, $v15.h1                          ## L:654  |    117 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:688  |      ^ | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  sb $t5, 6($t3)                                     ## L:621  |   *119 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vmacf $v15, $v11, $v15.h2                          ## L:655  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  sdv $v19, 8, 24, $t1                               ## L:684  |    120 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vmudn $v23, $v20, $v21.v                           ## L:674  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v22, $v19, $v21.v                           ## L:674  |    121 | vec32 vguard = vpos_clip * vclip_factors;
  sb $t4, 6($t2)                                     ## L:620  |      ^ | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t6, 7($t2)                                     ## L:622  |    122 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sdv $v19, 0, 24, $s7                               ## L:683  |    123 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v20, 8, 32, $t1                               ## L:684  |    124 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v20, 0, 32, $s7                               ## L:683  |    125 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vch $v29, $v22, $v22.h3                            ## L:675  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $t7, 7($t3)                                     ## L:623  |    126 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vcl $v29, $v23, $v23.h3                            ## L:675  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $fp, $vcc                                     ## L:675  |    127 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v19, $v19.h3                            ## L:679  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v20, $v20.h3                            ## L:679  |    128 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v22, 0, 0, $at                                ## L:688  |      ^ | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:688  |    129 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  andi $s1, $fp, 1799                                ## L:449  |    130 | t0 = codes & 0x707;
  srl $s0, $fp, 4                                    ## L:469  |    131 | u16 codes2 = codes >> 4;
  srl $s2, $s1, 5                                    ## L:450  |    132 | t1 = t0 >> 5;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:697  |    133 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vaddc $v23, $v20, $v22.h1                          ## L:689  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  or $t4, $s1, $s2                                   ## L:457  |    134 | result = t0 | t1;
  vadd $v21, $v19, $v22.h0                           ## L:690  |      ^ | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s1, $s0, 1799                                ## L:449  |    135 | t0 = codes & 0x707;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:727  |    136 | u16 addr = MATRICES_MV;
  ldv $v01, 0, 0, $fp                                ## L:483  |    137 | m0:sint   = load(address, 0x00).xyzwxyzw;
  cfc2 $s0, $vcc                                     ## L:679  |    138 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vmudn $v29, $v23, $v22.h2                          ## L:692  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v21, $v21, $v22.h2                          ## L:693  |    139 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v03, 0, 8, $fp                                ## L:485  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v04, 0, 40, $fp                               ## L:486  |    140 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:697  |    141 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:697  |    142 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v03, 8, 8, $fp                                ## L:485  |    143 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v01, 8, 0, $fp                                ## L:483  |    144 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v06, 0, 48, $fp                               ## L:488  |    145 | m2:ufract = load(address, 0x30).xyzwxyzw;
  srl $s2, $s1, 5                                    ## L:450  |    146 | t1 = t0 >> 5;
  vge $v21, $v00, $v21.h2                            ## L:694  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v20, $v20, $v22.h3                          ## L:700  |    147 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v23, 0, 8, $at                                ## L:698  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  or $t5, $s1, $s2                                   ## L:457  |    148 | result = t0 | t1;
  ldv $v02, 0, 32, $fp                               ## L:484  |    149 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v06, 8, 48, $fp                               ## L:488  |    150 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadm $v19, $v19, $v22.h3                          ## L:700  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadn $v20, $v00, $v00                             ## L:700  |    151 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  andi $s2, $s0, 1799                                ## L:449  |      ^ | t0 = codes & 0x707;
  ldv $v07, 0, 24, $fp                               ## L:489  |    152 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v23, 8, 8, $at                                ## L:698  |    153 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  srl $s3, $s2, 5                                    ## L:450  |    154 | t1 = t0 >> 5;
  nor $t6, $s2, $s3                                  ## L:464  |    155 | result = t0 ~| t1;
  vrcph $v24.e3, $v19.e3                             ## L:707  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ssv $v19, 6, 16, $s7                               ## L:703  |    156 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ssv $v19, 14, 16, $t1                              ## L:704  |    157 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  srl $s1, $s0, 4                                    ## L:476  |    158 | u16 codes2 = codes >> 4;
  ldv $v05, 0, 16, $fp                               ## L:487  |    159 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrcpl $v25.e3, $v20.e3                             ## L:707  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  slv $v16, 8, 12, $t3                               ## L:629  |    160 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vrcph $v24.e3, $v19.e7                             ## L:708  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  andi $s2, $s1, 1799                                ## L:449  |    161 | t0 = codes & 0x707;
  vrcpl $v25.e7, $v20.e7                             ## L:708  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v24.e7, $v00.e7                             ## L:708  |    162 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 6, 18, $s7                               ## L:703  |      ^ | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v05, 8, 16, $fp                               ## L:487  |    163 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ssv $v20, 14, 18, $t1                              ## L:704  |    164 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v08, 0, 56, $fp                               ## L:490  |    165 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudl $v29, $v20, $v25.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v24, 6, 20, $s7                               ## L:710  |    166 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadm $v29, $v19, $v25.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  srl $s3, $s2, 5                                    ## L:450  |    167 | t1 = t0 >> 5;
  vmadn $v20, $v20, $v24.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:736  |    168 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadh $v19, $v19, $v24.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmudh $v29, $v23, $v30.e7                          ## L:717  |    169 | VTEMP = vviewoff:sint * 1;
  ldv $v02, 8, 32, $fp                               ## L:484  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 8, 40, $fp                               ## L:486  |    170 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v17, 0, 0, $at                                ## L:736  |    171 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v20, $v20, $v22.v                           ## L:718  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  vmadh $v19, $v19, $v22.v                           ## L:718  |    172 | vpos_clip = vpos_clip +* vviewscale;
  ldv $v17, 8, 0, $at                                ## L:736  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmudn $v29, $v02, $v14.h0                          ## L:730  |    173 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v07, 8, 24, $fp                               ## L:489  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  addiu $at, $k0, MG_SCRATCH_MEM                     ## L:738  |    174 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  vmadh $v29, $v01, $v14.h0                          ## L:730  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v14.h1                          ## L:731  |    175 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  lqv $v22, 0, 0, $at                                ## L:738  |      ^ | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  ldv $v08, 8, 56, $fp                               ## L:490  |    176 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:739  |    177 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  vmadh $v29, $v03, $v14.h1                          ## L:731  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v29, $v06, $v14.h2                          ## L:732  |    178 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  lqv $v23, 0, 0, $at                                ## L:739  |      ^ | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  vmadh $v29, $v05, $v14.h2                          ## L:732  |    179 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ssv $v24, 14, 20, $t1                              ## L:711  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  lbu $fp, %lo(LIGHTING_COUNT + 0)                   ## L:747  |    180 | u8 light_count = load(LIGHTING_COUNT);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:740  |    181 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:749  |    182 | u16 light_ptr = LIGHTING_LIGHTS;
  vmadn $v29, $v08, $v30.e2                          ## L:733  |      ^ | vpos = vmvp3 +* 0x20;
  lqv $v24, 0, 0, $at                                ## L:740  |    183 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  ssv $v25, 14, 22, $t1                              ## L:711  |    184 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  nor $t7, $s2, $s3                                  ## L:464  |    185 | result = t0 ~| t1;
  ssv $v25, 6, 22, $s7                               ## L:710  |    186 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadh $v14, $v07, $v30.e2                          ## L:733  |      ^ | vpos = vmvp3 +* 0x20;
  vmudh $v29, $v24, $v30.e7                          ## L:742  |    187 | VTEMP = vemissive:sint * 1;
  beq $fp, $zero, LIGHT_END                          ## L:750  |      ^ | if (light_count == 0) goto LIGHT_END;
  vmacf $v17, $v17, $v23.v                           ## L:743  |   *189 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  ldv $v25, 0, 8, $s0                                ## L:755  |    190 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  addiu $s1, $s0, 14                                 ## L:753  |    191 | u16 amb_ptr = light_ptr + 14;
  ldv $v25, 8, 8, $s0                                ## L:755  |    192 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  lh $s2, 6($s0)                                     ## L:760  |    193 | s16 light_w = load(light_ptr, 6);
  ldv $v26, 0, 0, $s0                                ## L:756  |    194 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v24, 0, 0, $s1                                ## L:754  |    195 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v26, 8, 0, $s0                                ## L:756  |    196 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  vmulf $v25, $v25, $v22.v                           ## L:758  |      ^ | vlcol:sfract *= vdiffuse;
  beq $s2, $zero, LABEL_gl_pipeline_0005             ## L:761  |    197 | if (light_w != 0)
  ldv $v24, 8, 0, $s1                                ## L:754  |   *199 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  vsubc $v26, $v26, $v14.v                           ## L:775  |    200 | vlpos -= vpos;
  vmudh $v04, $v26, $v26.v                           ## L:776  | ***204 | vec32 vsqdist = vlpos * vlpos;
  vsar $v04, COP2_ACC_MD                             ## L:776  |    205 | vec32 vsqdist = vlpos * vlpos;
  vsar $v03, COP2_ACC_HI                             ## L:776  |    206 | vec32 vsqdist = vlpos * vlpos;
  addiu $s3, $s0, 20                                 ## L:769  |      ^ | u16 att_ptr = light_ptr + 20;
  vaddc $v06, $v04, $v04.h1                          ## L:777  |  **209 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v01, 0, 0, $s3                                ## L:771  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vadd $v05, $v03, $v03.h1                           ## L:777  |    210 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:778  |  **213 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:778  |    214 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v01, 8, 0, $s3                                ## L:771  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  addiu $s3, $s3, 6                                  ## L:772  | ***218 | att_ptr += 6;
  vrsqh $v05.e0, $v03.e0                             ## L:780  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:780  |    219 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v02, 0, 0, $s3                                ## L:773  |      ^ | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  ldv $v02, 8, 0, $s3                                ## L:773  |    220 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vrsqh $v05.e0, $v03.e4                             ## L:781  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:781  |    221 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:781  |    222 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:783  |  **225 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:783  |    226 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:783  |    227 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:783  |    228 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v26, $v06.h0                          ## L:785  |    229 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v26, $v26, $v05.h0                          ## L:785  |    230 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:787  |    231 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:787  |    232 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:788  |    233 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:788  |    234 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:788  |    235 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:788  |    236 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:789  |    237 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:789  |    238 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:789  |    239 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:789  |    240 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vrcph $v01.e0, $v01.e0                             ## L:791  | ***244 | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:791  |    245 | vatten.x = invert_half(vatten).x;
  vrcph $v01.e0, $v01.e4                             ## L:792  |  **248 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:792  |    249 | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:792  |    250 | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v25, $v02.h0                          ## L:794  |  **253 | vlcol *= vatten.xxxxXXXX;
  vmadh $v25, $v25, $v01.h0                          ## L:794  |    254 | vlcol *= vatten.xxxxXXXX;
  vmudm $v29, $v23, $v02.h0                          ## L:795  |    255 | vambient *= vatten.xxxxXXXX;
  vmadh $v23, $v23, $v01.h0                          ## L:795  |    256 | vambient *= vatten.xxxxXXXX;
  LABEL_gl_pipeline_0005:
  addiu $fp, $fp, 65535                              ## L:823  |      ^ | light_count -= 1;
  vmulf $v26, $v26, $v15.v                           ## L:810  |    257 | vlpos *= vnorm:sfract;
  vmulf $v29, $v25, $v26.h0                          ## L:811  | ***261 | VTEMP = vlcol:sfract * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v25, $v26.h1                          ## L:812  |    262 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v25, $v25, $v26.h2                          ## L:813  |    263 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vmulf $v24, $v24, $v23.v                           ## L:816  |    264 | vlambient = vlambient:sfract * vambient:sfract;
  vge $v25, $v25, $v00                               ## L:814  |  **267 | vlcol = max(vlcol, VZERO);
  vmudh $v29, $v17, $v30.e7                          ## L:818  |    268 | VTEMP = vrgba:sint * 1;
  addiu $s0, $s0, 32                                 ## L:822  |      ^ | light_ptr += 32;
  vmadh $v29, $v24, $v30.e7                          ## L:819  |    269 | VTEMP = vlambient:sint +* 1;
  bne $fp, $zero, LIGHT_LOOP                         ## L:824  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vmadh $v17, $v25, $v30.e7                          ## L:820  |  **272 | vrgba = vlcol:sint +* 1;
  LIGHT_END:
  or $t2, $zero, $s7                                 ## L:926  |    273 | prev_out0 = vtx_out0_ptr;
  vmov $v17.e3, $v22.e3                              ## L:828  |      ^ | vrgba.w = vdiffuse.w;
  vmov $v17.e7, $v22.e7                              ## L:829  |    274 | vrgba.W = vdiffuse.W;
  lbu $s1, %lo(FOG_MASK + 0)                         ## L:835  |      ^ | u8 fog_mask = load(FOG_MASK);
  ctc2 $s1, $vcc                                     ## L:836  |  **277 | set_vcc(fog_mask);
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:844  |    278 | u16 addr = MATRICES_MVP;
  vmrg $v17, $v17, $v21.h2                           ## L:837  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v22, $v14, $v14.v                           ## L:848  |    279 | vec32 vsqdist = vpos * vpos;
  ldv $v02, 8, 32, $s1                               ## L:484  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vsar $v22, COP2_ACC_MD                             ## L:848  |    280 | vec32 vsqdist = vpos * vpos;
  addiu $s7, $s7, 80                                 ## L:930  |      ^ | vtx_out0_ptr += 80;
  ldv $v07, 8, 24, $s1                               ## L:489  |    281 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vsar $v21, COP2_ACC_HI                             ## L:848  |      ^ | vec32 vsqdist = vpos * vpos;
  ori $at, $zero, %lo(TEXTURE_MATRIX)                ## L:897  |    282 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v27, 0, 16, $at                               ## L:901  |    283 | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  ldv $v04, 0, 40, $s1                               ## L:486  |    284 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vaddc $v24, $v22, $v22.h1                          ## L:849  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v08, 8, 56, $s1                               ## L:490  |    285 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vadd $v23, $v21, $v21.h1                           ## L:849  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v22, $v24, $v22.h2                          ## L:850  |  **288 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v01, 0, 0, $s1                                ## L:483  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vadd $v21, $v23, $v21.h2                           ## L:850  |    289 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v23.e0, $v21.e0                             ## L:852  | ***293 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v24.e0, $v22.e0                             ## L:852  |    294 | vtmp.x = invert_half_sqrt(vsqdist).x;
  addu $s4, $s4, $t9                                 ## L:928  |      ^ | vtx_in0_ptr += vtx_size2;
  vrsqh $v23.e0, $v21.e4                             ## L:853  |    295 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v03, 0, 8, $s1                                ## L:485  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrsql $v24.e4, $v22.e4                             ## L:853  |    296 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v23.e4, $v00.e0                             ## L:853  |    297 | vtmp.X = invert_half_sqrt(vsqdist).X;
  or $t3, $zero, $t1                                 ## L:927  |      ^ | prev_out1 = vtx_out1_ptr;
  ldv $v27, 8, 16, $at                               ## L:901  |  **300 | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  vmudm $v29, $v14, $v24.h0                          ## L:855  |      ^ | vpos *= vtmp.xxxxXXXX;
  vmadh $v14, $v14, $v23.h0                          ## L:855  |    301 | vpos *= vtmp.xxxxXXXX;
  addu $t0, $t0, $t9                                 ## L:929  |      ^ | vtx_in1_ptr += vtx_size2;
  LOAD_COLOR938: llv $v18, 4, 0, $t0                 ## L:938  | ***305 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmulf $v23, $v14, $v15.v                           ## L:858  |      ^ | vec16 vdot = vpos:sfract * vnorm:sfract;
  LOAD_COLOR937: llv $v18, 0, 0, $s4                 ## L:937  |    306 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  ldv $v03, 8, 8, $s1                                ## L:485  |  **309 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vadd $v24, $v23, $v23.h1                           ## L:859  |      ^ | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v23, $v24, $v23.h2                           ## L:860  | ***313 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  vor $v24, $v00, $v00                               ## L:865  |    314 | vec16 vzunit = VZERO;
  ldv $v06, 8, 48, $s1                               ## L:488  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vlt $v23, $v23, $v00                               ## L:862  |  **317 | vdot = min(vdot, VZERO);
  vsubc $v23, $v00, $v23.v                           ## L:863  | ***321 | vdot = VZERO - vdot;
  vmov $v24.e2, $v12.e3                              ## L:866  |    322 | vzunit.z = vnormmask.w;
  vmov $v24.e6, $v12.e3                              ## L:867  |    323 | vzunit.Z = vnormmask.w;
  vmulf $v29, $v15, $v23.h0                          ## L:869  |   *325 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v15, $v23.h0                          ## L:870  |    326 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  vmadh $v29, $v14, $v30.e7                          ## L:871  |    327 | VTEMP = vpos:sint +* 1;
  LOAD_NORMAL933: lsv $v15, 0, 0, $s4                ## L:933  |      ^ | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  vmadh $v29, $v24, $v30.e7                          ## L:872  |    328 | VTEMP = vzunit:sint +* 1;
  vsar $v24, COP2_ACC_MD                             ## L:875  |    329 | vec32 vrefl = get_acc();
  vsar $v23, COP2_ACC_HI                             ## L:875  |    330 | vec32 vrefl = get_acc();
  ldv $v28, 0, 40, $at                               ## L:902  |      ^ | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  ldv $v28, 8, 40, $at                               ## L:902  |    331 | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  LOAD_NORMAL934: lsv $v15, 8, 0, $t0                ## L:934  |   *333 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  vmudl $v29, $v24, $v24.v                           ## L:877  |      ^ | vsqdist = vrefl * vrefl;
  vmadm $v29, $v23, $v24.v                           ## L:877  |    334 | vsqdist = vrefl * vrefl;
  vmadn $v22, $v24, $v23.v                           ## L:877  |    335 | vsqdist = vrefl * vrefl;
  vmadh $v21, $v23, $v23.v                           ## L:877  |    336 | vsqdist = vrefl * vrefl;
  vaddc $v26, $v22, $v22.h1                          ## L:878  |  **339 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v25, $v21, $v21.h1                           ## L:878  |    340 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vxor $v16, $v00, $v30.e0                           ## L:884  |    341 | vtex = 0x80;
  vaddc $v22, $v26, $v22.h2                          ## L:879  |   *343 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v21, $v25, $v21.h2                           ## L:879  |    344 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v25.e0, $v21.e0                             ## L:881  | ***348 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v26.e0, $v22.e0                             ## L:881  |    349 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v25.e0, $v21.e4                             ## L:882  |    350 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v26.e4, $v22.e4                             ## L:882  |    351 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v25.e4, $v00.e0                             ## L:882  |    352 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudh $v29, $v16, $v30.e7                          ## L:885  |    353 | VTEMP = vtex:sint * 1;
  vmadl $v29, $v24, $v26.h0                          ## L:886  |   *355 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  vmadm $v29, $v23, $v26.h0                          ## L:886  |    356 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v26, 0, 32, $at                               ## L:900  |      ^ | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  vmadn $v29, $v24, $v25.h0                          ## L:886  |    357 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v24, 0, 24, $at                               ## L:898  |      ^ | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  vmadh $v16, $v23, $v25.h0                          ## L:886  |    358 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v24, 8, 24, $at                               ## L:898  |      ^ | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:897  |    359 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v26, 8, 32, $at                               ## L:900  |    360 | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:897  |    361 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v25, 0, 8, $at                                ## L:899  |    362 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  ldv $v25, 8, 8, $at                                ## L:899  |    363 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  LOAD_POSITION935: ldv $v14, 0, 0, $s4              ## L:935  |    364 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vmudn $v29, $v24, $v16.h0                          ## L:904  |      ^ | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadh $v29, $v23, $v16.h0                          ## L:904  |    365 | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadn $v29, $v26, $v16.h1                          ## L:905  |    366 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  ldv $v02, 0, 32, $s1                               ## L:484  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v29, $v25, $v16.h1                          ## L:905  |    367 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  vmadn $v22, $v28, $v31.e7                          ## L:906  |    368 | vtex_transformed = vmt3 +* 0x100;
  ldv $v04, 8, 40, $s1                               ## L:486  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadh $v21, $v27, $v31.e7                          ## L:906  |    369 | vtex_transformed = vmt3 +* 0x100;
  ldv $v07, 0, 24, $s1                               ## L:489  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrcph $v23.e0, $v21.e3                             ## L:911  | ***373 | vinv_tex.x = invert_half(vtex_transformed).w;
  ldv $v01, 8, 0, $s1                                ## L:483  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v06, 0, 48, $s1                               ## L:488  |    374 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrcpl $v24.e0, $v22.e3                             ## L:911  |      ^ | vinv_tex.x = invert_half(vtex_transformed).w;
  vrcph $v23.e0, $v21.e7                             ## L:912  |    375 | vinv_tex.X = invert_half(vtex_transformed).W;
  addiu $t1, $t1, 80                                 ## L:931  |      ^ | vtx_out1_ptr += 80;
  vrcpl $v24.e4, $v22.e7                             ## L:912  |    376 | vinv_tex.X = invert_half(vtex_transformed).W;
  ori $at, $zero, %lo(TEXTURE_SIZE)                  ## L:918  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  vrcph $v23.e4, $v00.e7                             ## L:912  |    377 | vinv_tex.X = invert_half(vtex_transformed).W;
  ldv $v05, 8, 16, $s1                               ## L:487  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmudl $v29, $v22, $v24.h0                          ## L:914  |  **380 | vtex_transformed *= vinv_tex.xxxxXXXX;
  ldv $v05, 0, 16, $s1                               ## L:487  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadm $v29, $v21, $v24.h0                          ## L:914  |    381 | vtex_transformed *= vinv_tex.xxxxXXXX;
  ldv $v08, 0, 56, $s1                               ## L:490  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadn $v22, $v22, $v23.h0                          ## L:914  |    382 | vtex_transformed *= vinv_tex.xxxxXXXX;
  LOAD_POSITION936: ldv $v14, 8, 0, $t0              ## L:936  |      ^ | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v21, $v21, $v23.h0                          ## L:914  |    383 | vtex_transformed *= vinv_tex.xxxxXXXX;
  llv $v23, 0, 0, $at                                ## L:918  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  llv $v23, 8, 0, $at                                ## L:919  |    384 | vtex_size.XY = load(TEXTURE_SIZE).xy;
  ori $at, $zero, %lo(TEXTURE_OFFSET)                ## L:920  |    385 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 0, 0, $at                                ## L:920  |    386 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:921  |    387 | vtex_offset.XY = load(TEXTURE_OFFSET).xy;
  vmudh $v29, $v24, $v30.e7                          ## L:923  | ***391 | VTEMP = vtex_offset:sint * 1;
  sltu $at, $s4, $s6                                 ## L:940  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadn $v29, $v22, $v23.v                           ## L:924  |    392 | vtex = vtex_transformed +* vtex_size;
  bne $at, $zero, VERTEX_LOOP                        ## L:940  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v16, $v21, $v23.v                           ## L:924  |   *394 | vtex = vtex_transformed +* vtex_size;
  suv $v17, 0, 8, $t2                                ## L:954  |    395 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v19, 8, 0, $t3                                ## L:947  |    396 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sdv $v19, 0, 0, $t2                                ## L:946  |    397 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  suv $v17, 4, 8, $t3                                ## L:955  |    398 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sb $t7, 7($t3)                                     ## L:952  |    399 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  slv $v16, 0, 12, $t2                               ## L:957  |    400 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t4, 6($t2)                                     ## L:949  |    401 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t5, 6($t3)                                     ## L:950  |    402 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  slv $v16, 8, 12, $t3                               ## L:958  |    403 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  j RSPQ_Loop                                        ## L:959  |    404 | }
  sb $t6, 7($t2)                                     ## L:951  |   *406 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8

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