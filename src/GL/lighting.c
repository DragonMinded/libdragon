#include "gl_internal.h"
#include "utils.h"
#include "debug.h"
#include <stddef.h>

void gl_lighting_init()
{
    ringbuffer_init(&state->lighting_buffer, sizeof(mgfx_lighting_t), 8);

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, (GLfloat[]) { 0.2f, 0.2f, 0.2f, 1.0f });
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, (GLfloat[]) { 0.8f, 0.8f, 0.8f, 1.0f });
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (GLfloat[]) { 0.0f, 0.0f, 0.0f, 1.0f });
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, (GLfloat[]) { 0.0f, 0.0f, 0.0f, 1.0f });
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, (GLfloat[]) { 0.2f, 0.2f, 0.2f, 1.0f });

    for (uint32_t i = 0; i < LIGHT_COUNT; i++)
    {
        state->lights[i].enable = ENABLE_LIGHT0 + i;

        GLenum light = GL_LIGHT0 + i;

        glLightfv(light, GL_AMBIENT, (GLfloat[]) { 0, 0, 0, 1 });
        if (i == 0) {
            glLightfv(light, GL_DIFFUSE, (GLfloat[]) { 1, 1, 1, 1 });
            glLightfv(light, GL_SPECULAR, (GLfloat[]) { 1, 1, 1, 1 });
        } else {
            glLightfv(light, GL_DIFFUSE, (GLfloat[]) { 0, 0, 0, 0 });
            glLightfv(light, GL_SPECULAR, (GLfloat[]) { 0, 0, 0, 0 });
        }
        glLightfv(light, GL_POSITION, (GLfloat[]) { 0, 0, 1, 0 });
        glLightfv(light, GL_SPOT_DIRECTION, (GLfloat[]) { 0, 0, -1 });
        glLightf(light, GL_SPOT_EXPONENT, 0);
        glLightf(light, GL_SPOT_CUTOFF, 180);
        glLightf(light, GL_CONSTANT_ATTENUATION, 1);
        glLightf(light, GL_LINEAR_ATTENUATION, 0);
        glLightf(light, GL_QUADRATIC_ATTENUATION, 0);
    }
}

void gl_lighting_close()
{
    ringbuffer_free(&state->lighting_buffer);
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

void gl_set_color(GLfloat *dst, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
}

void gl_set_material_ambient(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state->material_ambient, r, g, b, a);
}

void gl_set_material_diffuse(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state->material_diffuse, r, g, b, a);
    gl_set_state(STATE_MATERIAL_DIFFUSE);
}

void gl_set_material_specular(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state->material_specular, r, g, b, a);
}

void gl_set_material_emissive(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state->material_emissive, r, g, b, a);
}

void gl_set_material_shininess(GLfloat param)
{    
    state->material_shininess = param;
}

inline void assert_no_begin_end()
{
    // This is not using gl_ensure_no_begin_end on purpose, because that macro is used in places
    // where the spec disallows such usage. In this case, the spec allows using glMaterial* between glBegin/glEnd,
    // but we cannot support it (currently).
    assertf(!state->begin_end_active, "Changing material between glBegin/glEnd is not supported");
}

