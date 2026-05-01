/**
 * @file lossysprite.c
 * @brief LSPR (lossy sprite) decoder
 */

#include "lossysprite.h"

// Match the layout used inside the h264_decoder TU (h264_decoder.c defines
// this before including all internal .c files). Without it, mbStorage_t here
// gains an extra u32 `decoded` field, shifting mbA/mbB/mbC/mbD off the offset
// h264bsdInitMbNeighbours writes to — neighbour pointers come back NULL.
#define OPTIMIZE_NO_DECODED_FLAG

#include "video/h264_decoder.h"
#include "video/h264_decoder/h264bsd_macroblock_layer.h"
#include "video/h264_decoder/h264bsd_neighbour.h"
#include "video/h264_decoder/h264bsd_pic_param_set.h"
#include "video/h264_decoder/h264bsd_seq_param_set.h"
#include "video/h264_decoder/h264bsd_slice_header.h"
#include "video/h264_decoder/h264bsd_util.h"
#include "video/rsph264_internal.h"
#include "video/yuv_internal.h"
#include "asset.h"
#include "graphics.h"
#include "rdpq.h"
#include "rdpq_attach.h"
#include "rdpq_mode.h"
#include "sprite.h"
#include "sprite_internal.h"
#include "n64sys.h"
#include "surface.h"
#include "yuv.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

#define LSPR_MAGIC "LSPR"
#define LSPR_VERSION 5

// LSPR header flags layout (16 bits) — must match tools/mksprite/mksprite_lossy.cpp:
//   bits [1:0]  YUV chroma subsampling (LSPR_YUV_*)
//   bits [3:2]  YUV colorspace; values match sprite_yuv_colorspace_e and so
//               are assigned directly into the colorspace field of
//               sprite_ext_t::yuv_attrs.
//   bits [7:4]  Target memory format (LSPR_TARGET_*)
// Must mirror the enum in tools/mksprite/mksprite_lossy.cpp
enum lspr_chroma_e {
    LSPR_YUV_420 = 0,
    LSPR_YUV_422 = 1,
    LSPR_YUV_444 = 2,
    LSPR_YUV_400 = 3,
};

// Target sprite format the runtime decoder converts the YUV reconstruction to.
// Must mirror the enum in tools/mksprite/mksprite_lossy.cpp
enum lspr_target_e {
    LSPR_TARGET_RGBA16 = 0, // 5:5:5:1 RGBA (default)
    LSPR_TARGET_RGBA32 = 1, // 8:8:8:8 RGBA
    LSPR_TARGET_UYVY   = 2, // packed 4:2:2 (also known as FMT_YUV16)
    LSPR_TARGET_NV12   = 3, // semi-planar 4:2:0
    LSPR_TARGET_NV16   = 4, // semi-planar 4:2:2
};

/**
 * @brief Header structure for LSPR-encoded files.
 * 
 * Must mirror the layout used in tools/mksprite/mksprite_lossy.cpp
 */
typedef struct lspr_header_s {
    uint8_t magic[4];
    uint16_t version;
    uint16_t flags;
    uint16_t width;
    uint16_t height;
    uint16_t orig_width;
    uint16_t orig_height;
    // x264-emitted PPS values that the runtime decoder needs to match the
    // dequant scale used at encode time. Not derivable from the bitstream
    // because mksprite strips the SPS/PPS NALs (only the IDR slice ships).
    uint8_t  pic_init_qp;            // 0..51
    int8_t   chroma_qp_index_offset; // -12..12
    uint8_t  payload[];
} lspr_header_t;

#define LSPR_FLAGS_CHROMA_SHIFT     0
#define LSPR_FLAGS_CHROMA_MASK      0x3
#define LSPR_FLAGS_COLORSPACE_SHIFT 2
#define LSPR_FLAGS_COLORSPACE_MASK  (0x3 << LSPR_FLAGS_COLORSPACE_SHIFT)
#define LSPR_FLAGS_TARGET_SHIFT     4
#define LSPR_FLAGS_TARGET_MASK      (0xF << LSPR_FLAGS_TARGET_SHIFT)

/** @brief Extract the chroma subsampling from the LSPR flags. */
static inline enum lspr_chroma_e lspr_chroma(uint16_t flags) {
    return (flags & LSPR_FLAGS_CHROMA_MASK) >> LSPR_FLAGS_CHROMA_SHIFT;
}

