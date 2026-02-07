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
    MgVertexAttributeLoaders LOAD_POS_NORM518, LOAD_POS_NORM519, LOAD_POS_NORM520, LOAD_POS_NORM521, LOAD_POS_NORM819, LOAD_POS_NORM821, LOAD_POS_NORM822, LOAD_POS_NORM820
  MgEndVertexAttribute

  MgBeginVertexAttribute 1, 1
    MgVertexAttributeLoaders LOAD_COLOR522, LOAD_COLOR523, LOAD_COLOR824, LOAD_COLOR823
  MgEndVertexAttribute

  MgBeginVertexAttribute 2, 1
    MgVertexAttributeLoaders LOAD_TEXCOORD551, LOAD_TEXCOORD554
  MgEndVertexAttribute

MgEndVertexInput

MgBeginShader
  addiu $t0, $gp, %lo(RSPQ_DMEM_BUFFER)  -6          ## L:413  |      ^ | u32 inputs_ptr = get_cmd_address(0x2);
  ori $at, $zero, %lo(MG_VERTEX_SIZE)                ## L:414  |      2 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ldv $v02, 0, 0, $t0                                ## L:415  |      3 | vec16 vinputs = load(inputs_ptr).xyzw;
  ldv $v01, 0, 0, $at                                ## L:414  |      4 | vec16 vvtx_size = load(MG_VERTEX_SIZE).xyzw;
  ori $s4, $zero, %lo(MG_VERTEX_CACHE_END)           ## L:439  |   ***8 | vtx_in0_ptr = MG_VERTEX_CACHE_END;
  vmudh $v04, $v02, $v01.v                           ## L:417  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:418  |      9 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  vsar $v03, COP2_ACC_HI                             ## L:417  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  lw $s0, %lo(MG_VERTEX_BUFFER + 0)                  ## L:431  |     10 | u32<$s0> vtx_buffer = load(MG_VERTEX_BUFFER);
  vsar $v04, COP2_ACC_MD                             ## L:417  |      ^ | vec32 vproducts = vinputs * vvtx_size;
  ssv $v03, 0, 8, $at                                ## L:419  |   **13 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  sdv $v04, 0, 0, $at                                ## L:418  |     14 | @Barrier("inputs0") store(vproducts:ufract.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x1
  ssv $v04, 0, 10, $at                               ## L:419  |     15 | @Barrier("inputs1") store(vproducts.x, RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lw $t2, %lo(RSPQ_SCRATCH_MEM + 8)                  ## L:429  |     16 | @Barrier("inputs1") buffer_offset = load(RSPQ_SCRATCH_MEM, 0x8); ## Barrier: 0x2
  lhu $s7, %lo(RSPQ_SCRATCH_MEM + 2)                 ## L:425  |     17 | @Barrier("inputs0") vtx_out0_ptr = load(RSPQ_SCRATCH_MEM, 0x2); ## Barrier: 0x1
  lhu $t1, %lo(RSPQ_SCRATCH_MEM + 4)                 ## L:427  |     18 | @Barrier("inputs0") vertices_size = load(RSPQ_SCRATCH_MEM, 0x4); ## Barrier: 0x1
  addu $s0, $s0, $t2                                 ## L:432  |     19 | vtx_buffer += buffer_offset;
  andi $t3, $t2, 7                                   ## L:434  |     20 | u32 misalignment = buffer_offset & 0x7;
  addu $t0, $t1, $t3                                 ## L:435  |     21 | dma_size = vertices_size + misalignment;
  addiu $t2, $zero, 12                               ## L:442  |     22 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  addiu $t0, $t0, 7                                  ## L:436  |     23 | dma_size += 0x7;
  andi $t0, $t0, 4088                                ## L:437  |     24 | dma_size &= 0xFF8;
  subu $s4, $s4, $t0                                 ## L:440  |     25 | vtx_in0_ptr -= dma_size;
  jal DMAExec                                        ## L:442  |     26 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size); ## Args: $t0, $t1, $s0, $s4, $t2
  addiu $t0, $t0, -1                                 ## L:442  |    *28 | dma_in(vtx_in0_ptr, vtx_buffer, dma_size);
  ori $at, $zero, %lo(MATRICES_NORMAL)               ## L:461  |     29 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  lhu $t8, %lo(MG_VERTEX_SIZE + 0)                   ## L:444  |     30 | vtx_size = load(MG_VERTEX_SIZE);
  ori $t0, $zero, %lo(MATRICES_MVP)                  ## L:456  |     31 | u16 mvpaddr = MATRICES_MVP;
  ldv $v11, 0, 16, $at                               ## L:463  |     32 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v08, 0, 56, $t0                               ## L:401  |     33 | m3:ufract = load(address, 0x38).xyzwxyzw;
  addu $s6, $s4, $t1                                 ## L:447  |     34 | vtx_in_end = vtx_in0_ptr + vertices_size;
  ori $t4, $zero, %lo(MG_SCRATCH_MEM)                ## L:479  |     35 | u16 prev_out1 = MG_SCRATCH_MEM;
  ldv $v06, 0, 48, $t0                               ## L:399  |     36 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v09, 0, 0, $at                                ## L:461  |     37 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ldv $v11, 8, 16, $at                               ## L:463  |     38 | vmn2 = load(MATRICES_NORMAL, 0x10).xyzwxyzw;
  ldv $v10, 0, 8, $at                                ## L:462  |     39 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v03, 0, 8, $t0                                ## L:396  |     40 | m1:sint   = load(address, 0x08).xyzwxyzw;
  sll $t9, $t8, 1                                    ## L:445  |     41 | vtx_size2 = vtx_size * 2;
  ldv $v10, 8, 8, $at                                ## L:462  |     42 | vmn1 = load(MATRICES_NORMAL, 0x08).xyzwxyzw;
  ldv $v09, 8, 0, $at                                ## L:461  |     43 | vmn0 = load(MATRICES_NORMAL, 0x00).xyzwxyzw;
  ori $at, $zero, %lo(MG_NORMAL_MASK)                ## L:465  |     44 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  lhu $s0, %lo(MATERIAL_FLAGS + 0)                   ## L:503  |     45 | u16 mat_flags = load(MATERIAL_FLAGS);
  ldv $v12, 0, 0, $at                                ## L:465  |     46 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  addiu $s7, $s7, %lo(MG_VERTEX_CACHE)               ## L:448  |     47 | vtx_out0_ptr += MG_VERTEX_CACHE;
  ldv $v13, 0, 8, $at                                ## L:466  |     48 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v01, 0, 0, $t0                                ## L:394  |     49 | m0:sint   = load(address, 0x00).xyzwxyzw;
  addiu $fp, $zero, 80                               ## L:492  |     50 | u16 mat_ptr_emissive = 0x50;
  ldv $v13, 8, 8, $at                                ## L:466  |     51 | vec16 vnormfactor = load(MG_NORMAL_MASK,0x8).xyzwxyzw;
  ldv $v07, 0, 24, $t0                               ## L:400  |     52 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v12, 8, 0, $at                                ## L:465  |     53 | vec16 vnormmask = load(MG_NORMAL_MASK,0x0).xyzwxyzw;
  ori $at, $zero, %lo(FOG_PARAMS)                    ## L:473  |     54 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v04, 0, 40, $t0                               ## L:397  |     55 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v02, 0, 32, $t0                               ## L:395  |     56 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ori $t3, $zero, %lo(MG_SCRATCH_MEM)                ## L:478  |     57 | u16 prev_out0 = MG_SCRATCH_MEM;
  ldv $v14, 0, 0, $at                                ## L:473  |     58 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ldv $v05, 0, 16, $t0                               ## L:398  |     59 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v01, 8, 0, $t0                                ## L:394  |     60 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v03, 8, 8, $t0                                ## L:396  |     61 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v14, 8, 0, $at                                ## L:473  |     62 | vfog_parms = load(FOG_PARAMS).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_DIFFUSE)              ## L:495  |     63 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v22, 0, 0, $at                                ## L:495  |     64 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ldv $v02, 8, 32, $t0                               ## L:395  |     65 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 8, 40, $t0                               ## L:397  |     66 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v22, 8, 0, $at                                ## L:495  |     67 | vec16 vdiffuse = load(MATERIAL_DIFFUSE).xyzwxyzw;
  ori $at, $zero, %lo(MATERIAL_AMBIENT)              ## L:496  |     68 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  ldv $v05, 8, 16, $t0                               ## L:398  |     69 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v23, 0, 0, $at                                ## L:496  |     70 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  andi $s1, $s0, 1                                   ## L:504  |     71 | u16 tmp = mat_flags & 0x1;
  ldv $v23, 8, 0, $at                                ## L:496  |     72 | vec16 vambient = load(MATERIAL_AMBIENT).xyzwxyzw;
  addiu $t2, $s7, 40                                 ## L:476  |     73 | u16 vtx_out1_ptr = vtx_out0_ptr + 40;
  ori $at, $zero, %lo(MATERIAL_EMISSIVE)             ## L:497  |     74 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v24, 0, 0, $at                                ## L:497  |     75 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  ldv $v06, 8, 48, $t0                               ## L:399  |     76 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v24, 8, 0, $at                                ## L:497  |     77 | vec16 vemissive = load(MATERIAL_EMISSIVE).xyzwxyzw;
  addu $t1, $s4, $t8                                 ## L:475  |     78 | u16 vtx_in1_ptr = vtx_in0_ptr + vtx_size;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:499  |     79 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  sqv $v22, 0, 48, $at                               ## L:499  |     80 | store(vdiffuse, MG_SCRATCH_MEM, 0x30);
  ldv $v08, 8, 56, $t0                               ## L:401  |     81 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v07, 8, 24, $t0                               ## L:400  |     82 | m3:sint   = load(address, 0x18).xyzwxyzw;
  addiu $k1, $zero, 48                               ## L:490  |     83 | u16 mat_ptr_diffuse  = 0x30;
  addiu $sp, $zero, 64                               ## L:491  |     84 | u16 mat_ptr_ambient  = 0x40;
  lbu $t0, %lo(LIGHTING_HAS_POINTS + 0)              ## L:469  |     85 | u8 has_points = load(LIGHTING_HAS_POINTS);
  sqv $v23, 0, 64, $at                               ## L:500  |     86 | store(vambient, MG_SCRATCH_MEM, 0x40);
  beq $s1, $zero, LABEL_gl_pipeline_0001             ## L:505  |     87 | if (tmp != 0) {
  sqv $v24, 0, 80, $at                               ## L:501  |    *89 | store(vemissive, MG_SCRATCH_MEM, 0x50);
  addiu $k1, $zero, 96                               ## L:506  |     90 | mat_ptr_diffuse = 0x60;
  LABEL_gl_pipeline_0001:
  andi $s1, $s0, 2                                   ## L:508  |     91 | tmp = mat_flags & 0x2;
  beq $s1, $zero, LABEL_gl_pipeline_0002             ## L:509  |     92 | if (tmp != 0 ) {
  nop                                                ## L:509  |    *94 | if (tmp != 0 ) {
  addiu $sp, $zero, 96                               ## L:510  |     95 | mat_ptr_ambient = 0x60;
  LABEL_gl_pipeline_0002:
  andi $s1, $s0, 4                                   ## L:512  |     96 | tmp = mat_flags & 0x4;
  beq $s1, $zero, LABEL_gl_pipeline_0003             ## L:513  |     97 | if (tmp != 0) {
  nop                                                ## L:513  |    *99 | if (tmp != 0) {
  addiu $fp, $zero, 96                               ## L:514  |    100 | mat_ptr_emissive = 0x60;
  LABEL_gl_pipeline_0003:
  LOAD_POS_NORM518: ldv $v16, 0, 0, $s4              ## L:518  |    101 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM519: ldv $v16, 8, 0, $t1              ## L:519  |    102 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_POS_NORM520: ldv $v15, 0, 0, $s4              ## L:520  |    103 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM521: ldv $v15, 8, 0, $t1              ## L:521  |    104 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  LOAD_COLOR522: llv $v19, 0, 0, $s4                 ## L:522  |    105 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  LOAD_COLOR523: llv $v19, 4, 0, $t1                 ## L:523  |    106 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  VERTEX_LOOP:
  vand $v16, $v12, $v16.h3                           ## L:558  |      ^ | vnorm = vnormmask & vnorm.wwwwWWWW;
  ori $at, $zero, %lo(RSPQ_SCRATCH_MEM)              ## L:545  |    107 | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  suv $v18, 4, 8, $t4                                ## L:540  |   *109 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  sdv $v20, 8, 0, $t4                                ## L:532  |    110 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  sdv $v20, 0, 0, $t3                                ## L:531  |    111 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  vmudn $v16, $v16, $v13.v                           ## L:559  |      ^ | vnorm *= vnormfactor;
  vmudn $v29, $v02, $v15.h0                          ## L:562  |    112 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  slv $v17, 8, 12, $t4                               ## L:543  |      ^ | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  vmadh $v29, $v01, $v15.h0                          ## L:562  |    113 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  suv $v18, 0, 8, $t3                                ## L:539  |      ^ | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  vmadn $v29, $v06, $v15.h2                          ## L:563  |    114 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  sdv $v19, 0, 0, $at                                ## L:545  |      ^ | @Barrier("unpack_col") store(vrgba_in.xyzw, RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  vmadh $v29, $v05, $v15.h2                          ## L:563  |    115 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  luv $v19, 0, 0, $at                                ## L:546  |      ^ | @Barrier("unpack_col") vrgba_in = load_vec_u8(RSPQ_SCRATCH_MEM); ## Barrier: 0x100
  slv $v17, 0, 12, $t3                               ## L:542  |    116 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  LOAD_TEXCOORD551: llv $v17, 0, 0, $s4              ## L:551  |    117 | @AttrLoader("TEXCOORD") vtex.xy = load(vtx_in0_ptr).xy; ## Barrier: 0x7E00
  vmadn $v29, $v04, $v15.h1                          ## L:564  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ori $at, $zero, %lo(MG_SCRATCH_MEM)                ## L:547  |    118 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  vmadh $v29, $v03, $v15.h1                          ## L:564  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  LOAD_TEXCOORD554: llv $v17, 8, 0, $t1              ## L:554  |    119 | @AttrLoader("TEXCOORD") vtex.XY = load(vtx_in1_ptr).xy; ## Barrier: 0x7E00
  vmadn $v21, $v08, $v30.e2                          ## L:565  |      ^ | vpos_clip = vmvp3 +* 0x20;
  sqv $v19, 0, 96, $at                               ## L:547  |    120 | store(vrgba_in, MG_SCRATCH_MEM, 0x60);
  ori $at, $zero, %lo(MG_CLIP_FACTORS)               ## L:587  |    121 | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmadh $v20, $v07, $v30.e2                          ## L:565  |      ^ | vpos_clip = vmvp3 +* 0x20;
  vmulf $v29, $v09, $v16.h0                          ## L:567  |    122 | VTEMP = vmn0:sfract  * vnorm:sfract.xxxxXXXX;
  ldv $v22, 0, 0, $at                                ## L:587  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  vmacf $v29, $v10, $v16.h1                          ## L:568  |    123 | VTEMP = vmn1:sfract +* vnorm:sfract.yyyyYYYY;
  ldv $v22, 8, 0, $at                                ## L:587  |      ^ | vec16 vclip_factors = load(MG_CLIP_FACTORS).xyzwxyzw;
  sb $t5, 6($t3)                                     ## L:534  |  **126 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  vmacf $v16, $v11, $v16.h2                          ## L:569  |      ^ | vnorm = vmn2:sfract +* vnorm:sfract.zzzzZZZZ;
  sdv $v20, 8, 24, $t2                               ## L:598  |    127 | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  vmudn $v24, $v21, $v22.v                           ## L:588  |      ^ | vec32 vguard = vpos_clip * vclip_factors;
  vmadh $v23, $v20, $v22.v                           ## L:588  |    128 | vec32 vguard = vpos_clip * vclip_factors;
  sdv $v21, 8, 32, $t2                               ## L:598  |      ^ | @Barrier("st_cp1") store(vpos_clip.XYZW, vtx_out1_ptr, 24); ## Barrier: 0x400
  sdv $v20, 0, 24, $s7                               ## L:597  |    129 | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  sb $t6, 6($t4)                                     ## L:535  |    130 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  ori $at, $zero, %lo(MG_VIEWPORT)                   ## L:610  |   *132 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vch $v29, $v23, $v23.h3                            ## L:589  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  sb $t7, 7($t3)                                     ## L:536  |    133 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  vcl $v29, $v24, $v24.h3                            ## L:589  |      ^ | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  cfc2 $s0, $vcc                                     ## L:589  |    134 | u16 clip_codes = clip(vguard, vguard.wwwwWWWW);
  vch $v29, $v20, $v20.h3                            ## L:593  |      ^ | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vcl $v29, $v21, $v21.h3                            ## L:593  |    135 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  sdv $v21, 0, 32, $s7                               ## L:597  |      ^ | @Barrier("st_cp0") store(vpos_clip.xyzw, vtx_out0_ptr, 24); ## Barrier: 0x200
  vaddc $v23, $v21, $v14.h1                          ## L:602  |    136 | vec16 vtmp:ufract = vpos_clip:ufract + vfog_parms:ufract.yyyyYYYY;
  ldv $v24, 0, 8, $at                                ## L:611  |      ^ | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  andi $s2, $s0, 1799                                ## L:360  |    137 | t0 = codes & 0x707;
  srl $s1, $s0, 4                                    ## L:380  |    138 | u16 codes2 = codes >> 4;
  vadd $v22, $v20, $v14.h0                           ## L:603  |      ^ | vfog:sint = vpos_clip:sint + vfog_parms:sint.xxxxXXXX;
  srl $s3, $s2, 5                                    ## L:361  |    139 | t1 = t0 >> 5;
  or $t5, $s2, $s3                                   ## L:368  |    140 | result = t0 | t1;
  andi $s2, $s1, 1799                                ## L:360  |    141 | t0 = codes & 0x707;
  vmudn $v29, $v23, $v14.h2                          ## L:605  |      ^ | VTEMP = vtmp:ufract * vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 0, 0, $at                                ## L:610  |    142 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  vmadh $v22, $v22, $v14.h2                          ## L:606  |      ^ | vfog = vfog:sint +* vfog_parms:sint.zzzzZZZZ;
  ldv $v23, 8, 0, $at                                ## L:610  |    143 | vec16 vviewscale = load(MG_VIEWPORT, 0).xyzwxyzw;
  cfc2 $s1, $vcc                                     ## L:593  |  **146 | u16 tr_codes = clip(vpos_clip, vpos_clip.wwwwWWWW);
  vge $v22, $v00, $v22.h2                            ## L:607  |      ^ | vfog = max(VZERO, vfog.zzzzZZZZ);
  vmudl $v21, $v21, $v23.h3                          ## L:613  |    147 | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  srl $s3, $s2, 5                                    ## L:361  |      ^ | t1 = t0 >> 5;
  or $t6, $s2, $s3                                   ## L:368  |    148 | result = t0 | t1;
  ldv $v24, 8, 8, $at                                ## L:611  |    149 | vec16 vviewoff = load(MG_VIEWPORT, 8).xyzwxyzw;
  vmadm $v20, $v20, $v23.h3                          ## L:613  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  sb $k0, 7($t4)                                     ## L:537  |    150 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20
  vmadn $v21, $v00, $v00                             ## L:613  |      ^ | vpos_clip *= vviewscale:sfract.wwwwWWWW;
  andi $s3, $s1, 1799                                ## L:360  |    151 | t0 = codes & 0x707;
  srl $s5, $s3, 5                                    ## L:361  |    152 | t1 = t0 >> 5;
  nor $t7, $s3, $s5                                  ## L:375  |    153 | result = t0 ~| t1;
  vrcph $v25.e3, $v20.e3                             ## L:620  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  ssv $v20, 6, 16, $s7                               ## L:616  |    154 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vrcpl $v26.e3, $v21.e3                             ## L:620  |      ^ | vinvw.w = invert_half(vpos_clip).w;
  vrcph $v25.e3, $v20.e7                             ## L:621  |    155 | vinvw.W = invert_half(vpos_clip).W;
  srl $s2, $s1, 4                                    ## L:387  |      ^ | u16 codes2 = codes >> 4;
  vrcpl $v26.e7, $v21.e7                             ## L:621  |    156 | vinvw.W = invert_half(vpos_clip).W;
  andi $s3, $s2, 1799                                ## L:360  |      ^ | t0 = codes & 0x707;
  vrcph $v25.e7, $v00.e7                             ## L:621  |    157 | vinvw.W = invert_half(vpos_clip).W;
  ssv $v20, 14, 16, $t2                              ## L:617  |      ^ | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  srl $s5, $s3, 5                                    ## L:361  |    158 | t1 = t0 >> 5;
  ssv $v21, 14, 18, $t2                              ## L:617  |    159 | @Barrier("st_w1") store(vpos_clip.W, vtx_out1_ptr, 16); ## Barrier: 0x1000
  ssv $v21, 6, 18, $s7                               ## L:616  |    160 | @Barrier("st_w0") store(vpos_clip.w, vtx_out0_ptr, 16); ## Barrier: 0x800
  vmudl $v29, $v21, $v26.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 6, 20, $s7                               ## L:623  |    161 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  vmadm $v29, $v20, $v26.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v25, 14, 20, $t2                              ## L:624  |    162 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadn $v21, $v21, $v25.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v26, 14, 22, $t2                              ## L:624  |    163 | @Barrier("st_iw1") store(vinvw.W, vtx_out1_ptr, 20); ## Barrier: 0x4000
  vmadh $v20, $v20, $v25.h3                          ## L:626  |      ^ | vpos_clip *= vinvw.wwwwWWWW;
  ssv $v26, 6, 22, $s7                               ## L:623  |    164 | @Barrier("st_iw0") store(vinvw.w, vtx_out0_ptr, 20); ## Barrier: 0x2000
  nor $k0, $s3, $s5                                  ## L:375  |    165 | result = t0 ~| t1;
  vmudh $v29, $v24, $v30.e7                          ## L:630  |      ^ | VTEMP = vviewoff:sint * 1;
  vmadn $v21, $v21, $v23.v                           ## L:631  |    166 | vpos_clip = vpos_clip +* vviewscale;
  beq $t0, $zero, LABEL_gl_pipeline_0004             ## L:637  |      ^ | if (has_points != 0)
  vmadh $v20, $v20, $v23.v                           ## L:631  |   *168 | vpos_clip = vpos_clip +* vviewscale;
  ori $s0, $zero, %lo(MATRICES_MV)                   ## L:640  |    169 | u16 addr = MATRICES_MV;
  ldv $v02, 0, 32, $s0                               ## L:395  |    170 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v01, 0, 0, $s0                                ## L:394  |    171 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 8, 0, $s0                                ## L:394  |    172 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v02, 8, 32, $s0                               ## L:395  |    173 | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v04, 0, 40, $s0                               ## L:397  |    174 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v04, 8, 40, $s0                               ## L:397  |    175 | m1:ufract = load(address, 0x28).xyzwxyzw;
  ldv $v05, 0, 16, $s0                               ## L:398  |    176 | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v03, 0, 8, $s0                                ## L:396  |    177 | m1:sint   = load(address, 0x08).xyzwxyzw;
  ldv $v05, 8, 16, $s0                               ## L:398  |    178 | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmudn $v29, $v02, $v15.h0                          ## L:643  |      ^ | VTEMP = vmvp0  * vpos.xxxxXXXX;
  vmadh $v29, $v01, $v15.h0                          ## L:643  |    179 | VTEMP = vmvp0  * vpos.xxxxXXXX;
  ldv $v06, 0, 48, $s0                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v06, 8, 48, $s0                               ## L:399  |    180 | m2:ufract = load(address, 0x30).xyzwxyzw;
  ldv $v03, 8, 8, $s0                                ## L:396  |    181 | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmadn $v29, $v04, $v15.h1                          ## L:644  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  ldv $v08, 0, 56, $s0                               ## L:401  |    182 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v07, 0, 24, $s0                               ## L:400  |    183 | m3:sint   = load(address, 0x18).xyzwxyzw;
  ldv $v08, 8, 56, $s0                               ## L:401  |    184 | m3:ufract = load(address, 0x38).xyzwxyzw;
  ldv $v07, 8, 24, $s0                               ## L:400  |    185 | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadh $v29, $v03, $v15.h1                          ## L:644  |      ^ | VTEMP = vmvp1 +* vpos.yyyyYYYY;
  vmadn $v29, $v06, $v15.h2                          ## L:645  |    186 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadh $v29, $v05, $v15.h2                          ## L:645  |    187 | VTEMP = vmvp2 +* vpos.zzzzZZZZ;
  vmadn $v29, $v08, $v30.e2                          ## L:646  |    188 | vpos = vmvp3 +* 0x20;
  vmadh $v15, $v07, $v30.e2                          ## L:646  |    189 | vpos = vmvp3 +* 0x20;
  LABEL_gl_pipeline_0004:
  ori $at, $zero, %lo(LIGHTING_AMBIENT)              ## L:649  |      ^ | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ldv $v18, 0, 0, $at                                ## L:649  |    190 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  ori $s1, $zero, %lo(LIGHTING_LIGHTS)               ## L:662  |    191 | u16 light_ptr = LIGHTING_LIGHTS;
  ldv $v18, 8, 0, $at                                ## L:649  |    192 | vrgba = load(LIGHTING_AMBIENT).xyzwxyzw;
  addiu $at, $k1, MG_SCRATCH_MEM                     ## L:651  |    193 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  lbu $s0, %lo(LIGHTING_COUNT + 0)                   ## L:660  |    194 | u8 light_count = load(LIGHTING_COUNT);
  lqv $v23, 0, 0, $at                                ## L:651  |    195 | vec16 vdiffuse = load(mat_ptr_diffuse, MG_SCRATCH_MEM);
  addiu $at, $sp, MG_SCRATCH_MEM                     ## L:652  |    196 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  lqv $v24, 0, 0, $at                                ## L:652  |    197 | vec16 vambient = load(mat_ptr_ambient, MG_SCRATCH_MEM);
  addiu $at, $fp, MG_SCRATCH_MEM                     ## L:653  |    198 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  lqv $v25, 0, 0, $at                                ## L:653  |    199 | vec16 vemissive = load(mat_ptr_emissive, MG_SCRATCH_MEM);
  vmudh $v29, $v25, $v30.e7                          ## L:655  | ***203 | VTEMP = vemissive:sint * 1;
  beq $s0, $zero, LIGHT_END                          ## L:663  |      ^ | if (light_count == 0) goto LIGHT_END;
  vmacf $v18, $v18, $v24.v                           ## L:656  |   *205 | vrgba:sfract = vrgba:sfract +* vambient:sfract;
  LIGHT_LOOP:
  ldv $v26, 0, 8, $s1                                ## L:668  |    206 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  addiu $s2, $s1, 14                                 ## L:666  |    207 | u16 amb_ptr = light_ptr + 14;
  ldv $v25, 0, 0, $s2                                ## L:667  |    208 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  ldv $v26, 8, 8, $s1                                ## L:668  |    209 | vec16 vlcol = load(light_ptr, 8).xyzwxyzw;
  ldv $v25, 8, 0, $s2                                ## L:667  |    210 | vec16 vlambient = load(amb_ptr, 0).xyzwxyzw;
  lh $s3, 6($s1)                                     ## L:673  |    211 | s16 light_w = load(light_ptr, 6);
  ldv $v27, 0, 0, $s1                                ## L:669  |    212 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  ldv $v27, 8, 0, $s1                                ## L:669  |    213 | vec16 vlpos = load(light_ptr, 0).xyzwxyzw;
  beq $s3, $zero, LABEL_gl_pipeline_0006             ## L:674  |    214 | if (light_w != 0)
  vmulf $v26, $v26, $v23.v                           ## L:671  |   *216 | vlcol:sfract *= vdiffuse;
  vsubc $v27, $v27, $v15.v                           ## L:688  |    217 | vlpos -= vpos;
  addiu $s5, $s1, 20                                 ## L:682  |      ^ | u16 att_ptr = light_ptr + 20;
  ldv $v01, 0, 0, $s5                                ## L:684  |    218 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  ldv $v01, 8, 0, $s5                                ## L:684  |  **221 | vatten:sint = load(att_ptr, 0).xyzwxyzw;
  vmudh $v04, $v27, $v27.v                           ## L:689  |      ^ | vec32 vsqdist = vlpos * vlpos;
  addiu $s5, $s5, 6                                  ## L:685  |    222 | att_ptr += 6;
  vsar $v04, COP2_ACC_MD                             ## L:689  |      ^ | vec32 vsqdist = vlpos * vlpos;
  ldv $v02, 0, 0, $s5                                ## L:686  |    223 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vsar $v03, COP2_ACC_HI                             ## L:689  |      ^ | vec32 vsqdist = vlpos * vlpos;
  ldv $v02, 8, 0, $s5                                ## L:686  |  **226 | vatten:ufract = load(att_ptr, 0).xyzwxyzw;
  vaddc $v06, $v04, $v04.h1                          ## L:690  |      ^ | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vadd $v05, $v03, $v03.h1                           ## L:690  |    227 | vec32 vtmp = vsqdist + vsqdist.yyyyYYYY;
  vaddc $v04, $v06, $v04.h2                          ## L:691  |  **230 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vadd $v03, $v05, $v03.h2                           ## L:691  |    231 | vsqdist = vtmp + vsqdist.zzzzZZZZ;
  vrsqh $v05.e0, $v03.e0                             ## L:693  | ***235 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsql $v06.e0, $v04.e0                             ## L:693  |    236 | vtmp.x = invert_half_sqrt(vsqdist).x;
  vrsqh $v05.e0, $v03.e4                             ## L:694  |    237 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsql $v06.e4, $v04.e4                             ## L:694  |    238 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vrsqh $v05.e4, $v00.e0                             ## L:694  |    239 | vtmp.X = invert_half_sqrt(vsqdist).X;
  vmudl $v29, $v06, $v04.v                           ## L:696  |  **242 | vec32 vdist = vtmp * vsqdist;
  vmadm $v29, $v05, $v04.v                           ## L:696  |    243 | vec32 vdist = vtmp * vsqdist;
  vmadn $v08, $v06, $v03.v                           ## L:696  |    244 | vec32 vdist = vtmp * vsqdist;
  vmadh $v07, $v05, $v03.v                           ## L:696  |    245 | vec32 vdist = vtmp * vsqdist;
  ori $s5, $zero, %lo(MATRICES_MVP)                  ## L:716  |      ^ | u16 addr = MATRICES_MVP;
  vmudm $v29, $v27, $v06.h0                          ## L:698  |    246 | vlpos *= vtmp.xxxxXXXX;
  vmadh $v27, $v27, $v05.h0                          ## L:698  |    247 | vlpos *= vtmp.xxxxXXXX;
  ldv $v06, 0, 48, $s5                               ## L:399  |      ^ | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmudn $v29, $v02, $v30.e7                          ## L:700  |    248 | VTEMP = vatten * 1;
  ldv $v05, 8, 16, $s5                               ## L:398  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  vmadh $v29, $v01, $v30.e7                          ## L:700  |    249 | VTEMP = vatten * 1;
  ldv $v05, 0, 16, $s5                               ## L:398  |      ^ | m2:sint   = load(address, 0x10).xyzwxyzw;
  ldv $v06, 8, 48, $s5                               ## L:399  |    250 | m2:ufract = load(address, 0x30).xyzwxyzw;
  vmadl $v29, $v08, $v02.h1                          ## L:701  |      ^ | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadm $v29, $v07, $v02.h1                          ## L:701  |    251 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadn $v29, $v08, $v01.h1                          ## L:701  |    252 | VTEMP = vdist +* vatten.yyyyYYYY;
  vmadh $v29, $v07, $v01.h1                          ## L:701  |    253 | VTEMP = vdist +* vatten.yyyyYYYY;
  ldv $v07, 0, 24, $s5                               ## L:400  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vmadl $v29, $v04, $v02.h2                          ## L:702  |    254 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v08, 0, 56, $s5                               ## L:401  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vmadm $v29, $v03, $v02.h2                          ## L:702  |    255 | vatten = vsqdist +* vatten.zzzzZZZZ;
  vmadn $v02, $v04, $v01.h2                          ## L:702  |    256 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v04, 8, 40, $s5                               ## L:397  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vmadh $v01, $v03, $v01.h2                          ## L:702  |    257 | vatten = vsqdist +* vatten.zzzzZZZZ;
  ldv $v08, 8, 56, $s5                               ## L:401  |      ^ | m3:ufract = load(address, 0x38).xyzwxyzw;
  vrcph $v01.e0, $v01.e0                             ## L:704  | ***261 | vatten.x = invert_half(vatten).x;
  ldv $v07, 8, 24, $s5                               ## L:400  |      ^ | m3:sint   = load(address, 0x18).xyzwxyzw;
  vrcpl $v02.e0, $v02.e0                             ## L:704  |    262 | vatten.x = invert_half(vatten).x;
  ldv $v03, 0, 8, $s5                                ## L:396  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vrcph $v01.e0, $v01.e4                             ## L:705  |  **265 | vatten.X = invert_half(vatten).X;
  vrcpl $v02.e4, $v02.e4                             ## L:705  |    266 | vatten.X = invert_half(vatten).X;
  ldv $v04, 0, 40, $s5                               ## L:397  |      ^ | m1:ufract = load(address, 0x28).xyzwxyzw;
  vrcph $v01.e4, $v00.e4                             ## L:705  |    267 | vatten.X = invert_half(vatten).X;
  ldv $v03, 8, 8, $s5                                ## L:396  |      ^ | m1:sint   = load(address, 0x08).xyzwxyzw;
  vmudm $v29, $v26, $v02.h0                          ## L:707  |  **270 | vlcol *= vatten.xxxxXXXX;
  ldv $v02, 0, 32, $s5                               ## L:395  |      ^ | m0:ufract = load(address, 0x20).xyzwxyzw;
  ldv $v02, 8, 32, $s5                               ## L:395  |    271 | m0:ufract = load(address, 0x20).xyzwxyzw;
  vmadh $v26, $v26, $v01.h0                          ## L:707  |      ^ | vlcol *= vatten.xxxxXXXX;
  ldv $v01, 0, 0, $s5                                ## L:394  |    272 | m0:sint   = load(address, 0x00).xyzwxyzw;
  ldv $v01, 8, 0, $s5                                ## L:394  |    273 | m0:sint   = load(address, 0x00).xyzwxyzw;
  LABEL_gl_pipeline_0006:
  vmulf $v27, $v27, $v16.v                           ## L:721  |      ^ | vlpos *= vnorm:sfract;
  vmudn $v29, $v25, $v24.v                           ## L:722  |    274 | VTEMP = vlambient * vambient;
  vmacf $v29, $v26, $v27.h0                          ## L:723  |  **277 | VTEMP = vlcol:sfract +* vlpos:sfract.xxxxXXXX;
  addiu $s1, $s1, 32                                 ## L:730  |      ^ | light_ptr += 32;
  addiu $s0, $s0, 65535                              ## L:731  |    278 | light_count -= 1;
  vmacf $v29, $v26, $v27.h1                          ## L:724  |      ^ | VTEMP = vlcol:sfract +* vlpos:sfract.yyyyYYYY;
  vmacf $v26, $v26, $v27.h2                          ## L:725  |    279 | vlcol = vlcol:sfract +* vlpos:sfract.zzzzZZZZ;
  vge $v26, $v26, $v00                               ## L:726  | ***283 | vlcol = max(vlcol, VZERO);
  bne $s0, $zero, LIGHT_LOOP                         ## L:732  |      ^ | if (light_count != 0) goto LIGHT_LOOP;
  vadd $v18, $v18, $v26.v                            ## L:728  | ****288 | vrgba:sint += vlcol;
  LIGHT_END:
  lbu $s2, %lo(FOG_MASK + 0)                         ## L:743  |    289 | u8 fog_mask = load(FOG_MASK);
  vmov $v18.e3, $v23.e3                              ## L:736  |      ^ | vrgba.w = vdiffuse.w;
  ori $at, $zero, %lo(TEXTURING_SCALE)               ## L:799  |    290 | vtexscale:sint.xy = load(TEXTURING_SCALE).xy;
  vmov $v18.e7, $v23.e7                              ## L:737  |      ^ | vrgba.W = vdiffuse.W;
  llv $v23, 0, 4, $at                                ## L:800  |    291 | vtexscale:ufract.xy = load(TEXTURING_SCALE).zw;
  ctc2 $s2, $vcc                                     ## L:744  |    292 | set_vcc(fog_mask);
  or $t3, $zero, $s7                                 ## L:812  |    293 | prev_out0 = vtx_out0_ptr;
  addu $t1, $t1, $t9                                 ## L:815  |    294 | vtx_in1_ptr += vtx_size2;
  vmrg $v18, $v18, $v22.h2                           ## L:745  |      ^ | vrgba = select(vrgba, vfog.zzzzZZZZ);
  llv $v22, 0, 0, $at                                ## L:799  |    295 | vtexscale:sint.xy = load(TEXTURING_SCALE).xy;
  llv $v23, 8, 4, $at                                ## L:802  |    296 | vtexscale:ufract.XY = load(TEXTURING_SCALE).zw;
  llv $v22, 8, 0, $at                                ## L:801  |    297 | vtexscale:sint.XY = load(TEXTURING_SCALE).xy;
  ori $at, $zero, %lo(TEXTURING_OFFSET)              ## L:803  |    298 | vtexoffset:sint.xy = load(TEXTURING_OFFSET).xy;
  or $t4, $zero, $t2                                 ## L:813  |    299 | prev_out1 = vtx_out1_ptr;
  addu $s4, $s4, $t9                                 ## L:814  |    300 | vtx_in0_ptr += vtx_size2;
  llv $v25, 0, 4, $at                                ## L:804  |    301 | vtexoffset:ufract.xy = load(TEXTURING_OFFSET).zw;
  llv $v25, 8, 4, $at                                ## L:806  |    302 | vtexoffset:ufract.XY = load(TEXTURING_OFFSET).zw;
  llv $v24, 0, 0, $at                                ## L:803  |    303 | vtexoffset:sint.xy = load(TEXTURING_OFFSET).xy;
  llv $v24, 8, 0, $at                                ## L:805  |    304 | vtexoffset:sint.XY = load(TEXTURING_OFFSET).xy;
  LOAD_POS_NORM819: ldv $v16, 0, 0, $s4              ## L:819  |    305 | @AttrLoader("POS_NORM") vnorm.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_COLOR824: llv $v19, 4, 0, $t1                 ## L:824  |    306 | @AttrLoader("COLOR") vrgba_in.zw = load(vtx_in1_ptr).xy;
  vmudn $v29, $v25, $v30.e7                          ## L:809  |      ^ | VTEMP = vtexoffset * 1;
  LOAD_POS_NORM821: ldv $v15, 0, 0, $s4              ## L:821  |    307 | @AttrLoader("POS_NORM") vpos.xyzw = load(vtx_in0_ptr).xyzw;
  LOAD_POS_NORM822: ldv $v15, 8, 0, $t1              ## L:822  |    308 | @AttrLoader("POS_NORM") vpos.XYZW = load(vtx_in1_ptr).xyzw;
  vmadh $v29, $v24, $v30.e7                          ## L:809  |      ^ | VTEMP = vtexoffset * 1;
  LOAD_POS_NORM820: ldv $v16, 8, 0, $t1              ## L:820  |    309 | @AttrLoader("POS_NORM") vnorm.XYZW = load(vtx_in1_ptr).xyzw;
  vmadm $v29, $v17, $v23.v                           ## L:810  |      ^ | vtex = vtex +* vtexscale;
  sltu $at, $s4, $s6                                 ## L:826  |    310 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  LOAD_COLOR823: llv $v19, 0, 0, $s4                 ## L:823  |    311 | @AttrLoader("COLOR") vrgba_in.xy = load(vtx_in0_ptr).xy;
  addiu $t2, $t2, 80                                 ## L:817  |    312 | vtx_out1_ptr += 80;
  vmadh $v17, $v17, $v22.v                           ## L:810  |      ^ | vtex = vtex +* vtexscale;
  bne $at, $zero, VERTEX_LOOP                        ## L:826  |    313 | if (vtx_in0_ptr < vtx_in_end) goto VERTEX_LOOP;
  addiu $s7, $s7, 80                                 ## L:816  |   *315 | vtx_out0_ptr += 80;
  sdv $v20, 8, 0, $t4                                ## L:833  |    316 | @Barrier("st_c1") @Barrier("st_tr1") store(vpos_clip:sint.XYZW, prev_out1, 0); ## Barrier: 0x30
  suv $v18, 0, 8, $t3                                ## L:840  |    317 | @Barrier("st_tex0") store_vec_u8(vrgba.x, prev_out0, 8); ## Barrier: 0x40
  sdv $v20, 0, 0, $t3                                ## L:832  |    318 | @Barrier("st_c0") @Barrier("st_tr0") store(vpos_clip:sint.xyzw, prev_out0, 0); ## Barrier: 0xC
  slv $v17, 0, 12, $t3                               ## L:843  |    319 | @Barrier("st_tex0") store(vtex.xy, prev_out0, 12); ## Barrier: 0x40
  suv $v18, 4, 8, $t4                                ## L:841  |    320 | @Barrier("st_tex1") store_vec_u8(vrgba.X, prev_out1, 8); ## Barrier: 0x80
  slv $v17, 8, 12, $t4                               ## L:844  |    321 | @Barrier("st_tex1") store(vtex.XY, prev_out1, 12); ## Barrier: 0x80
  sb $t7, 7($t3)                                     ## L:837  |    322 | @Barrier("st_tr0") store(tr0, prev_out0, 7); ## Barrier: 0x8
  sb $t6, 6($t4)                                     ## L:836  |    323 | @Barrier("st_c1") store(c1, prev_out1, 6); ## Barrier: 0x10
  sb $t5, 6($t3)                                     ## L:835  |    324 | @Barrier("st_c0") store(c0, prev_out0, 6); ## Barrier: 0x4
  j RSPQ_Loop                                        ## L:845  |    325 | }
  sb $k0, 7($t4)                                     ## L:838  |   *327 | @Barrier("st_tr1") store(tr1, prev_out1, 7); ## Barrier: 0x20

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