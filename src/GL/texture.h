/**
 * @file texture.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_TEXTURE
#define __GL_TEXTURE

#include "GL/gl.h"
#include "gl_constants.h"
#include "rdpq_tex.h"

#define MAX_TEXTURE_SIZE      64
#define MAX_TEXTURE_LEVELS    7

#define TEXTURE_BILINEAR_MASK       0x001
#define TEXTURE_INTERPOLATE_MASK    0x002
#define TEXTURE_MIPMAP_MASK         0x100

typedef enum {
    TEX_IS_DEFAULT          = (1 << 0),
    TEX_IS_COMPLETE         = (1 << 1),
    TEX_HAS_IMAGE           = (1 << 2),
    TEX_IS_BLOCK_DIRTY      = (1 << 3),
} gl_texture_flag_t;

typedef struct {
    surface_t surface;
    rdpq_texparms_t parms;
} gl_texture_image_t;

typedef struct {
    uint32_t flags;
    GLenum dimensionality;
    GLenum wrap_s;
    GLenum wrap_t;
    GLenum min_filter;
    GLenum mag_filter;
    
    uint32_t levels_count;
    gl_texture_image_t levels[MAX_TEXTURE_LEVELS]; // TODO: allocate lazily
    sprite_t *sprite;
    rspq_block_t *upload_block;
} gl_texture_object_t;


#ifdef __cplusplus
extern "C" {
#endif

void gl_texture_init();
void gl_texture_close();

gl_texture_object_t *gl_get_active_texture();

inline bool texture_is_sprite(gl_texture_object_t *obj)
{
    return obj->sprite != NULL;
}

inline bool texture_is_complete(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_IS_COMPLETE) != 0;
}

inline bool texture_has_image(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_HAS_IMAGE) != 0;
}

inline bool texture_is_default(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_IS_DEFAULT) != 0;
}

inline bool texture_is_block_dirty(gl_texture_object_t *obj)
{
    return (obj->flags & TEX_IS_BLOCK_DIRTY) != 0 || obj->upload_block == NULL;
}

inline bool texture_has_mipmaps(gl_texture_object_t *obj)
{
    return (obj->min_filter & TEXTURE_MIPMAP_MASK) != 0;
}

inline bool texture_is_interpolating_mipmaps(gl_texture_object_t *obj)
{
    return (obj->min_filter & TEXTURE_INTERPOLATE_MASK) != 0;
}

inline bool texture_is_bilinear(gl_texture_object_t *obj)
{
    return ((obj->min_filter | obj->mag_filter) & TEXTURE_BILINEAR_MASK) != 0;
}

#ifdef __cplusplus
}
#endif

#endif
