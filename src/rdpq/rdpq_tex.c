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

enum tex_load_mode {
    TEX_LOAD_UNKNOWN,
    TEX_LOAD_TILE,
    TEX_LOAD_BLOCK,
};

typedef struct tex_loader_s {
    surface_t *tex;
    rdpq_tile_t tile;
    struct {
        int width, height;
        int num_texels, tmem_pitch;
        bool can_load_block;
    } rect;
    int tmem_addr;
    int tlut;
    enum tex_load_mode load_mode;
    void (*load_block)(struct tex_loader_s *tload, int s0, int t0, int s1, int t1);
    void (*load_tile)(struct tex_loader_s *tload, int s0, int t0, int s1, int t1);
} tex_loader_t;

static int texload_set_rect(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    tex_format_t fmt = surface_get_format(tload->tex);
    if (TEX_FORMAT_BITDEPTH(fmt) == 4) {
        s0 &= ~1; s1 = (s1+1) & ~1;
    }

    int width = s1 - s0;
    int height = t1 - t0;

    if (width != tload->rect.width || height != tload->rect.height) {
        if (width != tload->rect.width) {
            int pitch_shift = fmt == FMT_RGBA32 ? 1 : 0;
            int stride_mask = fmt == FMT_RGBA32 ? 15 : 7;
            tload->rect.tmem_pitch = ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, width) >> pitch_shift, 8);
            tload->rect.can_load_block = 
                tload->tile != RDPQ_TILE_INTERNAL && 
                TEX_FORMAT_PIX2BYTES(fmt, width) == tload->tex->stride &&
                (tload->tex->stride & stride_mask) == 0;
            tload->load_mode = TEX_LOAD_UNKNOWN;
        }
        tload->rect.width = width;
        tload->rect.height = height;
        tload->rect.num_texels = width * height;
    }
    return tload->rect.tmem_pitch * height;
}

static int tex_loader_load(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    int mem = texload_set_rect(tload, s0, t0, s1, t1);
    if (tload->rect.can_load_block && (t0 & 1) == 0)
        tload->load_block(tload, s0, t0, s1, t1);
    else
        tload->load_tile(tload, s0, t0, s1, t1);
    return mem;
}

static void tex_loader_set_tmem_addr(tex_loader_t *tload, int tmem_addr)
{
    tload->tmem_addr = tmem_addr;
    tload->load_mode = TEX_LOAD_UNKNOWN;
}

static void tex_loader_set_tlut(tex_loader_t *tload, int tlut)
{
    tload->tlut = tlut;
    tload->load_mode = TEX_LOAD_UNKNOWN;
}

static void texload_block_4bpp(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    if (tload->load_mode != TEX_LOAD_BLOCK) {
        // Use LOAD_BLOCK if we are uploading a full texture. Notice the weirdness of LOAD_BLOCK:
        // * SET_TILE must be configured with tmem_pitch=0, as that is weirdly used as the number of
        //   texels to skip per line, which we don't need.
        rdpq_set_texture_image_raw(0, PhysicalAddr(tload->tex->buffer), FMT_RGBA16, tload->tex->width, tload->tex->height);
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_RGBA16, tload->tmem_addr, 0, 0);
        rdpq_set_tile(tload->tile, surface_get_format(tload->tex), tload->tmem_addr, tload->rect.tmem_pitch, tload->tlut);
        tload->load_mode = TEX_LOAD_BLOCK;
    }

    s0 &= ~1; s1 = (s1+1) & ~1;
    rdpq_load_block(RDPQ_TILE_INTERNAL, s0/2, t0, tload->rect.num_texels/4, tload->rect.tmem_pitch);
    rdpq_set_tile_size(tload->tile, s0, t0, s1, t1);
}

static void texload_tile_4bpp(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    if (tload->load_mode != TEX_LOAD_TILE) {
        rdpq_set_texture_image_raw(0, PhysicalAddr(tload->tex->buffer), FMT_CI8, tload->tex->stride, tload->tex->height);
        rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_CI8, tload->tmem_addr, tload->rect.tmem_pitch, 0);
        rdpq_set_tile(tload->tile, surface_get_format(tload->tex), tload->tmem_addr, tload->rect.tmem_pitch, tload->tlut);
    }

    s0 &= ~1; s1 = (s1+1) & ~1;
    rdpq_load_tile(RDPQ_TILE_INTERNAL, s0/2, t0, s1/2, t1);
    rdpq_set_tile_size(tload->tile, s0, t0, s1, t1);
}

