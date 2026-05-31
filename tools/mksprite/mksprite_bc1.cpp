/*
    mksprite_bc1: BC1Q lossy sprite encoder for mksprite

    Encodes a PNG image into a BCSP file (BC1 block-compressed with RGBA5551
    endpoints and DXT1a punch-through alpha) for transparent decoding at
    sprite_load() time.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
/*
    ------------------------------------------------------------------------
    The BC1Q format
    ------------------------------------------------------------------------
    BC1Q ("BC1, Quality-tunable") is *bitstream-identical* to BC1/DXT1: every
    block is the standard 8 bytes (two 16-bit endpoints c0/c1 followed by 16
    two-bit indices), and the decoder (src/bcsprite.c, src/rsp_bcsp.S) is a plain
    BC1 decoder. The only difference between BC1 and BC1Q lives in the *encoder*:
    BC1Q chooses endpoints and indices so that, at a tunable quality level, the
    resulting block stream compresses far better under a generic entropy/LZ stage
    layered on top, while keeping perceived quality high. There is no new opcode,
    no new mode bit, no decoder change -- a BC1Q file is just a well-chosen BC1
    file.

    Two N64-specific details (shared with the plain BC1 path):
      - Endpoints are stored as RGBA5551 (the 15-bit RGB555 endpoint shifted left
        by one, alpha bit = 1). The shift is monotonic, so the runtime's raw
        `c0 > c1` opaque/alpha mode test still works on the stored values.
      - "Alpha" blocks use BC1's 3-color + punch-through-transparent mode
        (c0 <= c1): index 3 decodes to fully transparent.

    ------------------------------------------------------------------------
    The encoder, step by step (encode_bc1_block)
    ------------------------------------------------------------------------
    Each 4x4 block of 16 RGBA pixels is encoded as follows:

      1. Alpha decision. A pixel with alpha < 128 is "transparent". If any pixel
         is transparent the block uses 3-color mode (c0 <= c1) and transparent
         pixels are forced to index 3; otherwise it uses 4-color mode (c0 > c1).
         Flat/2-color modes (step 8) are only considered for fully opaque blocks.

      2. Bounding box. Compute the per-channel min/max over the opaque pixels:
         the diagonal of that RGB box is the initial endpoint segment.

      3. Inset. Pull the box in by 1/16 of its range (the classic stb_dxt trick):
         the 4 palette entries sample the segment at 0, 1/3, 2/3, 1, so the raw
         extrema systematically overshoot; insetting recenters the segment on the
         bulk of the samples.

      4. Order endpoints for the chosen mode (enforce_mode), nudging equal
         endpoints apart so the mode bit (c0 vs c1) is unambiguous.

      5. Least-squares endpoint refinement (refit_endpoints, 2 iterations).
         Given the current index assignment, solve for the two endpoints that
         minimize the reconstruction error in closed form, then re-quantize. This
         is done in exact integer arithmetic (scaled normal equations + a single
         rounded divide) so it is bit-reproducible.

      6. Final index search, luma-weighted (search_one with LUMA_W). Each pixel
         picks the nearest of the 4 palette entries using a perceptual weighting
         (0.299/0.587/0.114) rather than flat RGB distance, so error is pushed
         into channels the eye cares about least. Yields the 4-color candidate.

      7. Cheaper candidates. Also evaluate, for the same endpoints:
           - 2-color: indices restricted to {0,1} (the two endpoints only);
           - 1-color (flat): the whole block collapses to its average color.
         and measure each candidate's distortion (SSE over opaque pixels).

      8. Rate/distortion mode choice. Pick the mode minimizing
              distortion + lambda(Q) * rate_estimate
         where Q is `--lossy 0..99` (99 = no rate pressure, lossless intent;
         lower Q spends more "rate budget" and biases toward flatter, cheaper
         modes). The chosen mode is then written *in canonical BC1 form*:
           - flat: c0 = avg color, c1 = 0, all 16 indices = 0;
           - 2-color: standard endpoints, indices in {0,1};
           - 4-color: standard endpoints, full luma-searched indices.

    ------------------------------------------------------------------------
    Why this pairs so well with an LZ/entropy layer on top
    ------------------------------------------------------------------------
    Raw BC1 is already a fixed 4 bpp and, taken alone, is nearly incompressible:
    endpoints and indices look like high-entropy noise. BC1Q is designed for the
    fact that libdragon stores the asset through a generic compressor (aplib by
    default here, see BCSP_ASSET_COMPRESSION; lz4hc / shrinkler also possible),
    so the encoder optimizes *post-compression* size, not the raw 8-byte block:

      - Canonicalization creates repetition. A flat region becomes many blocks
        with an all-zero 32-bit index word and the same endpoint pair; a 2-color
        region only ever uses indices 0/1. These regular, low-entropy byte
        patterns are exactly what LZ matchers and entropy coders collapse, so two
        visually-identical blocks become byte-identical and compress to almost
        nothing.
      - The flat/2-color modes are chosen by an explicit rate term, so lowering Q
        trades a little quality for many more such repeats -- a smooth quality vs
        compressed-size curve that plain BC1 (which always emits "best-fit" noisy
        indices) cannot offer.
      - Because the stream stays standard BC1, all of this is free at runtime: the
        decoder is unchanged and the only cost is the (cheap) asset decompression
        already performed when loading any compressed sprite.
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

#include <vector>

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

// Asset-layer compression used for BC1 sprites. aplib (level 2) was the best
// fit for the canon bitstream in the bc1q_eval.py analysis (better than lz4hc
// at the same window, with a cheap decoder).
#define BCSP_ASSET_COMPRESSION 2

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

// Luma weights for the perceptual index search (must match LUMA_W in
// tools/mksprite/bc1q_eval.py for the byte-exact cross-check).
static const double LUMA_W[3] = {0.299, 0.587, 0.114};

// Order endpoints so the runtime's raw `c0 > c1` test selects the wanted mode:
// alpha blocks need c0 <= c1, opaque blocks need c0 > c1 (equal endpoints are
// nudged). Matches _enforce_mode_vec() in bc1q_eval.py.
static void enforce_mode(uint16_t a, uint16_t b, bool use_alpha,
                         uint16_t *o0, uint16_t *o1) {
    if (use_alpha) {
        *o0 = (a < b) ? a : b;          // min
        *o1 = (a > b) ? a : b;          // max
        return;
    }
    uint16_t hi = (a > b) ? a : b;      // max
    uint16_t lo = (a < b) ? a : b;      // min
    if (hi == lo) {
        if (hi > 0) lo = (uint16_t)(hi - 1);
        else { hi = 1; lo = 0; }
    }
    *o0 = hi;
    *o1 = lo;
}

// Build the decoded RGB888 palette from two RGB555 endpoints (matches the
// runtime decoder and _build_pal_vec()).
static void build_pal888(uint16_t c0, uint16_t c1, bool use_alpha, int pal[4][3]) {
    int r0, g0, b0, r1, g1, b1;
    rgb555_unpack_to_888(c0, &r0, &g0, &b0);
    rgb555_unpack_to_888(c1, &r1, &g1, &b1);
    pal[0][0] = r0; pal[0][1] = g0; pal[0][2] = b0;
    pal[1][0] = r1; pal[1][1] = g1; pal[1][2] = b1;
    if (use_alpha) {
        pal[2][0] = (r0 + r1) / 2; pal[2][1] = (g0 + g1) / 2; pal[2][2] = (b0 + b1) / 2;
        pal[3][0] = 0; pal[3][1] = 0; pal[3][2] = 0;
    } else {
        pal[2][0] = (2*r0 + r1) / 3; pal[2][1] = (2*g0 + g1) / 3; pal[2][2] = (2*b0 + b1) / 3;
        pal[3][0] = (r0 + 2*r1) / 3; pal[3][1] = (g0 + 2*g1) / 3; pal[3][2] = (b0 + 2*b1) / 3;
    }
}

// Nearest palette entry for one pixel under weights w (first-min tie-break,
// matching numpy argmin). Candidate 3 is excluded in alpha mode.
static int search_one(const int px[3], const int pal[4][3], bool use_alpha,
                      const double w[3]) {
    int ncand = use_alpha ? 3 : 4;
    int best = 0;
    double best_err = 1e300;
    for (int k = 0; k < ncand; k++) {
        int dr = px[0] - pal[k][0], dg = px[1] - pal[k][1], db = px[2] - pal[k][2];
        double err = (double)(dr*dr) * w[0] + (double)(dg*dg) * w[1] + (double)(db*db) * w[2];
        if (err < best_err) { best_err = err; best = k; }
    }
    return best;
}

// Exact-integer least-squares refit of both endpoints from assigned indices.
// Mirrors _refit_vec() in bc1q_eval.py (scaled integer normal equations, a
// single float divide rounded to nearest-even). Returns false if degenerate.
static bool refit_endpoints(const int rgb[16][3], const int idx[16], bool use_alpha,
                            uint16_t *nc0, uint16_t *nc1) {
    static const int A_OP[4] = {3, 0, 2, 1}, B_OP[4] = {0, 3, 1, 2};
    static const int A_AL[4] = {2, 0, 1, 0}, B_AL[4] = {0, 2, 1, 0};
    int S = use_alpha ? 2 : 3;
    long long SAA = 0, SAB = 0, SBB = 0, SAc[3] = {0,0,0}, SBc[3] = {0,0,0};
    int cnt = 0;
    for (int i = 0; i < 16; i++) {
        int id = idx[i];
        if (use_alpha && id == 3) continue;        // excluded (transparent)
        int A = use_alpha ? A_AL[id] : A_OP[id];
        int B = use_alpha ? B_AL[id] : B_OP[id];
        SAA += (long long)A*A; SAB += (long long)A*B; SBB += (long long)B*B;
        for (int c = 0; c < 3; c++) {
            SAc[c] += (long long)A * rgb[i][c];
            SBc[c] += (long long)B * rgb[i][c];
        }
        cnt++;
    }
    long long det = SAA*SBB - SAB*SAB;
    if (det == 0 || cnt < 2) return false;
    int e0[3], e1[3];
    for (int c = 0; c < 3; c++) {
        long long num0 = SBB*SAc[c] - SAB*SBc[c];
        long long num1 = SAA*SBc[c] - SAB*SAc[c];
        e0[c] = clamp_int((int)rint((double)(S*num0) / (double)det), 0, 255);
        e1[c] = clamp_int((int)rint((double)(S*num1) / (double)det), 0, 255);
    }
    *nc0 = rgb555_pack(e0[0], e0[1], e0[2]);
    *nc1 = rgb555_pack(e1[0], e1[1], e1[2]);
    return true;
}

#define BC1_REFIT_ITERS 2

// Encode a single 4x4 RGBA block (16 pixels, RGBA32 layout, row-major) into an
// 8-byte BC1 block. Implements the "canon" BC1Q pipeline: bounding-box + inset
// endpoints, least-squares refinement, luma-weighted index search, and a
// per-block 1/2/4-color mode decision (RD-driven by lossy_q) that stays inside
// the standard fixed 8-byte block (decoder unchanged). Byte-for-byte identical
// to bc1q_eval.py's canon@Q. Sets *had_alpha for 3-color + transparent blocks.
static void encode_bc1_block(const uint8_t pixels[16][4], uint8_t out_block[8],
                             bool *had_alpha, int lossy_q) {
    // 1. Alpha mode (threshold a < 128 => transparent) + integer RGB cache.
    bool alpha_mask[16];
    int rgb[16][3];
    int opaque_count = 0;
    for (int i = 0; i < 16; i++) {
        alpha_mask[i] = (pixels[i][3] < 128);
        if (!alpha_mask[i]) opaque_count++;
        rgb[i][0] = pixels[i][0]; rgb[i][1] = pixels[i][1]; rgb[i][2] = pixels[i][2];
    }
    bool use_alpha = (opaque_count < 16);

    // 2. Bounding box over opaque pixels.
    int mn[3] = {255,255,255}, mx[3] = {0,0,0};
    if (opaque_count > 0) {
        for (int i = 0; i < 16; i++) {
            if (alpha_mask[i]) continue;
            for (int c = 0; c < 3; c++) {
                if (rgb[i][c] < mn[c]) mn[c] = rgb[i][c];
                if (rgb[i][c] > mx[c]) mx[c] = rgb[i][c];
            }
        }
    } else {
        mn[0]=mn[1]=mn[2]=0; mx[0]=mx[1]=mx[2]=0;
    }

    // 3. Inset the bounding box by 1/16 of the range (stb_dxt trick).
    for (int c = 0; c < 3; c++) {
        int inset = (mx[c] - mn[c]) >> 4;
        mn[c] = clamp_int(mn[c] + inset, 0, 255);
        mx[c] = clamp_int(mx[c] - inset, 0, 255);
    }

    // 4. Initial endpoints in mode order.
    uint16_t c0, c1;
    enforce_mode(rgb555_pack(mx[0], mx[1], mx[2]),
                 rgb555_pack(mn[0], mn[1], mn[2]), use_alpha, &c0, &c1);

    // 5. Least-squares endpoint refinement.
    for (int it = 0; it < BC1_REFIT_ITERS; it++) {
        int pal[4][3];
        build_pal888(c0, c1, use_alpha, pal);
        int idx[16];
        for (int i = 0; i < 16; i++) {
            int unitw_idx;
            if (use_alpha && alpha_mask[i]) unitw_idx = 3;
            else {
                static const double UNIT_W[3] = {1.0, 1.0, 1.0};
                unitw_idx = search_one(rgb[i], pal, use_alpha, UNIT_W);
            }
            idx[i] = unitw_idx;
        }
        uint16_t nc0, nc1;
        if (refit_endpoints(rgb, idx, use_alpha, &nc0, &nc1)) {
            enforce_mode(nc0, nc1, use_alpha, &c0, &c1);
        }
    }

    // 6. Final luma-weighted index search (4-color candidate).
    int pal4[4][3];
    build_pal888(c0, c1, use_alpha, pal4);
    int idx4[16];
    long long D4 = 0;
    for (int i = 0; i < 16; i++) {
        if (use_alpha && alpha_mask[i]) { idx4[i] = 3; continue; }
        idx4[i] = search_one(rgb[i], pal4, use_alpha, LUMA_W);
        int dr = rgb[i][0]-pal4[idx4[i]][0], dg = rgb[i][1]-pal4[idx4[i]][1], db = rgb[i][2]-pal4[idx4[i]][2];
        D4 += dr*dr + dg*dg + db*db;
    }

    // 7. 2-color candidate (nearest of endpoint 0/1) + 1-color candidate (avg).
    int idx2[16];
    long long D2 = 0, D1 = 0;
    long long sumc[3] = {0,0,0};
    for (int i = 0; i < 16; i++) {
        int d0 = 0, d1 = 0;
        for (int c = 0; c < 3; c++) {
            int e0 = rgb[i][c]-pal4[0][c], e1 = rgb[i][c]-pal4[1][c];
            d0 += e0*e0; d1 += e1*e1;
        }
        idx2[i] = (d1 < d0) ? 1 : 0;
        if (!(use_alpha && alpha_mask[i])) {
            D2 += (d1 < d0) ? d1 : d0;
            for (int c = 0; c < 3; c++) sumc[c] += rgb[i][c];
        }
    }
    int cnt = opaque_count > 0 ? opaque_count : 1;
    int avg[3];
    for (int c = 0; c < 3; c++)
        avg[c] = clamp_int((int)rint((double)sumc[c] / (double)cnt), 0, 255);
    uint16_t ccf = rgb555_pack(avg[0], avg[1], avg[2]);
    int palf[3];
    { int rr, gg, bb; rgb555_unpack_to_888(ccf, &rr, &gg, &bb); palf[0]=rr; palf[1]=gg; palf[2]=bb; }
    for (int i = 0; i < 16; i++) {
        if (use_alpha && alpha_mask[i]) continue;
        for (int c = 0; c < 3; c++) { int d = rgb[i][c]-palf[c]; D1 += d*d; }
    }

    // 8. Rate/distortion mode choice: minimize D + lambda(Q) * rate_bytes.
    double allow = (100 - lossy_q) / 100.0 * 12.0;
    double lam_b = 3.0 * allow * allow;
    double cost4 = (double)D4 + lam_b * 8.0;
    double cost2 = use_alpha ? 1e300 : (double)D2 + lam_b * 6.0;
    double cost1 = (use_alpha || ccf == 0) ? 1e300 : (double)D1 + lam_b * 2.0;
    // argmin over [cost1, cost2, cost4] with first-min tie-break; the index
    // maps directly to mode (flat=0, 2col=1, 4col=2).
    int mode = 0;                                   // 1-color (flat)
    double best = cost1;
    if (cost2 < best) { best = cost2; mode = 1; }   // 2-color
    if (cost4 < best) { best = cost4; mode = 2; }   // 4-color

    // 9. Emit the chosen mode into the fixed 8-byte block.
    uint16_t be0, be1;
    uint32_t indices = 0;
    if (mode == 0) {                                // flat: c0=avg, c1=0, idx=0
        be0 = rgb555_to_rgba5551(ccf);
        be1 = 1;                                    // rgb555_to_rgba5551(0)
        indices = 0;
    } else {
        be0 = rgb555_to_rgba5551(c0);
        be1 = rgb555_to_rgba5551(c1);
        const int *src = (mode == 1) ? idx2 : idx4;
        for (int i = 0; i < 16; i++)
            indices |= ((uint32_t)src[i] & 0x3u) << (2 * i);
    }

    out_block[0] = (uint8_t)(be0 >> 8);
    out_block[1] = (uint8_t)(be0 & 0xFF);
    out_block[2] = (uint8_t)(be1 >> 8);
    out_block[3] = (uint8_t)(be1 & 0xFF);
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

    // --lossy Q (0..100, 100=lossless/disabled) drives the per-block 1/2/4-color
    // mode decision: lower Q => stronger bias toward cheaper (flatter) modes,
    // which compress better downstream. Q maps identically to bc1q_eval.py's Q.
    int lossy_q = pm->lossy_quality;

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
            encode_bc1_block(pixels, block, &had_alpha, lossy_q);
            if (had_alpha) any_alpha = true;
            size_t off = ((size_t)by * bw + bx) * BCSP_BLOCK_SIZE;
            memcpy(&payload[off], block, BCSP_BLOCK_SIZE);
        }
    }

    // Assemble the complete BCSP container in memory (24-byte header + block
    // payload), then hand it to the shared asset-layer compressor. The header
    // is big-endian; bytes 0..3 and 17..23 are zero padding so the header is 24
    // bytes and payload[] stays 8-byte aligned for RSP DMA on the decoder side.
    size_t file_bytes = 24 + payload.size();
    std::vector<uint8_t> filebuf(file_bytes, 0);
    uint8_t *hdr = filebuf.data();
    hdr[4] = 'B'; hdr[5] = 'C'; hdr[6] = 'S'; hdr[7] = 'P';
    hdr[8]  = (BCSP_VERSION >> 8) & 0xFF; hdr[9]  = BCSP_VERSION & 0xFF;
    uint16_t aa = any_alpha ? 1 : 0;
    hdr[10] = (aa >> 8) & 0xFF;           hdr[11] = aa & 0xFF;
    hdr[12] = ((uint16_t)orig_w >> 8) & 0xFF; hdr[13] = (uint16_t)orig_w & 0xFF;
    hdr[14] = ((uint16_t)orig_h >> 8) & 0xFF; hdr[15] = (uint16_t)orig_h & 0xFF;
    hdr[16] = BCSP_BLOCK_SIZE;
    memcpy(hdr + 24, payload.data(), payload.size());

    if (sprite_write_compressed(outfn, filebuf.data(), (int)file_bytes,
                                BCSP_ASSET_COMPRESSION) != 0) {
        if (padded) free(padded);
        free(img.image);
        return 1;
    }

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

    return 0;
}
