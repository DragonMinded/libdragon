/*
    mksprite_bc1: BC1/DXT1 lossy sprite encoder for mksprite

    Encodes a PNG image into a BCSP file (BC1 block-compressed with RGBA5551
    endpoints and DXT1a punch-through alpha) for transparent decoding at
    sprite_load() time.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <vector>

#include "../common/binout.h"

#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#define LODEPNG_NO_COMPILE_CPP
#ifdef __cplusplus
extern "C" {
#endif
#include "../common/lodepng.h"
#ifdef __cplusplus
}
#endif

#include "mksprite.h"

#define BCSP_VERSION 2
#define BCSP_BLOCK_SIZE 8

static void verbose(const char *str, ...) {
    if (!flag_verbose) return;
    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    fprintf(stderr, "\n");
    va_end(va);
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t gamma_correct_value(uint8_t input) {
    return (uint8_t)(((int)input * (int)input) >> 8);
}

static void apply_gamma_rgba(uint8_t *img, int width, int height) {
    size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; i++) {
        img[0] = gamma_correct_value(img[0]);
        img[1] = gamma_correct_value(img[1]);
        img[2] = gamma_correct_value(img[2]);
        img += 4;
    }
}

// Pad an RGBA32 image so width and height are multiples of 4, replicating the
// last in-bounds row/column into the padding. Returns NULL if no padding is
// needed (caller keeps using the input buffer); otherwise returns a freshly
// malloc'd padded buffer that the caller must free.
static uint8_t *pad_rgba_to_4(const uint8_t *src, int width, int height,
                              int *out_w, int *out_h) {
    int pw = (width + 3) & ~3;
    int ph = (height + 3) & ~3;
    *out_w = pw;
    *out_h = ph;
    if (pw == width && ph == height) return NULL;

    size_t row_bytes  = (size_t)width * 4;
    size_t prow_bytes = (size_t)pw * 4;
    uint8_t *dst = (uint8_t *)malloc((size_t)pw * (size_t)ph * 4);
    if (!dst) return NULL;

    for (int y = 0; y < height; y++) {
        const uint8_t *srow = src + (size_t)y * row_bytes;
        uint8_t *drow = dst + (size_t)y * prow_bytes;
        memcpy(drow, srow, row_bytes);
        if (pw > width) {
            const uint8_t *last = srow + (size_t)(width - 1) * 4;
            for (int x = width; x < pw; x++) {
                memcpy(drow + (size_t)x * 4, last, 4);
            }
        }
    }
    for (int y = height; y < ph; y++) {
        const uint8_t *last_row = dst + (size_t)(height - 1) * prow_bytes;
        memcpy(dst + (size_t)y * prow_bytes, last_row, prow_bytes);
    }
    return dst;
}

// Encode an 8-bit (R,G,B) triple as a 15-bit RGB555 value (r5<<10|g5<<5|b5).
// Endpoints are stored on-disk as RGBA5551, but all encoder-internal endpoint
// math (selection, mode comparison, tie-break nudging) is done on this 15-bit
// representation so the alpha bit never interferes; rgb555_to_rgba5551()
// converts to the on-disk format only at block-write time.
static uint16_t rgb555_pack(int r8, int g8, int b8) {
    int r5 = (r8 >> 3) & 0x1F;
    int g5 = (g8 >> 3) & 0x1F;
    int b5 = (b8 >> 3) & 0x1F;
    return (uint16_t)((r5 << 10) | (g5 << 5) | b5);
}

// Decode a 15-bit RGB555 value back to 8-bit per channel, replicating high bits
// into the low bits to fill the dynamic range (standard expansion).
static void rgb555_unpack_to_888(uint16_t c, int *r8, int *g8, int *b8) {
    int r5 = (c >> 10) & 0x1F;
    int g5 = (c >> 5)  & 0x1F;
    int b5 =  c        & 0x1F;
    *r8 = (r5 << 3) | (r5 >> 2);
    *g8 = (g5 << 3) | (g5 >> 2);
    *b8 = (b5 << 3) | (b5 >> 2);
}

// Convert a 15-bit RGB555 endpoint to the on-disk RGBA5551 value (alpha=1).
// The shift-by-1 is monotonic, so the runtime decoder's raw `c0 > c1` mode
// test on the stored values matches the encoder's 15-bit comparison.
static uint16_t rgb555_to_rgba5551(uint16_t c555) {
    return (uint16_t)((c555 << 1) | 1);
}

// Per-channel squared error between two 8-bit RGB triples (alpha ignored).
static int rgb_sq_err(int r0, int g0, int b0, int r1, int g1, int b1) {
    int dr = r0 - r1, dg = g0 - g1, db = b0 - b1;
    return dr*dr + dg*dg + db*db;
}

// Encode a single 4x4 RGBA block (16 pixels, RGBA32 layout, row-major).
// Writes an 8-byte BC1 block to out_block. Sets *had_alpha if the block was
// emitted in 3-color + transparent mode.
static void encode_bc1_block(const uint8_t pixels[16][4], uint8_t out_block[8],
                             bool *had_alpha) {
    // 1. Decide alpha mode. Threshold: a < 128 => transparent.
    bool alpha_mask[16];
    int opaque_count = 0;
    for (int i = 0; i < 16; i++) {
        alpha_mask[i] = (pixels[i][3] < 128);
        if (!alpha_mask[i]) opaque_count++;
    }
    bool use_alpha = (opaque_count < 16);

    // 2. Endpoint selection: per-channel min/max over opaque pixels.
    int r_min = 255, g_min = 255, b_min = 255;
    int r_max =   0, g_max =   0, b_max =   0;
    if (opaque_count > 0) {
        for (int i = 0; i < 16; i++) {
            if (alpha_mask[i]) continue;
            int r = pixels[i][0], g = pixels[i][1], b = pixels[i][2];
            if (r < r_min) r_min = r; if (r > r_max) r_max = r;
            if (g < g_min) g_min = g; if (g > g_max) g_max = g;
            if (b < b_min) b_min = b; if (b > b_max) b_max = b;
        }
    } else {
        // All-transparent block: still need to emit *something*. Use black
        // endpoints; every pixel will pick the transparent slot.
        r_min = g_min = b_min = 0;
        r_max = g_max = b_max = 0;
    }

    // 3. Inset the bounding box slightly to reduce systematic overshoot at
    // RGB555 quantization. This is the classic stb_dxt "inset" trick: the
    // bounding box covers the most extreme samples, but the BC1 palette is
    // sampled at 0, 1/3, 2/3, 1 of the segment, so the literal endpoints
    // are almost never the right thing — pulling in by 1/16 of the range
    // shifts the segment to better cover the bulk of the samples.
    {
        int inset_r = (r_max - r_min) >> 4;
        int inset_g = (g_max - g_min) >> 4;
        int inset_b = (b_max - b_min) >> 4;
        r_min = clamp_int(r_min + inset_r, 0, 255);
        g_min = clamp_int(g_min + inset_g, 0, 255);
        b_min = clamp_int(b_min + inset_b, 0, 255);
        r_max = clamp_int(r_max - inset_r, 0, 255);
        g_max = clamp_int(g_max - inset_g, 0, 255);
        b_max = clamp_int(b_max - inset_b, 0, 255);
    }

    uint16_t c_hi = rgb555_pack(r_max, g_max, b_max); // "high" endpoint (RGB555)
    uint16_t c_lo = rgb555_pack(r_min, g_min, b_min); // "low" endpoint (RGB555)

    // 4. Choose endpoint ordering for the desired alpha mode.
    // 4-color mode requires c0 > c1; 3-color mode requires c0 <= c1.
    // If they happen to match the wrong way around, bump c_hi up or c_lo
    // down by one unit in the dominant channel to break the tie.
    uint16_t c0, c1;
    if (use_alpha) {
        // Want c0 <= c1: ensure c_lo <= c_hi as raw 16-bit values.
        if (c_lo > c_hi) { uint16_t t = c_lo; c_lo = c_hi; c_hi = t; }
        c0 = c_lo;
        c1 = c_hi;
    } else {
        // Want c0 > c1.
        if (c_hi <= c_lo) {
            // Constant-color block (or pathological): nudge so c_hi > c_lo.
            // Choosing c1 = max(c_hi - 1, 0) preserves the average color.
            if (c_hi > 0) c_lo = (uint16_t)(c_hi - 1);
            else c_hi = (uint16_t)(c_lo + 1);
            if (c_hi <= c_lo) { c_hi = 1; c_lo = 0; }
        }
        c0 = c_hi;
        c1 = c_lo;
    }

    // 5. Build the decoded RGB888 palette (matches the runtime decoder).
    int pal[4][3];
    {
        int r0, g0, b0, r1, g1, b1;
        rgb555_unpack_to_888(c0, &r0, &g0, &b0);
        rgb555_unpack_to_888(c1, &r1, &g1, &b1);
        pal[0][0] = r0; pal[0][1] = g0; pal[0][2] = b0;
        pal[1][0] = r1; pal[1][1] = g1; pal[1][2] = b1;
        if (use_alpha) {
            pal[2][0] = (r0 + r1) / 2;
            pal[2][1] = (g0 + g1) / 2;
            pal[2][2] = (b0 + b1) / 2;
            pal[3][0] = 0; pal[3][1] = 0; pal[3][2] = 0; // unused (transparent)
        } else {
            pal[2][0] = (2*r0 + r1) / 3;
            pal[2][1] = (2*g0 + g1) / 3;
            pal[2][2] = (2*b0 + b1) / 3;
            pal[3][0] = (r0 + 2*r1) / 3;
            pal[3][1] = (g0 + 2*g1) / 3;
            pal[3][2] = (b0 + 2*b1) / 3;
        }
    }

    // 6. Per-pixel index search. In alpha mode, transparent pixels are
    // forced to index 3; in opaque mode, every pixel picks the closest
    // of the 4 entries by squared error.
    uint32_t indices = 0;
    for (int i = 0; i < 16; i++) {
        int idx;
        if (use_alpha && alpha_mask[i]) {
            idx = 3;
        } else {
            int r = pixels[i][0], g = pixels[i][1], b = pixels[i][2];
            int n_candidates = use_alpha ? 3 : 4;
            int best = 0;
            int best_err = rgb_sq_err(r, g, b, pal[0][0], pal[0][1], pal[0][2]);
            for (int k = 1; k < n_candidates; k++) {
                int err = rgb_sq_err(r, g, b, pal[k][0], pal[k][1], pal[k][2]);
                if (err < best_err) { best_err = err; best = k; }
            }
            idx = best;
        }
        indices |= ((uint32_t)idx & 0x3u) << (2 * i);
    }

    // 7. Pack into the 8-byte block (big-endian on disk). Endpoints are
    // converted from the internal RGB555 form to on-disk RGBA5551 (alpha=1).
    uint16_t e0 = rgb555_to_rgba5551(c0);
    uint16_t e1 = rgb555_to_rgba5551(c1);
    out_block[0] = (uint8_t)(e0 >> 8);
    out_block[1] = (uint8_t)(e0 & 0xFF);
    out_block[2] = (uint8_t)(e1 >> 8);
    out_block[3] = (uint8_t)(e1 & 0xFF);
    out_block[4] = (uint8_t)((indices >> 24) & 0xFF);
    out_block[5] = (uint8_t)((indices >> 16) & 0xFF);
    out_block[6] = (uint8_t)((indices >>  8) & 0xFF);
    out_block[7] = (uint8_t)( indices        & 0xFF);

    if (had_alpha) *had_alpha = use_alpha;
}

// Decode a BC1 block back to RGBA32 (alpha=0 for transparent slot, 255 elsewhere)
// for PSNR / round-trip diagnostics. Mirrors the runtime decoder's per-block
// math except it emits 8-bit RGB rather than RGBA5551.
static void decode_bc1_block(const uint8_t block[8], uint8_t out_pixels[16][4]) {
    uint16_t c0 = ((uint16_t)block[0] << 8) | block[1];
    uint16_t c1 = ((uint16_t)block[2] << 8) | block[3];
    uint32_t indices =
        ((uint32_t)block[4] << 24) |
        ((uint32_t)block[5] << 16) |
        ((uint32_t)block[6] << 8)  |
        ((uint32_t)block[7]);
    // Endpoints are RGBA5551 on disk; drop the alpha bit (>>1) to recover the
    // 15-bit RGB555 value before expanding to 8-bit. The `c0 > c1` mode test
    // below uses the raw stored values, which is monotonic w.r.t. the 555 form.
    int r0, g0, b0, r1, g1, b1;
    rgb555_unpack_to_888((uint16_t)(c0 >> 1), &r0, &g0, &b0);
    rgb555_unpack_to_888((uint16_t)(c1 >> 1), &r1, &g1, &b1);

    int pal_r[4], pal_g[4], pal_b[4], pal_a[4];
    pal_r[0] = r0; pal_g[0] = g0; pal_b[0] = b0; pal_a[0] = 255;
    pal_r[1] = r1; pal_g[1] = g1; pal_b[1] = b1; pal_a[1] = 255;
    if (c0 > c1) {
        pal_r[2] = (2*r0 + r1) / 3; pal_g[2] = (2*g0 + g1) / 3; pal_b[2] = (2*b0 + b1) / 3; pal_a[2] = 255;
        pal_r[3] = (r0 + 2*r1) / 3; pal_g[3] = (g0 + 2*g1) / 3; pal_b[3] = (b0 + 2*b1) / 3; pal_a[3] = 255;
    } else {
        pal_r[2] = (r0 + r1) / 2; pal_g[2] = (g0 + g1) / 2; pal_b[2] = (b0 + b1) / 2; pal_a[2] = 255;
        pal_r[3] = 0; pal_g[3] = 0; pal_b[3] = 0; pal_a[3] = 0;
    }
    for (int i = 0; i < 16; i++) {
        int idx = (indices >> (2 * i)) & 0x3;
        out_pixels[i][0] = (uint8_t)pal_r[idx];
        out_pixels[i][1] = (uint8_t)pal_g[idx];
        out_pixels[i][2] = (uint8_t)pal_b[idx];
        out_pixels[i][3] = (uint8_t)pal_a[idx];
    }
}

static double psnr_from_mse(double mse) {
    if (mse <= 0.0) return 99.0;
    return 10.0 * log10(255.0 * 255.0 / mse);
}

extern "C" int mksprite_convert_bc1(
    const char *infn, const char *outfn, const parms_t *pm,
    int compress
) {
    if (!pm) {
        fprintf(stderr, "mksprite: bc1 invalid parameters\n");
        return 1;
    }
    if (compress != 1) {
        // Should be unreachable (the CLI dispatcher only routes here for
        // compress=1) but assert anyway.
        fprintf(stderr, "mksprite: bc1 requires --compress 1 (got %d)\n", compress);
        return 1;
    }
    if (pm->dither_algo != 0) {
        fprintf(stderr, "mksprite: bc1 does not support --dither\n");
        return 1;
    }
    if (pm->mipmap_algo != 0) {
        fprintf(stderr, "mksprite: bc1 does not support mipmap generation\n");
        return 1;
    }
    if (pm->outfmt != FMT_NONE && pm->outfmt != FMT_RGBA16) {
        fprintf(stderr, "mksprite: bc1 supports only RGBA16 format\n");
        return 1;
    }

    palette_t pal = {0};
    image_t img = {0};
    if (!load_png_image(infn, pm->outfmt, &img, &pal)) {
        return 1;
    }
    if (img.ct != LCT_RGBA) {
        fprintf(stderr, "mksprite: bc1 needs an RGBA input image\n");
        free(img.image);
        return 1;
    }

    int orig_w = img.width;
    int orig_h = img.height;

    if (orig_w <= 0 || orig_h <= 0 || orig_w > 0xFFFF || orig_h > 0xFFFF) {
        fprintf(stderr, "mksprite: bc1 invalid image dimensions: %dx%d\n", orig_w, orig_h);
        free(img.image);
        return 1;
    }

    if (pm->gamma_correct) {
        apply_gamma_rgba(img.image, img.width, img.height);
    }

    int padded_w = orig_w;
    int padded_h = orig_h;
    uint8_t *padded = pad_rgba_to_4(img.image, orig_w, orig_h, &padded_w, &padded_h);
    uint8_t *work_image = padded ? padded : img.image;
    if (padded) {
        verbose("mksprite: bc1 padded to %dx%d", padded_w, padded_h);
    }

    int bw = padded_w / 4;
    int bh = padded_h / 4;
    size_t payload_bytes = (size_t)bw * (size_t)bh * BCSP_BLOCK_SIZE;
    std::vector<uint8_t> payload(payload_bytes);
    bool any_alpha = false;

    // We discard the lossy quality knob for v1: the encoder always uses a
    // single bounding-box + inset pass. Documenting here so it doesn't look
    // forgotten — see bcsprite.h for the rationale.
    (void)pm->lossy_quality;

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            uint8_t pixels[16][4];
            for (int dy = 0; dy < 4; dy++) {
                int y = by * 4 + dy;
                const uint8_t *row = work_image + (size_t)y * padded_w * 4;
                for (int dx = 0; dx < 4; dx++) {
                    int x = bx * 4 + dx;
                    pixels[dy * 4 + dx][0] = row[x * 4 + 0];
                    pixels[dy * 4 + dx][1] = row[x * 4 + 1];
                    pixels[dy * 4 + dx][2] = row[x * 4 + 2];
                    pixels[dy * 4 + dx][3] = row[x * 4 + 3];
                }
            }
            uint8_t block[8];
            bool had_alpha = false;
            encode_bc1_block(pixels, block, &had_alpha);
            if (had_alpha) any_alpha = true;
            size_t off = ((size_t)by * bw + bx) * BCSP_BLOCK_SIZE;
            memcpy(&payload[off], block, BCSP_BLOCK_SIZE);
        }
    }

    bool out_is_stdout = (strstr(outfn, "(stdout)") != NULL);
    FILE *f = out_is_stdout ? stdout : fopen(outfn, "wb");
    if (!f) {
        fprintf(stderr, "mksprite: bc1 cannot create output file: %s\n", outfn);
        if (padded) free(padded);
        free(img.image);
        return 1;
    }

    // Write the BCSP header (big-endian via binout helpers).
    w8(f, 0); w8(f, 0); w8(f, 0); w8(f, 0); // pad: see BCSP_FILE_MAGIC
    w8(f, 'B'); w8(f, 'C'); w8(f, 'S'); w8(f, 'P');
    w16(f, BCSP_VERSION);
    w16(f, (uint16_t)(any_alpha ? 1 : 0));
    w16(f, (uint16_t)orig_w);
    w16(f, (uint16_t)orig_h);
    w8(f, BCSP_BLOCK_SIZE);
    // 7 reserved bytes: pads the header to 24 bytes so payload[] is 8-byte
    // aligned for RSP DMA on the decoder side.
    w8(f, 0); w8(f, 0); w8(f, 0); w8(f, 0); w8(f, 0); w8(f, 0); w8(f, 0);

    if (fwrite(payload.data(), 1, payload.size(), f) != payload.size()) {
        fprintf(stderr, "mksprite: bc1 write failed\n");
        if (!out_is_stdout) { fclose(f); remove(outfn); }
        if (padded) free(padded);
        free(img.image);
        return 1;
    }

    if (!out_is_stdout) fclose(f);

    // Optional PSNR diagnostic: round-trip decode the payload we just wrote
    // and report PSNR vs the (gamma-corrected, padded) input. Reuses the
    // padded image since edge replication doesn't change the in-bounds pixels.
    if (flag_verbose) {
        std::vector<uint8_t> recon((size_t)padded_w * padded_h * 4);
        for (int by = 0; by < bh; by++) {
            for (int bx = 0; bx < bw; bx++) {
                uint8_t pixels[16][4];
                size_t off = ((size_t)by * bw + bx) * BCSP_BLOCK_SIZE;
                decode_bc1_block(&payload[off], pixels);
                for (int dy = 0; dy < 4; dy++) {
                    int y = by * 4 + dy;
                    uint8_t *row = recon.data() + (size_t)y * padded_w * 4;
                    for (int dx = 0; dx < 4; dx++) {
                        int x = bx * 4 + dx;
                        row[x * 4 + 0] = pixels[dy * 4 + dx][0];
                        row[x * 4 + 1] = pixels[dy * 4 + dx][1];
                        row[x * 4 + 2] = pixels[dy * 4 + dx][2];
                        row[x * 4 + 3] = pixels[dy * 4 + dx][3];
                    }
                }
            }
        }
        // Compute PSNR over the original (unpadded) region only.
        double sum_sq = 0.0;
        size_t count = 0;
        for (int y = 0; y < orig_h; y++) {
            const uint8_t *src_row = work_image + (size_t)y * padded_w * 4;
            const uint8_t *rec_row = recon.data() + (size_t)y * padded_w * 4;
            for (int x = 0; x < orig_w; x++) {
                int dr = src_row[x*4+0] - rec_row[x*4+0];
                int dg = src_row[x*4+1] - rec_row[x*4+1];
                int db = src_row[x*4+2] - rec_row[x*4+2];
                sum_sq += dr*dr + dg*dg + db*db;
                count += 3;
            }
        }
        double mse = (count > 0) ? (sum_sq / (double)count) : 0.0;
        verbose("mksprite: bc1 PSNR=%.2f dB (any_alpha=%d)",
                psnr_from_mse(mse), (int)any_alpha);
    }

    if (padded) free(padded);
    free(img.image);

    if (flag_verbose && !out_is_stdout) {
        struct stat st = {0};
        stat(outfn, &st);
        verbose("mksprite: bc1 written: %s (%d bytes)", outfn, (int)st.st_size);
    }

    return 0;
}
