#include "gl_internal.h"
#include "../rspq/rspq_internal.h"
#include "../rdpq/rdpq_sprite_internal.h"
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

_Static_assert(TEXTURE_BILINEAR_MASK << TEX_BILINEAR_SHIFT == SOM_SAMPLE_BILINEAR >> 32);
_Static_assert(TEXTURE_BILINEAR_MASK << TEX_BILINEAR_OFFSET_SHIFT == HALF_TEXEL);
_Static_assert((1<<TEX_GEN_S_SHIFT) == FLAG_TEX_GEN_S);
_Static_assert((1<<TEX_GEN_LINEAR_FLAG_SHIFT) == FLAG_TEX_GEN_LINEAR);
_Static_assert((1<<TEX_GEN_SPHERICAL_FLAG_SHIFT) == FLAG_TEX_GEN_SPHERICAL);
_Static_assert((1<<NEED_EYE_SPACE_SHIFT) == FLAG_NEED_EYE_SPACE);
_Static_assert((SOM_SAMPLE_BILINEAR >> 32) >> BILINEAR_TEX_OFFSET_SHIFT == HALF_TEXEL);
_Static_assert(TEX_FLAG_DETAIL << TEX_DETAIL_SHIFT == SOM_TEXTURE_DETAIL >> 32);

extern gl_state_t state;
static inline void texture_get_texparms(gl_texture_object_t *obj, GLint level, rdpq_texparms_t *parms);

void gl_texture_set_min_filter(gl_texture_object_t *obj, uint32_t offset, GLenum param);

void gl_init_texture_object(gl_texture_object_t *obj)
{
    gl_srv_texture_object_t *srv_obj = malloc_uncached(sizeof(gl_srv_texture_object_t));
    *srv_obj = (gl_srv_texture_object_t){
        .min_filter = GL_NEAREST_MIPMAP_LINEAR,
        .mag_filter = GL_LINEAR,
    };

    // Fill the levels block with NOOPs, and terminate it with a RET.
    for (int i=0; i<MAX_TEXTURE_LEVELS*2; i++) {
        srv_obj->levels_block[i] = RSPQ_CMD_NOOP << 24;
    }
    srv_obj->levels_block[MAX_TEXTURE_LEVELS*2] = (RSPQ_CMD_RET << 24) | (1<<2);

    *obj = (gl_texture_object_t) {
        .wrap_s = GL_REPEAT,
        .wrap_t = GL_REPEAT,
        .srv_object = srv_obj,
    };
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
    if (obj->blocks[level] != NULL) {
        rdpq_call_deferred((void (*)(void*))rspq_block_free, obj->blocks[level]);
        obj->blocks[level] = NULL;
    }

    surface_free_safe(&obj->surfaces[level]);
}

void texture_image_free(gl_texture_object_t *obj, uint32_t level)
{
    #if 0
    gl_srv_texture_object_t *srv_obj = obj->srv_object;
    if ((srv_obj->levels_block[level*2] >> 24) == RSPQ_CMD_CALL) {
        rspq_block_t *mem = (rspq_block_t*)((srv_obj->levels_block[level*2] & 0xFFFFFF) | 0xA0000000);
        rspq_block_free(mem);
    }
    #else
    if (obj->blocks[level] != NULL) {
        rspq_block_free(obj->blocks[level]);
        obj->blocks[level] = NULL;
    }
    #endif

    surface_free(&obj->surfaces[level]);
}

void gl_cleanup_texture_object(gl_texture_object_t *obj)
{
    for (uint32_t i = 0; i < MAX_TEXTURE_LEVELS; i++)
    {
        texture_image_free(obj, i);
    }
    
    free_uncached(obj->srv_object);
    obj->srv_object = NULL;
}

void gl_texture_init()
{
    state.default_textures = malloc_uncached(sizeof(gl_texture_object_t) * 2);

    gl_init_texture_object(&state.default_textures[0]);
    gl_init_texture_object(&state.default_textures[1]);

    state.default_textures[0].dimensionality = GL_TEXTURE_1D;
    state.default_textures[1].dimensionality = GL_TEXTURE_2D;

    state.default_textures[0].flags |= TEX_IS_DEFAULT;
    state.default_textures[1].flags |= TEX_IS_DEFAULT;

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
        return offsetof(gl_server_state_t, bound_textures) + sizeof(gl_srv_texture_object_t) * 0;
    case GL_TEXTURE_2D:
        return offsetof(gl_server_state_t, bound_textures) + sizeof(gl_srv_texture_object_t) * 1;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid texture target", target);
        return 0;
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
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid texture target", target);
        return NULL;
    }
}

inline bool texture_is_sprite(gl_texture_object_t *obj)
{
    return obj->sprite != NULL;
}

