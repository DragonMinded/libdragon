#include "gl_internal.h"
#include "utils.h"
#include "debug.h"
#include <stddef.h>

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
        .spot_cutoff = 180.0f,
        .constant_attenuation = 1.0f,
        .linear_attenuation = 0.0f,
        .quadratic_attenuation = 0.0f,
        .enabled = false,
    };
}

void gl_lighting_init()
{
    gl_init_material(&state.materials[0]);
    gl_init_material(&state.materials[1]);

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
    state.light_model_two_side = false;
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

float dot_product3(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float gl_clamped_dot(const GLfloat *a, const GLfloat *b)
{
    return MAX(dot_product3(a, b), 0.0f);
}

const GLfloat * gl_material_get_color(const gl_material_t *material, GLenum color)
{
    GLenum target = material->color_target;

    switch (color) {
    case GL_EMISSION:
        return state.color_material && target == GL_EMISSION ? state.current_color : material->emissive;
    case GL_AMBIENT:
        return state.color_material && (target == GL_AMBIENT || target == GL_AMBIENT_AND_DIFFUSE) ? state.current_color : material->ambient;
    case GL_DIFFUSE:
        return state.color_material && (target == GL_DIFFUSE || target == GL_AMBIENT_AND_DIFFUSE) ? state.current_color : material->diffuse;
    case GL_SPECULAR:
        return state.color_material && target == GL_SPECULAR ? state.current_color : material->specular;
    default:
        assertf(0, "Invalid material color!");
        return NULL;
    }
}

void gl_perform_lighting(GLfloat *color, const GLfloat *v, const GLfloat *n, const gl_material_t *material)
{
    const GLfloat *emissive = gl_material_get_color(material, GL_EMISSION);
    const GLfloat *ambient = gl_material_get_color(material, GL_AMBIENT);
    const GLfloat *diffuse = gl_material_get_color(material, GL_DIFFUSE);
    const GLfloat *specular = gl_material_get_color(material, GL_SPECULAR);

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
        if (light->spot_cutoff != 180.0f) {
            GLfloat plv[3];
            gl_homogeneous_unit_diff(plv, light->position, v);

            GLfloat s[3];
            gl_normalize(s, light->direction);

            float plvds = gl_clamped_dot(plv, s);

            if (plvds < cosf(RADIANS(light->spot_cutoff))) {
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
            ambient[1] * light->ambient[1],
            ambient[0] * light->ambient[0],
            ambient[2] * light->ambient[2],
        };

        GLfloat vpl[3];
        gl_homogeneous_unit_diff(vpl, v, light->position);

        float ndvp = gl_clamped_dot(n, vpl);

        // Diffuse
        col[0] += diffuse[0] * light->diffuse[0] * ndvp;
        col[1] += diffuse[1] * light->diffuse[1] * ndvp;
        col[2] += diffuse[2] * light->diffuse[2] * ndvp;

        // Specular
        if (ndvp != 0.0f) {
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

            col[0] += specular[0] * light->specular[0] * spec_factor;
            col[1] += specular[1] * light->specular[1] * spec_factor;
            col[2] += specular[2] * light->specular[2] * spec_factor;
        }

        float light_factor = att * spot;

        color[0] += col[0] * light_factor;
        color[1] += col[1] * light_factor;
        color[2] += col[2] * light_factor;
    }
}

void gl_set_material_paramf(gl_material_t *material, GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_AMBIENT:
        material->ambient[0] = params[0];
        material->ambient[1] = params[1];
        material->ambient[2] = params[2];
        material->ambient[3] = params[3];
        break;
    case GL_DIFFUSE:
        material->diffuse[0] = params[0];
        material->diffuse[1] = params[1];
        material->diffuse[2] = params[2];
        material->diffuse[3] = params[3];
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        material->ambient[0] = params[0];
        material->ambient[1] = params[1];
        material->ambient[2] = params[2];
        material->ambient[3] = params[3];
        material->diffuse[0] = params[0];
        material->diffuse[1] = params[1];
        material->diffuse[2] = params[2];
        material->diffuse[3] = params[3];
        break;
    case GL_SPECULAR:
        material->specular[0] = params[0];
        material->specular[1] = params[1];
        material->specular[2] = params[2];
        material->specular[3] = params[3];
        break;
    case GL_EMISSION:
        material->emissive[0] = params[0];
        material->emissive[1] = params[1];
        material->emissive[2] = params[2];
        material->emissive[3] = params[3];
        break;
    case GL_SHININESS:
        material->shininess = params[0];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_set_material_parami(gl_material_t *material, GLenum pname, const GLint *params)
{
    switch (pname) {
    case GL_AMBIENT:
        material->ambient[0] = I32_TO_FLOAT(params[0]);
        material->ambient[1] = I32_TO_FLOAT(params[1]);
        material->ambient[2] = I32_TO_FLOAT(params[2]);
        material->ambient[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_DIFFUSE:
        material->diffuse[0] = I32_TO_FLOAT(params[0]);
        material->diffuse[1] = I32_TO_FLOAT(params[1]);
        material->diffuse[2] = I32_TO_FLOAT(params[2]);
        material->diffuse[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        material->ambient[0] = I32_TO_FLOAT(params[0]);
        material->ambient[1] = I32_TO_FLOAT(params[1]);
        material->ambient[2] = I32_TO_FLOAT(params[2]);
        material->ambient[3] = I32_TO_FLOAT(params[3]);
        material->diffuse[0] = I32_TO_FLOAT(params[0]);
        material->diffuse[1] = I32_TO_FLOAT(params[1]);
        material->diffuse[2] = I32_TO_FLOAT(params[2]);
        material->diffuse[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_SPECULAR:
        material->specular[0] = I32_TO_FLOAT(params[0]);
        material->specular[1] = I32_TO_FLOAT(params[1]);
        material->specular[2] = I32_TO_FLOAT(params[2]);
        material->specular[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_EMISSION:
        material->emissive[0] = I32_TO_FLOAT(params[0]);
        material->emissive[1] = I32_TO_FLOAT(params[1]);
        material->emissive[2] = I32_TO_FLOAT(params[2]);
        material->emissive[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_SHININESS:
        material->shininess = params[0];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    switch (pname) {
    case GL_SHININESS:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (face) {
    case GL_FRONT:
        gl_set_material_paramf(&state.materials[0], pname, &param);
        break;
    case GL_BACK:
        gl_set_material_paramf(&state.materials[1], pname, &param);
        break;
    case GL_FRONT_AND_BACK:
        gl_set_material_paramf(&state.materials[0], pname, &param);
        gl_set_material_paramf(&state.materials[1], pname, &param);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
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
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (face) {
    case GL_FRONT:
        gl_set_material_parami(&state.materials[0], pname, params);
        break;
    case GL_BACK:
        gl_set_material_parami(&state.materials[1], pname, params);
        break;
    case GL_FRONT_AND_BACK:
        gl_set_material_parami(&state.materials[0], pname, params);
        gl_set_material_parami(&state.materials[1], pname, params);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
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
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (face) {
    case GL_FRONT:
        gl_set_material_paramf(&state.materials[0], pname, params);
        break;
    case GL_BACK:
        gl_set_material_paramf(&state.materials[1], pname, params);
        break;
    case GL_FRONT_AND_BACK:
        gl_set_material_paramf(&state.materials[0], pname, params);
        gl_set_material_paramf(&state.materials[1], pname, params);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

gl_light_t * gl_get_light(GLenum light)
{
    if (light < GL_LIGHT0 || light > GL_LIGHT7) {
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }

    return &state.lights[light - GL_LIGHT0];
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    switch (pname) {
    case GL_SPOT_EXPONENT:
        l->spot_exponent = param;
        break;
    case GL_SPOT_CUTOFF:
        l->spot_cutoff = param;
        break;
    case GL_CONSTANT_ATTENUATION:
        l->constant_attenuation = param;
        break;
    case GL_LINEAR_ATTENUATION:
        l->linear_attenuation = param;
        break;
    case GL_QUADRATIC_ATTENUATION:
        l->quadratic_attenuation = param;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glLighti(GLenum light, GLenum pname, GLint param) { glLightf(light, pname, param); }

void glLightiv(GLenum light, GLenum pname, const GLint *params)
{
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    switch (pname) {
    case GL_AMBIENT:
        l->ambient[0] = I32_TO_FLOAT(params[0]);
        l->ambient[1] = I32_TO_FLOAT(params[1]);
        l->ambient[2] = I32_TO_FLOAT(params[2]);
        l->ambient[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_DIFFUSE:
        l->diffuse[0] = I32_TO_FLOAT(params[0]);
        l->diffuse[1] = I32_TO_FLOAT(params[1]);
        l->diffuse[2] = I32_TO_FLOAT(params[2]);
        l->diffuse[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_SPECULAR:
        l->specular[0] = I32_TO_FLOAT(params[0]);
        l->specular[1] = I32_TO_FLOAT(params[1]);
        l->specular[2] = I32_TO_FLOAT(params[2]);
        l->specular[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_POSITION:
        l->position[0] = params[0];
        l->position[1] = params[1];
        l->position[2] = params[2];
        l->position[3] = params[3];
        gl_matrix_mult(l->position, gl_matrix_stack_get_matrix(&state.modelview_stack), l->position);
        break;
    case GL_SPOT_DIRECTION:
        l->direction[0] = params[0];
        l->direction[1] = params[1];
        l->direction[2] = params[2];
        gl_matrix_mult3x3(l->direction, gl_matrix_stack_get_matrix(&state.modelview_stack), l->direction);
        break;
    case GL_SPOT_EXPONENT:
        l->spot_exponent = params[0];
        break;
    case GL_SPOT_CUTOFF:
        l->spot_cutoff = params[0];
        break;
    case GL_CONSTANT_ATTENUATION:
        l->constant_attenuation = params[0];
        break;
    case GL_LINEAR_ATTENUATION:
        l->linear_attenuation = params[0];
        break;
    case GL_QUADRATIC_ATTENUATION:
        l->quadratic_attenuation = params[0];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    switch (pname) {
    case GL_AMBIENT:
        l->ambient[0] = params[0];
        l->ambient[1] = params[1];
        l->ambient[2] = params[2];
        l->ambient[3] = params[3];
        break;
    case GL_DIFFUSE:
        l->diffuse[0] = params[0];
        l->diffuse[1] = params[1];
        l->diffuse[2] = params[2];
        l->diffuse[3] = params[3];
        break;
    case GL_SPECULAR:
        l->specular[0] = params[0];
        l->specular[1] = params[1];
        l->specular[2] = params[2];
        l->specular[3] = params[3];
        break;
    case GL_POSITION:
        gl_matrix_mult(l->position, gl_matrix_stack_get_matrix(&state.modelview_stack), params);
        break;
    case GL_SPOT_DIRECTION:
        gl_matrix_mult3x3(l->direction, gl_matrix_stack_get_matrix(&state.modelview_stack), params);
        break;
    case GL_SPOT_EXPONENT:
        l->spot_exponent = params[0];
        break;
    case GL_SPOT_CUTOFF:
        l->spot_cutoff = params[0];
        break;
    case GL_CONSTANT_ATTENUATION:
        l->constant_attenuation = params[0];
        break;
    case GL_LINEAR_ATTENUATION:
        l->linear_attenuation = params[0];
        break;
    case GL_QUADRATIC_ATTENUATION:
        l->quadratic_attenuation = params[0];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glLightModeli(GLenum pname, GLint param) 
{
    switch (pname) {
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        state.light_model_local_viewer = param != 0;
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        state.light_model_two_side = param != 0;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
void glLightModelf(GLenum pname, GLfloat param) { glLightModeli(pname, param); }

void glLightModeliv(GLenum pname, const GLint *params)
{
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
        state.light_model_ambient[0] = I32_TO_FLOAT(params[0]);
        state.light_model_ambient[1] = I32_TO_FLOAT(params[1]);
        state.light_model_ambient[2] = I32_TO_FLOAT(params[2]);
        state.light_model_ambient[3] = I32_TO_FLOAT(params[3]);
        break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        state.light_model_local_viewer = params[0] != 0;
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        state.light_model_two_side = params[0] != 0;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
void glLightModelfv(GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
        state.light_model_ambient[0] = params[0];
        state.light_model_ambient[1] = params[1];
        state.light_model_ambient[2] = params[2];
        state.light_model_ambient[3] = params[3];
        break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        state.light_model_local_viewer = params[0] != 0;
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        state.light_model_two_side = params[0] != 0;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glColorMaterial(GLenum face, GLenum mode)
{
    switch (face) {
    case GL_FRONT:
        state.materials[0].color_target = mode;
        break;
    case GL_BACK:
        state.materials[1].color_target = mode;
        break;
    case GL_FRONT_AND_BACK:
        state.materials[0].color_target = mode;
        state.materials[1].color_target = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glShadeModel(GLenum mode)
{
    switch (mode) {
    case GL_FLAT:
    case GL_SMOOTH:
        state.shade_model = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
