/**
 * @file lossysprite.c
 * @brief LSPR (lossy sprite) decoder
 */

#include "lossysprite.h"

///@cond
// Match the layout used inside the h264_decoder TU (h264_decoder.c defines
// this before including all internal .c files). Without it, mbStorage_t here
// gains an extra u32 `decoded` field, shifting mbA/mbB/mbC/mbD off the offset
// h264bsdInitMbNeighbours writes to — neighbour pointers come back NULL.
#define OPTIMIZE_NO_DECODED_FLAG
///@endcond

#include "video/h264_decoder.h"
#include "video/h264_decoder/h264bsd_macroblock_layer.h"
#include "video/h264_decoder/h264bsd_neighbour.h"
#include "video/h264_decoder/h264bsd_pic_param_set.h"
#include "video/h264_decoder/h264bsd_seq_param_set.h"
#include "video/h264_decoder/h264bsd_slice_header.h"
#include "video/h264_decoder/h264bsd_util.h"
#include "video/rsph264_internal.h"
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

/** @brief LSPR version number. */
#define LSPR_VERSION 4

///@cond
#define ALIGN64(n) (((n) + 63) & ~63)
///@endcond

/**
 * @brief Header structure for LSPR-encoded files.
 *
 * Must mirror the layout used in tools/mksprite/mksprite_lossy.cpp
 */
typedef struct lspr_header_s {
    uint8_t magic[LSPR_FILE_MAGIC_SIZE];
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
    uint8_t  payload[];
} lspr_header_t;

static bool lspr_is_encoded(const void *buf, int sz) {
    if (!buf || sz < (int)sizeof(lspr_header_t)) return false;
    const lspr_header_t *hdr = (const lspr_header_t *)buf;
    return memcmp(hdr->magic, LSPR_FILE_MAGIC, LSPR_FILE_MAGIC_SIZE) == 0
           && hdr->version == LSPR_VERSION;
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
    ///@cond
    #define LSPR_MB_RING_SIZE 32
    ///@endcond
    macroblockLayer_t *mbLayers = (macroblockLayer_t*)calloc(LSPR_MB_RING_SIZE, sizeof(macroblockLayer_t));
    assertf(mbLayers, "LSPR: out of memory");
    u32 ring_idx = 0;

    while (currMbAddr < pic_size_in_mbs) {
        macroblockLayer_t *mbLayer = &mbLayers[ring_idx];
        ring_idx = (ring_idx + 1) % LSPR_MB_RING_SIZE;

        mbStorage_t *pMb = mb + currMbAddr;
        pMb->sliceId = 1;
        pMb->disableDeblockingFilterIdc = slice.disableDeblockingFilterIdc;
        pMb->filterOffsetA = slice.sliceAlphaC0Offset;
        pMb->filterOffsetB = slice.sliceBetaOffset;
        pMb->chromaQpIndexOffset = pps.chromaQpIndexOffset;
        
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

    assertf(currMbAddr == pic_size_in_mbs, "LSPR: incomplete slice");

    free(mbLayers);
    free(mb);

    *out_yuv = yuv;
    *out_yuv_size = yuv_size;
}

// Build the FMT_RGBA16 sprite from the I420 reconstruction. The RDP YUV
// combiner does the YUV→RGB conversion on the fly (BT.709 full range, hard
// coded to match the encoder's rgba_to_i420), with bilinear chroma upsample
// as a side benefit.
static void lspr_build_rgba16_sprite_into(sprite_t *spr,
                                          const uint8_t *pic,
                                          uint16_t orig_w, uint16_t orig_h,
                                          int stride, int luma_h) {
    size_t pixel_bytes = (size_t)orig_w * orig_h * 2;
    size_t pixel_bytes_aligned = (pixel_bytes + 15) & ~(size_t)15;

    size_t header_bytes = ALIGN64(sizeof(sprite_t) + sizeof(sprite_ext_t));
    memset(spr, 0, header_bytes);
    spr->width = orig_w;
    spr->height = orig_h;
    // Caller owns the buffer; OWNEDBUFFER is set later by lossysprite_load_buf
    // when it allocated via memalign. Callers of lossysprite_load_into keep
    // ownership and the flag stays clear.
    spr->flags = SPRITE_FLAGS_NODATA | SPRITE_FLAGS_EXT | FMT_RGBA16;
    spr->hslices = 1;
    spr->vslices = 1;

    sprite_ext_t *sx = (sprite_ext_t*)spr->data;
    sx->size = sizeof(sprite_ext_t);
    sx->version = SPRITE_EXT_VERSION;
    sx->flags = 0;
    sx->data_ptr = (uint32_t)header_bytes;

    int uv_stride = stride / 2;
    size_t y_bytes  = (size_t)stride * luma_h;
    size_t uv_bytes = (size_t)uv_stride * (luma_h / 2);
    const uint8_t *y_plane = pic;
    const uint8_t *u_plane = pic + y_bytes;
    const uint8_t *v_plane = u_plane + uv_bytes;
    uint8_t *dst = (uint8_t*)spr + header_bytes;

    yuv_frame_t frame = {
        .y = surface_make_linear((void*)y_plane, FMT_I8, stride,    luma_h),
        .u = surface_make_linear((void*)u_plane, FMT_I8, uv_stride, luma_h / 2),
        .v = surface_make_linear((void*)v_plane, FMT_I8, uv_stride, luma_h / 2),
    };

    // The RDP writes through to RAM bypassing the CPU cache. Discard
    // any (possibly speculative) cached lines for the destination so a
    // future read of `dst` doesn't return stale data.
    data_cache_hit_writeback_invalidate(dst, pixel_bytes_aligned);
    surface_t target_surf = surface_make_linear(dst, FMT_RGBA16, orig_w, orig_h);

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
                yuv_tex_blit(&frame, 0, 0, NULL, &YUV_BT709_FULL);
            }
            rdpq_mode_pop();
        }
        rdpq_detach_wait();
    }
    yuv_close();
}

