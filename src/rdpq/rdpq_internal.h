/**
 * @file rdpq_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: internal functions
 * @ingroup rdpq
 */

#ifndef __LIBDRAGON_RDPQ_INTERNAL_H
#define __LIBDRAGON_RDPQ_INTERNAL_H

#include "pputils.h"
#include "rspq.h"
#include "fgeom2d.h"
#include "../rspq/rspq_internal.h"

/** @brief True if the rdpq module was inited */
extern bool __rdpq_inited;

/** @brief Public rdpq_fence API, redefined it */
extern void rdpq_fence(void);

///@cond
typedef struct rdpq_block_s rdpq_block_t;
typedef struct rdpq_trifmt_s rdpq_trifmt_t;
///@endcond

/**
 * @brief RDP tracking state
 * 
 * This structure contains information that refer to the state of the RDP,
 * tracked by the CPU as it enqueues RDP instructions.ì
 * 
 * Tracking the RDP state on the CPU is in general possible (as all 
 * RDP commands are supposed to go through rdpq, when it is used), but it
 * doesn't fully work across blocks. In fact, blocks can be called in
 * multiple call sites with different RDP states, so it would be wrong
 * to do any assumption on the RDP state while generating the block.
 * 
 * Thus, this structure is reset at some default by #__rdpq_block_begin,
 * and then its previous state is restored by #__rdpq_block_end.
 */
typedef struct {
    /** 
     * @brief State of the autosync engine.
     * 
     * The state of the autosync engine is a 32-bit word, where bits are
     * mapped to specific internal resources of the RDP that might be in
     * use. The mapping of the bits is indicated by the `AUTOSYNC_TILE`,
     * `AUTOSYNC_TMEM`, and `AUTOSYNC_PIPE`
     * 
     * When a bit is set to 1, the corresponding resource is "in use"
     * by the RDP. For instance, drawing a textured rectangle can use
     * a tile and the pipe (which contains most of the mode registers).
     */ 
    uint32_t autosync : 17;
    /** @brief True if the mode changes are currently frozen. */
    bool mode_freeze : 1;
    /** @brief 0=unknown, 1=standard, 2=copy/fill  */
    uint8_t cycle_type_known : 2;
    uint8_t cycle_type_frozen : 2;
} rdpq_tracking_t;

extern rdpq_tracking_t rdpq_tracking;

/**
 * @brief CPU-side mirror of the RDP render state.
 *
 * This struct shadows the subset of DMEM-resident rdpq state that determines
 * what RDP commands the RSP would emit when resolving the render mode. The
 * mirror is the authoritative source of truth on CPU for "what state the
 * RDP will be in once all currently-queued rspq commands have executed".
 *
 * Every CPU-side rdpq call that programs SOM, CC, blender, scissor, fill
 * color or color image bitdepth updates this mirror before writing the rspq
 * command. The mirror is also kept current across block playback: at
 * #rspq_block_end the mirror is captured into rdpq_block_t::mirror_post,
 * and #__rdpq_block_run_with_rdp re-applies it when the block runs.
 *
 * This is the foundation for the "frozen blocks" feature (see
 * FROZEN_BLOCKS_PLAN.md): a snapshot of the mirror at block-begin time will
 * later be compared against the live mirror to detect staleness.
 */
typedef struct {
    uint64_t som;               ///< Mirror of RDPQ_OTHER_MODES (incl. SOMX_* flags)
    uint64_t cc;                ///< Mirror of RDPQ_COMBINER (user value, may carry 2PASS marker)
    uint64_t cc_mipmask;        ///< Mirror of RDPQ_COMBINER_MIPMAPMASK
    uint64_t scissor;           ///< Mirror of RDPQ_SCISSOR_RECT (raw SET_SCISSOR)
    uint32_t blender_steps[2];  ///< Mirror of RDPQ_MODE_BLENDER_STEPS (fog step, blender step)
    uint32_t fill_color;        ///< Mirror of RDPQ_FILL_COLOR (32-bit packed)
    uint32_t prim_color_ex;     ///< Mirror of RDPQ_PRIM_COLOR_EX (minlod/primlod + selector bits, no top byte)
    uint32_t prim_color_rgba;   ///< Mirror of RDPQ_PRIM_COLOR_RGBA (packed RGBA8888)
    uint16_t autotmem_addr;     ///< Mirror of RDPQ_AUTOTMEM_ADDR (current autotmem allocation, units of 8 bytes)
    uint16_t autotmem_addr_prev;///< Mirror of RDPQ_AUTOTMEM_ADDR_PREV (snapshot before last increment, for REUSE)
    uint8_t autotmem_enabled : 4; ///< Mirror of RDPQ_AUTOTMEM_ENABLED (reentrant counter, 0-15)
    uint8_t target_bitdepth  : 2; ///< Mirror of RDPQ_TARGET_BITDEPTH (low 2 bits of fmt, 0-3)
    uint8_t unknown          : 1; ///< Sentinel: live RDP state is unknown / has drifted from this mirror
    uint8_t autotmem_limit_lo : 1; ///< Mirror of RDPQ_AUTOTMEM_LIMIT: 0=4096/8, 1=2048/8 (lowered for 32bpp/YUV/CI)
} rdpq_state_mirror_t;