/** @brief Extract the target sprite format from the LSPR flags. */
static inline enum lspr_target_e lspr_target(uint16_t flags) {
    return (flags & LSPR_FLAGS_TARGET_MASK) >> LSPR_FLAGS_TARGET_SHIFT;
}

/** @brief Extract the YUV colorspace from the LSPR flags. */
static inline enum sprite_yuv_colorspace_e lspr_colorspace(uint16_t flags) {
    return (flags & LSPR_FLAGS_COLORSPACE_MASK) >> LSPR_FLAGS_COLORSPACE_SHIFT;
}

static void lspr_set_mb_params(mbStorage_t *pMb, sliceHeader_t *pSlice, u32 sliceId,
                               i32 chromaQpIndexOffset) {
    pMb->sliceId = sliceId;
    pMb->disableDeblockingFilterIdc = pSlice->disableDeblockingFilterIdc;
    pMb->filterOffsetA = pSlice->sliceAlphaC0Offset;
    pMb->filterOffsetB = pSlice->sliceBetaOffset;
    pMb->chromaQpIndexOffset = chromaQpIndexOffset;
}

static void lspr_decode_intra_slice(
    const uint8_t *payload,
    size_t payload_size,
    uint16_t width,
    uint16_t height,
    uint8_t pic_init_qp,
    int8_t chroma_qp_index_offset,
    uint8_t **out_yuv,
    size_t *out_yuv_size
) {
    assertf(payload_size >= 2, "LSPR: payload too small");

    uint8_t nal_hdr = payload[0];
    assertf((nal_hdr & 0x80) == 0, "LSPR: forbidden_zero_bit set");
    nalUnit_t nal = {
        .nalUnitType = (nalUnitType_e)(nal_hdr & 0x1F),
        .nalRefIdc = (nal_hdr >> 5) & 3,
    };
    assertf(nal.nalUnitType == NAL_CODED_SLICE_IDR, "LSPR: non-IDR NAL");

    uint8_t *rbsp = (uint8_t *)(payload + 1);
    size_t rbsp_size = payload_size - 1;

    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    u32 pic_size_in_mbs = (u32)(mb_w * mb_h);
    size_t yuv_size = (size_t)pic_size_in_mbs * 256 + (size_t)pic_size_in_mbs * 64 * 2;
    uint8_t *yuv = (uint8_t*)malloc_uncached(yuv_size);
    assertf(yuv, "LSPR: out of memory");

    seqParamSet_t sps = {
        .profileIdc = 66,
        .levelIdc = 22,
        .seqParameterSetId = 0,
        .maxFrameNum = 1 << 4,
        .picOrderCntType = 2,
        .numRefFrames = 1,
        .picWidthInMbs = (u32)mb_w,
        .picHeightInMbs = (u32)mb_h,
    };

    picParamSet_t pps = {
        .picParameterSetId = 0,
        .seqParameterSetId = 0,
        .picOrderPresentFlag = 0,
        .numSliceGroups = 1,
        .sliceGroupMapType = 0,
        .picSizeInMapUnits = (u32)pic_size_in_mbs,
        .numRefIdxL0Active = 1,
        .picInitQp = pic_init_qp,
        .chromaQpIndexOffset = chroma_qp_index_offset,
        .deblockingFilterControlPresentFlag = 1,
        .constrainedIntraPredFlag = 0,
        .redundantPicCntPresentFlag = 0,
    };

    strmData_t strm = {0};
    strm.pStart = rbsp;
    strm.pCurr = ((u64)(uintptr_t)rbsp) << 3;
    strm.pEnd = ((u64)(uintptr_t)(rbsp + rbsp_size)) << 3;

    uint64_t t_hdr_start = get_ticks_us();

    sliceHeader_t slice = {0};
    u32 slice_status = h264bsdDecodeSliceHeader(&strm, &slice, &sps, &pps, &nal);
    assertf(slice_status == HANTRO_OK, "LSPR: slice header decode failed");
    assertf(IS_I_SLICE(slice.sliceType), "LSPR: non-intra slice");
    assertf(slice.firstMbInSlice == 0, "LSPR: first_mb_in_slice != 0");
    assertf(slice.picParameterSetId == 0, "LSPR: unexpected PPS id");

    mbStorage_t *mb = (mbStorage_t*)calloc(pic_size_in_mbs, sizeof(mbStorage_t));
    assertf(mb, "LSPR: out of memory");
    h264bsdInitMbNeighbours(mb, (u32)mb_w, pic_size_in_mbs);

    image_t image = {
        .data = yuv,
        .width = (u32)mb_w,
        .height = (u32)mb_h,
    };

    i32 qpY = (i32)pps.picInitQp + slice.sliceQpDelta;
    u32 currMbAddr = 0;

    // Rotating ring of mbLayer instances. Each MB writes its CAVLC packed
    // delta records into mbLayer.residual.posCoefBuf, which the RSP later
    // DMAs asynchronously. Using a single mbLayer would race MB N+1's CAVLC
    // writes against MB N's pending DMA; rotating gives the RSP enough lag
    // to consume each MB's buffer before its slot is reused. This mirrors
    // the video-player path (h264bsdDecodeSliceData uses the same idiom
    // with NUM_PARALLEL_MACROBLOCKS).
    #define LSPR_MB_RING_SIZE 32
    macroblockLayer_t *mbLayers = (macroblockLayer_t*)calloc(LSPR_MB_RING_SIZE, sizeof(macroblockLayer_t));
    assertf(mbLayers, "LSPR: out of memory");
    u32 ring_idx = 0;

    uint64_t t_mb_start = get_ticks_us();

    while (currMbAddr < pic_size_in_mbs) {
        macroblockLayer_t *mbLayer = &mbLayers[ring_idx];
        ring_idx = (ring_idx + 1) % LSPR_MB_RING_SIZE;

        lspr_set_mb_params(mb + currMbAddr, &slice, 1, pps.chromaQpIndexOffset);
        u32 mb_layer_status = h264bsdDecodeMacroblockLayer(&strm, mbLayer, mb + currMbAddr,
                                                            slice.sliceType, slice.numRefIdxL0Active);
        assertf(mb_layer_status == HANTRO_OK, "LSPR: macroblock layer decode failed at mb=%lu",
                (unsigned long)currMbAddr);
        assertf(IS_INTRA_MB(*mbLayer), "LSPR: inter MB not supported (mb=%lu type=%d)",
                (unsigned long)currMbAddr, (int)mbLayer->mbType);
        u32 mb_status = h264bsdDecodeMacroblock(mb + currMbAddr, mbLayer, &image, NULL,
                                                 &qpY, currMbAddr, pps.constrainedIntraPredFlag,
                                                 &slice);
        assertf(mb_status == HANTRO_OK, "LSPR: macroblock decode failed at mb=%lu",
                (unsigned long)currMbAddr);
        currMbAddr++;
        if (!h264bsdMoreRbspData(&strm))
            break;
    }

    // Drain the RSP queue once at slice end. The YUV→target conversion
    // below reads `pic`, which is the destination of all queued residual /
    // intra-pred tasks; we must wait for them before the CPU reads it.
    rsph264_sync();

    uint64_t t_mb_end = get_ticks_us();

    assertf(currMbAddr == pic_size_in_mbs, "LSPR: incomplete slice");

    debugf("LSPR decode %dx%d (%lu MBs): hdr+setup=%luus mb_loop=%luus\n",
           (unsigned)width, (unsigned)height,
           (unsigned long)pic_size_in_mbs,
           (unsigned long)(t_mb_start - t_hdr_start),
           (unsigned long)(t_mb_end - t_mb_start));

    free(mbLayers);
    free(mb);

    *out_yuv = yuv;
    *out_yuv_size = yuv_size;
}

