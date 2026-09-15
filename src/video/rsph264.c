/**
 * @file rsph264.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 *
 * The H264 RSP code does not fit in IMEM, so it is split into two rspq
 * overlays: rsph264_inter (motion compensation and inter residuals) and
 * rsph264_intra (intra prediction and intra residuals). Both are full: 4040
 * and 4096 bytes out of 4096, with roughly 1.8 KB of shared code from
 * rsph264_common.inc duplicated into each. Merging them is not an option, and
 * rspq keeps only its own engine resident, so there is no way to have a third
 * always-loaded section for the common part.
 *
 * An overlay switch is therefore expensive: rspq reloads the whole
 * overlay-specific text and data segments and saves/restores the shared state,
 * which is about 6.3 KB of DMA to enter intra and 7.3 KB to enter inter (the
 * latter also pulls in the shared state as an extra segment, since it is the
 * destination of rspq_overlay_share_state). At a few hundred macroblocks per
 * frame, even a couple of switches per macroblock are enough to make the
 * decoder memory bandwidth bound rather than compute bound, so the number of
 * switches matters more than the cost of the tasks themselves. Both the
 * switches and the resulting DMA traffic can be measured by building with
 * RSPH264_PROFILE (see rsph264_internal.h).
 *
 * The command stream is thus arranged to stay on one overlay for as long as
 * possible. A macroblock is served end to end by a single overlay, so a run of
 * macroblocks of the same kind costs no switch at all, and only a transition
 * between an inter and an intra macroblock does. Two commands make this
 * possible: Task_SetPackedDeltaBuffer and Task_ProcessChromaResidual, which are
 * assembled into both overlays from the same source and only touch state that
 * the two share. The CPU sends them to whichever overlay the RSP already has
 * loaded, tracked here by cur_ovl_id.
 */
#include "rsph264_internal.h"
#include "rspq.h"
#include "fastcache.h"
#include <assert.h>
#include <string.h>
#if RSPH264_PROFILE
#include "emux.h"
#endif

DEFINE_RSP_UCODE(rsph264_inter);
DEFINE_RSP_UCODE(rsph264_intra);
static uint32_t rsph264_inter_ovl_id = 0;
static uint32_t rsph264_intra_ovl_id = 0;

enum {
    // Tasks in rsph264_inter
    TASK_OMX_DEQUANT_TRANSFORM_RESIDUAL        = 0,
    // Slot 1 was OMX_TransformDequantLumaDC; moved to the intra overlay
    // because TransformDequantLumaDC needs full 32-bit signed math (~324
    // bytes of IMEM) that this overlay can't spare. The slot is held as
    // an RSPQ_Loop placeholder so the remaining inter command IDs don't
    // shift.
    TASK_OMX_TRANSFORM_DEQUANT_CHROMADC        = 2,
    TASK_PROCESS_LUMA_INTER_RESIDUAL           = 3,
    TASK_PROCESS_CHROMA_RESIDUAL               = 4,
    TASK_OMX_INTERPOLATE_LUMA_OVERFILL         = 5,
    TASK_OMX_INTERPOLATE_CHROMA_OVERFILL       = 6,
    TASK_SET_WEIGHTS                           = 7,

    //TASK_SET_PACKED_DELTA_BUFFER               = 15,
};

enum {
    // Tasks in rsph264_intra
    TASK_OMX_INTRAPREDICT_CHROMA_8             = 0,
    TASK_PROCESS_LUMA_INTRA4_RESIDUAL          = 1,
    TASK_PROCESS_LUMA_INTRA16_RESIDUAL         = 2,
    // Same implementation as TASK_PROCESS_CHROMA_RESIDUAL in the inter
    // overlay, assembled here as well so that an intra macroblock never has
    // to leave this overlay.
    TASK_PROCESS_CHROMA_RESIDUAL_INTRA         = 3,

    TASK_SET_PACKED_DELTA_BUFFER               = 15,
};

// Overlay that the RSP will have loaded when it reaches the point of the
// command stream we are currently writing. rspq saves and restores the overlay
// state around highpri preemptions, so this stays accurate even if another
// subsystem interrupts us between two H264 commands. It only goes stale if
// somebody else writes into the same lowpri queue, and in that case the worst
// that happens is one extra overlay switch.
static uint32_t cur_ovl_id = 0;

#if RSPH264_PROFILE
static rsph264_stats_t stats;
#define STAT_INC(field)   (stats.field++)
#else
#define STAT_INC(field)   ((void)0)
#endif

