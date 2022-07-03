#include "gl_internal.h"
#include "rdpq.h"
#include "debug.h"

extern gl_state_t state;

void gl_init_texture_object(gl_texture_object_t *obj)
{
    *obj = (gl_texture_object_t) {
        .dimensionality = GL_TEXTURE_2D,
        .wrap_s = GL_REPEAT,
        .wrap_t = GL_REPEAT,
        .min_filter = GL_NEAREST_MIPMAP_LINEAR,
        .mag_filter = GL_LINEAR,
    };
}

void gl_texture_init()
{
    gl_init_texture_object(&state.default_texture_1d);
    gl_init_texture_object(&state.default_texture_2d);

    for (uint32_t i = 0; i < MAX_TEXTURE_OBJECTS; i++)
    {
        gl_init_texture_object(&state.texture_objects[i]);
    }

    state.default_texture_1d.is_used = true;
    state.default_texture_2d.is_used = true;

    state.texture_1d_object = &state.default_texture_1d;
    state.texture_2d_object = &state.default_texture_2d;
}

uint32_t gl_log2(uint32_t s)
{
    uint32_t log = 0;
    while (s >>= 1) ++log;
    return log;
}

tex_format_t gl_get_texture_format(GLenum format)
{
    switch (format) {
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
    case GL_TEXTURE_1D:
        return state.texture_1d_object;
    case GL_TEXTURE_2D:
        return state.texture_2d_object;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}

gl_texture_object_t * gl_get_active_texture()
{
    if (state.texture_2d) {
        return state.texture_2d_object;
    }

    if (state.texture_1d) {
        return state.texture_1d_object;
    }

    return NULL;
}

bool gl_texture_is_active(gl_texture_object_t *texture)
{
    return texture == gl_get_active_texture();
}

bool gl_get_texture_completeness(const gl_texture_object_t *texture, uint32_t *num_levels)
{
    const gl_texture_image_t *first_level = &texture->levels[0];

    if (first_level->width == 0 || first_level->height == 0) {
        *num_levels = 0;
        return false;
    }

    if (texture->min_filter == GL_NEAREST || texture->min_filter == GL_LINEAR) {
        // Mip mapping is disabled
        *num_levels = 1;
        return true;
    }

    GLenum format = first_level->internal_format;

    uint32_t cur_width = first_level->width;
    uint32_t cur_height = first_level->height;

    for (uint32_t i = 0; i < MAX_TEXTURE_LEVELS; i++)
    {
        const gl_texture_image_t *level = &texture->levels[i];

        if (cur_width != level->width || cur_height != level->height || level->internal_format != format) {
            break;
        }

        if (cur_width == 1 && cur_height == 1) {
            *num_levels = i + 1;
            return true;
        }

        if (cur_width > 1) {
            if (cur_width % 2 != 0) break;
            cur_width >>= 1;
        }

        if (cur_height > 1) {
            if (cur_height % 2 != 0) break;
            cur_height >>= 1;
        }
    }

    *num_levels = 0;
    return false;
}

void gl_update_texture_completeness(gl_texture_object_t *texture)
{
    texture->is_complete = gl_get_texture_completeness(texture, &texture->num_levels);
}

uint32_t add_tmem_size(uint32_t current, uint32_t size)
{
    return ROUND_UP(current + size, 8);
}

bool gl_texture_fits_tmem(gl_texture_object_t *texture, uint32_t additional_size)
{
    uint32_t size = 0;
    tex_format_t format = gl_get_texture_format(texture->levels[0].internal_format);
    for (uint32_t i = 0; i < texture->num_levels; i++)
    {
        uint32_t pitch = MAX(TEX_FORMAT_BYTES_PER_PIXEL(format) * texture->levels[i].width, 8);
        size = add_tmem_size(size, pitch * texture->levels[i].height);
    }
    
    size = add_tmem_size(size, additional_size);

    return size <= 0x1000;
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (obj == NULL) {
        return;
    }

    if (level < 0 || level > MAX_TEXTURE_LEVELS) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    gl_texture_image_t *image = &obj->levels[level];

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

    uint32_t rdp_format = gl_get_texture_format(preferred_format);
    uint32_t size = TEX_FORMAT_BYTES_PER_PIXEL(rdp_format) * width * height;

    if (!gl_texture_fits_tmem(obj, size)) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    // TODO: allocate buffer

    image->data = (void*)data;
    gl_copy_pixels(image->data, data, preferred_format, format, type);

    image->width = width;
    image->height = height;
    image->internal_format = preferred_format;
    state.is_texture_dirty = true;

    gl_update_texture_completeness(obj);
}

void gl_texture_set_wrap_s(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
        GL_SET_STATE(obj->wrap_s, param, state.is_texture_dirty);
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
        GL_SET_STATE(obj->wrap_t, param, state.is_texture_dirty);
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
        GL_SET_STATE(obj->min_filter, param, state.is_texture_dirty);
        gl_update_texture_completeness(obj);
        if (state.is_texture_dirty && gl_texture_is_active(obj)) {
            state.is_rendermode_dirty = true;
        }
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
        GL_SET_STATE(obj->mag_filter, param, state.is_texture_dirty);
        if (state.is_texture_dirty && gl_texture_is_active(obj)) {
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
    state.is_texture_dirty = true;
}

void gl_texture_set_priority(gl_texture_object_t *obj, GLclampf param)
{
    obj->priority = CLAMP01(param);
    state.is_texture_dirty = true;
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

void glBindTexture(GLenum target, GLuint texture)
{
    gl_texture_object_t **target_obj = NULL;

    switch (target) {
    case GL_TEXTURE_1D:
        target_obj = &state.texture_1d_object;
        break;
    case GL_TEXTURE_2D:
        target_obj = &state.texture_2d_object;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    if (texture == 0) {
        switch (target) {
        case GL_TEXTURE_1D:
            *target_obj = &state.default_texture_1d;
            break;
        case GL_TEXTURE_2D:
            *target_obj = &state.default_texture_2d;
            break;
        }
    } else {
        // TODO: Any texture name should be valid!
        assertf(texture > 0 && texture <= MAX_TEXTURE_OBJECTS, "NOT IMPLEMENTED: texture name out of range!");

        gl_texture_object_t *obj = &state.texture_objects[target - 1];

        if (obj->dimensionality == 0) {
            obj->dimensionality = target;
        }

        if (obj->dimensionality != target) {
            gl_set_error(GL_INVALID_OPERATION);
            return;
        }

        obj->is_used = true;

        *target_obj = obj;
    }
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    GLuint t = 0;

    for (uint32_t i = 0; i < n; i++)
    {
        gl_texture_object_t *obj;

        do {
            obj = &state.texture_objects[t++];
        } while (obj->is_used && t < MAX_TEXTURE_OBJECTS);

        // TODO: It shouldn't be possible to run out at this point!
        assertf(!obj->is_used, "Ran out of unused textures!");

        textures[i] = t;
        obj->is_used = true;
    }
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
    for (uint32_t i = 0; i < n; i++)
    {
        if (textures[i] == 0) {
            continue;
        }

        // TODO: Any texture name should be valid!
        assertf(textures[i] > 0 && textures[i] <= MAX_TEXTURE_OBJECTS, "NOT IMPLEMENTED: texture name out of range!");

        gl_texture_object_t *obj = &state.texture_objects[textures[i] - 1];

        if (obj == state.texture_1d_object) {
            state.texture_1d_object = &state.default_texture_1d;
        } else if (obj == state.texture_2d_object) {
            state.texture_2d_object = &state.default_texture_2d;
        }

        gl_init_texture_object(obj);
    }
}

void gl_update_texture()
{
    gl_texture_object_t *tex_obj = gl_get_active_texture();

    if (tex_obj == NULL || !tex_obj->is_complete) {
        return;
    }

    uint32_t tmem_used = 0;

    // All levels must have the same format to be complete
    tex_format_t fmt = gl_get_texture_format(tex_obj->levels[0].internal_format);

    uint32_t full_width = tex_obj->levels[0].width;
    uint32_t full_height = tex_obj->levels[0].height;

    int32_t full_width_log = gl_log2(full_width);
    int32_t full_height_log = gl_log2(full_height);

    for (uint32_t l = 0; l < tex_obj->num_levels; l++)
    {
        gl_texture_image_t *image = &tex_obj->levels[l];

        rdpq_set_texture_image(image->data, fmt, image->width);

        uint32_t tmem_pitch = MAX(image->width * TEX_FORMAT_BYTES_PER_PIXEL(fmt), 8);

        // Levels need to halve in size every time to be complete
        int32_t width_log = MAX(full_width_log - l, 0);
        int32_t height_log = MAX(full_height_log - l, 0);

        uint8_t mask_s = tex_obj->wrap_s == GL_REPEAT ? width_log : 0;
        uint8_t mask_t = tex_obj->wrap_t == GL_REPEAT ? height_log : 0;

        uint8_t shift_s = full_width_log - width_log;
        uint8_t shift_t = full_height_log - height_log;

        rdpq_set_tile_full(l, fmt, tmem_used, tmem_pitch, 0, 0, 0, mask_t, shift_t, 0, 0, mask_s, shift_s);
        rdpq_load_tile(l, 0, 0, image->width, image->height);

        tmem_used = add_tmem_size(tmem_used, tmem_pitch * image->height);
    }
}
