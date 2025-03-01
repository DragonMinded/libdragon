/**
 * @file magma_constants.h
 * @brief Constant definitions for magma
 * @ingroup magma
 */

#ifndef __MAGMA_CONSTANTS
#define __MAGMA_CONSTANTS

/** 
 * @brief The maximum number of vertices that the vertex cache can hold.
 * 
 * @see #mg_load_vertices
 * @see #mg_draw_triangle
 */
#define MG_VERTEX_CACHE_COUNT           48

/** @brief Default value of the clipping guard factor. 
 * 
 * @see #mg_set_clip_factor
 */
#define MG_DEFAULT_GUARD_BAND           2

/// @cond

#define MG_INLINE_UNIFORM_HEADER        12
#define MG_MAX_UNIFORM_PAYLOAD_SIZE     (RSPQ_MAX_COMMAND_SIZE*4 - MG_INLINE_UNIFORM_HEADER)

#define MG_CLIP_PLANE_COUNT             6
#define MG_CLIP_PLANE_SIZE              8
#define MG_CLIP_CACHE_SIZE              9

#define MG_VTX_XYZ                      0
#define MG_VTX_CLIP_CODE                6
#define MG_VTX_TR_CODE                  7
#define MG_VTX_RGBA                     8
#define MG_VTX_ST                       12
#define MG_VTX_Wi                       16
#define MG_VTX_Wf                       18
#define MG_VTX_INVWi                    20
#define MG_VTX_INVWf                    22
#define MG_VTX_CS_POSi                  24
#define MG_VTX_CS_POSf                  32
#define MG_VTX_SIZE                     40
#define MG_VTX_SIZE2                    80

/// @endcond

#endif
