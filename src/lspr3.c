/**
 * @file lspr3.c
 * @author Christopher Bonhage <christopher.bonhage@meeq.tech>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Lossy-sprite Level 3: H264I decoder
 */

#include "lspr3.h"

// h264_decoder.h defines OPTIMIZE_NO_DECODED_FLAG before pulling in the
// h264bsd headers, so mbStorage_t here matches the layout used inside the
// h264_decoder TU.
#include "video/h264_decoder.h"
#include "video/h264_decoder/h264bsd_macroblock_layer.h"
#include "video/h264_decoder/h264bsd_neighbour.h"
#include "video/h264_decoder/h264bsd_pic_param_set.h"
#include "video/h264_decoder/h264bsd_seq_param_set.h"
#include "video/h264_decoder/h264bsd_slice_header.h"
#include "video/h264_decoder/h264bsd_util.h"
#include "video/rsph264_internal.h"
#include "graphics.h"
#include "rdpq.h"
#include "rdpq_attach.h"
#include "rdpq_mode.h"
#include "rspq.h"
#include "sprite.h"
#include "sprite_internal.h"
#include "n64sys.h"
#include "scratch.h"
#include "surface.h"
#include "utils.h"
#include "yuv.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <stddef.h>

/** @brief H264I version number. */
#define H264I_VERSION 5

/** @brief H264I header flag: file carries a 1-bit alpha bitmap after the H.264 payload. */
#define LSPR3_FLAG_ALPHA1 0x01

/** @brief H264I decode macroblock ring size */
#define H264I_MB_RING 2

/** @brief Required alignment of the decoded sprite buffer. */
#define H264I_BUF_ALIGN 64

/** @brief Set to 1 to enable per-allocation heap/scratch trace dumps for
 *  diagnosing memory pressure during cam decodes. Off in production. */
#define LSPR3_TRACE_MEM 0

/** @brief Set to 1 to verify the banded intra decoder bit-for-bit against the
 *  full-frame path on every decode (needs a 2nd full YUV, so run on 8 MiB).
 *  Off in production. */
#define LSPR3_VERIFY_BANDING 0
#define LSPR3_VERIFY_BAND_ROWS 5

#if LSPR3_TRACE_MEM
static void lspr3_memdump(const char *tag);
#else
#define lspr3_memdump(tag) ((void)0)
#endif

/**
 * @brief Header structure for H264I-encoded files.
 *
 * Must mirror the layout used in tools/mksprite/mksprite_h264i.cpp
 */
typedef struct lspr3_header_s {
    uint8_t magic[H264I_FILE_MAGIC_SIZE];
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint16_t orig_width;
    uint16_t orig_height;
    // x264-emitted PPS values that the runtime decoder needs to match the
    // dequant scale used at encode time. Not derivable from the bitstream
    // because mksprite strips the SPS/PPS NALs (only the IDR slice ships).
    uint8_t  pic_init_qp;            // 0..51
    int8_t   chroma_qp_index_offset; // -12..12
    uint8_t  flags;                  // bit 0 (LSPR3_FLAG_ALPHA1): 1-bit alpha bitmap present
    uint8_t  reserved[3];            // zero in v5; also pads alpha_size to 4-byte alignment
    uint32_t alpha_size;             // byte size of the 1bpp alpha bitmap (0 if none)
    uint8_t  payload[];              // H.264 RBSP, followed by the alpha bitmap if present
} lspr3_header_t;

// The runtime reads fields via a raw pointer cast over the file bytes, and uses
// both `hdr->payload` and `sizeof(lspr3_header_t)` to locate/measure the H.264
// payload. Keep the struct free of trailing padding so the two stay consistent
// and match the byte layout emitted by tools/mksprite/mksprite_h264i.cpp.
_Static_assert(sizeof(lspr3_header_t) == 28, "lspr3_header_t must be 28 bytes");
_Static_assert(offsetof(lspr3_header_t, payload) == 28,
    "lspr3_header_t payload must start at offset 28 (no trailing padding)");

static bool lspr3_is_encoded(const void *buf, int sz) {
    if (!buf || sz < (int)sizeof(lspr3_header_t)) return false;
    const lspr3_header_t *hdr = (const lspr3_header_t *)buf;
    return memcmp(hdr->magic, H264I_FILE_MAGIC, H264I_FILE_MAGIC_SIZE) == 0;
}

static size_t lspr3_decoded_size_buf(const void *encoded_buf, int encoded_sz,
                                     int x_divisor, int y_divisor) {
    if (!lspr3_is_encoded(encoded_buf, encoded_sz)) return 0;
    if (x_divisor < 1) x_divisor = 1;
    if (y_divisor < 1) y_divisor = 1;
    const lspr3_header_t *hdr = (const lspr3_header_t *)encoded_buf;
    assertf(hdr->version == H264I_VERSION, "Invalid lossy sprite version (H264I version %d, expected %d)\nPlease regenerate your asset files", hdr->version, H264I_VERSION);
    size_t out_w = hdr->orig_width / x_divisor;
    size_t out_h = hdr->orig_height / y_divisor;
    size_t pixel_bytes = out_w * out_h * 2;
    size_t header_bytes = sizeof(sprite_t) + sizeof(sprite_ext_t);
    return ROUND_UP(header_bytes, 64) + ROUND_UP(pixel_bytes, 16);
}

