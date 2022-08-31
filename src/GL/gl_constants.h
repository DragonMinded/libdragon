#ifndef __GL_CONSTANTS
#define __GL_CONSTANTS

#define MODELVIEW_STACK_SIZE  32
#define PROJECTION_STACK_SIZE 2
#define TEXTURE_STACK_SIZE    2

#define VERTEX_CACHE_SIZE     16

#define CLIPPING_PLANE_COUNT  6
#define CLIPPING_CACHE_SIZE   9

#define LIGHT_COUNT           8

#define MAX_TEXTURE_SIZE      64
#define MAX_TEXTURE_LEVELS    7

#define MAX_PIXEL_MAP_SIZE    32

#define FLAG_DITHER         (1 << 0)
#define FLAG_BLEND          (1 << 1)
#define FLAG_DEPTH_TEST     (1 << 2)
#define FLAG_DEPTH_MASK     (1 << 3)
#define FLAG_ALPHA_TEST     (1 << 4)
#define FLAG_FOG            (1 << 5)
#define FLAG_MULTISAMPLE    (1 << 6)
#define FLAG_SCISSOR_TEST   (1 << 7)

#define DITHER_MASK         (SOM_RGBDITHER_MASK | SOM_ALPHADITHER_MASK)
#define BLEND_MASK          SOM_ZMODE_MASK
#define DEPTH_TEST_MASK     SOM_Z_COMPARE
#define DEPTH_MASK_MASK     SOM_Z_WRITE
#define POINTS_MASK         (SOM_ZSOURCE_MASK | SOM_TEXTURE_PERSP)
#define ALPHA_TEST_MASK     SOM_ALPHACOMPARE_MASK

#endif
