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
#include "asset.h"
#include "sprite.h"
#include "sprite_internal.h"
#include "n64sys.h"
#include "surface.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

#define LSPR_MAGIC "LSPR"
#define LSPR_VERSION 3

enum {
    LSPR_YUV_420 = 0,
    LSPR_YUV_422 = 1,
    LSPR_YUV_444 = 2,
    LSPR_YUV_400 = 3,
};

typedef struct lspr_header_s {
    uint8_t magic[4];
    uint16_t version;
    uint16_t flags;
    uint16_t width;
    uint16_t height;
    uint16_t orig_width;
    uint16_t orig_height;
    uint8_t payload[];
} lspr_header_t;

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
    memset(yuv, 0, yuv_size);

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
        .picInitQp = 26,
        .chromaQpIndexOffset = 0,
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
    macroblockLayer_t mbLayer;
    memset(&mbLayer, 0, sizeof(mbLayer));

    while (currMbAddr < pic_size_in_mbs) {
        lspr_set_mb_params(mb + currMbAddr, &slice, 1, pps.chromaQpIndexOffset);
        u32 mb_layer_status = h264bsdDecodeMacroblockLayer(&strm, &mbLayer, mb + currMbAddr,
                                                           slice.sliceType, slice.numRefIdxL0Active);
        assertf(mb_layer_status == HANTRO_OK, "LSPR: macroblock layer decode failed at mb=%lu",
                (unsigned long)currMbAddr);
        assertf(IS_INTRA_MB(mbLayer), "LSPR: inter MB not supported (mb=%lu type=%d)",
                (unsigned long)currMbAddr, (int)mbLayer.mbType);
        u32 mb_status = h264bsdDecodeMacroblock(mb + currMbAddr, &mbLayer, &image, NULL,
                                                &qpY, currMbAddr, pps.constrainedIntraPredFlag,
                                                &slice);
        assertf(mb_status == HANTRO_OK, "LSPR: macroblock decode failed at mb=%lu",
                (unsigned long)currMbAddr);
        // Drain the RSP queue between MBs. The CAVLC residual buffer
        // (mbLayer.residual.posCoefBuf) is reused across MBs, and the RSP's
        // SET_PACKED_DELTA_BUFFER task DMAs from it asynchronously; without a
        // sync, MB N+1's CAVLC writes can race the DMA for MB N's residual.
        rsph264_sync();
        currMbAddr++;
        if (!h264bsdMoreRbspData(&strm))
            break;
    }

    assertf(currMbAddr == pic_size_in_mbs, "LSPR: incomplete slice");

    free(mb);

    *out_yuv = yuv;
    *out_yuv_size = yuv_size;
}

sprite_t *__lossysprite_decode_buf(const void *buf, int sz) {
    assertf(buf && sz >= (int)sizeof(lspr_header_t), "Invalid LSPR buffer");

    const lspr_header_t *hdr = (const lspr_header_t *)buf;
    assertf(memcmp(hdr->magic, LSPR_MAGIC, 4) == 0, "Invalid LSPR magic");
    assertf(hdr->version == LSPR_VERSION, "Invalid LSPR version");
    assertf((hdr->flags & 0x3) == LSPR_YUV_420, "Invalid LSPR YUV format");

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
    lspr_decode_intra_slice(payload, payload_size, width, height, &pic, &pic_size);

    assertf((orig_w & 1) == 0, "LSPR: orig_width must be even for FMT_YUV16 (got %u)", orig_w);

    int mb_w = (width + 15) / 16;
    int mb_h = (height + 15) / 16;
    int stride = mb_w * 16;
    int luma_h = mb_h * 16;
    int chroma_stride = stride / 2;

    size_t pixel_bytes = (size_t)orig_w * (size_t)orig_h * 2;
    size_t pixel_bytes_aligned = (pixel_bytes + 15) & ~(size_t)15;
    size_t header_bytes = sizeof(sprite_t) + sizeof(sprite_ext_t);
    size_t pixel_off = (header_bytes + 63) & ~(size_t)63;
    size_t total_bytes = pixel_off + pixel_bytes_aligned;
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
    sx->colorspace = SPRITE_COLORSPACE_BT709_FULL;
    sx->data_ptr = (uint32_t)pixel_off;

    // Pack the 4:2:0 planar YUV that the H.264 decoder produced directly into
    // UYVY 4:2:2 (FMT_YUV16) — the RDP-native packed layout. Each chroma row
    // is reused for two luma rows (4:2:0 → 4:2:2 vertical doubling), and the
    // crop from padded MB-aligned dimensions to (orig_w, orig_h) happens
    // inline by iterating only over the visible pixels.
    uint8_t *dst_base = (uint8_t*)spr + pixel_off;
    size_t dst_stride = (size_t)orig_w * 2;
    const uint8_t *u_plane = pic + (size_t)stride * luma_h;
    const uint8_t *v_plane = u_plane + (size_t)chroma_stride * (luma_h / 2);
    for (int y = 0; y < orig_h; y++) {
        const uint8_t *yrow = pic + (size_t)y * stride;
        const uint8_t *urow = u_plane + (size_t)(y >> 1) * chroma_stride;
        const uint8_t *vrow = v_plane + (size_t)(y >> 1) * chroma_stride;
        uint8_t *dst = dst_base + (size_t)y * dst_stride;
        for (int x = 0; x < orig_w; x += 2) {
            dst[(x << 1) + 0] = urow[x >> 1];
            dst[(x << 1) + 1] = yrow[x];
            dst[(x << 1) + 2] = vrow[x >> 1];
            dst[(x << 1) + 3] = yrow[x + 1];
        }
    }
    // The RDP will load_block the packed pixels via DMA; flush the CPU
    // writes so the RDP sees them.
    data_cache_hit_writeback(dst_base, pixel_bytes_aligned);

    free_uncached(pic);

    return spr;
}

sprite_t* lossysprite_load(const char *fn) {
    int sz = 0;
    uint8_t *enc = asset_load(fn, &sz);
    sprite_t *spr = __lossysprite_decode_buf(enc, sz);
    free(enc);
    return spr;
}
