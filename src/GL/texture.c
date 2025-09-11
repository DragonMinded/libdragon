#include "gl_internal.h"
#include "rdpq.h"
#include "rdpq_mode.h"
#include "rdpq_tex.h"
#include "rdpq_sprite.h"
#include "sprite.h"
#include "debug.h"
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

static inline void texture_get_texparms(gl_texture_object_t *obj, GLint level, rdpq_texparms_t *parms);

void gl_texture_set_min_filter(gl_texture_object_t *obj, GLenum param);

void gl_init_texture_object(gl_texture_object_t *obj)
{
    obj->min_filter = GL_NEAREST_MIPMAP_LINEAR;
    obj->mag_filter = GL_LINEAR;
    obj->wrap_s = GL_REPEAT;
    obj->wrap_t = GL_REPEAT;
}

void surface_free_safe(surface_t *surface)
{
    if (surface_has_owned_buffer(surface)) {
        rdpq_call_deferred(free_uncached, surface->buffer);
    }
    memset(surface, 0, sizeof(surface_t));
}

void texture_image_free_safe(gl_texture_object_t *obj, uint32_t level)
{
    surface_free_safe(&obj->levels[level].surface);
}

void gl_cleanup_texture_object(gl_texture_object_t *obj)
{
    for (uint32_t i = 0; i < MAX_TEXTURE_LEVELS; i++)
    {
        surface_free(&obj->levels[i].surface);
    }

    if (obj->upload_block != NULL) {
        rspq_block_free(obj->upload_block);
    }
}

void gl_texture_init()
{
    ringbuffer_init(&state->texturing_buffer, sizeof(mgfx_texturing_t), 32);

    // TODO: lazy-init default texture objects
    state->default_textures = calloc(2, sizeof(gl_texture_object_t));

    gl_init_texture_object(&state->default_textures[0]);
    gl_init_texture_object(&state->default_textures[1]);

    state->default_textures[0].dimensionality = GL_TEXTURE_1D;
    state->default_textures[1].dimensionality = GL_TEXTURE_2D;

    state->default_textures[0].flags |= TEX_IS_DEFAULT;
    state->default_textures[1].flags |= TEX_IS_DEFAULT;

    state->texture_1d_object = &state->default_textures[0];
    state->texture_2d_object = &state->default_textures[1];
}

void gl_texture_close()
{
    gl_cleanup_texture_object(&state->default_textures[0]);
    gl_cleanup_texture_object(&state->default_textures[1]);

    free(state->default_textures);

    ringbuffer_free(&state->texturing_buffer);
}

uint32_t gl_log2(uint32_t s)
{
    uint32_t log = 0;
    while (s >>= 1) ++log;
    return log;
}

tex_format_t gl_tex_format_to_rdp(GLenum format)
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

GLenum rdp_tex_format_to_gl(tex_format_t format)
{
    switch (format) {
    case FMT_RGBA16:
        return GL_RGB5_A1;
    case FMT_RGBA32:
        return GL_RGBA8;
    case FMT_IA8:
        return GL_LUMINANCE4_ALPHA4;
    case FMT_IA16:
        return GL_LUMINANCE8_ALPHA8;
    case FMT_I4:
        return GL_INTENSITY4;
    case FMT_I8:
        return GL_INTENSITY8;
    default:
        return 0;
    }
}

gl_texture_object_t *gl_get_active_texture()
{
    if (gl_is_enabled(ENABLE_TEXTURE_2D)) {
        return state->texture_2d_object;
    }

    if (gl_is_enabled(ENABLE_TEXTURE_1D)) {
        return state->texture_1d_object;
    }

    return NULL;
}

gl_texture_object_t * gl_get_texture_object(GLenum target)
{
    switch (target) {
    case GL_TEXTURE_1D:
        return state->texture_1d_object;
    case GL_TEXTURE_2D:
        return state->texture_2d_object;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid texture target", target);
        return NULL;
    }
}

void set_texture_state(gl_texture_object_t *obj, gl_state_id_t state_id)
{
    if (obj == state->active_texture) {
        gl_set_state(state_id);
    }
}

void set_block_dirty(gl_texture_object_t *obj)
{
    // Sprite textures completely dictate all settings automatically
    if (texture_is_sprite(obj)) return;

    obj->flags |= TEX_IS_BLOCK_DIRTY;
    set_texture_state(obj, STATE_TEXTURE_BLOCK);
}

