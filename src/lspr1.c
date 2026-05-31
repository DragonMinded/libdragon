/**
 * @file lspr1.c
 * @brief Lossy-sprite Level 1: BC1Q (BC1/DXT1) decoder
 */

#include "lspr1.h"
#include "sprite.h"
#include "sprite_internal.h"
#include "surface.h"
#include "n64sys.h"
#include "utils.h"
#include "debug.h"
#include "rspq.h"
#include "rsp.h"
#include "asset.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

/** @brief Max BC1 blocks per RSP strip (mirrors N_STRIP_BLOCKS_MAX in rsp_lspr1.S). */
#define BC1Q_STRIP_BLOCKS_MAX 32

/** @brief rspq command IDs (must mirror RSPQ_DefineCommand order in rsp_lspr1.S). */
#define BC1Q_CMD_SET_BUFFERS     0x0
#define BC1Q_CMD_DECODE_STRIP    0x1

DEFINE_RSP_UCODE(rsp_lspr1);
static uint32_t lspr1_ovl_id = 0;

/** @brief BC1Q version number. */
#define BC1Q_VERSION 2

/** @brief Required alignment of the decoded sprite buffer. */
#define BC1Q_BUF_ALIGN 64

/** @brief BC1 block payload size in bytes. */
#define BC1Q_BLOCK_SIZE 8

/**
 * @brief Header structure for BC1Q-encoded files.
 *
 * Must mirror the layout produced by tools/mksprite/mksprite_bc1q.cpp.
 * All multi-byte fields are big-endian on disk; the runtime decoder runs
 * on the N64 (also big-endian) so no byte-swap is needed on read.
 */
typedef struct lspr1_header_s {
    uint8_t  magic[BC1Q_FILE_MAGIC_SIZE];
    uint16_t version;
    uint16_t flags;        // bit 0: has_alpha (any block uses 3-color mode)
    uint16_t width;
    uint16_t height;
    uint8_t  block_size;   // = BC1Q_BLOCK_SIZE
    uint8_t  reserved[7];  // zero in v1; pads header to 24 bytes so payload[]
                           // is 8-byte aligned (RSP DMA requirement).
    uint8_t  payload[];    // single BC1 block grid: ceil(w/4) * ceil(h/4) * 8 bytes
} lspr1_header_t;

_Static_assert(sizeof(lspr1_header_t) == 24, "lspr1_header_t must be 24 bytes");
_Static_assert(offsetof(lspr1_header_t, payload) % 8 == 0,
    "BC1Q payload must be 8-byte aligned for RSP DMA");

static bool lspr1_is_encoded(const void *buf, int sz) {
    if (!buf || sz < (int)sizeof(lspr1_header_t)) return false;
    const lspr1_header_t *hdr = (const lspr1_header_t *)buf;
    return memcmp(hdr->magic, BC1Q_FILE_MAGIC, BC1Q_FILE_MAGIC_SIZE) == 0
           && hdr->version == BC1Q_VERSION;
}

static size_t lspr1_decoded_size_buf(const void *encoded_buf, int encoded_sz) {
    if (!lspr1_is_encoded(encoded_buf, encoded_sz)) return 0;
    const lspr1_header_t *hdr = (const lspr1_header_t *)encoded_buf;
    size_t pixel_bytes = (size_t)hdr->width * hdr->height * 2;
    size_t header_bytes = sizeof(sprite_t) + sizeof(sprite_ext_t);
    return ROUND_UP(header_bytes, 64) + ROUND_UP(pixel_bytes, 16);
}

/**
 * @brief Decode one BC1 block into RGBA5551 pixels.
 *
 * @param block       Pointer to the 8-byte BC1 block (BE on disk, read raw on BE host).
 * @param dst         Destination row pointer for pixel (block_x*4, block_y*4).
 * @param dst_stride  Destination row stride in 16-bit pixels (= image width).
 * @param max_dx      Number of in-bounds columns in this block (1..4).
 * @param max_dy      Number of in-bounds rows in this block (1..4).
 */
