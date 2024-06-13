/**
 * @file rdpq_tex.c
 * @brief RDP Command queue: texture loading
 * @ingroup rdp
 */

///@cond
#define _GNU_SOURCE    // Activate GNU extensions in math.h (sincosf)
///@endcond
#include "rdpq.h"
#include "rdpq_tri.h"
#include "rdpq_rect.h"
#include "rdpq_tex.h"
#include "rdpq_tex_internal.h"
#include "utils.h"
#include "fmath.h"
#include <math.h>

/** @brief Non-zero if we are doing a multi-texture upload */
typedef struct rdpq_multi_upload_s {
    int  used;
    int  bytes;
    int  limit;
} rdpq_multi_upload_t;
static rdpq_multi_upload_t multi_upload;
/** @brief Information on last image uploaded we are doing a multi-texture upload */
tex_loader_t last_tload;

/** @brief Address in TMEM where the palettes must be loaded */
#define TMEM_PALETTE_ADDR   0x800

/// @brief Calculates the first power of 2 that is equal or larger than size
/// @param x input in units
/// @return Power of 2 that is equal or larger than x
int integer_to_pow2(int x){
    int res = 0;
    while(1<<res < x) res++;
    return res;
}

static void texload_recalc_tileparms(tex_loader_t *tload)
{
    const rdpq_texparms_t *parms = tload->texparms;
    int width = tload->rect.width;
    int height = tload->rect.height;

    assertf((width > 0 && height > 0), 
        "The sub rectangle of a texture can't be of negative size (%i,%i)", width, height);
    assertf(parms->s.repeats >= 0 && parms->t.repeats >= 0,
        "Repetition count (%f, %f) cannot be negative", parms->s.repeats, parms->t.repeats);
    
    int xmask = 0;
    int ymask = 0;

    rdpq_tileparms_t *res = &tload->tileparms;
    res->palette = parms->palette;

    if(parms->s.repeats > 1){
        xmask = integer_to_pow2(width);
        assertf(1<<xmask == width, 
            "Mirror and/or wrapping on S axis allowed only with X dimension (%i tx) = power of 2", width);
        res->s.mirror = parms->s.mirror;
    }
    if(parms->t.repeats > 1){
        ymask = integer_to_pow2(height);
        assertf(1<<ymask == height, 
            "Mirror and/or wrapping on T axis allowed only with Y dimension (%i tx) = power of 2", height);
        res->t.mirror = parms->t.mirror;
    }

    res->s.shift  = parms->s.scale_log;
    res->t.shift  = parms->t.scale_log;
    if(parms->s.repeats * width < 1024) res->s.clamp = true;
    else res->s.clamp = false;
    if(parms->t.repeats * height < 1024) res->t.clamp = true;
    else res->t.clamp = false;

    assertf((!res->s.clamp || parms->s.translate >= 0), 
        "Translation S (%f) cannot be negative with active clamping", parms->s.translate);
    assertf((!res->t.clamp || parms->t.translate >= 0), 
        "Translation T (%f) cannot be negative with active clamping", parms->t.translate);

    float srepeats = parms->s.repeats;
    float trepeats = parms->t.repeats;
    if(F2I(srepeats) > 0) {
        res->s.mask = xmask;
    } else
        srepeats = 1;
    if(F2I(parms->t.repeats) > 0) {
        res->t.mask = ymask;
    } else 
        trepeats = 1;

    tload->rect.s0fx = parms->s.translate*4;
    tload->rect.t0fx = parms->t.translate*4;
    tload->rect.s1fx = (parms->s.translate + (srepeats - 1) * width * res->s.clamp)*4;
    tload->rect.t1fx = (parms->t.translate + (trepeats - 1) * height * res->s.clamp)*4;
}


/** @brief Precomputes everything required for loading the rect (s0,t0)-(s1,t1) 
 * 
 * This function prepares for a new TMEM load for the specified rectangle. Since it is very
 * common to invoke multiple different rects with similar width and/or height, this function
 * tries to compute only what needs to be done with respect the previous load. Specifically:
 * 
 * * If the width of the rectangle changed, we need to compute the TMEM pitch, and verifies
 *   whether we can use LOAD_BLOCK. We can check basic constaints with the width, but there
 *   will be a maximum number of lines that can be transferred with LOAD_BLOCK.
 * * If the height of the rectangle changed, we can calculate the total number of texels
 *   and complete the LOAD_BLOCK calculation by verifying that the height is within the
 *   maximum allowed range.
 */
