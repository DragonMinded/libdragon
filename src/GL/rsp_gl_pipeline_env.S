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
    .align 1
    TEXTURING_SCALE: .ds.b 8
    .align 1
    TEXTURING_OFFSET: .ds.b 8
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
    MgVertexAttributeLoaders LOAD_POS_NORM518, LOAD_POS_NORM519, LOAD_POS_NORM520, LOAD_POS_NORM521, LOAD_POS_NORM821, LOAD_POS_NORM822, LOAD_POS_NORM819, LOAD_POS_NORM820
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR522, LOAD_COLOR523, LOAD_COLOR823, LOAD_COLOR824
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:413  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ldv $v02, 0, 0, $t0                                ## L:415  |      2 | vec16 vinputs = load(inputs_ptr).xyzw;
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:414  |      3 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v01, 0, 0, $at                                ## L:414  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:431  |      5 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:439  |    **8 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vmudh $v04, $v02, $v01.v                           ## L:417  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  vsar $v03, COP2_ACC_HI                             ## L:417  |      9 | vec32 vproducts = vinputs * vvtx_size;
  vsar $v04, COP2_ACC_MD                             ## L:417  |     10 | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:418  |      ^ | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v03, 0, 8, $at                                ## L:419  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:418  |     14 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v04, 0, 10, $at                               ## L:419  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:429  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:425  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:427  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:432  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:434  |     20 | u32 misalignment = buffer_offset & 0x7;
  addu $t0, $t1, $t3                                 ## L:435  |     21 | dma_size = vertices_size + misalignment;
  addiu $t0, $t0, 7                                  ## L:436  |     22 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:437  |     23 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:440  |     24 | vtx_in0_ptr -= dma_size;
  addiu $t0, $t0, -1                                 ## L:442  |     25 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  jal DMAExec                                        ## L:442  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t2, $zero, 12                               ## L:442  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:456  |     29 | u16 mvpaddr = MATRICES_MVP;
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:461  |     30 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:462  |     31 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v11, 0, 16, $at                               ## L:463  |     32 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:461  |     33 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:461  |     34 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v10, 8, 8, $at                                ## L:462  |     35 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:463  |     36 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:447  |     37 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:465  |     38 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:396  |     39 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v12, 0, 0, $at                                ## L:465  |     40 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:394  |     41 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v13, 0, 8, $at                                ## L:466  |     42 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v13, 8, 8, $at                                ## L:466  |     43 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:465  |     44 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ldv $v06, 0, 48, $t0                               ## L:399  |     45 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:473  |     46 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:400  |     47 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v14, 0, 0, $at                                ## L:473  |     48 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  lhu $fp, %lo(MATERIAL_FLAGS + 0)                   ## L:503  |     49 | u16 mat_flags = load(MATERIAL_FLAGS);
  ldv $v14, 8, 0, $at                                ## L:473  |     50 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:401  |     51 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:495  |     52 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:398  |     53 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:397  |     54 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:495  |     55 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:495  |     56 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:496  |     57 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:448  |     58 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v02, 0, 32, $t0                               ## L:395  |     59 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:400  |     60 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:396  |     61 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:399  |     62 | m2:ufract = load(address, 0x30).xyzwxyzw;
  addiu $k0, $zero, 48                               ## L:490  |     63 | u16 mat_ptr_diffuse  = 0x30;
  ldv $v05, 8, 16, $t0                               ## L:398  |     64 | m2:sint   = load(address, 0x10).xyzwxyzw;
  andi $s0, $fp, 1                                   ## L:504  |     65 | u16 tmp = mat_flags & 0x1;
  ori $t2, $zero, %lo(MG_SCRATCH_MEM)                ## L:478  |     66 | u16 prev_out0 = MG_SCRATCH_MEM;
  addiu $k1, $zero, 64                               ## L:491  |     67 | u16 mat_ptr_ambient  = 0x40;
  ldv $v23, 0, 0, $at                                ## L:496  |     68 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v08, 8, 56, $t0                               ## L:401  |     69 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addiu $t1, $s7, 40                                 ## L:476  |     70 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:444  |     71 | vtx_size = load(MG_VERTEX_SIZE);
  ldv $v23, 8, 0, $at                                ## L:496  |     72 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:397  |     73 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:497  |     74 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v24, 0, 0, $at                                ## L:497  |     75 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:394  |     76 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:395  |     77 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v24, 8, 0, $at                                ## L:497  |     78 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:445  |     79 | vtx_size2 = vtx_size * 2;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:499  |     80 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:479  |     81 | u16 prev_out1 = MG_SCRATCH_MEM;
  sqv $v22, 0, 48, $at                               ## L:499  |     82 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  addiu $sp, $zero, 80                               ## L:492  |     83 | u16 mat_ptr_emissive = 0x50;
  addu $t0, $s4, $t8                                 ## L:475  |     84 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  sqv $v23, 0, 64, $at                               ## L:500  |     85 | store(vambient, MG_SCRATCH_MEM, 0x40);
  beq $s0, $zero, LABEL_gl_pipeline_0001             ## L:505  |     86 | if (tmp != 0) {
  sqv $v24, 0, 80, $at                               ## L:501  |    *88 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  addiu $k0, $zero, 96                               ## L:506  |     89 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s0, $fp, 2                                   ## L:508  |     90 | tmp = mat_flags & 0x2;
  beq $s0, $zero, LABEL_gl_pipeline_0002             ## L:509  |     91 | if (tmp != 0 ) {
  nop                                                ## L:509  |    *93 | if (tmp != 0 ) {
  addiu $k1, $zero, 96                               ## L:510  |     94 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s0, $fp, 4                                   ## L:512  |     95 | tmp = mat_flags & 0x4;
  beq $s0, $zero, LABEL_gl_pipeline_0003             ## L:513  |     96 | if (tmp != 0) {
  nop                                                ## L:513  |    *98 | if (tmp != 0) {
  addiu $sp, $zero, 96                               ## L:514  |     99 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_POS_NORM518: ldv $v16, 0, 0, $s4              ## L:518  |    100 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM519: ldv $v16, 8, 0, $t0              ## L:519  |    101 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_COLOR522: llv $v19, 0, 0, $s4                 ## L:522  |    102 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_COLOR523: llv $v19, 4, 0, $t0                 ## L:523  |    103 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  LOAD_POS_NORM520: ldv $v15, 0, 0, $s4              ## L:520  |    104 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM521: ldv $v15, 8, 0, $t0              ## L:521  |    105 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h3                           ## L:558  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:545  |    106 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v19, 0, 0, $at                                ## L:545  |   *108 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  sdv $v20, 8, 0, $t3                                ## L:532  |    109 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  vmudn $v16, $v16, $v13.v                           ## L:559  |      ^ | vnorm *= vnormfactor;
  luv $v19, 0, 0, $at                                ## L:546  |    110 | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  suv $v18, 0, 8, $t2                                ## L:539  |    111 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmudn $v29, $v02, $v15.h0                          ## L:562  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:547  |    112 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmadh $v29, $v01, $v15.h0                          ## L:562  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  suv $v18, 4, 8, $t3                                ## L:540  |    113 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  vmadn $v29, $v06, $v15.h2                          ## L:563  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:563  |    114 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  sdv $v20, 0, 0, $t2                                ## L:531  |      ^ | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmadn $v29, $v04, $v15.h1                          ## L:564  |    115 | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  sqv $v19, 0, 96, $at                               ## L:547  |      ^ | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:587  |    116 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:564  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v22, 0, 0, $at                                ## L:587  |    117 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadn $v21, $v08, $v30.e2                          ## L:565  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmadh $v20, $v07, $v30.e2                          ## L:565  |    118 | vpos_clip = vmvp3 +* 0x20;
  ldv $v22, 8, 0, $at                                ## L:587  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmulf $v29, $v09, $v16.h0                          ## L:567  |    119 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:610  |      ^ | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmacf $v29, $v10, $v16.h1                          ## L:568  |    120 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  vmacf $v16, $v11, $v16.h2                          ## L:569  |    121 | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  sb $t6, 7($t2)                                     ## L:536  |      ^ | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  slv $v17, 8, 12, $t3                               ## L:543  |    122 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmudn $v24, $v21, $v22.v                           ## L:588  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  sb $t7, 7($t3)                                     ## L:537  |    123 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmadh $v23, $v20, $v22.v                           ## L:588  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  sb $t5, 6($t3)                                     ## L:535  |    124 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sdv $v20, 0, 24, $s7                               ## L:597  |    125 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sdv $v20, 8, 24, $t1                               ## L:598  |    126 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sb $t4, 6($t2)                                     ## L:534  |    127 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vch $v29, $v23, $v23.h3                            ## L:589  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v21, 8, 32, $t1                               ## L:598  |    128 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vcl $v29, $v24, $v24.h3                            ## L:589  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $fp, $vcc                                     ## L:589  |    129 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sdv $v21, 0, 32, $s7                               ## L:597  |    130 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  ldv $v24, 0, 8, $at                                ## L:611  |    131 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vch $v29, $v20, $v20.h3                            ## L:593  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  srl $s0, $fp, 4                                    ## L:380  |    132 | u16 codes2 = codes >> 4;
  vcl $v29, $v21, $v21.h3                            ## L:593  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  andi $s1, $fp, 1799                                ## L:360  |    133 | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:361  |    134 | t1 = t0 >> 5;
  vaddc $v23, $v21, $v14.h1                          ## L:602  |      ^ | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  ori $fp, $zero, %lo(MATRICES_MV)                   ## L:640  |    135 | u16 addr = MATRICES_MV;
  vadd $v22, $v20, $v14.h0                           ## L:603  |      ^ | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  ldv $v02, 0, 32, $fp                               ## L:395  |    136 | m0:ufract = load(address, 0x20).xyzwxyzw;
  or $t4, $s1, $s2                                   ## L:368  |    137 | result = t0 | t1;
  andi $s1, $s0, 1799                                ## L:360  |    138 | t0 = codes & 0x707;
  srl $s2, $s1, 5                                    ## L:361  |    139 | t1 = t0 >> 5;
  ldv $v01, 0, 0, $fp                                ## L:394  |    140 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmudn $v29, $v23, $v14.h2                          ## L:605  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  cfc2 $s0, $vcc                                     ## L:593  |    141 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vmadh $v22, $v22, $v14.h2                          ## L:606  |      ^ | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v07, 0, 24, $fp                               ## L:400  |    142 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:610  |    143 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  or $t5, $s1, $s2                                   ## L:368  |    144 | result = t0 | t1;
  ldv $v08, 0, 56, $fp                               ## L:401  |    145 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vge $v22, $v00, $v22.h2                            ## L:607  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  ldv $v23, 8, 0, $at                                ## L:610  |    146 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  ldv $v07, 8, 24, $fp                               ## L:400  |    147 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v03, 0, 8, $fp                                ## L:396  |    148 | m1:sint   = load(address, 0x08).xyzwxyzw;
  andi $s2, $s0, 1799                                ## L:360  |    149 | t0 = codes & 0x707;
  ldv $v05, 0, 16, $fp                               ## L:398  |    150 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmudl $v21, $v21, $v23.h3                          ## L:613  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  vmadm $v20, $v20, $v23.h3                          ## L:613  |    151 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v01, 8, 0, $fp                                ## L:394  |      ^ | m0:sint   = load(address, 0x00).xyzwxyzw;
  vmadn $v21, $v00, $v00                             ## L:613  |    152 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  ldv $v06, 0, 48, $fp                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  srl $s3, $s2, 5                                    ## L:361  |    153 | t1 = t0 >> 5;
  srl $s1, $s0, 4                                    ## L:387  |    154 | u16 codes2 = codes >> 4;
  ldv $v03, 8, 8, $fp                                ## L:396  |    155 | m1:sint   = load(address, 0x08).xyzwxyzw;
  nor $t6, $s2, $s3                                  ## L:375  |    156 | result = t0 ~| t1;
  andi $s2, $s1, 1799                                ## L:360  |    157 | t0 = codes & 0x707;
  vrcph $v25.e3, $v20.e3                             ## L:620  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcpl $v26.e3, $v21.e3                             ## L:620  |    158 | vinvw.w = invert_half(vpos_clip).w;
  ssv $v20, 14, 16, $t1                              ## L:617  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ldv $v24, 8, 8, $at                                ## L:611  |    159 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vrcph $v25.e3, $v20.e7                             ## L:621  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 6, 16, $s7                               ## L:616  |    160 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcpl $v26.e7, $v21.e7                             ## L:621  |      ^ | vinvw.W = invert_half(vpos_clip).W;
  vrcph $v25.e7, $v00.e7                             ## L:621  |    161 | vinvw.W = invert_half(vpos_clip).W;
  srl $s3, $s2, 5                                    ## L:361  |      ^ | t1 = t0 >> 5;
  ssv $v21, 14, 18, $t1                              ## L:617  |    162 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v21, 6, 18, $s7                               ## L:616  |    163 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  ldv $v04, 0, 40, $fp                               ## L:397  |    164 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmudl $v29, $v21, $v26.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  vmadm $v29, $v20, $v26.h3                          ## L:626  |    165 | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v02, 8, 32, $fp                               ## L:395  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v05, 8, 16, $fp                               ## L:398  |    166 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadn $v21, $v21, $v25.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ldv $v06, 8, 48, $fp                               ## L:399  |    167 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadh $v20, $v20, $v25.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:649  |    168 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v08, 8, 56, $fp                               ## L:401  |    169 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmudh $v29, $v24, $v30.e7                          ## L:630  |      ^ | VTEMP = vviewoff:sint * 1;
  ssv $v25, 6, 20, $s7                               ## L:623  |    170 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  ldv $v04, 8, 40, $fp                               ## L:397  |    171 | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadn $v21, $v21, $v23.v                           ## L:631  |      ^ | vpos_clip = vpos_clip +* vviewscale;
  vmadh $v20, $v20, $v23.v                           ## L:631  |    172 | vpos_clip = vpos_clip +* vviewscale;
  lbu $fp, %lo(LIGHTING_COUNT + 0)                   ## L:660  |      ^ | u8 light_count = load(LIGHTING_COUNT);
  vmudn $v29, $v02, $v15.h0                          ## L:643  |    173 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ori $s0, $zero, %lo(LIGHTING_LIGHTS)               ## L:662  |      ^ | u16 light_ptr = LIGHTING_LIGHTS;
  vmadh $v29, $v01, $v15.h0                          ## L:643  |    174 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v18, 0, 0, $at                                ## L:649  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ssv $v25, 14, 20, $t1                              ## L:624  |    175 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  ldv $v18, 8, 0, $at                                ## L:649  |    176 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  vmadn $v29, $v04, $v15.h1                          ## L:644  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  addiu $at, $k0, MG_SCRATCH_MEM                     ## L:651  |    177 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  vmadh $v29, $v03, $v15.h1                          ## L:644  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  lqv $v23, 0, 0, $at                                ## L:651  |    178 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  vmadn $v29, $v06, $v15.h2                          ## L:645  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:652  |    179 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  vmadh $v29, $v05, $v15.h2                          ## L:645  |      ^ | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  lqv $v24, 0, 0, $at                                ## L:652  |    180 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:653  |    181 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lqv $v25, 0, 0, $at                                ## L:653  |    182 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  vmadn $v29, $v08, $v30.e2                          ## L:646  |      ^ | vpos = vmvp3 +* 0x20;
  ssv $v26, 14, 22, $t1                              ## L:624  |    183 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  nor $t7, $s2, $s3                                  ## L:375  |    184 | result = t0 ~| t1;
  slv $v17, 0, 12, $t2                               ## L:542  |    185 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  vmadh $v15, $v07, $v30.e2                          ## L:646  |      ^ | vpos = vmvp3 +* 0x20;
  vmudh $v29, $v25, $v30.e7                          ## L:655  |    186 | VTEMP = vemissive:sint * 1;
  ssv $v26, 6, 22, $s7                               ## L:623  |      ^ | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  beq $fp, $zero, LIGHT_END                          ## L:663  |    187 | if (light_count == 0) goto LIGHT_END;
  vmacf $v18, $v18, $v24.v                           ## L:656  |   *189 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  ldv $v26, 0, 8, $s0                                ## L:668  |    190 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v26, 8, 8, $s0                                ## L:668  |    191 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  lh $s2, 6($s0)                                     ## L:673  |    192 | s16 light_w = load(light_ptr, 6);
  ldv $v27, 0, 0, $s0                                ## L:669  |    193 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  addiu $s1, $s0, 14                                 ## L:666  |    194 | u16 amb_ptr = light_ptr + 14;
  ldv $v25, 0, 0, $s1                                ## L:667  |    195 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v25, 8, 0, $s1                                ## L:667  |    196 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v27, 8, 0, $s0                                ## L:669  |    197 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  vmulf $v26, $v26, $v23.v                           ## L:671  |      ^ | vlcol:sfract *= vdiffuse;
  beq $s2, $zero, LABEL_gl_pipeline_0005             ## L:674  |    198 | if (light_w != 0)
  nop                                                ## L:754  |   *200 | 
  vsubc $v27, $v27, $v15.v                           ## L:688  |    201 | vlpos -= vpos;
  addiu $s3, $s0, 20                                 ## L:682  |      ^ | u16 att_ptr = light_ptr + 20;
  ldv $v01, 0, 0, $s3                                ## L:684  | ***205 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vmudh $v04, $v27, $v27.v                           ## L:689  |      ^ | vec32 vsqdist = vlpos * vlpos;
  vsar $v04, COP2_ACC_MD                             ## L:689  |    206 | vec32 vsqdist = vlpos * vlpos;
  ldv $v01, 8, 0, $s3                                ## L:684  |      ^ | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vsar $v03, COP2_ACC_HI                             ## L:689  |    207 | vec32 vsqdist = vlpos * vlpos;
  vaddc $v06, $v04, $v04.h1                          ## L:690  |  **210 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  addiu $s3, $s3, 6                                  ## L:685  |      ^ | att_ptr += 6;
  ldv $v02, 0, 0, $s3                                ## L:686  |    211 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vadd $v05, $v03, $v03.h1                           ## L:690  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  ldv $v02, 8, 0, $s3                                ## L:686  |  **214 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vaddc $v04, $v06, $v04.h2                          ## L:691  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:691  |    215 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:693  | ***219 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:693  |    220 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:694  |    221 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:694  |    222 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:694  |    223 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:696  |  **226 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:696  |    227 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:696  |    228 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:696  |    229 | vec32 vdist = vtmp * vsqdist;
  vmudm $v29, $v27, $v06.h0                          ## L:698  |    230 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v27, $v27, $v05.h0                          ## L:698  |    231 | vlpos *= vtmp.xxxxXXXX;
  vmudn $v29, $v02, $v30.e7                          ## L:700  |    232 | VTEMP = vatten * 1;
  vmadh $v29, $v01, $v30.e7                          ## L:700  |    233 | VTEMP = vatten * 1;
  vmadl $v29, $v08, $v02.h1                          ## L:701  |    234 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:701  |    235 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:701  |    236 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:701  |    237 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadl $v29, $v04, $v02.h2                          ## L:702  |    238 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadm $v29, $v03, $v02.h2                          ## L:702  |    239 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:702  |    240 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadh $v01, $v03, $v01.h2                          ## L:702  |    241 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vrcph $v01.e0, $v01.e0                             ## L:704  | ***245 | vatten.x = invert_half(vatten).x;
  vrcpl $v02.e0, $v02.e0                             ## L:704  |    246 | vatten.x = invert_half(vatten).x;
  vrcph $v01.e0, $v01.e4                             ## L:705  |  **249 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:705  |    250 | vatten.X = invert_half(vatten).X;
  vrcph $v01.e4, $v00.e4                             ## L:705  |    251 | vatten.X = invert_half(vatten).X;
  vmudm $v29, $v26, $v02.h0                          ## L:707  |  **254 | vlcol *= vatten.xxxxXXXX;
  vmadh $v26, $v26, $v01.h0                          ## L:707  |    255 | vlcol *= vatten.xxxxXXXX;
  LABEL_gl_pipeline_0005:
  vmulf $v27, $v27, $v16.v                           ## L:721  |    256 | vlpos *= vnorm:sfract;
  vmudn $v29, $v25, $v24.v                           ## L:722  |    257 | VTEMP = vlambient * vambient;
  vmacf $v29, $v26, $v27.h0                          ## L:723  |  **260 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  vmacf $v29, $v26, $v27.h1                          ## L:724  |    261 | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  addiu $s0, $s0, 32                                 ## L:730  |      ^ | light_ptr += 32;
  vmacf $v26, $v26, $v27.h2                          ## L:725  |    262 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  addiu $fp, $fp, 65535                              ## L:731  |      ^ | light_count -= 1;
  vge $v26, $v26, $v00                               ## L:726  | ***266 | vlcol = max(vlcol, VZERO);
  bne $fp, $zero, LIGHT_LOOP                         ## L:732  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v26.v                            ## L:728  | ****271 | vrgba:sint += vlcol;
  LIGHT_END:
  vmov $v18.e3, $v23.e3                              ## L:736  |    272 | vrgba.w = vdiffuse.w;
  or $t2, $zero, $s7                                 ## L:812  |      ^ | prev_out0 = vtx_out0_ptr;
  lbu $s1, %lo(FOG_MASK + 0)                         ## L:743  |    273 | u8 fog_mask = load(FOG_MASK);
  vmov $v18.e7, $v23.e7                              ## L:737  |      ^ | vrgba.W = vdiffuse.W;
  ctc2 $s1, $vcc                                     ## L:744  | ***277 | set_vcc(fog_mask);
  vmrg $v18, $v18, $v22.h2                           ## L:745  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  vmudh $v23, $v15, $v15.v                           ## L:756  |    278 | vec32 vsqdist = vpos * vpos;
  ori $s1, $zero, %lo(MATRICES_MVP)                  ## L:752  |      ^ | u16 addr = MATRICES_MVP;
  vsar $v23, COP2_ACC_MD                             ## L:756  |    279 | vec32 vsqdist = vpos * vpos;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:799  |      ^ | vtexscale:sint.xy = load(TEXTURING_SCALE).xy;
  addu $t0, $t0, $t9                                 ## L:815  |    280 | vtx_in1_ptr += vtx_size2;
  vsar $v22, COP2_ACC_HI                             ## L:756  |      ^ | vec32 vsqdist = vpos * vpos;
  ldv $v06, 8, 48, $s1                               ## L:399  |  **283 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vaddc $v25, $v23, $v23.h1                          ## L:757  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v24, $v22, $v22.h1                           ## L:757  |    284 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  addiu $s7, $s7, 80                                 ## L:816  |      ^ | vtx_out0_ptr += 80;
  ldv $v04, 8, 40, $s1                               ## L:397  |    285 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v01, 0, 0, $s1                                ## L:394  |   *287 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vaddc $v23, $v25, $v23.h2                          ## L:758  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v01, 8, 0, $s1                                ## L:394  |    288 | m0:sint   = load(address, 0x00).xyzwxyzw;
  vadd $v22, $v24, $v22.h2                           ## L:758  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  ldv $v05, 8, 16, $s1                               ## L:398  |    289 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v05, 0, 16, $s1                               ## L:398  |  **292 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vrsqh $v24.e0, $v22.e0                             ## L:760  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v25.e0, $v23.e0                             ## L:760  |    293 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v04, 0, 40, $s1                               ## L:397  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrsqh $v24.e0, $v22.e4                             ## L:761  |    294 | vtmp.X = invert_half_sqrt(vsqdist).X;
  ldv $v03, 8, 8, $s1                                ## L:396  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  addu $s4, $s4, $t9                                 ## L:814  |    295 | vtx_in0_ptr += vtx_size2;
  vrsql $v25.e4, $v23.e4                             ## L:761  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  or $t3, $zero, $t1                                 ## L:813  |    296 | prev_out1 = vtx_out1_ptr;
  vrsqh $v24.e4, $v00.e0                             ## L:761  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  LOAD_COLOR823: llv $v19, 0, 0, $s4                 ## L:823  |  **299 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  vmudm $v29, $v15, $v25.h0                          ## L:763  |      ^ | vpos *= vtmp.xxxxXXXX;
  vmadh $v15, $v15, $v24.h0                          ## L:763  |    300 | vpos *= vtmp.xxxxXXXX;
  ldv $v06, 0, 48, $s1                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmulf $v24, $v15, $v16.v                           ## L:766  | ***304 | vec16 vdot = vpos:sfract * vnorm:sfract;
  vadd $v25, $v24, $v24.h1                           ## L:767  | ***308 | vec16 vtmp:sint = vdot + vdot.yyyyYYYY;
  vadd $v24, $v25, $v24.h2                           ## L:768  | ***312 | vdot:sint = vtmp + vdot.zzzzZZZZ;
  vor $v25, $v00, $v00                               ## L:773  |    313 | vec16 vzunit = VZERO;
  vlt $v24, $v24, $v00                               ## L:770  |  **316 | vdot = min(vdot, VZERO);
  vmov $v25.e6, $v12.e3                              ## L:775  |    317 | vzunit.Z = vnormmask.w;
  vmov $v25.e2, $v12.e3                              ## L:774  |    318 | vzunit.z = vnormmask.w;
  vsubc $v24, $v00, $v24.v                           ## L:771  |   *320 | vdot = VZERO - vdot;
  vmulf $v29, $v16, $v24.h0                          ## L:777  | ***324 | VTEMP = vnorm:sfract  * vdot.xxxxXXXX;
  vmacf $v29, $v16, $v24.h0                          ## L:778  |    325 | VTEMP = vnorm:sfract +* vdot.xxxxXXXX;
  vmadh $v29, $v15, $v30.e7                          ## L:779  |    326 | VTEMP = vpos:sint +* 1;
  vmadh $v29, $v25, $v30.e7                          ## L:780  |    327 | VTEMP = vzunit:sint +* 1;
  vsar $v25, COP2_ACC_MD                             ## L:783  |    328 | vec32 vrefl = get_acc();
  LOAD_POS_NORM821: ldv $v15, 0, 0, $s4              ## L:821  |      ^ | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  ldv $v03, 0, 8, $s1                                ## L:396  |    329 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vsar $v24, COP2_ACC_HI                             ## L:783  |      ^ | vec32 vrefl = get_acc();
  vmudl $v29, $v25, $v25.v                           ## L:785  |  **332 | vsqdist = vrefl * vrefl;
  ldv $v08, 0, 56, $s1                               ## L:401  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadm $v29, $v24, $v25.v                           ## L:785  |    333 | vsqdist = vrefl * vrefl;
  vmadn $v23, $v25, $v24.v                           ## L:785  |    334 | vsqdist = vrefl * vrefl;
  LOAD_POS_NORM822: ldv $v15, 8, 0, $t0              ## L:822  |      ^ | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v22, $v24, $v24.v                           ## L:785  |    335 | vsqdist = vrefl * vrefl;
  addiu $t1, $t1, 80                                 ## L:817  |      ^ | vtx_out1_ptr += 80;
  vaddc $v27, $v23, $v23.h1                          ## L:786  |  **338 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v26, $v22, $v22.h1                           ## L:786  |    339 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  LOAD_POS_NORM819: ldv $v16, 0, 0, $s4              ## L:819  |      ^ | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  vaddc $v23, $v27, $v23.h2                          ## L:787  |  **342 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  LOAD_COLOR824: llv $v19, 4, 0, $t0                 ## L:824  |      ^ | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  ldv $v08, 8, 56, $s1                               ## L:401  |    343 | m3:ufract = load(address, 0x38).xyzwxyzw;
  vadd $v22, $v26, $v22.h2                           ## L:787  |      ^ | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vxor $v17, $v00, $v30.e0                           ## L:792  |    344 | vtex = 0x80;
  ldv $v02, 8, 32, $s1                               ## L:395  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v07, 8, 24, $s1                               ## L:400  |  **347 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrsqh $v26.e0, $v22.e0                             ## L:789  |      ^ | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v27.e0, $v23.e0                             ## L:789  |    348 | vtmp.x = invert_half_sqrt(vsqdist).x;
  ldv $v02, 0, 32, $s1                               ## L:395  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  vrsqh $v26.e0, $v22.e4                             ## L:790  |    349 | vtmp.X = invert_half_sqrt(vsqdist).X;
  LOAD_POS_NORM820: ldv $v16, 8, 0, $t0              ## L:820  |      ^ | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  llv $v22, 0, 0, $at                                ## L:799  |    350 | vtexscale:sint.xy = load(TEXTURING_SCALE).xy;
  vrsql $v27.e4, $v23.e4                             ## L:790  |      ^ | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v26.e4, $v00.e0                             ## L:790  |    351 | vtmp.X = invert_half_sqrt(vsqdist).X;
  llv $v23, 0, 4, $at                                ## L:800  |      ^ | vtexscale:ufract.xy = load(TEXTURING_SCALE).zw;
  llv $v22, 8, 0, $at                                ## L:801  |    352 | vtexscale:sint.XY = load(TEXTURING_SCALE).xy;
  llv $v23, 8, 4, $at                                ## L:802  |    353 | vtexscale:ufract.XY = load(TEXTURING_SCALE).zw;
  vmudh $v29, $v17, $v30.e7                          ## L:793  |      ^ | VTEMP = vtex:sint * 1;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:803  |    354 | vtexoffset:sint.xy = load(TEXTURING_OFFSET).xy;
  vmadl $v29, $v25, $v27.h0                          ## L:794  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  vmadm $v29, $v24, $v27.h0                          ## L:794  |    355 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  ldv $v07, 0, 24, $s1                               ## L:400  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadn $v29, $v25, $v26.h0                          ## L:794  |    356 | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  llv $v25, 0, 4, $at                                ## L:804  |      ^ | vtexoffset:ufract.xy = load(TEXTURING_OFFSET).zw;
  llv $v25, 8, 4, $at                                ## L:806  |    357 | vtexoffset:ufract.XY = load(TEXTURING_OFFSET).zw;
  vmadh $v17, $v24, $v26.h0                          ## L:794  |      ^ | vtex:sint = vrefl +* vtmp.xxxxXXXX;
  llv $v24, 0, 0, $at                                ## L:803  |    358 | vtexoffset:sint.xy = load(TEXTURING_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:805  |    359 | vtexoffset:sint.XY = load(TEXTURING_OFFSET).xy;
  sltu $at, $s4, $s6                                 ## L:826  |   *361 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmudn $v29, $v25, $v30.e7                          ## L:809  |      ^ | VTEMP = vtexoffset * 1;
  vmadh $v29, $v24, $v30.e7                          ## L:809  |   *363 | VTEMP = vtexoffset * 1;
  vmadm $v29, $v17, $v23.v                           ## L:810  |    364 | vtex = vtex +* vtexscale;
  bne $at, $zero, VERTEX_LOOP                        ## L:826  |      ^ | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  vmadh $v17, $v17, $v22.v                           ## L:810  |   *366 | vtex = vtex +* vtexscale;
  sdv $v20, 0, 0, $t2                                ## L:832  |    367 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  sb $t4, 6($t2)                                     ## L:835  |    368 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  suv $v18, 4, 8, $t3                                ## L:841  |    369 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sdv $v20, 8, 0, $t3                                ## L:833  |    370 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v18, 0, 8, $t2                                ## L:840  |    371 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  slv $v17, 8, 12, $t3                               ## L:844  |    372 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  slv $v17, 0, 12, $t2                               ## L:843  |    373 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  sb $t7, 7($t3)                                     ## L:838  |    374 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  sb $t6, 7($t2)                                     ## L:837  |    375 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  j RSPQ_Loop                                        ## L:845  |    376 | }
  sb $t5, 6($t3)                                     ## L:836  |   *378 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10

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