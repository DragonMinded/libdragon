#include "gl_internal.h"
#include "rdpq.h"
#include "debug.h"
#include <math.h>
#include <string.h>
#include <malloc.h>

extern gl_state_t state;

void gl_init_texture_object(gl_texture_object_t *obj)
{
    memset(obj, 0, sizeof(gl_texture_object_t));

    *obj = (gl_texture_object_t) {
        .wrap_s = GL_REPEAT,
        .wrap_t = GL_REPEAT,
        .min_filter = GL_NEAREST_MIPMAP_LINEAR,
        .mag_filter = GL_LINEAR,
    };
}

void gl_cleanup_texture_object(gl_texture_object_t *obj)
{
    for (uint32_t i = 0; i < MAX_TEXTURE_LEVELS; i++)
    {
        if (obj->levels[i].data != NULL) {
            free_uncached(obj->levels[i].data);
        }
    }
}

void gl_texture_init()
{
    state.default_textures = malloc_uncached(sizeof(gl_texture_object_t) * 2);

    gl_init_texture_object(&state.default_textures[0]);
    gl_init_texture_object(&state.default_textures[1]);

    state.default_textures[0].dimensionality = GL_TEXTURE_1D;
    state.default_textures[1].dimensionality = GL_TEXTURE_2D;

    state.texture_1d_object = &state.default_textures[0];
    state.texture_2d_object = &state.default_textures[1];
}

void gl_texture_close()
{
    gl_cleanup_texture_object(&state.default_textures[0]);
    gl_cleanup_texture_object(&state.default_textures[1]);

    free_uncached(state.default_textures);
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
    case GL_INTENSITY4:
        return FMT_I4;
    case GL_INTENSITY8:
        return FMT_I8;
    default:
        return FMT_NONE;
    }
}

uint32_t gl_get_format_element_count(GLenum format)
{
    switch (format) {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
        return 1;
    case GL_LUMINANCE_ALPHA:
        return 2;
    case GL_RGB:
        return 3;
    case GL_RGBA:
        return 4;
    case GL_COLOR_INDEX:
        assertf(0, "Color index format is not supported!");
        return 0;
    default:
        return 0;
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
        assertf(0, "Luminance-only textures are not supported!");
        break;

    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
        assertf(0, "Alpha-only textures are not supported!");
        break;

    case GL_INTENSITY4:
        return GL_INTENSITY4;

    case GL_INTENSITY:
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

#define BYTE_SWAP_16(x) ((((x)&0xFF)<<8) | (((x)&0xFF00)>>8))
#define BYTE_SWAP_32(x) ((((x)&0xFF)<<24) | (((x)&0xFF00)<<8) | (((x)&0xFF0000)>>8) | (((x)&0xFF000000)>>24))

#define COND_BYTE_SWAP_16(x, c) ((c) ? BYTE_SWAP_16(x) : (x))
#define COND_BYTE_SWAP_32(x, c) ((c) ? BYTE_SWAP_32(x) : (x))

void gl_unpack_pixel_byte(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = I8_TO_FLOAT(((const GLbyte*)data)[i]);
    }
}

void gl_unpack_pixel_ubyte(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = U8_TO_FLOAT(((const GLubyte*)data)[i]);
    }
}

void gl_unpack_pixel_short(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = I16_TO_FLOAT(COND_BYTE_SWAP_16(((const GLshort*)data)[i], swap));
    }
}

void gl_unpack_pixel_ushort(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = U16_TO_FLOAT(COND_BYTE_SWAP_16(((const GLushort*)data)[i], swap));
    }
}

void gl_unpack_pixel_int(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = I32_TO_FLOAT(COND_BYTE_SWAP_32(((const GLint*)data)[i], swap));
    }
}

void gl_unpack_pixel_uint(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = U32_TO_FLOAT(COND_BYTE_SWAP_32(((const GLuint*)data)[i], swap));
    }
}

void gl_unpack_pixel_float(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    for (uint32_t i = 0; i < num_elements; i++)
    {
        result[i] = ((const GLfloat*)data)[i];
    }
}

void gl_unpack_pixel_ubyte_3_3_2(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    GLubyte value = *(const GLubyte*)data;
    result[0] = (value>>5) / (float)(0x7);
    result[1] = ((value>>2)&0x7) / (float)(0x7);
    result[2] = (value&0x3) / (float)(0x3);
}

