/**
 * @file rdpq_sprite.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: high-level sprite loading and blitting
 * @ingroup rdpq
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
        int num_colors = sprite_get_palette_used_colors(sprite);
        if (pal) rdpq_tex_upload_tlut(pal, palidx*16, num_colors);
    }
}

// Helper function to merge the texparms from various sources including handling the detail texparms
// Returns the merged texparms for the main texture plus the detail texparms are modified with main texture's
static rdpq_texparms_t __rdpq_sprite_combine_texparms(const rdpq_texparms_t *arg, const rdpq_texparms_t *sprite, rdpq_texparms_t *detail){
    rdpq_texparms_t argparms = {0}, sprparms = {0}, out = {0}; // reserve texparms for argument + sprite's
    if(arg) argparms = *arg;
    if(sprite) sprparms = *sprite;
    // merge texparms from both sources
    out.s.mirror = argparms.s.mirror | sprparms.s.mirror;
    out.t.mirror = argparms.t.mirror | sprparms.t.mirror;
    out.s.scale_log = argparms.s.scale_log + sprparms.s.scale_log;
    out.t.scale_log = argparms.t.scale_log + sprparms.t.scale_log;
    out.s.translate = argparms.s.translate + sprparms.s.translate;
    out.t.translate = argparms.t.translate + sprparms.t.translate;
    out.s.repeats = argparms.s.repeats + sprparms.s.repeats;
    out.t.repeats = argparms.t.repeats + sprparms.t.repeats;
    // Setup the texparms for the detail texture (handle them mainly through argparms since mksprite has already added info)
    if(detail){
        detail->s.mirror |= argparms.s.mirror;
        detail->t.mirror |= argparms.t.mirror;
        detail->s.scale_log += argparms.s.scale_log;
        detail->t.scale_log += argparms.t.scale_log;
        // since the translation is in pixels instead of normalized coordinates, we need to normalize the scaling of it
        detail->s.translate += out.s.translate * (1 << (out.s.scale_log - detail->s.scale_log));
        detail->t.translate += out.t.translate * (1 << (out.t.scale_log - detail->t.scale_log));
        detail->s.repeats += argparms.s.repeats;
        detail->t.repeats += argparms.t.repeats;
    }
    return out;
}

/** @brief Internal implementation of #rdpq_sprite_upload that will optionally skip setting render modes */
int __rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms, bool set_mode)
{
    assertf(sprite_fits_tmem(sprite), "sprite doesn't fit in TMEM");

    // Load main sprite surface
    surface_t surf = sprite_get_pixels(sprite);

    // If no texparms were provided but the sprite contains some, use them
    rdpq_texparms_t parms_builtin = {0};
    sprite_get_texparms(sprite, &parms_builtin);

    // Check for detail texture
    sprite_detail_t detail = {0}; rdpq_texparms_t detailtexparms = {0};
    surface_t detailsurf = sprite_get_detail_pixels(sprite, &detail, &detailtexparms);
    // Combine the texparms from various sources including detail, mksprite and arg info
    rdpq_texparms_t mergedtexparms = __rdpq_sprite_combine_texparms(parms, &parms_builtin, &detailtexparms);
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

        // Upload the detail texture if necessary or reuse the main texture
        if(detail.use_main_tex){
            rdpq_tex_upload(tile, &surf, &mergedtexparms);
            rdpq_tex_reuse(detail_tile, &detailtexparms);
        }
        else {
            rdpq_tex_upload(detail_tile, &detailsurf, &detailtexparms);
            rdpq_tex_upload(tile, &surf, &mergedtexparms);
        }
    }
    else // Upload the main texture
        rdpq_tex_upload(tile, &surf, &mergedtexparms);

    // Upload mipmaps if any
    int num_mipmaps = 0;
    rdpq_texparms_t lod_parms;
    for (int i=1; i<8; i++) {
        surf = sprite_get_lod_pixels(sprite, i);
        if (!surf.buffer) break;

        // if this is the first lod, initialize lod parameters
        if (i==1) {
            lod_parms = mergedtexparms;
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
        // Enable/disable mipmapping (SHQ has a special subtractive interpolation function)
        if(is_shq) {
            rdpq_mode_mipmap(MIPMAP_INTERPOLATE_SHQ, num_mipmaps);
            rdpq_set_yuv_parms(0, 0, 0, 0, 0, 0xFF);
        } 
        else if(use_detail)     rdpq_mode_mipmap(MIPMAP_INTERPOLATE_DETAIL, num_mipmaps+1);
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