static int texload_set_rect(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    // For now, we don't support clamping/mirroring, as that would require
    // additional logic here to select the proper pixels
    assertf(s1 <= tload->tex->width && t1 <= tload->tex->height, "rdpq tex loader does not support clamping/mirroring");

    tex_format_t fmt = surface_get_format(tload->tex);
    if (TEX_FORMAT_BITDEPTH(fmt) == 4) {
        s0 &= ~1; s1 = (s1+1) & ~1;
    }

    int width = s1 - s0;
    int height = t1 - t0;

    if (width != tload->rect.width || height != tload->rect.height) {
        if (width != tload->rect.width) {
            // Calculate he new pitch in TMEM (in bytes). Notice that RGBA32 is special
            // as texture data is split in two halves, so the pitch can be halved.
            int pitch_shift = fmt == FMT_RGBA32 ? 1 : 0;
            int stride_mask = fmt == FMT_RGBA32 ? 15 : 7;
            tload->rect.tmem_pitch = ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, width) >> pitch_shift, 8);

            // Verify whether we can use LOAD_BLOCK. The conditions we can verify just by looking at the
            // width are:
            //   * The rectangle to load cover the whole texture horizontally, and the texture does not
            //     contain extraneous data at the end of each line.
            //   * The width of the texture is a multiple of 8 bytes (or 16 bytes, in case of RGBA32).
            bool can_load_block_width =
                TEX_FORMAT_PIX2BYTES(fmt, width) == tload->tex->stride &&
                (tload->tex->stride & stride_mask) == 0;

            if (can_load_block_width) {
                // If the requirements are satisfied, we need to compute the maximum number of lines
                // that can be loaded with LOAD_BLOCK. In fact, RDP uses fixed point precision;
                // the DXT parameter in the LOAD_BLOCK command is a 1.10 fixed point number, so
                // there is a precision error after a certain number of lines that can cause artifacts.

                // We precomputed a table that stores the maximum number of lines for each possible width.
                // (actually, for each possible pitch / 8, given that the pitch must be a multiple of 8).
                // This table was generated by the following Python code:
                //
                //            # (thanks to glank for describing a neat way to find the error in dxt per line)
                //            words_per_line = line_bytes // 8
                //            dxt = (1 << 11) / words_per_line
                //            # dxt is rounded up, so the error is 1 - the fractional part of dxt
                //            err = 1.0 - math.modf(dxt)[0]
                //            # the error per line is the error per 64-bit word * the number of words
                //            err_per_line = words_per_line * err
                //            # the maximum number of lines before this becomes an issue is
                //            max_lines = math.floor(dxt / err_per_line)
                //
                // The table doesn't contain the first 11 entries as they are all unlimited (that is, the error does not happen
                // within the 4K TMEM size).
                static const uint8_t block_max_lines_table[] = { 20, 42, 26, 14, 19, 32, 13, 28, 26, 8, 9, 4, 4, 5, 20, 13, 18, 3, 6, 3, 2, 16, 2, 2, 3, 14, 2, 13, 2, 1, 12, 4, 2, 2, 2, 2, 2, 2, 4, 10, 0, 1, 2, 9, 0, 1, 8, 0, 2, 0, 1, 0, 1, 8, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 0, 6, 0, 0, 4, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
 
                int words = tload->rect.tmem_pitch / 8;
                if (words >= 11)
                    tload->rect.block_max_lines = block_max_lines_table[words - 11];
                else
                    tload->rect.block_max_lines = 4096;  // arbitrary high number, it will be limited by TMEM size anyway
            } else {
                tload->rect.block_max_lines = 0;
            }
            
            // Invalidate the current load mode. This will force the next load_tile function to reissue
            // the RDP configuration.
            tload->load_mode = TEX_LOAD_UNKNOWN;
        }

        // If the height changed, complete filling the rect structure,
        // and calculate whether we can really use LOAD_BLOCK or not.
        int tmem_size = (fmt == FMT_RGBA32 || fmt == FMT_CI4 || fmt == FMT_CI8) ? 2048 : 4096;
        assertf(height * tload->rect.tmem_pitch <= tmem_size,
            "A rectangle of size %dx%d format %s is too big to fit in TMEM", width, height, tex_format_name(fmt));
        tload->rect.width = width;
        tload->rect.height = height;
        tload->rect.num_texels = width * height;
        tload->rect.can_load_block = height <= tload->rect.block_max_lines;
        tload->rect.s0fx = tload->rect.s1fx = tload->rect.t0fx = tload->rect.t1fx = 0;
        if (tload->texparms) texload_recalc_tileparms(tload);
    }
    return tload->rect.tmem_pitch * height;
}