static void lspr3_decode_intra_slice(
    const uint8_t *payload,
    size_t payload_size,
    uint16_t width,
    uint16_t height,
    uint8_t pic_init_qp,
    int8_t chroma_qp_index_offset,
    uint8_t **out_yuv,
    size_t *out_yuv_size,
    const lspr3_load_parms_t *parms
) {
    assertf(payload_size >= 2, "H264I: payload too small");

    uint8_t nal_hdr = payload[0];
    assertf((nal_hdr & 0x80) == 0, "H264I: forbidden_zero_bit set");
    nalUnit_t nal = {
        .nalUnitType = (nalUnitType_e)(nal_hdr & 0x1F),
        .nalRefIdc = (nal_hdr >> 5) & 3,
    };
    assertf(nal.nalUnitType == NAL_CODED_SLICE_IDR, "H264I: non-IDR NAL");

    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    u32 pic_size_in_mbs = (u32)(mb_w * mb_h);
    size_t yuv_size = (size_t)pic_size_in_mbs * 256 + (size_t)pic_size_in_mbs * 64 * 2;
    lspr3_memdump("pre-yuv");
    uint8_t *yuv = (uint8_t*)scratch_malloc_uncached(yuv_size);
    assertf(yuv, "H264I: out of memory (yuv %u)", (unsigned)yuv_size);
    lspr3_memdump("post-yuv");
    image_t image = { .data = yuv, .width = (u32)mb_w, .height = (u32)mb_h };

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

    uint8_t *rbsp = (uint8_t *)(payload + 1);
    size_t rbsp_size = payload_size - 1;
    strmData_t strm = {0};
    strm.pStart = rbsp;
    strm.pCurr = ((u64)(uintptr_t)rbsp) << 3;
    strm.pEnd = ((u64)(uintptr_t)(rbsp + rbsp_size)) << 3;

    // Reset the RSP decoder's per-frame state (notably last_packed_delta_buf)
    // before queuing any macroblock work for this slice.
    rsph264_begin_frame();

    sliceHeader_t slice = {0};
    u32 slice_status = h264bsdDecodeSliceHeader(&strm, &slice, &sps, &pps, &nal);
    assertf(slice_status == HANTRO_OK, "H264I: slice header decode failed");
    assertf(IS_I_SLICE(slice.sliceType), "H264I: non-intra slice");
    assertf(slice.firstMbInSlice == 0, "H264I: first_mb_in_slice != 0");
    assertf(slice.picParameterSetId == 0, "H264I: unexpected PPS id");

    lspr3_memdump("pre-mb");
    // mb storage is transient — allocated here, freed at the end of
    // this function. Default routes to the main heap to avoid stacking
    // on scratch's peak alongside the longer-lived yuv + output sprite
    // buffers (the three together would otherwise need a contiguous
    // scratch window large enough to hold all of them simultaneously).
    // Callers facing main-heap fragmentation can override via
    // parms->mb_alloc to route this elsewhere (e.g. a dedicated reserve
    // sized for the worst-case mb count).
    size_t mb_bytes = (size_t)pic_size_in_mbs * sizeof(mbStorage_t);
    mbStorage_t *mb;
    if (parms && parms->mb_alloc) {
        mb = (mbStorage_t*)parms->mb_alloc(parms->mb_alloc_ctx, mb_bytes);
    } else {
        mb = (mbStorage_t*)calloc(pic_size_in_mbs, sizeof(mbStorage_t));
    }
    assertf(mb, "H264I: out of memory (mb %u = %u x %u)",
        (unsigned)mb_bytes,
        (unsigned)sizeof(mbStorage_t), (unsigned)pic_size_in_mbs);
    lspr3_memdump("post-mb");
    h264bsdInitMbNeighbours(mb, (u32)mb_w, pic_size_in_mbs);

    i32 qpY = (i32)pps.picInitQp + slice.sliceQpDelta;
    u32 currMbAddr = 0;

    // Each MB writes its CAVLC packed delta records into
    // mbLayer.residual.posCoefBuf, which the RSP later DMAs asynchronously.
    // Reusing a slot before the RSP has consumed it would race MB N+1's CAVLC
    // writes against MB N's pending DMA. A 2-slot ring paced with rspq
    // syncpoints makes this deterministic: before reusing a slot we wait on
    // the syncpoint recorded right after that slot's RSP work was queued, so
    // the RSP is guaranteed to have drained the buffer. Two slots fit on the
    // stack, so no allocation is needed.
    macroblockLayer_t mbLayers[H264I_MB_RING] = {0};
    rspq_syncpoint_t slot_sync[H264I_MB_RING] = {0}; // 0 = none pending (ids start at 1)

    while (currMbAddr < pic_size_in_mbs) {
        u32 slot = currMbAddr % H264I_MB_RING;
        // Wait for the RSP to finish consuming this slot before overwriting it.
        if (slot_sync[slot]) rspq_syncpoint_wait(slot_sync[slot]);
        macroblockLayer_t *mbLayer = &mbLayers[slot];

        mbStorage_t *pMb = mb + currMbAddr;
        pMb->sliceId = 1;
        pMb->disableDeblockingFilterIdc = slice.disableDeblockingFilterIdc;
        pMb->filterOffsetA = slice.sliceAlphaC0Offset;
        pMb->filterOffsetB = slice.sliceBetaOffset;
        pMb->chromaQpIndexOffset = pps.chromaQpIndexOffset;

        u32 mb_layer_status = h264bsdDecodeMacroblockLayer(&strm, mbLayer, mb + currMbAddr,
                                                            slice.sliceType, slice.numRefIdxL0Active);
        assertf(mb_layer_status == HANTRO_OK,
                "H264I: macroblock layer decode failed at mb=%lu",
                (unsigned long)currMbAddr);
        assertf(IS_INTRA_MB(*mbLayer),
                "H264I: inter MB not supported (mb=%lu type=%d)",
                (unsigned long)currMbAddr,
                (int)mbLayer->mbType);
        u32 mb_status = h264bsdDecodeMacroblock(mb + currMbAddr, mbLayer, &image, NULL,
                                                 &qpY, currMbAddr, pps.constrainedIntraPredFlag,
                                                 &slice);
        assertf(mb_status == HANTRO_OK,
                "H264I: macroblock decode failed at mb=%lu",
                (unsigned long)currMbAddr);
        // Record the queue position after this MB's RSP work; the slot is safe
        // to reuse once the RSP reaches this point.
        slot_sync[slot] = rspq_syncpoint_new();
        currMbAddr++;
        if (!h264bsdMoreRbspData(&strm)) break;
    }

    // Drain the RSP queue before freeing `mb`: the RSP asynchronously DMAs
    // neighbour data and CAVLC/residual records out of it, so it must be idle
    // before the memory is released.
    rsph264_sync();
    assertf(currMbAddr == pic_size_in_mbs, "H264I: incomplete slice");
    if (parms && parms->mb_alloc) {
        parms->mb_free(parms->mb_alloc_ctx, mb);
    } else {
        free(mb);  // matches the calloc above (main heap, not scratch)
    }

    *out_yuv = yuv;
    *out_yuv_size = yuv_size;
}