static void set_upload_block(gl_texture_object_t *obj, rspq_block_t *block)
{
    if (obj->upload_block != NULL) {
        rspq_call_deferred((void(*)(void*))obj->upload_block, rspq_block_free);
    }

    obj->upload_block = block;
    obj->flags &= ~TEX_IS_BLOCK_DIRTY;
}

bool get_is_texture_complete(gl_texture_object_t *obj, uint32_t *levels_count)
{
    surface_t *surf0 = &obj->levels[0].surface;
    uint16_t width = surf0->width;
    uint16_t height = surf0->height;
    tex_format_t format = surface_get_format(surf0);

    *levels_count = 0;
    if (width == 0 || height == 0)
        return false;

    if (!texture_has_mipmaps(obj)) {
        *levels_count = 1;
        return true;
    }

    for (size_t i = 1; i < MAX_TEXTURE_LEVELS; i++)
    {
        if (width > 1)
            width >>= 1;
        if (height > 1)
            height >>= 1;
        
        surface_t *surface = &obj->levels[i].surface;
        
        if (surface->width != width || 
            surface->height != height || 
            surface_get_format(surface) != format)
        {
            return false;
        }
        
        if (width == 1 && height == 1) {
            *levels_count = i + 1;
            return true;
        }
    }

    return false;
}

void get_texture_size(gl_texture_object_t *obj, uint16_t *width, uint16_t *height)
{
    if (texture_is_sprite(obj)) {
        *width = obj->sprite->width;
        *height = obj->sprite->height;
    } else {
        *width = obj->levels[0].surface.width;
        *height = obj->levels[0].surface.height;
    }
}

void gl_update_texture_completeness(gl_texture_object_t *obj)
{
    bool was_complete = texture_is_complete(obj);
    bool is_complete = get_is_texture_complete(obj, &obj->levels_count);

    if (is_complete) {
        obj->flags |= TEX_IS_COMPLETE;
    } else {
        obj->flags &= ~TEX_IS_COMPLETE;
    }

    set_block_dirty(obj);
    if (is_complete != was_complete) {
        gl_set_state(STATE_TEXTURE_COMPLETE);
    }
}

void glSpriteTextureN64(GLenum target, sprite_t *sprite, rdpq_texparms_t *texparms)
{
    gl_assert_no_display_list();
    if (!gl_ensure_no_begin_end()) return;

    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (texture_is_default(obj)) {
        gl_set_error(GL_INVALID_OPERATION, "Cannot assign sprite to a default texture");
        return;
    }

    if (target == GL_TEXTURE_1D && sprite->height != 1) {
        gl_set_error(GL_INVALID_VALUE, "Sprite must have height 1 when using target GL_TEXTURE_1D");
        return;
    }

    for (uint32_t i = 0; i < MAX_TEXTURE_LEVELS; i++)
    {
        texture_image_free_safe(obj, i);
    }

    // Mark texture as complete because sprites are complete by definition
    obj->flags |= TEX_HAS_IMAGE | TEX_IS_COMPLETE;
    obj->sprite = sprite;

    // Set min filter (for query only)
    GLenum min_filter = sprite_get_lod_count(sprite) > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    gl_texture_set_min_filter(obj, min_filter);

    rspq_block_begin();
    rdpq_sprite_upload(TILE0, sprite, texparms);
    set_upload_block(obj, rspq_block_end());
    set_texture_state(obj, STATE_TEXTURE_SIZE);
    set_texture_state(obj, STATE_TEXTURE_BLOCK);
}

void on_image_assigned(gl_texture_object_t *obj, GLint level)
{
    obj->flags |= TEX_HAS_IMAGE;
    gl_update_texture_completeness(obj);

    if (level == 0) {
        set_texture_state(obj, STATE_TEXTURE_SIZE);
    }
    set_texture_state(obj, STATE_TEXTURE_BLOCK);
}