static inline uint32_t track_ovl(uint32_t ovl_id)
{
    if (ovl_id != cur_ovl_id) {
        cur_ovl_id = ovl_id;
        STAT_INC(ovl_switches);
        STAT_INC(switches_to[ovl_id == rsph264_intra_ovl_id]);
    }
    STAT_INC(commands);
    STAT_INC(cmds[ovl_id == rsph264_intra_ovl_id]);
    return ovl_id;
}

// Wrapper over rspq_write that keeps cur_ovl_id and the statistics up to date.
// rspq_write evaluates its ovl_id argument exactly once, so the side effects of
// track_ovl() happen once per command.
#define rsph264_write(ovl_id, cmd_id, ...) \
    rspq_write(track_ovl(ovl_id), cmd_id, ##__VA_ARGS__)

static const uint8_t *last_packed_delta_buf = NULL;

// Weightp coefficients currently configured in RSP DMEM. They survive across
// frames (the RSP keeps them in the overlay saved state), so this mirrors the
// initial content of WEIGHT_TAB in rsph264_common.inc.
static uint32_t last_weights[3] = {
    RSPH264_WEIGHT_IDENTITY, RSPH264_WEIGHT_IDENTITY, RSPH264_WEIGHT_IDENTITY,
};

inline void rsph264_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    rspq_init();
    rsph264_inter_ovl_id = rspq_overlay_register(&rsph264_inter);
    rsph264_intra_ovl_id = rspq_overlay_register(&rsph264_intra);
    rspq_overlay_share_state(&rsph264_inter, &rsph264_intra);
    cur_ovl_id = rsph264_intra_ovl_id;
}

#if RSPH264_PROFILE
inline void rsph264_stats_reset(void)
{
    stats = (rsph264_stats_t){0};
}

inline void rsph264_stats_get(rsph264_stats_t *st)
{
    *st = stats;
}

uint32_t rsph264_measure_overlay_switch_cost(void)
{
    // rsph264_queue_debug_load_overlay writes an RSPQ_Loop command, so a run of
    // alternating ones does nothing but switch back and forth: apart from the
    // 4 bytes of each command, all the DMA the RSP does in the meantime is the
    // cost of the switches. The two directions are not symmetric (entering
    // inter also pulls in the shared state, of which it is the owner), but
    // switches necessarily alternate, so their average is what matters.
    enum { N = 256 };

    if (!(emux_detect(1) & EMUX_FEAT1_PROFILER))
        return 0;

    rsph264_queue_debug_load_overlay("inter");
    rspq_wait();

    uint64_t dma0 = emux_prof_read(-1, EMUX_PROF_RAM_RSPDMA_BYTES);
    for (int i = 0; i < N; i++) {
        rsph264_queue_debug_load_overlay("intra");
        rsph264_queue_debug_load_overlay("inter");
    }
    rspq_wait();
    uint64_t dma1 = emux_prof_read(-1, EMUX_PROF_RAM_RSPDMA_BYTES);

    rsph264_stats_reset();
    return (dma1 - dma0) / (2 * N);
}
#endif

inline void rsph264_begin_frame(void)
{
    last_packed_delta_buf = NULL;
}

inline void rsph264_sync(void)
{
    rspq_wait();
}

inline void rsph264_queue_debug_random_status(void)
{
    // TODO: implement
}

inline void rsph264_queue_debug_load_overlay(const char *ovlname)
{
    // Slot 14 is an RSPQ_Loop placeholder in both overlays, so this is a no-op
    // command whose only effect is the overlay switch it forces.
    enum { TASK_NOP = 14 };

    bool intra = !strcmp(ovlname, "intra");
    assertf(intra || !strcmp(ovlname, "inter"), "unknown overlay: %s", ovlname);
    rsph264_write(intra ? rsph264_intra_ovl_id : rsph264_inter_ovl_id, TASK_NOP);
}

inline void rsph264_queue_interpolate_luma_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst, uint32_t dst_pitch,
    uint32_t frame_size, uint32_t block_size, uint32_t mv, uint32_t pos
) {
    // Our RSP implementation assumes that dst is aligned to
    // the block size, as it should be a macroblock partition.
    int block_width = block_size >> 16;
    // int block_height = block_size & 0xFFFF;

    assert(((uint32_t)dst & (block_width-1)) == 0);
#if 0
    assert((cache_flags & RSPH264_CACHE_SKIP_SOURCE) != 0);

    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        for (int i=0;i<block_height;i++)
             fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, block_width);
    }