static uint8_t *lspr3_setup_rgba16_sprite_header(sprite_t *sprite, uint16_t out_w, uint16_t out_h);

// Banded variant: reconstruct the frame in horizontal bands of `band_rows`
// MB-rows into a small windowed scratch buffer, so the peak transient YUV is a
// fraction of the full-frame path. Each completed band is either assembled into
// the full-frame `out_yuv` (verification: lets a harness diff against the
// full-frame reconstruction) OR converted directly into `out_sprite` (shipping:
// no full YUV ever resident). Exactly one of out_yuv / out_sprite is non-NULL.
// For out_sprite, x_div/y_div carry the same output downsample as the full-frame
// path.
//
// The window is `band_rows`+2 MB-rows: the band's content occupies window rows
// 1..band_rows, with one extra "lookahead" row below it (row band_rows+1) and an
// unused padding row above it (row 0, an artefact of the window addressing's +1
// offset). The lookahead row matters for two reasons:
//   1. Chroma seam: the YUV->RGB bilinear chroma upsample at the band's bottom
//      row needs the next row's chroma. Decoding one row ahead and including it
//      in the blit feeds that upsample its real neighbour, so band boundaries are
//      seam-free. The lookahead row's OWN output (its bottom edge clamps) lands
//      on the next band's first output rows and is overwritten by that band.
//   2. Continuity: the lookahead row IS the next band's first content row, and it
//      was decoded with its top intra-pred neighbour in-window, so it is carried
//      forward (to window row 1) rather than re-decoded. Because that row is never
//      re-decoded, the next band's first DECODED row sits at window row 2 and
//      reads window row 1 as its top neighbour — window row 0 is never read.
// Output is bit-identical to the full-frame path.
static void lspr3_decode_intra_slice_banded(
    const uint8_t *payload, size_t payload_size,
    uint16_t width, uint16_t height,
    uint16_t orig_w, uint16_t orig_h,
    uint8_t pic_init_qp, int8_t chroma_qp_index_offset,
    uint8_t *out_yuv, sprite_t *out_sprite, int band_rows,
    int x_div, int y_div,
    const lspr3_load_parms_t *parms
) {
    assertf(payload_size >= 2, "H264I-band: payload too small");
    uint8_t nal_hdr = payload[0];
    nalUnit_t nal = {
        .nalUnitType = (nalUnitType_e)(nal_hdr & 0x1F),
        .nalRefIdc = (nal_hdr >> 5) & 3,
    };

    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    u32 pic_size_in_mbs = (u32)(mb_w * mb_h);
    if (band_rows <= 0 || band_rows > mb_h) band_rows = mb_h;
    int win_rows = band_rows + 2;   // content (1..band_rows) + lookahead + pad row 0

    size_t win_yuv_size = (size_t)(win_rows * mb_w) * 256 + (size_t)(win_rows * mb_w) * 64 * 2;
    uint8_t *win = (uint8_t*)scratch_malloc_uncached(win_yuv_size);
    assertf(win, "H264I-band: out of memory (yuv-window %u)", (unsigned)win_yuv_size);
    image_t image = { .data = win, .width = (u32)mb_w, .height = (u32)mb_h };

    seqParamSet_t sps = {
        .profileIdc = 66, .levelIdc = 22, .seqParameterSetId = 0,
        .maxFrameNum = 1 << 4, .picOrderCntType = 2, .numRefFrames = 1,
        .picWidthInMbs = (u32)mb_w, .picHeightInMbs = (u32)mb_h,
    };
    picParamSet_t pps = {
        .picParameterSetId = 0, .seqParameterSetId = 0, .picOrderPresentFlag = 0,
        .numSliceGroups = 1, .sliceGroupMapType = 0, .picSizeInMapUnits = (u32)pic_size_in_mbs,
        .numRefIdxL0Active = 1, .picInitQp = pic_init_qp, .chromaQpIndexOffset = chroma_qp_index_offset,
        .deblockingFilterControlPresentFlag = 1, .constrainedIntraPredFlag = 0,
        .redundantPicCntPresentFlag = 0,
    };

    uint8_t *rbsp = (uint8_t *)(payload + 1);
    size_t rbsp_size = payload_size - 1;
    strmData_t strm = {0};
    strm.pStart = rbsp;
    strm.pCurr = ((u64)(uintptr_t)rbsp) << 3;
    strm.pEnd = ((u64)(uintptr_t)(rbsp + rbsp_size)) << 3;

    rsph264_begin_frame();

    sliceHeader_t slice = {0};
    u32 slice_status = h264bsdDecodeSliceHeader(&strm, &slice, &sps, &pps, &nal);
    assertf(slice_status == HANTRO_OK, "H264I-band: slice header decode failed");

    size_t mb_bytes = (size_t)pic_size_in_mbs * sizeof(mbStorage_t);
    mbStorage_t *mb;
    if (parms && parms->mb_alloc) mb = (mbStorage_t*)parms->mb_alloc(parms->mb_alloc_ctx, mb_bytes);
    else                          mb = (mbStorage_t*)calloc(pic_size_in_mbs, sizeof(mbStorage_t));
    assertf(mb, "H264I-band: out of memory (mb %u)", (unsigned)mb_bytes);
    h264bsdInitMbNeighbours(mb, (u32)mb_w, pic_size_in_mbs);

    i32 qpY = (i32)pps.picInitQp + slice.sliceQpDelta;
    macroblockLayer_t mbLayers[H264I_MB_RING] = {0};
    rspq_syncpoint_t slot_sync[H264I_MB_RING] = {0};

    // Plane offsets (window vs full-frame). One MB-row = mb_w*256 (luma) /
    // mb_w*64 (each chroma) bytes.
    const size_t win_luma_plane = (size_t)(win_rows * mb_w) * 256;
    const size_t win_cb_plane   = (size_t)(win_rows * mb_w) * 64;
    const size_t full_luma_plane = (size_t)pic_size_in_mbs * 256;
    const size_t full_cb_plane   = (size_t)pic_size_in_mbs * 64;
    const size_t row_luma = (size_t)mb_w * 256;
    const size_t row_cb   = (size_t)mb_w * 64;
    const int stride = mb_w * 16;          /* luma plane width (px) */
    const int uv_stride = stride / 2;

    // Convert-mode setup: build the sprite header and the RDP YUV→RGB session
    // once; each band is blitted into its slice of the output sprite below.
    surface_t target = {0};
    if (out_sprite) {
        uint16_t out_w = orig_w / x_div;
        uint16_t out_h = orig_h / y_div;
        uint8_t *dst = lspr3_setup_rgba16_sprite_header(out_sprite, out_w, out_h);
        size_t pixel_bytes_aligned = ROUND_UP((size_t)out_w * out_h * 2, 16);
        data_cache_hit_writeback_invalidate(dst, pixel_bytes_aligned);
        target = surface_make_linear(dst, FMT_RGBA16, out_w, out_h);
        yuv_init();
    }

    u32 currMbAddr = 0;
    for (int bandStart = 0; bandStart < mb_h; bandStart += band_rows) {
        int nrows = (bandStart + band_rows <= mb_h) ? band_rows : (mb_h - bandStart);
        int contentEnd = bandStart + nrows;
        int hasLA = (contentEnd < mb_h);   // a lookahead MB-row exists below this band
        h264bsdSetImageWindow((u32)bandStart, (u32)win_rows);

        // Decode this band's content rows plus one lookahead row (if any). For
        // bands after the first the band's first content row was already decoded
        // as the previous band's lookahead and carried into window row 1, so
        // currMbAddr already sits past it (at window row 2's global address).
        u32 bandEndMb = (u32)((hasLA ? contentEnd + 1 : contentEnd) * mb_w);
        while (currMbAddr < bandEndMb) {
            u32 slot = currMbAddr % H264I_MB_RING;
            if (slot_sync[slot]) rspq_syncpoint_wait(slot_sync[slot]);
            macroblockLayer_t *mbLayer = &mbLayers[slot];
            mbStorage_t *pMb = mb + currMbAddr;
            pMb->sliceId = 1;
            pMb->disableDeblockingFilterIdc = slice.disableDeblockingFilterIdc;
            pMb->filterOffsetA = slice.sliceAlphaC0Offset;
            pMb->filterOffsetB = slice.sliceBetaOffset;
            pMb->chromaQpIndexOffset = pps.chromaQpIndexOffset;
            u32 s1 = h264bsdDecodeMacroblockLayer(&strm, mbLayer, mb + currMbAddr,
                                                  slice.sliceType, slice.numRefIdxL0Active);
            assertf(s1 == HANTRO_OK, "H264I-band: mb-layer fail at %lu", (unsigned long)currMbAddr);
            u32 s2 = h264bsdDecodeMacroblock(mb + currMbAddr, mbLayer, &image, NULL,
                                             &qpY, currMbAddr, pps.constrainedIntraPredFlag, &slice);
            assertf(s2 == HANTRO_OK, "H264I-band: mb decode fail at %lu", (unsigned long)currMbAddr);
            slot_sync[slot] = rspq_syncpoint_new();
            currMbAddr++;
        }
        rsph264_sync();   // RSP has written this band's MBs into the window

        if (out_yuv) {
            // Verification: assemble window rows [1..nrows] -> full-frame rows.
            memcpy(out_yuv + (size_t)bandStart * row_luma,
                   win + 1 * row_luma, (size_t)nrows * row_luma);
            memcpy(out_yuv + full_luma_plane + (size_t)bandStart * row_cb,
                   win + win_luma_plane + 1 * row_cb, (size_t)nrows * row_cb);
            memcpy(out_yuv + full_luma_plane + full_cb_plane + (size_t)bandStart * row_cb,
                   win + win_luma_plane + win_cb_plane + 1 * row_cb, (size_t)nrows * row_cb);
        } else {
            // Shipping: convert window rows [1..nrows] — plus the lookahead row
            // (row nrows+1) when present — straight into the output sprite. The
            // lookahead row in the blit gives the band's bottom content row its
            // real next chroma row, so the YUV->RGB upsample has no edge-clamp
            // seam at the boundary. The lookahead row's own output lands on the
            // next band's first rows and is overwritten by that band's blit, so
            // only the seam-free content survives.
            int blit_rows = nrows + (hasLA ? 1 : 0);
            yuv_frame_t bf = {
                .y = surface_make_linear(win + 1 * row_luma, FMT_I8, stride, blit_rows * 16),
                .u = surface_make_linear(win + win_luma_plane + 1 * row_cb, FMT_I8, uv_stride, blit_rows * 8),
                .v = surface_make_linear(win + win_luma_plane + win_cb_plane + 1 * row_cb, FMT_I8, uv_stride, blit_rows * 8),
            };
            rdpq_attach(&target, NULL);
            rdpq_mode_push();
            if (x_div > 1 || y_div > 1) {
                rdpq_blitparms_t bp = { .scale_x = 1.0f / x_div, .scale_y = 1.0f / y_div };
                yuv_tex_blit(&bf, 0, (bandStart * 16) / y_div, &bp, &YUV_BT709_FULL);
            } else {
                yuv_tex_blit(&bf, 0, bandStart * 16, NULL, &YUV_BT709_FULL);
            }
            rdpq_mode_pop();
            rdpq_detach_wait();   // RDP done reading the window -> safe to reuse it
        }

        if (hasLA) {
            // The lookahead row (window row nrows+1) IS the next band's first
            // content row, already decoded. Carry it to window row 1 so the next
            // band continues from it without re-decoding; the next band's first
            // decoded row then sits at window row 2 and reads this as its top
            // neighbour. Window is uncached -> the copy is visible to the RSP
            // immediately. (Window row 0 is unused padding; nothing reads it.)
            memcpy(win + 1 * row_luma, win + (size_t)(nrows + 1) * row_luma, row_luma);
            memcpy(win + win_luma_plane + 1 * row_cb,
                   win + win_luma_plane + (size_t)(nrows + 1) * row_cb, row_cb);
            memcpy(win + win_luma_plane + win_cb_plane + 1 * row_cb,
                   win + win_luma_plane + win_cb_plane + (size_t)(nrows + 1) * row_cb, row_cb);
        }
    }
    h264bsdSetImageWindow(0, 0);   // restore full-frame addressing for FMV path
    rsph264_sync();
    if (out_sprite) yuv_close();
    assertf(currMbAddr == pic_size_in_mbs, "H264I-band: incomplete slice");
    if (parms && parms->mb_alloc) parms->mb_free(parms->mb_alloc_ctx, mb);
    else                          free(mb);
    scratch_free(win);
}

