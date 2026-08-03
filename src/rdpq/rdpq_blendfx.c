/**
 * @file rdpq_blendfx.c
 * @brief BlendFX implementation using the destination-as-texture technique.
 * @ingroup rdpq
 *
 * The public BlendFX API composites a source surface with pixels already in
 * the active color image. This file implements that operation with DESTEX
 * (destination as texture): the source is sampled as TEX0, a recently loaded
 * destination region is sampled as TEX1, and the combiner writes the result
 * back opaquely.
 *
 * ## TMEM layout and chunk sizes
 *
 * TMEM is divided into two fixed 2 KiB regions. The source occupies the upper
 * half and destination feedback occupies the lower half. An RGBA16 feedback
 * window can therefore be at most 32x32 pixels. The general packed-coordinate
 * paths draw cores of at most 30x30 pixels, leaving room for bilinear sampling
 * and the source-coordinate contribution embedded in TEX1 coordinates.
 *
 * Axis-aligned sources that do not fit are split into format-aware cores. The
 * usual core is 30x30 texels; 4-bpp formats use a 29-pixel horizontal core so
 * nibble alignment and the one-texel filter halo still fit. Edge cores shrink
 * to the remaining source region. Destination feedback loads also shrink for
 * safe interior edge chunks, without changing tile setup or adding commands.
 *
 * ## Packed texture coordinates
 *
 * RDP triangles provide one S/T pair to both texture units. DESTEX packs the
 * local source coordinate into the low five bits and the local destination
 * coordinate into the high bits. TEX0 masks the low bits; TEX1 shifts away
 * those bits. The rectangle path uses a related half-resolution packing to
 * fit texture-rectangle derivative ranges. All coordinates are derived from
 * the complete source-to-screen mapping so chunk boundaries do not accumulate
 * transform rounding errors.
 *
 * ## Drawing paths
 *
 * Axis-aligned draws use texture rectangles and can split both source and
 * destination space. Rotation uses textured triangles over the original
 * axis-aligned draw box, with affine source gradients packed together with
 * destination gradients. Rotated and UV-scaled draws require the selected
 * source region to fit as a single resident upload.
 *
 * ## Source residency and recorded blocks
 *
 * Standalone public calls reset the private source cache before and after the
 * draw, making each call independent of prior TMEM contents. A BlendFX multi
 * scope keeps a compatible source resident across calls. When recorded in an
 * RSPQ block, the first upload is part of the block, so every playback remains
 * self-contained.
 *
 * ## Active destination tracking
 *
 * The destination address cannot be baked into a recorded block because the
 * active attachment may change before playback or within the block itself.
 * The RDPQ overlay therefore mirrors the fully resolved SET_COLOR_IMAGE
 * command. A compact RSP command converts that current command into
 * SET_TEXTURE_IMAGE immediately before each destination load.
 *
 * ## Performance invariants
 *
 * Each destination chunk emits one LOAD_SYNC, one LOAD_TILE, and one draw.
 * Partial feedback loads change only LOAD_TILE bounds. Source reuse does not
 * add RDP commands, and all transform planning happens before the chunk loops.
 * Keep these invariants in mind when changing helper boundaries or making
 * apparently generic abstractions around the hot paths.
 */

#include "rdpq.h"
#include "rdpq_blendfx.h"
#include "rdpq_attach.h"
#include "rdpq_mode.h"
#include "rdpq_rect.h"
#include "rdpq_tex.h"
#include "rdpq_tex_internal.h"
#include "rdpq_internal.h"
#include "fmath.h"
#include "utils.h"
#include <math.h>

/** @name DESTEX TMEM and coordinate constants
 * These constants encode the fixed TMEM split and the coordinate-range proof
 * described in the file overview.
 * @{ */
#define DESTEX_SOURCE_TMEM           2048  ///< First byte of the upper source half of TMEM.
#define DESTEX_FEEDBACK_TMEM            0  ///< First byte of the lower feedback half of TMEM.
#define DESTEX_FEEDBACK_SIDE           32  ///< Maximum RGBA16 feedback-window side in pixels.
#define DESTEX_DRAW_CHUNK_MAX          30  ///< Maximum general packed draw-core side in pixels.
#define DESTEX_SOURCE_FILTER_HALO       1  ///< Source texels retained on each side for filtering.
#define DESTEX_FEEDBACK_ALLOWANCE       2  ///< Feedback texels reserved beyond a draw core.
#define DESTEX_PACK_BITS                5  ///< Low coordinate bits assigned to the source.
/// Destination coordinate multiplier used by the triangle path.
#define DESTEX_PACK_SCALE              (1 << DESTEX_PACK_BITS)
#define DESTEX_RECT_SCALE              16  ///< Rectangle-path destination coordinate multiplier.
#define DESTEX_BILINEAR_CENTER          2  ///< Half texel in the RDP's unsigned 10.2 coordinates.
/** @} */

static struct {
    const void *buffer;
    uint16_t stride;
    uint16_t s0, t0, width, height;
    uint16_t tmem_pitch;
    uint8_t format;
    uint8_t tile;
    bool source_tile_ready;
    bool source_rectangle_pack;
    bool valid;
} __attribute__((aligned(16))) destex_residency;

/** Source-space rectangle currently resident or being prepared. */
typedef struct {
    int u0;  ///< Inclusive horizontal start.
    int v0;  ///< Inclusive vertical start.
    int u1;  ///< Exclusive horizontal end.
    int v1;  ///< Exclusive vertical end.
} destex_chunk_t;

