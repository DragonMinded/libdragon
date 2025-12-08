#include "rsph264_internal.h"
#include "rspq.h"
#include "fastcache.h"
#include <assert.h>

DEFINE_RSP_UCODE(rsph264_inter);
static uint32_t rsph264_inter_ovl_id = 0;

enum {
    // Tasks in rsph264_inter
    TASK_OMX_INTERPOLATE_CHROMA                = 0,
    TASK_OMX_INTERPOLATE_LUMA                  = 1,
    TASK_OMX_DEQUANT_TRANSFORM_RESIDUAL        = 2,
    TASK_OMX_TRANSFORM_DEQUANT_LUMADC          = 3,
    TASK_OMX_TRANSFORM_DEQUANT_CHROMADC        = 4,
    TASK_PROCESS_LUMA_INTER_RESIDUAL           = 5,
    TASK_PROCESS_CHROMA_RESIDUAL               = 6,
    TASK_OMX_INTERPOLATE_LUMA_OVERFILL         = 7,
    TASK_OMX_INTERPOLATE_CHROMA_OVERFILL       = 8,
};

void rsph264_init(void)
{
    rspq_init();
    rsph264_inter_ovl_id = rspq_overlay_register(&rsph264_inter);
}

void rsph264_begin_frame(void)
{
    
}

void rsph264_end_frame(void)
{
    
}

void rsph264_sync(void)
{
    rspq_wait();
}

void rsph264_queue_debug_random_status(void)
{
    // TODO: implement
}

void rsph264_queue_interpolate_luma_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst, uint32_t dst_pitch,
    uint32_t frame_size, uint32_t block_size, uint32_t mv, uint32_t pos
) {
    // Our RSP implementation assumes that dst is aligned to
    // the block size, as it should be a macroblock partition.
    int block_width = block_size >> 16;
    int block_height = block_size & 0xFFFF;

    assert(((uint32_t)dst & (block_width-1)) == 0);
    assert((cache_flags & RSPH264_CACHE_SKIP_SOURCE) != 0);

    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        for (int i=0;i<block_height;i++)
             fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, block_width);
    }

    rspq_write(rsph264_inter_ovl_id, TASK_OMX_INTERPOLATE_LUMA_OVERFILL,
        PhysicalAddr(frame), frame_pitch,
        PhysicalAddr(dst), dst_pitch,
        frame_size, block_size,
        mv, pos,
        0,    // Just luma buffer
        0,
        0);
}

void rsph264_queue_interpolate_chroma_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst, uint32_t dst_pitch,
    uint32_t frame_size, uint32_t block_size, uint32_t mv, uint32_t pos
) {
    // Our RSP implementation assumes that dst is aligned to
    // the block size, as it should be a macroblock partition.
    int block_width = block_size >> 16;
    int block_height = block_size & 0xFFFF;

    assert(((uint32_t)dst & (block_width-1)) == 0);
    assert((cache_flags & RSPH264_CACHE_SKIP_SOURCE) != 0);

    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        for (int i=0;i<block_height;i++)
             fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, block_width);
    }
    
    rspq_write(rsph264_inter_ovl_id, TASK_OMX_INTERPOLATE_CHROMA_OVERFILL,
        PhysicalAddr(frame), frame_pitch,
        PhysicalAddr(dst), dst_pitch,
        frame_size, block_size,
        mv, pos,
        0,    // Just 1 chroma buffer
        0,
        0);
}
