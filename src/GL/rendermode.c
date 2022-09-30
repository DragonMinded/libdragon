#include "gl_internal.h"
#include "rdpq_mode.h"
#include "rdpq_debug.h"
#include "rdpq_macros.h"
#include "rspq.h"

extern gl_state_t state;

// All possible combinations of blend functions. Configs that cannot be supported by the RDP are set to 0.
// NOTE: We always set fog alpha to one to support GL_ONE in both factors
// TODO: src = ZERO, dst = ONE_MINUS_SRC_ALPHA could be done with BLEND_RGB * IN_ALPHA + MEMORY_RGB * INV_MUX_ALPHA
static const rdpq_blender_t blend_configs[64] = {
    RDPQ_BLENDER((IN_RGB, ZERO, MEMORY_RGB, ZERO)),                // src = ZERO, dst = ZERO
    RDPQ_BLENDER((IN_RGB, ZERO, MEMORY_RGB, ONE)),                 // src = ZERO, dst = ONE
    RDPQ_BLENDER((MEMORY_RGB, IN_ALPHA, IN_RGB, ZERO)),            // src = ZERO, dst = SRC_ALPHA
    0,                                                             // src = ZERO, dst = ONE_MINUS_SRC_ALPHA
    0,                                                             // src = ZERO, dst = GL_DST_COLOR
    0,                                                             // src = ZERO, dst = GL_ONE_MINUS_DST_COLOR
    RDPQ_BLENDER((IN_RGB, ZERO, MEMORY_RGB, MEMORY_CVG)),          // src = ZERO, dst = DST_ALPHA
    0,                                                             // src = ZERO, dst = ONE_MINUS_DST_ALPHA

    RDPQ_BLENDER((IN_RGB, FOG_ALPHA, MEMORY_RGB, ZERO)),           // src = ONE, dst = ZERO
    RDPQ_BLENDER((IN_RGB, FOG_ALPHA, MEMORY_RGB, ONE)),            // src = ONE, dst = ONE
    RDPQ_BLENDER((MEMORY_RGB, IN_ALPHA, IN_RGB, ONE)),             // src = ONE, dst = SRC_ALPHA
    0,                                                             // src = ONE, dst = ONE_MINUS_SRC_ALPHA
    0,                                                             // src = ONE, dst = GL_DST_COLOR
    0,                                                             // src = ONE, dst = GL_ONE_MINUS_DST_COLOR
    RDPQ_BLENDER((IN_RGB, FOG_ALPHA, MEMORY_RGB, MEMORY_CVG)),     // src = ONE, dst = DST_ALPHA
    0,                                                             // src = ONE, dst = ONE_MINUS_DST_ALPHA

    RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, ZERO)),            // src = SRC_ALPHA, dst = ZERO
    RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, ONE)),             // src = SRC_ALPHA, dst = ONE
    0,                                                             // src = SRC_ALPHA, dst = SRC_ALPHA
    RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, INV_MUX_ALPHA)),   // src = SRC_ALPHA, dst = ONE_MINUS_SRC_ALPHA
    0,                                                             // src = SRC_ALPHA, dst = GL_DST_COLOR
    0,                                                             // src = SRC_ALPHA, dst = GL_ONE_MINUS_DST_COLOR
    RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, MEMORY_CVG)),      // src = SRC_ALPHA, dst = DST_ALPHA
    0,                                                             // src = SRC_ALPHA, dst = ONE_MINUS_DST_ALPHA

    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = ZERO
    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = ONE
    RDPQ_BLENDER((MEMORY_RGB, IN_ALPHA, IN_RGB, INV_MUX_ALPHA)),   // src = ONE_MINUS_SRC_ALPHA, dst = SRC_ALPHA
    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = ONE_MINUS_SRC_ALPHA
    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = GL_DST_COLOR
    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = GL_ONE_MINUS_DST_COLOR
    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = DST_ALPHA
    0,                                                             // src = ONE_MINUS_SRC_ALPHA, dst = ONE_MINUS_DST_ALPHA

    0, 0, 0, 0, 0, 0, 0, 0,                                        // src = GL_DST_COLOR, dst = ...
    0, 0, 0, 0, 0, 0, 0, 0,                                        // src = GL_ONE_MINUS_DST_COLOR, dst = ...

    RDPQ_BLENDER((MEMORY_RGB, ZERO, IN_RGB, MEMORY_CVG)),          // src = DST_ALPHA, dst = ZERO
    RDPQ_BLENDER((MEMORY_RGB, FOG_ALPHA, IN_RGB, MEMORY_CVG)),     // src = DST_ALPHA, dst = ONE
    RDPQ_BLENDER((MEMORY_RGB, IN_ALPHA, IN_RGB, MEMORY_CVG)),      // src = DST_ALPHA, dst = SRC_ALPHA
    0,                                                             // src = DST_ALPHA, dst = ONE_MINUS_SRC_ALPHA
    0,                                                             // src = DST_ALPHA, dst = GL_DST_COLOR
    0,                                                             // src = DST_ALPHA, dst = GL_ONE_MINUS_DST_COLOR
    0,                                                             // src = DST_ALPHA, dst = DST_ALPHA
    0,                                                             // src = DST_ALPHA, dst = ONE_MINUS_DST_ALPHA

    0, 0, 0, 0, 0, 0, 0, 0,                                        // src = ONE_MINUS_DST_ALPHA, dst = ...
};

