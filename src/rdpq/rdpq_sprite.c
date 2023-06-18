/**
 * @file rdpq_sprite.c
 * @brief RDP Command queue: high-level sprite loading and blitting
 * @ingroup rdp
 */

#include "rspq.h"
#include "rdpq.h"
#include "rdpq_sprite.h"
#include "rdpq_mode.h"
#include "rdpq_tex.h"
#include "sprite.h"
#include "sprite_internal.h"

static void sprite_upload_palette(sprite_t *sprite, int palidx)
{
    // Check if the sprite has a palette
    tex_format_t fmt = sprite_get_format(sprite);
    rdpq_tlut_t tlut_mode = rdpq_tlut_from_format(fmt);

    // Configure the TLUT render mode
    rdpq_mode_tlut(tlut_mode);

    if (tlut_mode != TLUT_NONE) {
        // Load the palette (if any). We account for sprites being CI4
        // but without embedded palette: mksprite doesn't create sprites like
        // this today, but it could in the future (eg: sharing a palette across
        // multiple sprites).
        uint16_t *pal = sprite_get_palette(sprite);
        if (pal) rdpq_tex_upload_tlut(pal, palidx*16, fmt == FMT_CI4 ? 16 : 256);
    }
}

int rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms)
{
    // Load main sprite surface
    surface_t surf = sprite_get_pixels(sprite);

    // If no texparms were provided but the sprite contains some, use them
    rdpq_texparms_t parms_builtin;
    if (!parms && sprite_get_texparms(sprite, &parms_builtin))
        parms = &parms_builtin;

    rdpq_tex_multi_begin();
    rdpq_tex_upload(tile, &surf, parms);

    // Upload mipmaps if any
    int num_mipmaps = 0;
    rdpq_texparms_t lod_parms;
    for (int i=1; i<8; i++) {
        surf = sprite_get_lod_pixels(sprite, i);
        if (!surf.buffer) break;

        // if this is the first lod, initialize lod parameters
        if (i==1) {
            if (!parms) {
                memset(&lod_parms, 0, sizeof(lod_parms));
            } else {
                lod_parms = *parms;
            }
        }

        // Update parameters for next lod. If the scale maxes out, stop here
        num_mipmaps++;
        tile = (tile+1) & 7;
        if (++lod_parms.s.scale_log >= 11) break;
        if (++lod_parms.t.scale_log >= 11) break;
        lod_parms.s.translate *= 0.5f;
        lod_parms.t.translate *= 0.5f;

        // Load the mipmap
        rdpq_tex_upload(tile, &surf, &lod_parms);
    }

    // Enable/disable mipmapping
    if (num_mipmaps)
        rdpq_mode_mipmap(MIPMAP_INTERPOLATE, num_mipmaps);
    else
        rdpq_mode_mipmap(MIPMAP_NONE, 0);

    // Upload the palette and configure the render mode
    sprite_upload_palette(sprite, parms ? parms->palette : 0);

    return rdpq_tex_multi_end();
}

void rdpq_sprite_blit(sprite_t *sprite, float x0, float y0, const rdpq_blitparms_t *parms)
{
    // Upload the palette and configure the render mode
    sprite_upload_palette(sprite, 0);

    // Get the sprite surface
    surface_t surf = sprite_get_pixels(sprite);
    rdpq_tex_blit(&surf, x0, y0, parms);
}
