/**
 * @file gl_constants.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_CONSTANTS
#define __GL_CONSTANTS

#define PIPELINE_FEATURE_COUNT  1
#define PIPELINE_COUNT          (1<<PIPELINE_FEATURE_COUNT)

#define MODELVIEW_STACK_SIZE  32
#define PROJECTION_STACK_SIZE 2
#define TEXTURE_STACK_SIZE    2
#define PALETTE_STACK_SIZE    1

#define VERTEX_UNIT_COUNT     1
#define MATRIX_PALETTE_SIZE   32

#define VERTEX_CACHE_SIZE     32

#define CLIPPING_PLANE_COUNT  6
#define CLIPPING_CACHE_SIZE   9
#define CLIPPING_PLANE_SIZE   8

#define MATRIX_SIZE           64

#define TEX_COORD_COUNT         4
#define TEX_GEN_COUNT           TEX_COORD_COUNT
#define TEX_GEN_PLANE_COUNT     2
#define TEX_GEN_SIZE            34

#define TEX_GEN_STRUCT_SIZE     144
#define TEX_GEN_INTEGER_OFFSET  0
#define TEX_GEN_FRACTION_OFFSET 64
#define TEX_GEN_MODE_OFFSET     128
#define TEX_GEN_MODE_T_OFFSET   130
#define TEX_GEN_CONST_SIZE      (4*2)

#define RDP_TEX_SHIFT       5

#define LIGHT_COUNT           8

#define MAX_TEXTURE_SIZE      64
#define MAX_TEXTURE_LEVELS    7

#define TEXTURE_IMAGE_SIZE          6
#define TEXTURE_OBJECT_PROPS_OFFSET (TEXTURE_IMAGE_SIZE * MAX_TEXTURE_LEVELS)

#define TEXTURE_LEVELS_COUNT_OFFSET     (TEXTURE_OBJECT_PROPS_OFFSET + 0)
#define TEXTURE_TLUT_MODE_OFFSET        (TEXTURE_OBJECT_PROPS_OFFSET + 1)
#define TEXTURE_LEVELS_BLOCK_OFFSET     (TEXTURE_OBJECT_PROPS_OFFSET + 2)
#define TEXTURE_FLAGS_OFFSET            (TEXTURE_OBJECT_PROPS_OFFSET + 62)
#define TEXTURE_MIN_FILTER_OFFSET       (TEXTURE_OBJECT_PROPS_OFFSET + 66)
#define TEXTURE_MAG_FILTER_OFFSET       (TEXTURE_OBJECT_PROPS_OFFSET + 68)

#define TEXTURE_OBJECT_SIZE         (TEXTURE_OBJECT_PROPS_OFFSET + 70)

#define IMAGE_WIDTH_OFFSET              0
#define IMAGE_HEIGHT_OFFSET             2
#define IMAGE_INTERNAL_FORMAT_OFFSET    4

#define TEXTURE_BILINEAR_MASK       0x001
#define TEXTURE_INTERPOLATE_MASK    0x002
#define TEXTURE_MIPMAP_MASK         0x100

#define MAX_PIXEL_MAP_SIZE    32

#define ATTRIB_TYPE_COUNT     10

#define FLAG_DITHER             (1 << 0)
#define FLAG_BLEND              (1 << 1)
#define FLAG_ALPHA_TEST         (1 << 2)
#define FLAG_FOG                (1 << 3)
#define FLAG_SCISSOR_TEST       (1 << 4)
#define FLAG_TEXTURE_1D         (1 << 5)
#define FLAG_TEXTURE_2D         (1 << 6)
#define FLAG_CULL_FACE          (1 << 7)
#define FLAG_TEX_GEN_LINEAR     (1 << 8)
#define FLAG_TEX_GEN_SPHERICAL  (1 << 9)
#define FLAG_LIGHTING           (1 << 10)
#define FLAG_COLOR_MATERIAL     (1 << 11)
#define FLAG_NORMALIZE          (1 << 12)
#define FLAG_NEED_EYE_SPACE     (1 << 13)
#define FLAG_MATRIX_PALETTE     (1 << 14)
#define FLAG_TEX_MATRIX         (1 << 15)
#define FLAG_DEPTH_TEST         (1 << 16)
#define FLAG_TEXTURE_ACTIVE     (1 << 17)
#define FLAG_DEPTH_MASK         (1 << 18)
#define FLAG_FINAL_MTX_DIRTY    (1 << 19)
#define FLAG_LIGHT0             (1 << 20)
#define FLAG_LIGHT1             (1 << 21)
#define FLAG_LIGHT2             (1 << 22)
#define FLAG_LIGHT3             (1 << 23)
#define FLAG_LIGHT4             (1 << 24)
#define FLAG_LIGHT5             (1 << 25)
#define FLAG_LIGHT6             (1 << 26)
#define FLAG_LIGHT7             (1 << 27)
#define FLAG_TEX_GEN_S          (1 << 28)
#define FLAG_TEX_GEN_T          (1 << 29)
#define FLAG_TEX_GEN_R          (1 << 30)
#define FLAG_TEX_GEN_Q          (1 << 31)

#define FLAG2_USE_RDPQ_MATERIAL  (1 << 0)
#define FLAG2_USE_RDPQ_TEXTURING (1 << 1)
#define FLAG2_REDUCED_ALIASING   (1 << 2)
#define FLAG2_MULTISAMPLE        (1 << 3)
#define FLAG2_TEX_FLIP_T         (1 << 4)

#define TEX_FLAG_COMPLETE       (1 << 0)
#define TEX_FLAG_UPLOAD_DIRTY   (1 << 1)
#define TEX_FLAG_FORCE_COMPLETE (1 << 2)
#define TEX_FLAG_DETAIL         (1 << 3)

#define DITHER_MASK         (SOM_RGBDITHER_MASK | SOM_ALPHADITHER_MASK)
#define BLEND_MASK          SOM_ZMODE_MASK
#define DEPTH_TEST_MASK     SOM_Z_COMPARE
#define DEPTH_MASK_MASK     SOM_Z_WRITE
#define POINTS_MASK         (SOM_ZSOURCE_MASK | SOM_TEXTURE_PERSP)
#define ALPHA_TEST_MASK     SOM_ALPHACOMPARE_MASK

#define LOAD_TILE 7

#define GUARD_BAND_FACTOR 2

#define MULTISAMPLE_FLAG_SHIFT      2
#define ZMODE_BLEND_FLAG_SHIFT      10

#define COMBINER_FLAG_CONSTANT      (1 << 0)
#define COMBINER_FLAG_REPLACE       (1 << 1)
#define COMBINER_FLAG_TEXTURE       (1 << 2)

#define TEXTURE_ACTIVE_SHIFT        17
#define TEX_ACTIVE_COMBINER_SHIFT   (TEXTURE_ACTIVE_SHIFT - 2)

#define TEX_COORD_SHIFT             3
#define HALF_TEXEL                  0x0010

#define TEX_BILINEAR_SHIFT          13
#define TEX_BILINEAR_OFFSET_SHIFT   4

#define TEX_DETAIL_SHIFT            15

#define BILINEAR_TEX_OFFSET_SHIFT   9

#define TRICMD_ATTR_SHIFT       8
#define TRICMD_ATTR_MASK        0x300

#define LIGHT0_SHIFT            20

#define TEX_GEN_S_SHIFT                 28
#define TEX_GEN_LINEAR_FLAG_SHIFT       8
#define TEX_GEN_SPHERICAL_FLAG_SHIFT    9

#define NEED_EYE_SPACE_SHIFT    13

#define RDPQ_TEXTURING_MASK ((SOM_SAMPLE_MASK | SOM_TEXTURE_LOD | SOMX_LOD_INTERPOLATE | SOMX_NUMLODS_MASK | SOM_TLUT_MASK)>>32)

#define PALETTE_MATRIX_INDEX    3

#define PALETTE_DIRTY_FLAGS_SIZE    ((MATRIX_PALETTE_SIZE+7)>>3)

#endif