// Initialise the RGBA16 sprite header (no pixel data) and return a pointer to
// the sprite's pixel area. Shared by the full-frame and banded converters so
// both produce an identically-laid-out sprite.
static uint8_t *lspr3_setup_rgba16_sprite_header(sprite_t *sprite, uint16_t out_w, uint16_t out_h)
{
    uint8_t preserved_flags = sprite->flags & SPRITE_FLAGS_OWNEDBUFFER;
    size_t header_bytes = ROUND_UP(sizeof(sprite_t) + sizeof(sprite_ext_t), 64);
    sys_hw_memset(sprite, 0, header_bytes);
    sprite->width = out_w;
    sprite->height = out_h;
    sprite->flags = preserved_flags | SPRITE_FLAGS_NODATA | SPRITE_FLAGS_EXT | FMT_RGBA16;
    sprite->hslices = 1;
    sprite->vslices = 1;

    sprite_ext_t *sx = (sprite_ext_t*)sprite->data;
    sx->size = sizeof(sprite_ext_t);
    sx->version = SPRITE_EXT_VERSION;
    sx->data_ptr = (uint32_t)header_bytes;
    return (uint8_t*)sprite + header_bytes;
}

// Build the FMT_RGBA16 sprite from the I420 reconstruction. The RDP YUV
// combiner does the YUV→RGB conversion on the fly (BT.709 full range, hard
// coded to match the encoder's rgba_to_i420), with bilinear chroma upsample
// as a side benefit. If x_divisor and/or y_divisor are > 1 the source is
// also downsampled in that axis by the RDP's bilinear filter during the blit.
static void lspr3_build_rgba16_sprite(
    sprite_t *sprite, const uint8_t *pic,
    uint16_t orig_w, uint16_t orig_h,
    int stride, int luma_h,
    int x_divisor, int y_divisor
) {
    if (x_divisor < 1) x_divisor = 1;
    if (y_divisor < 1) y_divisor = 1;
    uint16_t out_w = orig_w / x_divisor;
    uint16_t out_h = orig_h / y_divisor;
    uint8_t *dst = lspr3_setup_rgba16_sprite_header(sprite, out_w, out_h);

    int uv_stride = stride / 2;
    size_t y_bytes  = (size_t)stride * luma_h;
    size_t uv_bytes = (size_t)uv_stride * (luma_h / 2);
    const uint8_t *y_plane = pic;
    const uint8_t *u_plane = pic + y_bytes;
    const uint8_t *v_plane = u_plane + uv_bytes;

    yuv_frame_t frame = {
        .y = surface_make_linear((void*)y_plane, FMT_I8, stride,    luma_h),
        .u = surface_make_linear((void*)u_plane, FMT_I8, uv_stride, luma_h / 2),
        .v = surface_make_linear((void*)v_plane, FMT_I8, uv_stride, luma_h / 2),
    };

    // The RDP writes through to RAM bypassing the CPU cache. Discard
    // any (possibly speculative) cached lines for the destination so a
    // future read of `dst` doesn't return stale data.
    size_t pixel_bytes = (size_t)out_w * out_h * 2;
    size_t pixel_bytes_aligned = ROUND_UP(pixel_bytes, 16);
    data_cache_hit_writeback_invalidate(dst, pixel_bytes_aligned);
    surface_t target_surf = surface_make_linear(dst, FMT_RGBA16, out_w, out_h);

    yuv_init();
    {
        rdpq_attach(&target_surf, NULL);
        {
            // yuv_tex_blit puts the RDP in YUV mode. Without push/pop the YUV
            // combiner state leaks back to the caller's render mode, which makes
            // the next non-YUV draw on the screen surface emit magenta/purple
            // pixels until the caller next sets a render mode.
            rdpq_mode_push();
            {
                if (x_divisor > 1 || y_divisor > 1) {
                    // Downsample during the blit. The RDP bilinear filter
                    // handles sub-pixel sampling, so a 0.5x scale produces
                    // a clean downsample (chroma is naturally 2x2 upsampled
                    // before the combiner so the chroma plane's sample
                    // density is preserved through the downscale).
                    rdpq_blitparms_t bp = {
                        .scale_x = 1.0f / (float)x_divisor,
                        .scale_y = 1.0f / (float)y_divisor,
                    };
                    yuv_tex_blit(&frame, 0, 0, &bp, &YUV_BT709_FULL);
                } else {
                    yuv_tex_blit(&frame, 0, 0, NULL, &YUV_BT709_FULL);
                }
            }
            rdpq_mode_pop();
        }
        rdpq_detach_wait();
    }
    yuv_close();
}