extern rdpq_state_mirror_t rdpq_state_mirror;

/**
 * @brief A buffer that piggybacks onto rspq_block_t to store RDP commands
 *
 * In rspq blocks, raw RDP commands are not stored as passthroughs for performance.
 * Instead, they are stored in a parallel buffer in RDRAM and the RSP block contains
 * commands to send (portions of) this buffer directly to RDP via DMA. This saves
 * memory bandwidth compared to doing passthrough for every command.
 *
 * Since the buffer can grow during creation, it is stored as a linked list of buffers.
 */
typedef struct rdpq_block_s {
    rdpq_block_t *next;                           ///< Link to next buffer (or NULL if this is the last one for this block)
    rdpq_tracking_t tracking;                     ///< Tracking state at the end of a block (this is populated only on the first link)
    rdpq_state_mirror_t mirror_post;              ///< CPU mirror of RDP state at end of block (populated only on the first link)
    rdpq_state_mirror_t mirror_pre;               ///< CPU mirror snapshot at block-begin (only meaningful when @c frozen is set)
    bool frozen;                                  ///< True if recorded under #RDPQ_CFG_FROZEN_BLOCKS (eligible for staleness checks)
    uint32_t cmds[] __attribute__((aligned(8)));  ///< RDP commands
} rdpq_block_t;

/** 
 * @brief RDP block management state 
 * 
 * This is the internal state used by rdpq.c to manage block creation.
 */
typedef struct rdpq_block_state_s {
    /** @brief During block creation, current write pointer within the RDP buffer. */
    volatile uint32_t *wptr;
    /** @brief During block creation, pointer to the end of the RDP buffer. */
    volatile uint32_t *wend;
    /** @brief Previous wptr, swapped out to go back to dynamic buffer. */
    volatile uint32_t *pending_wptr;
    /** @brief Previous wend, swapped out to go back to dynamic buffer. */
    volatile uint32_t *pending_wend;
    /** @brief Point to the RDP block being created */
    rdpq_block_t *last_node;
    /** @brief Point to the first link of the RDP block being created */
    rdpq_block_t *first_node;
    /** @brief Current buffer size for RDP blocks */
    int bufsize;
    /** 
     * During block creation, this variable points to the last
     * #RSPQ_CMD_RDP_APPEND_BUFFER command, that can be coalesced
     * in case a pure RDP command is enqueued next.
     */
    volatile uint32_t *last_rdp_append_buffer;
    /**
     * @brief Tracking state before starting building the block.
     */
    rdpq_tracking_t previous_tracking;
    /**
     * @brief CPU mirror state before block recording started.
     *
     * The mirror keeps advancing during recording (so it reflects the post-state
     * of the block being recorded). At #__rdpq_block_end this saved value is
     * restored to the live mirror, so that the act of recording a block does
     * not leak in-block state changes to the surrounding scope.
     */
    rdpq_state_mirror_t previous_mirror;
    /**
     * @brief True if the current recording session is a frozen block.
     *
     * Set by #__rdpq_block_begin / #__rdpq_block_recycle when
     * #RDPQ_CFG_FROZEN_BLOCKS is enabled. Causes the snapshot at begin to be
     * persisted onto the block (rdpq_block_t::mirror_pre) and unlocks
     * staleness checks at playback time.
     */
    bool frozen;
} rdpq_block_state_t;

extern rdpq_block_state_t rdpq_block_state;

/**
 * @brief Frozen blocks: global bitmask of RDP state groups stale in DMEM.
 *
 * Set (to all RDPQ_WRITE_READS_* groups) whenever a frozen-block RDP command
 * is written to the static buffer (mode change, scissor, fill, texture,
 * etc.), marking that DMEM no longer reflects the RDP render state. A
 * RDPQ_WRITE_READS_* command flushes the groups it requests (via
 * #__rdpq_frozen_sync_dmem) and clears those bits, so repeated reads of the
 * same group (e.g. per-triangle) are cheap.
 */
extern uint16_t __rdpq_frozen_dmem_pending;

/**
 * @brief Frozen blocks: sets if resolved render mode is deferred, awaiting the next draw.
 *
 * Inside a frozen block, mode changes (combiner/blender/SOM) do not emit immediately. 
 * They advance the CPU mirror and set this flag, 
 * the resolved SET_OTHER_MODES + SET_COMBINE pair is emitted by #__rdpq_frozen_flush_pending_mode.
 */
extern bool __rdpq_frozen_mode_pending;
void __rdpq_frozen_flush_pending_mode(void);

