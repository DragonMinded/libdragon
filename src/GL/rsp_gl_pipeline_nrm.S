## Auto-generated file, transpiled with RSPL
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
    MgVertexAttributeLoaders LOAD_POSITION589, LOAD_POSITION590, LOAD_POSITION914, LOAD_POSITION913
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_NORMAL587, LOAD_NORMAL588, LOAD_NORMAL912, LOAD_NORMAL911
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_COLOR591, LOAD_COLOR592, LOAD_COLOR915, LOAD_COLOR916
  MgEndVertexAttribute

  MgBeginVertexAttribute 3, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD620, LOAD_TEXCOORD623
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:485  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:486  |      2 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v01, 0, 0, $at                                ## L:486  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:487  |      4 | vec16 vinputs = load(inputs_ptr).xyzw;
  vmudh $v04, $v02, $v01.v                           ## L:489  |   ***8 | vec32 vproducts = vinputs * vvtx_size;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:511  |      ^ | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vsar $v03, COP2_ACC_HI                             ## L:489  |      9 | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:503  |      ^ | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:489  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:490  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v03, 0, 8, $at                                ## L:491  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:490  |     14 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v04, 0, 10, $at                               ## L:491  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:501  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:497  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:499  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:504  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:506  |     20 | u32 misalignment = buffer_offset & 0x7;
  addiu $t2, $zero, 12                               ## L:514  |     21 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addu $t0, $t1, $t3                                 ## L:507  |     22 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:508  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:509  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:512  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:514  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:514  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:528  |     29 | u16 mvpaddr = MATRICES_MVP;
  addu $s6, $s4, $t1                                 ## L:519  |     30 | vtx_in_end = vtx_in0_ptr + vertices_size;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:516  |     31 | vtx_size = load(MG_VERTEX_SIZE);
  lhu $s0, %lo(MATERIAL_FLAGS + 0)                   ## L:572  |     32 | u16 mat_flags = load(MATERIAL_FLAGS);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:533  |     33 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:533  |     34 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:534  |     35 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:533  |     36 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:470  |     37 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:534  |     38 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:535  |     39 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:535  |     40 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:537  |     41 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:538  |     42 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:473  |     43 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:520  |     44 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v03, 0, 8, $t0                                ## L:468  |     45 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:537  |     46 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  addu $t1, $s4, $t8                                 ## L:544  |     47 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ldv $v08, 8, 56, $t0                               ## L:473  |     48 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:538  |     49 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:537  |     50 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:469  |     51 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:564  |     52 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v21, 0, 0, $at                                ## L:564  |     53 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:466  |     54 | m0:sint   = load(address, 0x00).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:517  |     55 | vtx_size2 = vtx_size * 2;
  ldv $v01, 8, 0, $t0                                ## L:466  |     56 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:470  |     57 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v21, 8, 0, $at                                ## L:564  |     58 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:565  |     59 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:472  |     60 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:565  |     61 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:467  |     62 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:565  |     63 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:472  |     64 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:566  |     65 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:469  |     66 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:566  |     67 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:468  |     68 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:566  |     69 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:471  |     70 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:568  |     71 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  addiu $k1, $zero, 48                               ## L:559  |     72 | u16 mat_ptr_diffuse  = 0x30;
  ldv $v02, 8, 32, $t0                               ## L:467  |     73 | m0:ufract = load(address, 0x20).xyzwxyzw;
  andi $s1, $s0, 1                                   ## L:573  |     74 | u16 tmp = mat_flags & 0x1;
  ldv $v06, 8, 48, $t0                               ## L:471  |     75 | m2:ufract = load(address, 0x30).xyzwxyzw;
  sqv $v21, 0, 48, $at                               ## L:568  |     76 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:541  |     77 | u8 has_points = load(LIGHTING_HAS_POINTS);
  addiu $t2, $s7, 40                                 ## L:545  |     78 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:548  |     79 | u16 prev_out1 = MG_SCRATCH_MEM;
  sqv $v23, 0, 80, $at                               ## L:570  |     80 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  addiu $sp, $zero, 64                               ## L:560  |     81 | u16 mat_ptr_ambient  = 0x40;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:547  |     82 | u16 prev_out0 = MG_SCRATCH_MEM;
  addiu $fp, $zero, 80                               ## L:561  |     83 | u16 mat_ptr_emissive = 0x50;
  beq $s1, $zero, LABEL_gl_pipeline_0001             ## L:574  |     84 | if (tmp != 0) {
  sqv $v22, 0, 64, $at                               ## L:569  |    *86 | store(vambient, MG_SCRATCH_MEM, 0x40);
  addiu $k1, $zero, 96                               ## L:575  |     87 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s1, $s0, 2                                   ## L:577  |     88 | tmp = mat_flags & 0x2;
  beq $s1, $zero, LABEL_gl_pipeline_0002             ## L:578  |     89 | if (tmp != 0 ) {
  nop                                                ## L:578  |    *91 | if (tmp != 0 ) {
  addiu $sp, $zero, 96                               ## L:579  |     92 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s1, $s0, 4                                   ## L:581  |     93 | tmp = mat_flags & 0x4;
  beq $s1, $zero, LABEL_gl_pipeline_0003             ## L:582  |     94 | if (tmp != 0) {
  nop                                                ## L:582  |    *96 | if (tmp != 0) {
  addiu $fp, $zero, 96                               ## L:583  |     97 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_NORMAL587: lsv $v15, 0, 0, $s4                ## L:587  |     98 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  LOAD_NORMAL588: lsv $v15, 8, 0, $t1                ## L:588  |     99 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  LOAD_COLOR591: llv $v18, 0, 0, $s4                 ## L:591  |    100 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_COLOR592: llv $v18, 4, 0, $t1                 ## L:592  |    101 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  LOAD_POSITION589: ldv $v14, 0, 0, $s4              ## L:589  |    102 | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POSITION590: ldv $v14, 8, 0, $t1              ## L:590  |    103 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  vand $v15, $v12, $v15.h0                           ## L:627  |      ^ | vnorm = vnormmask & vnorm.xxxxXXXX;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:614  | ***107 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudn $v15, $v15, $v13.v                           ## L:628  |      ^ | vnorm *= vnormfactor;
  vmudn $v29, $v02, $v14.h0                          ## L:631  |    108 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  sdv $v18, 0, 0, $at                                ## L:614  |      ^ | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadh $v29, $v01, $v14.h0                          ## L:631  |    109 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadn $v29, $v04, $v14.h1                          ## L:632  |    110 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  suv $v17, 4, 8, $t4                                ## L:609  |      ^ | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadh $v29, $v03, $v14.h1                          ## L:632  |    111 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  sdv $v19, 8, 0, $t4                                ## L:601  |      ^ | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmadn $v29, $v06, $v14.h2                          ## L:633  |    112 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  suv $v17, 0, 8, $t3                                ## L:608  |      ^ | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmadh $v29, $v05, $v14.h2                          ## L:633  |    113 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  sdv $v19, 0, 0, $t3                                ## L:600  |      ^ | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t5, 6($t3)                                     ## L:603  |    114 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vmadn $v20, $v08, $v30.e2                          ## L:634  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmadh $v19, $v07, $v30.e2                          ## L:634  |    115 | vpos_clip = vmvp3 +* 0x20;
  slv $v16, 0, 12, $t3                               ## L:611  |      ^ | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  slv $v16, 8, 12, $t4                               ## L:612  |    116 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmulf $v29, $v09, $v15.h0                          ## L:636  |      ^ | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  sb $k0, 7($t4)                                     ## L:606  |    117 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmacf $v29, $v10, $v15.h1                          ## L:637  |      ^ | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  vmacf $v15, $v11, $v15.h2                          ## L:638  |    118 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  LOAD_TEXCOORD620: llv $v16, 0, 0, $s4              ## L:620  |      ^ | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  luv $v18, 0, 0, $at                                ## L:615  | ***122 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmudh $v22, $v15, $v15.v                           ## L:642  |      ^ | vec32 vsqdist = vnorm * vnorm;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:616  |    123 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vsar $v22, COP2_ACC_MD                             ## L:642  |      ^ | vec32 vsqdist = vnorm * vnorm;
  vsar $v21, COP2_ACC_HI                             ## L:642  |    124 | vec32 vsqdist = vnorm * vnorm;
  vaddc $v24, $v22, $v22.h1                          ## L:643  |  **127 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v23, $v21, $v21.h1                           ## L:643  |    128 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v22, $v24, $v22.h2                          ## L:644  |  **131 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  sqv $v18, 0, 96, $at                               ## L:616  |      ^ | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  sb $t6, 6($t4)                                     ## L:604  |    132 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  vadd $v21, $v23, $v21.h2                           ## L:644  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  LOAD_TEXCOORD623: llv $v16, 8, 0, $t1              ## L:623  |    133 | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  sdv $v19, 8, 24, $t2                               ## L:667  |  **136 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vrsqh $v25.e0, $v21.e0                             ## L:647  |      ^ | vinvdist.x = invert_half_sqrt(vsqdist).x;
  vrsql $v26.e0, $v22.e0                             ## L:647  |    137 | vinvdist.x = invert_half_sqrt(vsqdist).x;
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:656  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vrsqh $v25.e0, $v21.e4                             ## L:648  |    138 | vinvdist.X = invert_half_sqrt(vsqdist).X;
  ldv $v21, 0, 0, $at                                ## L:656  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vrsql $v26.e4, $v22.e4                             ## L:648  |    139 | vinvdist.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v25.e4, $v00.e0                             ## L:648  |    140 | vinvdist.X = invert_half_sqrt(vsqdist).X;
  ldv $v21, 8, 0, $at                                ## L:656  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  sdv $v19, 0, 24, $s7                               ## L:666  |  **143 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vmudm $v29, $v15, $v26.h0                          ## L:650  |      ^ | vnorm *= vinvdist.xxxxXXXX;
  vmadh $v15, $v15, $v25.h0                          ## L:650  |    144 | vnorm *= vinvdist.xxxxXXXX;
  sb $t7, 7($t3)                                     ## L:605  |      ^ | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:671  |    145 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  vmudn $v23, $v20, $v21.v                           ## L:657  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v20, 8, 32, $t2                               ## L:667  |    146 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vmadh $v22, $v19, $v21.v                           ## L:657  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v20, 0, 32, $s7                               ## L:666  | ***150 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vch $v29, $v22, $v22.h3                            ## L:658  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  ldv $v22, 0, 0, $at                                ## L:671  |    151 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  vcl $v29, $v23, $v23.h3                            ## L:658  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $s0, $vcc                                     ## L:658  |    152 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  ldv $v22, 8, 0, $at                                ## L:671  |    153 | vec16 vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:680  |    154 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  andi $s2, $s0, 1799                                ## L:432  |    155 | t0 = codes & 0x707;
  vch $v29, $v19, $v19.h3                            ## L:662  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v20, $v20.h3                            ## L:662  |    156 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  srl $s3, $s2, 5                                    ## L:433  |      ^ | t1 = t0 >> 5;
  vaddc $v23, $v20, $v22.h1                          ## L:672  |    157 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  srl $s1, $s0, 4                                    ## L:452  |      ^ | u16 codes2 = codes >> 4;
  or $t5, $s2, $s3                                   ## L:440  |    158 | result = t0 | t1;
  vadd $v21, $v19, $v22.h0                           ## L:673  |      ^ | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  andi $s2, $s1, 1799                                ## L:432  |    159 | t0 = codes & 0x707;
  srl $s3, $s2, 5                                    ## L:433  |    160 | t1 = t0 >> 5;
  cfc2 $s1, $vcc                                     ## L:662  |    161 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vmudn $v29, $v23, $v22.h2                          ## L:675  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  vmadh $v21, $v21, $v22.h2                          ## L:676  |    162 | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v22, 0, 0, $at                                ## L:680  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:680  |    163 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  or $t6, $s2, $s3                                   ## L:440  |    164 | result = t0 | t1;
  andi $s3, $s1, 1799                                ## L:432  |    165 | t0 = codes & 0x707;
  srl $s5, $s3, 5                                    ## L:433  |    166 | t1 = t0 >> 5;
  vge $v21, $v00, $v21.h2                            ## L:677  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  srl $s2, $s1, 4                                    ## L:459  |    167 | u16 codes2 = codes >> 4;
  vmudl $v20, $v20, $v22.h3                          ## L:683  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v19, $v19, $v22.h3                          ## L:683  |    168 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  nor $t7, $s3, $s5                                  ## L:447  |      ^ | result = t0 ~| t1;
  vmadn $v20, $v00, $v00                             ## L:683  |    169 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v23, 0, 8, $at                                ## L:681  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ssv $v19, 14, 16, $t2                              ## L:687  |  **172 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vrcph $v24.e3, $v19.e3                             ## L:690  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  andi $s3, $s2, 1799                                ## L:432  |    173 | t0 = codes & 0x707;
  vrcpl $v25.e3, $v20.e3                             ## L:690  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcph $v24.e3, $v19.e7                             ## L:691  |    174 | vinvw.W = invert_half(vpos_clip).W;
  ldv $v23, 8, 8, $at                                ## L:681  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  ssv $v19, 6, 16, $s7                               ## L:686  |    175 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcpl $v25.e7, $v20.e7                             ## L:691  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v24.e7, $v00.e7                             ## L:691  |    176 | vinvw.W = invert_half(vpos_clip).W;
  srl $s5, $s3, 5                                    ## L:433  |      ^ | t1 = t0 >> 5;
  ssv $v20, 6, 18, $s7                               ## L:686  |    177 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  nor $k0, $s3, $s5                                  ## L:447  |    178 | result = t0 ~| t1;
  ssv $v20, 14, 18, $t2                              ## L:687  |    179 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  vmudl $v29, $v20, $v25.h3                          ## L:696  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v24, 6, 20, $s7                               ## L:693  |    180 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadm $v29, $v19, $v25.h3                          ## L:696  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 6, 22, $s7                               ## L:693  |    181 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadn $v20, $v20, $v24.h3                          ## L:696  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v24, 14, 20, $t2                              ## L:694  |    182 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v19, $v19, $v24.h3                          ## L:696  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmudh $v29, $v23, $v30.e7                          ## L:700  |    183 | VTEMP = vviewoff:sint * 1;
  ssv $v25, 14, 22, $t2                              ## L:694  |      ^ | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v20, $v20, $v22.v                           ## L:701  |   *185 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_gl_pipeline_0004             ## L:707  |      ^ | if (has_points != 0)
  vmadh $v19, $v19, $v22.v                           ## L:701  |   *187 | vpos_clip = vpos_clip +* vviewscale;
  ori $s0, $zero, %lo(MATRICES_MV)                   ## L:710  |    188 | u16 addr = MATRICES_MV;
  ldv $v01, 0, 0, $s0                                ## L:466  |    189 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v04, 0, 40, $s0                               ## L:469  |    190 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 8, 0, $s0                                ## L:466  |    191 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v06, 0, 48, $s0                               ## L:471  |    192 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v02, 0, 32, $s0                               ## L:467  |    193 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 0, 8, $s0                                ## L:468  |    194 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v02, 8, 32, $s0                               ## L:467  |    195 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v03, 8, 8, $s0                                ## L:468  |    196 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v08, 0, 56, $s0                               ## L:473  |    197 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v04, 8, 40, $s0                               ## L:469  |    198 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v08, 8, 56, $s0                               ## L:473  |    199 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v06, 8, 48, $s0                               ## L:471  |    200 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudn $v29, $v02, $v14.h0                          ## L:713  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v05, 0, 16, $s0                               ## L:470  |    201 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v29, $v01, $v14.h0                          ## L:713  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v05, 8, 16, $s0                               ## L:470  |    202 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadn $v29, $v04, $v14.h1                          ## L:714  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadh $v29, $v03, $v14.h1                          ## L:714  |    203 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v07, 0, 24, $s0                               ## L:472  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v07, 8, 24, $s0                               ## L:472  |    204 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v29, $v06, $v14.h2                          ## L:715  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v14.h2                          ## L:715  |   *206 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e2                          ## L:716  |    207 | vpos = vmvp3 +* 0x20;
  vmadh $v14, $v07, $v30.e2                          ## L:716  |    208 | vpos = vmvp3 +* 0x20;
  LABEL_gl_pipeline_0004:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:719  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v17, 0, 0, $at                                ## L:719  |    209 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v17, 8, 0, $at                                ## L:719  |    210 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:721  |    211 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  lqv $v22, 0, 0, $at                                ## L:721  |    212 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:722  |    213 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  lqv $v23, 0, 0, $at                                ## L:722  |    214 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  addiu $at, $fp, MG_SCRATCH_MEM                     ## L:723  |    215 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lqv $v24, 0, 0, $at                                ## L:723  |    216 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lbu $s0, %lo(LIGHTING_COUNT + 0)                   ## L:730  |    217 | u8 light_count = load(LIGHTING_COUNT);
  ori $s1, $zero, %lo(LIGHTING_LIGHTS)               ## L:732  |  **220 | u16 light_ptr = LIGHTING_LIGHTS;
  vmudh $v29, $v24, $v30.e7                          ## L:725  |      ^ | VTEMP = vemissive:sint * 1;
  beq $s0, $zero, LIGHT_END                          ## L:733  |    221 | if (light_count == 0) goto LIGHT_END;
  vmacf $v17, $v17, $v23.v                           ## L:726  |   *223 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  ldv $v26, 0, 0, $s1                                ## L:739  |    224 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v25, 0, 8, $s1                                ## L:738  |    225 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  addiu $s2, $s1, 14                                 ## L:736  |    226 | u16 amb_ptr = light_ptr + 14;
  ldv $v26, 8, 0, $s1                                ## L:739  |    227 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  lh $s3, 6($s1)                                     ## L:743  |    228 | s16 light_w = load(light_ptr, 6);
  ldv $v25, 8, 8, $s1                                ## L:738  |    229 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v24, 0, 0, $s2                                ## L:737  |    230 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v24, 8, 0, $s2                                ## L:737  |    231 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  beq $s3, $zero, LABEL_gl_pipeline_0006             ## L:744  |    232 | if (light_w != 0)
  vmulf $v25, $v25, $v22.v                           ## L:741  |   *234 | vlcol:sfract *= vdiffuse;
  vsubc $v26, $v26, $v14.v                           ## L:758  |    235 | vlpos -= vpos;
  addiu $s5, $s1, 20                                 ## L:752  |      ^ | u16 att_ptr = light_ptr + 20;
  vmudh $v04, $v26, $v26.v                           ## L:759  | ***239 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 0, 0, $s5                                ## L:754  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  ldv $v01, 8, 0, $s5                                ## L:754  |    240 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v04, COP2_ACC_MD                             ## L:759  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vsar $v03, COP2_ACC_HI                             ## L:759  |    241 | vec32 vsqdist = vlpos * vlpos;
  addiu $s5, $s5, 6                                  ## L:755  |      ^ | att_ptr += 6;
  ldv $v02, 0, 0, $s5                                ## L:756  |    242 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  ldv $v02, 8, 0, $s5                                ## L:756  |   *244 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vaddc $v06, $v04, $v04.h1                          ## L:760  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ori $s5, $zero, %lo(MATRICES_MVP)                  ## L:786  |    245 | u16 addr = MATRICES_MVP;
  vadd $v05, $v03, $v03.h1                           ## L:760  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:761  |  **248 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:761  |    249 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:763  | ***253 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:763  |    254 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:764  |    255 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:764  |    256 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:764  |    257 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:766  |  **260 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:766  |    261 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:766  |    262 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:766  |    263 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v26, $v06.h0                          ## L:768  |    264 | vlpos *= vtmp.xxxxXXXX;
  ldv $v06, 0, 48, $s5                               ## L:471  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v26, $v26, $v05.h0                          ## L:768  |    265 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:770  |    266 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:770  |    267 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:771  |    268 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:771  |    269 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:771  |    270 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:771  |    271 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:772  |    272 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v07, 8, 24, $s5                               ## L:472  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadm $v29, $v03, $v02.h2                          ## L:772  |    273 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v06, 8, 48, $s5                               ## L:471  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadn $v02, $v04, $v01.h2                          ## L:772  |    274 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v07, 0, 24, $s5                               ## L:472  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadh $v01, $v03, $v01.h2                          ## L:772  |    275 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v08, 0, 56, $s5                               ## L:473  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcph $v01.e0, $v01.e0                             ## L:774  | ***279 | vatten.x = invert_half(vatten).x;
  ldv $v03, 8, 8, $s5                                ## L:468  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v04, 8, 40, $s5                               ## L:469  |    280 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrcpl $v02.e0, $v02.e0                             ## L:774  |      ^ | vatten.x = invert_half(vatten).x;
  ldv $v05, 8, 16, $s5                               ## L:470  |    281 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 0, 16, $s5                               ## L:470  |    282 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v03, 0, 8, $s5                                ## L:468  |    283 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrcph $v01.e0, $v01.e4                             ## L:775  |      ^ | vatten.X = invert_half(vatten).X;
  ldv $v08, 8, 56, $s5                               ## L:473  |    284 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcpl $v02.e4, $v02.e4                             ## L:775  |      ^ | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:775  |    285 | vatten.X = invert_half(vatten).X;
  ldv $v04, 0, 40, $s5                               ## L:469  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmudm $v29, $v25, $v02.h0                          ## L:777  |  **288 | vlcol *= vatten.xxxxXXXX;
  ldv $v02, 0, 32, $s5                               ## L:467  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v25, $v25, $v01.h0                          ## L:777  |    289 | vlcol *= vatten.xxxxXXXX;
  ldv $v02, 8, 32, $s5                               ## L:467  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v01, 8, 0, $s5                                ## L:466  |    290 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 0, 0, $s5                                ## L:466  |    291 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LABEL_gl_pipeline_0006:
  vmulf $v26, $v26, $v15.v                           ## L:791  |      ^ | vlpos *= vnorm:sfract;
  vmudn $v29, $v24, $v23.v                           ## L:792  |    292 | VTEMP = vlambient * vambient;
  vmacf $v29, $v25, $v26.h0                          ## L:793  |  **295 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v25, $v26.h1                          ## L:794  |    296 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $s0, $s0, 65535                              ## L:801  |      ^ | light_count -= 1;
  vmacf $v25, $v25, $v26.h2                          ## L:795  |    297 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $s1, $s1, 32                                 ## L:800  |      ^ | light_ptr += 32;
  vge $v25, $v25, $v00                               ## L:796  | ***301 | vlcol = max(vlcol, VZERO);
  bne $s0, $zero, LIGHT_LOOP                         ## L:802  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v17, $v17, $v25.v                            ## L:798  | ****306 | vrgba:sint += vlcol;
  LIGHT_END:
  ori $at, $zero, %lo(TEXTURE_MATRIX)                ## L:875  |    307 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v26, 0, 32, $at                               ## L:878  |    308 | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  vmov $v17.e3, $v22.e3                              ## L:806  |      ^ | vrgba.w = vdiffuse.w;
  lbu $s2, %lo(FOG_MASK + 0)                         ## L:813  |    309 | u8 fog_mask = load(FOG_MASK);
  vmov $v17.e7, $v22.e7                              ## L:807  |      ^ | vrgba.W = vdiffuse.W;
  ldv $v24, 0, 24, $at                               ## L:876  |    310 | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  or $t4, $zero, $t2                                 ## L:905  |    311 | prev_out1 = vtx_out1_ptr;
  ldv $v24, 8, 24, $at                               ## L:876  |    312 | vmt0:ufract = load(TEXTURE_MATRIX, 0x18).xyzwxyzw;
  ctc2 $s2, $vcc                                     ## L:814  |    313 | set_vcc(fog_mask);
  ldv $v26, 8, 32, $at                               ## L:878  |    314 | vmt1:ufract = load(TEXTURE_MATRIX, 0x20).xyzwxyzw;
  vmrg $v17, $v17, $v21.h2                           ## L:815  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  ldv $v23, 0, 0, $at                                ## L:875  |    315 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v25, 0, 8, $at                                ## L:877  |    316 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  ldv $v23, 8, 0, $at                                ## L:875  |    317 | vmt0:sint   = load(TEXTURE_MATRIX, 0x00).xyzwxyzw;
  ldv $v25, 8, 8, $at                                ## L:877  |    318 | vmt1:sint   = load(TEXTURE_MATRIX, 0x08).xyzwxyzw;
  ldv $v28, 0, 40, $at                               ## L:880  |    319 | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  ldv $v28, 8, 40, $at                               ## L:880  |    320 | vmt3:ufract = load(TEXTURE_MATRIX, 0x28).xyzwxyzw;
  vmudn $v29, $v24, $v16.h0                          ## L:882  |      ^ | VTEMP = vmt0  * vtex.xxxxXXXX;
  vmadh $v29, $v23, $v16.h0                          ## L:882  |    321 | VTEMP = vmt0  * vtex.xxxxXXXX;
  ldv $v27, 0, 16, $at                               ## L:879  |      ^ | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  vmadn $v29, $v26, $v16.h1                          ## L:883  |    322 | VTEMP = vmt1 +* vtex.yyyyYYYY;
  ldv $v27, 8, 16, $at                               ## L:879  |      ^ | vmt3:sint   = load(TEXTURE_MATRIX, 0x10).xyzwxyzw;
  addu $s4, $s4, $t9                                 ## L:906  |    323 | vtx_in0_ptr += vtx_size2;
  addu $t1, $t1, $t9                                 ## L:907  |    324 | vtx_in1_ptr += vtx_size2;
  vmadh $v29, $v25, $v16.h1                          ## L:883  |      ^ | VTEMP = vmt1 +* vtex.yyyyYYYY;
  ori $at, $zero, %lo(TEXTURE_SIZE)                  ## L:896  |    325 | vtex_size.xy = load(TEXTURE_SIZE).xy;
  vmadn $v22, $v28, $v31.e7                          ## L:884  |      ^ | vtex_transformed = vmt3 +* 0x100;
  or $t3, $zero, $s7                                 ## L:904  |    326 | prev_out0 = vtx_out0_ptr;
  vmadh $v21, $v27, $v31.e7                          ## L:884  |      ^ | vtex_transformed = vmt3 +* 0x100;
  LOAD_NORMAL912: lsv $v15, 8, 0, $t1                ## L:912  | ***330 | @AttrLoader("NORMAL") vnorm.X = load(vtx_in1_ptr).x;
  vrcph $v23.e0, $v21.e3                             ## L:889  |      ^ | vinv_tex.x = invert_half(vtex_transformed).w;
  vrcpl $v24.e0, $v22.e3                             ## L:889  |    331 | vinv_tex.x = invert_half(vtex_transformed).w;
  addiu $t2, $t2, 80                                 ## L:909  |      ^ | vtx_out1_ptr += 80;
  LOAD_POSITION914: ldv $v14, 8, 0, $t1              ## L:914  |    332 | @AttrLoader("POSITION") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vrcph $v23.e0, $v21.e7                             ## L:890  |      ^ | vinv_tex.X = invert_half(vtex_transformed).W;
  vrcpl $v24.e4, $v22.e7                             ## L:890  |    333 | vinv_tex.X = invert_half(vtex_transformed).W;
  LOAD_COLOR915: llv $v18, 0, 0, $s4                 ## L:915  |      ^ | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  addiu $s7, $s7, 80                                 ## L:908  |    334 | vtx_out0_ptr += 80;
  vrcph $v23.e4, $v00.e7                             ## L:890  |      ^ | vinv_tex.X = invert_half(vtex_transformed).W;
  LOAD_NORMAL911: lsv $v15, 0, 0, $s4                ## L:911  |  **337 | @AttrLoader("NORMAL") vnorm.x = load(vtx_in0_ptr).x;
  vmudl $v29, $v22, $v24.h0                          ## L:892  |      ^ | vtex_transformed *= vinv_tex.xxxxXXXX;
  vmadm $v29, $v21, $v24.h0                          ## L:892  |    338 | vtex_transformed *= vinv_tex.xxxxXXXX;
  LOAD_POSITION913: ldv $v14, 0, 0, $s4              ## L:913  |      ^ | @AttrLoader("POSITION") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR916: llv $v18, 4, 0, $t1                 ## L:916  |    339 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmadn $v22, $v22, $v23.h0                          ## L:892  |      ^ | vtex_transformed *= vinv_tex.xxxxXXXX;
  vmadh $v21, $v21, $v23.h0                          ## L:892  |    340 | vtex_transformed *= vinv_tex.xxxxXXXX;
  llv $v23, 0, 0, $at                                ## L:896  |      ^ | vtex_size.xy = load(TEXTURE_SIZE).xy;
  llv $v23, 8, 0, $at                                ## L:897  |    341 | vtex_size.XY = load(TEXTURE_SIZE).xy;
  ori $at, $zero, %lo(TEXTURE_OFFSET)                ## L:898  |    342 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 0, 0, $at                                ## L:898  |    343 | vtex_offset.xy = load(TEXTURE_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:899  |    344 | vtex_offset.XY = load(TEXTURE_OFFSET).xy;
  vmudh $v29, $v24, $v30.e7                          ## L:901  | ***348 | VTEMP = vtex_offset:sint * 1;
  sltu $at, $s4, $s6                                 ## L:918  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadn $v29, $v22, $v23.v                           ## L:902  |    349 | vtex = vtex_transformed +* vtex_size;
  bne $at, $zero, VERTEX_LOOP                        ## L:918  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v16, $v21, $v23.v                           ## L:902  |   *351 | vtex = vtex_transformed +* vtex_size;
  sdv $v19, 8, 0, $t4                                ## L:925  |    352 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sb $k0, 7($t4)                                     ## L:930  |    353 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  suv $v17, 4, 8, $t4                                ## L:933  |    354 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  suv $v17, 0, 8, $t3                                ## L:932  |    355 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  slv $v16, 8, 12, $t4                               ## L:936  |    356 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sdv $v19, 0, 0, $t3                                ## L:924  |    357 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t5, 6($t3)                                     ## L:927  |    358 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  sb $t6, 6($t4)                                     ## L:928  |    359 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $t7, 7($t3)                                     ## L:929  |    360 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  j RSPQ_Loop                                        ## L:937  |    361 | }
  slv $v16, 0, 12, $t3                               ## L:935  |   *363 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40

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