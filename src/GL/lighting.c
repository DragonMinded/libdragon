#include "gl_internal.h"
#include "utils.h"
#include "debug.h"
#include <stddef.h>

_Static_assert((1<<LIGHT0_SHIFT) == FLAG_LIGHT0);

extern gl_state_t state;

void gl_init_material(gl_material_t *material)
{
    *material = (gl_material_t) {
        .ambient = { 0.2f, 0.2f, 0.2f, 1.0f },
        .diffuse = { 0.8f, 0.8f, 0.8f, 1.0f },
        .specular = { 0.0f, 0.0f, 0.0f, 1.0f },
        .emissive = { 0.0f, 0.0f, 0.0f, 1.0f },
        .shininess = 0.0f,
        .color_target = GL_AMBIENT_AND_DIFFUSE,
    };
}

void gl_init_light(gl_light_t *light)
{
    *light = (gl_light_t) {
        .ambient = { 0.0f, 0.0f, 0.0f, 1.0f },
        .diffuse = { 0.0f, 0.0f, 0.0f, 1.0f },
        .specular = { 0.0f, 0.0f, 0.0f, 1.0f },
        .position = { 0.0f, 0.0f, 1.0f, 0.0f },
        .direction = { 0.0f, 0.0f, -1.0f },
        .spot_exponent = 0.0f,
        .spot_cutoff_cos = -1.0f,
        .constant_attenuation = 1.0f,
        .linear_attenuation = 0.0f,
        .quadratic_attenuation = 0.0f,
        .enabled = false,
    };
}

void gl_lighting_init()
{
    gl_init_material(&state.material);

    for (uint32_t i = 0; i < LIGHT_COUNT; i++)
    {
        gl_init_light(&state.lights[i]);
    }

    state.lights[0].diffuse[0] = 0.2f;
    state.lights[0].diffuse[1] = 0.2f;
    state.lights[0].diffuse[2] = 0.2f;

    state.lights[0].specular[0] = 0.8f;
    state.lights[0].specular[1] = 0.8f;
    state.lights[0].specular[2] = 0.8f;

    state.light_model_ambient[0] = 0.2f;
    state.light_model_ambient[1] = 0.2f;
    state.light_model_ambient[2] = 0.2f;
    state.light_model_ambient[3] = 1.0f;
    state.light_model_local_viewer = false;
}