void gl_set_material_paramf(GLenum pname, const GLfloat *params)
{
    assert_no_begin_end();

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
    assert_no_begin_end();

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

gl_light_t * gl_get_light(GLenum light)
{
    if (light < GL_LIGHT0 || light > GL_LIGHT7) {
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid light (Must be in [GL_LIGHT0, GL_LIGHT7])", light);
        return NULL;
    }

    return &state->lights[light - GL_LIGHT0];
}

void set_light_state(gl_light_t *light)
{
    if (gl_is_enabled(light->enable)) {
        gl_set_state(STATE_LIGHT);
    }
}

void gl_light_set_ambient(gl_light_t *light, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(light->ambient, r, g, b, a);
}

void gl_light_set_diffuse(gl_light_t *light, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(light->diffuse, r, g, b, a);
    set_light_state(light);
}

void gl_light_set_specular(gl_light_t *light, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(light->specular, r, g, b, a);
}

void gl_light_set_position(gl_light_t *light, const GLfloat *pos)
{
    fm_vec4_t pos_tmp = {{ pos[0], pos[1], pos[2], pos[3] }};
    fm_mat4_mul_vec4(&light->position, gl_matrix_stack_get_matrix(&state->modelview_stack), &pos_tmp);
    set_light_state(light);
}

void gl_light_set_direction(gl_light_t *light, const GLfloat *dir)
{
    fm_vec3_t dir_tmp = {{ dir[0], dir[1], dir[2] }};
    fm_vec4_t out;
    fm_mat4_mul_vec3(&out, gl_matrix_stack_get_matrix(&state->modelview_stack), &dir_tmp);
    light->direction.x = out.x;
    light->direction.y = out.y;
    light->direction.z = out.z;
}

void gl_light_set_spot_exponent(gl_light_t *light, float param)
{
    light->spot_exponent = param;
}

void gl_light_set_spot_cutoff(gl_light_t *light, float param)
{
    light->spot_cutoff_cos = fm_cosf(FM_DEG2RAD(param));
}

void gl_light_set_constant_attenuation(gl_light_t *light, float param)
{
    light->constant_attenuation = param;
}

void gl_light_set_linear_attenuation(gl_light_t *light, float param)
{
    light->linear_attenuation = param;
}

void gl_light_set_quadratic_attenuation(gl_light_t *light, float param)
{
    light->quadratic_attenuation = param;
    set_light_state(light);
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_light_t *l = gl_get_light(light);
    if (l == NULL) {
        return;
    }

    switch (pname) {
    case GL_SPOT_EXPONENT:
        gl_light_set_spot_exponent(l, param);
        break;
    case GL_SPOT_CUTOFF:
        gl_light_set_spot_cutoff(l, param);
        break;
    case GL_CONSTANT_ATTENUATION:
        gl_light_set_constant_attenuation(l, param);
        break;
    case GL_LINEAR_ATTENUATION:
        gl_light_set_linear_attenuation(l, param);
        break;
    case GL_QUADRATIC_ATTENUATION:
        gl_light_set_quadratic_attenuation(l, param);
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

    GLfloat tmp[4];

    switch (pname) {
    case GL_AMBIENT:
        gl_light_set_ambient(l,
            I32_TO_FLOAT(params[0]), 
            I32_TO_FLOAT(params[1]), 
            I32_TO_FLOAT(params[2]), 
            I32_TO_FLOAT(params[3]));
        break;
    case GL_DIFFUSE:
        gl_light_set_diffuse(l,
            I32_TO_FLOAT(params[0]), 
            I32_TO_FLOAT(params[1]), 
            I32_TO_FLOAT(params[2]), 
            I32_TO_FLOAT(params[3]));
        break;
    case GL_SPECULAR:
        gl_light_set_specular(l,
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
        gl_light_set_position(l, tmp);
        break;
    case GL_SPOT_DIRECTION:
        tmp[0] = params[0];
        tmp[1] = params[1];
        tmp[2] = params[2];
        gl_light_set_direction(l, tmp);
        break;
    case GL_SPOT_EXPONENT:
        gl_light_set_spot_exponent(l, params[0]);
        break;
    case GL_SPOT_CUTOFF:
        gl_light_set_spot_cutoff(l, params[0]);
        break;
    case GL_CONSTANT_ATTENUATION:
        gl_light_set_constant_attenuation(l, params[0]);
        break;
    case GL_LINEAR_ATTENUATION:
        gl_light_set_linear_attenuation(l, params[0]);
        break;
    case GL_QUADRATIC_ATTENUATION:
        gl_light_set_quadratic_attenuation(l, params[0]);
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

    switch (pname) {
    case GL_AMBIENT:
        gl_light_set_ambient(l, params[0], params[1], params[2], params[3]);
        break;
    case GL_DIFFUSE:
        gl_light_set_diffuse(l, params[0], params[1], params[2], params[3]);
        break;
    case GL_SPECULAR:
        gl_light_set_specular(l, params[0], params[1], params[2], params[3]);
        break;
    case GL_POSITION:
        gl_light_set_position(l, params);
        break;
    case GL_SPOT_DIRECTION:
        gl_light_set_direction(l, params);
        break;
    case GL_SPOT_EXPONENT:
        gl_light_set_spot_exponent(l, params[0]);
        break;
    case GL_SPOT_CUTOFF:
        gl_light_set_spot_cutoff(l, params[0]);
        break;
    case GL_CONSTANT_ATTENUATION:
        gl_light_set_constant_attenuation(l, params[0]);
        break;
    case GL_LINEAR_ATTENUATION:
        gl_light_set_linear_attenuation(l, params[0]);
        break;
    case GL_QUADRATIC_ATTENUATION:
        gl_light_set_quadratic_attenuation(l, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void gl_set_light_model_local_viewer(bool param)
{
    state->light_model_local_viewer = param;
}

void gl_set_light_model_ambient(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl_set_color(state->light_model_ambient, r, g, b, a);
    gl_set_state(STATE_LIGHT);
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

    switch (mode) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid color material mode", mode);
        return;
    }

    state->color_material = mode;
    gl_set_state(STATE_COLOR_MATERIAL);
}

void glShadeModel(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (mode) {
    case GL_FLAT:
    case GL_SMOOTH:
        state->shade_model = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid shade model", mode);
        return;
    }
}

bool gl_is_diffuse_tracking_color()
{
    return gl_is_enabled(ENABLE_COLOR_MATERIAL) && (state->color_material == GL_DIFFUSE || state->color_material == GL_AMBIENT_AND_DIFFUSE);
}

color_t gl_get_material_diffuse()
{
    if (gl_is_diffuse_tracking_color()) {
        return color_from_packed32(state->current_attribs.color);
    } else {
        return color_from_floats(state->material_diffuse);
    }
}

void get_lighting_parms(mgfx_lighting_parms_t *parms)
{
    if (!gl_is_enabled(ENABLE_LIGHTING)) {
        parms->ambient_color = color_from_packed32(0xFFFFFFFF);
    } else {
        parms->ambient_color = color_from_floats(state->light_model_ambient);

        for (size_t i = 0; i < LIGHT_COUNT; i++) {
            gl_light_t *in_light = &state->lights[i];
            if (!gl_is_enabled(in_light->enable)) continue;

            mgfx_light_parms_t *out_light = &parms->lights[parms->light_count++];
            out_light->color = color_from_floats(in_light->diffuse);
            out_light->position = in_light->position;
            if (in_light->position.w != 0.f) {
                // TODO: how to map constant and linear attenuation?
                out_light->intensity = fabsf(in_light->quadratic_attenuation) > FM_EPSILON ? 1.f / in_light->quadratic_attenuation : 1.f;
            }
        }
    }
}

void gl_upload_lighting()
{
    mgfx_light_parms_t lights[LIGHT_COUNT];
    mgfx_lighting_parms_t parms = {
        .lights = lights
    };

    get_lighting_parms(&parms);
    mgfx_lighting_t *buffer = ringbuffer_alloc_next(&state->lighting_buffer);
    mgfx_get_lighting(buffer, &parms);
    mg_uniform_load(state->lighting_uniform, buffer);
    ringbuffer_release_current(&state->lighting_buffer);
}