void glSurfaceTexImageN64(GLenum target, GLint level, surface_t *surface, rdpq_texparms_t *texparms)
{
    tex_format_t fmt = surface_get_format(surface);
    assertf(fmt != FMT_CI4 && fmt != FMT_CI8, "CI textures are not supported by glSurfaceTexImageN64 yet");

    gl_assert_no_display_list();
    if (!gl_ensure_no_begin_end()) return;
    
    if (level >= MAX_TEXTURE_LEVELS || level < 0) {
        gl_set_error(GL_INVALID_VALUE, "Invalid level number (must be in [0, %d])", MAX_TEXTURE_LEVELS-1);
        return;
    }

    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (texture_is_sprite(obj)) {
        gl_set_error(GL_INVALID_OPERATION, "Cannot apply image to a sprite texture");
        return;
    }

    if (target == GL_TEXTURE_1D && surface->height != 1) {
        gl_set_error(GL_INVALID_VALUE, "Surface must have height 1 when using target GL_TEXTURE_1D");
        return;
    }

    rdpq_texparms_t *parms = &obj->levels[level].parms;
    if (texparms != NULL) {
        *parms = *texparms;
        parms->s.scale_log = level;
        parms->t.scale_log = level;
    } else {
        texture_get_texparms(obj, level, parms);
    }

    texture_image_free_safe(obj, level);

    // Store the surface. We duplicate the surface structure (not the pixels)
    // using surface_make_sub so that we get a variant in which the owned bit
    // is not set; this in turns will make sure texture deletion would not free
    // the original surface (whose lifetime is left to the caller).
    obj->levels[level].surface = surface_make_sub(surface, 0, 0, surface->width, surface->height);

    on_image_assigned(obj, level);
}

void gl_texture_set_wrap_s(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
    case GL_MIRRORED_REPEAT_ARB:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid wrapping mode", param);
        return;
    }

    if (texture_has_image(obj)) {
        gl_set_error(GL_INVALID_OPERATION, "Cannot set wrapping mode on a texture that has at least one image applied");
    }

    obj->wrap_s = param;
    set_block_dirty(obj);
}

void gl_texture_set_wrap_t(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_CLAMP:
    case GL_REPEAT:
    case GL_MIRRORED_REPEAT_ARB:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid wrapping mode", param);
        return;
    }

    if (texture_has_image(obj)) {
        gl_set_error(GL_INVALID_OPERATION, "Cannot set wrapping mode on a texture that has at least one image applied");
    }

    obj->wrap_t = param;
    set_block_dirty(obj);
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
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid minification filter", param);
        return;
    }

    obj->min_filter = param;

    if (!texture_is_sprite(obj)) {
        gl_update_texture_completeness(obj);
    }

    set_texture_state(obj, STATE_TEXTURE_FILTER);
}

void gl_texture_set_mag_filter(gl_texture_object_t *obj, GLenum param)
{
    switch (param) {
    case GL_NEAREST:
    case GL_LINEAR:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid magnification filter", param);
        return;
    }

    obj->mag_filter = param;
    set_texture_state(obj, STATE_TEXTURE_FILTER);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    gl_texture_object_t *obj = gl_get_texture_object(target);

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
        // Ignored
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_texture_object_t *obj = gl_get_texture_object(target);

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
        // Ignored
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_texture_object_t *obj = gl_get_texture_object(target);

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
        assertf(0, "Texture border color is not supported!");
        break;
    case GL_TEXTURE_PRIORITY:
        // Ignored
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_texture_object_t *obj = gl_get_texture_object(target);

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
        assertf(0, "Texture border color is not supported!");
        break;
    case GL_TEXTURE_PRIORITY:
        // Ignored
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid parameter name for this function", pname);
        return;
    }
}

GLboolean glIsTexture(GLuint texture)
{
    if (!gl_ensure_no_begin_end()) return 0;
    
    // FIXME: This doesn't actually guarantee that it's a valid texture object, but just uses the heuristic of
    //        "is it somewhere in the heap memory?". This way we can at least rule out arbitrarily chosen integer constants,
    //        which used to be valid texture IDs in legacy OpenGL.
    return is_valid_object_id(texture);
}

