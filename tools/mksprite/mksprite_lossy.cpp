/*
    mksprite_lossy: lossy sprite encoder for mksprite (H.264 intra)
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

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
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#include "../common/binout.h"
#include "../common/utils.h"
#include "../common/nanotime.h"

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
#include "x264/x264.h"

// Must mirror enum sprite_yuv_colorspace_e in src/sprite_internal.h
enum sprite_yuv_colorspace_e {
    SPRITE_YUV_COLORSPACE_BT601_TV   = 0,
    SPRITE_YUV_COLORSPACE_BT601_FULL = 1,
    SPRITE_YUV_COLORSPACE_BT709_TV   = 2,
    SPRITE_YUV_COLORSPACE_BT709_FULL = 3,
};

// LSPR header flags layout (16 bits):
//   bits [1:0]  YUV chroma subsampling (LSPR_YUV_*)
//   bits [3:2]  YUV colorspace (SPRITE_YUV_COLORSPACE_*; values must match
//               sprite_yuv_colorspace_e in src/sprite_internal.h)
//   bits [7:4]  Target memory format (LSPR_TARGET_*)
// Must mirror enum lspr_chroma_e in src/lossysprite.c
enum lspr_chroma_e {
    LSPR_YUV_420 = 0,
    LSPR_YUV_422 = 1,
    LSPR_YUV_444 = 2,
    LSPR_YUV_400 = 3,
};

// Target memory format the runtime decoder converts the YUV reconstruction to.
// Default (RGBA16 = 0) means an all-zero target field decodes to the new
// default. NV12/NV16 produce semi-planar layouts that the FMT_YUV16
// render path consumes via yuv_tex_blit.
// Must mirror enum lspr_target_e in src/lossysprite.c
enum lspr_target_e {
    LSPR_TARGET_RGBA16 = 0, // 5:5:5:1 RGBA (default)
    LSPR_TARGET_RGBA32 = 1, // 8:8:8:8 RGBA
    LSPR_TARGET_UYVY   = 2, // packed 4:2:2 (also known as FMT_YUV16)
    LSPR_TARGET_NV12   = 3, // semi-planar 4:2:0
    LSPR_TARGET_NV16   = 4, // semi-planar 4:2:2
};

#define LSPR_FLAGS_COLORSPACE_SHIFT 2
#define LSPR_FLAGS_TARGET_SHIFT     4

#define LSPR_VERSION 4

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

static int quality_to_h264_crf(int q) {
    const double CRF_Q100 = 14.0;
    const double CRF_Q0   = 36.0;
    const double DECAY    = 0.5;

    q = clamp_int(q, 0, 100);
    double x = (100 - q) / 100.0;
    double crf = CRF_Q100 + (CRF_Q0 - CRF_Q100) * pow(x, DECAY);
    return clamp_int((int)(crf + 0.5), (int)CRF_Q100, (int)CRF_Q0);
}

static uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
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

static bool alpha_is_opaque(const image_t *img) {
    if (!img || img->ct != LCT_RGBA) return false;
    size_t count = (size_t)img->width * (size_t)img->height;
    const uint8_t *p = img->image + 3;
    for (size_t i = 0; i < count; i++) {
        if (*p != 255) return false;
        p += 4;
    }
    return true;
}

static void write_nal_rbsp(FILE *f, const uint8_t *payload, int size) {
    if (size <= 0) return;
    fputc(payload[0], f);
    int zero_count = 0;
    for (int i = 1; i < size; i++) {
        uint8_t b = payload[i];
        if (zero_count >= 2 && b == 0x03) {
            zero_count = 0;
            continue;
        }
        fputc(b, f);
        zero_count = (b == 0) ? zero_count + 1 : 0;
    }
}

static void write_idr_nals(FILE *f, x264_nal_t *nals, int i_nals, int *idr_written) {
    for (int i = 0; i < i_nals; i++) {
        if (nals[i].i_type != NAL_SLICE_IDR)
            continue;
        const uint8_t *p = nals[i].p_payload;
        int n = nals[i].i_payload;
        int skip = 0;
        if (n >= 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01)
            skip = 4;
        else if (n >= 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
            skip = 3;
        const uint8_t *nal = p + skip;
        int nal_size = n - skip;
        write_nal_rbsp(f, nal, nal_size);
        (*idr_written)++;
    }
}

static void rgba_to_i420(
    const uint8_t *rgba, int width, int height,
    std::vector<uint8_t> &y, std::vector<uint8_t> &u, std::vector<uint8_t> &v
) {
    y.resize((size_t)width * (size_t)height);
    u.resize((size_t)(width / 2) * (size_t)(height / 2));
    v.resize((size_t)(width / 2) * (size_t)(height / 2));

    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            int idx = (py * width + px) * 4;
            float r = (float)rgba[idx + 0];
            float g = (float)rgba[idx + 1];
            float b = (float)rgba[idx + 2];
            float yf = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            y[py * width + px] = clamp_u8((int)(yf + 0.5f));
        }
    }

    for (int py = 0; py < height; py += 2) {
        for (int px = 0; px < width; px += 2) {
            float r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int idx = ((py + dy) * width + (px + dx)) * 4;
                    r += (float)rgba[idx + 0];
                    g += (float)rgba[idx + 1];
                    b += (float)rgba[idx + 2];
                }
            }
            r *= 0.25f;
            g *= 0.25f;
            b *= 0.25f;
            float yf = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float uf = (b - yf) / 1.8556f + 128.0f;
            float vf = (r - yf) / 1.5748f + 128.0f;
            int uv_idx = (py / 2) * (width / 2) + (px / 2);
            u[uv_idx] = clamp_u8((int)(uf + 0.5f));
            v[uv_idx] = clamp_u8((int)(vf + 0.5f));
        }
    }
}

static uint8_t *pad_rgba_edge(const uint8_t *src, int width, int height, int *out_w, int *out_h) {
    int pw = (width + 1) & ~1;
    int ph = (height + 1) & ~1;
    *out_w = pw;
    *out_h = ph;
    if (pw == width && ph == height) {
        return NULL;
    }

    size_t row_bytes = (size_t)width * 4;
    size_t prow_bytes = (size_t)pw * 4;
    uint8_t *dst = (uint8_t *)malloc((size_t)pw * (size_t)ph * 4);
    if (!dst) return NULL;

    for (int y = 0; y < height; y++) {
        const uint8_t *srow = src + (size_t)y * row_bytes;
        uint8_t *drow = dst + (size_t)y * prow_bytes;
        memcpy(drow, srow, row_bytes);
        if (pw > width) {
            const uint8_t *last = srow + (size_t)(width - 1) * 4;
            memcpy(drow + (size_t)width * 4, last, 4);
        }
    }

    if (ph > height) {
        uint8_t *last_row = dst + (size_t)(height - 1) * prow_bytes;
        uint8_t *pad_row = dst + (size_t)height * prow_bytes;
        memcpy(pad_row, last_row, prow_bytes);
    }

    return dst;
}

static void yuv420_to_rgba(
    const uint8_t *y_plane, const uint8_t *u_plane, const uint8_t *v_plane,
    int width, int height, int y_stride, int uv_stride,
    std::vector<uint8_t> &rgba
) {
    rgba.resize((size_t)width * (size_t)height * 4);
    for (int py = 0; py < height; py++) {
        const uint8_t *yrow = y_plane + py * y_stride;
        const uint8_t *urow = u_plane + (py / 2) * uv_stride;
        const uint8_t *vrow = v_plane + (py / 2) * uv_stride;
        for (int px = 0; px < width; px++) {
            int yv = yrow[px];
            int uv = urow[px / 2] - 128;
            int vv = vrow[px / 2] - 128;
            float r = (float)yv + 1.5748f * (float)vv;
            float g = (float)yv - 0.1873f * (float)uv - 0.4681f * (float)vv;
            float b = (float)yv + 1.8556f * (float)uv;
            size_t out = ((size_t)py * (size_t)width + (size_t)px) * 4;
            rgba[out + 0] = clamp_u8((int)(r + 0.5f));
            rgba[out + 1] = clamp_u8((int)(g + 0.5f));
            rgba[out + 2] = clamp_u8((int)(b + 0.5f));
            rgba[out + 3] = 255;
        }
    }
}

static double mse_plane(const uint8_t *a, const uint8_t *b, size_t n) {
    if (n == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        int d = (int)a[i] - (int)b[i];
        sum += (double)(d * d);
    }
    return sum / (double)n;
}

static double psnr_from_mse(double mse) {
    if (mse <= 0.0) return INFINITY;
    return 10.0 * log10((255.0 * 255.0) / mse);
}

static const char *format_psnr(char *buf, size_t sz, double psnr) {
    if (isinf(psnr)) {
        snprintf(buf, sz, "inf");
    } else {
        snprintf(buf, sz, "%.2f", psnr);
    }
    return buf;
}

static char *make_temp_yuv_path(void) {
    const char *dir = getenv("TMPDIR");
#ifdef _WIN32
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = getenv("TMP");
#endif
    if (!dir || !dir[0]) dir = ".";
    char name[512];
    unsigned long long t = (unsigned long long)nanotime();
    snprintf(name, sizeof(name), "%s/lspr-%u-%llu.yuv",
             dir, (unsigned)getpid(), t);
    int fd = open(name, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) {
        fprintf(stderr, "cannot create temp yuv file\n");
        return NULL;
    }
    close(fd);
    return strdup(name);
}

extern "C" int mksprite_convert_lossy(
    const char *infn, const char *outfn, const parms_t *pm,
    int compress
) {
    if (!pm) {
        fprintf(stderr, "mksprite: lossy invalid parameters\n");
        return 1;
    }
    int lossy_compress = (compress == -1) ? 3 : compress;
    if (lossy_compress != 3) {
        fprintf(stderr, "mksprite: lossy --compress only supports value 3\n");
        return 1;
    }
    if (pm->dither_algo != 0) {
        fprintf(stderr, "mksprite: lossy does not support --dither\n");
        return 1;
    }
    if (pm->mipmap_algo != 0) {
        fprintf(stderr, "mksprite: lossy does not support mipmap generation\n");
        return 1;
    }
    uint16_t target_code;
    const char *target_name;
    if (pm->lossy_nv12) {
        target_code = LSPR_TARGET_NV12;
        target_name = "NV12";
    } else if (pm->lossy_nv16) {
        target_code = LSPR_TARGET_NV16;
        target_name = "NV16";
    } else {
        switch (pm->outfmt) {
        case FMT_NONE:   target_code = LSPR_TARGET_RGBA16; target_name = "RGBA16"; break;
        case FMT_RGBA16: target_code = LSPR_TARGET_RGBA16; target_name = "RGBA16"; break;
        case FMT_RGBA32: target_code = LSPR_TARGET_RGBA32; target_name = "RGBA32"; break;
        case FMT_YUV16:  target_code = LSPR_TARGET_UYVY;   target_name = "UYVY";   break;
        default:
            fprintf(stderr, "mksprite: lossy --format only supports RGBA32, RGBA16, UYVY, NV12, NV16\n");
            return 1;
        }
    }

    palette_t pal = {0};
    image_t img = {0};
    if (!load_png_image(infn, pm->outfmt, &img, &pal)) {
        return 1;
    }

    if (img.ct != LCT_RGBA) {
        fprintf(stderr, "mksprite: lossy: failed to load PNG as RGBA\n");
        free(img.image);
        return 1;
    }

    if (target_code == LSPR_TARGET_UYVY && (img.width & 1)) {
        fprintf(stderr, "mksprite: lossy --format=UYVY requires even width (got %d)\n", img.width);
        free(img.image);
        return 1;
    }

    if (!alpha_is_opaque(&img)) {
        fprintf(stderr, "WARNING: lossy does not support alpha, will be dropped\n");
    }

    if (img.width > 0xFFFF || img.height > 0xFFFF) {
        fprintf(stderr, "mksprite: lossy image size too large for lossy header: %dx%d\n", img.width, img.height);
        free(img.image);
        return 1;
    }

    int orig_w = img.width;
    int orig_h = img.height;

    if (pm->gamma_correct) {
        apply_gamma_rgba(img.image, img.width, img.height);
    }

    int padded_w = img.width;
    int padded_h = img.height;
    uint8_t *padded = pad_rgba_edge(img.image, img.width, img.height, &padded_w, &padded_h);
    if (padded) {
        free(img.image);
        img.image = padded;
        img.width = padded_w;
        img.height = padded_h;
        verbose("mksprite: lossy padded to %dx%d", img.width, img.height);
    }

    if (img.width > 0xFFFF || img.height > 0xFFFF) {
        fprintf(stderr, "mksprite: lossy padded size too large for lossy header: %dx%d\n", img.width, img.height);
        free(img.image);
        return 1;
    }

    verbose("mksprite: lossy %s -> %s [%dx%d]", infn, outfn, img.width, img.height);

    std::vector<uint8_t> y, u, v;
    rgba_to_i420(img.image, img.width, img.height, y, u, v);
    free(img.image);

    int crf = quality_to_h264_crf(pm->lossy_quality);
    verbose("mksprite: lossy quality=%d -> crf=%d target=%s", pm->lossy_quality, crf, target_name);

    x264_param_t param;
    char *recon_yuv_path = NULL;
    x264_param_default_preset(&param, "veryslow", "stillimage");
    param.i_log_level = flag_verbose ? X264_LOG_INFO : X264_LOG_ERROR;
    if (flag_debug || flag_verbose) {
        param.b_full_recon = 1;
        recon_yuv_path = make_temp_yuv_path();
        if (!recon_yuv_path) return 1;
        param.psz_dump_yuv = recon_yuv_path;
    }
    param.i_width = img.width;
    param.i_height = img.height;
    param.i_csp = X264_CSP_I420;
    param.i_fps_num = 1;
    param.i_fps_den = 1;
    param.i_frame_total = 1;
    param.i_keyint_max = 1;
    param.i_keyint_min = 1;
    param.i_scenecut_threshold = 0;
    param.i_bframe = 0;
    param.i_bframe_adaptive = 0;
    param.i_bframe_pyramid = 0;
    param.i_frame_reference = 1;
    param.b_open_gop = 0;
    param.b_intra_refresh = 0;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;
    param.b_deblocking_filter = 0;
    param.rc.i_rc_method = X264_RC_CRF;
    param.rc.f_rf_constant = (float)crf;
    param.rc.f_rf_constant_max = (float)crf;
    param.rc.i_lookahead = 0;
    param.analyse.i_trellis = 2;
    param.analyse.i_subpel_refine = 11;
    param.analyse.b_fast_pskip = 0;
    param.analyse.b_dct_decimate = 0;
    param.i_sync_lookahead = 0;
    param.i_threads = 1;

    if (x264_param_apply_profile(&param, "baseline") < 0) {
        fprintf(stderr, "x264: failed to apply baseline profile\n");
        if (recon_yuv_path) { unlink(recon_yuv_path); free(recon_yuv_path); }
        return 1;
    }

    x264_t *enc = x264_encoder_open(&param);
    if (!enc) {
        fprintf(stderr, "x264: encoder open failed\n");
        if (recon_yuv_path) { unlink(recon_yuv_path); free(recon_yuv_path); }
        return 1;
    }

    x264_picture_t pic;
    x264_picture_t pic_out;
    x264_picture_init(&pic);
    x264_picture_init(&pic_out);
    if (x264_picture_alloc(&pic, X264_CSP_I420, img.width, img.height) < 0) {
        fprintf(stderr, "x264: picture alloc failed\n");
        x264_encoder_close(enc);
        if (recon_yuv_path) { unlink(recon_yuv_path); free(recon_yuv_path); }
        return 1;
    }

    pic.i_type = X264_TYPE_IDR;
    pic.i_pts = 0;

    for (int row = 0; row < img.height; row++) {
        memcpy(pic.img.plane[0] + row * pic.img.i_stride[0],
               &y[(size_t)row * (size_t)img.width], img.width);
    }
    for (int row = 0; row < img.height / 2; row++) {
        memcpy(pic.img.plane[1] + row * pic.img.i_stride[1],
               &u[(size_t)row * (size_t)(img.width / 2)], img.width / 2);
        memcpy(pic.img.plane[2] + row * pic.img.i_stride[2],
               &v[(size_t)row * (size_t)(img.width / 2)], img.width / 2);
    }

    bool out_is_stdout = (strstr(outfn, "(stdout)") != NULL);
    FILE *f = out_is_stdout ? stdout : fopen(outfn, "wb");
    if (!f) {
        fprintf(stderr, "mksprite: lossy cannot create output file: %s\n", outfn);
        x264_picture_clean(&pic);
        x264_encoder_close(enc);
        if (recon_yuv_path) { unlink(recon_yuv_path); free(recon_yuv_path); }
        return 1;
    }

    // rgba_to_i420 above uses Kr=0.2126/Kb=0.0722 with full-range scaling,
    // i.e. BT.709 full range. If that conversion is ever changed, update the
    // colorspace tag here too so the decoder picks the matching K0..K5.
    uint16_t lspr_flags = LSPR_YUV_420 |
        (SPRITE_YUV_COLORSPACE_BT709_FULL << LSPR_FLAGS_COLORSPACE_SHIFT) |
        ((uint16_t)target_code << LSPR_FLAGS_TARGET_SHIFT);

    w8(f, 'L'); w8(f, 'S'); w8(f, 'P'); w8(f, 'R');
    w16(f, LSPR_VERSION); // version
    w16(f, lspr_flags);   // flags: chroma subsampling + colorspace
    w16(f, img.width);
    w16(f, img.height);
    w16(f, orig_w);
    w16(f, orig_h);

    x264_nal_t *nals = NULL;
    int i_nals = 0;
    int frame_size = x264_encoder_encode(enc, &nals, &i_nals, &pic, &pic_out);
    if (frame_size < 0) {
        if (!out_is_stdout) { fclose(f); remove(outfn); }
        fprintf(stderr, "x264: encode failed\n");
        x264_picture_clean(&pic);
        x264_encoder_close(enc);
        if (recon_yuv_path) { unlink(recon_yuv_path); free(recon_yuv_path); }
        return 1;
    }
    int idr_written = 0;
    write_idr_nals(f, nals, i_nals, &idr_written);

    while ((frame_size = x264_encoder_encode(enc, &nals, &i_nals, NULL, &pic_out)) > 0) {
        write_idr_nals(f, nals, i_nals, &idr_written);
    }
    if (idr_written == 0) {
        if (!out_is_stdout) { fclose(f); remove(outfn); }
        fprintf(stderr, "x264: no IDR slice generated\n");
        x264_picture_clean(&pic);
        x264_encoder_close(enc);
        if (recon_yuv_path) { unlink(recon_yuv_path); free(recon_yuv_path); }
        return 1;
    }

    if (!out_is_stdout) fclose(f);
    x264_picture_clean(&pic);
    x264_encoder_close(enc);

    if (recon_yuv_path) {
        const size_t y_size = (size_t)img.width * (size_t)img.height;
        const size_t uv_size = y_size / 4;
        const size_t total_size = y_size + uv_size * 2;
        std::vector<uint8_t> yuv = slurp(recon_yuv_path);

        if (yuv.size() != total_size) {
            fprintf(stderr, "mksprite: internal error: unexpected yuv size (%zu)\n", yuv.size());
        } else {
            const uint8_t *ry = yuv.data();
            const uint8_t *ru = yuv.data() + y_size;
            const uint8_t *rv = yuv.data() + y_size + uv_size;

            if (flag_verbose) {
                double mse_y = mse_plane(y.data(), ry, y_size);
                double mse_u = mse_plane(u.data(), ru, uv_size);
                double mse_v = mse_plane(v.data(), rv, uv_size);
                double mse_all = (mse_y * y_size + mse_u * uv_size + mse_v * uv_size) /
                                 (double)(y_size + uv_size * 2);
                double psnr_y = psnr_from_mse(mse_y);
                double psnr_u = psnr_from_mse(mse_u);
                double psnr_v = psnr_from_mse(mse_v);
                double psnr_all = psnr_from_mse(mse_all);
                char by[16], bu[16], bv[16], ball[16];
                verbose("mksprite: PSNR: Y=%s U=%s V=%s avg=%s",
                            format_psnr(by, sizeof(by), psnr_y),
                            format_psnr(bu, sizeof(bu), psnr_u),
                            format_psnr(bv, sizeof(bv), psnr_v),
                            format_psnr(ball, sizeof(ball), psnr_all));
            }

            if (flag_debug && !out_is_stdout) {
                std::vector<uint8_t> rgba_dbg;
                yuv420_to_rgba(
                    yuv.data(),
                    yuv.data() + y_size,
                    yuv.data() + y_size + uv_size,
                    img.width, img.height, img.width, img.width / 2,
                    rgba_dbg
                );
                char *debug_path = change_ext(outfn, ".debug.png");
                lodepng_encode32_file(debug_path, rgba_dbg.data(), img.width, img.height);
                verbose("mksprite: written decoded debug image: %s", debug_path);
                free(debug_path);
            }
        }

        unlink(recon_yuv_path);
        free(recon_yuv_path);
    }

    if (flag_verbose && !out_is_stdout) {
        struct stat st = {0};
        stat(outfn, &st);
        verbose("mksprite: written: %s (%d bytes)", outfn, (int)st.st_size);
    }

    return 0;
}