// Vertically upsample chroma (4:2:0 -> 4:2:2) by reusing the same source
// (U,V) row across both vertical neighbors. Output byte order is U,Y0,V,Y1
// per pixel pair, matching the FMT_YUV16 (UYVY) layout the RDP expects.
//
// `pic` is uncached, so per-byte reads each pay one full RDRAM transaction.
// We coalesce by reading luma as uint32_t (4 px / transaction) and chroma as
// uint16_t (2 px / transaction), and write the packed UYVY output as
// uint32_t (1 word = 2 px). For 4 input pixels this becomes 3 read + 2 write
// word/halfword transactions.
//
// Strides/alignment: stride is mb_w*16 (always 4-byte aligned), uv_stride
// is stride/2 = mb_w*8 (2-byte aligned), and dst is 64-byte aligned at the
// row start with row pitch w*2 (caller asserts w is even, so 4-byte
// aligned).
static void lspr_i420_to_uyvy(const uint8_t *y_plane, const uint8_t *u_plane, const uint8_t *v_plane,
                              int stride, int uv_stride,
                              int w, int h,
                              uint8_t *dst) {
    int w4 = w & ~3;  // largest multiple of 4 <= w
    for (int py = 0; py < h; py++) {
        const uint32_t *yrow32 = (const uint32_t *)(y_plane + (size_t)py * stride);
        const uint16_t *urow16 = (const uint16_t *)(u_plane + (size_t)(py / 2) * uv_stride);
        const uint16_t *vrow16 = (const uint16_t *)(v_plane + (size_t)(py / 2) * uv_stride);
        uint32_t *drow32 = (uint32_t *)(dst + (size_t)py * w * 2);
        int n = w4 / 4;
        for (int q = 0; q < n; q++) {
            uint32_t y4 = yrow32[q];   // Y0 Y1 Y2 Y3 (big-endian byte order)
            uint32_t u2 = urow16[q];   // U0 in bits 15..8, U1 in bits 7..0
            uint32_t v2 = vrow16[q];
            // Word 0: U0 Y0 V0 Y1
            drow32[q * 2 + 0] = ((u2 << 16) & 0xFF000000)
                              | ((y4 >>  8) & 0x00FF0000)
                              | ( v2        & 0x0000FF00)
                              | ((y4 >> 16) & 0x000000FF);
            // Word 1: U1 Y2 V1 Y3
            drow32[q * 2 + 1] = ((u2 << 24) & 0xFF000000)
                              | ((y4 <<  8) & 0x00FF0000)
                              | ((v2 <<  8) & 0x0000FF00)
                              | ( y4        & 0x000000FF);
        }
        // Tail: caller asserts w is even, so the only possible remainder
        // is exactly 2 pixels.
        if (w & 2) {
            int px = w4;
            const uint8_t *yrow = y_plane + (size_t)py * stride;
            const uint8_t *urow = u_plane + (size_t)(py / 2) * uv_stride;
            const uint8_t *vrow = v_plane + (size_t)(py / 2) * uv_stride;
            uint8_t *drow = dst + (size_t)py * w * 2;
            drow[px * 2 + 0] = urow[px / 2];
            drow[px * 2 + 1] = yrow[px];
            drow[px * 2 + 2] = vrow[px / 2];
            drow[px * 2 + 3] = yrow[px + 1];
        }
    }
}