void gl_rendermode_init()
{
    state.fog_start = 0.0f;
    state.fog_end = 1.0f;

    glEnable(GL_DITHER);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glAlphaFunc(GL_ALWAYS, 0.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    GLfloat fog_color[] = {0, 0, 0, 0};
    glFogfv(GL_FOG_COLOR, fog_color);
}

void glFogi(GLenum pname, GLint param)
{
    switch (pname) {
    case GL_FOG_MODE:
        assertf(param == GL_LINEAR, "Only linear fog is supported!");
        break;
    case GL_FOG_START:
        state.fog_start = param;
        break;
    case GL_FOG_END:
        state.fog_end = param;
        break;
    case GL_FOG_DENSITY:
    case GL_FOG_INDEX:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glFogf(GLenum pname, GLfloat param)
{
    switch (pname) {
    case GL_FOG_MODE:
        assertf(param == GL_LINEAR, "Only linear fog is supported!");
        break;
    case GL_FOG_START:
        state.fog_start = param;
        break;
    case GL_FOG_END:
        state.fog_end = param;
        break;
    case GL_FOG_DENSITY:
    case GL_FOG_INDEX:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glFogiv(GLenum pname, const GLint *params)
{
    switch (pname) {
    case GL_FOG_COLOR:
        rdpq_set_fog_color(RGBA32(
            MAX(params[0]>>23, 0),
            MAX(params[1]>>23, 0),
            MAX(params[2]>>23, 0),
            0xFF
        ));
        break;
    case GL_FOG_MODE:
    case GL_FOG_START:
    case GL_FOG_END:
    case GL_FOG_DENSITY:
    case GL_FOG_INDEX:
        glFogi(pname, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glFogfv(GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_FOG_COLOR:
        rdpq_set_fog_color(RGBA32(
            FLOAT_TO_U8(params[0]),
            FLOAT_TO_U8(params[1]),
            FLOAT_TO_U8(params[2]),
            0xFF
        ));
        break;
    case GL_FOG_MODE:
    case GL_FOG_START:
    case GL_FOG_END:
    case GL_FOG_DENSITY:
    case GL_FOG_INDEX:
        glFogf(pname, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    if (left < 0 || bottom < 0) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    uint64_t rect = (((uint64_t)left) << 48) | (((uint64_t)bottom) << 32) | (((uint64_t)width) << 16) | ((uint64_t)height);
    gl_set_long(GL_UPDATE_SCISSOR, offsetof(gl_server_state_t, scissor_rect), rect);
}

void glBlendFunc(GLenum src, GLenum dst)
{
    switch (src) {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA: 
    case GL_DST_ALPHA:
        break;
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR: 
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
        assertf(0, "Unsupported blend source factor");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (dst) {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
        break;
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_ONE_MINUS_SRC_COLOR:
        assertf(0, "Unsupported blend destination factor");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    uint32_t config_index = ((src & 0x7) << 3) | (dst & 0x7);

    uint32_t cycle = blend_configs[config_index] | SOM_BLENDING;
    assertf(cycle != 0, "Unsupported blend function");

    // TODO: coalesce these
    gl_set_word(GL_UPDATE_NONE, offsetof(gl_server_state_t, blend_src), (((uint32_t)src) << 16) | (uint32_t)dst);
    gl_set_word(GL_UPDATE_BLEND_CYCLE, offsetof(gl_server_state_t, blend_cycle), cycle);
}

void glDepthFunc(GLenum func)
{
    switch (func) {
    case GL_LESS:
    case GL_ALWAYS:
        gl_set_short(GL_UPDATE_DEPTH_TEST, offsetof(gl_server_state_t, depth_func), (uint16_t)func);
        break;
    case GL_NEVER:
    case GL_EQUAL:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
        assertf(0, "Depth func not supported: %lx", func);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glDepthMask(GLboolean mask)
{
    gl_set_flag(GL_UPDATE_DEPTH_MASK, FLAG_DEPTH_MASK, mask);
}

void glAlphaFunc(GLenum func, GLclampf ref)
{
    switch (func) {
    case GL_GREATER:
    case GL_ALWAYS:
        gl_set_short(GL_UPDATE_ALPHA_TEST, offsetof(gl_server_state_t, alpha_func), (uint16_t)func);
        gl_set_byte(GL_UPDATE_NONE, offsetof(gl_server_state_t, alpha_ref), FLOAT_TO_U8(ref));
        rdpq_set_blend_color(RGBA32(0, 0, 0, FLOAT_TO_U8(ref)));
        break;
    case GL_NEVER:
    case GL_EQUAL:
    case GL_LEQUAL:
    case GL_LESS:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
        assertf(0, "Alpha func not supported: %lx", func);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    if (target != GL_TEXTURE_ENV || pname != GL_TEXTURE_ENV_MODE) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (param) {
    case GL_MODULATE:
    case GL_REPLACE:
        gl_set_short(GL_UPDATE_COMBINER, offsetof(gl_server_state_t, tex_env_mode), (uint16_t)param);
        break;
    case GL_DECAL:
    case GL_BLEND:
        assertf(0, "Unsupported Tex Env mode!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    glTexEnvi(target, pname, param);
}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
    if (target != GL_TEXTURE_ENV) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (pname) {
    case GL_TEXTURE_ENV_COLOR:
        assertf(0, "Tex env color is not supported!");
        break;
    default:
        glTexEnvi(target, pname, params[0]);
        break;
    }
}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
    if (target != GL_TEXTURE_ENV) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (pname) {
    case GL_TEXTURE_ENV_COLOR:
        assertf(0, "Tex env color is not supported!");
        break;
    default:
        glTexEnvf(target, pname, params[0]);
        break;
    }
}