// Apply the 1-bit alpha bitmap to the decoded RGBA5551 sprite. Each set bit
// marks a transparent pixel (see build_alpha1_bitmap() in the encoder): the
// whole pixel is cleared to 0x0000 (RGB and alpha both zero, like BC1Q's
// pal[3]) so bilinear filtering at draw time cannot bleed color out of
// transparent texels. The bitmap is row-major, ceil(w/8) bytes per row,
// MSB-first, at the sprite's (cropped) width/height.
static void lspr3_apply_alpha1(uint16_t *dst, uint16_t w, uint16_t h,
                               const uint8_t *bitmap)
{
    int row_bytes = (w + 7) / 8;
    for (int y = 0; y < h; y++) {
        const uint8_t *brow = bitmap + (size_t)y * row_bytes;
        uint16_t *drow = dst + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            if (brow[x >> 3] & (0x80 >> (x & 7)))
                drow[x] = 0x0000;
        }
    }
    // The RGBA5551 buffer was written by the RDP straight to RDRAM (its cache
    // lines were invalidated before), so our CPU edits sit in cache: flush them
    // back so the sprite is coherent when the RDP samples it at draw time.
    data_cache_hit_writeback(dst, ROUND_UP((size_t)w * h * 2, 16));
}

#if LSPR3_TRACE_MEM
static void lspr3_memdump(const char *tag) {
    struct mallinfo mi = mallinfo();
    scratch_stats_t ss;
    scratch_get_stats(&ss);
    debugf("n64: MEMF [%s] heap.uord=%u ford=%u keep=%u | scratch.live=%u resv=%u\n",
        tag,
        (unsigned)mi.uordblks, (unsigned)mi.fordblks, (unsigned)mi.keepcost,
        (unsigned)ss.live_bytes, (unsigned)ss.reserved_bytes);
}
#endif

