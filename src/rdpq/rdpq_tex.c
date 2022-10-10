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

int rdpq_tex_load_sub_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut, int s0, int t0, int s1, int t1)
{
    int tmem_pitch = ROUND_UP(tex->stride, 8);

    // LOAD_TILE does not support loading from a CI4 texture. We need to pretend
    // it's CI8 instead during loading, and then configure the tile with CI4. 
    rdpq_set_texture_image_raw(0, PhysicalAddr(tex->buffer), FMT_CI8, tex->width/2, tex->height);
    if (tex->stride == (s1-s0)/2 && tex->stride%8 == 0) {
        // Use LOAD_BLOCK if we are uploading a full texture. SET_TILE must be configured
        // with tmem_pitch=0, as that is weirdly used as the number of texels to skip per line,
        // which we don't need.
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tmem_addr, 0, 0);
        rdpq_load_block(RDPQ_TILE_INTERNAL, s0/2, t0, tex->stride * (t1 - t0), tmem_pitch);
    } else {
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tmem_addr, tmem_pitch, 0);
        rdpq_load_tile(RDPQ_TILE_INTERNAL, s0/2, t0, s1/2, t1);
    }
    rdpq_set_tile(tile, FMT_CI4, tmem_addr, tmem_pitch, tlut);
    rdpq_set_tile_size(tile, s0, t0, s1, t1);

    return tmem_pitch * tex->height;
}

int rdpq_tex_load_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut)
{
    return rdpq_tex_load_sub_ci4(tile, tex, tmem_addr, tlut, 0, 0, tex->width, tex->height);
}


int rdpq_tex_load_sub(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int s0, int t0, int s1, int t1)
{
    tex_format_t fmt = surface_get_format(tex);
    if (fmt == FMT_CI4)
        return rdpq_tex_load_sub_ci4(tile, tex, tmem_addr, 0, s0, t0, s1, t1);

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