static struct {
    tex_format_t format;
    rdpq_tile_t tile;
    rdpq_tile_t load_tile;
    uint8_t width, height;
    bool rectangle_pack;
    bool valid;
} __attribute__((aligned(16))) destex_feedback_tile;

static struct {
    surface_t surface;
    bool valid;
} __attribute__((aligned(16))) destex_feedback_image;

/* Cold source-upload state. Keeping tex_loader_t prepared avoids repeating
   its format/pitch/load-mode setup after an unrelated RDP block invalidates
   TMEM residency. The hot draw state above remains in separate cache lines. */
static struct {
    surface_t surface;
    tex_loader_t loader;
    rdpq_tile_t tile;
    bool valid;
} __attribute__((aligned(16))) destex_source_loader;

/* Source residency is intentionally scoped. Standalone blits are self-contained;
   a multi-blit scope allows compatible consecutive calls to reuse upper TMEM. */
static int destex_multi_depth;

static inline rdpq_tile_t destex_feedback_load_tile(rdpq_tile_t source_tile)
{
    /* TEX1 occupies source_tile+1. Use the following descriptor only for
       framebuffer loads, wrapping exactly like the RDP tile index does. */
    return (rdpq_tile_t)((source_tile + 2) & 7);
}

static inline bool destex_is_ci(tex_format_t fmt)
{
    return fmt == FMT_CI4 || fmt == FMT_CI8;
}

static inline bool destex_source_format_supported(tex_format_t fmt)
{
    /* 4bpp IA4/I4 are intentionally both accepted; the loader aligns their
       source rectangle to complete nibbles before issuing LOAD_TILE. */
    return fmt == FMT_RGBA16 || fmt == FMT_IA16 || fmt == FMT_IA8 ||
        fmt == FMT_I8 || fmt == FMT_IA4 || fmt == FMT_I4;
}

static inline int destex_tmem_pitch(tex_format_t fmt, int width)
{
    int pitch_shift = (fmt == FMT_RGBA32 || fmt == FMT_YUV16) ? 1 : 0;
    return ROUND_UP(TEX_FORMAT_PIX2BYTES(fmt, width) >> pitch_shift, 8);
}

static inline int32_t destex_fixed16(float value)
{
    return (int32_t)fm_floorf(value * 65536.0f);
}

static inline uint16_t destex_fixed5(float value)
{
    int32_t v = (int32_t)fm_floorf(value * 32.0f + 0.5f);
    assertf(v >= 0 && v <= 0xFFFF,
        "BlendFX packed texture coordinate is out of range: %ld", v);
    return (uint16_t)v;
}

static inline int32_t destex_triangle_fixed(float value)
{
    return destex_fixed16(value * DESTEX_PACK_SCALE);
}

static inline int16_t destex_fixed10(float value)
{
    int v = (int)fm_floorf(value * 1024.0f + (value >= 0.0f ? 0.5f : -0.5f));
    assertf(v >= -32768 && v <= 32767,
        "BlendFX packed texture derivative is outside signed 5.10 range: %d", v);
    return (int16_t)v;
}

static inline uint32_t destex_pack_fixed(int32_t s, int32_t t)
{
    return ((uint32_t)s << 16) | ((uint32_t)t & 0xFFFF);
}

static inline uint32_t destex_pack_fixed_hi(int32_t s, int32_t t)
{
    return ((uint32_t)s & 0xFFFF0000) | (((uint32_t)t >> 16) & 0xFFFF);
}

static void destex_set_source_tile(rdpq_tile_t tile, tex_format_t fmt,
    int pitch, int width, int height, bool rectangle_pack)
{
    if (destex_residency.source_tile_ready &&
        destex_residency.tile == tile &&
        destex_residency.format == fmt &&
        destex_residency.tmem_pitch == pitch &&
        destex_residency.width == width &&
        destex_residency.height == height &&
        destex_residency.source_rectangle_pack == rectangle_pack)
        return;

    rdpq_set_tile(tile, fmt, DESTEX_SOURCE_TMEM, pitch,
        &(rdpq_tileparms_t){
            .s = {
                /* Masking extracts the low packed source bits. Clamp would
                   collapse the large packed coordinate to the tile edge. */
                .clamp = false,
                .mask = DESTEX_PACK_BITS,
                .shift = rectangle_pack ? -1 : 0,
            },
            .t = {
                .clamp = false,
                .mask = DESTEX_PACK_BITS,
                .shift = rectangle_pack ? -1 : 0,
            },
        });
    rdpq_set_tile_size(tile, 0, 0, width, height);
    destex_residency.source_tile_ready = true;
    destex_residency.source_rectangle_pack = rectangle_pack;
}