size_t lossysprite_decoded_size_buf(const void *encoded_buf, int encoded_sz) {
    if (!lspr_is_encoded(encoded_buf, encoded_sz)) return 0;
    const lspr_header_t *hdr = (const lspr_header_t *)encoded_buf;

    size_t pixel_bytes = (size_t)hdr->orig_width * hdr->orig_height * 2;
    size_t aligned_bytes = (pixel_bytes + 15) & ~(size_t)15;

    size_t header_bytes = ALIGN64(sizeof(sprite_t) + sizeof(sprite_ext_t));
    return header_bytes + aligned_bytes;
}

static sprite_decoder_t *lossysprite_decoder = NULL;

void lossysprite_init(void)
{
    assertf(!lossysprite_decoder, "lossysprite_init is already initialized");
    lossysprite_decoder = sprite_decoder_register(lspr_is_encoded, lossysprite_load_buf);
}

void lossysprite_close(void)
{
    sprite_decoder_unregister(lossysprite_decoder);
    lossysprite_decoder = NULL;
}

sprite_t *lossysprite_load_into(const void *buf, int sz, void *out, size_t out_sz) {
    size_t decoded_sz = lossysprite_decoded_size_buf(buf, sz);
    assertf(decoded_sz > 0, "Invalid LSPR buffer");
    const lspr_header_t *hdr = (const lspr_header_t *)buf;

    assertf(out, "lossysprite_load_into: NULL output buffer");
    assertf(out_sz >= decoded_sz,
            "lossysprite_load_into: output buffer too small (%u < %u)",
            (unsigned)out_sz, (unsigned)decoded_sz);
    assertf(((uintptr_t)out & (LOSSYSPRITE_DECODE_ALIGN - 1)) == 0,
            "lossysprite_load_into: output buffer must be %d-byte aligned",
            LOSSYSPRITE_DECODE_ALIGN);

    uint16_t width = hdr->width;
    uint16_t height = hdr->height;
    uint16_t orig_w = hdr->orig_width;
    uint16_t orig_h = hdr->orig_height;
    const uint8_t *payload = hdr->payload;
    size_t payload_size = (size_t)sz - sizeof(lspr_header_t);

    rsph264_init();
    rsph264_begin_frame();

    uint8_t *pic = NULL;
    size_t pic_size = 0;
    lspr_decode_intra_slice(payload, payload_size, width, height,
                            hdr->pic_init_qp, hdr->chroma_qp_index_offset,
                            &pic, &pic_size);

    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    int stride = mb_w * 16;
    int luma_h = mb_h * 16;

    sprite_t *spr = (sprite_t *)out;
    lspr_build_rgba16_sprite_into(spr, pic, orig_w, orig_h, stride, luma_h);

    free_uncached(pic);

    return spr;
}

sprite_t *lossysprite_load_buf(const void *buf, int sz) {
    size_t decoded_sz = lossysprite_decoded_size_buf(buf, sz);
    assertf(decoded_sz > 0, "Invalid LSPR buffer");
    sprite_t *spr = (sprite_t *)memalign(LOSSYSPRITE_DECODE_ALIGN, decoded_sz);
    assertf(spr, "Out of memory");
    lossysprite_load_into(buf, sz, spr, decoded_sz);
    // load_into leaves OWNEDBUFFER clear; we allocated, so set it.
    spr->flags |= SPRITE_FLAGS_OWNEDBUFFER;
    return spr;
}

sprite_t* lossysprite_load(const char *fn) {
    int sz = 0;
    uint8_t *enc = asset_load(fn, &sz);
    sprite_t *spr = lossysprite_load_buf(enc, sz);
    free(enc);
    return spr;
}
