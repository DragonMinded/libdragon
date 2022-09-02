#include "sprite.h"
#include "surface.h"
#include "sprite_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sprite_t *last_spritemap = NULL;

bool __sprite_upgrade(sprite_t *sprite)
{
    // Previously, the "format" field of the sprite structure was unused
    // and always contained 0. Sprites could only be RGBA16 and RGBA32 anyway,
    // so only a bitdepth field could be used to understand the format.
    // To help backward compatibility, we want to try and still support this
    // old format.
    // Notice also that it is not enough to do this in sprite_load, because
    // sprite_load didn't exist at the time, and sprites were loaded manually
    // via fopen/fread.
    if (sprite->format == FMT_NONE) {
        // Read the bitdepth field without triggering the deprecation warning
        uint8_t bitdepth = ((uint8_t*)sprite)[4];
        if (bitdepth == 2)
            sprite->format = FMT_RGBA16;
        else
            sprite->format = FMT_RGBA32;
        return true;
    }
    return false;
}

sprite_t *sprite_load(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);

    int sz = ftell(f);
    sprite_t *s = malloc(sz);

    fseek(f, 0, SEEK_SET);
    fread(s, 1, sz, f);
    fclose(f);

    __sprite_upgrade(s);

    return s;
}

void sprite_free(sprite_t *s)
{
    #ifndef NDEBUG
    // To help debugging, zero the sprite structure as well
    memset(s, 0, sizeof(sprite_t));
    #endif

    free(s);
    if (last_spritemap == s)
        last_spritemap = NULL;
}

surface_t sprite_get_pixels(sprite_t *sprite) {
    uint8_t *data = (uint8_t*)sprite->data;

    // Skip palette (if any)
    if (sprite->format == FMT_CI4) data += 16*2;
    if (sprite->format == FMT_CI8) data += 256*2;

    return surface_make(data, sprite->format,
        sprite->width, sprite->height,
        TEX_FORMAT_PIX2BYTES(sprite->format, sprite->width));
}

uint16_t* sprite_palette(sprite_t *sprite) {
    if (sprite->format == FMT_CI4 || sprite->format == FMT_CI8)
        return (uint16_t*)sprite->data;
    return NULL;
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