static void destex_set_feedback_tile(rdpq_tile_t source_tile, tex_format_t fmt,
    bool rectangle_pack, int dest_width, int dest_height)
{
    rdpq_tile_t tile = (rdpq_tile_t)(source_tile + 1);
    rdpq_tile_t load_tile = destex_feedback_load_tile(source_tile);
    int width = MIN(dest_width, DESTEX_FEEDBACK_SIDE);
    int height = MIN(dest_height, DESTEX_FEEDBACK_SIDE);
    if (destex_feedback_tile.valid &&
        destex_feedback_tile.tile == tile &&
        destex_feedback_tile.load_tile == load_tile &&
        destex_feedback_tile.format == fmt &&
        destex_feedback_tile.width == width &&
        destex_feedback_tile.height == height &&
        destex_feedback_tile.rectangle_pack == rectangle_pack)
        return;

    int pitch = destex_tmem_pitch(fmt, DESTEX_FEEDBACK_SIDE);
    /* LOAD_TILE and TEX1 use separate descriptors. The load coordinates can
       therefore change without replacing the descriptor sampled by a draw. */
    rdpq_set_tile(load_tile, fmt, DESTEX_FEEDBACK_TMEM, pitch, NULL);
    rdpq_set_tile(tile, fmt, DESTEX_FEEDBACK_TMEM, pitch,
        &(rdpq_tileparms_t){
            .s = {
                .clamp = true,
                .mask = 0,
                .shift = rectangle_pack ? 4 : DESTEX_PACK_BITS,
            },
            .t = {
                .clamp = true,
                .mask = 0,
                .shift = rectangle_pack ? 4 : DESTEX_PACK_BITS,
            },
        });
    rdpq_set_tile_size_fx(tile,
        DESTEX_BILINEAR_CENTER, DESTEX_BILINEAR_CENTER,
        width * 4 + DESTEX_BILINEAR_CENTER,
        height * 4 + DESTEX_BILINEAR_CENTER);
    destex_feedback_tile = (typeof(destex_feedback_tile)){
        .format = fmt,
        .tile = tile,
        .load_tile = load_tile,
        .width = width,
        .height = height,
        .rectangle_pack = rectangle_pack,
        .valid = true,
    };
}

static void destex_set_feedback_image(const surface_t *dest)
{
    const surface_t *cached = &destex_feedback_image.surface;
    if (destex_feedback_image.valid && cached->buffer == dest->buffer &&
        cached->flags == dest->flags && cached->width == dest->width &&
        cached->height == dest->height && cached->stride == dest->stride)
        return;

    /* Resolve from ordered RDPQ state at execution time in both immediate and
       recorded paths. This is smaller than a normal fixup command and follows
       framebuffer rotation without consuming a public lookup slot. */
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_write(1, RDPQ_OVL_ID, RDPQ_CMD_SET_TEXTURE_IMAGE_COLOR);
    destex_feedback_image.surface = *dest;
    destex_feedback_image.valid = true;
}

static void destex_load_feedback(rdpq_tile_t load_tile,
    int x0, int y0, int width, int height)
{
    /* The previous draw samples this same half of TMEM. LOAD_SYNC is the one
       required barrier before replacing it; the dedicated load descriptor
       avoids a separate TILE_SYNC. */
    rdpq_sync_load();

    int x1 = x0 + width;
    int y1 = y0 + height;
    int s0 = x0 * 4;
    int t0 = y0 * 4;
    int s1 = x1 * 4;
    int t1 = y1 * 4;
    rdpq_passthrough_write((RDPQ_CMD_LOAD_TILE,
        ((s0 & 0xFFF) << 12) | (t0 & 0xFFF),
        (load_tile << 24) | (((s1 - 4) & 0xFFF) << 12) | ((t1 - 4) & 0xFFF)));
}

static inline void destex_mark_feedback_used(rdpq_tile_t tile)
{
    /* The raw hot loop bypasses RDPQ tracking. Restore the externally visible
       state once, so commands following this blit still receive autosync. */
    __rdpq_autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILE(tile) |
        AUTOSYNC_TILE(tile + 1) |
        AUTOSYNC_TILE(destex_feedback_load_tile(tile)) | AUTOSYNC_TMEMS);
}

static void destex_draw_rectangle(rdpq_tile_t tile,
    int x0, int y0, int x1, int y1,
    int32_t s, int32_t t, int16_t dsdx, int16_t dtdy)
{
    uint16_t fs = destex_fixed5((float)s / 65536.0f);
    uint16_t ft = destex_fixed5((float)t / 65536.0f);
    rdpq_passthrough_write((RDPQ_CMD_TEXTURE_RECTANGLE,
        ((x1 * 4) << 12) | (y1 * 4),
        (tile << 24) | ((x0 * 4) << 12) | (y0 * 4),
        ((uint32_t)fs << 16) | ft,
        ((uint32_t)(uint16_t)dsdx << 16) | (uint16_t)dtdy));
}

static void destex_draw_triangle(rdpq_tile_t tile,
    int x0, int y0, int x1, int y1, int32_t s, int32_t t,
    const uint32_t gradients[4])
{
    int yh = y0 * 4;
    int yl = y1 * 4;
    int ym = (yh + yl) / 2;
    if (__builtin_expect(rspq_block_is_recording(), 0))
        __rdpq_block_reserve(12);
    rspq_write_t cmd = rspq_write_begin(RDPQ_OVL_ID, RDPQ_CMD_TRI_TEX, 24);

    /* Parallel major/minor edges make the triangle cover one rectangle. */
    rspq_write_arg(&cmd, (1 << 23) | (tile << 16) | (yl & 0x3FFF));
    rspq_write_arg(&cmd, ((ym & 0x3FFF) << 16) | (yh & 0x3FFF));
    rspq_write_arg(&cmd, (uint32_t)x1 << 16);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, (uint32_t)x0 << 16);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, (uint32_t)x1 << 16);
    rspq_write_arg(&cmd, 0);

    rspq_write_arg(&cmd, destex_pack_fixed_hi(s, t));
    rspq_write_arg(&cmd, 0x7FFF0000);
    rspq_write_arg(&cmd, gradients[0]);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, destex_pack_fixed(s, t));
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, gradients[1]);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, gradients[2]);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, gradients[2]);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, gradients[3]);
    rspq_write_arg(&cmd, 0);
    rspq_write_arg(&cmd, gradients[3]);
    rspq_write_arg(&cmd, 0);
    rspq_write_end(&cmd);

}