void glBindTexture(GLenum target, GLuint texture)
{
    if (!gl_ensure_no_begin_end()) return;
    assertf(texture == 0 || is_valid_object_id(texture),
        "Not a valid texture object: %#lx. Make sure to allocate IDs via glGenTextures", texture);

    gl_texture_object_t **target_obj = NULL;

    switch (target) {
    case GL_TEXTURE_1D:
        target_obj = &state->texture_1d_object;
        break;
    case GL_TEXTURE_2D:
        target_obj = &state->texture_2d_object;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid texture target", target);
        return;
    }

    if (texture == 0) {
        switch (target) {
        case GL_TEXTURE_1D:
            *target_obj = &state->default_textures[0];
            break;
        case GL_TEXTURE_2D:
            *target_obj = &state->default_textures[1];
            break;
        }
    } else {
        gl_texture_object_t *obj = (gl_texture_object_t*)texture;

        if (obj->dimensionality == 0) {
            obj->dimensionality = target;
        }

        if (obj->dimensionality != target) {
            gl_set_error(GL_INVALID_OPERATION, "Texture object has already been bound to another texture target");
            return;
        }

        *target_obj = obj;
    }

    gl_set_state(STATE_BOUND_TEXTURES);
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    if (!gl_ensure_no_begin_end()) return;
    
    for (uint32_t i = 0; i < n; i++)
    {
        gl_texture_object_t *new_object = calloc(1, sizeof(gl_texture_object_t));
        gl_init_texture_object(new_object);
        textures[i] = (GLuint)new_object;
    }
}