float gl_mag2(const GLfloat *v)
{
    return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

float gl_mag(const GLfloat *v)
{
    return sqrtf(gl_mag2(v));
}

void gl_normalize(GLfloat *d, const GLfloat *v)
{
    float inv_mag = 1.0f / gl_mag(v);

    d[0] = v[0] * inv_mag;
    d[1] = v[1] * inv_mag;
    d[2] = v[2] * inv_mag;
}

void gl_homogeneous_unit_diff(GLfloat *d, const GLfloat *p1, const GLfloat *p2)
{
    bool p1wzero = p1[3] == 0.0f;
    bool p2wzero = p2[3] == 0.0f;

    if (!(p1wzero ^ p2wzero)) {
        d[0] = p2[0] - p1[0];
        d[1] = p2[1] - p1[1];
        d[2] = p2[2] - p1[2];
    } else if (p1wzero) {
        d[0] = -p1[0];
        d[1] = -p1[1];
        d[2] = -p1[2];
    } else {
        d[0] = p2[0];
        d[1] = p2[1];
        d[2] = p2[2];
    }

    gl_normalize(d, d);
}

void gl_cross(GLfloat* p, const GLfloat* a, const GLfloat* b)
{
    p[0] = (a[1] * b[2] - a[2] * b[1]);
    p[1] = (a[2] * b[0] - a[0] * b[2]);
    p[2] = (a[0] * b[1] - a[1] * b[0]);
};

float dot_product3(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float gl_clamped_dot(const GLfloat *a, const GLfloat *b)
{
    return MAX(dot_product3(a, b), 0.0f);
}

const GLfloat * gl_material_get_color(const gl_material_t *material, GLenum color, const GLfloat *input)
{
    GLenum target = material->color_target;

    switch (color) {
    case GL_EMISSION:
        return state.color_material && target == GL_EMISSION ? input : material->emissive;
    case GL_AMBIENT:
        return state.color_material && (target == GL_AMBIENT || target == GL_AMBIENT_AND_DIFFUSE) ? input : material->ambient;
    case GL_DIFFUSE:
        return state.color_material && (target == GL_DIFFUSE || target == GL_AMBIENT_AND_DIFFUSE) ? input : material->diffuse;
    case GL_SPECULAR:
        return state.color_material && target == GL_SPECULAR ? input : material->specular;
    default:
        assertf(0, "Invalid material color!");
        return NULL;
    }
}

void gl_perform_lighting(GLfloat *color, const GLfloat *input, const GLfloat *v, const GLfloat *n, const gl_material_t *material)
{
    const GLfloat *emissive = gl_material_get_color(material, GL_EMISSION, input);
    const GLfloat *ambient = gl_material_get_color(material, GL_AMBIENT, input);
    const GLfloat *diffuse = gl_material_get_color(material, GL_DIFFUSE, input);
    const GLfloat *specular = gl_material_get_color(material, GL_SPECULAR, input);

    // Emission and ambient
    color[0] = emissive[0] + ambient[0] * state.light_model_ambient[0];
    color[1] = emissive[1] + ambient[1] * state.light_model_ambient[1];
    color[2] = emissive[2] + ambient[2] * state.light_model_ambient[2];
    color[3] = diffuse[3];

    for (uint32_t l = 0; l < LIGHT_COUNT; l++)
    {
        const gl_light_t *light = &state.lights[l];
        if (!light->enabled) {
            continue;
        }

        // Spotlight
        float spot = 1.0f;
        if (light->spot_cutoff_cos >= 0.0f) {
            GLfloat plv[3];
            gl_homogeneous_unit_diff(plv, light->position, v);

            GLfloat s[3];
            gl_normalize(s, light->direction);

            float plvds = gl_clamped_dot(plv, s);

            if (plvds < light->spot_cutoff_cos) {
                // Outside of spotlight cutoff
                continue;
            }

            spot = powf(plvds, light->spot_exponent);
        }

        // Attenuation
        float att = 1.0f;
        if (light->position[3] != 0.0f) {
            GLfloat diff[3] = {
                v[0] - light->position[0],
                v[1] - light->position[1],
                v[2] - light->position[2],
            };
            float dsq = gl_mag2(diff);
            float d = sqrtf(dsq);
            att = 1.0f / (light->constant_attenuation + light->linear_attenuation * d + light->quadratic_attenuation * dsq);
        }

        // Light ambient color
        GLfloat col[3] = {
            ambient[0] * light->ambient[0],
            ambient[1] * light->ambient[1],
            ambient[2] * light->ambient[2],
        };

        GLfloat vpl[3];
        gl_homogeneous_unit_diff(vpl, v, light->position);

        float ndvp = gl_clamped_dot(n, vpl);

        // Diffuse
        col[0] += diffuse[0] * light->diffuse[0] * ndvp;
        col[1] += diffuse[1] * light->diffuse[1] * ndvp;
        col[2] += diffuse[2] * light->diffuse[2] * ndvp;

        GLfloat spec_mix[3] = {
            specular[0] * light->specular[0],
            specular[1] * light->specular[1],
            specular[2] * light->specular[2]
        };

        bool spec_any = spec_mix[0] != 0.0f || spec_mix[1] != 0.0f || spec_mix[2] != 0.0f;

        // Specular
        if (ndvp != 0.0f && spec_any) {
            GLfloat h[3] = {
                vpl[0],
                vpl[1],
                vpl[2],
            };
            if (state.light_model_local_viewer) {
                GLfloat pe[4] = { 0, 0, 0, 1 };
                gl_homogeneous_unit_diff(pe, v, pe);
                h[0] += pe[0];
                h[1] += pe[1];
                h[2] += pe[2];
            } else {
                h[2] += 1;
            }
            gl_normalize(h, h);
            
            float ndh = gl_clamped_dot(n, h);
            float spec_factor = powf(ndh, material->shininess);

            col[0] += spec_mix[0] * spec_factor;
            col[1] += spec_mix[1] * spec_factor;
            col[2] += spec_mix[2] * spec_factor;
        }

        float light_factor = att * spot;

        color[0] += col[0] * light_factor;
        color[1] += col[1] * light_factor;
        color[2] += col[2] * light_factor;
    }
}

bool gl_validate_material_face(GLenum face)
{
    switch (face) {
    case GL_FRONT_AND_BACK:
        return true;
    case GL_FRONT:
    case GL_BACK:
        assertf(0, "Separate materials for front and back faces are not supported!");
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid material face", face);
        return false;
    }
}

void gl_set_color_cpu(GLfloat *dst, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
}

void gl_set_color(GLfloat *dst, uint32_t offset, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    int16_t r_fx = FLOAT_TO_I16(r);
    int16_t g_fx = FLOAT_TO_I16(g);
    int16_t b_fx = FLOAT_TO_I16(b);
    int16_t a_fx = FLOAT_TO_I16(a);

    uint64_t packed = ((uint64_t)r_fx << 48) | ((uint64_t)g_fx << 32) | ((uint64_t)b_fx << 16) | (uint64_t)a_fx;
    gl_set_long(GL_UPDATE_NONE, offset, packed);
    gl_set_color_cpu(dst, r, g, b, a);
}

void gl_set_material_ambient(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state.material.ambient, offsetof(gl_server_state_t, mat_ambient), r, g, b, a);
}

void gl_set_material_diffuse(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state.material.diffuse, offsetof(gl_server_state_t, mat_diffuse), r, g, b, a);
}