static void texload_block_4bpp(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    rdpq_tile_t tile_internal = (tload->tile + 1) & 7;
    if (tload->load_mode != TEX_LOAD_BLOCK) {
        // Use LOAD_BLOCK if we are uploading a full texture. Notice the weirdness of LOAD_BLOCK:
        // * SET_TILE must be configured with tmem_pitch=0, as that is weirdly used as the number of
        //   texels to skip per line, which we don't need.
        assertf(ROUND_UP(tload->tex->width, 2) % 4 == 0, "Internal Error: invalid width for LOAD_BLOCK (%d)", tload->tex->width);
        rdpq_set_texture_image_raw(surface_get_placeholder_index(tload->tex), PhysicalAddr(tload->tex->buffer), FMT_RGBA16, (tload->tex->width+1)/4, tload->tex->height);
        rdpq_set_tile(tile_internal, FMT_RGBA16, tload->tmem_addr, 0, NULL);
        rdpq_set_tile(tload->tile, surface_get_format(tload->tex), tload->tmem_addr, tload->rect.tmem_pitch, &(tload->tileparms));
        tload->load_mode = TEX_LOAD_BLOCK;
    }

    s0 &= ~1; s1 = (s1+1) & ~1;
    rdpq_load_block(tile_internal, s0/2, t0, tload->rect.num_texels/4, tload->rect.tmem_pitch);

    s0 = s0*4 + tload->rect.s0fx;
    t0 = t0*4 + tload->rect.t0fx;
    s1 = s1*4 + tload->rect.s1fx;
    t1 = t1*4 + tload->rect.t1fx;
    rdpq_set_tile_size_fx(tload->tile, s0, t0, s1, t1);
}

static void texload_block_8bpp(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    rdpq_tile_t tile_internal = (tload->tile + 1) & 7;
    tex_format_t fmt = surface_get_format(tload->tex);
    if (tload->load_mode != TEX_LOAD_BLOCK) {
        // Use LOAD_BLOCK if we are uploading a full texture. Notice the weirdness of LOAD_BLOCK:
        // * SET_TILE must be configured with tmem_pitch=0, as that is weirdly used as the number of
        //   texels to skip per line, which we don't need.
        rdpq_set_texture_image_raw(surface_get_placeholder_index(tload->tex), PhysicalAddr(tload->tex->buffer), FMT_RGBA16, tload->tex->width/2, tload->tex->height);
        rdpq_set_tile(tile_internal, FMT_RGBA16, tload->tmem_addr, 0, NULL);
        rdpq_set_tile(tload->tile, fmt, tload->tmem_addr, tload->rect.tmem_pitch, &(tload->tileparms));
        tload->load_mode = TEX_LOAD_BLOCK;
    }

    rdpq_load_block(tile_internal, s0/2, t0, tload->rect.num_texels/2, tload->rect.tmem_pitch);

    s0 = s0*4 + tload->rect.s0fx;
    t0 = t0*4 + tload->rect.t0fx;
    s1 = s1*4 + tload->rect.s1fx;
    t1 = t1*4 + tload->rect.t1fx;
    rdpq_set_tile_size_fx(tload->tile, s0, t0, s1, t1);
}

