/**
 * @file rspq_internal.h
 * @brief RSP Command queue
 * @ingroup rsp
 */

#ifndef __LIBDRAGON_RSPQ_INTERNAL_H
#define __LIBDRAGON_RSPQ_INTERNAL_H

#include "rsp.h"
#include "rspq_constants.h"

/**
 * RSPQ internal commands (overlay 0)
 */
enum {
    /**
     * @brief RSPQ command: Invalid
     * 
     * Reserved ID for invalid command. This is used as a marker so that RSP knows
     * when it has caught up with CPU and reached an empty portion of the buffer.
     */
    RSPQ_CMD_INVALID           = 0x00,

    /**
     * @brief RSPQ command: No-op
     * 
     * This commands does nothing. It can be useful for debugging purposes.
     */
    RSPQ_CMD_NOOP              = 0x01,

    /**
     * @brief RSPQ command: Jump to another buffer
     * 
     * This commands tells the RSP to start fetching commands from a new address.
     * It is mainly used internally to implement the queue as a ring buffer (jumping
     * at the start when we reach the end of the buffer).
     */
    RSPQ_CMD_JUMP              = 0x02,

    /**
     * @brief RSPQ command: Call a block
     * 
     * This command is used by the block functions to implement the execution of
     * a block. It tells RSP to starts fetching commands from the block address,
     * saving the current address in an internal save slot in DMEM, from which
     * it will be recovered by CMD_RET. Using multiple slots allow for nested
     * calls.
     */    
    RSPQ_CMD_CALL              = 0x03,

    /**
     * @brief RSPQ command: Return from a block
     * 
     * This command tells the RSP to recover the buffer address from a save slot
     * (from which it was currently saved by a CALL command) and begin fetching
     * commands from there. It is used to finish the execution of a block.
     */
    RSPQ_CMD_RET               = 0x04,

    /**
     * @brief RSPQ command: DMA transfer
     * 
     * This commands runs a DMA transfer (either DRAM to DMEM, or DMEM to DRAM).
     * It is used by #rspq_overlay_register to register a new overlay table into
     * DMEM while the RSP is already running (to allow for overlays to be
     * registered even after boot), and can be used by the users to perform
     * manual DMA transfers to and from DMEM without risking a conflict with the
     * RSP itself.
     */
    RSPQ_CMD_DMA               = 0x05,

    /**
     * @brief RSPQ Command: write SP_STATUS register
     * 
     * This command asks the RSP to write to the SP_STATUS register. It is normally
     * used to set/clear signals or to raise RSP interrupts.
     */
    RSPQ_CMD_WRITE_STATUS      = 0x06,

    /**
     * @brief RSPQ Command: Swap lowpri/highpri buffers
     * 
     * This command is used as part of the highpri feature. It allows to switch
     * between lowpri and highpri queue, by saving the current buffer pointer
     * in a special save slot, and restoring the buffer pointer of the other
     * queue from another slot. In addition, it also writes to SP_STATUS, to
     * be able to adjust signals: entering highpri mode requires clearing
     * SIG_HIGHPRI_REQUESTED and setting SIG_HIGHPRI_RUNNING; exiting highpri
     * mode requires clearing SIG_HIGHPRI_RUNNING.
     * 
     * The command is called internally by RSP to switch to highpri when the
     * SIG_HIGHPRI_REQUESTED is found set; then it is explicitly enqueued by the
     * CPU when the highpri queue is finished to switch back to lowpri
     * (see #rspq_highpri_end).
     */
    RSPQ_CMD_SWAP_BUFFERS      = 0x07,

    /**
     * @brief RSPQ Command: Test and write SP_STATUS register
     * 
     * This commands does a test-and-write sequence on the SP_STATUS register: first,
     * it waits for a certain mask of bits to become zero, looping on it. Then
     * it writes a mask to the register. It is used as part of the syncpoint
     * feature to raise RSP interrupts, while waiting for the previous
     * interrupt to be processed (coalescing interrupts would cause syncpoints
     * to be missed).
     */
    RSPQ_CMD_TEST_WRITE_STATUS = 0x08,