void gl_unpack_pixel_ushort_4_4_4_4(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    GLushort value = COND_BYTE_SWAP_16(*(const GLushort*)data, swap);
    result[0] = (value>>12) / (float)(0xF);
    result[1] = ((value>>8)&0xF) / (float)(0xF);
    result[2] = ((value>>4)&0xF) / (float)(0xF);
    result[3] = (value&0xF) / (float)(0xF);
}

void gl_unpack_pixel_ushort_5_5_5_1(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    GLushort value = COND_BYTE_SWAP_16(*(const GLushort*)data, swap);
    result[0] = (value>>11) / (float)(0x1F);
    result[1] = ((value>>6)&0x1F) / (float)(0x1F);
    result[2] = ((value>>1)&0x1F) / (float)(0x1F);
    result[3] = value & 0x1;
}

void gl_unpack_pixel_uint_8_8_8_8(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    GLuint value = COND_BYTE_SWAP_32(*(const GLuint*)data, swap);
    result[0] = U8_TO_FLOAT((value>>24));
    result[1] = U8_TO_FLOAT((value>>16)&0xFF);
    result[2] = U8_TO_FLOAT((value>>8)&0xFF);
    result[3] = U8_TO_FLOAT(value&0xFF);
}

void gl_unpack_pixel_uint_10_10_10_2(GLfloat *result, uint32_t num_elements, bool swap, const GLvoid *data)
{
    GLuint value = COND_BYTE_SWAP_32(*(const GLuint*)data, swap);
    result[0] = (value>>22) / (float)(0x3FF);
    result[1] = ((value>>12)&0x3FF) / (float)(0x3FF);
    result[2] = ((value>>2)&0x3FF) / (float)(0x3FF);
    result[3] = (value & 0x3) / (float)(0x3);
}

void gl_pack_pixel_rgb5a1(GLvoid *dest, uint32_t x, const GLfloat *components)
{
    *((GLushort*)dest) = ((GLushort)roundf(components[0]*0x1F) << 11) |
                         ((GLushort)roundf(components[1]*0x1F) << 6)  |
                         ((GLushort)roundf(components[2]*0x1F) << 1)  |
                         ((GLushort)roundf(components[3]));
}

void gl_pack_pixel_rgba8(GLvoid *dest, uint32_t x, const GLfloat *components)
{
    *((GLuint*)dest) = ((GLuint)roundf(components[0]*0xFF) << 24) |
                       ((GLuint)roundf(components[1]*0xFF) << 16) |
                       ((GLuint)roundf(components[2]*0xFF) << 8)  |
                       ((GLuint)roundf(components[3]*0xFF));
}

void gl_pack_pixel_luminance4_alpha4(GLvoid *dest, uint32_t x, const GLfloat *components)
{
    *((GLubyte*)dest) = ((GLubyte)roundf(components[0]*0xF) << 4) |
                        ((GLubyte)roundf(components[3]*0xF));
}

void gl_pack_pixel_luminance8_alpha8(GLvoid *dest, uint32_t x, const GLfloat *components)
{
    *((GLushort*)dest) = ((GLushort)roundf(components[0]*0xFF) << 8) |
                         ((GLushort)roundf(components[3]*0xFF));
}

void gl_pack_pixel_intensity4(GLvoid *dest, uint32_t x, const GLfloat *components)
{
    GLubyte c = (GLubyte)roundf(components[0]*0xF);

    if (x & 1) {
        *((GLubyte*)dest) = (*((GLubyte*)dest) & 0xF0) | c;
    } else {
        *((GLubyte*)dest) = (*((GLubyte*)dest) & 0xF) | (c << 4);
    }
}

void gl_pack_pixel_intensity8(GLvoid *dest, uint32_t x, const GLfloat *components)
{
    *((GLubyte*)dest) = (GLubyte)roundf(components[0]*0xFF);
}