sprite_t *lspr3_load_buf_ex(const void *encoded_buf, int encoded_sz,
                            const lspr3_load_parms_t *parms)
{
    lspr3_memdump("lspr3-entry");
    int x_divisor = (parms && parms->output_x_divisor > 0) ? parms->output_x_divisor : 1;
    int y_divisor = (parms && parms->output_y_divisor > 0) ? parms->output_y_divisor : 1;

    // Allocate the buffer for the decoded sprite. Default path uses memalign
    // with H264I_BUF_ALIGN so the post-header pixel area lands on a 64-byte
    // boundary (required by rdpq_set_color_image when the pixel area is
    // used as a YUV blit target). A caller-provided allocator is responsible
    // for satisfying the same alignment — typically by passing the
    // alignment through to its own aligned allocator.
    size_t decoded_sz = lspr3_decoded_size_buf(encoded_buf, encoded_sz, x_divisor, y_divisor);
    assertf(decoded_sz > 0, "Invalid H264I buffer");
    sprite_t *sprite;
    if (parms && parms->alloc) {
        sprite = (sprite_t *)parms->alloc(parms->alloc_ctx, decoded_sz, H264I_BUF_ALIGN);
    } else {
        sprite = (sprite_t *)memalign(H264I_BUF_ALIGN, decoded_sz);
    }
    assertf(sprite, "H264I: out of memory (sprite %u)", (unsigned)decoded_sz);
    sprite->flags = SPRITE_FLAGS_OWNEDBUFFER;
    lspr3_memdump("post-sprite-alloc");

    // Decode the H264I bitstream into YUV planes. The transient yuv +
    // macroblock-storage buffers always go through scratch — they're
    // short-lived and stack cleanly with no fragmentation impact on the
    // caller's heap. When a 1-bit alpha bitmap is present it is appended
    // after the H.264 payload, so the payload is shorter than the whole
    // buffer by alpha_size bytes.
    const lspr3_header_t *hdr = (const lspr3_header_t *)encoded_buf;
    size_t alpha_size = (hdr->flags & LSPR3_FLAG_ALPHA1) ? hdr->alpha_size : 0;
    // 1-bit alpha is authored at the source resolution; it can't be applied to
    // a divisor-downsampled buffer. Fail loudly rather than corrupt memory.
    assertf(!(alpha_size && (x_divisor > 1 || y_divisor > 1)),
        "H264I: 1-bit alpha is not supported together with output divisors");
    assertf((size_t)encoded_sz >= sizeof(lspr3_header_t) + alpha_size,
        "H264I buffer truncated (sz=%d, alpha_size=%zu)", encoded_sz, alpha_size);
    size_t payload_size = (size_t)encoded_sz - sizeof(lspr3_header_t) - alpha_size;
    const int mb_w = (hdr->width + 15) / 16;
    const int mb_h = (hdr->height + 15) / 16;
    const int stride = mb_w * 16;
    const int luma_h = mb_h * 16;
    const int band_rows = (parms && parms->band_rows > 0) ? parms->band_rows : 0;

    if (band_rows > 0) {
        // Banded: reconstruct + convert band-by-band straight into the sprite;
        // no full-frame YUV is ever resident (the memory win).
        lspr3_decode_intra_slice_banded(
            hdr->payload, payload_size, hdr->width, hdr->height,
            hdr->orig_width, hdr->orig_height,
            hdr->pic_init_qp, hdr->chroma_qp_index_offset,
            NULL, sprite, band_rows, x_divisor, y_divisor, parms);
    } else {
        // Full-frame: decode the whole frame, then convert in one pass.
        uint8_t *pic = NULL;
        size_t pic_size = 0;
        lspr3_decode_intra_slice(
            hdr->payload, payload_size, hdr->width, hdr->height,
            hdr->pic_init_qp, hdr->chroma_qp_index_offset, &pic, &pic_size, parms);
        lspr3_build_rgba16_sprite(sprite, pic, hdr->orig_width, hdr->orig_height,
                                  stride, luma_h, x_divisor, y_divisor);
        scratch_free(pic);
    }

#if LSPR3_VERIFY_BANDING
    // Produce the sprite the OTHER way and diff the pixel areas. Needs a 2nd
    // sprite-sized scratch buffer -> run this on 8 MiB. The full-frame and
    // banded converts should be identical except possibly the chroma-upsample
    // rows at band seams, so the report includes the diff count + first row.
    {
        sprite_t *spr2 = (parms && parms->alloc)
            ? (sprite_t*)parms->alloc(parms->alloc_ctx, decoded_sz, H264I_BUF_ALIGN)
            : (sprite_t*)memalign(H264I_BUF_ALIGN, decoded_sz);
        if (spr2) {
            spr2->flags = SPRITE_FLAGS_OWNEDBUFFER;
            const int vbr = (band_rows > 0) ? 0 : LSPR3_VERIFY_BAND_ROWS;
            uint32_t tv0 = TICKS_READ();
            if (vbr > 0) {
                lspr3_decode_intra_slice_banded(
                    hdr->payload, payload_size, hdr->width, hdr->height,
                    hdr->orig_width, hdr->orig_height,
                    hdr->pic_init_qp, hdr->chroma_qp_index_offset,
                    NULL, spr2, vbr, x_divisor, y_divisor, parms);
            } else {
                uint8_t *p2 = NULL; size_t ps2 = 0;
                lspr3_decode_intra_slice(hdr->payload, payload_size, hdr->width, hdr->height,
                    hdr->pic_init_qp, hdr->chroma_qp_index_offset, &p2, &ps2, parms);
                lspr3_build_rgba16_sprite(spr2, p2, hdr->orig_width, hdr->orig_height,
                                          stride, luma_h, x_divisor, y_divisor);
                scratch_free(p2);
            }
            unsigned us = (unsigned)TICKS_TO_US(TICKS_READ() - tv0);
            const size_t hdr_bytes = ROUND_UP(sizeof(sprite_t) + sizeof(sprite_ext_t), 64);
            const size_t px = (size_t)sprite->width * sprite->height * 2;
            const uint8_t *a = (const uint8_t*)sprite + hdr_bytes;
            const uint8_t *b = (const uint8_t*)spr2 + hdr_bytes;
            size_t first = 0, ndiff = 0;
            for (size_t k = 0; k < px; k++) { if (a[k] != b[k]) { if (!ndiff) first = k; ndiff++; } }
            if (ndiff) {
                debugf("n64: *** SPRITE DIFF brPrimary=%d vbr=%d first@%u row=%u ndiff=%u/%u (%.2f%%) ***\n",
                    band_rows, vbr, (unsigned)first, (unsigned)(first / (sprite->width * 2)),
                    (unsigned)ndiff, (unsigned)px, 100.0 * (double)ndiff / (double)px);
            } else {
                debugf("n64: SPRITE OK vbr=%d (%u px-bytes identical, %uus)\n", vbr, (unsigned)px, us);
            }
            if (parms && parms->alloc) scratch_free(spr2); else free(spr2);
        } else {
            debugf("n64: sprite-verify SKIP (no scratch for 2nd sprite; need 8 MiB)\n");
        }
    }
#endif

    // Re-apply the lossless 1-bit alpha mask (if any) on top of the RGBA5551
    // pixels the RDP just produced (the YUV combiner writes alpha=1 everywhere).
    // The mask is applied at source resolution to the final sprite, so it works
    // for both the full-frame and banded paths (divisors are ruled out above).
    if (alpha_size) {
        size_t header_bytes = ROUND_UP(sizeof(sprite_t) + sizeof(sprite_ext_t), 64);
        uint16_t *dst = (uint16_t *)((uint8_t *)sprite + header_bytes);
        const uint8_t *bitmap = hdr->payload + payload_size;
        lspr3_apply_alpha1(dst, hdr->orig_width, hdr->orig_height, bitmap);
    }

    return sprite;
}

// Default decoder callback registered with sprite_load_buf: uses memalign
// for the output (callers that go through sprite_load / sprite_load_buf
// can pass the result to sprite_free unchanged).
static sprite_t *lspr3_load_buf(const void *encoded_buf, int encoded_sz) {
    return lspr3_load_buf_ex(encoded_buf, encoded_sz, NULL);
}

static int lspr3_init_refcount = 0;
static sprite_decoder_t *lspr3_decoder = NULL;

void lspr3_init(void)
{
    // Just increment the refcount if already initialized.
    if (lspr3_init_refcount++ > 0) return;

    assertf(lspr3_decoder == NULL, "lspr3 is already initialized");
    lspr3_decoder = sprite_decoder_register(lspr3_is_encoded, lspr3_load_buf);
    // Register and upload the H.264 RSP overlays used by the slice decoder.
    rsph264_init();
}

void lspr3_close(void)
{
	// Just decrement the refcount if there are still dangling references.
    if (--lspr3_init_refcount > 0) return;

    assertf(lspr3_decoder != NULL, "lspr3 is not initialized");
    sprite_decoder_unregister(lspr3_decoder);
    lspr3_decoder = NULL;
}
