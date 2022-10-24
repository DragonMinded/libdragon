/**
 * @file rdpq_tex.c
 * @brief RDP Command queue: texture loading
 * @ingroup rdp
 */

#include "rdpq_tex.h"
#include "utils.h"

void rdpq_tex_load_tlut(uint16_t *tlut, int color_idx, int num_colors)
{
    rdpq_set_texture_image_raw(0, PhysicalAddr(tlut), FMT_RGBA16, num_colors, 1);
    rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_I4, 0x800 + color_idx*16*2*4, num_colors, 0);
    rdpq_load_tlut(RDPQ_TILE_INTERNAL, color_idx, num_colors);
}

static int rdpq_tex_load_sub_4bpp(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut, int s0, int t0, int s1, int t1)
{
    int tmem_pitch = ROUND_UP(s1/2 - s0/2, 8);

    // LOAD_TILE does not support loading from a 4bpp texture. We need to pretend
    // it's CI8 instead during loading, and then configure the tile with the correct 4bpp format.
    rdpq_set_texture_image_raw(0, PhysicalAddr(tex->buffer), FMT_CI8, tex->width/2, tex->height);
    if (tex->stride == s1/2 - s0/2 && tex->stride%8 == 0) {
        // Use LOAD_BLOCK if we are uploading a full texture. SET_TILE must be configured
        // with tmem_pitch=0, as that is weirdly used as the number of texels to skip per line,
        // which we don't need.
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tmem_addr, 0, 0);
        rdpq_load_block(RDPQ_TILE_INTERNAL, s0/2, t0, tex->stride * (t1 - t0), tmem_pitch);
    } else {
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tmem_addr, tmem_pitch, 0);
        rdpq_load_tile(RDPQ_TILE_INTERNAL, s0/2, t0, s1/2, t1);
    }
    rdpq_set_tile(tile, surface_get_format(tex), tmem_addr, tmem_pitch, tlut);
    rdpq_set_tile_size(tile, s0, t0, s1, t1);

    return tmem_pitch * tex->height;
}

int rdpq_tex_load_sub_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut, int s0, int t0, int s1, int t1)
{
    return rdpq_tex_load_sub_4bpp(tile, tex, tmem_addr, tlut, s0, t0, s1, t1);
}

int rdpq_tex_load_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut)
{
    return rdpq_tex_load_sub_ci4(tile, tex, tmem_addr, tlut, 0, 0, tex->width, tex->height);
}

int rdpq_tex_load_sub(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int s0, int t0, int s1, int t1)
{
    // Call the CI4 version for both FMT_CI4 and FMT_IA4 (in the latter case,
    // the tlut argument will be ignored).
    tex_format_t fmt = surface_get_format(tex);
    if (TEX_FORMAT_BITDEPTH(fmt) == 4)
        return rdpq_tex_load_sub_4bpp(tile, tex, tmem_addr, 0, s0, t0, s1, t1);

    int tmem_pitch = ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, s1 - s0), 8);

    rdpq_set_tile(tile, fmt, tmem_addr, tmem_pitch, 0);
    rdpq_set_texture_image(tex);
    rdpq_load_tile(tile, s0, t0, s1, t1);

    return tmem_pitch * tex->height;
}

int rdpq_tex_load(rdpq_tile_t tile, surface_t *tex, int tmem_addr)
{
    return rdpq_tex_load_sub(tile, tex, tmem_addr, 0, 0, tex->width, tex->height);
}