void texture_free(gl_texture_object_t* obj)
{
    gl_cleanup_texture_object(obj);
    free(obj);
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
    if (!gl_ensure_no_begin_end()) return;
    
    for (uint32_t i = 0; i < n; i++)
    {
        assertf(textures[i] == 0 || is_valid_object_id(textures[i]),
            "Not a valid texture object: %#lx. Make sure to allocate IDs via glGenTextures", textures[i]);

        gl_texture_object_t *obj = (gl_texture_object_t*)textures[i];
        if (obj == NULL) {
            continue;
        }

        if (obj == state->texture_1d_object) {
            glBindTexture(GL_TEXTURE_1D, 0);
        } else if (obj == state->texture_2d_object) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        rdpq_call_deferred((void (*)(void*))texture_free, obj);
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid pixel data format", format);
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
        return -1;

    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
        assertf(0, "Alpha-only textures are not supported!");
        return -1;

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
        abort();
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
        abort();
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
        abort();
    }

    tex_format_t dest_tex_fmt = gl_tex_format_to_rdp(dest_format);

    uint32_t row_length = state->unpack_row_length > 0 ? state->unpack_row_length : width;

    uint32_t src_stride = ROUND_UP(row_length * src_pixel_size, state->unpack_alignment);

    const GLvoid *src_ptr = data + src_stride * state->unpack_skip_rows + src_pixel_size * state->unpack_skip_pixels;
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
    bool can_mempcy = formats_match && state->transfer_is_noop;

    for (uint32_t r = 0; r < height; r++)
    {
        if (can_mempcy) {
            memcpy(dest_ptr + TEX_FORMAT_PIX2BYTES(dest_tex_fmt, xoffset), src_ptr, TEX_FORMAT_PIX2BYTES(dest_tex_fmt, width));
        } else {
            for (uint32_t c = 0; c < width; c++)
            {
                GLfloat components[4] = { 0, 0, 0, 1 };
                unpack_func(&components[component_offset], num_elements, state->unpack_swap_bytes, src_ptr + c * src_pixel_size);

                if (format == GL_LUMINANCE) {
                    components[2] = components[1] = components[0];
                } else if (format == GL_LUMINANCE_ALPHA) {
                    components[3] = components[1];
                    components[2] = components[1] = components[0];
                }

                for (uint32_t i = 0; i < 4; i++)
                {
                    components[i] = CLAMP01(components[i] * state->transfer_scale[i] + state->transfer_bias[i]);
                }
                
                if (state->map_color) {
                    for (uint32_t i = 0; i < 4; i++)
                    {
                        uint32_t index = floorf(components[i]) * (state->pixel_maps[i].size - 1);
                        components[i] = CLAMP01(state->pixel_maps[i].entries[index]);
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

bool gl_validate_upload_image(GLenum format, GLenum type, uint32_t *num_elements)
{
    *num_elements = gl_get_format_element_count(format);
    if (*num_elements == 0) {
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
            gl_set_error(GL_INVALID_OPERATION, "GL_UNSIGNED_BYTE_3_3_2_EXT must be used with GL_RGB");
            return false;
        }
        break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
        if (*num_elements != 4) {
            gl_set_error(GL_INVALID_OPERATION, "GL_UNSIGNED_SHORT_4_4_4_4_EXT must be used with GL_RGBA");
            return false;
        }
    case GL_UNSIGNED_SHORT_5_5_5_1_EXT:
        if (*num_elements != 4) {
            gl_set_error(GL_INVALID_OPERATION, "GL_UNSIGNED_SHORT_5_5_5_1_EXT must be used with GL_RGBA");
            return false;
        }
    case GL_UNSIGNED_INT_8_8_8_8_EXT:
        if (*num_elements != 4) {
            gl_set_error(GL_INVALID_OPERATION, "GL_UNSIGNED_INT_8_8_8_8_EXT must be used with GL_RGBA");
            return false;
        }
    case GL_UNSIGNED_INT_10_10_10_2_EXT:
        if (*num_elements != 4) {
            gl_set_error(GL_INVALID_OPERATION, "GL_UNSIGNED_INT_10_10_10_2_EXT must be used with GL_RGBA");
            return false;
        }
        break;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid pixel data type", type);
        return false;
    }

    return true;
}

static inline float wrap_mode_to_repeats(GLenum wrap_mode)
{
    switch (wrap_mode) {
    case GL_REPEAT:
    case GL_MIRRORED_REPEAT_ARB:
        return REPEAT_INFINITE;
    case GL_CLAMP:
    default:
        return 0;
    }
}

static inline void texture_get_texparms(gl_texture_object_t *obj, GLint level, rdpq_texparms_t *parms)
{
    *parms = (rdpq_texparms_t){
        .s.scale_log = level,
        .t.scale_log = level,
        .s.mirror = obj->wrap_s == GL_MIRRORED_REPEAT_ARB,
        .t.mirror = obj->wrap_t == GL_MIRRORED_REPEAT_ARB,
        .s.repeats = wrap_mode_to_repeats(obj->wrap_s),
        .t.repeats = wrap_mode_to_repeats(obj->wrap_t),
    };
}

void gl_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    assertf(border == 0, "Texture border is not supported!");
    if (level >= MAX_TEXTURE_LEVELS || level < 0) {
        gl_set_error(GL_INVALID_VALUE, "Invalid level number (must be in [0, %d])", MAX_TEXTURE_LEVELS-1);
        return;
    }

    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (texture_is_sprite(obj)) {
        gl_set_error(GL_INVALID_OPERATION, "Cannot apply image to a sprite texture");
        return;
    }

    GLint preferred_format = gl_choose_internalformat(internalformat);
    if (preferred_format < 0) {
        gl_set_error(GL_INVALID_VALUE, "Internal format %#04lx is not supported", internalformat);
        return;
    }

    uint32_t num_elements;
    if (!gl_validate_upload_image(format, type, &num_elements)) {
        return;
    }

    texture_image_free_safe(obj, level);

    surface_t *surface = &obj->levels[level].surface;

    tex_format_t rdp_format = gl_tex_format_to_rdp(preferred_format);
    *surface = surface_alloc(rdp_format, width, height);
    if (surface->buffer == NULL) {
        gl_set_error(GL_OUT_OF_MEMORY, "Failed to allocate texture image");
        return;
    }

    if (data != NULL) {
        gl_transfer_pixels(surface->buffer, preferred_format, surface->stride, width, height, num_elements, format, type, 0, data);
    }

    texture_get_texparms(obj, level, &obj->levels[level].parms);
    on_image_assigned(obj, level);
}

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    gl_assert_no_display_list();
    if (!gl_ensure_no_begin_end()) return;
    
    switch (target) {
    case GL_TEXTURE_1D:
        break;
    case GL_PROXY_TEXTURE_1D:
        assertf(0, "Proxy texture targets are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid target for glTexImage1D", target);
        return;
    }

    gl_tex_image(target, level, internalformat, width, 1, border, format, type, data);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    gl_assert_no_display_list();
    if (!gl_ensure_no_begin_end()) return;
    
    switch (target) {
    case GL_TEXTURE_2D:
        break;
    case GL_PROXY_TEXTURE_2D:
        assertf(0, "Proxy texture targets are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid target for glTexImage2D", target);
        return;
    }

    gl_tex_image(target, level, internalformat, width, height, border, format, type, data);
}

GLboolean glAreTexturesResident(GLsizei n, const GLuint *textures, const GLboolean *residences)
{
    return GL_FALSE;
}

void glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
    // Priorities are ignored
}

void glTexSizeN64(GLushort width, GLushort height)
{
    state->rdpq_tex_width = width;
    state->rdpq_tex_height = height;
    gl_set_state(STATE_RDPQ_TEX_SIZE);
}

void update_active_texture()
{
    gl_texture_object_t *obj = gl_get_active_texture();
    state->active_texture = obj != NULL && texture_is_complete(obj) ? obj : NULL;
    gl_set_state(STATE_ACTIVE_TEXTURE);
}

bool gl_is_texture_active()
{
    return gl_is_enabled(ENABLE_RDPQ_TEXTURING) || state->active_texture != NULL;
}

rdpq_mipmap_t get_mipmap_mode(gl_texture_object_t *obj)
{
    if (!texture_has_mipmaps(obj)) {
        return MIPMAP_NONE;
    }

    return texture_is_interpolating_mipmaps(obj) ? MIPMAP_INTERPOLATE : MIPMAP_NEAREST;
}

void upload_texture(gl_texture_object_t *obj)
{
    rdpq_tex_multi_begin();
    for (size_t i = 0; i < obj->levels_count; i++)
    {
        rdpq_tex_upload(TILE0 + i, &obj->levels[i].surface, &obj->levels[i].parms);
    }
    rdpq_tex_multi_end();

    rdpq_mode_mipmap(get_mipmap_mode(obj), obj->levels_count);
    rdpq_mode_tlut(rdpq_tlut_from_format(surface_get_format(&obj->levels[0].surface)));
}

void upload_uniform(uint16_t width, uint16_t height, bool is_bilinear)
{
    // Extract offset and scale from the texture matrix
    const fm_mat4_t *matrix = gl_matrix_stack_get_matrix(&state->texture_stack);

    // Get translation from 4th column
    float offset_x = matrix->m[3][0] * (width << RDP_TEX_SHIFT);
    float offset_y = matrix->m[3][1] * (height << RDP_TEX_SHIFT);

    // Get scale from upper left 3x3 submatrix
    fm_vec3_t c0, c1;
    memcpy(c0.v, matrix->m[0], sizeof(fm_vec3_t));
    memcpy(c1.v, matrix->m[1], sizeof(fm_vec3_t));
    float scale_x = fm_vec3_len(&c0) * width;
    float scale_y = fm_vec3_len(&c1) * height;

    mgfx_texturing_parms_t parms = {
        .offset = { offset_x, offset_y },
        .scale = { scale_x, scale_y }
    };

    // TODO: Improve tex scale by making it 32 bit
    parms.scale[0] >>= TEX_SIZE_SHIFT;
    parms.scale[1] >>= TEX_SIZE_SHIFT;

    if (is_bilinear) {
        parms.offset[0] -= RDP_HALF_TEXEL;
        parms.offset[1] -= RDP_HALF_TEXEL;
    }

    mgfx_texturing_t *buffer = ringbuffer_alloc_next(&state->texturing_buffer);
    mgfx_get_texturing(buffer, &parms);
    mg_uniform_load(state->texturing_uniform, buffer);
    ringbuffer_release_current(&state->texturing_buffer);
}

void gl_upload_texturing()
{
    if (gl_is_enabled(ENABLE_RDPQ_TEXTURING)) {
        upload_uniform(state->rdpq_tex_width, state->rdpq_tex_height, false);
    } else {
        if (state->active_texture == NULL) return;
        uint16_t width, height;
        get_texture_size(state->active_texture, &width, &height);
        upload_uniform(width, height, texture_is_bilinear(state->active_texture));
    }
}

void apply_texture()
{
    // When RDPQ texturing is enabled, skip uploading textures entirely.
    if (gl_is_enabled(ENABLE_RDPQ_TEXTURING)) return;

    // Do nothing if there is no active texture. In this case the rendermode is not using texture anyway.
    if (state->active_texture == NULL) return;

    if (texture_is_block_dirty(state->active_texture)) {
        rspq_block_begin();
        upload_texture(state->active_texture);
        set_upload_block(state->active_texture, rspq_block_end());
    }

    rspq_block_run(state->active_texture->upload_block);
}