bool gl_do_formats_match(GLint dst_fmt, GLenum src_fmt, GLenum src_type)
{
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
    case GL_LUMINANCE8_ALPHA8:
        if (src_fmt == GL_LUMINANCE_ALPHA && (src_type == GL_UNSIGNED_BYTE || src_type == GL_BYTE)) {
            return true;
        }
        break;
    case GL_INTENSITY8:
        if ((src_fmt == GL_LUMINANCE || src_fmt == GL_INTENSITY || src_fmt == GL_RED) && (src_type == GL_UNSIGNED_BYTE || src_type == GL_BYTE)) {
            return true;
        }
        break;
    }

    return false;
}

void gl_transfer_pixels(GLvoid *dest, GLenum dest_format, GLsizei dest_stride, GLsizei width, GLsizei height, uint32_t num_elements, GLenum format, GLenum type, uint32_t xoffset, const GLvoid *data)
{
    uint32_t src_pixel_size;
    void (*unpack_func)(GLfloat*,uint32_t,bool,const GLvoid*);
    void (*pack_func)(GLvoid*,uint32_t,const GLfloat*);

    switch (type) {
    case GL_BYTE:
        src_pixel_size = sizeof(GLbyte) * num_elements;
        unpack_func = gl_unpack_pixel_byte;
        break;
    case GL_UNSIGNED_BYTE:
        src_pixel_size = sizeof(GLubyte) * num_elements;
        unpack_func = gl_unpack_pixel_ubyte;
        break;
    case GL_SHORT:
        src_pixel_size = sizeof(GLshort) * num_elements;
        unpack_func = gl_unpack_pixel_short;
        break;
    case GL_UNSIGNED_SHORT:
        src_pixel_size = sizeof(GLushort) * num_elements;
        unpack_func = gl_unpack_pixel_ushort;
        break;
    case GL_INT:
        src_pixel_size = sizeof(GLint) * num_elements;
        unpack_func = gl_unpack_pixel_int;
        break;
    case GL_UNSIGNED_INT:
        src_pixel_size = sizeof(GLuint) * num_elements;
        unpack_func = gl_unpack_pixel_uint;
        break;
    case GL_FLOAT:
        src_pixel_size = sizeof(GLfloat) * num_elements;
        unpack_func = gl_unpack_pixel_float;
        break;
    case GL_UNSIGNED_BYTE_3_3_2_EXT:
        src_pixel_size = sizeof(GLubyte);
        unpack_func = gl_unpack_pixel_ubyte_3_3_2;
        break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
        src_pixel_size = sizeof(GLushort);
        unpack_func = gl_unpack_pixel_ushort_4_4_4_4;
        break;
    case GL_UNSIGNED_SHORT_5_5_5_1_EXT:
        src_pixel_size = sizeof(GLushort);
        unpack_func = gl_unpack_pixel_ushort_5_5_5_1;
        break;
    case GL_UNSIGNED_INT_8_8_8_8_EXT:
        src_pixel_size = sizeof(GLuint);
        unpack_func = gl_unpack_pixel_uint_8_8_8_8;
        break;
    case GL_UNSIGNED_INT_10_10_10_2_EXT:
        src_pixel_size = sizeof(GLuint);
        unpack_func = gl_unpack_pixel_uint_10_10_10_2;
        break;
    default:
        assertf(0, "Invalid type");
    }

    switch (dest_format) {
    case GL_RGB5_A1:
        pack_func = gl_pack_pixel_rgb5a1;
        break;
    case GL_RGBA8:
        pack_func = gl_pack_pixel_rgba8;
        break;
    case GL_LUMINANCE4_ALPHA4:
        pack_func = gl_pack_pixel_luminance4_alpha4;
        break;
    case GL_LUMINANCE8_ALPHA8:
        pack_func = gl_pack_pixel_luminance8_alpha8;
        break;
    case GL_INTENSITY4:
        pack_func = gl_pack_pixel_intensity4;
        break;
    case GL_INTENSITY8:
        pack_func = gl_pack_pixel_intensity8;
        break;
    default:
        assertf(0, "Unsupported destination format!");
    }

    tex_format_t dest_tex_fmt = gl_get_texture_format(dest_format);

    uint32_t row_length = state.unpack_row_length > 0 ? state.unpack_row_length : width;

    uint32_t src_stride = ROUND_UP(row_length * src_pixel_size, state.unpack_alignment);

    const GLvoid *src_ptr = data + src_stride * state.unpack_skip_rows + src_pixel_size * state.unpack_skip_pixels;
    GLvoid *dest_ptr = dest;

    uint32_t component_offset = 0;
    switch (format) {
    case GL_GREEN:
        component_offset = 1;
        break;
    case GL_BLUE:
        component_offset = 2;
        break;
    case GL_ALPHA:
        component_offset = 3;
        break;
    }

    bool formats_match = gl_do_formats_match(dest_format, format, type);
    bool can_mempcy = formats_match && state.transfer_is_noop;

    for (uint32_t r = 0; r < height; r++)
    {
        if (can_mempcy) {
            memcpy(dest_ptr + TEX_FORMAT_PIX2BYTES(dest_tex_fmt, xoffset), src_ptr, TEX_FORMAT_PIX2BYTES(dest_tex_fmt, width));
        } else {
            for (uint32_t c = 0; c < width; c++)
            {
                GLfloat components[4] = { 0, 0, 0, 1 };
                unpack_func(&components[component_offset], num_elements, state.unpack_swap_bytes, src_ptr + c * src_pixel_size);

                if (format == GL_LUMINANCE) {
                    components[2] = components[1] = components[0];
                } else if (format == GL_LUMINANCE_ALPHA) {
                    components[3] = components[1];
                    components[2] = components[1] = components[0];
                }

                for (uint32_t i = 0; i < 4; i++)
                {
                    components[i] = CLAMP01(components[i] * state.transfer_scale[i] + state.transfer_bias[i]);
                }
                
                if (state.map_color) {
                    for (uint32_t i = 0; i < 4; i++)
                    {
                        uint32_t index = floorf(components[i]) * (state.pixel_maps[i].size - 1);
                        components[i] = CLAMP01(state.pixel_maps[i].entries[index]);
                    }
                }

                uint32_t x = xoffset + c;
                pack_func(dest_ptr + TEX_FORMAT_PIX2BYTES(dest_tex_fmt, x), x, components);
            }
        }

        src_ptr += src_stride;
        dest_ptr += dest_stride;
    }
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

gl_texture_image_t * gl_get_texture_image(gl_texture_object_t *obj, GLint level)
{
    if (level < 0 || level > MAX_TEXTURE_LEVELS) {
        gl_set_error(GL_INVALID_VALUE);
        return NULL;
    }

    return &obj->levels[level];
}

bool gl_get_texture_object_and_image(GLenum target, GLint level, gl_texture_object_t **obj, gl_texture_image_t **image)
{
    gl_texture_object_t *tmp_obj = gl_get_texture_object(target);
    if (tmp_obj == NULL) {
        return false;
    }

    gl_texture_image_t *tmp_img = gl_get_texture_image(tmp_obj, level);
    if (tmp_img == NULL) {
        return false;
    }

    if (obj != NULL) {
        *obj = tmp_obj;
    }

    if (image != NULL) {
        *image = tmp_img;
    }

    return true;
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

uint32_t gl_texture_get_offset(GLenum target)
{
    switch (target) {
    case GL_TEXTURE_1D:
        return offsetof(gl_server_state_t, bound_textures) + sizeof(gl_texture_object_t) * 0;
    case GL_TEXTURE_2D:
        return offsetof(gl_server_state_t, bound_textures) + sizeof(gl_texture_object_t) * 1;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return 0;
    }
}

/*bool gl_get_texture_completeness(const gl_texture_object_t *texture, uint8_t *num_levels)
{
    const gl_texture_image_t *first_level = &texture->levels[0];

    if (first_level->width == 0 || first_level->height == 0) {
        *num_levels = 0;
        return false;
    }

    if ((texture->min_filter & TEXTURE_MIPMAP_MASK) == 0) {
        // Mip mapping is disabled
        *num_levels = 1;
        return true;
    }

    GLenum format = first_level->internal_format;

    uint32_t cur_width = first_level->width;
    uint32_t cur_height = first_level->height;

    for (uint8_t i = 0; i < MAX_TEXTURE_LEVELS; i++)
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
            if (cur_width & 0x1) break;
            cur_width >>= 1;
        }

        if (cur_height > 1) {
            if (cur_height & 0x1) break;
            cur_height >>= 1;
        }
    }

    *num_levels = 0;
    return false;
}

void gl_update_texture_completeness(gl_texture_object_t *texture)
{
    uint8_t num_levels;
    uint32_t is_complete = gl_get_texture_completeness(texture, &num_levels) ? TEX_FLAG_COMPLETE : 0;

    texture->flags &= ~(TEX_FLAG_COMPLETE | 0x7);
    texture->flags |= is_complete | num_levels;
}*/

/*uint32_t add_tmem_size(uint32_t current, uint32_t size)
{
    return ROUND_UP(current + size, 8);
}

bool gl_texture_fits_tmem(gl_texture_object_t *texture, uint32_t additional_size)
{
    uint32_t size = 0;
    uint8_t num_levels = gl_tex_get_levels(texture);
    for (uint32_t i = 0; i < num_levels; i++)
    {
        size = add_tmem_size(size, texture->levels[i].stride * texture->levels[i].height);
    }
    
    size = add_tmem_size(size, additional_size);

    return size <= 0x1000;
}*/

bool gl_validate_upload_image(GLenum format, GLenum type, uint32_t *num_elements)
{
    *num_elements = gl_get_format_element_count(format);
    if (*num_elements == 0) {
        gl_set_error(GL_INVALID_ENUM);
        return false;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        break;
    case GL_UNSIGNED_BYTE_3_3_2_EXT:
        if (*num_elements != 3) {
            gl_set_error(GL_INVALID_OPERATION);
            return false;
        }
        break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
    case GL_UNSIGNED_SHORT_5_5_5_1_EXT:
    case GL_UNSIGNED_INT_8_8_8_8_EXT:
    case GL_UNSIGNED_INT_10_10_10_2_EXT:
        if (*num_elements != 4) {
            gl_set_error(GL_INVALID_OPERATION);
            return false;
        }
        break;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return false;
    }

    return true;
}


void gl_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    assertf(border == 0, "Texture border is not supported!");

    GLsizei width_without_border = width - 2 * border;
    GLsizei height_without_border = height - 2 * border;

    // Check for power of two
    if ((width_without_border & (width_without_border - 1)) || 
        (height_without_border & (height_without_border - 1))) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    GLint preferred_format = gl_choose_internalformat(internalformat);
    if (preferred_format < 0) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    uint32_t num_elements;
    if (!gl_validate_upload_image(format, type, &num_elements)) {
        return;
    }

    uint32_t rdp_format = gl_get_texture_format(preferred_format);
    uint32_t stride = MAX(TEX_FORMAT_PIX2BYTES(rdp_format, width), 8);
    uint32_t size = stride * height;

    // TODO: How to validate this?
    //if (!gl_texture_fits_tmem(obj, size)) {
    //    gl_set_error(GL_INVALID_VALUE);
    //    return;
    //}

    GLvoid *new_buffer = malloc_uncached(size);
    if (new_buffer == NULL) {
        gl_set_error(GL_OUT_OF_MEMORY);
        return;
    }

    if (data != NULL) {
        gl_transfer_pixels(new_buffer, preferred_format, stride, width, height, num_elements, format, type, 0, data);
    }

    uint32_t offset = gl_texture_get_offset(target);
    uint32_t img_offset = offset + level * sizeof(gl_texture_image_t);
    
    uint64_t *deletion_slot = gl_reserve_deletion_slot();
    gl_get_value(deletion_slot, img_offset + offsetof(gl_texture_image_t, tex_image), sizeof(uint64_t));

    uint8_t width_log = gl_log2(width);
    uint8_t height_log = gl_log2(height);

    tex_format_t load_fmt = rdp_format;

    // TODO: do this for 8-bit formats as well?
    switch (rdp_format) {
    case FMT_CI4:
    case FMT_I4:
        load_fmt = FMT_RGBA16;
        break;
    default:
        break;
    }

    uint16_t load_width = TEX_FORMAT_BYTES2PIX(load_fmt, stride);
    uint16_t num_texels = load_width * height;
    uint16_t words = stride / 8;
    uint16_t dxt = (2048 + words - 1) / words;
    uint16_t tmem_size = (stride * height) / 8;

    uint32_t tex_image = ((0xC0 + RDPQ_CMD_SET_TEXTURE_IMAGE) << 24) | (load_fmt << 19);
    uint32_t set_load_tile = ((0xC0 + RDPQ_CMD_SET_TILE) << 24) | (load_fmt << 19);
    uint32_t load_block = (LOAD_TILE << 24) | ((num_texels-1) << 12) | dxt;
    uint32_t set_tile = ((0xC0 + RDPQ_CMD_SET_TILE) << 24) | (rdp_format << 19) | ((stride/8) << 9);

    // TODO: do this in one command?
    gl_set_long(GL_UPDATE_NONE, img_offset + offsetof(gl_texture_image_t, tex_image), ((uint64_t)tex_image << 32) | PhysicalAddr(new_buffer));
    gl_set_long(GL_UPDATE_NONE, img_offset + offsetof(gl_texture_image_t, set_load_tile), ((uint64_t)set_load_tile << 32) | load_block);
    gl_set_long(GL_UPDATE_NONE, img_offset + offsetof(gl_texture_image_t, set_tile), ((uint64_t)set_tile << 32) | ((uint64_t)width << 16) | height);
    gl_set_long(GL_UPDATE_NONE, img_offset + offsetof(gl_texture_image_t, stride), ((uint64_t)stride << 48) | ((uint64_t)preferred_format << 32) | ((uint64_t)tmem_size << 16) | ((uint64_t)width_log << 8) | height_log);

    gl_set_flag_raw(GL_UPDATE_NONE, offset + TEXTURE_FLAGS_OFFSET, TEX_FLAG_UPLOAD_DIRTY, true);

    gl_update_texture_completeness(offset);
}

void gl_tex_sub_image(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data)
{
    assertf(0, "glTexSubImage* is temporarily unsupported. Please check again later!");

    // TODO: can't access the image here!
    gl_texture_object_t *obj;
    gl_texture_image_t *image;

    if (!gl_get_texture_object_and_image(target, level, &obj, &image)) {
        return;
    }

    if (image->data == NULL) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    uint32_t num_elements;
    if (!gl_validate_upload_image(format, type, &num_elements)) {
        return;
    }

    GLvoid *dest = image->data + yoffset * image->stride;

    if (data != NULL) {
        gl_transfer_pixels(dest, image->internal_format, image->stride, width, height, num_elements, format, type, xoffset, data);
        obj->flags |= TEX_FLAG_UPLOAD_DIRTY;
    }
}

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    switch (target) {
    case GL_TEXTURE_1D:
        break;
    case GL_PROXY_TEXTURE_1D:
        assertf(0, "Proxy texture targets are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_tex_image(target, level, internalformat, width, 1, border, format, type, data);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    switch (target) {
    case GL_TEXTURE_2D:
        break;
    case GL_PROXY_TEXTURE_2D:
        assertf(0, "Proxy texture targets are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_tex_image(target, level, internalformat, width, height, border, format, type, data);
}

void glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *data)
{
    if (target != GL_TEXTURE_1D) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_tex_sub_image(target, level, xoffset, 0, width, 1, format, type, data);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data)
{
    if (target != GL_TEXTURE_2D) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_tex_sub_image(target, level, xoffset, yoffset, width, height, format, type, data);
}

// TODO: should CopyTex[Sub]Image be supported?
/*
void gl_get_fb_data_for_copy(GLint x, GLint y, GLenum *format, GLenum *type, uint32_t *stride, const GLvoid **ptr)
{
    const surface_t *fb_surface = state.cur_framebuffer->color_buffer;

    tex_format_t src_format = surface_get_format(fb_surface);
    uint32_t pixel_size = TEX_FORMAT_BYTES_PER_PIXEL(src_format);

    switch (src_format) {
    case FMT_RGBA16:
        *format = GL_RGBA;
        *type = GL_UNSIGNED_SHORT_5_5_5_1_EXT;
        break;
    case FMT_RGBA32:
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        break;
    case FMT_IA16:
        *format = GL_LUMINANCE_ALPHA;
        *type = GL_UNSIGNED_BYTE;
        break;
    case FMT_I8:
        *format = GL_LUMINANCE;
        *type = GL_UNSIGNED_BYTE;
        break;
    default:
        assertf(0, "Unsupported framebuffer format!");
        return;
    }

    // TODO: validate rectangle
    // TODO: from bottom left corner?
    *ptr = fb_surface->buffer + y * fb_surface->stride + x * pixel_size;
    *stride = fb_surface->stride;
}

void gl_copy_tex_image(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    GLenum format, type;
    const GLvoid *ptr;
    uint32_t stride;
    gl_get_fb_data_for_copy(x, y, &format, &type, &ptr, &stride);
    rspq_wait();
    gl_tex_image(target, level, internalformat, width, height, border, format, type, ptr, stride);
}

void gl_copy_tex_sub_image(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLenum format, type;
    const GLvoid *ptr;
    uint32_t stride;
    gl_get_fb_data_for_copy(x, y, &format, &type, &ptr, &stride);
    rspq_wait();
    gl_tex_sub_image(target, level, xoffset, yoffset, width, height, format, type, ptr, );
}

void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
    if (target != GL_TEXTURE_1D) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_copy_tex_image(target, level, internalformat, x, y, width, 1, border);
}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    if (target != GL_TEXTURE_2D) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_copy_tex_image(target, level, internalformat, x, y, width, height, border);
}

void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLint width)
{
    if (target != GL_TEXTURE_1D) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (target != GL_TEXTURE_2D) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
*/

void gl_texture_set_wrap_s(uint32_t offset, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
        gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_texture_object_t, wrap_s), (uint16_t)param);
        gl_set_flag_raw(GL_UPDATE_NONE, offset + TEXTURE_FLAGS_OFFSET, TEX_FLAG_UPLOAD_DIRTY, true);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_wrap_t(uint32_t offset, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
        gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_texture_object_t, wrap_t), (uint16_t)param);
        gl_set_flag_raw(GL_UPDATE_NONE, offset + TEXTURE_FLAGS_OFFSET, TEX_FLAG_UPLOAD_DIRTY, true);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_min_filter(uint32_t offset, GLenum param)
{
    switch (param) {
    case GL_NEAREST:
    case GL_LINEAR:
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_LINEAR:
        gl_set_short(GL_UPDATE_TEXTURE, offset + offsetof(gl_texture_object_t, min_filter), (uint16_t)param);
        gl_update_texture_completeness(offset);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_mag_filter(uint32_t offset, GLenum param)
{
    switch (param) {
    case GL_NEAREST:
    case GL_LINEAR:
        gl_set_short(GL_UPDATE_TEXTURE, offset + offsetof(gl_texture_object_t, mag_filter), (uint16_t)param);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void gl_texture_set_priority(uint32_t offset, GLint param)
{
    gl_set_word(GL_UPDATE_NONE, offset + offsetof(gl_texture_object_t, priority), param);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(offset, param);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(offset, param);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(offset, param);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, param);
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(offset, param);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(offset, param);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(offset, param);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(offset, param);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, param);
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(offset, CLAMPF_TO_I32(param));
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(offset, params[0]);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(offset, params[0]);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(offset, params[0]);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, params[0]);
        break;
    case GL_TEXTURE_BORDER_COLOR:
        assertf(0, "Texture border color is not supported!");
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(offset, I32_TO_FLOAT(params[0]));
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(offset, params[0]);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(offset, params[0]);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(offset, params[0]);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, params[0]);
        break;
    case GL_TEXTURE_BORDER_COLOR:
        assertf(0, "Texture border color is not supported!");
        break;
    case GL_TEXTURE_PRIORITY:
        gl_texture_set_priority(offset, params[0]);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

GLboolean glIsTexture(GLuint texture)
{
    // FIXME: This doesn't actually guarantee that it's a valid texture object, but just uses the heuristic of
    //        "is it somewhere in the heap memory?". This way we can at least rule out arbitrarily chosen integer constants,
    //        which used to be valid texture IDs in legacy OpenGL.
    return is_valid_object_id(texture);
}

void glBindTexture(GLenum target, GLuint texture)
{
    assertf(texture == 0 || is_valid_object_id(texture), "Not a valid texture object: %#lx", texture);

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
            *target_obj = &state.default_textures[0];
            break;
        case GL_TEXTURE_2D:
            *target_obj = &state.default_textures[1];
            break;
        }
    } else {
        gl_texture_object_t *obj = (gl_texture_object_t*)texture;

        // TODO: Is syncing the dimensionality required? It always gets set before the texture is ever bound
        //       and is never modified on RSP.
        if (obj->dimensionality == 0) {
            obj->dimensionality = target;
        }

        if (obj->dimensionality != target) {
            gl_set_error(GL_INVALID_OPERATION);
            return;
        }

        *target_obj = obj;
    }

    gl_bind_texture(target, *target_obj);
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    for (uint32_t i = 0; i < n; i++)
    {
        gl_texture_object_t *new_object = malloc_uncached(sizeof(gl_texture_object_t));
        gl_init_texture_object(new_object);
        textures[i] = (GLuint)new_object;
    }
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
    for (uint32_t i = 0; i < n; i++)
    {
        assertf(textures[i] == 0 || is_valid_object_id(textures[i]), "Not a valid texture object: %#lx", textures[i]);

        gl_texture_object_t *obj = (gl_texture_object_t*)textures[i];
        if (obj == NULL) {
            continue;
        }

        // TODO: Unbind properly (on RSP too)

        if (obj == state.texture_1d_object) {
            state.texture_1d_object = &state.default_textures[0];
        } else if (obj == state.texture_2d_object) {
            state.texture_2d_object = &state.default_textures[1];
        }

        gl_cleanup_texture_object(obj);
        free_uncached(obj);
    }
}
/*
void gl_upload_texture(gl_texture_object_t *tex_obj)
{
    // TODO: re-implement this so that multiple textures can potentially be in TMEM at the same time
    // TODO: seperate uploading from updating tile descriptors

    uint32_t tmem_used = 0;

    // All levels must have the same format to be complete
    tex_format_t fmt = gl_get_texture_format(tex_obj->levels[0].internal_format);
    tex_format_t load_fmt = fmt;

    // TODO: do this for 8-bit formats as well
    switch (fmt) {
    case FMT_CI4:
    case FMT_I4:
        load_fmt = FMT_RGBA16;
        break;
    default:
        break;
    }

    int32_t full_width_log = gl_log2(tex_obj->levels[0].width);
    int32_t full_height_log = gl_log2(tex_obj->levels[0].height);

    uint8_t num_levels = gl_tex_get_levels(tex_obj);

    for (uint8_t l = 0; l < num_levels; l++)
    {
        gl_texture_image_t *image = &tex_obj->levels[l];

        uint32_t tmem_pitch = image->stride;
        uint32_t load_width = TEX_FORMAT_BYTES2PIX(load_fmt, tmem_pitch);
        uint32_t num_load_texels = load_width * image->height;
        uint32_t tmem_size = tmem_pitch * image->height;

        rdpq_set_texture_image_raw(0, PhysicalAddr(image->data), load_fmt, 0, 0);
        rdpq_set_tile(LOAD_TILE, load_fmt, tmem_used, 0, 0); // 4
        rdpq_load_block(LOAD_TILE, 0, 0, num_load_texels, tmem_pitch); // 4

        // Levels need to halve in size every time to be complete
        int32_t width_log = MAX(full_width_log - l, 0);
        int32_t height_log = MAX(full_height_log - l, 0);

        uint8_t mask_s = tex_obj->wrap_s == GL_REPEAT ? width_log : 0;
        uint8_t mask_t = tex_obj->wrap_t == GL_REPEAT ? height_log : 0;

        uint8_t shift_s = full_width_log - width_log;
        uint8_t shift_t = full_height_log - height_log;

        rdpq_set_tile_full(l, fmt, tmem_used, tmem_pitch, 0, 0, 0, mask_t, shift_t, 0, 0, mask_s, shift_s); // 4
        rdpq_set_tile_size(l, 0, 0, image->width, image->height);

        tmem_used += tmem_size;
    }
}

void gl_update_texture()
{
    gl_texture_object_t *tex_obj = gl_get_active_texture();
    if (tex_obj == NULL || !gl_tex_is_complete(tex_obj)) {
        tex_obj = NULL;
    }

    bool is_applied = tex_obj != NULL;

    if (is_applied && (tex_obj != state.uploaded_texture || (tex_obj->flags & TEX_FLAG_UPLOAD_DIRTY))) {
        gl_upload_texture(tex_obj);

        tex_obj->flags &= ~TEX_FLAG_UPLOAD_DIRTY;
        state.uploaded_texture = tex_obj;
    }
}
*/