static void texload_block(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    rdpq_tile_t tile_internal = (tload->tile + 1) & 7;
    tex_format_t fmt = surface_get_format(tload->tex);
    if (tload->load_mode != TEX_LOAD_BLOCK) {
        // Use LOAD_BLOCK if we are uploading a full texture. Notice the weirdness of LOAD_BLOCK:
        // * SET_TILE must be configured with tmem_pitch=0, as that is weirdly used as the number of
        //   texels to skip per line, which we don't need.
        rdpq_set_texture_image_raw(surface_get_placeholder_index(tload->tex), PhysicalAddr(tload->tex->buffer), fmt, tload->tex->width, tload->tex->height);
        rdpq_set_tile(tile_internal, fmt, tload->tmem_addr, 0, NULL);
        rdpq_set_tile(tload->tile, fmt, tload->tmem_addr, tload->rect.tmem_pitch,  &(tload->tileparms));
        tload->load_mode = TEX_LOAD_BLOCK;
    }

    rdpq_load_block(tile_internal, s0, t0, tload->rect.num_texels, (fmt == FMT_RGBA32) ? tload->rect.tmem_pitch*2 : tload->rect.tmem_pitch);

    s0 = s0*4 + tload->rect.s0fx;
    t0 = t0*4 + tload->rect.t0fx;
    s1 = s1*4 + tload->rect.s1fx;
    t1 = t1*4 + tload->rect.t1fx;
    rdpq_set_tile_size_fx(tload->tile, s0, t0, s1, t1);
}

static void texload_tile_4bpp(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    rdpq_tile_t tile_internal = (tload->tile + 1) & 7;
    if (tload->load_mode != TEX_LOAD_TILE) {
        rdpq_set_texture_image_raw(surface_get_placeholder_index(tload->tex), PhysicalAddr(tload->tex->buffer), FMT_I8, tload->tex->stride, tload->tex->height);
        rdpq_set_tile(tile_internal, FMT_I8, tload->tmem_addr, tload->rect.tmem_pitch, NULL);
        rdpq_set_tile(tload->tile, surface_get_format(tload->tex), tload->tmem_addr, tload->rect.tmem_pitch, &(tload->tileparms));
        tload->load_mode = TEX_LOAD_TILE;
    }

    s0 &= ~1; s1 = (s1+1) & ~1;
    rdpq_load_tile(tile_internal, s0/2, t0, s1/2, t1);
    s0 = s0*4 + tload->rect.s0fx;
    t0 = t0*4 + tload->rect.t0fx;
    s1 = s1*4 + tload->rect.s1fx;
    t1 = t1*4 + tload->rect.t1fx;
    rdpq_set_tile_size_fx(tload->tile, s0, t0, s1, t1);
}

static void texload_tile(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    tex_format_t fmt = surface_get_format(tload->tex);
    if (tload->load_mode != TEX_LOAD_TILE) {
        rdpq_set_texture_image(tload->tex);
        rdpq_set_tile(tload->tile, fmt, tload->tmem_addr, tload->rect.tmem_pitch, &(tload->tileparms));
        tload->load_mode = TEX_LOAD_TILE;
    }

    rdpq_load_tile(tload->tile, s0, t0, s1, t1);
    s0 = s0*4 + tload->rect.s0fx;
    t0 = t0*4 + tload->rect.t0fx;
    s1 = s1*4 + tload->rect.s1fx;
    t1 = t1*4 + tload->rect.t1fx;
    rdpq_set_tile_size_fx(tload->tile, s0, t0, s1, t1);
}


static void texload_settile(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    tex_format_t fmt = surface_get_format(tload->tex);

    rdpq_set_tile(tload->tile, fmt, tload->tmem_addr, tload->rect.tmem_pitch, &(tload->tileparms));

    s0 = s0*4 + tload->rect.s0fx;
    t0 = t0*4 + tload->rect.t0fx;
    s1 = s1*4 + tload->rect.s1fx;
    t1 = t1*4 + tload->rect.t1fx;
    rdpq_set_tile_size_fx(tload->tile, s0, t0, s1, t1);
}

///@cond
// Tex loader API, not yet documented
int tex_loader_load(tex_loader_t *tload, int s0, int t0, int s1, int t1)
{
    assertf(s0 <= s1, "Invalid texture load: s0:%d s1:%d", s0, s1);
    assertf(t0 <= t1, "Invalid texture load: t0:%d t1:%d", t0, t1);
    int mem = texload_set_rect(tload, s0, t0, s1, t1);
    if (tload->rect.can_load_block && (t0 & 1) == 0)
        tload->load_block(tload, s0, t0, s1, t1);
    else
        tload->load_tile(tload, s0, t0, s1, t1);
    return mem;
}

tex_loader_t tex_loader_init(rdpq_tile_t tile, const surface_t *tex) {
    int bpp = TEX_FORMAT_BITDEPTH(surface_get_format(tex));
    bool is_4bpp = bpp == 4;
    bool is_8bpp = bpp == 8;
    return (tex_loader_t){
        .tex = tex,
        .tile = tile,
        .load_block = is_4bpp ? texload_block_4bpp : (is_8bpp ? texload_block_8bpp : texload_block),
        .load_tile = is_4bpp ? texload_tile_4bpp : texload_tile,
    };
}

