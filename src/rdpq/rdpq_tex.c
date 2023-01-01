/**
 * @file rdpq_tex.c
 * @brief RDP Command queue: texture loading
 * @ingroup rdp
 */

#include "rdpq.h"
#include "rdpq_tex.h"
#include "utils.h"

/** @brief Address in TMEM where the palettes must be loaded */
#define TMEM_PALETTE_ADDR   0x800

void rdpq_tex_load_tlut(uint16_t *tlut, int color_idx, int num_colors)
{
    rdpq_set_texture_image_raw(0, PhysicalAddr(tlut), FMT_RGBA16, num_colors, 1);
    rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_I4, TMEM_PALETTE_ADDR + color_idx*16*2*4, num_colors, 0);
    rdpq_load_tlut(RDPQ_TILE_INTERNAL, color_idx, num_colors);
}

static bool tex_load_as_block_4bpp(surface_t *tex, int tmem_addr, int tmem_pitch, int s0, int t0, int s1, int t1)
{
    if (tex->stride != (s1+1)/2 - s0/2)
        return false;
    if (tex->stride%8 != 0)
        return false;
    if (t0 & 1)  // can't load starting from odd lines because of odd lines are dword-swapped in TMEM
        return false;

    // Calculate the number of texels to transfer using a 8bpp format.
    // If it's more than 2048, try as a 16bpp format instead
    tex_format_t load_fmt = FMT_CI8;
    int num_texels = tex->stride * (t1 - t0);
    if (num_texels > 2048) {
        // If the stride in bytes is odd, we can't use 16bpp, so fallback to LOAD_TILE instead.
        if (tex->stride%2 != 0)
            return false;

        load_fmt = FMT_RGBA16;
        num_texels /= 2;
        if (num_texels > 2048)
            return false;
    }

    // Use LOAD_BLOCK if we are uploading a full texture. Notice the weirdness of LOAD_BLOCK:
    // * SET_TILE must be configured with tmem_pitch=0, as that is weirdly used as the number of
    //   texels to skip per line, which we don't need.
    // * SET_TEXTURE_IMAGE width is ignored, so we just put 0 there to avoid confusion.
    rdpq_set_texture_image_raw(0, PhysicalAddr(tex->buffer), load_fmt, 0, tex->height);
    rdpq_set_tile(RDPQ_TILE_INTERNAL, load_fmt, tmem_addr, 0, 0);
    rdpq_load_block(RDPQ_TILE_INTERNAL, s0/2, t0, num_texels, tmem_pitch);
    return true;
}

static int rdpq_tex_load_sub_4bpp(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut, int s0, int t0, int s1, int t1)
{
    int tmem_pitch = ROUND_UP((s1+1)/2 - s0/2, 8);

    // Try to load the texture as a block, if possible. If it is not, fall back to LOAD_TILE.
    if (!tex_load_as_block_4bpp(tex, tmem_addr, tmem_pitch, s0, t0, s1, t1)) {
        // LOAD_TILE does not support loading from a 4bpp texture. We need to pretend
        // it's CI8 instead during loading, and then configure the tile with the correct 4bpp format.
        rdpq_set_texture_image_raw(0, PhysicalAddr(tex->buffer), FMT_CI8, tex->stride, tex->height);
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tmem_addr, tmem_pitch, 0);
        rdpq_load_tile(RDPQ_TILE_INTERNAL, s0/2, t0, (s1+1)/2, t1);
    }

    rdpq_set_tile(tile, surface_get_format(tex), tmem_addr, tmem_pitch, tlut);
    rdpq_set_tile_size(tile, s0/2*2, t0, (s1+1)/2*2, t1);

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

    int tmem_pitch = TEX_FORMAT_PIX2BYTES(fmt, s1 - s0);

    // In RGBA32 mode, data is split in two halves in TMEM (R,G in the first TMEM half,
    // B,A in the second TMEM half). This means that the pitch can be halved, as it is
    // calculated only over 2 channels instead of 4.
    if (fmt == FMT_RGBA32)
        tmem_pitch /= 2;

    tmem_pitch = ROUND_UP(tmem_pitch, 8);

    rdpq_set_tile(tile, fmt, tmem_addr, tmem_pitch, 0);
    rdpq_set_texture_image(tex);
    rdpq_load_tile(tile, s0, t0, s1, t1);

    return tmem_pitch * tex->height;
}

int rdpq_tex_load(rdpq_tile_t tile, surface_t *tex, int tmem_addr)
{
    return rdpq_tex_load_sub(tile, tex, tmem_addr, 0, 0, tex->width, tex->height);
}

/**
 * @brief Helper function to draw a large surface that doesn't fit in TMEM.
 * 
 * This function analyzes the surface, finds the optimal splitting strategy to
 * divided into rectangles that fit TMEM, and then go through them one of by one,
 * loading them into TMEM and drawing them.
 * 
 * The actual drawing is done by the caller, through the draw_cb function. This
 * function will just call it with the information on the current rectangle
 * within the original surface.
 * 
 * @param tile          Hint of the tile to use. Note that this function is free to use
 *                      other tiles to perform its job.
 * @param tex           Surface to draw
 * @param draw_cb       Callback function to draw rectangle by rectangle. It will be called
 *                      with the tile to use for drawing, and the rectangle of the original
 *                      surface that has been loaded into TMEM.
 */
static void tex_draw_split(rdpq_tile_t tile, surface_t *tex, 
    void (*draw_cb)(rdpq_tile_t tile, int s0, int t0, int s1, int t1))
{
    // The most efficient way to split a large surface is to load it in horizontal strips,
    // whose height maximizes TMEM usage. The last strip might be smaller than the others.

    // Calculate the optimal height for a strip, based on the TMEM pitch.
    tex_format_t fmt = surface_get_format(tex);
    int tmem_pitch = ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, tex->width), 8);
    int tile_h = 4096 / tmem_pitch;
    
    // Initial configuration of the tile
    rdpq_set_texture_image(tex);
    rdpq_set_tile(tile, fmt, 0, tmem_pitch, 0);

    // Go through the surface
    int s0 = 0, t0 = 0;
    while (t0 < tex->height) 
    {
        // Load the current strip
        int h = MIN(tile_h, tex->height - t0);
        rdpq_load_tile(tile, s0, t0, tex->width, t0 + h);

        // Call the draw callback for this strip
        draw_cb(tile, s0, t0, tex->width, t0 + h);

        t0 += h;
    }
}


void rdpq_tex_blit(rdpq_tile_t tile, surface_t *tex, int x0, int y0, int screen_width, int screen_height)
{
    float scalex = (float)screen_width / (float)tex->width;
    float scaley = (float)screen_height / (float)tex->height;
    float dsdx = 1.0f / scalex;
    float dsdy = 1.0f / scaley;

    void draw_cb(rdpq_tile_t tile, int s0, int t0, int s1, int t1)
    {
        rdpq_texture_rectangle(tile, 
            x0 + s0 * scalex, y0 + t0 * scaley,
            x0 + s1 * scalex, y0 + t1 * scaley,
            s0, t0, dsdx, dsdy);
    }

    tex_draw_split(tile, tex, draw_cb);
}
