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
#include "yuv.h"

static inline const yuv_colorspace_t *sprite_lookup_yuv_colorspace(sprite_ext_t *sx)
{
    if (!sx) return &YUV_BT601_TV;
    return __sprite_colorspace(sx->colorspace);
}

static void sprite_setup_yuv_mode(sprite_t *sprite)
{
    rdpq_set_mode_yuv(true);
    const yuv_colorspace_t *cs = sprite_lookup_yuv_colorspace(__sprite_ext(sprite));
    rdpq_set_yuv_parms(cs->k0, cs->k1, cs->k2, cs->k3, cs->k4, cs->k5);
}

static void sprite_blit_yuv_nv12(sprite_t *sprite, float x0, float y0, const rdpq_blitparms_t *parms_in)
{
    sprite_ext_t *sx = __sprite_ext(sprite);
    int padded_w = (int)sx->texparms.s.translate;
    int padded_h = (int)sx->texparms.t.translate;
    uint8_t *base = (uint8_t*)sprite + sx->data_ptr;
    size_t y_bytes = (size_t)padded_w * padded_h;

    // Y plane is FMT_I8; UV plane is FMT_IA16 with U in the high byte and
    // V in the low byte of each pixel — the layout that the LSPR decoder
    // pre-interleaves at decode time.
    surface_t y_surf  = surface_make_linear(base,           FMT_I8,   padded_w,     padded_h);
    surface_t uv_surf = surface_make_linear(base + y_bytes, FMT_IA16, padded_w / 2, padded_h / 2);

    // Crop from the padded plane to the sprite's visible region. If the caller
    // already supplied width/height, respect those; otherwise pin to the
    // sprite dims.
    rdpq_blitparms_t parms;
    if (parms_in) parms = *parms_in;
    else memset(&parms, 0, sizeof(parms));
    if (parms.width  == 0) parms.width  = sprite->width;
    if (parms.height == 0) parms.height = sprite->height;

    // Save/restore the caller's render mode around the YUV blit so the
    // YUV combiner + SOM bits set inside yuv_tex_blit_run don't leak into
    // subsequent non-YUV draws.
    rdpq_mode_push();
    yuv_tex_blit_nv12(&y_surf, &uv_surf, x0, y0, &parms, sprite_lookup_yuv_colorspace(sx));
    rdpq_mode_pop();
}

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

/** @brief Internal implementation of #rdpq_sprite_upload that will optionally skip setting render modes */
int __rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms, bool set_mode)
{
    sprite_ext_t *sx = __sprite_ext(sprite);
    assertf(!sx || !(sx->flags & SPRITE_FLAG_YUV_NV12),
        "NV12 YUV sprites cannot be uploaded to TMEM as a single texture; use rdpq_sprite_blit");
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
        // For YUV sprites, configure the RDP YUV render mode + colorspace
        // from the sprite's metadata. This must happen before mipmap/tlut
        // tweaks below, since rdpq_set_mode_yuv resets the SOM.
        if (sprite_get_format(sprite) == FMT_YUV16)
            sprite_setup_yuv_mode(sprite);

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

    // NV12 YUV sprites can't be uploaded as a single texture (the Y and UV
    // halves are loaded separately into the two TMEM banks). Hand off to
    // yuv_tex_blit_nv12, which sets up its own render mode and colorspace.
    sprite_ext_t *sx = __sprite_ext(sprite);
    if (sx && (sx->flags & SPRITE_FLAG_YUV_NV12)) {
        sprite_blit_yuv_nv12(sprite, x0, y0, parms);
        return;
    }

    // For YUV sprites, configure the RDP YUV render mode + colorspace from
    // the sprite's metadata so the caller doesn't need to set it up. Save
    // the caller's render mode around the blit so the YUV combiner + SOM
    // bits don't leak into subsequent non-YUV draws.
    bool is_yuv = sprite_get_format(sprite) == FMT_YUV16;
    if (is_yuv) {
        rdpq_mode_push();
        sprite_setup_yuv_mode(sprite);
    }

    // Upload the palette and configure the render mode
    sprite_upload_palette(sprite, 0, true);

    // Get the sprite surface
    surface_t surf = sprite_get_pixels(sprite);
    rdpq_tex_blit(&surf, x0, y0, parms);

    // Restore the caller's render mode if we changed it for YUV
    if (is_yuv) rdpq_mode_pop();
}
