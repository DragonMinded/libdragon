#include "mgfx.h"
#include <math.h>
#include <float.h>
#include "utils.h"
#include "rsp_asm.h"

_Static_assert(sizeof(mgfx_matrix_t) == MGFX_MATRIX_SIZE);

_Static_assert(offsetof(mgfx_light_t, position) == MGFX_LIGHT_POSITION);
_Static_assert(offsetof(mgfx_light_t, color) == MGFX_LIGHT_COLOR);
_Static_assert(offsetof(mgfx_light_t, intensity) == MGFX_LIGHT_INTENSITY);
_Static_assert(sizeof(mgfx_light_t) == MGFX_LIGHT_SIZE);

#define U8_TO_I16(x) ((x) << 7)
#define FLOAT_TO_S10_5(x) ((x) * (1<<5))
#define FLOAT_TO_I16(x) (CLAMP(x, -1.f, 1.f) * 0x7FFF)

DEFINE_RSP_UCODE(rsp_mgfx);
DEFINE_RSP_UCODE(rsp_mgfx_env);

static void mgfx_convert_matrix(mgfx_matrix_t *dst, const float *src)
{
    for (uint32_t i = 0; i < 16; i++)
    {
        int32_t fixed = src[i] * (1<<16);
        dst->i[i] = (int16_t)((fixed & 0xFFFF0000) >> 16);
        dst->f[i] = (uint16_t)(fixed & 0x0000FFFF);
    }
}

void mgfx_get_matrices(mgfx_matrices_t *dst, const mgfx_matrices_parms_t *parms)
{
    mgfx_convert_matrix(&dst->mvp, parms->model_view_projection);
    mgfx_convert_matrix(&dst->mv, parms->model_view);
    
    for (uint32_t i = 0; i < 16; i++)
    {
        dst->normal[i] = parms->normal[i] * (1<<15);
    }
}

void mgfx_get_texturing(mgfx_texturing_t *dst, const mgfx_texturing_parms_t *parms)
{
    dst->tex_scale[0] = parms->scale[0];
    dst->tex_scale[1] = parms->scale[1];
    dst->tex_offset[0] = parms->offset[0];
    dst->tex_offset[1] = parms->offset[1];
}

static inline void color_to_i16(int16_t *dst, color_t color)
{
    dst[0] = U8_TO_I16(color.r);
    dst[1] = U8_TO_I16(color.g);
    dst[2] = U8_TO_I16(color.b);
}

static int mgfx_get_light(mgfx_light_t *dst, const mgfx_light_parms_t *parms)
{
    color_to_i16(dst->color, parms->color);

    // The user should pre-transform positional lights into eye-space

    const fm_vec4_t *p = &parms->position;
    // If W is zero then the light is directional
    if (p->w == 0.0f) {
        // Pre-normalize the light direction
        float magnitude = sqrtf(p->x * p->x + p->y * p->y + p->z * p->z);
        dst->position[0] = -FLOAT_TO_I16(p->x / magnitude);
        dst->position[1] = -FLOAT_TO_I16(p->y / magnitude);
        dst->position[2] = -FLOAT_TO_I16(p->z / magnitude);
        dst->position[3] = FLOAT_TO_I16(0.0f);
        dst->intensity = 0;
        return 0;
    } else {
        dst->position[0] = FLOAT_TO_S10_5(p->x);
        dst->position[1] = FLOAT_TO_S10_5(p->y);
        dst->position[2] = FLOAT_TO_S10_5(p->z);
        dst->position[3] = FLOAT_TO_S10_5(1.0f);
        dst->intensity = sqrtf(parms->intensity) * (1<<5);
        return 1;
    }
}

void mgfx_get_lighting(mgfx_lighting_t *dst, const mgfx_lighting_parms_t *parms)
{
    assertf(parms->light_count <= MGFX_LIGHT_COUNT_MAX, "Light count must be %d or less!", MGFX_LIGHT_COUNT_MAX);

    dst->count = parms->light_count;
    color_to_i16(dst->ambient, parms->ambient_color);
    int point_light_count = 0;
    for (size_t i = 0; i < parms->light_count; i++)
    {
        point_light_count += mgfx_get_light(&dst->lights[i], &parms->lights[i]);
    }
    dst->has_points = point_light_count;
}

void mgfx_get_fog(mgfx_fog_t *dst, const mgfx_fog_parms_t *parms)
{
    float diff = parms->end - parms->start;
    // start == end is undefined, so disable fog by setting the factor to 0
    bool is_disabled = fabsf(diff) < FLT_MIN;
    float factor = is_disabled ? 0.0f : 1.0f / diff;
    float offset = -parms->start;

    int32_t offset_fx = offset * (1<<(16 + MGFX_VTX_POS_SHIFT));
    // Premultiply with 1.15 conversion factor
    int16_t factor_fx = factor * (1<<(15 - MGFX_VTX_POS_SHIFT));

    int16_t offset_i = offset_fx >> 16;
    uint16_t offset_f = offset_fx & 0xFFFF;

    dst->factor_int = factor_fx;
    dst->offset_int = offset_i;
    dst->offset_frac = offset_f;
    dst->mask = is_disabled ? 0x00 : 0x77;
}

void mgfx_set_matrices_inline(const mg_uniform_t *uniform, const mgfx_matrices_parms_t *parms)
{
    mgfx_matrices_t matrices = {};
    mgfx_get_matrices(&matrices, parms);
    mg_uniform_load_inline(uniform, &matrices);
}

void mgfx_set_texturing_inline(const mg_uniform_t *uniform, const mgfx_texturing_parms_t *parms)
{
    mgfx_texturing_t texturing = {};
    mgfx_get_texturing(&texturing, parms);
    mg_uniform_load_inline(uniform, &texturing);
}

void mgfx_set_lighting_inline(const mg_uniform_t *uniform, const mgfx_lighting_parms_t *parms)
{
    mgfx_lighting_t lighting = {};
    mgfx_get_lighting(&lighting, parms);
    mg_uniform_load_inline(uniform, &lighting);
}

void mgfx_set_fog_inline(const mg_uniform_t *uniform, const mgfx_fog_parms_t *parms)
{
    mgfx_fog_t fog = {};
    mgfx_get_fog(&fog, parms);
    mg_uniform_load_inline(uniform, &fog);
}

// TODO: RSP side matrix manipulation.
//       This could be done using a separate overlay which acts as a batch matrix processor.