#endif
    rsph264_write(rsph264_inter_ovl_id, TASK_OMX_INTERPOLATE_LUMA_OVERFILL,
        PhysicalAddr(frame), frame_pitch,
        PhysicalAddr(dst), dst_pitch,
        frame_size, block_size,
        mv, pos,
        0,    // Just luma buffer
        0,
        0);
}

inline void rsph264_queue_interpolate_chroma_overfill(
    int cache_flags,
    const uint8_t *frame, uint32_t frame_pitch,
    uint8_t *dst, uint32_t dst_pitch,
    uint32_t frame_size, uint32_t block_size, uint32_t mv, uint32_t pos
) {
    // Our RSP implementation assumes that dst is aligned to
    // the block size, as it should be a macroblock partition.
    int block_width = block_size >> 16;
    // int block_height = block_size & 0xFFFF;

    assert(((uint32_t)dst & (block_width-1)) == 0);
#if 0
    assert((cache_flags & RSPH264_CACHE_SKIP_SOURCE) != 0);
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        for (int i=0;i<block_height;i++)
             fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, block_width);
    }
#endif    
    rsph264_write(rsph264_inter_ovl_id, TASK_OMX_INTERPOLATE_CHROMA_OVERFILL,
        PhysicalAddr(frame), frame_pitch,
        PhysicalAddr(dst), dst_pitch,
        frame_size, block_size,
        mv, pos,
        0,    // Just 1 chroma buffer
        0,
        0);
}


inline void rsph264_queue_intrapred_chroma_8x8(
    int cache_flags,
    const uint8_t *src_l, const uint8_t *src_u, const uint8_t *src_ul,
    uint8_t *dst, uint32_t left_pitch, uint32_t dst_pitch,
    uint32_t mode, uint32_t availability
) {
    assert(((uint32_t)src_u & 7) == 0);
    assert(((uint32_t)dst & 7) == 0);
#if 0
    if (!(cache_flags & RSPH264_CACHE_SKIP_SOURCE)) {
        for (int i=0;i<8;i++)
            fast_data_cache_hit_writeback_invalidate(src_l+left_pitch*i, 1);
        fast_data_cache_hit_writeback_invalidate(src_ul, 1);
        fast_data_cache_hit_writeback_invalidate(src_u, 8);
    }

    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        for (int i=0;i<8;i++)
            fast_data_cache_hit_writeback_invalidate(dst+dst_pitch*i, 8);
    }
#endif
    rsph264_write(rsph264_intra_ovl_id, TASK_OMX_INTRAPREDICT_CHROMA_8,
        PhysicalAddr(src_l), PhysicalAddr(src_u), PhysicalAddr(src_ul),
        PhysicalAddr(dst), left_pitch, dst_pitch,
        mode, availability);
}

inline void rsph264_queue_process_luma_intra4_residual(
    int cache_flags,
    const uint8_t *src, uint8_t *dst,
    uint32_t src_pitch, uint32_t dst_pitch,
    const uint8_t *modeAvail,
    uint32_t qp, uint32_t totalCoeffMask
) {
    assert(((uint32_t)src & 15) == 0);
    assert(((uint32_t)dst & 15) == 0);
#if 1
    if (!(cache_flags & RSPH264_CACHE_SKIP_SOURCE)) {
        // fast_data_cache_hit_writeback(src-src_pitch-1, 16+4+1);
        // for (int i=0;i<16;i++)
        //     fast_data_cache_hit_writeback(src+i*src_pitch-1, 1);
    }
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        // for (int i=0;i<16;i++)
        //     fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, 16);
    }
#endif
    rsph264_write(rsph264_intra_ovl_id, TASK_PROCESS_LUMA_INTRA4_RESIDUAL,
        PhysicalAddr(src), PhysicalAddr(dst),
        src_pitch, dst_pitch,
        ((uint32_t)modeAvail[3]) | ((uint32_t)modeAvail[2]<<8) | ((uint32_t)modeAvail[1]<<16) | ((uint32_t)modeAvail[0]<<24),
        ((uint32_t)modeAvail[7]) | ((uint32_t)modeAvail[6]<<8) | ((uint32_t)modeAvail[5]<<16) | ((uint32_t)modeAvail[4]<<24),
        ((uint32_t)modeAvail[11]) | ((uint32_t)modeAvail[10]<<8) | ((uint32_t)modeAvail[9]<<16) | ((uint32_t)modeAvail[8]<<24),
        ((uint32_t)modeAvail[15]) | ((uint32_t)modeAvail[14]<<8) | ((uint32_t)modeAvail[13]<<16) | ((uint32_t)modeAvail[12]<<24),
        ((qp / 6) << 8) | (qp % 6),
        totalCoeffMask);
}

