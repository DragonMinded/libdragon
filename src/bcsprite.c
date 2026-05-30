/**
 * @file bcsprite.c
 * @brief BCSP (BC1/DXT1 lossy sprite) decoder
 */

#include "bcsprite.h"
#include "sprite.h"
#include "sprite_internal.h"
#include "surface.h"
#include "n64sys.h"
#include "utils.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#include <malloc.h>

/** @brief BCSP version number. */
#define BCSP_VERSION 1

/** @brief Required alignment of the decoded sprite buffer. */
#define BCSP_BUF_ALIGN 64

/** @brief BC1 block payload size in bytes. */
#define BCSP_BLOCK_SIZE 8

/**
 * @brief Header structure for BCSP-encoded files.
 *
 * Must mirror the layout produced by tools/mksprite/mksprite_bc1.cpp.
 * All multi-byte fields are big-endian on disk; the runtime decoder runs
 * on the N64 (also big-endian) so no byte-swap is needed on read.
 */
typedef struct bcsp_header_s {
    uint8_t  magic[BCSP_FILE_MAGIC_SIZE];
    uint16_t version;
    uint16_t flags;        // bit 0: has_alpha (any block uses 3-color mode)
    uint16_t width;
    uint16_t height;
    uint8_t  block_size;   // = BCSP_BLOCK_SIZE
    uint8_t  reserved[3];  // zero in v1 (one byte reserved for num_mips)
    uint8_t  payload[];    // single BC1 block grid: ceil(w/4) * ceil(h/4) * 8 bytes
} bcsp_header_t;

_Static_assert(sizeof(bcsp_header_t) == 20, "bcsp_header_t must be 20 bytes");

static bool bcsp_is_encoded(const void *buf, int sz) {
    if (!buf || sz < (int)sizeof(bcsp_header_t)) return false;
    const bcsp_header_t *hdr = (const bcsp_header_t *)buf;
    return memcmp(hdr->magic, BCSP_FILE_MAGIC, BCSP_FILE_MAGIC_SIZE) == 0
           && hdr->version == BCSP_VERSION;
}

static size_t bcsp_decoded_size_buf(const void *encoded_buf, int encoded_sz) {
    if (!bcsp_is_encoded(encoded_buf, encoded_sz)) return 0;
    const bcsp_header_t *hdr = (const bcsp_header_t *)encoded_buf;
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
static void bcsp_decode_block(
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

    // Unpack the two RGB565 endpoints to 5/6/5 components.
    int r0 = (c0 >> 11) & 0x1F;
    int g0 = (c0 >> 5)  & 0x3F;
    int b0 =  c0        & 0x1F;
    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 5)  & 0x3F;
    int b1 =  c1        & 0x1F;

    // Build the 4-entry RGBA5551 palette. DXT1a uses endpoint ordering
    // as the per-block alpha flag: c0 > c1 selects opaque 4-color mode;
    // c0 <= c1 selects 3-color + transparent.
    uint16_t pal[4];
    // Endpoint 0 and 1 are always literal; collapse G6 -> G5 by dropping LSB.
    pal[0] = ((r0 & 0x1F) << 11) | ((g0 >> 1) << 6) | ((b0 & 0x1F) << 1) | 1;
    pal[1] = ((r1 & 0x1F) << 11) | ((g1 >> 1) << 6) | ((b1 & 0x1F) << 1) | 1;
    if (c0 > c1) {
        int r2 = (2*r0 + r1) / 3, g2 = (2*g0 + g1) / 3, b2 = (2*b0 + b1) / 3;
        int r3 = (r0 + 2*r1) / 3, g3 = (g0 + 2*g1) / 3, b3 = (b0 + 2*b1) / 3;
        pal[2] = ((r2 & 0x1F) << 11) | ((g2 >> 1) << 6) | ((b2 & 0x1F) << 1) | 1;
        pal[3] = ((r3 & 0x1F) << 11) | ((g3 >> 1) << 6) | ((b3 & 0x1F) << 1) | 1;
    } else {
        int r2 = (r0 + r1) / 2, g2 = (g0 + g1) / 2, b2 = (b0 + b1) / 2;
        pal[2] = ((r2 & 0x1F) << 11) | ((g2 >> 1) << 6) | ((b2 & 0x1F) << 1) | 1;
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

static sprite_t *bcsp_load_buf(const void *encoded_buf, int encoded_sz) {
    assertf(bcsp_is_encoded(encoded_buf, encoded_sz), "Invalid BCSP buffer");
    const bcsp_header_t *hdr = (const bcsp_header_t *)encoded_buf;
    assertf(hdr->version == BCSP_VERSION,
        "Unsupported BCSP version %u (this build supports %u)",
        hdr->version, BCSP_VERSION);
    assertf(hdr->block_size == BCSP_BLOCK_SIZE,
        "Unsupported BCSP block_size %u (this build supports %u)",
        hdr->block_size, BCSP_BLOCK_SIZE);
    // Reserved byte at offset 13 was set aside for a future mip-count field.
    // A non-zero value indicates the file was produced by a future encoder
    // that this build does not understand; refuse to decode.
    assertf(hdr->reserved[0] == 0,
        "BCSP file requires a newer bcsprite decoder (reserved=%u)",
        hdr->reserved[0]);

    int width = hdr->width;
    int height = hdr->height;
    assertf(width > 0 && height > 0, "Invalid BCSP dimensions %dx%d", width, height);

    int bw = (width + 3) / 4;
    int bh = (height + 3) / 4;
    size_t payload_size = (size_t)bw * bh * BCSP_BLOCK_SIZE;
    assertf((size_t)encoded_sz >= sizeof(bcsp_header_t) + payload_size,
        "BCSP buffer truncated (sz=%d, expected at least %zu)",
        encoded_sz, sizeof(bcsp_header_t) + payload_size);

    size_t decoded_sz = bcsp_decoded_size_buf(encoded_buf, encoded_sz);
    sprite_t *sprite = (sprite_t *)memalign(BCSP_BUF_ALIGN, decoded_sz);
    assertf(sprite, "Out of memory decoding BCSP sprite (%zu bytes)", decoded_sz);

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

    for (int by = 0; by < bh; by++) {
        int max_dy = height - by * 4;
        if (max_dy > 4) max_dy = 4;
        for (int bx = 0; bx < bw; bx++) {
            int max_dx = width - bx * 4;
            if (max_dx > 4) max_dx = 4;
            uint16_t *block_dst = dst + (by * 4) * width + (bx * 4);
            bcsp_decode_block(src, block_dst, width, max_dx, max_dy);
            src += BCSP_BLOCK_SIZE;
        }
    }

    // Flush the decoded pixels from the CPU cache so the RDP DMA reads see
    // the final values when this sprite is later sampled.
    size_t pixel_bytes_aligned = ROUND_UP((size_t)width * height * 2, 16);
    data_cache_hit_writeback(dst, pixel_bytes_aligned);

    return sprite;
}

static int bcsprite_init_refcount = 0;
static sprite_decoder_t *bcsp_decoder = NULL;

void bcsprite_init(void)
{
    if (bcsprite_init_refcount++ > 0) return;

    assertf(bcsp_decoder == NULL, "bcsprite is already initialized");
    bcsp_decoder = sprite_decoder_register(bcsp_is_encoded, bcsp_load_buf);
}

void bcsprite_close(void)
{
    if (--bcsprite_init_refcount > 0) return;

    assertf(bcsp_decoder != NULL, "bcsprite is not initialized");
    sprite_decoder_unregister(bcsp_decoder);
    bcsp_decoder = NULL;
}
