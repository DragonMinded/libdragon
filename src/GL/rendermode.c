#include <float.h>

#include "utils.h"
#include "gl_internal.h"
#include "rdpq_mode.h"
#include "rdpq_debug.h"
#include "rdpq_macros.h"
#include "rdpq_attach.h"
#include "rspq.h"
#include "mgfx.h"

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
    ringbuffer_init(&state->fog_buffer, sizeof(mgfx_fog_t), 4);

    glEnable(GL_DITHER);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glAlphaFunc(GL_ALWAYS, 0.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    GLfloat fog_color[] = {0, 0, 0, 0};
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_START, 0.0f);
    glFogf(GL_FOG_END, 1.0f);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
    glHint(GL_MULTISAMPLE_HINT_N64, GL_DONT_CARE);
}

void gl_rendermode_close()
{
    ringbuffer_free(&state->fog_buffer);
}

void gl_upload_fog(const mg_uniform_t *uniform)
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_FOG_UNIFORM)) return;

    bool enabled = gl_is_enabled(ENABLE_FOG);
    mgfx_fog_t *buffer = ringbuffer_alloc_next(&state->fog_buffer);
    mgfx_get_fog(buffer, &(mgfx_fog_parms_t) {
        .start = enabled ? state->fog_start : 0.0f,
        .end = enabled ? state->fog_end : 0.0f
    });
    mg_uniform_load(uniform, buffer);
    ringbuffer_release_current(&state->fog_buffer);
}

void gl_set_fog_start(GLfloat param)
{
    state->fog_start = param;
    gl_set_dirty_flags(DIRTY_FOG_UNIFORM);
}

void gl_set_fog_end(GLfloat param)
{
    state->fog_end = param;
    gl_set_dirty_flags(DIRTY_FOG_UNIFORM);
}