static sprite_t *lspr_build_semi_planar_sprite(const uint8_t *pic,
                                               uint16_t orig_w, uint16_t orig_h,
                                               int stride, int luma_h,
                                               uint16_t flags,
                                               yuv_format_t fmt) {
    // yuv_tex_blit (semi-planar path) asserts (Y width % 32) == 0 (in
    // addition to height % 16, which is always satisfied since
    // luma_h = mb_h*16). LSPR-aware encoders pad accordingly; if not, this
    // fires loudly rather than producing a garbled blit.
    assertf((stride % 32) == 0,
            "LSPR: padded width %d not multiple of 32 (yuv_tex_blit constraint); pad encoder output to %d",
            stride, (stride + 31) & ~31);
    assertf(fmt == YUV_NV12 || fmt == YUV_NV16,
            "LSPR: lspr_build_semi_planar_sprite called with non-semi-planar format %d", (int)fmt);

    // Sprite layout: Y plane (FMT_I8, stride x luma_h) followed by an
    // interleaved UV plane (FMT_IA16, stride/2 x uv_h with U in the high
    // byte and V in the low byte of each pixel). For NV12 (4:2:0), uv_h is
    // luma_h/2; for NV16 (4:2:2), uv_h is luma_h (chroma vertically
    // upsampled at decode time by row replication). This matches the byte
    // layout that the RSP UV interleaver in yuv.c produces, so the RDP can
    // load it directly via yuv_tex_blit without a per-frame interleave
    // pass.
    int uv_h = (fmt == YUV_NV16) ? luma_h : luma_h / 2;
    size_t y_bytes  = (size_t)stride * luma_h;
    // The H.264 reconstruction is 4:2:0, so source U/V planes are always at
    // luma_h/2 rows. NV16 destination has twice as many UV rows.
    size_t src_uv_bytes = (size_t)(stride / 2) * (luma_h / 2);
    size_t dst_uv_bytes = (size_t)(stride / 2) * uv_h;
    size_t plane_bytes = y_bytes + 2 * dst_uv_bytes;
    size_t plane_bytes_aligned = (plane_bytes + 15) & ~(size_t)15;
    size_t header_bytes = sizeof(sprite_t) + sizeof(sprite_ext_t);
    size_t pixel_off = (header_bytes + 63) & ~(size_t)63;
    size_t total_bytes = pixel_off + plane_bytes_aligned;
    sprite_t *spr = (sprite_t*)memalign(64, total_bytes);
    assertf(spr, "Out of memory");
    memset(spr, 0, pixel_off);
    spr->width = orig_w;
    spr->height = orig_h;
    spr->flags = SPRITE_FLAGS_OWNEDBUFFER | SPRITE_FLAGS_NODATA | SPRITE_FLAGS_EXT | FMT_YUV16;
    spr->hslices = 1;
    spr->vslices = 1;

    sprite_ext_t *sx = (sprite_ext_t*)spr->data;
    sx->size = sizeof(sprite_ext_t);
    sx->version = SPRITE_EXT_VERSION;
    sx->flags = 0;
    sx->yuv_attrs =
        (lspr_colorspace(flags)                                  << SPRITE_YUV_COLORSPACE_SHIFT) |
        (((fmt == YUV_NV16) ? SPRITE_YUV_CHROMA_422
                            : SPRITE_YUV_CHROMA_420)             << SPRITE_YUV_CHROMA_SHIFT) |
        (SPRITE_YUV_LAYOUT_SEMIPLANAR                            << SPRITE_YUV_LAYOUT_SHIFT);
    sx->data_ptr = (uint32_t)pixel_off;
    // Stash the padded plane dimensions for the renderer. The texparms fields
    // are otherwise unread (SPRITE_FLAG_HAS_TEXPARMS is not set).
    sx->texparms.s.translate = (float)stride;
    sx->texparms.t.translate = (float)luma_h;

    // Copy Y and interleave U+V (NV12/NV16) entirely on the RSP. The CPU's
    // alternative is a memcpy from uncached `pic`, which pays full RDRAM
    // latency per load; RSP DMA does cache-line bursts at near-peak RDRAM
    // bandwidth. Both `stride % 32` and `luma_h % 16` are guaranteed by the
    // mb-padded layout (and the assert at the top of this function), so the
    // 32x16 RSP tile loop fits the frame exactly.
    uint8_t *plane_base = (uint8_t*)spr + pixel_off;
    uint8_t *uv_dst = plane_base + y_bytes;
    const uint8_t *u_src = pic + y_bytes;
    const uint8_t *v_src = u_src + src_uv_bytes;

    // Invalidate the entire plane region so the RSP DMA-out wins against
    // any cached lines (RSP DMA bypasses the CPU cache).
    yuv_init();
    data_cache_hit_writeback_invalidate(plane_base, plane_bytes_aligned);
    yuv_plane_copy(pic, stride, plane_base, stride, stride, luma_h);
    if (fmt == YUV_NV16) {
        yuv_i420_chroma_to_nv16(u_src, v_src, stride / 2,
                                uv_dst, stride, stride, luma_h);
    } else if (fmt == YUV_NV12) {
        yuv_i420_chroma_to_nv12(u_src, v_src, stride / 2,
                                uv_dst, stride, stride, luma_h);
    } else {
        assertf(0, "LSPR: unsupported semi-planar format %d", (int)fmt);
    }
    rspq_wait();
    yuv_close();

    return spr;
}

