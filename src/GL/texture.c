#include "gl_internal.h"
#include "rdpq.h"
#include "debug.h"

extern gl_state_t state;

void gl_texture_init()
{
    state.texture_2d_object = (gl_texture_object_t) {
        .wrap_s = GL_REPEAT,
        .wrap_t = GL_REPEAT,
        .min_filter = GL_NEAREST_MIPMAP_LINEAR,
        .mag_filter = GL_LINEAR,
    };
}

uint32_t gl_log2(uint32_t s)
{
    uint32_t log = 0;
    while (s >>= 1) ++log;
    return log;
}

tex_format_t gl_texture_get_format(const gl_texture_object_t *texture_object)
{
    switch (texture_object->internal_format) {
    case GL_RGB5_A1:
        return FMT_RGBA16;
    case GL_RGBA8:
        return FMT_RGBA32;
    case GL_LUMINANCE4_ALPHA4:
        return FMT_IA8;
    case GL_LUMINANCE8_ALPHA8:
        return FMT_IA16;
    case GL_LUMINANCE8:
    case GL_INTENSITY8:
        return FMT_I8;
    default:
        return FMT_NONE;
    }
}

GLint gl_choose_internalformat(GLint requested)
{
    switch (requested) {
    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:
        return GL_LUMINANCE8;

    // TODO: is intensity semantically equivalent to alpha?
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
        return GL_INTENSITY8;

    case 2:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
        return GL_LUMINANCE4_ALPHA4;

    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16:
        return GL_LUMINANCE8_ALPHA8;

    case 3:
    case 4:
    case GL_RGB:
    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
        return GL_RGB5_A1;

    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
        return GL_RGBA8;

    default:
        return -1;
    }
}

bool gl_copy_pixels(void *dst, const void *src, GLint dst_fmt, GLenum src_fmt, GLenum src_type)
{
    // TODO: Actually copy the pixels. Right now this function does nothing unless the 
    //       source format/type does not match the destination format directly, then it asserts.

    switch (dst_fmt) {
    case GL_RGB5_A1:
        if (src_fmt == GL_RGBA && src_type == GL_UNSIGNED_SHORT_5_5_5_1_EXT) {
            return true;
        }
        break;
    case GL_RGBA8:
        if (src_fmt == GL_RGBA && (src_type == GL_UNSIGNED_BYTE || src_type == GL_BYTE || src_type == GL_UNSIGNED_INT_8_8_8_8_EXT)) {
            return true;
        }
        break;
    case GL_LUMINANCE4_ALPHA4:
        break;
    case GL_LUMINANCE8_ALPHA8:
        if (src_fmt == GL_LUMINANCE_ALPHA && (src_type == GL_UNSIGNED_BYTE || src_type == GL_BYTE)) {
            return true;
        }
        break;
    case GL_LUMINANCE8:
    case GL_INTENSITY8:
        if (src_fmt == GL_LUMINANCE && (src_type == GL_UNSIGNED_BYTE || src_type == GL_BYTE)) {
            return true;
        }
        break;
    }

    assertf(0, "Pixel format conversion not yet implemented!");

    return false;
}

gl_texture_object_t * gl_get_texture_object(GLenum target)
{
    switch (target) {
    case GL_TEXTURE_2D:
        return &state.texture_2d_object;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}

gl_texture_object_t * gl_get_active_texture()
{
    if (state.texture_2d) {
        return &state.texture_2d_object;
    }

    return NULL;
}

bool gl_texture_is_active(gl_texture_object_t *texture)
{
    return texture == gl_get_active_texture();
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (obj == NULL) {
        return;
    }

    GLint preferred_format = gl_choose_internalformat(internalformat);
    if (preferred_format < 0) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    switch (format) {
    case GL_COLOR_INDEX:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
    case GL_BITMAP:
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_UNSIGNED_BYTE_3_3_2_EXT:
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
    case GL_UNSIGNED_SHORT_5_5_5_1_EXT:
    case GL_UNSIGNED_INT_8_8_8_8_EXT:
    case GL_UNSIGNED_INT_10_10_10_2_EXT:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    obj->data = (void*)data;
    gl_copy_pixels(obj->data, data, preferred_format, format, type);

    obj->width = width;
    obj->height = height;
    obj->internal_format = preferred_format;
    obj->format = format;
    obj->type = type;
    obj->is_dirty = true;
}

void gl_texture_set_wrap_s(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
        GL_SET_STATE(obj->wrap_s, param, obj->is_dirty);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_wrap_t(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
        GL_SET_STATE(obj->wrap_t, param, obj->is_dirty);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_min_filter(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_NEAREST:
    case GL_LINEAR:
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_LINEAR:
        GL_SET_STATE(obj->min_filter, param, obj->is_dirty);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_mag_filter(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_NEAREST:
    case GL_LINEAR:
        GL_SET_STATE(obj->mag_filter, param, obj->is_dirty);
        if (obj->is_dirty && gl_texture_is_active(obj)) {
            state.is_rendermode_dirty = true;
        }
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_border_color(gl_texture_object_t *obj, GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    obj->border_color[0] = CLAMP01(r);
    obj->border_color[1] = CLAMP01(g);
    obj->border_color[2] = CLAMP01(b);
    obj->border_color[3] = CLAMP01(a);
    obj->is_dirty = true;
}

void gl_texture_set_priority(gl_texture_object_t *obj, GLclampf param)
{
    obj->priority = CLAMP01(param);
    obj->is_dirty = true;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (obj == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, param);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, param);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, param);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(obj, param);
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(obj, I32_TO_FLOAT(param));
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (obj == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, param);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, param);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, param);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(obj, param);
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(obj, param);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (obj == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, params[0]);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, params[0]);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, params[0]);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(obj, params[0]);
        break;
    case GL_TEXTURE_BORDER_COLOR:
        gl_texture_set_border_color(obj, I32_TO_FLOAT(params[0]), I32_TO_FLOAT(params[1]), I32_TO_FLOAT(params[2]), I32_TO_FLOAT(params[3]));
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(obj, I32_TO_FLOAT(params[0]));
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (obj == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, params[0]);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, params[0]);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, params[0]);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(obj, params[0]);
        break;
    case GL_TEXTURE_BORDER_COLOR:
        gl_texture_set_border_color(obj, params[0], params[1], params[2], params[3]);
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(obj, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_update_texture()
{
    gl_texture_object_t *tex_obj = gl_get_active_texture();

    if (tex_obj == NULL || !tex_obj->is_dirty) {
        return;
    }

    tex_format_t fmt = gl_texture_get_format(tex_obj);

    // TODO: min filter (mip mapping?)
    // TODO: border color?
    rdpq_set_texture_image(tex_obj->data, fmt, tex_obj->width);

    uint8_t mask_s = tex_obj->wrap_s == GL_REPEAT ? gl_log2(tex_obj->width) : 0;
    uint8_t mask_t = tex_obj->wrap_t == GL_REPEAT ? gl_log2(tex_obj->height) : 0;

    rdpq_set_tile_full(0, fmt, 0, tex_obj->width * TEX_FORMAT_BYTES_PER_PIXEL(fmt), 0, 0, 0, mask_t, 0, 0, 0, mask_s, 0);
    rdpq_load_tile(0, 0, 0, tex_obj->width, tex_obj->height);

    tex_obj->is_dirty = false;
}