void glFogi(GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_FOG_MODE:
        assertf(param == GL_LINEAR, "Only linear fog is supported!");
        break;
    case GL_FOG_START:
        gl_set_fog_start(param);
        break;
    case GL_FOG_END:
        gl_set_fog_end(param);
        break;
    case GL_FOG_DENSITY:
    case GL_FOG_INDEX:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glFogf(GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_FOG_MODE:
        assertf(param == GL_LINEAR, "Only linear fog is supported!");
        break;
    case GL_FOG_START:
        gl_set_fog_start(param);
        break;
    case GL_FOG_END:
        gl_set_fog_end(param);
        break;
    case GL_FOG_DENSITY:
    case GL_FOG_INDEX:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glFogiv(GLenum pname, const GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glFogfv(GLenum pname, const GLfloat *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (left < 0) {
        gl_set_error(GL_INVALID_VALUE, "Left must not be negative");
        return;
    }
    
    if (bottom < 0) {
        gl_set_error(GL_INVALID_VALUE, "Bottom must not be negative");
        return;
    }

    const surface_t *fb = rdpq_get_attached();

    rdpq_set_scissor(left, fb->height - (bottom + height), left + width, fb->height - bottom);
}

void glBlendFunc(GLenum src, GLenum dst)
{
    if (!gl_ensure_no_begin_end()) return;
    
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid blend source factor", src);
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid blend destination factor", dst);
        return;
    }

    uint32_t config_index = ((src & 0x7) << 3) | (dst & 0x7);
    rdpq_blender_t blender = blend_configs[config_index];
    assertf(blender != 0, "Unsupported blend function");

    state->blender = blender;
    state->blend_src = src;
    state->blend_dst = dst;

    gl_set_dirty_flags(DIRTY_BLENDER);
}

void glDepthFunc(GLenum func)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (func) {
    case GL_LESS:
    case GL_ALWAYS:
    case GL_EQUAL:
    case GL_LESS_INTERPENETRATING_N64:
        break;
    case GL_NEVER:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
        assertf(0, "Depth func not supported: %#04lx", func);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid depth function", func);
        return;
    }

    state->depth_func = func;
    gl_set_dirty_flags(DIRTY_GEOM_FLAGS | DIRTY_ZBUF);
}

void glDepthMask(GLboolean mask)
{
    if (!gl_ensure_no_begin_end()) return;
    
    state->depth_mask = mask;
    gl_set_dirty_flags(DIRTY_GEOM_FLAGS | DIRTY_ZBUF);
}

bool is_depth_compare_active()
{
    return gl_is_enabled(GL_DEPTH_TEST) && state->depth_func != GL_ALWAYS;
}

bool is_depth_update_active()
{
    return gl_is_enabled(GL_DEPTH_TEST) && state->depth_mask != GL_FALSE;
}

bool gl_is_depth_active()
{
    return is_depth_compare_active() || is_depth_update_active();
}

void glAlphaFunc(GLenum func, GLclampf ref)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (func) {
    case GL_GREATER:
    case GL_ALWAYS:
        break;
    case GL_NEVER:
    case GL_EQUAL:
    case GL_LEQUAL:
    case GL_LESS:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
        assertf(0, "Alpha func not supported: %#04lx", func);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid alpha function", func);
        return;
    }

    state->alpha_func = func;
    state->alpha_ref = ref;
    gl_set_dirty_flags(DIRTY_ALPHACOMPARE);
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (target != GL_TEXTURE_ENV) {
        gl_set_error(GL_INVALID_ENUM, "Target must be GL_TEXTURE_ENV");
        return;
    }
    
    if (pname != GL_TEXTURE_ENV_MODE) {
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }

    switch (param) {
    case GL_MODULATE:
    case GL_REPLACE:
        state->tex_env_mode = param;
        gl_set_dirty_flags(DIRTY_COMBINER | DIRTY_GEOM_FLAGS);
        break;
    case GL_DECAL:
    case GL_BLEND:
        assertf(0, "Unsupported Tex Env mode!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid tex env mode", param);
        return;
    }
}
void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    glTexEnvi(target, pname, param);
}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (target != GL_TEXTURE_ENV) {
        gl_set_error(GL_INVALID_ENUM, "Target must be GL_TEXTURE_ENV");
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
    if (!gl_ensure_no_begin_end()) return;
    
    if (target != GL_TEXTURE_ENV) {
        gl_set_error(GL_INVALID_ENUM, "Target must be GL_TEXTURE_ENV");
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

rdpq_antialias_t get_antialias()
{
    if (!gl_is_enabled(ENABLE_MULTISAMPLE)) {
        return AA_NONE;
    } else {
        return gl_is_hint_enabled(HINT_FULL_AA) ? AA_STANDARD : AA_REDUCED;
    }
}

rdpq_dither_t get_dither()
{
    return gl_is_enabled(ENABLE_DITHER) ? state->dither_mode : DITHER_NONE_NONE;
}

rdpq_zmode_t get_zmode()
{
    if (!gl_is_enabled(ENABLE_DEPTH_TEST)) return ZMODE_STANDARD;

    switch (state->depth_func)
    {
    case GL_EQUAL:
        return ZMODE_DECAL;
    case GL_LESS_INTERPENETRATING_N64:
        return ZMODE_INTERPENETRATING;
    default:
        return ZMODE_STANDARD;
    }
}

bool is_texture_replace()
{
    return state->tex_env_mode == GL_REPLACE;
}

bool is_color_constant()
{
    // Color is always considered varying between glBegin/glEnd
    return !state->begin_end_active && !state->array_object->arrays[ATTRIB_COLOR].enabled;
}

bool gl_is_shade_active()
{
    // Fog always requires shade
    if (gl_is_enabled(ENABLE_FOG)) 
        return true;

    // Shade is unused if texture replaces it
    if (gl_is_texture_active() && is_texture_replace())
        return false;

    // Otherwise it is only used if lighting is active or if vertex color is not constant
    // (If not, prim color is used instead)
    return gl_is_enabled(ENABLE_LIGHTING) || !is_color_constant();
}

color_t get_prim_color()
{
    if (gl_is_enabled(ENABLE_LIGHTING)) {
        return gl_get_material_diffuse();
    } else {
        return color_from_packed32(state->current_attribs.color);
    }
}

void apply_prim_color()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_PRIM_COLOR)) return;
    rdpq_set_prim_color(get_prim_color());
}