static sprite_t *lspr_build_target_sprite(const uint8_t *pic,
                                          uint16_t orig_w, uint16_t orig_h,
                                          int stride, int luma_h,
                                          uint16_t flags) {
    tex_format_t fmt;
    size_t pixel_bytes;
    enum lspr_target_e target = lspr_target(flags);
    switch (target) {
    case LSPR_TARGET_RGBA32:
        fmt = FMT_RGBA32;
        pixel_bytes = (size_t)orig_w * orig_h * 4;
        break;
    case LSPR_TARGET_RGBA16:
        fmt = FMT_RGBA16;
        pixel_bytes = (size_t)orig_w * orig_h * 2;
        break;
    case LSPR_TARGET_UYVY:
        fmt = FMT_YUV16;
        pixel_bytes = (size_t)orig_w * orig_h * 2;
        break;
    default:
        assertf(0, "LSPR: unsupported target format %d", target);
        return NULL;
    }

    size_t pixel_bytes_aligned = (pixel_bytes + 15) & ~(size_t)15;
    size_t header_bytes = sizeof(sprite_t) + sizeof(sprite_ext_t);
    size_t pixel_off = (header_bytes + 63) & ~(size_t)63;
    size_t total_bytes = pixel_off + pixel_bytes_aligned;
    sprite_t *spr = (sprite_t*)memalign(64, total_bytes);
    assertf(spr, "Out of memory");
    memset(spr, 0, pixel_off);
    spr->width = orig_w;
    spr->height = orig_h;
    spr->flags = SPRITE_FLAGS_OWNEDBUFFER | SPRITE_FLAGS_NODATA | SPRITE_FLAGS_EXT | fmt;
    spr->hslices = 1;
    spr->vslices = 1;

    sprite_ext_t *sx = (sprite_ext_t*)spr->data;
    sx->size = sizeof(sprite_ext_t);
    sx->version = SPRITE_EXT_VERSION;
    sx->flags = 0; // not semi-planar; ordinary blit path
    // For YUV16 (UYVY) targets, encode layout=PACKED + chroma=4:2:2 + colorspace.
    if (fmt == FMT_YUV16) {
        sx->yuv_attrs =
            (lspr_colorspace(flags)         << SPRITE_YUV_COLORSPACE_SHIFT) |
            (SPRITE_YUV_CHROMA_422          << SPRITE_YUV_CHROMA_SHIFT) |
            (SPRITE_YUV_LAYOUT_PACKED       << SPRITE_YUV_LAYOUT_SHIFT);
    }
    sx->data_ptr = (uint32_t)pixel_off;

    // Convert from the I420 reconstruction the H.264 decoder produced.
    // Y/U/V strides are derived from the macroblock-padded picture size.
    int uv_stride = stride / 2;
    size_t y_bytes  = (size_t)stride * luma_h;
    size_t uv_bytes = (size_t)uv_stride * (luma_h / 2);
    const uint8_t *y_plane = pic;
    const uint8_t *u_plane = pic + y_bytes;
    const uint8_t *v_plane = u_plane + uv_bytes;
    uint8_t *dst = (uint8_t*)spr + pixel_off;

    if (target == LSPR_TARGET_UYVY) {
        assertf((orig_w & 1) == 0,
                "LSPR: UYVY target requires even width (got %d)", orig_w);
        if ((orig_w % 32) == 0 && (orig_h % 16) == 0) {
            // RSP fast path: the existing rsp_yuv `interleave4` command
            // already emits packed UYVY (32x16 px tiles). Source planes
            // live in uncached `pic`, so the RSP DMA-in reads fresh data
            // without a writeback. Invalidate dst so the RSP DMA-out
            // (which writes through to RAM) wins against any cached
            // lines.
            yuv_init();
            data_cache_hit_writeback_invalidate(dst, pixel_bytes_aligned);
            yuv_i420_to_uyvy(y_plane, u_plane, v_plane, stride,
                             dst, orig_w * 2, orig_w, orig_h);
            rspq_wait();
            yuv_close();
        } else {
            lspr_i420_to_uyvy(y_plane, u_plane, v_plane,
                              stride, uv_stride, orig_w, orig_h, dst);
            data_cache_hit_writeback(dst, pixel_bytes_aligned);
        }
    } else {
        // RGBA targets: let the RDP YUV combiner do the conversion. We
        // attach the destination pixel buffer as a render surface and ask
        // yuv_tex_blit to draw the I420 reconstruction onto it. This skips
        // the CPU YUV→RGB pass entirely (which on uncached `pic` reads was
        // the dominant cost in this path) and gets bilinear chroma upsample
        // for free as a side benefit.
        //
        // Requires rdpq_init() to have been called by the application.
        // rdpq_attach asserts on that. yuv_init() is refcounted: the first
        // call loads the YUV RSP overlay, subsequent calls are no-ops.
        yuv_init();

        yuv_frame_t frame = {
            .format = YUV_I420,
            .y = surface_make_linear((void*)y_plane, FMT_I8, stride,    luma_h),
            .u = surface_make_linear((void*)u_plane, FMT_I8, uv_stride, luma_h / 2),
            .v = surface_make_linear((void*)v_plane, FMT_I8, uv_stride, luma_h / 2),
        };

        // The RDP writes through to RAM bypassing the CPU cache. Discard
        // any (possibly speculative) cached lines for the destination so a
        // future read of `dst` doesn't return stale data.
        data_cache_hit_writeback_invalidate(dst, pixel_bytes_aligned);

        surface_t target_surf = surface_make_linear(dst, fmt, orig_w, orig_h);
        const yuv_colorspace_t *cs = __sprite_yuv_colorspace(sx->yuv_attrs);
        rdpq_attach(&target_surf, NULL);
        // yuv_tex_blit puts the RDP in YUV mode. Without push/pop the YUV
        // combiner state leaks back to the caller's render mode, which makes
        // the next non-YUV draw on the screen surface emit magenta/purple
        // pixels until the caller next sets a render mode.
        rdpq_mode_push();
        yuv_tex_blit(&frame, 0, 0, NULL, cs);
        rdpq_mode_pop();
        rdpq_detach_wait();
        yuv_close();
    }

    return spr;
}

