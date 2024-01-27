#include "gl_internal.h"
#include <debug.h>

extern gl_state_t state;

bool gl_calc_transfer_is_noop()
{
    if (state.map_color || state.unpack_swap_bytes) {
        return false;
    }

    for (uint32_t i = 0; i < 4; i++)
    {
        if (state.transfer_bias[i] != 0.0f || state.transfer_scale[i] != 1.0f) {
            return false;
        }
    }

    return true;
}

void gl_update_transfer_state()
{
    state.transfer_is_noop = gl_calc_transfer_is_noop();
}

void gl_pixel_init()
{
    state.unpack_alignment = 4;
    state.transfer_scale[0] = 1;
    state.transfer_scale[1] = 1;
    state.transfer_scale[2] = 1;
    state.transfer_scale[3] = 1;

    state.pixel_maps[0].size = 1;
    state.pixel_maps[1].size = 1;
    state.pixel_maps[2].size = 1;
    state.pixel_maps[3].size = 1;

    gl_update_transfer_state();
}

void glPixelStorei(GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_UNPACK_SWAP_BYTES:
        state.unpack_swap_bytes = param != 0;
        gl_update_transfer_state();
        break;
    case GL_UNPACK_LSB_FIRST:
        state.unpack_lsb_first = param != 0;
        break;
    case GL_UNPACK_ROW_LENGTH:
        if (param < 0) {
            gl_set_error(GL_INVALID_VALUE, "GL_UNPACK_ROW_LENGTH must not be negative");
            return;
        }
        state.unpack_row_length = param;
        break;
    case GL_UNPACK_SKIP_ROWS:
        if (param < 0) {
            gl_set_error(GL_INVALID_VALUE, "GL_UNPACK_SKIP_ROWS must not be negative");
            return;
        }
        state.unpack_skip_rows = param;
        break;
    case GL_UNPACK_SKIP_PIXELS:
        if (param < 0) {
            gl_set_error(GL_INVALID_VALUE, "GL_UNPACK_SKIP_PIXELS must not be negative");
            return;
        }
        state.unpack_skip_pixels = param;
        break;
    case GL_UNPACK_ALIGNMENT:
        if (param != 1 && param != 2 && param != 4 && param != 8) {
            gl_set_error(GL_INVALID_VALUE, "GL_UNPACK_ALIGNMENT must be 1, 2, 4 or 8");
            return;
        }
        state.unpack_alignment = param;
        break;
    case GL_PACK_SWAP_BYTES:
    case GL_PACK_LSB_FIRST:
    case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_ROWS:
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_ALIGNMENT:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glPixelStoref(GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_UNPACK_SWAP_BYTES:
        state.unpack_swap_bytes = param != 0.0f;
        gl_update_transfer_state();
        break;
    case GL_UNPACK_LSB_FIRST:
        state.unpack_lsb_first = param != 0.0f;
        break;
    default:
        glPixelStorei(pname, param);
        break;
    }
}

void glPixelTransferi(GLenum pname, GLint value)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_MAP_COLOR:
        state.map_color = value != 0;
        gl_update_transfer_state();
        break;
    default:
        glPixelTransferf(pname, value);
        break;
    }
}

void glPixelTransferf(GLenum pname, GLfloat value)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (pname) {
    case GL_MAP_COLOR:
        state.map_color = value != 0.0f;
        break;
    case GL_RED_SCALE:
        state.transfer_scale[0] = value;
        break;
    case GL_GREEN_SCALE:
        state.transfer_scale[1] = value;
        break;
    case GL_BLUE_SCALE:
        state.transfer_scale[2] = value;
        break;
    case GL_ALPHA_SCALE:
        state.transfer_scale[3] = value;
        break;
    case GL_RED_BIAS:
        state.transfer_bias[0] = value;
        break;
    case GL_GREEN_BIAS:
        state.transfer_bias[1] = value;
        break;
    case GL_BLUE_BIAS:
        state.transfer_bias[2] = value;
        break;
    case GL_ALPHA_BIAS:
        state.transfer_bias[3] = value;
        break;
    case GL_DEPTH_SCALE:
    case GL_DEPTH_BIAS:
    case GL_MAP_STENCIL:
    case GL_INDEX_SHIFT:
    case GL_INDEX_OFFSET:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }

    gl_update_transfer_state();
}

gl_pixel_map_t * gl_get_pixel_map(GLenum map)
{
    switch (map) {
    case GL_PIXEL_MAP_R_TO_R:
        return &state.pixel_maps[0];
    case GL_PIXEL_MAP_G_TO_G:
        return &state.pixel_maps[1];
    case GL_PIXEL_MAP_B_TO_B:
        return &state.pixel_maps[2];
    case GL_PIXEL_MAP_A_TO_A:
        return &state.pixel_maps[3];
    case GL_PIXEL_MAP_I_TO_I:
    case GL_PIXEL_MAP_S_TO_S:
    case GL_PIXEL_MAP_I_TO_R:
    case GL_PIXEL_MAP_I_TO_G:
    case GL_PIXEL_MAP_I_TO_B:
    case GL_PIXEL_MAP_I_TO_A:
        return NULL;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid pixel map", map);
        return NULL;
    }
}

void glPixelMapusv(GLenum map, GLsizei size, const GLushort *values)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_pixel_map_t *pixel_map = gl_get_pixel_map(map);
    if (pixel_map == NULL) {
        return;
    }

    if (size < 1 || size > MAX_PIXEL_MAP_SIZE) {
        gl_set_error(GL_INVALID_VALUE, "Size must be in [1,%d]", MAX_PIXEL_MAP_SIZE);
        return;
    }

    for (GLsizei i = 0; i < size; i++)
    {
        pixel_map->entries[i] = U16_TO_FLOAT(values[i]);
    }
}

void glPixelMapuiv(GLenum map, GLsizei size, const GLuint *values)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_pixel_map_t *pixel_map = gl_get_pixel_map(map);
    if (pixel_map == NULL) {
        return;
    }

    if (size < 1 || size > MAX_PIXEL_MAP_SIZE) {
        gl_set_error(GL_INVALID_VALUE, "Size must be in [1,%d]", MAX_PIXEL_MAP_SIZE);
        return;
    }

    for (GLsizei i = 0; i < size; i++)
    {
        pixel_map->entries[i] = U32_TO_FLOAT(values[i]);
    }
}

void glPixelMapfv(GLenum map, GLsizei size, const GLfloat *values)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_pixel_map_t *pixel_map = gl_get_pixel_map(map);
    if (pixel_map == NULL) {
        return;
    }

    if (size < 1 || size > MAX_PIXEL_MAP_SIZE) {
        gl_set_error(GL_INVALID_VALUE, "Size must be in [1,%d]", MAX_PIXEL_MAP_SIZE);
        return;
    }

    for (GLsizei i = 0; i < size; i++)
    {
        pixel_map->entries[i] = values[i];
    }
}
