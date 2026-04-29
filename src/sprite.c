/**
 * @file sprite.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include "sprite.h"
#include "n64sys.h"
#include "debug.h"
#include "surface.h"
#include "sprite_internal.h"
#include "asset_internal.h"
#include "asset.h"
#include "utils.h"
#include "rdpq_tex.h"
#include "yuv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static sprite_t *last_spritemap = NULL;

/** @brief Access the sprite extended structure, or NULL if the structure does not exist */
__attribute__((noinline))
sprite_ext_t *__sprite_ext(sprite_t *sprite)
{
    if (!(sprite->flags & SPRITE_FLAGS_EXT))
        return NULL;

    // For SPRITE_FLAGS_NODATA, the ext header is immediately after the base sprite header.
    // Otherwise, it follows the embedded pixel data.
    uint8_t *data = (uint8_t*)sprite->data;
    if (!(sprite->flags & SPRITE_FLAGS_NODATA)) {
        tex_format_t format = sprite_get_format(sprite);
        data += ROUND_UP(TEX_FORMAT_PIX2BYTES(format, sprite->width) * sprite->height, 8);
    }

    // Access extended header
    sprite_ext_t *sx = (sprite_ext_t*)data;
    assertf(sx->version == SPRITE_EXT_VERSION, "Invalid sprite version (%d); please regenerate your asset files", sx->version);
    return sx;
}

bool __sprite_upgrade(sprite_t *sprite)
{
    // Check if the sprite header begins with ASSET_MAGIC, which indicates a 
    // compressed sprite loaded with old-style file reading. In this case, we
    // can emit an assertion.
    assertf(memcmp(sprite, ASSET_MAGIC, 3) != 0, 
        "Sprite is compressed: use sprite_load() instead of reading the file manually");

    // Previously, the "format" field of the sprite structure (now renamed "flags")
    // was unused and always contained 0. Sprites could only be RGBA16 and RGBA32 anyway,
    // so only a bitdepth field could be used to understand the format.
    // To help backward compatibility, we want to try and still support this
    // old format.
    // Notice also that it is not enough to do this in sprite_load, because
    // sprite_load didn't exist at the time, and sprites were loaded manually
    // via fopen/fread.
    if (sprite->flags == 0) {
        // Read the bitdepth field without triggering the deprecation warning
        uint8_t bitdepth = ((uint8_t*)sprite)[4];
        sprite->flags = bitdepth == 2 ? FMT_RGBA16 : FMT_RGBA32;
        return true;
    }
    return false;
}

sprite_t *sprite_load_buf(void *buf, int sz)
{
    assertf(sz > 4, "Sprite buffer too small (sz=%d)", sz);
    if (memcmp(buf, "LSPR", 4) == 0)
    {
        // LSPR is a compressed format: it can't be used in-place. Decode it
        // into a freshly allocated FMT_YUV16 sprite. Caller retains ownership
        // of the source buffer, which can be freed after this call.
        return __lossysprite_decode_buf(buf, sz);
    }
    assertf(sz >= sizeof(sprite_t), "Sprite buffer too small (sz=%d)", sz);
    sprite_t *s = buf;
    __sprite_upgrade(s);
    (void)__sprite_ext(s); // just check if the sprite is valid (the version is checked in __sprite_ext)
    data_cache_hit_writeback(s, sz);
    return s;
}

sprite_t *sprite_load(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    sprite_t *s = sprite_load_buf(buf, sz);
    if ((void*)s != buf) {
        // The decoder allocated a fresh sprite (e.g. LSPR); the source
        // buffer is no longer needed. The new sprite already has
        // SPRITE_FLAGS_OWNEDBUFFER set internally.
        free(buf);
    } else {
        s->flags |= SPRITE_FLAGS_OWNEDBUFFER;
    }
    return s;
}

void sprite_free(sprite_t *s)
{
    // Release any subsystem refcounts held for this sprite. The YUV overlay
    // is registered in __lossysprite_decode_buf() for planar YUV sprites
    if (s->flags & SPRITE_FLAGS_EXT) {
        sprite_ext_t *sx = __sprite_ext(s);
        if (sx && (sx->flags & SPRITE_FLAG_YUV_PLANAR)) yuv_close();
    }

    if(s->flags & SPRITE_FLAGS_OWNEDBUFFER) {
        #ifndef NDEBUG
        //To help debugging, zero the sprite structure as well
        memset(s, 0, sizeof(sprite_t));
        #endif
        free(s);
    }
    
    if (last_spritemap == s)
        last_spritemap = NULL;
}

surface_t sprite_get_pixels(sprite_t *sprite) {
    void *pixels = sprite->data;
    if (sprite->flags & SPRITE_FLAGS_NODATA) {
        sprite_ext_t *sx = __sprite_ext(sprite);
        pixels = (void*)((uint8_t*)sprite + sx->data_ptr);
    }
    return surface_make_linear(pixels, sprite_get_format(sprite),
        sprite->width, sprite->height);
}