void __rdpq_block_begin();
void __rdpq_block_recycle(rdpq_block_t *head);
rdpq_block_t* __rdpq_block_end();
void __rdpq_block_free(rdpq_block_t *block);
void __rdpq_block_run_with_rdp(rdpq_block_t *block);
void __rdpq_block_run_no_rdp(void);
void __rdpq_block_run_maybe_rdp(void);
void __rdpq_block_next_buffer(void);
void __rdpq_block_update(volatile uint32_t *wptr);
void __rdpq_block_reserve(int num_rdp_commands);
void __rdpq_frozen_publish_post_state(unsigned int groups);
void __rdpq_frozen_sync_dmem(unsigned int groups);

/** Close rdpq_attach subsystem */
void __rdpq_attach_close(void);

inline void __rdpq_tracking_state_reset(rdpq_tracking_t *state) {
  *state = (rdpq_tracking_t){
      // current autosync status is unknown because blocks can be
      // played in any context. So assume the worst: all resources
      // are being used. This will cause all SYNCs to be generated,
      // which is the safest option.
      .autosync = ~0,
      // we don't know whether mode changes will be frozen or not
      // when the block will play. Assume the worst (and thus
      // do not optimize out mode changes).
      .mode_freeze = false,
      // we don't know the cycle type after we run the block
      .cycle_type_known = 0,
      .cycle_type_frozen = 0,
  };
}

inline void __rdpq_autosync_use(uint32_t res)
{
    rdpq_tracking.autosync |= res;
    // Frozen-block mode coalescing: a pipe-using command (draw) is about to be written, 
    // so flush any deferred resolved mode into the static RDP buffer first.
    if (__builtin_expect((res & AUTOSYNC_PIPE) && __rdpq_frozen_mode_pending, 0)) {
        __rdpq_frozen_flush_pending_mode();
    }
}

/**
 * @brief Notify the rdpq engine that a rspq block is about to run, before its CALL command is enqueued.
 *        This intern *may* flush out pending DMEM states form a frozen blocks
 */
inline void __rdpq_block_run_prepare(rdpq_block_t *block)
{
    if (__builtin_expect(
      __rdpq_frozen_dmem_pending && !block->frozen && !rdpq_block_state.frozen, 0)
    ) {
        __rdpq_frozen_sync_dmem(RDPQ_WRITE_READS_RDP_STATE);
    }
}
void __rdpq_autosync_change(uint32_t res);

void __rdpq_write8(uint32_t cmd_id, uint32_t arg0, uint32_t arg1);
void __rdpq_write16(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

void rdpq_triangle_cpu(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3);
void rdpq_triangle_rsp(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3, fm_mat3_t *mtx);

extern volatile int __rdpq_syncpoint_at_syncfull;


///@cond
/* Helpers for rdpq_passthrough_write / rdpq_fixup_write */
#define __rdpcmd_count_words2(rdp_cmd_id, arg0, ...)  nwords += __COUNT_VARARGS(__VA_ARGS__) + 1;
#define __rdpcmd_count_words(arg)                    __rdpcmd_count_words2 arg

#define __rdpcmd_write_arg(arg)                      *ptr++ = arg;
#define __rdpcmd_write2(rdp_cmd_id, arg0, ...)        \
        *ptr++ = (RDPQ_OVL_ID + ((rdp_cmd_id)<<24)) | (arg0); \
        __CALL_FOREACH_BIS(__rdpcmd_write_arg, ##__VA_ARGS__);
#define __rdpcmd_write(arg)                          __rdpcmd_write2 arg

#define __rspcmd_write(...)                          ({ rspq_write(RDPQ_OVL_ID, __VA_ARGS__ ); })
///@endcond

/**
 * @brief Write a passthrough RDP command into the rspq queue
 * 
 * This macro handles writing a single RDP command into the rspq queue. It must be
 * used only with raw commands aka passthroughs, that is commands that are not
 * intercepted by RSP in any way, but just forwarded to RDP.
 * 
 * In block mode, the RDP command will be written to the static RDP buffer instead,
 * so that it will be sent directly to RDP without going through RSP at all.
 * 
 * Example syntax (notice the double parenthesis):
 * 
 *     rdpq_passthrough_write((RDPQ_CMD_SYNC_PIPE, 0, 0));
 * 
 * @hideinitializer
 */
#define rdpq_passthrough_write(rdp_cmd) ({ \
    if (__builtin_expect(rspq_block_is_recording(), 0)) { \
        extern rdpq_block_state_t rdpq_block_state; \
        int nwords = 0; __rdpcmd_count_words(rdp_cmd); \
        while (__builtin_expect(rdpq_block_state.wptr + nwords > rdpq_block_state.wend, 0)) \
            __rdpq_block_next_buffer(); \
        volatile uint32_t *ptr = rdpq_block_state.wptr; \
        __rdpcmd_write(rdp_cmd); \
        __rdpq_block_update((uint32_t*)ptr); \
    } else { \
        __rspcmd_write rdp_cmd; \
    } \
})

#endif