static void destex_upload_source(rdpq_tile_t tile, const surface_t *surf,
    int s0, int t0, int s1, int t1, bool rectangle_pack)
{
    tex_format_t fmt = surface_get_format(surf);
    int load_s0 = s0;
    int load_s1 = s1;
    if (fmt == FMT_I4 || fmt == FMT_IA4) {
        /* LOAD_TILE addresses 4bpp data in whole bytes. Keep the start even;
           tex_loader_load performs the safe exclusive-end rounding itself,
           including for an odd-width surface edge. */
        load_s0 &= ~1;
    }

    int width = load_s1 - load_s0;
    int height = t1 - t0;
    int pitch = destex_tmem_pitch(fmt, width);
    assertf(width <= DESTEX_PACK_SCALE && height <= DESTEX_PACK_SCALE,
        "DESTEX source chunk is too large for packed coordinates: %dx%d",
        width, height);
    assertf(pitch * height <= 2048,
        "DESTEX source chunk does not fit in upper TMEM: %dx%d %s",
        width, height, tex_format_name(fmt));

    /* tex_loader_load changes the texture image and may use tile+1 while
       loading. Invalidate only the hot command-state caches it overwrites. */
    destex_residency.valid = false;
    destex_residency.source_tile_ready = false;
    destex_feedback_tile.valid = false;
    destex_feedback_image.valid = false;

    bool direct_block = (fmt == FMT_RGBA16 || fmt == FMT_IA16) &&
        load_s0 == 0 && load_s1 == surf->width &&
        TEX_FORMAT_PIX2BYTES(fmt, surf->width) == surf->stride &&
        (surf->stride & 7) == 0 && (t0 & 1) == 0;
    if (direct_block) {
        /* This is the common particle case. It is the same LOAD_BLOCK path
           selected by tex_loader, without rebuilding its generic state. */
        rdpq_set_texture_image(surf);
        rdpq_set_tile((rdpq_tile_t)(tile + 1), fmt,
            DESTEX_SOURCE_TMEM, 0, NULL);
        rdpq_load_block((rdpq_tile_t)(tile + 1), load_s0, t0,
            width * height, pitch);
    } else {
        surface_t *cached = &destex_source_loader.surface;
        if (!destex_source_loader.valid ||
            destex_source_loader.tile != tile ||
            cached->buffer != surf->buffer || cached->flags != surf->flags ||
            cached->width != surf->width || cached->height != surf->height ||
            cached->stride != surf->stride)
        {
            *cached = *surf;
            destex_source_loader.loader = tex_loader_init(tile, cached);
            tex_loader_set_tmem_addr(&destex_source_loader.loader,
                DESTEX_SOURCE_TMEM);
            destex_source_loader.tile = tile;
            destex_source_loader.valid = true;
        }
        /* Feedback loading leaves SET_TEXTURE_IMAGE pointing at the destination.
           Keep the cached geometry calculations, but force the loader to rebind
           its source image and load descriptor for every source chunk. */
        destex_source_loader.loader.load_mode = TEX_LOAD_UNKNOWN;
        tex_loader_load(&destex_source_loader.loader,
            load_s0, t0, load_s1, t1);
    }
    destex_set_source_tile(tile, fmt, pitch, width, height, rectangle_pack);
}

static bool destex_surface_is_resident(const surface_t *surf,
    tex_format_t fmt, int s0, int t0, int width, int height, rdpq_tile_t tile)
{
    return destex_residency.valid &&
        destex_residency.buffer == surf->buffer &&
        destex_residency.format == fmt &&
        destex_residency.stride == surf->stride &&
        s0 >= destex_residency.s0 && t0 >= destex_residency.t0 &&
        s0 + width <= destex_residency.s0 + destex_residency.width &&
        t0 + height <= destex_residency.t0 + destex_residency.height &&
        destex_residency.tile == tile;
}

static bool destex_region_fits(tex_format_t fmt, int width, int height)
{
    return width <= DESTEX_PACK_SCALE && height <= DESTEX_PACK_SCALE &&
        destex_tmem_pitch(fmt, width) * height <= 2048;
}

static void destex_make_resident(const surface_t *surf,
    int s0, int t0, int width, int height, rdpq_tile_t tile)
{
    destex_upload_source(tile, surf, s0, t0, s0 + width, t0 + height, false);
    destex_residency = (typeof(destex_residency)){
        .buffer = surf->buffer,
        .format = surface_get_format(surf),
        .stride = surf->stride,
        .s0 = s0, .t0 = t0, .width = width, .height = height,
        .tile = tile,
        .tmem_pitch = destex_tmem_pitch(surface_get_format(surf), width),
        .source_tile_ready = true,
        .source_rectangle_pack = false,
        .valid = true,
    };
}

static int destex_edge(float value)
{
    return (int)fm_floorf(value + 0.5f);
}

