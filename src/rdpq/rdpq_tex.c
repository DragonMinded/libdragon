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
    rdpq_load_tlut(RDPQ_TILE_INTERNAL, color_idx, color_idx + num_colors - 1);
}

int rdpq_tex_load_ci4(int tile, surface_t *tex, int tmem_addr, int tlut)
{
    int tmem_pitch = ROUND_UP(tex->width / 2, 8);

    rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tmem_addr, tmem_pitch, 0);
    rdpq_set_texture_image_raw(0, PhysicalAddr(tex->buffer), FMT_CI8, tex->width/2, tex->height);
    rdpq_load_tile(RDPQ_TILE_INTERNAL, 0, 0, tex->width/2, tex->height);
    rdpq_set_tile(tile, FMT_CI4, tmem_addr, tmem_pitch, tlut);
    rdpq_set_tile_size(tile, 0, 0, tex->width, tex->height);

    return tmem_pitch * tex->height;
}

int rdpq_tex_load(int tile, surface_t *tex, int tmem_addr)
{
    tex_format_t fmt = surface_get_format(tex);
    switch (fmt) {
    case FMT_CI4: return rdpq_tex_load_ci4(tile, tex, tmem_addr, 0);
    default:      assertf(0, "format %s not yet supported", tex_format_name(fmt));
    }
}