static void lspr1_decode_block(
    const uint8_t *block,
    uint16_t *dst, int dst_stride,
    int max_dx, int max_dy
) {
    uint16_t c0 = ((uint16_t)block[0] << 8) | block[1];
    uint16_t c1 = ((uint16_t)block[2] << 8) | block[3];
    uint32_t indices =
        ((uint32_t)block[4] << 24) |
        ((uint32_t)block[5] << 16) |
        ((uint32_t)block[6] << 8)  |
        ((uint32_t)block[7]);

    // Endpoints are stored on-disk as RGBA5551 (alpha bit always 1); unpack
    // the 5-bit R/G/B channels (the alpha bit in position 0 is ignored here).
    int r0 = (c0 >> 11) & 0x1F;
    int g0 = (c0 >> 6)  & 0x1F;
    int b0 = (c0 >> 1)  & 0x1F;
    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 6)  & 0x1F;
    int b1 = (c1 >> 1)  & 0x1F;

    // Build the 4-entry RGBA5551 palette. DXT1a uses endpoint ordering
    // as the per-block alpha flag: c0 > c1 selects opaque 4-color mode;
    // c0 <= c1 selects 3-color + transparent.
    uint16_t pal[4];
    // Endpoint 0 and 1 are already RGBA5551; use them directly (force alpha=1).
    pal[0] = c0 | 1;
    pal[1] = c1 | 1;
    if (c0 > c1) {
        int r2 = (2*r0 + r1) / 3, g2 = (2*g0 + g1) / 3, b2 = (2*b0 + b1) / 3;
        int r3 = (r0 + 2*r1) / 3, g3 = (g0 + 2*g1) / 3, b3 = (b0 + 2*b1) / 3;
        pal[2] = (r2 << 11) | (g2 << 6) | (b2 << 1) | 1;
        pal[3] = (r3 << 11) | (g3 << 6) | (b3 << 1) | 1;
    } else {
        int r2 = (r0 + r1) / 2, g2 = (g0 + g1) / 2, b2 = (b0 + b1) / 2;
        pal[2] = (r2 << 11) | (g2 << 6) | (b2 << 1) | 1;
        pal[3] = 0x0000; // fully transparent (a=0)
    }

    for (int dy = 0; dy < max_dy; dy++) {
        uint16_t *row = dst + dy * dst_stride;
        for (int dx = 0; dx < max_dx; dx++) {
            int idx = (indices >> (2 * (dy * 4 + dx))) & 0x3;
            row[dx] = pal[idx];
        }
    }
}