void gl_set_material_specular(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state.material.specular, offsetof(gl_server_state_t, mat_specular), r, g, b, a);
    set_can_use_rsp_dirty();
}

void gl_set_material_emissive(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state.material.emissive, offsetof(gl_server_state_t, mat_emissive), r, g, b, a);
}

void gl_set_material_shininess(GLfloat param)
{    
    state.material.shininess = param;
    gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, mat_shininess), param * 32.f);
}

void gl_set_material_paramf(GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_AMBIENT:
        gl_set_material_ambient(params[0], params[1], params[2], params[3]);
        break;
    case GL_DIFFUSE:
        gl_set_material_diffuse(params[0], params[1], params[2], params[3]);
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        gl_set_material_ambient(params[0], params[1], params[2], params[3]);
        gl_set_material_diffuse(params[0], params[1], params[2], params[3]);
        break;
    case GL_SPECULAR:
        gl_set_material_specular(params[0], params[1], params[2], params[3]);
        break;
    case GL_EMISSION:
        gl_set_material_emissive(params[0], params[1], params[2], params[3]);
        break;
    case GL_SHININESS:
        gl_set_material_shininess(params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void gl_set_material_parami(GLenum pname, const GLint *params)
{
    switch (pname) {
    case GL_AMBIENT:
        gl_set_material_ambient(
            I32_TO_FLOAT(params[0]),
            I32_TO_FLOAT(params[1]),
            I32_TO_FLOAT(params[2]),
            I32_TO_FLOAT(params[3]));
        break;
    case GL_DIFFUSE:
        gl_set_material_diffuse(
            I32_TO_FLOAT(params[0]),
            I32_TO_FLOAT(params[1]),
            I32_TO_FLOAT(params[2]),
            I32_TO_FLOAT(params[3]));
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        gl_set_material_ambient(
            I32_TO_FLOAT(params[0]),
            I32_TO_FLOAT(params[1]),
            I32_TO_FLOAT(params[2]),
            I32_TO_FLOAT(params[3]));
        gl_set_material_diffuse(
            I32_TO_FLOAT(params[0]),
            I32_TO_FLOAT(params[1]),
            I32_TO_FLOAT(params[2]),
            I32_TO_FLOAT(params[3]));
        break;
    case GL_SPECULAR:
        gl_set_material_specular(
            I32_TO_FLOAT(params[0]),
            I32_TO_FLOAT(params[1]),
            I32_TO_FLOAT(params[2]),
            I32_TO_FLOAT(params[3]));
        break;
    case GL_EMISSION:
        gl_set_material_emissive(
            I32_TO_FLOAT(params[0]),
            I32_TO_FLOAT(params[1]),
            I32_TO_FLOAT(params[2]),
            I32_TO_FLOAT(params[3]));
        break;
    case GL_SHININESS:
        gl_set_material_shininess(params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    switch (pname) {
    case GL_SHININESS:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }

    if (!gl_validate_material_face(face)) {
        return;
    }

    gl_set_material_paramf(pname, &param);
}

void glMateriali(GLenum face, GLenum pname, GLint param) { glMaterialf(face, pname, param); }

void glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
    switch (pname) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_AMBIENT_AND_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_SHININESS:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }

    if (!gl_validate_material_face(face)) {
        return;
    }

    gl_set_material_parami(pname, params);
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_AMBIENT_AND_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_SHININESS:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }

    if (!gl_validate_material_face(face)) {
        return;
    }

    gl_set_material_paramf(pname, params);
}

uint32_t gl_get_light_offset(GLenum light)
{
    uint32_t light_index = light - GL_LIGHT0;
    return offsetof(gl_server_state_t, lights) + light_index * sizeof(int16_t) * 4;
}

gl_light_t * gl_get_light(GLenum light)
{
    if (light < GL_LIGHT0 || light > GL_LIGHT7) {
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid light (Must be in [GL_LIGHT0, GL_LIGHT7])", light);
        return NULL;
    }

    return &state.lights[light - GL_LIGHT0];
}

void gl_light_set_ambient(gl_light_t *light, uint32_t offset, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(light->ambient, offset + offsetof(gl_lights_soa_t, ambient), r, g, b, a);
}

void gl_light_set_diffuse(gl_light_t *light, uint32_t offset, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(light->diffuse, offset + offsetof(gl_lights_soa_t, diffuse), r, g, b, a);
}

void gl_light_set_specular(gl_light_t *light, uint32_t offset, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color_cpu(light->specular, r, g, b, a);
}

void gl_light_set_position(gl_light_t *light, uint32_t offset, const GLfloat *pos)
{
    gl_matrix_mult(light->position, gl_matrix_stack_get_matrix(&state.modelview_stack), pos);

    int16_t x, y, z, w;

    if (pos[3] == 0.0f) {
        // Light is directional
        // -> Pre-normalize so the ucode doesn't need to
        float mag = gl_mag(pos);
        x = (pos[0] / mag) * 0x7FFF;
        y = (pos[1] / mag) * 0x7FFF;
        z = (pos[2] / mag) * 0x7FFF;
        w = 0;
    } else {
        // Light is positional
        // -> Convert to s10.5 to match with object space position
        x = pos[0] * 32.f;
        y = pos[1] * 32.f;
        z = pos[2] * 32.f;
        w = 32.f;
    }

    uint32_t packed0 = ((uint32_t)x) << 16 | (uint32_t)y;
    uint32_t packed1 = ((uint32_t)z) << 16 | (uint32_t)w;

    gl_write(GL_CMD_SET_LIGHT_POS, offset, packed0, packed1);
}

void gl_light_set_direction(gl_light_t *light, uint32_t offset, const GLfloat *dir)
{
    gl_matrix_mult3x3(light->direction, gl_matrix_stack_get_matrix(&state.modelview_stack), dir);

/*
    int16_t x = dir[0] * 0x7FFF;
    int16_t y = dir[1] * 0x7FFF;
    int16_t z = dir[2] * 0x7FFF;

    uint32_t packed0 = ((uint32_t)x) << 16 | (uint32_t)y;
    uint32_t packed1 = ((uint32_t)z) << 16;

    gl_write(GL_CMD_SET_LIGHT_DIR, offset, packed0, packed1);
*/
}

void gl_light_set_spot_exponent(gl_light_t *light, uint32_t offset, float param)
{
    light->spot_exponent = param;
    //gl_set_byte(GL_UPDATE_NONE, offset + offsetof(gl_light_srv_t, spot_exponent), param);
}

void gl_light_set_spot_cutoff(gl_light_t *light, uint32_t offset, float param)
{
    light->spot_cutoff_cos = cosf(RADIANS(param));
    //gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_light_srv_t, spot_cutoff_cos), light->spot_cutoff_cos * 0x7FFF);
    set_can_use_rsp_dirty();
}

void gl_light_set_constant_attenuation(gl_light_t *light, uint32_t offset, float param)
{
    light->constant_attenuation = param;
    // Shifted right by 1 to compensate for vrcp
    uint32_t fx = param * (1<<15);
    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_lights_soa_t, attenuation_int) + 0, fx >> 16);
    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_lights_soa_t, attenuation_frac) + 0, fx & 0xFFFF);
}