    /**
     * @brief RSPQ command: Wait for RDP to be idle.
     * 
     * This command will let the RSP spin-wait until the RDP is idle (that is,
     * the DP_STATUS_BUSY bit in COP0_DP_STATUS goes to 0). Notice that the
     * RDP is fully asynchronous, and reading DP_STATUS_BUSY basically makes
     * sense only after a RDP SYNC_FULL command (#rdpq_sync_full()), when it
     * really does make sure that all previous commands have finished
     * running.
     */
    RSPQ_CMD_RDP_WAIT_IDLE     = 0x09,

    /**
     * @brief RSPQ Command: send a new buffer to RDP and/or configure it for new commands
     * 
     * This command configures a new buffer in RSP for RDP commands. It requires three
     * arguments: cur, start, end.
     * 
     * Start is the beginning of the buffer. Cur is the current write pointer in the buffer.
     * If start==cur, it means the buffer is currently empty; otherwise, it means it contains
     * some RDP commands that will be sent to RDP right away. Sentinel is the end of the
     * buffer. If cur==sentinel, the buffer is full and no more commands will be written to it. 
     */
    RSPQ_CMD_RDP_SET_BUFFER    = 0x0A,

    /**
     * @brief RSPQ Command: send more data to RDP (appended to the end of the current buffer)
     * 
     * This commands basically just sets DP_END to the specified argument, allowing new
     * commands appended in the current buffer to be sent to RDP.
     */
    RSPQ_CMD_RDP_APPEND_BUFFER = 0x0B,
};

/** @brief Write an internal command to the RSP queue */
#define rspq_int_write(cmd_id, ...) rspq_write(0, cmd_id, ##__VA_ARGS__)

///@cond
typedef struct rdpq_block_s rdpq_block_t;
///@endcond

/**
 * @brief A rspq block: pre-recorded array of commands
 *
 * A block (#rspq_block_t) is a prerecorded sequence of RSP commands that can
 * be played back. Blocks can be created via #rspq_block_begin / #rspq_block_end,
 * and then executed by #rspq_block_run. It is also possible to do nested
 * calls (a block can call another block), up to 8 levels deep.
 */
typedef struct rspq_block_s {
    uint32_t nesting_level;     ///< Nesting level of the block
    rdpq_block_t *rdp_block;    ///< Option RDP static buffer (with RDP commands)
    uint32_t cmds[];            ///< Block contents (commands)
} rspq_block_t;

/** @brief RDP render mode definition 
 * 
 * This is the definition of the current RDP render mode 
 * 
 */
typedef struct __attribute__((packed)) {
    uint64_t combiner;
    uint64_t combiner_mipmapmask;
    uint32_t blend_step0;
    uint32_t blend_step1;
    uint64_t other_modes;
} rspq_rdp_mode_t;

/**
 * @brief Table of registered overlays (as stored in DMEM)
 */
typedef struct __attribute__((packed)) {
    uint32_t data_rdram[RSPQ_MAX_OVERLAYS];     ///< Packed RDRAM address of data segment and size
    uint8_t idmap[RSPQ_MAX_OVERLAYS];           ///< Map of overlay IDs to base IDs
} rspq_ovl_table_t;

/**
 * @brief RSP profiling data for a single overlay.
 */
typedef struct rspq_profile_slot_dmem_s {
    uint32_t total_ticks;
    uint32_t sample_count;
} rspq_profile_slot_dmem_t;

/**
 * @brief RSP Queue data in DMEM.
 * 
 * This structure is defined by rsp_queue.S, and represents the
 * top portion of DMEM.
 */