static sprite_t *lspr1_load_buf(const void *encoded_buf, int encoded_sz) {
    assertf(lspr1_is_encoded(encoded_buf, encoded_sz), "Invalid BC1Q buffer");
    const lspr1_header_t *hdr = (const lspr1_header_t *)encoded_buf;
    assertf(hdr->version == BC1Q_VERSION,
        "Unsupported BC1Q version %u (this build supports %u)",
        hdr->version, BC1Q_VERSION);
    assertf(hdr->block_size == BC1Q_BLOCK_SIZE,
        "Unsupported BC1Q block_size %u (this build supports %u)",
        hdr->block_size, BC1Q_BLOCK_SIZE);
    // Reserved byte at offset 13 was set aside for a future mip-count field.
    // A non-zero value indicates the file was produced by a future encoder
    // that this build does not understand; refuse to decode.
    assertf(hdr->reserved[0] == 0,
        "BC1Q file requires a newer lspr1 decoder (reserved=%u)",
        hdr->reserved[0]);

    int width = hdr->width;
    int height = hdr->height;
    assertf(width > 0 && height > 0, "Invalid BC1Q dimensions %dx%d", width, height);

    int bw = (width + 3) / 4;
    int bh = (height + 3) / 4;
    size_t payload_size = (size_t)bw * bh * BC1Q_BLOCK_SIZE;
    assertf((size_t)encoded_sz >= sizeof(lspr1_header_t) + payload_size,
        "BC1Q buffer truncated (sz=%d, expected at least %zu)",
        encoded_sz, sizeof(lspr1_header_t) + payload_size);

    size_t decoded_sz = lspr1_decoded_size_buf(encoded_buf, encoded_sz);
    sprite_t *sprite = (sprite_t *)memalign(BC1Q_BUF_ALIGN, decoded_sz);
    assertf(sprite, "Out of memory decoding BC1Q sprite (%zu bytes)", decoded_sz);

    size_t header_bytes = ROUND_UP(sizeof(sprite_t) + sizeof(sprite_ext_t), 64);
    memset(sprite, 0, header_bytes);
    sprite->width = width;
    sprite->height = height;
    sprite->flags = SPRITE_FLAGS_OWNEDBUFFER | SPRITE_FLAGS_NODATA | SPRITE_FLAGS_EXT | FMT_RGBA16;
    sprite->hslices = 1;
    sprite->vslices = 1;

    sprite_ext_t *sx = (sprite_ext_t *)sprite->data;
    sx->size = sizeof(sprite_ext_t);
    sx->version = SPRITE_EXT_VERSION;
    sx->data_ptr = (uint32_t)header_bytes;

    uint16_t *dst = (uint16_t *)((uint8_t *)sprite + header_bytes);
    const uint8_t *src = hdr->payload;

    int bw_full = width / 4;          // fully in-bounds block columns
    int bh_full = height / 4;         // fully in-bounds block rows
    int has_right_edge  = (width  & 3) != 0;
    int has_bottom_edge = (height & 3) != 0;

    size_t pixel_bytes = (size_t)width * height * 2;
    size_t pixel_bytes_aligned = ROUND_UP(pixel_bytes, 16);

    // Hand the source buffer and destination region to the RSP. The source
    // payload was just CPU-read from the asset; flush it so RSP DMA sees the
    // final bytes. The destination region is RSP-written; invalidate it so any
    // dirty CPU lines covering it don't overwrite RSP's stores on later evict.
    data_cache_hit_writeback((void *)src, (size_t)bw * bh * BC1Q_BLOCK_SIZE);
    data_cache_hit_writeback_invalidate(dst, pixel_bytes_aligned);

    rspq_write(lspr1_ovl_id, BC1Q_CMD_SET_BUFFERS,
        PhysicalAddr(src), PhysicalAddr(dst), (uint32_t)(width * 2));

    // Walk the fully-aligned interior block grid as horizontal strips.
    for (int by = 0; by < bh_full; by++) {
        int strip_x = 0;
        while (strip_x < bw_full) {
            int n_blocks = bw_full - strip_x;
            if (n_blocks > BC1Q_STRIP_BLOCKS_MAX) n_blocks = BC1Q_STRIP_BLOCKS_MAX;
            uint32_t in_off  = (uint32_t)(by * bw + strip_x) * BC1Q_BLOCK_SIZE;
            uint32_t out_off = (uint32_t)((by * 4) * width + strip_x * 4) * 2;
            rspq_write(lspr1_ovl_id, BC1Q_CMD_DECODE_STRIP,
                in_off, out_off, (uint32_t)n_blocks);
            strip_x += n_blocks;
        }
    }

    // CPU-decode the right-edge partial column (if any) while the RSP drains
    // the interior. These touch < 1/bw of the image so the cost is negligible.
    if (has_right_edge) {
        int bx = bw_full;
        int max_dx = width - bx * 4;
        for (int by = 0; by < bh_full; by++) {
            const uint8_t *blk = src + (by * bw + bx) * BC1Q_BLOCK_SIZE;
            uint16_t *block_dst = dst + (by * 4) * width + (bx * 4);
            lspr1_decode_block(blk, block_dst, width, max_dx, 4);
        }
    }

    // CPU-decode the bottom-edge partial row (if any) across the full width,
    // which naturally covers the bottom-right corner block as well.
    if (has_bottom_edge) {
        int by = bh_full;
        int max_dy = height - by * 4;
        for (int bx = 0; bx < bw; bx++) {
            int max_dx = width - bx * 4;
            if (max_dx > 4) max_dx = 4;
            const uint8_t *blk = src + (by * bw + bx) * BC1Q_BLOCK_SIZE;
            uint16_t *block_dst = dst + (by * 4) * width + (bx * 4);
            lspr1_decode_block(blk, block_dst, width, max_dx, max_dy);
        }
    }

    // Wait for the RSP to finish before returning the sprite.
    rspq_wait();

    // CPU edge fixups wrote through the cache, so flush those lines too. The
    // interior was written by RSP DMA so it's already coherent with RDRAM.
    if (has_right_edge || has_bottom_edge) {
        data_cache_hit_writeback(dst, pixel_bytes_aligned);
    }

    return sprite;
}

static int lspr1_init_refcount = 0;
static sprite_decoder_t *lspr1_decoder = NULL;

void lspr1_init(void)
{
    if (lspr1_init_refcount++ > 0) return;

    assertf(lspr1_decoder == NULL, "lspr1 is already initialized");
    rspq_init();
    asset_init_compression(2);
    lspr1_ovl_id = rspq_overlay_register(&rsp_lspr1);
    assertf(lspr1_ovl_id != 0, "lspr1: failed to register rsp_lspr1 overlay");
    lspr1_decoder = sprite_decoder_register(lspr1_is_encoded, lspr1_load_buf);
}

void lspr1_close(void)
{
    if (--lspr1_init_refcount > 0) return;

    assertf(lspr1_decoder != NULL, "lspr1 is not initialized");
    sprite_decoder_unregister(lspr1_decoder);
    lspr1_decoder = NULL;
    rspq_overlay_unregister(lspr1_ovl_id);
    lspr1_ovl_id = 0;
}
