/**
 * @file lspr3.c
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
#include "surface.h"
#include "utils.h"
#include "yuv.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

/** @brief H264I version number. */
#define H264I_VERSION 4

/** @brief H264I decode macroblock ring size */
#define H264I_MB_RING 2

/** @brief Required alignment of the decoded sprite buffer. */
#define H264I_BUF_ALIGN 64

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
    uint8_t  payload[];
} lspr3_header_t;

static bool lspr3_is_encoded(const void *buf, int sz) {
    if (!buf || sz < (int)sizeof(lspr3_header_t)) return false;
    const lspr3_header_t *hdr = (const lspr3_header_t *)buf;
    return memcmp(hdr->magic, H264I_FILE_MAGIC, H264I_FILE_MAGIC_SIZE) == 0
           && hdr->version == H264I_VERSION;
}

static size_t lspr3_decoded_size_buf(const void *encoded_buf, int encoded_sz) {
    if (!lspr3_is_encoded(encoded_buf, encoded_sz)) return 0;
    const lspr3_header_t *hdr = (const lspr3_header_t *)encoded_buf;
    size_t pixel_bytes = hdr->orig_width * hdr->orig_height * 2;
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
    size_t *out_yuv_size
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
    uint8_t *yuv = (uint8_t*)malloc_uncached(yuv_size);
    assertf(yuv, "H264I: out of memory");
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

    mbStorage_t *mb = (mbStorage_t*)calloc(pic_size_in_mbs, sizeof(mbStorage_t));
    assertf(mb, "H264I: out of memory");
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
    free(mb);

    *out_yuv = yuv;
    *out_yuv_size = yuv_size;
}

// Build the FMT_RGBA16 sprite from the I420 reconstruction. The RDP YUV
// combiner does the YUV→RGB conversion on the fly (BT.709 full range, hard
// coded to match the encoder's rgba_to_i420), with bilinear chroma upsample
// as a side benefit.
static void lspr3_build_rgba16_sprite(
    sprite_t *sprite, const uint8_t *pic,
    uint16_t orig_w, uint16_t orig_h,
    int stride, int luma_h
) {
    uint8_t preserved_flags = sprite->flags & SPRITE_FLAGS_OWNEDBUFFER;
    size_t header_bytes = ROUND_UP(sizeof(sprite_t) + sizeof(sprite_ext_t), 64);
    memset(sprite, 0, header_bytes);
    sprite->width = orig_w;
    sprite->height = orig_h;
    sprite->flags = preserved_flags | SPRITE_FLAGS_NODATA | SPRITE_FLAGS_EXT | FMT_RGBA16;
    sprite->hslices = 1;
    sprite->vslices = 1;

    sprite_ext_t *sx = (sprite_ext_t*)sprite->data;
    sx->size = sizeof(sprite_ext_t);
    sx->version = SPRITE_EXT_VERSION;
    sx->data_ptr = (uint32_t)header_bytes;

    int uv_stride = stride / 2;
    size_t y_bytes  = (size_t)stride * luma_h;
    size_t uv_bytes = (size_t)uv_stride * (luma_h / 2);
    const uint8_t *y_plane = pic;
    const uint8_t *u_plane = pic + y_bytes;
    const uint8_t *v_plane = u_plane + uv_bytes;
    uint8_t *dst = (uint8_t*)sprite + header_bytes;

    yuv_frame_t frame = {
        .y = surface_make_linear((void*)y_plane, FMT_I8, stride,    luma_h),
        .u = surface_make_linear((void*)u_plane, FMT_I8, uv_stride, luma_h / 2),
        .v = surface_make_linear((void*)v_plane, FMT_I8, uv_stride, luma_h / 2),
    };

    // The RDP writes through to RAM bypassing the CPU cache. Discard
    // any (possibly speculative) cached lines for the destination so a
    // future read of `dst` doesn't return stale data.
    size_t pixel_bytes = (size_t)orig_w * orig_h * 2;
    size_t pixel_bytes_aligned = ROUND_UP(pixel_bytes, 16);
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

static sprite_t *lspr3_load_buf(const void *encoded_buf, int encoded_sz) {
    // Allocate the buffer for the decoded sprite.
    size_t decoded_sz = lspr3_decoded_size_buf(encoded_buf, encoded_sz);
    assertf(decoded_sz > 0, "Invalid H264I buffer");
    sprite_t *sprite = (sprite_t *)memalign(H264I_BUF_ALIGN, decoded_sz);
    assertf(sprite, "Out of memory");
    sprite->flags = SPRITE_FLAGS_OWNEDBUFFER;

    // Decode the H264I bitstream into YUV planes
    const lspr3_header_t *hdr = (const lspr3_header_t *)encoded_buf;
    size_t payload_size = (size_t)encoded_sz - sizeof(lspr3_header_t);
    uint8_t *pic = NULL;
    size_t pic_size = 0;
    lspr3_decode_intra_slice(
        hdr->payload, payload_size,
        hdr->width, hdr->height,
        hdr->pic_init_qp, hdr->chroma_qp_index_offset,
        &pic, &pic_size
    );

    // Convert the YUV planes into the final RGBA16 sprite
    int mb_w = (hdr->width + 15) / 16;
    int mb_h = (hdr->height + 15) / 16;
    int stride = mb_w * 16;
    int luma_h = mb_h * 16;
    lspr3_build_rgba16_sprite(
        sprite, pic,
        hdr->orig_width, hdr->orig_height,
        stride, luma_h
    );
    free_uncached(pic);
    return sprite;
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
