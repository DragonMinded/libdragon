/**
 * @file rdpq_sprite.c
 * @brief RDP Command queue: high-level sprite loading and blitting
 * @ingroup rdp
 */

#include "rspq.h"
#include "rdpq.h"
#include "rdpq_sprite.h"
#include "rdpq_sprite_internal.h"
#include "rdpq_mode.h"
#include "rdpq_tex.h"
#include "sprite.h"
#include "sprite_internal.h"

static void sprite_upload_palette(sprite_t *sprite, int palidx, bool set_mode)
{
    // Check if the sprite has a palette
    tex_format_t fmt = sprite_get_format(sprite);
    rdpq_tlut_t tlut_mode = rdpq_tlut_from_format(fmt);

    if (__builtin_expect(set_mode, 1)) {
        // Configure the TLUT render mode
        rdpq_mode_tlut(tlut_mode);
    }

    if (tlut_mode != TLUT_NONE) {
        // Load the palette (if any). We account for sprites being CI4
        // but without embedded palette: mksprite doesn't create sprites like
        // this today, but it could in the future (eg: sharing a palette across
        // multiple sprites).
        uint16_t *pal = sprite_get_palette(sprite);
        if (pal) rdpq_tex_upload_tlut(pal, palidx*16, fmt == FMT_CI4 ? 16 : 256);
    }
}

/** @brief Internal implementation of #rdpq_sprite_upload that will optionally skip setting render modes */
int __rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms, bool set_mode)
{
    assertf(sprite_fits_tmem(sprite), "sprite doesn't fit in TMEM");

    // Load main sprite surface
    surface_t surf = sprite_get_pixels(sprite);

    // If no texparms were provided but the sprite contains some, use them
    rdpq_texparms_t parms_builtin;
    if (!parms && sprite_get_texparms(sprite, &parms_builtin))
        parms = &parms_builtin;

    // Check for detail texture
    sprite_detail_t detail; rdpq_texparms_t detailtexparms = {0};
    surface_t detailsurf = sprite_get_detail_pixels(sprite, &detail, &detailtexparms);
    bool use_detail = detailsurf.buffer != NULL;
    bool is_shq = sprite_is_shq(sprite);

    rdpq_tex_multi_begin();

    if(use_detail){
        // If there is a detail texture, we upload the main texture to TILE+1 and detail texture to TILE+0, then any mipmaps if there are any
        rdpq_tile_t detail_tile = tile;
        tile = (tile+1) & 7;

        // Setup the blend factor for the detail texture
        float factor = detail.blend_factor;
        rdpq_set_detail_factor(factor);

        // Setup the texparms for the detail texture
        if (parms) {
            detailtexparms.s.translate += parms->s.translate * (1 << (parms->s.scale_log - detailtexparms.s.scale_log));
            detailtexparms.t.translate += parms->t.translate * (1 << (parms->t.scale_log - detailtexparms.t.scale_log));
        }

        // Upload the detail texture if necessary or reuse the main texture
        if(detail.use_main_tex){
            rdpq_tex_upload(tile, &surf, parms);
            rdpq_tex_reuse(detail_tile, &detailtexparms);
        }
        else {
            rdpq_tex_upload(detail_tile, &detailsurf, &detailtexparms);
            rdpq_tex_upload(tile, &surf, parms);
        }
    }
    else // Upload the main texture
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

    if (__builtin_expect(set_mode, 1)) {
        // Enable/disable mipmapping
        if(is_shq) {
            rdpq_mode_mipmap(MIPMAP_INTERPOLATE_SHQ, num_mipmaps);
            rdpq_set_yuv_parms(0, 0, 0, 0, 0, 0xFF);
        } 
        else if(use_detail)          rdpq_mode_mipmap(MIPMAP_INTERPOLATE_DETAIL, num_mipmaps+1);
        else if (num_mipmaps)   rdpq_mode_mipmap(MIPMAP_INTERPOLATE, num_mipmaps);
        else                    rdpq_mode_mipmap(MIPMAP_NONE, 0);
    }

    // Upload the palette and configure the render mode
    sprite_upload_palette(sprite, parms ? parms->palette : 0, set_mode);

    return rdpq_tex_multi_end();
}

int rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms)
{
    return __rdpq_sprite_upload(tile, sprite, parms, true);
}

void rdpq_sprite_blit(sprite_t *sprite, float x0, float y0, const rdpq_blitparms_t *parms)
{
    assertf(!sprite_is_shq(sprite), "SHQ sprites only work with rdpq_sprite_upload, not rdpq_sprite_blit");

    // Upload the palette and configure the render mode
    sprite_upload_palette(sprite, 0, true);

    // Get the sprite surface
    surface_t surf = sprite_get_pixels(sprite);
    rdpq_tex_blit(&surf, x0, y0, parms);
}
