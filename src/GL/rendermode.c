#include "gl_internal.h"
#include "rdpq.h"

extern gl_state_t state;

void gl_rendermode_init()
{
    state.fog_start = 0.0f;
    state.fog_end = 1.0f;

    state.is_rendermode_dirty = true;
    state.is_scissor_dirty = true;

    glBlendFunc(GL_ONE, GL_ZERO);
    glDepthFunc(GL_LESS);
    glAlphaFunc(GL_ALWAYS, 0.0f);

    GLfloat fog_color[] = {0, 0, 0, 0};
    glFogfv(GL_FOG_COLOR, fog_color);
}

bool gl_is_invisible()
{
    return state.draw_buffer == GL_NONE 
        || (state.depth_test && state.depth_func == GL_NEVER)
        || (state.alpha_test && state.alpha_func == GL_NEVER);
}

void gl_update_scissor()
{
    if (!state.is_scissor_dirty) {
        return;
    }

    uint32_t w = state.cur_framebuffer->color_buffer->width;
    uint32_t h = state.cur_framebuffer->color_buffer->height;

    if (state.scissor_test) {
        rdpq_set_scissor(
            state.scissor_box[0],
            h - state.scissor_box[1] - state.scissor_box[3],
            state.scissor_box[0] + state.scissor_box[2],
            h - state.scissor_box[1]
        );
    } else {
        rdpq_set_scissor(0, 0, w, h);
    }

    state.is_scissor_dirty = false;
}

void gl_update_render_mode()
{
    if (!state.is_rendermode_dirty) {
        return;
    }

    uint64_t modes = SOM_CYCLE_1;
    uint64_t combine = 0;

    if (state.dither) {
        modes |= SOM_RGBDITHER_SQUARE | SOM_ALPHADITHER_SQUARE;
    } else {
        modes |= SOM_RGBDITHER_NONE | SOM_ALPHADITHER_NONE;
    }

    if (0 /* antialiasing */) {
        modes |= SOM_AA_ENABLE | SOM_READ_ENABLE | SOM_COLOR_ON_COVERAGE | SOM_COVERAGE_DEST_CLAMP | SOM_ALPHA_USE_CVG;
    }

    if (state.depth_test) {
        modes |= SOM_Z_WRITE | SOM_Z_OPAQUE | SOM_Z_SOURCE_PIXEL;

        if (state.depth_func == GL_LESS) {
            modes |= SOM_Z_COMPARE;
        }
    }

    if (state.blend) {
        // TODO: derive the blender config from blend_src and blend_dst
        modes |= SOM_BLENDING | Blend(PIXEL_RGB, MUX_ALPHA, MEMORY_RGB, INV_MUX_ALPHA);
    }

    if (state.fog) {
        modes |= SOM_BLENDING | Blend(PIXEL_RGB, SHADE_ALPHA, FOG_RGB, INV_MUX_ALPHA);
    }

    if (state.alpha_test && state.alpha_func == GL_GREATER) {
        modes |= SOM_ALPHA_COMPARE;
    }
    
    if (state.texture_2d) {
        modes |= SOM_TEXTURE_PERSP | SOM_TC_FILTER;

        if (state.texture_2d_object.mag_filter == GL_LINEAR) {
            modes |= SOM_SAMPLE_2X2;
        }

        combine = Comb_Rgb(TEX0, ZERO, SHADE, ZERO) | Comb_Alpha(TEX0, ZERO, SHADE, ZERO);
    } else {
        combine = Comb_Rgb(ONE, ZERO, SHADE, ZERO) | Comb_Alpha(ONE, ZERO, SHADE, ZERO);
    }

    rdpq_set_combine_mode(combine);
    rdpq_set_other_modes(modes);

    state.is_rendermode_dirty = false;
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
            MAX(params[3]>>23, 0)
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
            FLOAT_TO_U8(params[3])
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

    state.scissor_box[0] = left;
    state.scissor_box[1] = bottom;
    state.scissor_box[2] = width;
    state.scissor_box[3] = height;

    state.is_scissor_dirty = true;
}

void glBlendFunc(GLenum src, GLenum dst)
{
    switch (src) {
    case GL_ZERO: 
    case GL_ONE: 
    case GL_DST_COLOR: 
    case GL_ONE_MINUS_DST_COLOR: 
    case GL_SRC_ALPHA: 
    case GL_ONE_MINUS_SRC_ALPHA: 
    case GL_DST_ALPHA: 
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (dst) {
    case GL_ZERO: 
    case GL_ONE: 
    case GL_DST_COLOR: 
    case GL_ONE_MINUS_DST_COLOR: 
    case GL_SRC_ALPHA: 
    case GL_ONE_MINUS_SRC_ALPHA: 
    case GL_DST_ALPHA: 
    case GL_ONE_MINUS_DST_ALPHA:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    state.blend_src = src;
    state.blend_dst = dst;
    state.is_rendermode_dirty = true;
}

void glDepthFunc(GLenum func)
{
    switch (func) {
    case GL_NEVER:
    case GL_LESS:
    case GL_ALWAYS:
        GL_SET_STATE(state.depth_func, func, state.is_rendermode_dirty);
        break;
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

void glAlphaFunc(GLenum func, GLclampf ref)
{
    switch (func) {
    case GL_NEVER:
    case GL_GREATER:
    case GL_ALWAYS:
        GL_SET_STATE(state.alpha_func, func, state.is_rendermode_dirty);
        state.alpha_ref = ref;
        rdpq_set_blend_color(RGBA32(0, 0, 0, FLOAT_TO_U8(ref)));
        break;
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

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    assertf(0, "Stencil is not supported!");
}

void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass)
{
    assertf(0, "Stencil is not supported!");
}

void glLogicOp(GLenum op)
{
    assertf(0, "Logical operation is not supported!");
}