inline bool texture_has_image(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_HAS_IMAGE) != 0;
}

inline bool texture_is_default(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_IS_DEFAULT) != 0;
}

void gl_texture_set_upload_block(uint32_t offset, int level, int width, int height, tex_format_t fmt, rspq_block_t *texup_block)
{
    assertf(texup_block->nesting_level == 0, "texture loader: nesting level is %ld", texup_block->nesting_level);

    uint32_t img_offset = offset + level * sizeof(gl_texture_image_t);
    gl_set_word (GL_UPDATE_NONE, img_offset + IMAGE_WIDTH_OFFSET,           (width << 16) | height);
    gl_set_short(GL_UPDATE_NONE, img_offset + IMAGE_INTERNAL_FORMAT_OFFSET, fmt);

    uint32_t cmd0 = (RSPQ_CMD_CALL << 24) | PhysicalAddr(texup_block->cmds);
    uint32_t cmd1 = texup_block->nesting_level << 2;
    gl_set_long(GL_UPDATE_TEXTURE_OBJECTS, offset + TEXTURE_LEVELS_BLOCK_OFFSET + level*8, ((uint64_t)cmd0 << 32) | cmd1);

    gl_set_flag_raw(GL_UPDATE_NONE, offset + TEXTURE_FLAGS_OFFSET, TEX_FLAG_UPLOAD_DIRTY, true);
}

void glSpriteTextureN64(GLenum target, sprite_t *sprite, rdpq_texparms_t *texparms)
{
    gl_assert_no_display_list();
    if (!gl_ensure_no_begin_end()) return;

    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) return;

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

    rspq_block_begin();
        rdpq_tex_multi_begin();
            __rdpq_sprite_upload(TILE0, sprite, texparms, false);
        rdpq_tex_multi_end();
    rspq_block_t *texup_block = rspq_block_end();

    obj->flags |= TEX_HAS_IMAGE;
    obj->sprite = sprite;
    obj->blocks[0] = texup_block;
    
    // Set tlut mode and level count
    rdpq_tlut_t tlut_mode = rdpq_tlut_from_format(sprite_get_format(sprite));
    int lod_count = sprite_get_lod_count(sprite) - 1;
    gl_set_short(GL_UPDATE_NONE, offset + TEXTURE_LEVELS_COUNT_OFFSET, (lod_count << 8) | tlut_mode);

    // Set min filter
    GLenum min_filter = lod_count > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    gl_texture_set_min_filter(obj, offset, min_filter);

    // Set detail mode
    surface_t detailsurf = sprite_get_detail_pixels(sprite, NULL, NULL);
    bool use_detail = detailsurf.buffer != NULL;
    gl_set_flag_raw(GL_UPDATE_NONE, offset + TEXTURE_FLAGS_OFFSET, TEX_FLAG_DETAIL, use_detail);

    // Mark texture as complete because sprites are complete by definition
    gl_set_flag_raw(GL_UPDATE_NONE, offset + TEXTURE_FLAGS_OFFSET, TEX_FLAG_COMPLETE, true);

    gl_texture_set_upload_block(offset, 0, sprite->width, sprite->height, sprite_get_format(sprite), texup_block);
}

void gl_surface_image(gl_texture_object_t *obj, uint32_t offset, GLint level, surface_t *surface, rdpq_texparms_t *parms)
{
    rspq_block_begin();
        rdpq_tex_multi_begin();
            rdpq_tex_upload(TILE0+level, surface, parms);
        rdpq_tex_multi_end();
    rspq_block_t *texup_block = rspq_block_end();

    obj->flags |= TEX_HAS_IMAGE;
    obj->blocks[level] = texup_block;

    // FIXME: This is kind of a hack because it sets the TLUT mode for the entire texture object.
    //        But since all levels need to have the same format for the texture to be complete, this happens to work.
    rdpq_tlut_t tlut_mode = rdpq_tlut_from_format(surface_get_format(surface));
    gl_set_byte(GL_UPDATE_NONE, offset + TEXTURE_TLUT_MODE_OFFSET, tlut_mode);

    gl_texture_set_upload_block(offset, level, surface->width, surface->height, surface_get_format(surface), texup_block);
    gl_update_texture_completeness(offset);
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

    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) return;

    gl_texture_object_t *obj = gl_get_texture_object(target);
    if (texture_is_sprite(obj)) {
        gl_set_error(GL_INVALID_OPERATION, "Cannot apply image to a sprite texture");
        return;
    }

    if (target == GL_TEXTURE_1D && surface->height != 1) {
        gl_set_error(GL_INVALID_VALUE, "Surface must have height 1 when using target GL_TEXTURE_1D");
        return;
    }

    rdpq_texparms_t parms;
    if (texparms != NULL) {
        parms = *texparms;
        parms.s.scale_log = level;
        parms.t.scale_log = level;
    } else {
        texture_get_texparms(obj, level, &parms);
    }

    texture_image_free_safe(obj, level);

    // Store the surface. We duplicate the surface structure (not the pixels)
    // using surface_make_sub so that we get a variant in which the owned bit
    // is not set; this in turns will make sure texture deletion would not free
    // the original surface (whose lifetime is left to the caller).
    obj->surfaces[level] = surface_make_sub(surface, 0, 0, surface->width, surface->height);

    gl_surface_image(obj, offset, level, &obj->surfaces[level], &parms);
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
}