static void texload_block(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    tex_format_t fmt = surface_get_format(tload->tex);

    if (tload->load_mode != TEX_LOAD_BLOCK) {
        // Use LOAD_BLOCK if we are uploading a full texture. Notice the weirdness of LOAD_BLOCK:
        // * SET_TILE must be configured with tmem_pitch=0, as that is weirdly used as the number of
        //   texels to skip per line, which we don't need.
        rdpq_set_texture_image_raw(0, PhysicalAddr(tload->tex->buffer), fmt, tload->tex->width, tload->tex->height);
        rdpq_set_tile(RDPQ_TILE_INTERNAL, fmt, tload->tmem_addr, 0, 0);
        rdpq_set_tile(tload->tile, fmt, tload->tmem_addr, tload->rect.tmem_pitch, tload->tlut);
        tload->load_mode = TEX_LOAD_BLOCK;
    }

    rdpq_load_block(RDPQ_TILE_INTERNAL, s0, t0, tload->rect.num_texels, (fmt == FMT_RGBA32) ? tload->rect.tmem_pitch*2 : tload->rect.tmem_pitch);
    rdpq_set_tile_size(tload->tile, s0, t0, s1, t1);
}

static void texload_tile(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    tex_format_t fmt = surface_get_format(tload->tex);

    if (tload->load_mode != TEX_LOAD_TILE) {
        rdpq_set_texture_image(tload->tex);
        rdpq_set_tile(tload->tile, fmt, tload->tmem_addr, tload->rect.tmem_pitch, tload->tlut);
        tload->load_mode = TEX_LOAD_TILE;
    }

    rdpq_load_tile(tload->tile, s0, t0, s1, t1);
}

static tex_loader_t tex_loader_init(rdpq_tile_t tile, surface_t *tex) {
    bool is_4bpp = (surface_get_format(tex) & 3)== 0;
    return (tex_loader_t){
        .tex = tex,
        .tile = tile,
        .load_block = is_4bpp ? texload_block_4bpp : texload_block,
        .load_tile = is_4bpp ? texload_tile_4bpp : texload_tile,
    };
}

int rdpq_tex_load_sub_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut, int s0, int t0, int s1, int t1)
{
    tex_loader_t tload = tex_loader_init(tile, tex);
    tex_loader_set_tlut(&tload, tlut);
    tex_loader_set_tmem_addr(&tload, tmem_addr);
    return tex_loader_load(&tload, s0, t0, s1, t1);
#   
}

int rdpq_tex_load_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut)
{
    return rdpq_tex_load_sub_ci4(tile, tex, tmem_addr, tlut, 0, 0, tex->width, tex->height);
}

int rdpq_tex_load_sub(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int s0, int t0, int s1, int t1)
{
    tex_loader_t tload = tex_loader_init(tile, tex);
    tex_loader_set_tmem_addr(&tload, tmem_addr);
    return tex_loader_load(&tload, s0, t0, s1, t1);
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
    int tile_h = (fmt == FMT_CI4 || fmt == FMT_CI8) ? 2048 / tmem_pitch : 4096 / tmem_pitch;
    int s0 = 0, t0 = 0;
    
    // Initial configuration of texloader
    tex_loader_t tload = tex_loader_init(tile, tex);

    // Go through the surface
    while (t0 < tex->height) 
    {
        // Calculate the height of the current strip
        int s1 = tex->width;
        int t1 = MIN(t0 + tile_h, tex->height);

        // Load the current strip
        tex_loader_load(&tload, s0, t0, s1, t1);

        // Call the draw callback for this strip
        draw_cb(tile, s0, t0, s1, t1);

        // Move to the next strip
        t0 = t1;
    }
}

void rdpq_tex_blit(rdpq_tile_t tile, surface_t *tex, int x0, int y0, int screen_width, int screen_height)
{
    float scalex = (float)screen_width / (float)tex->width;
    float scaley = (float)screen_height / (float)tex->height;
    float dsdx = 1.0f / scalex;
    float dtdy = 1.0f / scaley;

    void draw_cb(rdpq_tile_t tile, int s0, int t0, int s1, int t1)
    {
        rdpq_texture_rectangle(tile, 
            x0 + s0 * scalex, y0 + t0 * scaley,
            x0 + s1 * scalex, y0 + t1 * scaley,
            s0, t0, dsdx, dtdy);
    }

    tex_draw_split(tile, tex, draw_cb);
}

void rdpq_tex_load_tlut(uint16_t *tlut, int color_idx, int num_colors)
{
    rdpq_set_texture_image_raw(0, PhysicalAddr(tlut), FMT_RGBA16, num_colors, 1);
    rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_I4, TMEM_PALETTE_ADDR + color_idx*16*2*4, num_colors, 0);
    rdpq_load_tlut(RDPQ_TILE_INTERNAL, color_idx, num_colors);
}