void tex_loader_set_texparms(tex_loader_t *tload, const rdpq_texparms_t *parms)
{
    tload->texparms = parms;
    tload->rect.width = tload->rect.height = 0; // Force recalculation of rect-dependent paramaters
}

void tex_loader_set_tmem_addr(tex_loader_t *tload, int tmem_addr)
{
    tload->tmem_addr = tmem_addr;
    tload->load_mode = TEX_LOAD_UNKNOWN;
}

int tex_loader_calc_max_height(tex_loader_t *tload, int width)
{
    texload_set_rect(tload, 0, 0, width, 1);

    tex_format_t fmt = surface_get_format(tload->tex);
    int tmem_size = (fmt == FMT_RGBA32 || fmt == FMT_CI4 || fmt == FMT_CI8) ? 2048 : 4096;
    return tmem_size / tload->rect.tmem_pitch;
}

///@endcond

int rdpq_tex_upload_sub(rdpq_tile_t tile, const surface_t *tex, const rdpq_texparms_t *parms, int s0, int t0, int s1, int t1)
{
    last_tload = tex_loader_init(tile, tex);
    if (parms) tex_loader_set_texparms(&last_tload, parms);
    
    if (multi_upload.used) {
        assertf(parms == NULL || parms->tmem_addr == 0, "Do not specify a TMEM address while doing a multi-texture upload");
        tex_loader_set_tmem_addr(&last_tload, RDPQ_AUTOTMEM);
    } else {
        tex_loader_set_tmem_addr(&last_tload, parms ? parms->tmem_addr : 0);
    }

    int nbytes = tex_loader_load(&last_tload, s0, t0, s1, t1);

    if (multi_upload.used) {
        rdpq_set_tile_autotmem(nbytes);
        multi_upload.bytes += nbytes;

        #ifndef NDEBUG
        // Do a best-effort check to make sure we don't exceed TMEM size. This is not 100%
        // guaranteed to catch all cases: if a texture is uploaded via block playback, we will
        // not know about its size. Anyway, the RSP will also do check and trigger a RSP assert,
        // with the only gotcha that there will be no traceback for it.
        tex_format_t fmt = surface_get_format(tex);
        if (fmt == FMT_CI4 || fmt == FMT_CI8 || fmt == FMT_RGBA32 || fmt == FMT_YUV16)
            multi_upload.limit = 2048;
        assertf(multi_upload.bytes <= multi_upload.limit, "Multi-texture upload exceeded TMEM size");
        #endif
    }

    return nbytes;
}

int rdpq_tex_upload(rdpq_tile_t tile, const surface_t *tex, const rdpq_texparms_t *parms)
{
    return rdpq_tex_upload_sub(tile, tex, parms, 0, 0, tex->width, tex->height);
}

int rdpq_tex_reuse_sub(rdpq_tile_t tile, const rdpq_texparms_t *parms, int s0, int t0, int s1, int t1)
{
    assertf(multi_upload.used, "Reusing existing texture needs to be done through multi-texture upload");
    assertf(last_tload.tex, "Reusing existing texture is not possible without uploading at least one texture first");  
    assertf(parms == NULL || parms->tmem_addr == 0, "Do not specify a TMEM address while reusing an existing texture");

    // Check if just copying a tile descriptor is enough
    if(!s0 && !t0 && s1 == last_tload.rect.width && t1 == last_tload.rect.height){
        if(!parms){
            last_tload.tile = tile;
            last_tload.tmem_addr = RDPQ_AUTOTMEM_REUSE(0);
            texload_settile(&last_tload, s0, t0, s1, t1);
            return 0;
        }
    }

    // Make a new texloader to a new sub-rect
    tex_loader_t tload = last_tload;
    
    assertf(s0 >= 0 && t0 >= 0 && s1 <= tload.rect.width && t1 <= tload.rect.height, "Sub coordinates (%i,%i)-(%i,%i) must be within bounds of the texture reused (%ix%i)", s0, t0, s1, t1,  tload.rect.width, tload.rect.height);
    assertf(t0 % 2 == 0, "t0=%i must be in multiples of 2 pixels", t0);

    tex_format_t fmt = surface_get_format(tload.tex);
    int tmem_offset = TEX_FORMAT_PIX2BYTES(fmt, s0);

    assertf(tmem_offset % 8 == 0, "Due to 8-byte texture alignment, for %s format, s0=%i must be in multiples of %i pixels", tex_format_name(fmt), s0, TEX_FORMAT_BYTES2PIX(fmt, 8));
    
    tmem_offset += tload.rect.tmem_pitch*t0;
    tload.tmem_addr = RDPQ_AUTOTMEM_REUSE(tmem_offset);

    if(parms) tload.texparms = parms;
    int subwidth = s1 - s0, subheight = t1 - t0;
    tload.rect.width = subwidth;
    tload.rect.height = subheight;
    texload_recalc_tileparms(&tload);
    
    tload.tile = tile;
    texload_settile(&tload, 0, 0, subwidth, subheight);

    return 0;
}