typedef struct rsp_queue_s {
    rspq_ovl_table_t rspq_ovl_table;     ///< Overlay table
    /** @brief Pointer stack used by #RSPQ_CMD_CALL and #RSPQ_CMD_RET. */
    uint32_t rspq_pointer_stack[RSPQ_MAX_BLOCK_NESTING_LEVEL];
    uint32_t rspq_dram_lowpri_addr;      ///< Address of the lowpri queue (special slot in the pointer stack)
    uint32_t rspq_dram_highpri_addr;     ///< Address of the highpri queue  (special slot in the pointer stack)
    uint8_t banner[32];                  ///< Banner
    rspq_rdp_mode_t rdp_mode;            ///< RDP current render mode definition
    uint64_t rdp_scissor_rect;           ///< Current RDP scissor rectangle
    uint32_t rspq_rdp_buffers[2];        ///< RDRAM Address of dynamic RDP buffers
    uint32_t rspq_rdp_current;           ///< Current RDP RDRAM write pointer (normally DP_END)
    uint32_t rspq_rdp_sentinel;          ///< Current RDP RDRAM end pointer (when rdp_current reaches this, the buffer is full)
    uint32_t rdp_fill_color;             ///< Current RDP fill color
    uint8_t rdp_target_bitdepth;         ///< Current RDP target buffer bitdepth
    uint8_t rdp_syncfull_ongoing;        ///< True if a SYNC_FULL is currently ongoing
    uint8_t rdpq_debug;                  ///< Debug mode flag
    uint8_t padding;                     ///< Padding
    uint32_t rspq_dram_addr;             ///< Current RDRAM address being processed
    uint16_t current_ovl;                ///< Current overlay ID
    uint16_t padding2;                   ///< Padding
#if RSPQ_PROFILE
    uint32_t rspq_profile_cur_slot;
    uint32_t rspq_profile_start_time;
    rspq_profile_slot_dmem_t rspq_profile_cslots[RSPQ_PROFILE_CSLOT_COUNT];
    rspq_profile_slot_dmem_t rspq_profile_builtin_slot;
#endif
 } __attribute__((aligned(16), packed)) rsp_queue_t;

/** @brief Address of the RSPQ data header in DMEM (see #rsp_queue_t) */
#define RSPQ_DATA_ADDRESS                8

/** @brief ID of the last syncpoint reached by RSP (plus padding). */
extern volatile int __rspq_syncpoints_done[4];

/** @brief Registered overlays */
extern rsp_ucode_t *rspq_overlay_ucodes[RSPQ_MAX_OVERLAYS];

/** @brief Flag to mark deferred calls that needs to wait for RDP SYNC_FULL */
#define RSPQ_DCF_WAITRDP                 (1<<0)

/** @brief A call deferred for execution after RSP reaches a certain syncpoint */
typedef struct rspq_deferred_call_s {
    union {
        void (*func)(void *arg);        ///< Function to call
        uint32_t flags;                 ///< Flags (see RSPQ_DCF_*) -- used last 2 bits
    };
    void *arg;                          ///< Argument to pass to the function
    rspq_syncpoint_t sync;              ///< Syncpoint to wait for
    void *next;                         ///< Next deferred call (linked list)
} rspq_deferred_call_t;

/** @brief Enqueue a new deferred call. */
rspq_syncpoint_t __rspq_call_deferred(void (*func)(void *), void *arg, bool waitrdp);

/** @brief Polls the deferred calls list, calling callbacks ready to be called. */
bool __rspq_deferred_poll(void);

/** @brief True if we are currently building a block. */
static inline bool rspq_in_block(void) {
    extern rspq_block_t *rspq_block;
    return rspq_block != NULL;
}

/** 
 * @brief Return a pointer to a copy of the current RSPQ state. 
 * 
 * @note This function forces a full sync by calling #rspq_wait to
 *       avoid race conditions.
 */
rsp_queue_t *__rspq_get_state(void);

/**
 * @brief Notify that a RSP command is going to run a block
 */
void rspq_block_run_rsp(int nesting_level);

#endif