void lossysprite_init(void)
{
    sprite_decoder_register(LSPR_MAGIC, lossysprite_decode_buf);
}

void lossysprite_close(void)
{
    sprite_decoder_unregister(LSPR_MAGIC);
}

sprite_t *lossysprite_decode_buf(const void *buf, int sz) {
    uint64_t t0 = get_ticks_us();

    assertf(buf && sz >= (int)sizeof(lspr_header_t), "Invalid LSPR buffer");

    const lspr_header_t *hdr = (const lspr_header_t *)buf;
    assertf(memcmp(hdr->magic, LSPR_MAGIC, 4) == 0, "Invalid LSPR magic");
    assertf(hdr->version == LSPR_VERSION,
            "Invalid LSPR version %u (expected %u)",
            (unsigned)hdr->version, (unsigned)LSPR_VERSION);
    assertf(lspr_chroma(hdr->flags) == LSPR_YUV_420, "Invalid LSPR YUV format");

    uint16_t width = hdr->width;
    uint16_t height = hdr->height;
    uint16_t orig_w = hdr->orig_width;
    uint16_t orig_h = hdr->orig_height;
    const uint8_t *payload = hdr->payload;
    size_t payload_size = (size_t)sz - sizeof(lspr_header_t);

    rsph264_init();
    rsph264_begin_frame();
    uint64_t t1 = get_ticks_us();

    uint8_t *pic = NULL;
    size_t pic_size = 0;
    lspr_decode_intra_slice(payload, payload_size, width, height,
                            hdr->pic_init_qp, hdr->chroma_qp_index_offset,
                            &pic, &pic_size);
    uint64_t t2 = get_ticks_us();

    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    int stride = mb_w * 16;
    int luma_h = mb_h * 16;

    sprite_t *spr;
    enum lspr_target_e target = lspr_target(hdr->flags);
    if (target == LSPR_TARGET_NV12) {
        spr = lspr_build_semi_planar_sprite(pic, orig_w, orig_h, stride, luma_h, hdr->flags, YUV_NV12);
    } else if (target == LSPR_TARGET_NV16) {
        spr = lspr_build_semi_planar_sprite(pic, orig_w, orig_h, stride, luma_h, hdr->flags, YUV_NV16);
    } else {
        spr = lspr_build_target_sprite(pic, orig_w, orig_h, stride, luma_h, hdr->flags);
    }
    uint64_t t3 = get_ticks_us();

    free_uncached(pic);
    uint64_t t4 = get_ticks_us();

    debugf("LSPR %dx%d: init=%luus decode=%luus convert=%luus cleanup=%luus total=%luus\n",
           (unsigned)width, (unsigned)height,
           (unsigned long)(t1 - t0),
           (unsigned long)(t2 - t1),
           (unsigned long)(t3 - t2),
           (unsigned long)(t4 - t3),
           (unsigned long)(t4 - t0));

    return spr;
}

sprite_t* lossysprite_load(const char *fn) {
    int sz = 0;
    uint8_t *enc = asset_load(fn, &sz);
    sprite_t *spr = lossysprite_decode_buf(enc, sz);
    free(enc);
    return spr;
}