static void destex_draw_axis_region(const surface_t *dest,
    rdpq_tile_t tile, const destex_chunk_t *chunk,
    float x_origin, float y_origin, float scale_x, float scale_y,
    bool flip_x, bool flip_y, float source_start_x, float source_start_y,
    float source_center_x, float source_center_y, float uv_scale)
{
    float draw_x0f = x_origin + chunk->u0 * scale_x;
    float draw_x1f = x_origin + chunk->u1 * scale_x;
    float draw_y0f = y_origin + chunk->v0 * scale_y;
    float draw_y1f = y_origin + chunk->v1 * scale_y;
    int draw_x0 = destex_edge(draw_x0f);
    int draw_x1 = destex_edge(draw_x1f);
    int draw_y0 = destex_edge(draw_y0f);
    int draw_y1 = destex_edge(draw_y1f);
    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0)
        return;

    draw_x0 = MAX(draw_x0, 0);
    draw_y0 = MAX(draw_y0, 0);
    draw_x1 = MIN(draw_x1, dest->width);
    draw_y1 = MIN(draw_y1, dest->height);
    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0)
        return;

    /* These values stay constant for every destination chunk covered by this
       resident source region. */
    destex_set_feedback_tile(tile, surface_get_format(dest), true,
        dest->width, dest->height);
    destex_set_feedback_image(dest);

    float sx = flip_x ? -1.0f : 1.0f;
    float sy = flip_y ? -1.0f : 1.0f;
    float source_dx = sx * uv_scale / scale_x;
    float source_dy = sy * uv_scale / scale_y;
    float source_base_x = source_center_x +
        (source_start_x - source_center_x) * uv_scale - draw_x0f * source_dx;
    float source_base_y = source_center_y +
        (source_start_y - source_center_y) * uv_scale - draw_y0f * source_dy;
    int16_t dsdx = destex_fixed10(DESTEX_RECT_SCALE + source_dx * 0.5f);
    int16_t dtdy = destex_fixed10(DESTEX_RECT_SCALE + source_dy * 0.5f);

    /* Partial loads assume the packed source contribution to TEX1 stays in
       [0,1). Normal source chunks satisfy this by construction, including
       flips. Keep the old full feedback window for UV mappings that leave the
       five-bit source field, rather than adding per-chunk extrema math. */
    float source_first_x = source_base_x + draw_x0 * source_dx;
    float source_last_x = source_base_x + (draw_x1 - 1) * source_dx;
    float source_first_y = source_base_y + draw_y0 * source_dy;
    float source_last_y = source_base_y + (draw_y1 - 1) * source_dy;
    float source_min_x = source_first_x < source_last_x ?
        source_first_x : source_last_x;
    float source_max_x = source_first_x > source_last_x ?
        source_first_x : source_last_x;
    float source_min_y = source_first_y < source_last_y ?
        source_first_y : source_last_y;
    float source_max_y = source_first_y > source_last_y ?
        source_first_y : source_last_y;
    bool partial_x_safe = source_min_x >= 0.0f &&
        source_max_x < DESTEX_PACK_SCALE;
    bool partial_y_safe = source_min_y >= 0.0f &&
        source_max_y < DESTEX_PACK_SCALE;

    for (int y = draw_y0; y < draw_y1; y += DESTEX_DRAW_CHUNK_MAX) {
        int y_end = MIN(y + DESTEX_DRAW_CHUNK_MAX, draw_y1);
        int load_height = destex_feedback_tile.height;
        int load_y0 = MIN(y, dest->height - load_height);
        int draw_height = y_end - y;
        if (__builtin_expect(partial_y_safe &&
            draw_height < DESTEX_DRAW_CHUNK_MAX, 0))
        {
            int partial_height = draw_height + DESTEX_FEEDBACK_ALLOWANCE;
            if (partial_height < load_height &&
                y + partial_height <= dest->height)
            {
                load_height = partial_height;
                load_y0 = y;
            }
        }

        for (int x = draw_x0; x < draw_x1; x += DESTEX_DRAW_CHUNK_MAX) {
            int x_end = MIN(x + DESTEX_DRAW_CHUNK_MAX, draw_x1);
            int load_width = destex_feedback_tile.width;
            int load_x0 = MIN(x, dest->width - load_width);
            int draw_width = x_end - x;
            if (__builtin_expect(partial_x_safe &&
                draw_width < DESTEX_DRAW_CHUNK_MAX, 0))
            {
                int partial_width = draw_width + DESTEX_FEEDBACK_ALLOWANCE;
                if (partial_width < load_width &&
                    x + partial_width <= dest->width)
                {
                    load_width = partial_width;
                    load_x0 = x;
                }
            }

            /* Partial edge chunks load only their draw area plus the two-texel
               packed/filter allowance. If that footprint would cross the
               framebuffer edge, retain the old full-window inward shift so
               TEX1's fixed clamp extent never exposes unwritten TMEM. */
            destex_load_feedback(destex_feedback_load_tile(tile),
                load_x0, load_y0, load_width, load_height);

            float local_x = source_base_x + x * source_dx;
            float local_y = source_base_y + y * source_dy;
            int32_t packed_s = destex_fixed16(
                (x - load_x0) * DESTEX_RECT_SCALE + local_x * 0.5f);
            int32_t packed_t = destex_fixed16(
                (y - load_y0) * DESTEX_RECT_SCALE + local_y * 0.5f);
            destex_draw_rectangle(tile, x, y, x_end, y_end,
                packed_s, packed_t, dsdx, dtdy);
        }
    }
    destex_mark_feedback_used(tile);
}