void gl_texture_set_min_filter(gl_texture_object_t *obj, uint32_t offset, GLenum param)
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

    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_srv_texture_object_t, min_filter), (uint16_t)param);

    // TODO: is this correct?
    if (!texture_is_sprite(obj)) {
        gl_update_texture_completeness(offset);
    }
}

void gl_texture_set_mag_filter(uint32_t offset, GLenum param)
{
    switch (param) {
    case GL_NEAREST:
    case GL_LINEAR:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid magnification filter", param);
        return;
    }

    gl_set_short(GL_UPDATE_NONE, offset + offsetof(gl_srv_texture_object_t, mag_filter), (uint16_t)param);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    if (!gl_ensure_no_begin_end()) return;
    
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    gl_texture_object_t *obj = gl_get_texture_object(target);

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, param);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, param);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, offset, param);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, param);
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
    
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    gl_texture_object_t *obj = gl_get_texture_object(target);

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, param);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, param);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, offset, param);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, param);
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
    
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    gl_texture_object_t *obj = gl_get_texture_object(target);

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, params[0]);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, params[0]);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, offset, params[0]);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, params[0]);
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
    
    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) {
        return;
    }

    gl_texture_object_t *obj = gl_get_texture_object(target);

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        gl_texture_set_wrap_s(obj, params[0]);
        break;
    case GL_TEXTURE_WRAP_T:
        gl_texture_set_wrap_t(obj, params[0]);
        break;
    case GL_TEXTURE_MIN_FILTER:
        gl_texture_set_min_filter(obj, offset, params[0]);
        break;
    case GL_TEXTURE_MAG_FILTER:
        gl_texture_set_mag_filter(offset, params[0]);
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
        target_obj = &state.texture_1d_object;
        break;
    case GL_TEXTURE_2D:
        target_obj = &state.texture_2d_object;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid texture target", target);
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

        if (obj->dimensionality == 0) {
            obj->dimensionality = target;
        }

        if (obj->dimensionality != target) {
            gl_set_error(GL_INVALID_OPERATION, "Texture object has already been bound to another texture target");
            return;
        }

        *target_obj = obj;
    }

    gl_bind_texture(target, *target_obj);
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    if (!gl_ensure_no_begin_end()) return;
    
    for (uint32_t i = 0; i < n; i++)
    {
        gl_texture_object_t *new_object = malloc_uncached(sizeof(gl_texture_object_t));
        gl_init_texture_object(new_object);
        textures[i] = (GLuint)new_object;
    }
}

void texture_free(gl_texture_object_t* obj)
{
    gl_cleanup_texture_object(obj);
    free_uncached(obj);
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

        if (obj == state.texture_1d_object) {
            glBindTexture(GL_TEXTURE_1D, 0);
        } else if (obj == state.texture_2d_object) {
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

gl_texture_image_t * gl_get_texture_image(gl_texture_object_t *obj, GLint level)
{
    if (level < 0 || level > MAX_TEXTURE_LEVELS) {
        gl_set_error(GL_INVALID_VALUE, "%ld is not a valid texture image level (Must be in [0, %d])", level, MAX_TEXTURE_LEVELS);
        return NULL;
    }

    return &obj->srv_object->levels[level];
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

    uint32_t offset = gl_texture_get_offset(target);
    if (offset == 0) return;

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

    surface_t *surface = &obj->surfaces[level];

    uint32_t rdp_format = gl_tex_format_to_rdp(preferred_format);
    *surface = surface_alloc(rdp_format, width, height);
    if (surface->buffer == NULL) {
        gl_set_error(GL_OUT_OF_MEMORY, "Failed to allocate texture image");
        return;
    }

    if (data != NULL) {
        gl_transfer_pixels(surface->buffer, preferred_format, surface->stride, width, height, num_elements, format, type, 0, data);
    }

    rdpq_texparms_t parms;
    texture_get_texparms(obj, level, &parms);
    gl_surface_image(obj, offset, level, surface, &parms);
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