int rdpq_tex_reuse(rdpq_tile_t tile, const rdpq_texparms_t *parms)
{
    return rdpq_tex_reuse_sub(tile, parms, 0, 0, last_tload.rect.width, last_tload.rect.height);
}

/** 
 * @brief Implement large_tex_draw protocol via the texloader
 * 
 * This is the most generic implementation, as using the texloader allows to
 * support any texture of any size and any format.
 */
static void ltd_texloader(rdpq_tile_t tile, const surface_t *tex, int s0, int t0, int s1, int t1, 
    void (*draw_cb)(rdpq_tile_t tile, int s0, int t0, int s1, int t1), bool filtering)
{
    // The most efficient way to split a large surface is to load it in horizontal strips,
    // whose height maximizes TMEM usage. The last strip might be smaller than the others.

    // Initial configuration of texloader
    tex_loader_t tload = tex_loader_init(tile, tex);

    // Calculate the optimal height for a strip, based on strips of maximum length.
    int tile_h = tex_loader_calc_max_height(&tload, s1 - s0);
    
    // Go through the surface
    while (t0 < t1) 
    {
        // Calculate the height of the current strip
        int tm = filtering ? MAX(t0 - 1, 0) : t0;
        int tn = MIN(tm + tile_h, t1);

        // Load the current strip
        tex_loader_load(&tload, s0, tm, s1, tn);

        // Call the draw callback for this strip
        int tx = (!filtering || tn == t1) ? tn : tn - 1;
        draw_cb(tile, s0, t0, s1, tx);

        // Move to the next strip
        t0 = tx;
    }
}

__attribute__((noinline))
static void tex_xblit_norotate_noscale(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms, large_tex_draw ltd)
{
    rdpq_tile_t tile = parms->tile;
    int src_width = parms->width ? parms->width : surf->width;
    int src_height = parms->height ? parms->height : surf->height;
    int os0 = parms->s0;
    int ot0 = parms->t0;
    int os1 = os0 + src_width;
    int ot1 = ot0 + src_height;
    bool flip_x = parms->flip_x;
    bool flip_y = parms->flip_y;
    x0 -= os0 + parms->cx;
    y0 -= ot0 + parms->cy;

    void draw_cb(rdpq_tile_t tile, int s0, int t0, int s1, int t1)
    {
        int ks0 = s0, kt0 = t0, ks1 = s1, kt1 = t1;

        if (flip_x) { ks0 = os1 - s0 + os0 - 1; ks1 = os1 - s1 + os0 - 1; }
        if (flip_y) { kt0 = ot1 - t0 + ot0 - 1; kt1 = ot1 - t1 + ot0 - 1; }

        rdpq_texture_rectangle(tile, x0 + ks0, y0 + kt0, x0 + ks1, y0 + kt1, s0, t0);
    }

    (*ltd)(tile, surf, os0, ot0, os1, ot1, draw_cb, parms->filtering);
}