static void destex_blit_axis(const surface_t *surf, const surface_t *dest,
    float x0, float y0, const rdpq_blitparms_t *parms,
    const rdpq_blit_plan_t *plan, rdpq_tile_t tile, float uv_scale)
{
    int s0 = plan->s0;
    int t0 = plan->t0;
    int width = plan->width;
    int height = plan->height;
    float scale_x = plan->scale_x;
    float scale_y = plan->scale_y;
    bool flip_x = parms->flip_x;
    bool flip_y = parms->flip_y;
    int source_hotspot_x = plan->cx - plan->s0;
    int source_hotspot_y = plan->cy - plan->t0;
    int geometry_hotspot_x = source_hotspot_x;
    int geometry_hotspot_y = source_hotspot_y;

    /* rdpq_tex_blit implements a negative axis scale as a mirrored draw around
       the opposite side of the hotspot. Turning it into a positive scale and
       toggling the source flip is equivalent only if the geometry hotspot is
       mirrored as well. Keep the original source hotspot for UV scaling. */
    if (scale_x < 0.0f) {
        scale_x = -scale_x;
        flip_x = !flip_x;
        geometry_hotspot_x = width - source_hotspot_x;
    }
    if (scale_y < 0.0f) {
        scale_y = -scale_y;
        flip_y = !flip_y;
        geometry_hotspot_y = height - source_hotspot_y;
    }
    assertf(scale_x > 0.0f && scale_y > 0.0f, "invalid DESTEX scale");

    float origin_x = x0 - geometry_hotspot_x * scale_x;
    float origin_y = y0 - geometry_hotspot_y * scale_y;
    tex_format_t source_fmt = surface_get_format(surf);
    bool resident = destex_surface_is_resident(surf, source_fmt,
        s0, t0, width, height, tile);

    /* A resident source is one region. Non-resident sources use the largest
       core that still leaves a one-texel filter halo in the 32x32 field.
       Four-bit uploads lose one horizontal pixel to byte alignment. */
    int core_w = resident ? width : DESTEX_DRAW_CHUNK_MAX;
    int core_h = resident ? height : DESTEX_DRAW_CHUNK_MAX;
    if (!resident && TEX_FORMAT_BITDEPTH(source_fmt) == 4)
        core_w--;

    for (int v0 = 0; v0 < height; v0 += core_h) {
        int v1 = MIN(v0 + core_h, height);
        for (int u0 = 0; u0 < width; u0 += core_w) {
            int u1 = MIN(u0 + core_w, width);
            destex_chunk_t chunk = { u0, v0, u1, v1 };
            int ps0 = flip_x ? width - u1 : u0;
            int ps1 = flip_x ? width - u0 : u1;
            int pt0 = flip_y ? height - v1 : v0;
            int pt1 = flip_y ? height - v0 : v1;
            int load_s0 = resident ? destex_residency.s0 - s0 :
                MAX(0, ps0 - DESTEX_SOURCE_FILTER_HALO);
            int load_t0 = resident ? destex_residency.t0 - t0 :
                MAX(0, pt0 - DESTEX_SOURCE_FILTER_HALO);

            if (!resident) {
                int load_s1 = MIN(width, ps1 + DESTEX_SOURCE_FILTER_HALO);
                int load_t1 = MIN(height, pt1 + DESTEX_SOURCE_FILTER_HALO);
                destex_upload_source(tile, surf,
                    s0 + load_s0, t0 + load_t0,
                    s0 + load_s1, t0 + load_t1, true);
            } else if (!destex_residency.source_tile_ready ||
                       !destex_residency.source_rectangle_pack) {
                destex_set_source_tile(tile, source_fmt,
                    destex_residency.tmem_pitch, destex_residency.width,
                    destex_residency.height, true);
            }

            float source_start_x = flip_x ?
                ps1 - 1 - load_s0 : ps0 - load_s0;
            float source_start_y = flip_y ?
                pt1 - 1 - load_t0 : pt0 - load_t0;
            destex_draw_axis_region(dest, tile, &chunk,
                origin_x, origin_y, scale_x, scale_y, flip_x, flip_y,
                source_start_x, source_start_y,
                source_hotspot_x - load_s0, source_hotspot_y - load_t0,
                uv_scale);
        }
    }
}