inline void rsph264_queue_process_luma_intra16_residual(
    int cache_flags,
    const uint8_t *src, uint8_t *dst,
    uint32_t src_pitch, uint32_t dst_pitch,
    const uint32_t mode, const uint32_t availability,
    uint32_t qp, uint32_t totalCoeffMask)
{
    assert(((uint32_t)src & 15) == 0);
    assert(((uint32_t)dst & 15) == 0);
#if 1
    if (!(cache_flags & RSPH264_CACHE_SKIP_SOURCE)) {
        // fast_data_cache_hit_writeback(src-src_pitch-1, 16+4+1);
        // for (int i=0;i<16;i++)
        //     fast_data_cache_hit_writeback(src+i*src_pitch-1, 1);
    }
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        // for (int i=0;i<16;i++)
        //     fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, 16);
    }
#endif
    rsph264_write(rsph264_intra_ovl_id, TASK_PROCESS_LUMA_INTRA16_RESIDUAL,
        PhysicalAddr(src), PhysicalAddr(dst),
        src_pitch, dst_pitch,
        mode, availability,
        ((qp / 6) << 8) | (qp % 6),
        totalCoeffMask);
}


inline void rsph264_queue_set_weights_if_changed(
    uint32_t luma, uint32_t cb, uint32_t cr) {

    if (last_weights[0] == luma && last_weights[1] == cb && last_weights[2] == cr)
        return;

    last_weights[0] = luma;
    last_weights[1] = cb;
    last_weights[2] = cr;

    rsph264_write(rsph264_inter_ovl_id, TASK_SET_WEIGHTS, 0, luma, cb, cr);
}

inline void rsph264_queue_set_packed_delta_buffer_if_changed(
    int cache_flags,
    const uint8_t *src) {

    if (last_packed_delta_buf != src) {
        last_packed_delta_buf = src;
        if (src)
            rsph264_queue_set_packed_delta_buffer(cache_flags, src);
    }
}

inline void rsph264_queue_set_packed_delta_buffer(
    int cache_flags,
    const uint8_t *src) {

    #define PACKED_DELTA_MAX 840    // keep this in sync with rsph264.S

    if (!(cache_flags & RSPH264_CACHE_SKIP_SOURCE)) {
        // We don't know the actual size (it's part of the parsing),
        // so we flush a size that should be enough (as this area encodes
        // at most 16 16-bit coefficients).
        fast_data_cache_hit_writeback(src, PACKED_DELTA_MAX);
    }

    // Slot 15 of both overlays, so target the one already loaded (see the
    // overlay switch discussion at the top of this file).
    rsph264_write(cur_ovl_id, TASK_SET_PACKED_DELTA_BUFFER,
        PhysicalAddr(src));
}

static void internal_queue_dequant_transform_residual(
    uint32_t ovl_id, int taskid, int cache_flags,
    uint8_t *dst1, uint8_t *dst2, uint32_t dst_pitch,
    const int16_t *dc, uint32_t qp, uint32_t ac
) {
    rsph264_write(ovl_id, taskid,
        0,      // FIXME: remove, not needed anymore
        PhysicalAddr(dst1),
        dst_pitch,
        dc ? (uint16_t)dc[0] : 0xFFFFFFFF,
        ((qp / 6) << 8) | (qp % 6),
        ac,
        PhysicalAddr(dst2));
}

inline void rsph264_queue_dequant_transform_residual(
    int cache_flags,
    uint8_t *dst, uint32_t dst_pitch,
    const int16_t *dc, uint32_t qp, uint32_t ac
) {
    assert(((uint32_t)dst & 3) == 0);
#if 0
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        fast_data_cache_hit_writeback_invalidate(dst+0*dst_pitch, 8);
        fast_data_cache_hit_writeback_invalidate(dst+1*dst_pitch, 8);
        fast_data_cache_hit_writeback_invalidate(dst+2*dst_pitch, 8);
        fast_data_cache_hit_writeback_invalidate(dst+3*dst_pitch, 8);
    }
#endif        

    internal_queue_dequant_transform_residual(
        rsph264_inter_ovl_id, TASK_OMX_DEQUANT_TRANSFORM_RESIDUAL,
        cache_flags, dst, NULL, dst_pitch, dc, qp, ac);
}