void apply_antialias()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_ANTIALIAS)) return;
    rdpq_mode_antialias(get_antialias());
}

void apply_dither()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_DITHER)) return;
    rdpq_mode_dithering(get_dither());
}

rdpq_combiner_t get_combiner()
{
    bool has_tex = gl_is_texture_active();
    if (has_tex && is_texture_replace()) {
        return RDPQ_COMBINER_TEX;
    }

    // TODO: emissive color
    static const rdpq_combiner_t table[] = {
        RDPQ_COMBINER_SHADE,                                                                            // No tex, no light, var color
        RDPQ_COMBINER_FLAT,                                                                             // No tex, no light, const color
        RDPQ_COMBINER_SHADE,                                                                            // No tex, light,    var color
        RDPQ_COMBINER1((SHADE,0,PRIM,0), (SHADE,0,PRIM,0)),                                             // No tex, light,    const color
        RDPQ_COMBINER_TEX_SHADE,                                                                        // tex,    no light, var color
        RDPQ_COMBINER_TEX_FLAT,                                                                         // tex,    no light, const color
        RDPQ_COMBINER_TEX_SHADE,                                                                        // tex,    light,    var color
        // TODO: doesn't work with mipmapping
        RDPQ_COMBINER2((TEX0,0,SHADE,0), (TEX0,0,SHADE,0), (COMBINED,0,PRIM,0), (COMBINED,0,PRIM,0)),   // tex,    light,    const color
    };

    bool constant_color = is_color_constant();
    if (gl_is_enabled(ENABLE_LIGHTING)) constant_color = constant_color || !gl_is_diffuse_tracking_color();

    uint32_t index = 0;
    if (has_tex) index |= 4;
    if (gl_is_enabled(ENABLE_LIGHTING)) index |= 2;
    if (constant_color) index |= 1;

    return table[index];
}

void apply_combiner()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_COMBINER)) return;
    rdpq_mode_combiner(get_combiner());
}

rdpq_blender_t get_blender()
{
    return gl_is_enabled(ENABLE_BLEND) ? state->blender : 0;
}

void apply_blender()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_BLENDER)) return;
    rdpq_mode_blender(get_blender());
}

rdpq_blender_t get_fog()
{
    return gl_is_enabled(ENABLE_FOG) ? RDPQ_BLENDER((FOG_RGB, SHADE_ALPHA, IN_RGB, INV_MUX_ALPHA)) : 0;
}

void apply_fog()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_FOG)) return;
    rdpq_mode_fog(get_fog());
}

int get_alphacompare()
{
    return gl_is_enabled(ENABLE_ALPHA_TEST) && state->alpha_func != GL_ALWAYS ? FLOAT_TO_U8(state->alpha_ref) : 0;
}

void apply_alphacompare()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_ALPHACOMPARE)) return;
    rdpq_mode_alphacompare(get_alphacompare());
}

void apply_zbuf()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_ZBUF)) return;
    rdpq_mode_zbuf(is_depth_compare_active(), is_depth_update_active());
}

void apply_zmode()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_ZMODE)) return;
    rdpq_mode_zmode(get_zmode());
}

void apply_persp()
{
    if (!gl_check_and_clear_dirty_flags(DIRTY_PERSP)) return;
    rdpq_mode_persp(gl_is_hint_enabled(HINT_PERSP_CORRECT));
}

void apply_rendermode(bool reset)
{
    if (reset) {
        // When resetting, we need to re-apply all modes. Mark all of them as dirty.
        gl_set_dirty_flags(DIRTY_RDPQ_MODE_ALL);
    }
    
    if (gl_check_dirty_flags_any(DIRTY_RDPQ_MODE_ALL)) {
        rdpq_mode_begin();
            if (reset) rdpq_set_mode_standard();
            apply_antialias();
            apply_dither();
            apply_combiner();
            apply_blender();
            apply_fog();
            apply_alphacompare();
            apply_zbuf();
            apply_zmode();
            apply_persp();
        rdpq_mode_end();
    }

    apply_prim_color();
}