static void destex_blit_rotated(const surface_t *dest, rdpq_tile_t tile,
    float x0, float y0, const rdpq_blitparms_t *parms,
    const rdpq_blit_plan_t *plan, int source_offset_x, int source_offset_y,
    float uv_scale)
{
    int width = plan->width;
    int height = plan->height;
    float scale_x = plan->scale_x;
    float scale_y = plan->scale_y;
    bool flip_x = parms->flip_x;
    bool flip_y = parms->flip_y;
    if (scale_x < 0.0f) { scale_x = -scale_x; flip_x = !flip_x; }
    if (scale_y < 0.0f) { scale_y = -scale_y; flip_y = !flip_y; }
    float sin_theta, cos_theta;
    fm_sincosf(parms->theta, &sin_theta, &cos_theta);

    float cx = plan->cx - plan->s0;
    float cy = plan->cy - plan->t0;

    /* The proven path rotates UVs inside an axis-aligned box. It is not a
       rotated-quad rasterizer; transparent gutters are part of the contract. */
    float box_x0 = x0 - cx * scale_x;
    float box_y0 = y0 - cy * scale_y;
    float box_x1 = x0 + (width - cx) * scale_x;
    float box_y1 = y0 + (height - cy) * scale_y;
    int ix0 = MAX(0, destex_edge(box_x0));
    int iy0 = MAX(0, destex_edge(box_y0));
    int ix1 = MIN(dest->width, destex_edge(box_x1));
    int iy1 = MIN(dest->height, destex_edge(box_y1));
    if (ix1 <= ix0 || iy1 <= iy0)
        return;

    destex_set_feedback_tile(tile, surface_get_format(dest), false,
        dest->width, dest->height);
    destex_set_feedback_image(dest);

    float inv_x = 1.0f / scale_x;
    float inv_y = 1.0f / scale_y;
    /* Invert the same y-down rotation matrix used by rdpq_tex_blit. The old
       signs inverted a conventional y-up matrix instead, making positive
       theta rotate in the opposite direction. */
    float du_dx = cos_theta * inv_x * uv_scale * (flip_x ? -1.0f : 1.0f);
    float dv_dx = sin_theta * inv_y * uv_scale * (flip_y ? -1.0f : 1.0f);
    float du_dy = -sin_theta * inv_x * uv_scale * (flip_x ? -1.0f : 1.0f);
    float dv_dy = cos_theta * inv_y * uv_scale * (flip_y ? -1.0f : 1.0f);

    float base_dx = ix0 - x0;
    float base_dy = iy0 - y0;
    float base_u = cx +
        (base_dx * cos_theta - base_dy * sin_theta) * inv_x * uv_scale;
    float base_v = cy +
        (base_dx * sin_theta + base_dy * cos_theta) * inv_y * uv_scale;
    if (flip_x) base_u = width - base_u;
    if (flip_y) base_v = height - base_v;

    int32_t fixed_s = destex_triangle_fixed(base_u + source_offset_x);
    int32_t fixed_t = destex_triangle_fixed(base_v + source_offset_y);
    int32_t fixed_du_dx = destex_triangle_fixed(du_dx);
    int32_t fixed_dv_dx = destex_triangle_fixed(dv_dx);
    int32_t fixed_du_dy = destex_triangle_fixed(du_dy);
    int32_t fixed_dv_dy = destex_triangle_fixed(dv_dy);
    int32_t feedback_step = destex_triangle_fixed(DESTEX_PACK_SCALE);
    int32_t gradient_dx_s = feedback_step + fixed_du_dx;
    int32_t gradient_dy_t = feedback_step + fixed_dv_dy;
    const uint32_t gradients[4] = {
        destex_pack_fixed_hi(gradient_dx_s, fixed_dv_dx),
        destex_pack_fixed(gradient_dx_s, fixed_dv_dx),
        destex_pack_fixed_hi(fixed_du_dy, gradient_dy_t),
        destex_pack_fixed(fixed_du_dy, gradient_dy_t),
    };

    for (int y = iy0; y < iy1; y += DESTEX_DRAW_CHUNK_MAX) {
        int y_end = MIN(y + DESTEX_DRAW_CHUNK_MAX, iy1);
        for (int x = ix0; x < ix1; x += DESTEX_DRAW_CHUNK_MAX) {
            int x_end = MIN(x + DESTEX_DRAW_CHUNK_MAX, ix1);
            int load_x0 = MIN(x,
                dest->width - destex_feedback_tile.width);
            int load_y0 = MIN(y,
                dest->height - destex_feedback_tile.height);
            destex_load_feedback(destex_feedback_load_tile(tile),
                load_x0, load_y0,
                destex_feedback_tile.width, destex_feedback_tile.height);

            int local_x = x - ix0;
            int local_y = y - iy0;
            int32_t s = fixed_s + local_x * fixed_du_dx +
                local_y * fixed_du_dy + (x - load_x0) * feedback_step;
            int32_t t = fixed_t + local_x * fixed_dv_dx +
                local_y * fixed_dv_dy + (y - load_y0) * feedback_step;
            destex_draw_triangle(tile, x, y, x_end, y_end,
                s, t, gradients);
        }
    }
    destex_mark_feedback_used(tile);
}

static void destex_reset_cached_state(void)
{
    destex_residency.valid = false;
    destex_residency.source_tile_ready = false;
    destex_feedback_tile.valid = false;
    destex_feedback_image.valid = false;
    if (destex_source_loader.valid)
        destex_source_loader.loader.load_mode = TEX_LOAD_UNKNOWN;
}


void rdpq_blendfx_multi_begin(void)
{
    if (destex_multi_depth++ == 0)
        destex_reset_cached_state();
}

void rdpq_blendfx_multi_end(void)
{
    assertf(destex_multi_depth > 0,
        "rdpq_blendfx_multi_end called without rdpq_blendfx_multi_begin");
    if (--destex_multi_depth == 0)
        destex_reset_cached_state();
}

void rdpq_set_blendfx_parms(rdpq_blendfx_t effect,
    const rdpq_blendfx_parms_t *parms)
{
    color_t color = RGBA32(255, 255, 255, 255);
    bool transparency = false;
    if (parms) {
        color = parms->color;
        transparency = parms->transparency;
        if ((color.r | color.g | color.b | color.a) == 0)
            color = RGBA32(255, 255, 255, 255);
    }

    uint8_t strength =
        (effect == RDPQ_BLENDFX_MULTIPLY || effect == RDPQ_BLENDFX_SCREEN) ?
        color.a : ((uint16_t)color.a + 1) / 2;

    uint8_t env_r = (uint8_t)(((uint16_t)color.r * strength + 127) / 255);
    uint8_t env_g = (uint8_t)(((uint16_t)color.g * strength + 127) / 255);
    uint8_t env_b = (uint8_t)(((uint16_t)color.b * strength + 127) / 255);
    rdpq_mode_alphacompare(transparency);

    /* Pass source alpha through both cycles so optional alpha comparison tests
       TEX0. With alpha comparison disabled, this does not affect the RGB effect. */
    switch (effect) {
    case RDPQ_BLENDFX_ADD:
        rdpq_set_env_color(RGBA32(env_r, env_g, env_b, strength));
        rdpq_mode_combiner(RDPQ_COMBINER2(
            (TEX0, 0, ENV, 0), (0, 0, 0, TEX0),
            (COMBINED, 0, COMBINED_ALPHA, TEX1), (0, 0, 0, COMBINED)));
        break;
    case RDPQ_BLENDFX_SUBTRACT:
        rdpq_set_env_color(RGBA32(env_r, env_g, env_b, strength));
        rdpq_mode_combiner(RDPQ_COMBINER2(
            (TEX0, 0, ENV, 0), (0, 0, 0, TEX0),
            (0, COMBINED, COMBINED_ALPHA, TEX1), (0, 0, 0, COMBINED)));
        break;
    case RDPQ_BLENDFX_MULTIPLY:
        rdpq_set_env_color(RGBA32(env_r, env_g, env_b, strength));
        rdpq_set_prim_color(RGBA32(255 - strength, 255 - strength,
            255 - strength, 255));
        rdpq_mode_combiner(RDPQ_COMBINER2(
            (TEX0, 0, ENV, PRIM), (0, 0, 0, TEX0),
            (TEX1, 0, COMBINED, 0), (0, 0, 0, COMBINED)));
        break;
    case RDPQ_BLENDFX_SCREEN:
        rdpq_set_env_color(RGBA32(env_r, env_g, env_b, strength));
        rdpq_mode_combiner(RDPQ_COMBINER2(
            (TEX0, 0, ENV, 0), (0, 0, 0, TEX0),
            (1, TEX1, COMBINED, TEX1), (0, 0, 0, COMBINED)));
        break;
    default:
        assertf(0, "invalid BlendFX effect %d", effect);
    }
}

