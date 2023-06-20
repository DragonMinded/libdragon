/**
 * @file rdpq_internal.h
 * @brief RDP Command queue: internal functions
 * @ingroup rdp
 */

#ifndef __LIBDRAGON_RDPQ_INTERNAL_H
#define __LIBDRAGON_RDPQ_INTERNAL_H

#include "pputils.h"
#include "rspq.h"
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
} rdpq_block_state_t;

void __rdpq_block_begin();
rdpq_block_t* __rdpq_block_end();
void __rdpq_block_free(rdpq_block_t *block);
void __rdpq_block_run(rdpq_block_t *block);
void __rdpq_block_next_buffer(void);
void __rdpq_block_update(volatile uint32_t *wptr);
void __rdpq_block_reserve(int num_rdp_commands);

inline void __rdpq_autosync_use(uint32_t res)
{
    rdpq_tracking.autosync |= res;
}
void __rdpq_autosync_change(uint32_t res);

void __rdpq_write8(uint32_t cmd_id, uint32_t arg0, uint32_t arg1);
void __rdpq_write16(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

void rdpq_triangle_cpu(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3);
void rdpq_triangle_rsp(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3);

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
    if (__builtin_expect(rspq_in_block(), 0)) { \
        extern rdpq_block_state_t rdpq_block_state; \
        int nwords = 0; __rdpcmd_count_words(rdp_cmd); \
        if (__builtin_expect(rdpq_block_state.wptr + nwords > rdpq_block_state.wend, 0)) \
            __rdpq_block_next_buffer(); \
        volatile uint32_t *ptr = rdpq_block_state.wptr; \
        __rdpcmd_write(rdp_cmd); \
        __rdpq_block_update((uint32_t*)ptr); \
    } else { \
        __rspcmd_write rdp_cmd; \
    } \
})

#endif