surface_t sprite_get_lod_pixels(sprite_t *sprite, int num_level) {
    assert(num_level >= 0 && num_level < 8);

    // First LOD = image. Return the image pixels
    if (num_level == 0)
        return sprite_get_pixels(sprite);

    // Get access to the extended sprite structure
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        return (surface_t){0};

    // Get access to the lod structure
    struct sprite_lod_s *lod = &sx->lods[num_level-1];
    if (lod->width == 0)
        return (surface_t){0};

    // Return the surface that refers to this LOD
    tex_format_t fmt = lod->fmt_file_pos >> 24;
    void *pixels = (void*)sprite + (lod->fmt_file_pos & 0x00FFFFFF);
    return surface_make_linear(pixels, fmt, lod->width, lod->height);
}

static void sprite_get_detail_texparms(sprite_t *sprite, rdpq_texparms_t *parms) {
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        return;
    if (parms) {
        memset(parms, 0, sizeof(*parms));
        parms->s.translate = sx->detail.texparms.s.translate;
        parms->t.translate = sx->detail.texparms.t.translate;
        parms->s.scale_log = sx->detail.texparms.s.scale_log;
        parms->t.scale_log = sx->detail.texparms.t.scale_log;
        parms->s.repeats = sx->detail.texparms.s.repeats;
        parms->t.repeats = sx->detail.texparms.t.repeats;
        parms->s.mirror = sx->detail.texparms.s.mirror;
        parms->t.mirror = sx->detail.texparms.t.mirror;
    }
}

surface_t sprite_get_detail_pixels(sprite_t *sprite, sprite_detail_t *info, rdpq_texparms_t *infoparms) {
    // Get access to the extended sprite structure
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        return (surface_t){0};

    if(!(sx->flags & SPRITE_FLAG_HAS_DETAIL))
        return (surface_t){0};

    if(info){
        info->use_main_tex = sx->detail.use_main_texture;
        info->blend_factor = sx->detail.blend_factor;
    } 
    if(infoparms) 
        sprite_get_detail_texparms(sprite, infoparms);

    if(sx->detail.use_main_texture)
        return sprite_get_lod_pixels(sprite, 0);
    // Return the detail texture (LOD7)
    return sprite_get_lod_pixels(sprite, 7);
}

uint16_t* sprite_get_palette(sprite_t *sprite) {
    sprite_ext_t *sx = __sprite_ext(sprite);
    if(!sx || !sx->pal_file_pos)
        return NULL;
    return (void*)sprite + sx->pal_file_pos;
}

int sprite_get_palette_used_colors(sprite_t *sprite) {
    sprite_ext_t *sx = __sprite_ext(sprite);
    return sx->pal_used_colors ? sx->pal_used_colors : 256;
}

surface_t sprite_get_tile(sprite_t *sprite, int h, int v) {
    static int tile_width = 0, tile_height = 0;

    // Compute tile width and height. Unfortunately, the sprite structure
    // store the number of tile rather than the size of a tile, so we are
    // forced to do divisions. Cache the result, as it is common to call
    // this function multiple times anyway.
    if (last_spritemap != sprite) {
        last_spritemap = sprite;
        tile_width = sprite->width / sprite->hslices;
        tile_height = sprite->height / sprite->vslices;
    }

    surface_t surf = sprite_get_pixels(sprite);
    return surface_make_sub(&surf, 
        h*tile_width, v*tile_height,
        tile_width, tile_height);
}

bool sprite_get_texparms(sprite_t *sprite, rdpq_texparms_t *parms) {
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        return false;
    if (!(sx->flags & SPRITE_FLAG_HAS_TEXPARMS))
        return false;
    if (parms) {
        memset(parms, 0, sizeof(*parms));
        parms->s.translate = sx->texparms.s.translate;
        parms->t.translate = sx->texparms.t.translate;
        parms->s.scale_log = sx->texparms.s.scale_log;
        parms->t.scale_log = sx->texparms.t.scale_log;
        parms->s.repeats = sx->texparms.s.repeats;
        parms->t.repeats = sx->texparms.t.repeats;
        parms->s.mirror = sx->texparms.s.mirror;
        parms->t.mirror = sx->texparms.t.mirror;
    }
    return true;
}

int sprite_get_lod_count(sprite_t *sprite) {
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        return 1;

    int count = 1; // start from main texture
    for (int i=0; i<7; i++)
        if (sx->lods[i].width)
            count++;
    return count;
}

bool sprite_fits_tmem(sprite_t *sprite)
{
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        // FIXME: we don't have the information readily available for old sprites
        return false;
    
    return (sx->flags & SPRITE_FLAG_FITS_TMEM) != 0;
}

bool sprite_is_shq(sprite_t *sprite)
{
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (!sx)
        return false;
    return (sx->flags & SPRITE_FLAG_SHQ) != 0;
}

extern inline tex_format_t sprite_get_format(sprite_t *sprite);