__attribute__((noinline))
static void tex_xblit_norotate(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms, large_tex_draw ltd)
{
    rdpq_tile_t tile = parms->tile;
    int src_width = parms->width ? parms->width : surf->width;
    int src_height = parms->height ? parms->height : surf->height;
    int os0 = parms->s0;
    int ot0 = parms->t0;
    int os1 = os0 + src_width;
    int ot1 = ot0 + src_height;
    int cx = parms->cx + os0;
    int cy = parms->cy + ot0;
    float scalex = parms->scale_x == 0 ? 1.0f : parms->scale_x;
    float scaley = parms->scale_y == 0 ? 1.0f : parms->scale_y;

    float mtx[3][2] = {
        { scalex, 0 },
        { 0, scaley },
        { x0 - cx * scalex,
          y0 - cy * scaley }
    };

    void draw_cb(rdpq_tile_t tile, int s0, int t0, int s1, int t1)
    {
        int ks0 = s0, kt0 = t0, ks1 = s1, kt1 = t1;

        if (parms->flip_x) { ks0 = os1 - s0 + os0 - 1; ks1 = os1 - s1 + os0 - 1;  }
        if (parms->flip_y) { kt0 = ot1 - t0 + ot0 - 1; kt1 = ot1 - t1 + ot0 - 1; }

        float k0x = mtx[0][0] * ks0 + mtx[1][0] * kt0 + mtx[2][0];
        float k0y = mtx[0][1] * ks0 + mtx[1][1] * kt0 + mtx[2][1];
        float k2x = mtx[0][0] * ks1 + mtx[1][0] * kt1 + mtx[2][0];
        float k2y = mtx[0][1] * ks1 + mtx[1][1] * kt1 + mtx[2][1];

        rdpq_texture_rectangle_scaled(tile, k0x, k0y, k2x, k2y, s0, t0, s1, t1);
    }

    (*ltd)(tile, surf, os0, ot0, os1, ot1, draw_cb, parms->filtering);
}

__attribute__((noinline))
static void tex_xblit(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms, large_tex_draw ltd)
{
    rdpq_tile_t tile = parms->tile;
    int src_width = parms->width ? parms->width : surf->width;
    int src_height = parms->height ? parms->height : surf->height;
    int os0 = parms->s0;
    int ot0 = parms->t0;
    int os1 = os0 + src_width;
    int ot1 = ot0 + src_height;
    int cx = parms->cx + os0;
    int cy = parms->cy + ot0;
    int nx = parms->nx;
    int ny = parms->ny;
    float scalex = parms->scale_x == 0 ? 1.0f : parms->scale_x;
    float scaley = parms->scale_y == 0 ? 1.0f : parms->scale_y;
    bool flip_x = parms->flip_x;
    bool flip_y = parms->flip_y;

    if (scalex < 0) { flip_x = !flip_x; scalex = -scalex; }
    if (scaley < 0) { flip_y = !flip_y; scaley = -scaley; }

    float sin_theta, cos_theta; 
    fm_sincosf(parms->theta, &sin_theta, &cos_theta);

    float mtx[3][2] = {
        { cos_theta * scalex, -sin_theta * scaley },
        { sin_theta * scalex, cos_theta * scaley },
        { x0 - cx * cos_theta * scalex - cy * sin_theta * scaley,
          y0 + cx * sin_theta * scalex - cy * cos_theta * scaley }
    };

    void draw_cb(rdpq_tile_t tile, int s0, int t0, int s1, int t1)
    {
        int ks0 = s0, kt0 = t0, ks1 = s1, kt1 = t1;

        if (flip_x) { ks0 = os1 - ks0 + os0; ks1 = os1 - ks1 + os0; }
        if (flip_y) { kt0 = ot1 - kt0 + ot0; kt1 = ot1 - kt1 + ot0; }

        float k0x = mtx[0][0] * ks0 + mtx[1][0] * kt0 + mtx[2][0];
        float k0y = mtx[0][1] * ks0 + mtx[1][1] * kt0 + mtx[2][1];
        float k2x = mtx[0][0] * ks1 + mtx[1][0] * kt1 + mtx[2][0];
        float k2y = mtx[0][1] * ks1 + mtx[1][1] * kt1 + mtx[2][1];
        float k1x = mtx[0][0] * ks1 + mtx[1][0] * kt0 + mtx[2][0];
        float k1y = mtx[0][1] * ks1 + mtx[1][1] * kt0 + mtx[2][1];
        float k3x = mtx[0][0] * ks0 + mtx[1][0] * kt1 + mtx[2][0];
        float k3y = mtx[0][1] * ks0 + mtx[1][1] * kt1 + mtx[2][1];

        float v0[5] = { k0x, k0y, s0, t0, 1.0f };
        float v1[5] = { k1x, k1y, s1, t0, 1.0f };
        float v2[5] = { k2x, k2y, s1, t1, 1.0f };
        float v3[5] = { k3x, k3y, s0, t1, 1.0f };
        rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
        rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);
    }

    void draw_cb_multi_rot(rdpq_tile_t tile, int s0, int t0, int s1, int t1)
    {
        int ks0 = s0, kt0 = t0, ks1 = s1, kt1 = t1;
        if (flip_x) { ks0 = os1 - ks0 + os0; ks1 = os1 - ks1 + os0; }
        if (flip_y) { kt0 = ot1 - kt0 + ot0; kt1 = ot1 - kt1 + ot0; }

        assert(s1-s0 == src_width);

        for (int j=0; j<ny; j++) {
            int kkt0 = kt0 + j * src_height;
            int kkt1 = kt1 + j * src_height;

            // rdpq_triangle_strip_begin(&TRIFMT_TEX);

            float kks0 = ks0;
            float kks1 = ks1;
            for (int i=0; i<=nx; i++) {
                float k0x = mtx[0][0] * kks0 + mtx[1][0] * kkt0 + mtx[2][0];
                float k0y = mtx[0][1] * kks0 + mtx[1][1] * kkt0 + mtx[2][1];
                float k2x = mtx[0][0] * kks1 + mtx[1][0] * kkt1 + mtx[2][0];
                float k2y = mtx[0][1] * kks1 + mtx[1][1] * kkt1 + mtx[2][1];
                float k1x = mtx[0][0] * kks1 + mtx[1][0] * kkt0 + mtx[2][0];
                float k1y = mtx[0][1] * kks1 + mtx[1][1] * kkt0 + mtx[2][1];
                float k3x = mtx[0][0] * kks0 + mtx[1][0] * kkt1 + mtx[2][0];
                float k3y = mtx[0][1] * kks0 + mtx[1][1] * kkt1 + mtx[2][1];

                float v0[5] = { k0x, k0y, s0, t0, 1.0f };
                float v1[5] = { k1x, k1y, s1, t0, 1.0f };
                float v2[5] = { k2x, k2y, s1, t1, 1.0f };
                float v3[5] = { k3x, k3y, s0, t1, 1.0f };
                rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
                rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);

                // rdpq_triangle_strip(v0);
                // rdpq_triangle_strip(v3);
                kks0 += src_width;
                kks1 += src_width;
            }
        }
    }

    if (nx || ny) {
        (*ltd)(tile, surf, os0, ot0, os1, ot1, draw_cb_multi_rot, parms->filtering);    
    } else {
        (*ltd)(tile, surf, os0, ot0, os1, ot1, draw_cb, parms->filtering);
    }
}