inline void rsph264_queue_transform_dequant_chromadc(
    int cache_flags,
    int16_t *dst, uint32_t qp
) {
    assert(((uint32_t)dst & 7) == 0);
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        fast_data_cache_hit_writeback_invalidate(dst, 8);
    }

    rsph264_write(rsph264_inter_ovl_id, TASK_OMX_TRANSFORM_DEQUANT_CHROMADC,
        0, PhysicalAddr(dst),
        ((qp / 6) << 8) | (qp % 6));

    // check_overlay(TASK_OMX_TRANSFORM_DEQUANT_CHROMADC);
    // uint32_t *q = queue_push_begin();
    // q[0] = (uint32_t)TASK_OMX_TRANSFORM_DEQUANT_CHROMADC & MASK_TASKID;
    // q[1] = (uint32_t)0;      // FIXME: remove, not needed anymore
    // q[2] = (uint32_t)dst;
    // q[3] = (uint32_t)((qp / 6) << 8) | (qp % 6);
    // queue_push_end();
}

inline void rsph264_queue_process_luma_inter_residual(
    int cache_flags,
    uint8_t *dst, uint32_t dst_pitch,
    const int16_t *dc, uint32_t qp, uint32_t totalCoeffMask
) {
    assert(((uint32_t)dst & 7) == 0);

    if (!(cache_flags & RSPH264_CACHE_SKIP_SOURCE)) {
    }
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        // for (int i=0;i<16;i++)
        //     fast_data_cache_hit_writeback_invalidate(dst+i*dst_pitch, 16);
    }

    // check_overlay(TASK_PROCESS_LUMA_INTER_RESIDUAL);
    internal_queue_dequant_transform_residual(
        rsph264_inter_ovl_id, TASK_PROCESS_LUMA_INTER_RESIDUAL,
        cache_flags, dst, NULL, dst_pitch, dc, qp, totalCoeffMask);
}

inline void rsph264_queue_process_chroma_residual(
    int cache_flags,
    uint8_t *dst1, uint8_t *dst2, uint32_t dst_pitch,
    uint32_t qp, uint32_t totalCoeffMask
) {
    assert(((uint32_t)dst1 & 7) == 0);
    assert(((uint32_t)dst2 & 7) == 0);

    if (!(cache_flags & RSPH264_CACHE_SKIP_SOURCE)) {
    }
    if (!(cache_flags & RSPH264_CACHE_SKIP_DEST)) {
        // for (int i=0;i<8;i++) {            
        //     fast_data_cache_hit_writeback_invalidate(dst1+i*dst_pitch, 8);
        //     fast_data_cache_hit_writeback_invalidate(dst2+i*dst_pitch, 8);
        // }
    }

    // Assembled into both overlays with the same implementation, so target
    // the one already loaded (see the overlay switch discussion at the top of
    // this file). This is what makes an intra macroblock cost no switch: its
    // chroma residual no longer has to go back to the inter overlay.
    bool on_intra = (cur_ovl_id == rsph264_intra_ovl_id);
    internal_queue_dequant_transform_residual(
        on_intra ? rsph264_intra_ovl_id : rsph264_inter_ovl_id,
        on_intra ? TASK_PROCESS_CHROMA_RESIDUAL_INTRA : TASK_PROCESS_CHROMA_RESIDUAL,
        cache_flags, dst1, dst2, dst_pitch, NULL, qp, totalCoeffMask);
}

inline void rsph264_queue_interpolate_all_overfill(
    int cache_flags,
    const uint8_t *frame_luma, uint32_t frame_pitch,
    uint8_t *dst_luma, uint8_t *dst_chroma1, uint8_t *dst_chroma2, uint32_t dst_pitch,
    uint32_t frame_size, uint32_t block_size, uint32_t mv, uint32_t pos
) {
    uint32_t frame_width = frame_size >> 16;
    uint32_t frame_height = frame_size & 0xFFFF;

    assert(cache_flags == RSPH264_CACHE_SKIP_ALL);

    rsph264_write(rsph264_inter_ovl_id, TASK_OMX_INTERPOLATE_LUMA_OVERFILL,
        PhysicalAddr(frame_luma), frame_pitch,
        PhysicalAddr(dst_luma), dst_pitch,
        frame_size, block_size,
        mv, pos,
        frame_width*frame_height,
        PhysicalAddr(dst_chroma1),
        PhysicalAddr(dst_chroma2));
} 