void gl_light_set_linear_attenuation(gl_light_t *light, uint32_t offset, float param)
{
    light->linear_attenuation = param;
    // Shifted right by 4 to compensate for various precision shifts (see rsp_gl_lighting.inc)
    // Shifted right by 1 to compensate for vrcp
    // Result: Shifted right by 5
    uint32_t fx = param * (1 << (16 - 5));
    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_lights_soa_t, attenuation_int) + 2, fx >> 16);
    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_lights_soa_t, attenuation_frac) + 2, fx & 0xFFFF);
}

void gl_light_set_quadratic_attenuation(gl_light_t *light, uint32_t offset, float param)
{
    light->quadratic_attenuation = param;
    // Shifted left by 6 to compensate for various precision shifts (see rsp_gl_lighting.inc)
    // Shifted right by 1 to compensate for vrcp
    // Result: Shifted left by 5
    uint32_t fx = param * (1 << (16 + 5));
    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_lights_soa_t, attenuation_int) + 4, fx >> 16);
    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_lights_soa_t, attenuation_frac) + 4, fx & 0xFFFF);
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    uint32_t offset = gl_get_light_offset(light);

    switch (pname) {
    case GL_SPOT_EXPONENT:
        gl_light_set_spot_exponent(l, offset, param);
        break;
    case GL_SPOT_CUTOFF:
        gl_light_set_spot_cutoff(l, offset, param);
        break;
    case GL_CONSTANT_ATTENUATION:
        gl_light_set_constant_attenuation(l, offset, param);
        break;
    case GL_LINEAR_ATTENUATION:
        gl_light_set_linear_attenuation(l, offset, param);
        break;
    case GL_QUADRATIC_ATTENUATION:
        gl_light_set_quadratic_attenuation(l, offset, param);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glLighti(GLenum light, GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    glLightf(light, pname, param);
}

void glLightiv(GLenum light, GLenum pname, const GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    uint32_t offset = gl_get_light_offset(light);

    GLfloat tmp[4];

    switch (pname) {
    case GL_AMBIENT:
        gl_light_set_ambient(l, offset, 
            I32_TO_FLOAT(params[0]), 
            I32_TO_FLOAT(params[1]), 
            I32_TO_FLOAT(params[2]), 
            I32_TO_FLOAT(params[3]));
        break;
    case GL_DIFFUSE:
        gl_light_set_diffuse(l, offset, 
            I32_TO_FLOAT(params[0]), 
            I32_TO_FLOAT(params[1]), 
            I32_TO_FLOAT(params[2]), 
            I32_TO_FLOAT(params[3]));
        break;
    case GL_SPECULAR:
        gl_light_set_specular(l, offset, 
            I32_TO_FLOAT(params[0]), 
            I32_TO_FLOAT(params[1]), 
            I32_TO_FLOAT(params[2]), 
            I32_TO_FLOAT(params[3]));
        break;
    case GL_POSITION:
        tmp[0] = params[0];
        tmp[1] = params[1];
        tmp[2] = params[2];
        tmp[3] = params[3];
        gl_light_set_position(l, offset, tmp);
        break;
    case GL_SPOT_DIRECTION:
        tmp[0] = params[0];
        tmp[1] = params[1];
        tmp[2] = params[2];
        gl_light_set_direction(l, offset, tmp);
        break;
    case GL_SPOT_EXPONENT:
        gl_light_set_spot_exponent(l, offset, params[0]);
        break;
    case GL_SPOT_CUTOFF:
        gl_light_set_spot_cutoff(l, offset, params[0]);
        break;
    case GL_CONSTANT_ATTENUATION:
        gl_light_set_constant_attenuation(l, offset, params[0]);
        break;
    case GL_LINEAR_ATTENUATION:
        gl_light_set_linear_attenuation(l, offset, params[0]);
        break;
    case GL_QUADRATIC_ATTENUATION:
        gl_light_set_quadratic_attenuation(l, offset, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    uint32_t offset = gl_get_light_offset(light);

    switch (pname) {
    case GL_AMBIENT:
        gl_light_set_ambient(l, offset, params[0], params[1], params[2], params[3]);
        break;
    case GL_DIFFUSE:
        gl_light_set_diffuse(l, offset, params[0], params[1], params[2], params[3]);
        break;
    case GL_SPECULAR:
        gl_light_set_specular(l, offset, params[0], params[1], params[2], params[3]);
        break;
    case GL_POSITION:
        gl_light_set_position(l, offset, params);
        break;
    case GL_SPOT_DIRECTION:
        gl_light_set_direction(l, offset, params);
        break;
    case GL_SPOT_EXPONENT:
        gl_light_set_spot_exponent(l, offset, params[0]);
        break;
    case GL_SPOT_CUTOFF:
        gl_light_set_spot_cutoff(l, offset, params[0]);
        break;
    case GL_CONSTANT_ATTENUATION:
        gl_light_set_constant_attenuation(l, offset, params[0]);
        break;
    case GL_LINEAR_ATTENUATION:
        gl_light_set_linear_attenuation(l, offset, params[0]);
        break;
    case GL_QUADRATIC_ATTENUATION:
        gl_light_set_quadratic_attenuation(l, offset, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void gl_set_light_model_local_viewer(bool param)
{
    state.light_model_local_viewer = param;
}

void gl_set_light_model_ambient(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state.light_model_ambient, offsetof(gl_server_state_t, light_ambient), r, g, b, a);
}

void glLightModeli(GLenum pname, GLint param) 
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        gl_set_light_model_local_viewer(param != 0);
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        assertf(0, "Two sided lighting is not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}
void glLightModelf(GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    glLightModeli(pname, param);
}

void glLightModeliv(GLenum pname, const GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
        gl_set_light_model_ambient(
            I32_TO_FLOAT(params[0]), 
            I32_TO_FLOAT(params[1]), 
            I32_TO_FLOAT(params[2]), 
            I32_TO_FLOAT(params[3]));
        break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        gl_set_light_model_local_viewer(params[0] != 0);
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        assertf(0, "Two sided lighting is not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glLightModelfv(GLenum pname, const GLfloat *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
        gl_set_light_model_ambient(params[0], params[1], params[2], params[3]);
        break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        gl_set_light_model_local_viewer(params[0] != 0);
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        assertf(0, "Two sided lighting is not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glColorMaterial(GLenum face, GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (!gl_validate_material_face(face)) {
        return;
    }

    uint64_t color_target = 0;

    switch (mode) {
    case GL_AMBIENT:
        color_target |= 1ULL << 48;
        break;
    case GL_DIFFUSE:
        color_target |= 1ULL << 32;
        break;
    case GL_SPECULAR:
    case GL_EMISSION:
        color_target |= 1ULL << 16;
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        color_target |= 1ULL << 48;
        color_target |= 1ULL << 32;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid color material mode", mode);
        return;
    }

    gl_set_long(GL_UPDATE_NONE, offsetof(gl_server_state_t, mat_color_target), color_target);
    state.material.color_target = mode;
}

void glShadeModel(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (mode) {
    case GL_FLAT:
    case GL_SMOOTH:
        gl_set_short(GL_UPDATE_NONE, offsetof(gl_server_state_t, shade_model), mode);
        state.shade_model = mode;
        set_can_use_rsp_dirty();
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid shade model", mode);
        return;
    }
}
