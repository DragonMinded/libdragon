/**
 * @file mgfx.h
 * @brief Interface for magma's "fixed function" builtin shader
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_H
#define __LIBDRAGON_MGFX_H

#include <magma.h>
#include <mgfx_constants.h>
#include <mgfx_macros.h>
#include <fgeom.h>

typedef enum
{
    MGFX_FEATURE_ENV_MAP = (1<<0)
} mgfx_features_t;

/** @brief Data structure of a single matrix in the matrices uniform. */
typedef struct
{
    int16_t  i[16];     ///< Integer parts of the matrix coefficients.
    uint16_t f[16];     ///< Fractional parts of the matrix coefficients.
} __attribute__((packed, aligned(16))) mgfx_matrix_t;

/** @brief Data structure of the matrices uniform. */
typedef struct
{
    mgfx_matrix_t mvp;      ///< The model-view-projection matrix.
    mgfx_matrix_t mv;       ///< The model-view matrix.
    int16_t normal[16];     ///< The normal matrix.
} __attribute__((packed, aligned(16))) mgfx_matrices_t;

/** @brief Data structure of the texturing uniform. */
typedef struct
{
    int16_t tex_scale[2];       ///< Texture coordinate scale.
    int16_t tex_offset[2];      ///< Texture coordinate offset.
} __attribute__((packed, aligned(16))) mgfx_texturing_t;

/** @brief Data structure of a single light in the lighting uniform. */
typedef struct
{
    int16_t position[4];            ///< The light's position.
    int16_t color[4];               ///< The light's color.
    int16_t attenuation_int[4];     ///< Integer parts of the attenuation coefficients.
    uint16_t attenuation_frac[4];   ///< Fractional parts of the attenuation coefficients.
} __attribute__((packed, aligned(16))) mgfx_light_t;

/** @brief Data structure of the lighting uniform. */
typedef struct
{
    mgfx_light_t lights[MGFX_LIGHT_COUNT_MAX];  ///< Array of lights.
    int16_t ambient[4];                         ///< The ambient color.
    uint32_t count;                             ///< Number of lights.
} __attribute__((packed, aligned(16))) mgfx_lighting_t;

/** @brief Data structure of the fog uniform. */
typedef struct
{
    int16_t offset_int;     ///< Integer part of the fog offset.
    uint16_t offset_frac;   ///< Fractional part of the fog offset.
    int16_t factor_int;     ///< Integer part of the fog factor.
    uint8_t mask;           ///< Mask that determines whether the fog factor is loaded into vertex alpha.
} __attribute__((packed, aligned(16))) mgfx_fog_t;

/** @brief Matrices parameters. */
typedef struct
{
    const float *model_view_projection;     ///< The model-view-projection matrix in column-major order.
    const float *model_view;                ///< The model-view matrix in column-major order.
    const float *normal;                    ///< The normal matrix in column-major order.
} mgfx_matrices_parms_t;

/** @brief Texturing parameters. */
typedef struct
{
    int16_t scale[2];       ///< Factors by which texture coordinates are be scaled.
    int16_t offset[2];      ///< Offsets that are added to texture coordinates after scaling.
} mgfx_texturing_parms_t;

/** @brief Parameters of a single light. */
typedef struct
{
    /**
     * @brief The light's position/direction.
     * 
     * If w is 0, the light is directional. In this case x, y and z specify the light's (normalized) direction in eye space.
     * Otherwise, the light is positional. In this case x, y and z specify the light's position in eye space.
     */
    fm_vec4_t position;

    color_t color;      ///< The light's color.
    float radius;       ///< The light's radius. Must only be set if the light is positional.
} mgfx_light_parms_t;

/** @brief Lighting parameters. */
typedef struct
{
    color_t ambient_color;              ///< The ambient light color, which is always added to all geometry.
    const mgfx_light_parms_t *lights;   ///< The list of lights.
    uint32_t light_count;               ///< The number of lights in the list. The maximum value is #MGFX_LIGHT_COUNT_MAX.
} mgfx_lighting_parms_t;

/** @brief Fog parameters. */
typedef struct
{
    float start;    ///< Distance from the eye position where fog starts. Geometry closer than this distance will receive no fog at all.
    float end;      ///< Distance from the eye position where fog ends. Geometry farther away than this distance will be fully fog-colored.
} mgfx_fog_parms_t;

typedef struct mgfx_pipeline_cache_s    mgfx_pipeline_cache_t;

#ifdef __cplusplus
extern "C" {
#endif

extern rsp_ucode_t rsp_mgfx;
extern rsp_ucode_t rsp_mgfx_env;

/** 
 * @brief Returns a pointer to the mgfx shader ucode. Use this to create a pipeline with this shader.
 * 
 * @see #mg_pipeline_create
 */
inline rsp_ucode_t *mgfx_get_shader_ucode(mgfx_features_t features)
{
    if (features & MGFX_FEATURE_ENV_MAP) return &rsp_mgfx_env;
    return &rsp_mgfx;
}

/** @brief Convert matrices parameters to the internal data structure of the matrices uniform. */
void mgfx_get_matrices(mgfx_matrices_t *dst, const mgfx_matrices_parms_t *parms);

/** @brief Convert texturing parameters to the internal data structure of the texturing uniform. */
void mgfx_get_texturing(mgfx_texturing_t *dst, const mgfx_texturing_parms_t *parms);

/** @brief Convert lighting parameters to the internal data structure of the lighting uniform. */
void mgfx_get_lighting(mgfx_lighting_t *dst, const mgfx_lighting_parms_t *parms);

/** @brief Convert fog parameters to the internal data structure of the fog uniform. */
void mgfx_get_fog(mgfx_fog_t *dst, const mgfx_fog_parms_t *parms);

/** @brief Set the value of the matrices uniform inline. */
void mgfx_set_matrices_inline(const mg_uniform_t *uniform, const mgfx_matrices_parms_t *parms);

/** @brief Set the value of the texturing uniform inline. */
void mgfx_set_texturing_inline(const mg_uniform_t *uniform, const mgfx_texturing_parms_t *parms);

/** @brief Set the value of the lighting uniform inline. */
void mgfx_set_lighting_inline(const mg_uniform_t *uniform, const mgfx_lighting_parms_t *parms);

/** @brief Set the value of the fog uniform inline. */
void mgfx_set_fog_inline(const mg_uniform_t *uniform, const mgfx_fog_parms_t *parms);

mgfx_pipeline_cache_t *mgfx_pipeline_cache_create();
void mgfx_pipeline_cache_free(mgfx_pipeline_cache_t *cache);
mg_pipeline_t *mgfx_pipeline_cache_get_or_create(const mg_pipeline_parms_t *parms);

#ifdef __cplusplus
}
#endif

#endif