/** @brief Internal implementation of #rdpq_tex_blit, using a custom large tex loader callback function */
void __rdpq_tex_blit(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms, large_tex_draw ltd)
{
    static const rdpq_blitparms_t default_parms = {0};
    if (!parms) parms = &default_parms;

    // Check which implementation to use, depending on the requested features.
    if (F2I(parms->theta) == 0) {
        if (F2I(parms->scale_x) == 0 && F2I(parms->scale_y) == 0)
                tex_xblit_norotate_noscale(surf, x0, y0, parms, ltd);
            else
                tex_xblit_norotate(surf, x0, y0, parms, ltd);
    } else {
        tex_xblit(surf, x0, y0, parms, ltd);
    }
}

void rdpq_tex_blit(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms)
{
    __rdpq_tex_blit(surf, x0, y0, parms, ltd_texloader);
}

void rdpq_tex_upload_tlut(uint16_t *tlut, int color_idx, int num_colors)
{
    rdpq_set_texture_image_raw(0, PhysicalAddr(tlut), FMT_RGBA16, num_colors, 1);
    rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_I4, TMEM_PALETTE_ADDR + color_idx*2*4, num_colors, NULL);
    rdpq_load_tlut_raw(RDPQ_TILE_INTERNAL, 0, num_colors);
}

void rdpq_tex_multi_begin(void)
{
    // Initialize autotmem engine
    rdpq_set_tile_autotmem(0);
    if (multi_upload.used++ == 0) {
        multi_upload.bytes = 0;
        multi_upload.limit = 4096;
        last_tload.tex = 0;
    }
}

int rdpq_tex_multi_end(void)
{
    rdpq_set_tile_autotmem(-1);
    --multi_upload.used;
    assert(multi_upload.used >= 0);
    return 0;
}
