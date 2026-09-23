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
    MgVertexAttributeLoaders LOAD_COLOR608, LOAD_COLOR609, LOAD_COLOR937, LOAD_COLOR938
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD637, LOAD_TEXCOORD640
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:502  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:503  |      2 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v01, 0, 0, $at                                ## L:503  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:504  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:506  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:528  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:507  |      9 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:506  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  vsar $v04, COP2_ACC_MD                             ## L:506  |     10 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:520  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  ssv $v03, 0, 8, $at                                ## L:508  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:507  |     14 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v04, 0, 10, $at                               ## L:508  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:518  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:514  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:516  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:521  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:523  |     20 | u32 misalignment = buffer_offset & 0x7;
  addu $t0, $t1, $t3                                 ## L:524  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:525  |     22 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:526  |     23 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:529  |     24 | vtx_in0_ptr -= dma_size;
  addiu $t2, $zero, 12                               ## L:531  |     25 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  jal DMAExec                                        ## L:531  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:531  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:545  |     29 | u16 mvpaddr = MATRICES_MVP;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:550  |     30 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  addiu $sp, $zero, 64                               ## L:577  |     31 | u16 mat_ptr_ambient  = 0x40;
  ldv $v06, 0, 48, $t0                               ## L:488  |     32 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:485  |     33 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:485  |     34 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:483  |     35 | m0:sint   = load(address, 0x00).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:536  |     36 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ldv $v09, 0, 0, $at                                ## L:550  |     37 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:486  |     38 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:552  |     39 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:537  |     40 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v09, 8, 0, $at                                ## L:550  |     41 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:551  |     42 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:552  |     43 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:533  |     44 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v10, 8, 8, $at                                ## L:551  |     45 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:565  |     46 | u16 prev_out1 = MG_SCRATCH_MEM;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:554  |     47 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:489  |     48 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:490  |     49 | m3:ufract = load(address, 0x38).xyzwxyzw;
  lhu $s0, %lo(MATERIAL_FLAGS + 0)                   ## L:589  |     50 | u16 mat_flags = load(MATERIAL_FLAGS);
  sll $t9, $t8, 1                                    ## L:534  |     51 | vtx_size2 = vtx_size * 2;
  ldv $v12, 0, 0, $at                                ## L:554  |     52 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:555  |     53 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:555  |     54 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:554  |     55 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:487  |     56 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:490  |     57 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:487  |     58 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:581  |     59 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  andi $s1, $s0, 1                                   ## L:590  |     60 | u16 tmp = mat_flags & 0x1;
  ldv $v01, 8, 0, $t0                                ## L:483  |     61 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v21, 0, 0, $at                                ## L:581  |     62 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v21, 8, 0, $at                                ## L:581  |     63 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:489  |     64 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:484  |     65 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:582  |     66 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:582  |     67 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:582  |     68 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  addiu $k1, $zero, 48                               ## L:576  |     69 | u16 mat_ptr_diffuse  = 0x30;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:583  |     70 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:484  |     71 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:583  |     72 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:583  |     73 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:488  |     74 | m2:ufract = load(address, 0x30).xyzwxyzw;
  addu $t1, $s4, $t8                                 ## L:561  |     75 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:585  |     76 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  sqv $v22, 0, 64, $at                               ## L:586  |     77 | store(vambient, MG_SCRATCH_MEM, 0x40);
  ldv $v04, 8, 40, $t0                               ## L:486  |     78 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:564  |     79 | u16 prev_out0 = MG_SCRATCH_MEM;
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:558  |     80 | u8 has_points = load(LIGHTING_HAS_POINTS);
  sqv $v21, 0, 48, $at                               ## L:585  |     81 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  addiu $fp, $zero, 80                               ## L:578  |     82 | u16 mat_ptr_emissive = 0x50;
  addiu $t2, $s7, 40                                 ## L:562  |     83 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  beq $s1, $zero, LABEL_gl_pipeline_0001             ## L:591  |     84 | if (tmp != 0) {
  sqv $v23, 0, 80, $at                               ## L:587  |    *86 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  addiu $k1, $zero, 96                               ## L:592  |     87 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s1, $s0, 2                                   ## L:594  |     88 | tmp = mat_flags & 0x2;
  beq $s1, $zero, LABEL_gl_pipeline_0002             ## L:595  |     89 | if (tmp != 0 ) {
  nop                                                ## L:595  |    *91 | if (tmp != 0 ) {
  addiu $sp, $zero, 96                               ## L:596  |     92 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s1, $s0, 4                                   ## L:598  |     93 | tmp = mat_flags & 0x4;
  beq $s1, $zero, LABEL_gl_pipeline_0003             ## L:599  |     94 | if (tmp != 0) {
  nop                                                ## L:599  |    *96 | if (tmp != 0) {
  addiu $fp, $zero, 96                               ## L:600  |     97 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_NORMAL604: lsv $v15, 0, 0, $s4                ## L:604  |     98 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  LOAD_NORMAL605: lsv $v15, 8, 0, $t1                ## L:605  |     99 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  LOAD_COLOR608: llv $v18, 0, 0, $s4                 ## L:608  |    100 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_COLOR609: llv $v18, 4, 0, $t1                 ## L:609  |    101 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  LOAD_POSITION606: ldv $v14, 0, 0, $s4              ## L:606  |    102 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POSITION607: ldv $v14, 8, 0, $t1              ## L:607  |    103 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  vand $v15, $v12, $v15.h0                           ## L:644  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:631  |    104 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v18, 0, 0, $at                                ## L:631  |   *106 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  luv $v18, 0, 0, $at                                ## L:632  |    107 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v15, $v15, $v13.v                           ## L:645  |      ^ | vnorm *= vnormfactor;
  suv $v17, 0, 8, $t3                                ## L:625  |    108 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmudn $v29, $v02, $v14.h0                          ## L:648  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v14.h0                          ## L:648  |    109 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:633  |      ^ | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmadn $v29, $v04, $v14.h1                          ## L:649  |    110 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  slv $v16, 0, 12, $t3                               ## L:628  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sqv $v18, 0, 96, $at                               ## L:633  |    111 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmadh $v29, $v03, $v14.h1                          ## L:649  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  suv $v17, 4, 8, $t4                                ## L:626  |    112 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadn $v29, $v06, $v14.h2                          ## L:650  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v14.h2                          ## L:650  |    113 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:673  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  ldv $v21, 0, 0, $at                                ## L:673  |    114 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  sdv $v19, 8, 0, $t4                                ## L:618  |    115 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  ldv $v21, 8, 0, $at                                ## L:673  |    116 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v20, $v08, $v30.e2                          ## L:651  |      ^ | vpos_clip = vmvp3 +* 0x20;
  sdv $v19, 0, 0, $t3                                ## L:617  |    117 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmadh $v19, $v07, $v30.e2                          ## L:651  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmulf $v29, $v09, $v15.h0                          ## L:653  |    118 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:688  |      ^ | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  slv $v16, 8, 12, $t4                               ## L:629  |    119 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmacf $v29, $v10, $v15.h1                          ## L:654  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  LOAD_TEXCOORD637: llv $v16, 0, 0, $s4              ## L:637  |    120 | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  vmacf $v15, $v11, $v15.h2                          ## L:655  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  sb $t5, 6($t3)                                     ## L:620  |    121 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vmudn $v23, $v20, $v21.v                           ## L:674  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v22, $v19, $v21.v                           ## L:674  |    122 | vec32 vguard = vpos_clip * vclip_factors;
  LOAD_TEXCOORD640: llv $v16, 8, 0, $t1              ## L:640  |      ^ | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  sdv $v19, 8, 24, $t2                               ## L:684  |    123 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v19, 0, 24, $s7                               ## L:683  |   *125 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sb $t6, 6($t4)                                     ## L:621  |    126 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vch $v29, $v22, $v22.h3                            ## L:675  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v20, 8, 32, $t2                               ## L:684  |    127 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vcl $v29, $v23, $v23.h3                            ## L:675  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $s0, $vcc                                     ## L:675  |    128 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  ldv $v22, 0, 0, $at                                ## L:688  |    129 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  vch $v29, $v19, $v19.h3                            ## L:679  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ldv $v22, 8, 0, $at                                ## L:688  |    130 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  srl $s1, $s0, 4                                    ## L:469  |    131 | u16 codes2 = codes >> 4;
  vcl $v29, $v20, $v20.h3                            ## L:679  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  andi $s2, $s0, 1799                                ## L:449  |    132 | t0 = codes & 0x707;
  srl $s3, $s2, 5                                    ## L:450  |    133 | t1 = t0 >> 5;
  or $t5, $s2, $s3                                   ## L:457  |    134 | result = t0 | t1;
  vaddc $v23, $v20, $v22.h1                          ## L:689  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  vadd $v21, $v19, $v22.h0                           ## L:690  |    135 | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s2, $s1, 1799                                ## L:449  |      ^ | t0 = codes & 0x707;
  cfc2 $s1, $vcc                                     ## L:679  |    136 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:697  |    137 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  srl $s3, $s2, 5                                    ## L:450  |    138 | t1 = t0 >> 5;
  vmudn $v29, $v23, $v22.h2                          ## L:692  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v21, $v21, $v22.h2                          ## L:693  |    139 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v22, 0, 0, $at                                ## L:697  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:697  |    140 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  or $t6, $s2, $s3                                   ## L:457  |    141 | result = t0 | t1;
  andi $s3, $s1, 1799                                ## L:449  |    142 | t0 = codes & 0x707;
  sdv $v20, 0, 32, $s7                               ## L:683  |    143 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vge $v21, $v00, $v21.h2                            ## L:694  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v20, $v20, $v22.h3                          ## L:700  |    144 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v23, 0, 8, $at                                ## L:698  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  sb $k0, 7($t4)                                     ## L:623  |    145 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmadm $v19, $v19, $v22.h3                          ## L:700  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  srl $s2, $s1, 4                                    ## L:476  |    146 | u16 codes2 = codes >> 4;
  vmadn $v20, $v00, $v00                             ## L:700  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  sb $t7, 7($t3)                                     ## L:622  |    147 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  srl $s5, $s3, 5                                    ## L:450  |    148 | t1 = t0 >> 5;
  ssv $v19, 6, 16, $s7                               ## L:703  |    149 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcph $v24.e3, $v19.e3                             ## L:707  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcpl $v25.e3, $v20.e3                             ## L:707  |    150 | vinvw.w = invert_half(vpos_clip).w;
  ssv $v20, 6, 18, $s7                               ## L:703  |      ^ | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  nor $t7, $s3, $s5                                  ## L:464  |    151 | result = t0 ~| t1;
  vrcph $v24.e3, $v19.e7                             ## L:708  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcpl $v25.e7, $v20.e7                             ## L:708  |    152 | vinvw.W = invert_half(vpos_clip).W;
  andi $s3, $s2, 1799                                ## L:449  |      ^ | t0 = codes & 0x707;
  ssv $v19, 14, 16, $t2                              ## L:704  |    153 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v20, 14, 18, $t2                              ## L:704  |    154 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v24.e7, $v00.e7                             ## L:708  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ldv $v23, 8, 8, $at                                ## L:698  |    155 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  srl $s5, $s3, 5                                    ## L:450  |    156 | t1 = t0 >> 5;
  vmudl $v29, $v20, $v25.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  nor $k0, $s3, $s5                                  ## L:464  |    157 | result = t0 ~| t1;
  vmadm $v29, $v19, $v25.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v24, 14, 20, $t2                              ## L:711  |    158 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v20, $v20, $v24.h3                          ## L:713  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadh $v19, $v19, $v24.h3                          ## L:713  |    159 | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v24, 6, 20, $s7                               ## L:710  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ssv $v25, 14, 22, $t2                              ## L:711  |    160 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  ssv $v25, 6, 22, $s7                               ## L:710  |    161 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmudh $v29, $v23, $v30.e7                          ## L:717  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v20, $v20, $v22.v                           ## L:718  |    162 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_gl_pipeline_0004             ## L:724  |      ^ | if (has_points != 0)
  vmadh $v19, $v19, $v22.v                           ## L:718  |   *164 | vpos_clip = vpos_clip +* vviewscale;
  ori $s0, $zero, %lo(MATRICES_MV)                   ## L:727  |    165 | u16 addr = MATRICES_MV;
  ldv $v01, 0, 0, $s0                                ## L:483  |    166 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 0, 32, $s0                               ## L:484  |    167 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 0, 40, $s0                               ## L:486  |    168 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v02, 8, 32, $s0                               ## L:484  |    169 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 8, 40, $s0                               ## L:486  |    170 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 8, 0, $s0                                ## L:483  |    171 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v06, 0, 48, $s0                               ## L:488  |    172 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v03, 0, 8, $s0                                ## L:485  |    173 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v03, 8, 8, $s0                                ## L:485  |    174 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudn $v29, $v02, $v14.h0                          ## L:730  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v06, 8, 48, $s0                               ## L:488  |    175 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v29, $v01, $v14.h0                          ## L:730  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v05, 0, 16, $s0                               ## L:487  |    176 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v08, 0, 56, $s0                               ## L:490  |    177 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadn $v29, $v04, $v14.h1                          ## L:731  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v05, 8, 16, $s0                               ## L:487  |    178 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v29, $v03, $v14.h1                          ## L:731  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v08, 8, 56, $s0                               ## L:490  |    179 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v07, 0, 24, $s0                               ## L:489  |    180 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v07, 8, 24, $s0                               ## L:489  |    181 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v29, $v06, $v14.h2                          ## L:732  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v14.h2                          ## L:732  |    182 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e2                          ## L:733  |    183 | vpos = vmvp3 +* 0x20;
  vmadh $v14, $v07, $v30.e2                          ## L:733  |   *185 | vpos = vmvp3 +* 0x20;
  LABEL_gl_pipeline_0004:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:736  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v17, 0, 0, $at                                ## L:736  |    186 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v17, 8, 0, $at                                ## L:736  |    187 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:738  |    188 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  lqv $v22, 0, 0, $at                                ## L:738  |    189 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:739  |    190 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  lqv $v23, 0, 0, $at                                ## L:739  |    191 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  addiu $at, $fp, MG_SCRATCH_MEM                     ## L:740  |    192 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lqv $v24, 0, 0, $at                                ## L:740  |    193 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lbu $s0, %lo(LIGHTING_COUNT + 0)                   ## L:747  |    194 | u8 light_count = load(LIGHTING_COUNT);
  ori $s1, $zero, %lo(LIGHTING_LIGHTS)               ## L:749  |  **197 | u16 light_ptr = LIGHTING_LIGHTS;
  vmudh $v29, $v24, $v30.e7                          ## L:742  |      ^ | VTEMP = vemissive:sint * 1;
  beq $s0, $zero, LIGHT_END                          ## L:750  |    198 | if (light_count == 0) goto LIGHT_END;
  vmacf $v17, $v17, $v23.v                           ## L:743  |   *200 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  lh $s3, 6($s1)                                     ## L:760  |    201 | s16 light_w = load(light_ptr, 6);
  ldv $v25, 0, 8, $s1                                ## L:755  |    202 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  addiu $s2, $s1, 14                                 ## L:753  |    203 | u16 amb_ptr = light_ptr + 14;
  ldv $v25, 8, 8, $s1                                ## L:755  |    204 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v24, 0, 0, $s2                                ## L:754  |    205 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $s2                                ## L:754  |    206 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v26, 0, 0, $s1                                ## L:756  |    207 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v26, 8, 0, $s1                                ## L:756  |    208 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $s3, $zero, LABEL_gl_pipeline_0006             ## L:761  |    209 | if (light_w != 0)
  vmulf $v25, $v25, $v22.v                           ## L:758  |   *211 | vlcol:sfract *= vdiffuse;
  addiu $s5, $s1, 20                                 ## L:769  |    212 | u16 att_ptr = light_ptr + 20;
  vsubc $v26, $v26, $v14.v                           ## L:775  |      ^ | vlpos -= vpos;
  ldv $v01, 0, 0, $s5                                ## L:771  |    213 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  ldv $v01, 8, 0, $s5                                ## L:771  |  **216 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vmudh $v04, $v26, $v26.v                           ## L:776  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vsar $v04, COP2_ACC_MD                             ## L:776  |    217 | vec32 vsqdist = vlpos * vlpos;
  vsar $v03, COP2_ACC_HI                             ## L:776  |    218 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v06, $v04, $v04.h1                          ## L:777  |  **221 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  addiu $s5, $s5, 6                                  ## L:772  |      ^ | att_ptr += 6;
  ldv $v02, 0, 0, $s5                                ## L:773  |    222 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vadd $v05, $v03, $v03.h1                           ## L:777  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:778  |  **225 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:778  |    226 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:780  | ***230 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:780  |    231 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:781  |    232 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:781  |    233 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:781  |    234 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:783  |  **237 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:783  |    238 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:783  |    239 | vec32 vdist = vtmp * vsqdist;
  ldv $v02, 8, 0, $s5                                ## L:773  |      ^ | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vmadh $v07, $v05, $v03.v                           ## L:783  |    240 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v26, $v06.h0                          ## L:785  |    241 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v26, $v26, $v05.h0                          ## L:785  |    242 | vlpos *= vtmp.xxxxXXXX;
  ori $s5, $zero, %lo(MATRICES_MVP)                  ## L:804  |      ^ | u16 addr = MATRICES_MVP;
  ldv $v05, 0, 16, $s5                               ## L:487  |    243 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmudn $v29, $v02, $v30.e7                          ## L:787  |      ^ | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:787  |    244 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:788  |    245 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:788  |    246 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:788  |    247 | VTEMP = vdist +* vatten.yyyyYYYY;
  ldv $v08, 8, 56, $s5                               ## L:490  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadh $v29, $v07, $v01.h1                          ## L:788  |    248 | VTEMP = vdist +* vatten.yyyyYYYY;
  ldv $v07, 8, 24, $s5                               ## L:489  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadl $v29, $v04, $v02.h2                          ## L:789  |    249 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:789  |    250 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v05, 8, 16, $s5                               ## L:487  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadn $v02, $v04, $v01.h2                          ## L:789  |    251 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:789  |    252 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v03, 8, 8, $s5                                ## L:485  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrcph $v01.e0, $v01.e0                             ## L:791  | ***256 | vatten.x = invert_half(vatten).x;
  ldv $v06, 0, 48, $s5                               ## L:488  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v07, 0, 24, $s5                               ## L:489  |    257 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrcpl $v02.e0, $v02.e0                             ## L:791  |      ^ | vatten.x = invert_half(vatten).x;
  ldv $v03, 0, 8, $s5                                ## L:485  |  **260 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrcph $v01.e0, $v01.e4                             ## L:792  |      ^ | vatten.X = invert_half(vatten).X;
  ldv $v06, 8, 48, $s5                               ## L:488  |    261 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vrcpl $v02.e4, $v02.e4                             ## L:792  |      ^ | vatten.X = invert_half(vatten).X;
  ldv $v04, 8, 40, $s5                               ## L:486  |    262 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrcph $v01.e4, $v00.e4                             ## L:792  |      ^ | vatten.X = invert_half(vatten).X;
  ldv $v08, 0, 56, $s5                               ## L:490  |  **265 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudm $v29, $v25, $v02.h0                          ## L:794  |      ^ | vlcol *= vatten.xxxxXXXX;
  vmadh $v25, $v25, $v01.h0                          ## L:794  |    266 | vlcol *= vatten.xxxxXXXX;
  ldv $v04, 0, 40, $s5                               ## L:486  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmudm $v29, $v23, $v02.h0                          ## L:795  |    267 | vambient *= vatten.xxxxXXXX;
  ldv $v02, 8, 32, $s5                               ## L:484  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v02, 0, 32, $s5                               ## L:484  |    268 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v23, $v23, $v01.h0                          ## L:795  |      ^ | vambient *= vatten.xxxxXXXX;
  ldv $v01, 0, 0, $s5                                ## L:483  |    269 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 8, 0, $s5                                ## L:483  |    270 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LABEL_gl_pipeline_0006:
  vmulf $v26, $v26, $v15.v                           ## L:810  |      ^ | vlpos *= vnorm:sfract;
  addiu $s1, $s1, 32                                 ## L:822  | ***274 | light_ptr += 32;
  vmulf $v29, $v25, $v26.h0                          ## L:811  |      ^ | VTEMP = vlcol:sfract * vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v25, $v26.h1                          ## L:812  |    275 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v25, $v25, $v26.h2                          ## L:813  |    276 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vmulf $v24, $v24, $v23.v                           ## L:816  |    277 | vlambient = vlambient:sfract * vambient:sfract;
  vge $v25, $v25, $v00                               ## L:814  |  **280 | vlcol = max(vlcol, VZERO);
  vmudh $v29, $v17, $v30.e7                          ## L:818  |    281 | VTEMP = vrgba:sint * 1;
  addiu $s0, $s0, 65535                              ## L:823  |      ^ | light_count -= 1;
  vmadh $v29, $v24, $v30.e7                          ## L:819  |    282 | VTEMP = vlambient:sint +* 1;
  bne $s0, $zero, LIGHT_LOOP                         ## L:824  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vmadh $v17, $v25, $v30.e7                          ## L:820  |  **285 | vrgba = vlcol:sint +* 1;
  LIGHT_END:
  vmov $v17.e3, $v22.e3                              ## L:828  |    286 | vrgba.w = vdiffuse.w;
  lbu $s2, %lo(FOG_MASK + 0)                         ## L:835  |      ^ | u8 fog_mask = load(FOG_MASK);
  ori $at, $zero, %lo(TEXTURE_MATRIX)                ## L:897  |    287 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v24, 0, 24, $at                               ## L:898  |    288 | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  vmov $v17.e7, $v22.e7                              ## L:829  |      ^ | vrgba.W = vdiffuse.W;
  ldv $v23, 0, 0, $at                                ## L:897  |    289 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v26, 0, 32, $at                               ## L:900  |    290 | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  ldv $v25, 0, 8, $at                                ## L:899  |    291 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  ldv $v24, 8, 24, $at                               ## L:898  |    292 | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  ldv $v26, 8, 32, $at                               ## L:900  |    293 | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:897  |    294 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  addu $s4, $s4, $t9                                 ## L:928  |    295 | vtx_in0_ptr += vtx_size2;
  ldv $v28, 0, 40, $at                               ## L:902  |    296 | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  ctc2 $s2, $vcc                                     ## L:836  |    297 | set_vcc(fog_mask);
  ldv $v25, 8, 8, $at                                ## L:899  |    298 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  vmrg $v17, $v17, $v21.h2                           ## L:837  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudn $v29, $v24, $v16.h0                          ## L:904  |    299 | VTEMP = vmt0  * vtex.xxxxXXXX;
  ldv $v27, 0, 16, $at                               ## L:901  |      ^ | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  ldv $v28, 8, 40, $at                               ## L:902  |    300 | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  ldv $v27, 8, 16, $at                               ## L:901  |    301 | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  vmadh $v29, $v23, $v16.h0                          ## L:904  |      ^ | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadn $v29, $v26, $v16.h1                          ## L:905  |    302 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  LOAD_NORMAL933: lsv $v15, 0, 0, $s4                ## L:933  |      ^ | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  or $t3, $zero, $s7                                 ## L:926  |    303 | prev_out0 = vtx_out0_ptr;
  vmadh $v29, $v25, $v16.h1                          ## L:905  |      ^ | VTEMP = vmt1 +* vtex.yyyyYYYY;
  vmadn $v22, $v28, $v31.e7                          ## L:906  |    304 | vtex_transformed = vmt3 +* 0x100;
  ori $at, $zero, %lo(TEXTURE_SIZE)                  ## L:918  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  or $t4, $zero, $t2                                 ## L:927  |    305 | prev_out1 = vtx_out1_ptr;
  vmadh $v21, $v27, $v31.e7                          ## L:906  |      ^ | vtex_transformed = vmt3 +* 0x100;
  vrcph $v23.e0, $v21.e3                             ## L:911  | ***309 | vinv_tex.x = invert_half(vtex_transformed).w;
  addu $t1, $t1, $t9                                 ## L:929  |      ^ | vtx_in1_ptr += vtx_size2;
  vrcpl $v24.e0, $v22.e3                             ## L:911  |    310 | vinv_tex.x = invert_half(vtex_transformed).w;
  vrcph $v23.e0, $v21.e7                             ## L:912  |    311 | vinv_tex.X = invert_half(vtex_transformed).W;
  LOAD_POSITION935: ldv $v14, 0, 0, $s4              ## L:935  |      ^ | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  vrcpl $v24.e4, $v22.e7                             ## L:912  |    312 | vinv_tex.X = invert_half(vtex_transformed).W;
  LOAD_COLOR937: llv $v18, 0, 0, $s4                 ## L:937  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_NORMAL934: lsv $v15, 8, 0, $t1                ## L:934  |    313 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  vrcph $v23.e4, $v00.e7                             ## L:912  |      ^ | vinv_tex.X = invert_half(vtex_transformed).W;
  vmudl $v29, $v22, $v24.h0                          ## L:914  |  **316 | vtex_transformed *= vinv_tex.xxxxXXXX;
  LOAD_COLOR938: llv $v18, 4, 0, $t1                 ## L:938  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  addiu $s7, $s7, 80                                 ## L:930  |    317 | vtx_out0_ptr += 80;
  vmadm $v29, $v21, $v24.h0                          ## L:914  |      ^ | vtex_transformed *= vinv_tex.xxxxXXXX;
  vmadn $v22, $v22, $v23.h0                          ## L:914  |    318 | vtex_transformed *= vinv_tex.xxxxXXXX;
  addiu $t2, $t2, 80                                 ## L:931  |      ^ | vtx_out1_ptr += 80;
  vmadh $v21, $v21, $v23.h0                          ## L:914  |    319 | vtex_transformed *= vinv_tex.xxxxXXXX;
  llv $v23, 0, 0, $at                                ## L:918  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  llv $v23, 8, 0, $at                                ## L:919  |    320 | vtex_size.XY = load(TEXTURE_SIZE).xy;
  ori $at, $zero, %lo(TEXTURE_OFFSET)                ## L:920  |    321 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 0, 0, $at                                ## L:920  |    322 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:921  |    323 | vtex_offset.XY = load(TEXTURE_OFFSET).xy;
  LOAD_POSITION936: ldv $v14, 8, 0, $t1              ## L:936  |    324 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  sltu $at, $s4, $s6                                 ## L:940  |  **327 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmudh $v29, $v24, $v30.e7                          ## L:923  |      ^ | VTEMP = vtex_offset:sint * 1;
  vmadn $v29, $v22, $v23.v                           ## L:924  |    328 | vtex = vtex_transformed +* vtex_size;
  bne $at, $zero, VERTEX_LOOP                        ## L:940  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v16, $v21, $v23.v                           ## L:924  |   *330 | vtex = vtex_transformed +* vtex_size;
  suv $v17, 4, 8, $t4                                ## L:955  |    331 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sdv $v19, 0, 0, $t3                                ## L:946  |    332 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t7, 7($t3)                                     ## L:951  |    333 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sb $t5, 6($t3)                                     ## L:949  |    334 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  suv $v17, 0, 8, $t3                                ## L:954  |    335 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v19, 8, 0, $t4                                ## L:947  |    336 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  slv $v16, 8, 12, $t4                               ## L:958  |    337 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $k0, 7($t4)                                     ## L:952  |    338 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  slv $v16, 0, 12, $t3                               ## L:957  |    339 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  j RSPQ_Loop                                        ## L:959  |    340 | }
  sb $t6, 6($t4)                                     ## L:950  |   *342 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10

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