static void destex_blit_inner(const surface_t *surf, float x0, float y0,
    float uv_scale, const rdpq_blitparms_t *parms)
{
    static const rdpq_blitparms_t default_parms = { .filtering = true };
    if (!parms) parms = &default_parms;
    assertf(surf, "BlendFX source surface cannot be NULL");
    assertf(uv_scale > 0.0f, "BlendFX UV scale must be positive");
    tex_format_t source_fmt = surface_get_format(surf);
    assertf(!parms->allow_xform,
        "BlendFX does not support rdpq_xform transforms");
    assertf(parms->filtering,
        "BlendFX requires bilinear filtering for centered destination sampling");
    assertf(parms->tile <= TILE6, "BlendFX needs tile+1 for the destination");
    assertf(rdpq_is_attached(), "BlendFX requires an attached destination surface");

    const surface_t *dest = rdpq_get_attached();
    tex_format_t dest_fmt = surface_get_format(dest);
    assertf(dest_fmt == FMT_RGBA16,
        "BlendFX destination must be RGBA16, got %s",
        tex_format_name(dest_fmt));

    rdpq_blit_plan_t plan = __rdpq_blit_plan(surf, parms);
    int s0 = plan.s0;
    int t0 = plan.t0;
    int width = plan.width;
    int height = plan.height;
    bool positive_region = width > 0 && height > 0;
    bool source_resident = positive_region &&
        destex_surface_is_resident(surf, source_fmt,
            s0, t0, width, height, parms->tile);

    /* Source format, bounds, and TMEM fit cannot change while this region is
       resident. Keep those checks off the repeated-particle hot path. */
    bool source_fits = source_resident;
    if (!source_resident) {
        assertf(!destex_is_ci(source_fmt),
            "BlendFX cannot use CI4/CI8: the hardware TLUT layout conflicts "
            "with the simultaneous source and destination textures");
        assertf(destex_source_format_supported(source_fmt),
            "BlendFX has no compatible source layout for format %s",
            tex_format_name(source_fmt));
        assertf(positive_region && s0 >= 0 && t0 >= 0 &&
            s0 + width <= surf->width && t0 + height <= surf->height,
            "invalid BlendFX source rectangle");
        source_fits = destex_region_fits(source_fmt, width, height);
    }

    bool rotated = (F2I(parms->theta) & 0x7FFFFFFF) != 0;
    if (uv_scale != 1.0f)
        assertf(source_fits,
            "BlendFX UV scaling requires the selected source to fit upper TMEM");
    if (rotated)
        assertf(source_fits,
            "BlendFX rotation requires the selected source to fit upper "
            "TMEM and packed coordinates");

    /* Keep a fitting source resident as one region. This is shared by the
       rotated path and the common axis-aligned particle path. */
    if (source_fits && !source_resident) {
        if (destex_region_fits(source_fmt, surf->width, surf->height))
            destex_make_resident(surf, 0, 0, surf->width, surf->height,
                parms->tile);
        else
            destex_make_resident(surf, s0, t0, width, height, parms->tile);
    }

    if (rotated) {
        if (!destex_residency.source_tile_ready ||
            destex_residency.source_rectangle_pack)
            destex_set_source_tile(parms->tile, source_fmt,
                destex_residency.tmem_pitch, destex_residency.width,
                destex_residency.height, false);
        destex_blit_rotated(dest, parms->tile, x0, y0, parms, &plan,
            s0 - destex_residency.s0, t0 - destex_residency.t0, uv_scale);
        return;
    }

    destex_blit_axis(surf, dest, x0, y0, parms,
        &plan, parms->tile, uv_scale);
}

static void destex_blit_scoped(const surface_t *surf, float x0, float y0,
    float uv_scale, const rdpq_blitparms_t *parms)
{
    bool standalone = destex_multi_depth == 0;
    if (standalone)
        destex_reset_cached_state();
    destex_blit_inner(surf, x0, y0, uv_scale, parms);
    if (standalone)
        destex_reset_cached_state();
}


void rdpq_blendfx_blit(const surface_t *surf, float x0, float y0,
    const rdpq_blitparms_t *parms)
{
    destex_blit_scoped(surf, x0, y0, 1.0f, parms);
}

void rdpq_blendfx_blit_uv_scaled(const surface_t *surf, float x0, float y0,
    float uv_scale, const rdpq_blitparms_t *parms)
{
    destex_blit_scoped(surf, x0, y0, uv_scale, parms);
}
