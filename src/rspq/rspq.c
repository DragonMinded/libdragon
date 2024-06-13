/**
 * @file rspq.c
 * @brief RSP Command queue
 * @ingroup rsp
 *
 * # RSP Queue: implementation
 * 
 * This documentation block describes the internal workings of the RSP Queue.
 * This is useful to understand the implementation. For description of the
 * API of the RSP queue, see rspq.h
 *
 * ## Architecture
 * 
 * The RSP queue can be thought in abstract as a single contiguous memory buffer
 * that contains RSP commands. The CPU is the writing part, which appends command
 * to the buffer. The RSP is the reading part, which reads commands and execute
 * them. Both work at the same time on the same buffer, so careful engineering
 * is required to make sure that they do not interfere with each other.
 * 
 * The complexity of this library is trying to achieve this design without any
 * explicit synchronization primitive. The basic design constraint is that,
 * in the standard code path, the CPU should be able to just append a new command
 * in the buffer without talking to the RSP, and the RSP should be able to just
 * read a new command from the buffer without talking to the CPU. Obviously
 * there are side cases where the synchronization is required (eg: if the RSP
 * catches up with the CPU, or if the CPU fins that the buffer is full), but
 * these cases should in general be rare.
 * 
 * To achieve a fully lockless approach, there are specific rules that the CPU
 * has to follow while writing to make sure that the RSP does not get confused
 * and execute invalid or partially-written commands. On the other hand, the RSP
 * must be careful in discerning between a fully-written command and a
 * partially-written command, and at the same time not waste memory bandwidth
 * to continuously "poll" the buffer when it has caught up with the CPU.
 * 
 * The RSP uses the following algorithm to parse the buffer contents. Assume for
 * now that the buffer is linear and unlimited in size.
 * 
 * 1. The RSP fetches a "portion" of the buffer from RDRAM to DMEM. The size
 *    of the portion is RSPQ_DMEM_BUFFER_SIZE. It also resets its internal
 *    read pointer to the start of the DMEM buffer.
 * 2. The RSP reads the first byte pointed by the internal read pointer. The
 *    first byte is the command ID. It splits it into overlay ID (4 bits) and
 *    command index (4 bits).
 * 3. If the command is 0x00 (overlay 0, index 0), it means that the RSP has
 *    caught up with the CPU and there are no more pending commands.
 *    
 *     * The RSP checks whether the signal SIG_MORE was set by the CPU. This
 *       signal is set any time the CPU writes a new command in the queue.
 *       If the signal is set, it means that the CPU has continued writing but
 *       the RSP has probably fetched the buffer before those commands were
 *       written. The RSP goes back to step 1 (refetch the buffer, from the
 *       current position).
 *     * If SIG_MORE is not set, the RSP has really caught up the CPU, and no
 *       more commands are available in the queue. The RSP goes to sleep via
 *       the BREAK opcode, and waits for the CPU to wake it up when more
 *       commands are available.
 *     * After the CPU has woken the RSP, it goes back to step 1.
 * 
 * 4. If the overlay ID refers to an overlay which is not the currently loaded
 *    one, the RSP loads the new overlay into IMEM/DMEM. Before doing so, it
 *    also saves the current overlay's state back into RDRAM (this is a portion
 *    of DMEM specified by the overlay itself as "state", that is preserved
 *    across overlay switching).
 * 5. The RSP uses the command index to fetch the "command descriptor", a small
 *    structure that contains a pointer to the function in IMEM that executes
 *    the command, and the size of the command in word.
 * 6. If the command overflows the internal buffer (that is, it is longer than
 *    the number of bytes left in the buffer), it means that we need to
 *    refetch a subsequent portion of the buffer to see the whole command. Go back
 *    to step 1.
 * 7. The RSP jumps to the function that executes the command. After the command
 *    is finished, the function is expected to jump back to the main loop, going
 *    to step 2.
 *     
 * Given the above algorithm, it is easy to understand how the CPU must behave
 * when filling the buffer:
 * 
 *   * The buffer must be initialized with 0x00. This makes sure that unwritten
 *     portions of the buffers are seen as "special command 0x00" by the RSP.
 *   * The CPU must take special care not to write the command ID before the
 *     full command is written. For instance let's say a command is made by
 *     two words: 0xAB000001 0xFFFF8000 (overlay 0xA, command index 0xB,
 *     length 2). If the CPU writes the two words in the standard order,
 *     there might be a race where the RSP reads the memory via DMA when
 *     only the first word has been written, and thus see 0xAB000001 0x00000000,
 *     executing the command with a wrong second word. So the CPU has to
 *     write the first word as last (or at least its first byte must be
 *     written last).
 *   * It is important that the C compiler does not reorder writes. In general,
 *     compilers are allowed to change the order in which writes are performed
 *     in a buffer. For instance, if the code writes to buf[0], buf[1], buf[2],
 *     the compiler might decide to generate code that writes buf[2] first,
 *     for optimization reasons. It is possible to fix it using the #MEMORY_BARRIER
 *     macro, or the volatile qualifier (which guarantees a fixed order of
 *     accesses between volatile pointers, though non-volatile accesses can
 *     be reordered freely also across volatile ones).
 *
 * ## Internal commands
 *
 * To manage the queue and implement all the various features, rspq reserves
 * for itself the overlay ID 0x0 to implement internal commands. You can
 * look at the list of commands and their description below. All command IDs
 * are defined with `RSPQ_CMD_*` macros.
 * 
 * ## Buffer swapping
 * 
 * Internally, double buffering is used to implement the queue. The size of
 * each of the buffers is RSPQ_DRAM_LOWPRI_BUFFER_SIZE. When a buffer is full,
 * the queue engine writes a #RSPQ_CMD_JUMP command with the address of the
 * other buffer, to tell the RSP to jump there when it is done. 
 * 
 * Moreover, just before the jump, the engine also enqueue a #RSPQ_CMD_WRITE_STATUS
 * command that sets the SP_STATUS_SIG_BUFDONE_LOW signal. This is used to
 * keep track when the RSP has finished processing a buffer, so that we know
 * it becomes free again for more commands.
 * 
 * This logic is implemented in #rspq_next_buffer.
 *
 * ## Blocks
 * 
 * Blocks are implemented by redirecting #rspq_write to a different memory buffer,
 * allocated for the block. The starting size for this buffer is
 * RSPQ_BLOCK_MIN_SIZE. If the buffer becomes full, a new buffer is allocated
 * with double the size (to achieve exponential growth), and it is linked
 * to the previous buffer via a #RSPQ_CMD_JUMP. So a block can end up being
 * defined by multiple memory buffers linked via jumps.
 * 
 * Calling a block requires some work because of the nesting calls we want
 * to support. To make the RSP ucode as short as possible, the two internal
 * command dedicated to block calls (#RSPQ_CMD_CALL and #RSPQ_CMD_RET) do not
 * manage a call stack by themselves, but only allow to save/restore the
 * current queue position from a "save slot", whose index must be provided
 * by the CPU. 
 * 
 * Thus, the CPU has to make sure that each CALL opcode saves the
 * position into a save slot which will not be overwritten by nested block
 * calls. To do this, it calculates the "nesting level" of a block at
 * block creation time: the nesting level of a block is defined by the smallest
 * number greater than the nesting levels of all blocks that are called within
 * the block itself. So for instance if a block calls another block whose
 * nesting level is 5, it will get assigned a level of 6. The nesting level
 * is then used as call slot in both all future calls to the block, and by
 * the RSPQ_CMD_RET command placed at the end of the block itself.
 * 
 * ## Highpri queue
 * 
 * The high priority queue is implemented as an alternative couple of buffers,
 * that replace the standard buffers when the high priority mode is activated.
 * 
 * When #rspq_highpri_begin is called, the CPU notifies the RSP that it must
 * switch to the highpri queues by setting signal SP_STATUS_SIG_HIGHPRI_REQUESTED.
 * The RSP checks for that signal between each command, and when it sees it, it
 * internally calls #RSPQ_CMD_SWAP_BUFFERS. This command loads the highpri queue
 * pointer from a special call slot, saves the current lowpri queue position
 * in another special save slot, and finally clear SP_STATUS_SIG_HIGHPRI_REQUESTED
 * and set SP_STATUS_SIG_HIGHPRI_RUNNING instead.
 * 
 * When the #rspq_highpri_end is called, the opposite is done. The CPU writes
 * in the queue a #RSPQ_CMD_SWAP_BUFFERS that saves the current highpri pointer
 * into its call slot, recover the previous lowpri position, and turns off
 * SP_STATUS_SIG_HIGHPRI_RUNNING.
 * 
 * Some careful tricks are necessary to allow multiple highpri queues to be
 * pending, see #rspq_highpri_begin for details.
 * 
 * ## rdpq integrations
 * 
 * There are a few places where the rsqp code is hooked with rdpq to provide
 * for coherent usage of the two peripherals. In particular:
 * 
 *  * #rspq_wait automatically calls #rdpq_fence. This means that
 *    it will also wait for RDP to finish executing all commands, which is
 *    actually expected for its intended usage of "full sync for debugging
 *    purposes".
 *  * All rsqp block creation functions call into hooks in rdpq. This is
 *    necessary because blocks are specially handled by rdpq via static
 *    buffer, to make sure RDP commands in the block don't passthrough
 *    via RSP, but are directly DMA from RDRAM into RDP. Moreover,
 *    See rdpq.c documentation for more details. 
 *  * In specific places, we call into the rdpq debugging module to help
 *    tracing the RDP commands. For instance, when switching RDP RDRAM
 *    buffers, RSP will generate an interrupt to inform the debugging
 *    code that it needs to finish dumping the previous RDP buffer.
 * 
 */

#include "rsp.h"
#include "rspq.h"
#include "rspq_internal.h"
#include "rspq_constants.h"
#include "rspq_profile.h"
#include "rdp.h"
#include "rdpq_constants.h"
#include "rdpq/rdpq_internal.h"
#include "rdpq/rdpq_debug_internal.h"
#include "interrupt.h"
#include "utils.h"
#include "n64sys.h"
#include "debug.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>


/// @cond
// Make sure that RSPQ_CMD_WRITE_STATUS and RSPQ_CMD_TEST_WRITE_STATUS have
// an even ID number. This is a small trick used to save one opcode in
// rsp_queue.S (see cmd_write_status there for an explanation).
_Static_assert((RSPQ_CMD_WRITE_STATUS & 1) == 0);
_Static_assert((RSPQ_CMD_TEST_WRITE_STATUS & 1) == 0);

// Check that the DMEM buffer is sized at least for the largest command
// that we can handle, plus some extra space that's required because
// the RSP code won't run a command that ends exactly at the end of
// the buffer (see rsp_queue.inc).
_Static_assert(RSPQ_DMEM_BUFFER_SIZE >= (RSPQ_MAX_COMMAND_SIZE + 2) * 4);

// Check that the maximum command size is actually supported by the 
// internal command descriptor format.
_Static_assert(RSPQ_MAX_COMMAND_SIZE * 4 <= RSPQ_DESCRIPTOR_MAX_SIZE);
/// @endcond

/** @brief Smaller version of rspq_write that writes to an arbitrary pointer */
#define rspq_append1(ptr, cmd, arg1) ({ \
    ((volatile uint32_t*)(ptr))[0] = ((cmd)<<24) | (arg1); \
    ptr += 1; \
})

/** @brief Smaller version of rspq_write that writes to an arbitrary pointer */
#define rspq_append2(ptr, cmd, arg1, arg2) ({ \
    ((volatile uint32_t*)(ptr))[1] = (arg2); \
    ((volatile uint32_t*)(ptr))[0] = ((cmd)<<24) | (arg1); \
    ptr += 2; \
})

/** @brief Smaller version of rspq_write that writes to an arbitrary pointer */
#define rspq_append3(ptr, cmd, arg1, arg2, arg3) ({ \
    ((volatile uint32_t*)(ptr))[1] = (arg2); \
    ((volatile uint32_t*)(ptr))[2] = (arg3); \
    ((volatile uint32_t*)(ptr))[0] = ((cmd)<<24) | (arg1); \
    ptr += 3; \
})

static void rspq_crash_handler(rsp_snapshot_t *state);
static void rspq_assert_handler(rsp_snapshot_t *state, uint16_t assert_code);

/** The RSPQ ucode */
DEFINE_RSP_UCODE(rsp_queue, 
    .crash_handler = rspq_crash_handler,
    .assert_handler = rspq_assert_handler);

/** 
 * @brief The header of the overlay in DMEM.
 * 
 * This structure is placed at the start of the overlay in DMEM, via the
 * RSPQ_OverlayHeader macros (defined in rsp_queue.inc).
 */
typedef struct __attribute__((packed)) rspq_overlay_header_t {
    uint16_t state_start;       ///< Start of the portion of DMEM used as "state"
    uint16_t state_size;        ///< Size of the portion of DMEM used as "state"
    uint32_t state_rdram;       ///< RDRAM address of the portion of DMEM used as "state"
    uint32_t text_rdram;        ///< RDRAM address of the overlay's text section
    uint16_t text_size;         ///< Size of the overlay's text section
    uint16_t command_base;      ///< Primary overlay ID used for this overlay
    #if RSPQ_PROFILE
    uint16_t profile_slot_dmem; ///< Start of the profile slots in DMEM
    #endif
    uint16_t commands[];
} rspq_overlay_header_t;

///@cond
_Static_assert(sizeof(rspq_overlay_header_t) == RSPQ_OVERLAY_HEADER_SIZE);
///@endcond

/** @brief RSPQ overlays */
rsp_ucode_t *rspq_overlay_ucodes[RSPQ_MAX_OVERLAYS];

/**
 * @brief RSP queue building context
 * 
 * This structure contains the state of a RSP queue as it is built by the CPU.
 * It is instantiated two times: one for the lwopri queue, and one for the
 * highpri queue. It contains the two buffers used in the double buffering
 * scheme, and some metadata about the queue.
 * 
 * The current write pointer is stored in the "cur" field. The "sentinel" field
 * contains the pointer to the last byte at which a new command can start,
 * before overflowing the buffer (given #RSPQ_MAX_COMMAND_SIZE). This is used
 * for efficiently check when it is time to switch to the other buffer: basically,
 * it is sufficient to check whether "cur > sentinel".
 * 
 * The current queue is stored in 3 global pointers: #rspq_ctx, #rspq_cur_pointer
 * and #rspq_cur_sentinel. #rspq_cur_pointer and #rspq_cur_sentinel are 
 * external copies of the "cur" and "sentinel" pointer of the
 * current context, but they are kept as separate global variables for
 * maximum performance of the hottest code path: #rspq_write. In fact, it is
 * much faster to access a global 32-bit pointer (via gp-relative offset) than
 * dereferencing a member of a global structure pointer.
 * 
 * rspq_switch_context is called to switch between lowpri and highpri,
 * updating the three global pointers.
 * 
 * When building a block, #rspq_ctx is set to NULL, while the other two
 * pointers point inside the block memory.
 */
typedef struct {
    void *buffers[2];                   ///< The two buffers used to build the RSP queue
    int buf_size;                       ///< Size of each buffer in 32-bit words
    int buf_idx;                        ///< Index of the buffer currently being written to.
    uint32_t sp_status_bufdone;         ///< SP status bit to signal that one buffer has been run by RSP
    uint32_t sp_wstatus_set_bufdone;    ///< SP mask to set the bufdone bit
    uint32_t sp_wstatus_clear_bufdone;  ///< SP mask to clear the bufdone bit
    volatile uint32_t *cur;             ///< Current write pointer within the active buffer
    volatile uint32_t *sentinel;        ///< Current write sentinel within the active buffer
} rspq_ctx_t;

static rspq_ctx_t lowpri;               ///< Lowpri queue context
static rspq_ctx_t highpri;              ///< Highpri queue context

rspq_ctx_t *rspq_ctx;                   ///< Current context
volatile uint32_t *rspq_cur_pointer;    ///< Copy of the current write pointer (see #rspq_ctx_t)
volatile uint32_t *rspq_cur_sentinel;   ///< Copy of the current write sentinel (see #rspq_ctx_t)

/** @brief Buffers that hold outgoing RDP commands (generated via RSP). */
void *rspq_rdp_dynamic_buffers[2];

/** @brief RSP queue data in DMEM. */
static rsp_queue_t rspq_data;

/** @brief True if the queue system has been initialized. */
static bool rspq_initialized = 0;

/** @brief Pointer to the current block being built, or NULL. */
rspq_block_t *rspq_block;
/** @brief Size of the current block memory buffer (in 32-bit words). */
static int rspq_block_size;

/** @brief ID that will be used for the next syncpoint that will be created. */
static int rspq_syncpoints_genid;
/** @brief ID of the last syncpoint reached by RSP (plus padding). */
volatile int __rspq_syncpoints_done[4]  __attribute__((aligned(16)));

/** @brief True if the RSP queue engine is running in the RSP. */
static bool rspq_is_running;

/** @brief Dummy state used for overlay 0 */
static uint64_t dummy_overlay_state[2] __attribute__((aligned(16)));

/** @brief Deferred calls: head of list */
rspq_deferred_call_t *__rspq_defcalls_head;
/** @brief Deferred calls: tail of list */
rspq_deferred_call_t *__rspq_defcalls_tail;

static void rspq_flush_internal(void);

/** @brief RSP interrupt handler, used for syncpoints. */
static void rspq_sp_interrupt(void) 
{
    uint32_t status = *SP_STATUS;
    uint32_t wstatus = 0;

    // Check if a syncpoint was reached by RSP. If so, increment the
    // syncpoint done ID and clear the signal.
    if (status & SP_STATUS_SIG_SYNCPOINT) {
        wstatus |= SP_WSTATUS_CLEAR_SIG_SYNCPOINT;
        ++__rspq_syncpoints_done[0];
        // writeback to memory; this is required for RDPQCmd_SyncFull to fetch the correct value 
        data_cache_hit_writeback(&__rspq_syncpoints_done[0], 4);
    }
    if (status & SP_STATUS_SIG0) {
        wstatus |= SP_WSTATUS_CLEAR_SIG0;
        if (rdpq_trace_fetch) rdpq_trace_fetch(true);
    }

    MEMORY_BARRIER();

    if (wstatus)
        *SP_STATUS = wstatus;
}

/** @brief Extract the current overlay index and name from the RSP queue state */
static void rspq_get_current_ovl(rsp_queue_t *rspq, uint8_t *ovl_id, const char **ovl_name)
{
    *ovl_id = rspq->current_ovl;
    if (*ovl_id == 0) {
        *ovl_name = "builtin";
    } else if (*ovl_id < RSPQ_MAX_OVERLAYS && rspq_overlay_ucodes[*ovl_id]) {
        *ovl_name = rspq_overlay_ucodes[*ovl_id]->name;
    } else
        *ovl_name = "?";
}

/** @brief RSPQ crash handler. This shows RSPQ-specific info the in RSP crash screen. */
static void rspq_crash_handler(rsp_snapshot_t *state)
{
    rsp_queue_t *rspq = (rsp_queue_t*)(state->dmem + RSPQ_DATA_ADDRESS);
    uint32_t cur = rspq->rspq_dram_addr + state->gpr[28];
    uint32_t dmem_buffer = ROUND_UP(RSPQ_DATA_ADDRESS + sizeof(rsp_queue_t), 8);

    const char *ovl_name; uint8_t ovl_id;
    rspq_get_current_ovl(rspq, &ovl_id, &ovl_name);

    printf("RSPQ: Normal  DRAM address: %08lx\n", rspq->rspq_dram_lowpri_addr);
    printf("RSPQ: Highpri DRAM address: %08lx\n", rspq->rspq_dram_highpri_addr);
    printf("RSPQ: Current DRAM address: %08lx + GP=%lx = %08lx\n", 
        rspq->rspq_dram_addr, state->gpr[28], cur);
    printf("RSPQ: RDP     DRAM address: %08lx\n", rspq->rspq_rdp_buffers[1]);
    printf("RSPQ: Current Overlay: %s (%x)\n", ovl_name, ovl_id);

    // Dump the command queue in DMEM. In debug mode, there is a marker to check
    // if we know the correct address. TODO: find a way to expose the symbols
    // from rsp_queue.inc.
    debugf("RSPQ: Command queue:\n");
    for (int j=0;j<4;j++) {        
        for (int i=0;i<16;i++)
            debugf("%08lx%c", ((uint32_t*)state->dmem)[dmem_buffer/4+i+j*16], state->gpr[28] == (j*16+i)*4 ? '*' : ' ');
        debugf("\n");
    }

    // Dump the command queue in RDRAM (both data before and after the current pointer).
    debugf("RSPQ: RDRAM Command queue: %s\n", (cur&3) ? "MISALIGNED" : "");
    uint32_t *q = (uint32_t*)(0xA0000000 | (cur & 0xFFFFFC));
    for (int j=0;j<4;j++) {        
        for (int i=0;i<16;i++)
            debugf("%08lx%c", q[i+j*16-32], i+j*16-32==0 ? '*' : ' ');
        debugf("\n");
    }

    debugf("RSPQ: RDP Command queue: %s\n", (cur&7) ? "MISALIGNED" : "");
    q = (uint32_t*)(0xA0000000 | (state->cop0[10] & 0xFFFFF8));
    for (int j=0;j<4;j++) {        
        for (int i=0;i<16;i+=2) {
            debugf("%08lx", q[i+0+j*16-32]);
            debugf("%08lx%c", q[i+1+j*16-32], i+j*16-32==0 ? '*' : ' ');
        }
        debugf("\n");
    }
}

/** @brief Special RSP assert handler for ASSERT_INVALID_COMMAND */
static void rspq_assert_invalid_command(rsp_snapshot_t *state)
{
    rsp_queue_t *rspq = (rsp_queue_t*)(state->dmem + RSPQ_DATA_ADDRESS);
    const char *ovl_name; uint8_t ovl_id;
    rspq_get_current_ovl(rspq, &ovl_id, &ovl_name);

    uint32_t dmem_buffer = ROUND_UP(RSPQ_DATA_ADDRESS + sizeof(rsp_queue_t), 8);
    uint32_t cur = dmem_buffer + state->gpr[28];
    printf("Invalid command\nCommand %02x not found in overlay %s (0x%01x)\n", state->dmem[cur], ovl_name, ovl_id);
}

/** @brief Special RSP assert handler for ASSERT_INVALID_OVERLAY */
static void rspq_assert_invalid_overlay(rsp_snapshot_t *state)
{
    printf("Invalid overlay\nOverlay 0x%01lx not registered\n", state->gpr[8]);
}

/** @brief RSP assert handler for rspq */
static void rspq_assert_handler(rsp_snapshot_t *state, uint16_t assert_code)
{
    switch (assert_code) {
        case ASSERT_INVALID_OVERLAY:
            rspq_assert_invalid_overlay(state);
            break;
        case ASSERT_INVALID_COMMAND:
            rspq_assert_invalid_command(state);
            break;
        default: {
            rsp_queue_t *rspq = (rsp_queue_t*)(state->dmem + RSPQ_DATA_ADDRESS);

            // Check if there is an assert handler for the current overlay.
            // If it exists, forward request to it.
            // Be defensive against DMEM corruptions.
            int ovl_id = rspq->current_ovl;
            if (ovl_id < RSPQ_MAX_OVERLAYS &&
                rspq_overlay_ucodes[ovl_id] &&
                rspq_overlay_ucodes[ovl_id]->assert_handler)
                rspq_overlay_ucodes[ovl_id]->assert_handler(state, assert_code);
            break;
        }
    }
}

/** @brief Switch current queue context (used to switch between highpri and lowpri) */
__attribute__((noinline))
static void rspq_switch_context(rspq_ctx_t *new)
{
    if (rspq_ctx) {
        // Save back the external pointers into the context structure, where
        // they belong.
        rspq_ctx->cur = rspq_cur_pointer;
        rspq_ctx->sentinel = rspq_cur_sentinel;
    }

    // Switch to the new context, and make an external copy of cur/sentinel
    // for performance reason.
    rspq_ctx = new;
    rspq_cur_pointer = rspq_ctx ? rspq_ctx->cur : NULL;
    rspq_cur_sentinel = rspq_ctx ? rspq_ctx->sentinel : NULL;
}

/** @brief Switch the current write buffer */
static volatile uint32_t* rspq_switch_buffer(uint32_t *new, int size, bool clear)
{
    volatile uint32_t* prev = rspq_cur_pointer;

    // Notice that the buffer must have been cleared before, as the
    // command queue are expected to always contain 0 on unwritten data.
    // We don't do this for performance reasons.
    assert(size >= RSPQ_MAX_COMMAND_SIZE);
    if (clear) memset(new, 0, size * sizeof(uint32_t));

    // Switch to the new buffer, and calculate the new sentinel.
    rspq_cur_pointer = new;
    rspq_cur_sentinel = new + size - RSPQ_MAX_SHORT_COMMAND_SIZE;

    // Return a pointer to the previous buffer
    return prev;
}

/** @brief Start the RSP queue engine in the RSP */
static void rspq_start(void)
{
    if (rspq_is_running)
        return;

    // Load the RSP queue ucode
    rsp_wait();
    rsp_load(&rsp_queue);

    // Load data with initialized overlays into DMEM
    data_cache_hit_writeback(&rspq_data, sizeof(rsp_queue_t));
    rsp_load_data(&rspq_data, sizeof(rsp_queue_t), RSPQ_DATA_ADDRESS);

    static rspq_overlay_header_t dummy_header = (rspq_overlay_header_t){
        .state_start = 0,
        .state_size = 7,
        .command_base = 0
    };
    dummy_header.state_rdram = PhysicalAddr(dummy_overlay_state);
    data_cache_hit_writeback(&dummy_header, sizeof(rspq_overlay_header_t));

    uint32_t rspq_data_size = rsp_queue_data_end - rsp_queue_data_start;
    rsp_load_data(&dummy_header, sizeof(dummy_header), rspq_data_size);

    MEMORY_BARRIER();

    // Set initial value of all signals.
    *SP_STATUS = SP_WSTATUS_CLEAR_SIG0 | 
                 SP_WSTATUS_CLEAR_SIG1 | 
                 SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING | 
                 SP_WSTATUS_CLEAR_SIG_SYNCPOINT |
                 SP_WSTATUS_SET_SIG_BUFDONE_LOW |
                 SP_WSTATUS_SET_SIG_BUFDONE_HIGH |
                 SP_WSTATUS_CLEAR_SIG_HIGHPRI_REQUESTED |
                 SP_WSTATUS_CLEAR_SIG_MORE;

    MEMORY_BARRIER();

    // Off we go!
    // Do not turn on INTR_BREAK as we don't need it.
    extern void __rsp_run_async(uint32_t status_flags);
    __rsp_run_async(0);
    rspq_is_running = 1;
}

/** @brief Initialize a rspq_ctx_t structure */
static void rspq_init_context(rspq_ctx_t *ctx, int buf_size)
{
    memset(ctx, 0, sizeof(rspq_ctx_t));
    ctx->buffers[0] = malloc_uncached(buf_size * sizeof(uint32_t));
    ctx->buffers[1] = malloc_uncached(buf_size * sizeof(uint32_t));
    memset(ctx->buffers[0], 0, buf_size * sizeof(uint32_t));
    memset(ctx->buffers[1], 0, buf_size * sizeof(uint32_t));
    ctx->buf_idx = 0;
    ctx->buf_size = buf_size;
    ctx->cur = ctx->buffers[0];
    ctx->sentinel = ctx->cur + buf_size - RSPQ_MAX_COMMAND_SIZE;
}

static void rspq_close_context(rspq_ctx_t *ctx)
{
    free_uncached(ctx->buffers[1]);
    free_uncached(ctx->buffers[0]);
}

void rspq_init(void)
{
    // Do nothing if rspq_init has already been called
    if (rspq_initialized)
        return;

    rspq_ctx = NULL;
    rspq_cur_pointer = NULL;
    rspq_cur_sentinel = NULL;

    // Allocate RSPQ contexts
    rspq_init_context(&lowpri, RSPQ_DRAM_LOWPRI_BUFFER_SIZE);
    lowpri.sp_status_bufdone = SP_STATUS_SIG_BUFDONE_LOW;
    lowpri.sp_wstatus_set_bufdone = SP_WSTATUS_SET_SIG_BUFDONE_LOW;
    lowpri.sp_wstatus_clear_bufdone = SP_WSTATUS_CLEAR_SIG_BUFDONE_LOW;

    rspq_init_context(&highpri, RSPQ_DRAM_HIGHPRI_BUFFER_SIZE);
    highpri.sp_status_bufdone = SP_STATUS_SIG_BUFDONE_HIGH;
    highpri.sp_wstatus_set_bufdone = SP_WSTATUS_SET_SIG_BUFDONE_HIGH;
    highpri.sp_wstatus_clear_bufdone = SP_WSTATUS_CLEAR_SIG_BUFDONE_HIGH;

    // Start in low-priority mode
    rspq_switch_context(&lowpri);

    // Allocate the RDP dynamic buffers.
    rspq_rdp_dynamic_buffers[0] = malloc_uncached(RDPQ_DYNAMIC_BUFFER_SIZE);
    rspq_rdp_dynamic_buffers[1] = malloc_uncached(RDPQ_DYNAMIC_BUFFER_SIZE);

    // Verify consistency of state
    int banner_offset = RSPQ_DATA_ADDRESS + offsetof(rsp_queue_t, banner);
    assertf(!memcmp(rsp_queue.data + banner_offset, "Dragon RSP Queue", 16),
        "rsp_queue_t does not seem to match DMEM; did you forget to update it?");

    // Load initial settings
    memcpy(&rspq_data, rsp_queue.data + RSPQ_DATA_ADDRESS, sizeof(rsp_queue_t));
    rspq_data.rspq_dram_lowpri_addr = PhysicalAddr(lowpri.cur);
    rspq_data.rspq_dram_highpri_addr = PhysicalAddr(highpri.cur);
    rspq_data.rspq_dram_addr = rspq_data.rspq_dram_lowpri_addr;
    rspq_data.rspq_rdp_buffers[0] = PhysicalAddr(rspq_rdp_dynamic_buffers[0]);
    rspq_data.rspq_rdp_buffers[1] = PhysicalAddr(rspq_rdp_dynamic_buffers[1]);
    rspq_data.rspq_rdp_current = rspq_data.rspq_rdp_buffers[0];
    rspq_data.rspq_rdp_sentinel = rspq_data.rspq_rdp_buffers[0] + RDPQ_DYNAMIC_BUFFER_SIZE;
    rspq_data.rspq_ovl_table.data_rdram[0] = PhysicalAddr(dummy_overlay_state) | (0<<24);

#if RSPQ_PROFILE
    rspq_data.rspq_profile_cur_slot = -1;
#endif
    
    // Init syncpoints
    rspq_syncpoints_genid = 0;
    __rspq_syncpoints_done[0] = 0;

    // Init blocks
    rspq_block = NULL;
    rspq_is_running = false;

    // Activate SP interrupt (used for syncpoints)
    register_SP_handler(rspq_sp_interrupt);
    set_SP_interrupt(1);

    rspq_initialized = 1;

    // Initialize the RDP
    MEMORY_BARRIER();
    *DP_STATUS = DP_WSTATUS_RESET_XBUS_DMEM_DMA | DP_WSTATUS_RESET_FLUSH | DP_WSTATUS_RESET_FREEZE;
    MEMORY_BARRIER();
    RSP_WAIT_LOOP(500) {
        if (!(*DP_STATUS & (DP_STATUS_START_VALID | DP_STATUS_END_VALID))) {
            break;
        }
    }
    MEMORY_BARRIER();
    *DP_START = rspq_data.rspq_rdp_buffers[0];
    MEMORY_BARRIER();
    *DP_END = rspq_data.rspq_rdp_buffers[0];
    MEMORY_BARRIER();

    rspq_start();
}

/** @brief Stop the RSP queue engine in the RSP */
static void rspq_stop(void)
{
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    MEMORY_BARRIER();

    rspq_is_running = 0;
}

void rspq_close(void)
{
    rspq_stop();
    
    rspq_initialized = 0;

    free_uncached(rspq_rdp_dynamic_buffers[0]);
    free_uncached(rspq_rdp_dynamic_buffers[1]);

    rspq_close_context(&highpri);
    rspq_close_context(&lowpri);

    set_SP_interrupt(0);
    unregister_SP_handler(rspq_sp_interrupt);
}

static void* overlay_get_state(rsp_ucode_t *overlay_ucode, int *state_size)
{
    uint32_t rspq_data_size = rsp_queue_data_end - rsp_queue_data_start;
    rspq_overlay_header_t *overlay_header = (rspq_overlay_header_t*)(overlay_ucode->data + rspq_data_size);

    uint32_t state_offset = (overlay_header->state_start & 0xFFF);
    assertf(state_offset >= rspq_data_size + sizeof(rspq_overlay_header_t), 
        "Saved overlay state must start after the overlay header (overlay: %s)!", overlay_ucode->name);

    void* state_ptr = overlay_ucode->data + state_offset;
    assertf(state_ptr + overlay_header->state_size + 1 <= overlay_ucode->data_end, 
    "Saved overlay state must be completely within the data segment (overlay: %s)", overlay_ucode->name);

    if (state_size)
        *state_size = overlay_header->state_size;

    return state_ptr;
}

void* rspq_overlay_get_state(rsp_ucode_t *overlay_ucode)
{
    // Get the RDRAM pointers to the overlay state
    int state_size;
    uint8_t* state_ptr = overlay_get_state(overlay_ucode, &state_size);

    if (rspq_is_running)
    {
        // Make sure the RSP is idle, otherwise the overlay state could be modified
        // at any time causing race conditions.
        rspq_wait();

        // Check if the current overlay is the one that we are requesting the
        // state for. If so, read back the latest updated state from DMEM
        // manually via DMA, so that the caller finds the latest contents.
        const char *ovl_name; uint8_t ovl_id;
        rsp_queue_t *rspq = (rsp_queue_t*)((uint8_t*)SP_DMEM + RSPQ_DATA_ADDRESS);
        rspq_get_current_ovl(rspq, &ovl_id, &ovl_name);

        if (ovl_id && rspq_overlay_ucodes[ovl_id] == overlay_ucode) {
            rsp_read_data(state_ptr, state_size, state_ptr - overlay_ucode->data);
        }
    }

    return state_ptr;
}

rsp_queue_t* __rspq_get_state(void)
{
    // Make sure the RSP is idle, otherwise the state could be modified
    // at any time causing race conditions.
    rspq_wait();

    // Read the state and return it
    rsp_read_data(&rspq_data, sizeof(rsp_queue_t), RSPQ_DATA_ADDRESS);
    return &rspq_data;
}

static uint32_t rspq_overlay_get_command_count(rspq_overlay_header_t *header)
{
    for (uint32_t i = 0; i < RSPQ_MAX_OVERLAY_COMMAND_COUNT; i++)
    {
        if (header->commands[i] == 0) {
            return i;
        }
    }

    assertf(0, "Overlays can only define up to %d commands!", RSPQ_MAX_OVERLAY_COMMAND_COUNT);
    return 0;
}

static uint32_t rspq_find_new_overlay_id(uint32_t slot_count)
{
    uint32_t cur_free_slots = 0;

    for (uint32_t i = 1; i <= RSPQ_MAX_OVERLAYS - slot_count; i++)
    {
        // If this slot is occupied, reset number of free slots found
        if (rspq_data.rspq_ovl_table.data_rdram[i] != 0) {
            cur_free_slots = 0;
            continue;
        }

        ++cur_free_slots;

        // If required number of slots have not been found, keep searching
        if (cur_free_slots < slot_count) {
            continue;
        }

        // We have found <slot_count> consecutive free slots
        uint32_t found_slot = i - slot_count + 1;
        return found_slot;
    }
    
    // If no free slots have been found, return zero, which means the search failed.
    return 0;
}

static void rspq_update_tables(bool is_highpri)
{
    // Issue a DMA request to update the overlay tables in DMEM.
    // Note that we don't use rsp_load_data() here and instead use the dma command,
    // so we don't need to synchronize with the RSP. All commands queued after this
    // point will be able to use the newly registered overlay.
    data_cache_hit_writeback_invalidate(&rspq_data.rspq_ovl_table, sizeof(rspq_ovl_table_t));
    if (is_highpri) rspq_highpri_begin();
    rspq_dma_to_dmem(
        RSPQ_DATA_ADDRESS + offsetof(rsp_queue_t, rspq_ovl_table),
        &rspq_data.rspq_ovl_table, sizeof(rspq_ovl_table_t), false);
    if (is_highpri) rspq_highpri_end();
}

static uint32_t rspq_overlay_register_internal(rsp_ucode_t *overlay_ucode, uint32_t static_id)
{
    assertf(rspq_initialized, "rspq_overlay_register must be called after rspq_init!");
    assert(overlay_ucode);

    // The RSPQ ucode is always linked into overlays, so we need to load the overlay from an offset.
    uint32_t rspq_text_size = rsp_queue_text_end - rsp_queue_text_start;
    uint32_t rspq_data_size = rsp_queue_data_end - rsp_queue_data_start;

    assertf(memcmp(rsp_queue_text_start, overlay_ucode->code, rspq_text_size) == 0,
        "Common code of overlay %s does not match!", overlay_ucode->name);
    assertf(memcmp(rsp_queue_data_start, overlay_ucode->data, rspq_data_size) == 0,
        "Common data of overlay %s does not match!", overlay_ucode->name);

    void *overlay_code = overlay_ucode->code + rspq_text_size;
    void *overlay_data = overlay_ucode->data + rspq_data_size;
    int overlay_data_size = (void*)overlay_ucode->data_end - overlay_data;
    int overlay_code_size = (void*)overlay_ucode->code_end - overlay_code;

    // Check if the overlay has been registered already
    for (uint32_t i = 0; i < RSPQ_MAX_OVERLAYS; i++)
    {
        assertf((rspq_data.rspq_ovl_table.data_rdram[i] & 0x00FFFFFF) != PhysicalAddr(overlay_data),
            "Overlay %s is already registered!", overlay_ucode->name);
    }

    // determine number of commands and try to allocate ID(s) accordingly
    rspq_overlay_header_t *overlay_header = (rspq_overlay_header_t*)overlay_data;
    assertf((uint16_t)(overlay_header->state_size + 1) > 0, "Size of saved state must not be zero (overlay: %s)", overlay_ucode->name);
    assertf((overlay_header->state_size + 1) <= 0x1000, "Saved state is too large: %#x", overlay_header->state_size + 1);

    uint32_t command_count = rspq_overlay_get_command_count(overlay_header);
    uint32_t slot_count = DIVIDE_CEIL(command_count, 16);

    uint32_t id = static_id >> 28;
    if (id != 0) {
        for (uint32_t i = 0; i < slot_count; i++)
        {
            assertf(rspq_data.rspq_ovl_table.data_rdram[id + i] == 0,
                "Tried to register overlay %s in already occupied slot!", overlay_ucode->name);
        }
    } else {
        id = rspq_find_new_overlay_id(slot_count);
        assertf(id != 0, "Not enough consecutive free slots available for overlay %s (%ld commands)!",
            overlay_ucode->name, command_count);
    }

    // Store the address of the data segment in the overlay table, packed with the size
    for (uint32_t i = 0; i < slot_count; i++) {
        rspq_data.rspq_ovl_table.data_rdram[id + i] = 
            PhysicalAddr(overlay_data) | (((overlay_data_size - 1) >> 4) << 24);
        rspq_data.rspq_ovl_table.idmap[id + i] = id;
    }

    // Fill information in the overlay header
    overlay_header->text_size = overlay_code_size;
    overlay_header->text_rdram = PhysicalAddr(overlay_code);
    overlay_header->state_rdram = PhysicalAddr(overlay_ucode->data) + overlay_header->state_start;
    overlay_header->command_base = id << 5;
    data_cache_hit_writeback_invalidate(overlay_header, sizeof(rspq_overlay_header_t));

    // Save the overlay pointer
    rspq_overlay_ucodes[id] = overlay_ucode;

    rspq_update_tables(true);

    return id << 28;
}

uint32_t rspq_overlay_register(rsp_ucode_t *overlay_ucode)
{
    return rspq_overlay_register_internal(overlay_ucode, 0);
}

void rspq_overlay_register_static(rsp_ucode_t *overlay_ucode, uint32_t overlay_id)
{
    assertf((overlay_id & 0x0FFFFFFF) == 0, 
        "the specified overlay_id should only use the top 4 bits (must be preshifted by 28) (overlay: %s)", overlay_ucode->name);
    rspq_overlay_register_internal(overlay_ucode, overlay_id);
}

void rspq_overlay_unregister(uint32_t overlay_id)
{
    assertf(overlay_id != 0, "Overlay 0 cannot be unregistered!");

    // Un-shift ID to convert to actual index again
    uint8_t ovl_id = overlay_id >> 28;
    rsp_ucode_t *ucode = rspq_overlay_ucodes[ovl_id];

    rspq_overlay_header_t *overlay_header = (rspq_overlay_header_t*)(ucode->data + (rsp_queue_data_end - rsp_queue_data_start));
    int slot_count = DIVIDE_CEIL(rspq_overlay_get_command_count(overlay_header), 16);

    // Reset the command base in the overlay header
    overlay_header->command_base = 0;
    data_cache_hit_writeback_invalidate(overlay_header, sizeof(rspq_overlay_header_t));

    // Remove all registered ids
    for (int i = 0; i < slot_count; i++) {
        rspq_data.rspq_ovl_table.data_rdram[ovl_id + i] = 0;
        rspq_data.rspq_ovl_table.idmap[ovl_id + i] = 0;
    }

    rspq_update_tables(false);
}

/**
 * @brief Switch to the next write buffer for the current RSP queue.
 * 
 * This function is invoked by #rspq_write when the current buffer is
 * full, that is, when the write pointer (#rspq_cur_pointer) reaches
 * the sentinel (#rspq_cur_sentinel). This means that we cannot safely
 * write any more new command in the buffer (the remaining bytes are less
 * than the maximum command size), and thus a new buffer must be configured.
 * 
 * If we're creating a block, we need to allocate a new buffer from the heap.
 * Otherwise, if we're writing into either the lowpri or the highpri queue,
 * we need to switch buffer (double buffering strategy), making sure the
 * other buffer has been already fully executed by the RSP.
 */
__attribute__((noinline))
void rspq_next_buffer(void) {
    // If we're creating a block
    if (rspq_block) {
        // Allocate next chunk (double the size of the current one).
        // We use doubling here to reduce overheads for large blocks
        // and at the same time start small.
        if (rspq_block_size < RSPQ_BLOCK_MAX_SIZE) rspq_block_size *= 2;

        // Allocate a new chunk of the block and switch to it.
        uint32_t *rspq2 = malloc_uncached(rspq_block_size*sizeof(uint32_t));
        volatile uint32_t *prev = rspq_switch_buffer(rspq2, rspq_block_size, true);

        // Terminate the previous chunk with a JUMP op to the new chunk.
        rspq_append1(prev, RSPQ_CMD_JUMP, PhysicalAddr(rspq2));

        return;
    }

    // We are about to switch buffer. If the debugging engine is activate,
    // it is a good time to run it, so that it does not accumulate too many
    // commands.
    if (rdpq_trace) rdpq_trace();

    // Poll the deferred list at least once per buffer switch. We will poll
    // more if we need to wait
    __rspq_deferred_poll();

    // Wait until the previous buffer is executed by the RSP.
    // We cannot write to it if it's still being executed.
    // FIXME: this should probably transition to a sync-point,
    // so that the kernel can switch away while waiting. Even
    // if the overhead of an interrupt is obviously higher.
    MEMORY_BARRIER();
    if (!(*SP_STATUS & rspq_ctx->sp_status_bufdone)) {
        rspq_flush_internal();
        RSP_WAIT_LOOP(200) {
            __rspq_deferred_poll();
            if (*SP_STATUS & rspq_ctx->sp_status_bufdone)
                break;
        }
    }
    MEMORY_BARRIER();
    *SP_STATUS = rspq_ctx->sp_wstatus_clear_bufdone;
    MEMORY_BARRIER();

    // Switch current buffer
    rspq_ctx->buf_idx = 1-rspq_ctx->buf_idx;
    uint32_t *new = rspq_ctx->buffers[rspq_ctx->buf_idx];
    volatile uint32_t *prev = rspq_switch_buffer(new, rspq_ctx->buf_size, true);

    // Terminate the previous buffer with an op to set SIG_BUFDONE
    // (to notify when the RSP finishes the buffer), plus a jump to
    // the new buffer.
    rspq_append1(prev, RSPQ_CMD_WRITE_STATUS, rspq_ctx->sp_wstatus_set_bufdone);
    rspq_append1(prev, RSPQ_CMD_JUMP, PhysicalAddr(new));
    assert(prev+1 < (uint32_t*)(rspq_ctx->buffers[1-rspq_ctx->buf_idx]) + rspq_ctx->buf_size);
    rspq_flush_internal();
}

__attribute__((noinline))
static void rspq_flush_internal(void)
{
    // Tell the RSP to wake up because there is more data pending.
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();

    // Most of the times, the above is enough. But there is a small and very rare
    // race condition that can happen: if the above status change happens
    // exactly in the few instructions between RSP checking for the status
    // register ("mfc0 t0, COP0_SP_STATUS") RSP halting itself("break"),
    // the call to rspq_flush might have no effect (see command_wait_new_input in
    // rsp_queue.S).
    // In general this is not a big problem even if it happens, as the RSP
    // would wake up at the next flush anyway, but we guarantee that rspq_flush
    // does actually make the RSP finish the current buffer. To keep this
    // invariant, we wait 10 cycles and then issue the command again. This
    // make sure that even if the race condition happened, we still succeed
    // in waking up the RSP.
    __asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();
}

void rspq_flush(void)
{
    // If we are recording a block, flushes can be ignored.
    if (rspq_block) return;

    rspq_flush_internal();
    if (rdpq_trace) rdpq_trace();
}

void rspq_highpri_begin(void)
{
    assertf(rspq_ctx != &highpri, "already in highpri mode");
    assertf(!rspq_block, "cannot switch to highpri mode while creating a block");

    rspq_switch_context(&highpri);

    // Check if we're not at the beginning of the buffer. This avoids doing
    // OOB reads in the next check.
    if (rspq_cur_pointer != rspq_ctx->buffers[rspq_ctx->buf_idx]) {

        // If we're continuing on the same buffer another highpri sequence,
        // try to skip the highpri epilog and jump to the buffer continuation.
        // This is a small performance gain (the RSP doesn't need to exit and re-enter
        // the highpri mode) but it also allows to enqueue more than one highpri
        // sequence, since we only have a single SIG_HIGHPRI_REQUESTED and there
        // would be no way to tell the RSP "there are 3 sequences pending, so exit
        // and re-enter three times".
        // 
        // To skip the epilog we write single atomic words over  the epilog,
        // changing it with a JUMP to the buffer continuation. This operation
        // is completely safe because the RSP either see the memory before the
        // change (it sees the epilog) or after the change (it sees the new JUMP).
        // 
        // In the first case, it will run the epilog and then reenter the highpri
        // mode soon (as we're turning on SIG_HIGHPRI_REQUESTED anyway). In the 
        // second case, it's going to see the JUMP, skip the epilog and continue.
        // The SIG_HIGHPRI_REQUESTED bit will be set but this function, and reset
        // at the beginning of the new segment, but it doesn't matter at this point.
        if (rspq_cur_pointer[-3]>>24 == RSPQ_CMD_SWAP_BUFFERS) {
            volatile uint32_t *epilog = rspq_cur_pointer-4;
            rspq_append1(epilog, RSPQ_CMD_JUMP, PhysicalAddr(rspq_cur_pointer));
            rspq_append1(epilog, RSPQ_CMD_JUMP, PhysicalAddr(rspq_cur_pointer));
        }
    }

    // Clear SIG_HIGHPRI_REQUESTED and set SIG_HIGHPRI_RUNNING. This is normally done
    // automatically by RSP when entering highpri mode, but we want to still
    // add a command in case the previous epilog was skipped. Otherwise,
    // a dummy SIG_HIGHPRI_REQUESTED could stay on and eventually highpri
    // mode would enter once again.
    rspq_append1(rspq_cur_pointer, RSPQ_CMD_WRITE_STATUS,
        SP_WSTATUS_CLEAR_SIG_HIGHPRI_REQUESTED | SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING);
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_SIG_HIGHPRI_REQUESTED;
    rspq_flush_internal();
}

void rspq_highpri_end(void)
{
    assertf(rspq_ctx == &highpri, "not in highpri mode");

    // Write the highpri epilog. The epilog starts with a JUMP to the next
    // instruction because we want to force the RSP to reload the buffer
    // from RDRAM in case the epilog has been overwritten by a new highpri
    // queue (see rspq_highpri_begin).
    rspq_append1(rspq_cur_pointer, RSPQ_CMD_JUMP, PhysicalAddr(rspq_cur_pointer+1));
    rspq_append3(rspq_cur_pointer, RSPQ_CMD_SWAP_BUFFERS,
        RSPQ_LOWPRI_CALL_SLOT<<2, RSPQ_HIGHPRI_CALL_SLOT<<2,
        SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING);
    rspq_flush_internal();
    rspq_switch_context(&lowpri);
}

void rspq_highpri_sync(void)
{
    assertf(rspq_ctx != &highpri, "this function can only be called outside of highpri mode");

    // Make sure the RSP is running, otherwise we might be blocking forever.
    rspq_flush_internal();

    RSP_WAIT_LOOP(200) {
        __rspq_deferred_poll();
        if (!(*SP_STATUS & (SP_STATUS_SIG_HIGHPRI_REQUESTED | SP_STATUS_SIG_HIGHPRI_RUNNING)))
            break;
    }
}

void rspq_block_begin(void)
{
    assertf(!rspq_block, "a block was already being created");
    assertf(rspq_ctx != &highpri, "cannot create a block in highpri mode");

    // Allocate a new block (at minimum size) and initialize it.
    rspq_block_size = RSPQ_BLOCK_MIN_SIZE;
    rspq_block = malloc_uncached(sizeof(rspq_block_t) + rspq_block_size*sizeof(uint32_t));
    rspq_block->nesting_level = 0;
    rspq_block->rdp_block = NULL;

    // Switch to the block buffer. From now on, all rspq_writes will
    // go into the block.
    rspq_switch_context(NULL);
    rspq_switch_buffer(rspq_block->cmds, rspq_block_size, true);

    __rdpq_block_begin();
}

rspq_block_t* rspq_block_end(void)
{
    assertf(rspq_block, "a block was not being created");

    // Terminate the block with a RET command, encoding
    // the nesting level which is used as stack slot by RSP.
    rspq_append1(rspq_cur_pointer, RSPQ_CMD_RET, rspq_block->nesting_level<<2);

    // Switch back to the normal display list
    rspq_switch_context(&lowpri);

    // Save pointer to rdpq block (if any)
    rspq_block->rdp_block = __rdpq_block_end();

    // Return the created block
    rspq_block_t *b = rspq_block;
    rspq_block = NULL;
    return b;
}

void rspq_block_free(rspq_block_t *block)
{
    // Free RDP blocks first
    __rdpq_block_free(block->rdp_block);

    // Start from the commands in the first chunk of the block
    int size = RSPQ_BLOCK_MIN_SIZE;
    void *start = block;
    uint32_t *ptr = block->cmds + size;
    while (1) {
        // Rollback until we find a non-zero command
        while (*--ptr == 0x00) {}
        uint32_t cmd = *ptr;

        // If the last command is a JUMP
        if (cmd>>24 == RSPQ_CMD_JUMP) {
            // Free the memory of the current chunk.
            free_uncached(start);
            // Get the pointer to the next chunk
            start = UncachedAddr(0x80000000 | (cmd & 0xFFFFFF));
            if (size < RSPQ_BLOCK_MAX_SIZE) size *= 2;
            ptr = (uint32_t*)start + size;
            continue;
        }
        // If the last command is a RET
        if (cmd>>24 == RSPQ_CMD_RET) {
            // This is the last chunk, free it and exit
            free_uncached(start);
            return;
        }
        // The last command is neither a JUMP nor a RET:
        // this is an invalid chunk of a block, better assert.
        assertf(0, "invalid terminator command in block: %08lx\n", cmd);
    }
}

void rspq_block_run(rspq_block_t *block)
{
    // TODO: add support for block execution in highpri mode. This would be
    // possible by allocating another range of nesting levels (eg: 8-16) to use
    // in highpri mode (to avoid stepping on the call stack of lowpri). This
    // would basically mean that a block can either work in highpri or in lowpri
    // mode, but it might be an acceptable limitation.
    assertf(rspq_ctx != &highpri, "block run is not supported in highpri mode");

    // Notify rdpq engine we are about to run a block
    __rdpq_block_run(block->rdp_block);

    // Write the CALL op. The second argument is the nesting level
    // which is used as stack slot in the RSP to save the current
    // pointer position.
    rspq_int_write(RSPQ_CMD_CALL, PhysicalAddr(block->cmds), block->nesting_level << 2);

    // If this is CALL within the creation of a block, update
    // the nesting level. A block's nesting level must be bigger
    // than the nesting level of all blocks called from it.
    if (rspq_block && rspq_block->nesting_level <= block->nesting_level) {
        rspq_block->nesting_level = block->nesting_level + 1;
        assertf(rspq_block->nesting_level < RSPQ_MAX_BLOCK_NESTING_LEVEL,
            "reached maximum number of nested block runs");
    }
}

void rspq_block_run_rsp(int nesting_level)
{
    __rdpq_block_run(NULL);
    if (rspq_block && rspq_block->nesting_level <= nesting_level) {
        rspq_block->nesting_level = nesting_level + 1;
        assertf(rspq_block->nesting_level < RSPQ_MAX_BLOCK_NESTING_LEVEL,
            "reached maximum number of nested block runs");
    }    
}

void rspq_noop()
{
    rspq_int_write(RSPQ_CMD_NOOP);
}

rspq_syncpoint_t rspq_syncpoint_new(void)
{   
    assertf(rspq_ctx != &highpri, "cannot create syncpoint in highpri mode");
    assertf(!rspq_block, "cannot create syncpoint in a block");
    assertf(rspq_ctx != &highpri, "cannot create syncpoint in highpri mode");

    // To create a syncpoint, schedule a CMD_TEST_WRITE_STATUS command that:
    //   1. Wait for SP_STATUS_SIG_SYNCPOINT to go zero. This is cleared in
    //      the RSP interrupt routine and basically make sure that any other
    //      pending interrupt had been acknowledged. Otherwise, we might
    //      end up coalescing multiple RSP interrupts, and thus missing
    //      syncpoints (as we need exactly one handled interrupt per syncpoint).
    //   2. Write SP_STATUS with SP_WSTATUS_SET_SIG_SYNCPOINT and SP_WSTATUS_SET_INTR,
    //      forcing a new RSP interrupt to be generated. The interrupt routine
    //      (#rspq_sp_interrupt) will notice the SP_STATUS_SIG_SYNCPOINT and know
    //      that the interrupt has been generated for a syncpoint.
    rspq_int_write(RSPQ_CMD_TEST_WRITE_STATUS, 
        SP_WSTATUS_SET_INTR | SP_WSTATUS_SET_SIG_SYNCPOINT,
        SP_STATUS_SIG_SYNCPOINT);
    return ++rspq_syncpoints_genid;
}

bool rspq_syncpoint_check(rspq_syncpoint_t sync_id) 
{
    int difference = (int)((uint32_t)(sync_id) - (uint32_t)(__rspq_syncpoints_done[0]));
    return difference <= 0;
}

void rspq_syncpoint_wait(rspq_syncpoint_t sync_id)
{
    if (rspq_syncpoint_check(sync_id))
        return;

    assertf(get_interrupts_state() == INTERRUPTS_ENABLED,
        "deadlock: interrupts are disabled");

    // Make sure the RSP is running, otherwise we might be blocking forever.
    rspq_flush_internal();

    // Spinwait until the the syncpoint is reached.
    // TODO: with the kernel, it will be possible to wait for the RSP interrupt
    // to happen, without spinwaiting.
    RSP_WAIT_LOOP(200) {
        __rspq_deferred_poll();
        if (rspq_syncpoint_check(sync_id))
            break;
    }
}

/**
 * @brief Polls the deferred calls list, calling callbacks ready to be called.
 * 
 * This function will check the deferred call list and if there is one callback
 * ready to be called, it will call it and remove it from the list.
 * 
 * The function will process maximum one callback per call, so that it does
 * not steal too much CPU time.
 * 
 * @return true   if there are still callbacks to be processed
 * @return false  if there are no more callbacks to be processed
 */ 
bool __rspq_deferred_poll(void)
{
    rspq_deferred_call_t *prev = NULL, *cur =  __rspq_defcalls_head;
    while (cur != NULL) {
        rspq_deferred_call_t *next = cur->next;

        // Since the list is chronologically sorted, once we reach the first
        // call that is still waiting for its RSP checkpoint, we can stop.
        if (!rspq_syncpoint_check(cur->sync))
            break;

        // If this call requires waiting for SYNC_FULL, check if we reached it.
        // Otherwise, jsut skio it and go through the list: maybe a later callback
        // does not require RDP and can be called.
        if (cur->flags & RSPQ_DCF_WAITRDP) {
            int difference = (int)((uint32_t)(cur->sync) - (uint32_t)(__rdpq_syncpoint_at_syncfull));
            if (difference <= 0)
                cur->flags &= ~RSPQ_DCF_WAITRDP;
        }

        // If this call does not require waiting for next SYNC_FULL, call it.
        if (!(cur->flags & RSPQ_DCF_WAITRDP)) {
            // Call the deferred calllback
            cur->func(cur->arg);

            // Remove it from the list (possibly updating the head/tail pointer)
            if (prev)
                prev->next = next;
            else
                __rspq_defcalls_head = next;
            if (!next)
                __rspq_defcalls_tail = prev;
            free(cur);
            break;
        }

        prev = cur;
        cur = next;
    }

    return __rspq_defcalls_head != NULL;
}

rspq_syncpoint_t __rspq_call_deferred(void (*func)(void *), void *arg, bool waitrdp)
{
    assertf(rspq_ctx != &highpri, "cannot defer in highpri mode");
    assertf(!rspq_block, "cannot defer in a block");

    // Allocate a new deferred call
    rspq_deferred_call_t *call = malloc(sizeof(rspq_deferred_call_t));
    call->func = func;
    call->arg = arg;
    call->next = NULL;
    call->sync = rspq_syncpoint_new();
    if (waitrdp)
        call->flags |= RSPQ_DCF_WAITRDP;

    // Add it to the list of deferred calls
    if (__rspq_defcalls_tail) {
        __rspq_defcalls_tail->next = call;
    } else {
        __rspq_defcalls_head = call;
    }
    __rspq_defcalls_tail = call;

    return call->sync;
}

rspq_syncpoint_t rspq_syncpoint_new_cb(void (*func)(void *), void *arg)
{
    return __rspq_call_deferred(func, arg, false);
}

void rspq_wait(void)
{
    // Check if the RDPQ module was initialized.
    if (__rdpq_inited) {
        // If so, a full sync requires also waiting for RDP to finish.
        rdpq_fence();

        // Also force a buffer switch to go back to dynamic buffer. This is useful
        // in the case the RDP is still pointing to a static buffer (after a block
        // is just finished). This allows the user to safely free the static buffer
        // after rspq_wait(), as intuition would suggest.
        rspq_int_write(RSPQ_CMD_RDP_SET_BUFFER, 0, 0, 0);
    }
    
    // Wait until RSP has finished processing the queue
    rspq_syncpoint_wait(rspq_syncpoint_new());

    // Update the tracing engine (if enabled)
    if (rdpq_trace) rdpq_trace();

    // Make sure to process all deferred calls. Since this is a full sync point,
    // it makes sense to give this guarantee to the user.
    RSP_WAIT_LOOP(500) {
        if (!__rspq_deferred_poll())
            break;
    }

    // Last thing to check is whether there is a RSP DMA in progress. This is
    // basically impossible because RSP DMA is very fast, but we still keep
    // this code even just as documentation that we want to ensure that rspq_wait()
    // exits with all RSP idle.
    if (UNLIKELY(*SP_STATUS & SP_STATUS_DMA_BUSY)) {
        RSP_WAIT_LOOP(200) {
            if (!(*SP_STATUS & SP_STATUS_DMA_BUSY))
                break;
        }
    }
}

/// @cond
void rspq_signal(uint32_t signal)
{
    const uint32_t allowed_mask = SP_WSTATUS_CLEAR_SIG0|SP_WSTATUS_SET_SIG0;
    assertf((signal & allowed_mask) == signal, "rspq_signal called with a mask that contains bits outside SIG0: %lx", signal);

    rspq_int_write(RSPQ_CMD_WRITE_STATUS, signal);
}
/// @endcond

static void rspq_dma(void *rdram_addr, uint32_t dmem_addr, uint32_t len, uint32_t flags)
{
    rspq_int_write(RSPQ_CMD_DMA, PhysicalAddr(rdram_addr), dmem_addr, len, flags);
}

void rspq_dma_to_rdram(void *rdram_addr, uint32_t dmem_addr, uint32_t len, bool is_async)
{
    rspq_dma(rdram_addr, dmem_addr, len - 1, 0xFFFF8000 | (is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL));
}

void rspq_dma_to_dmem(uint32_t dmem_addr, void *rdram_addr, uint32_t len, bool is_async)
{
    rspq_dma(rdram_addr, dmem_addr, len - 1, is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL);
}

/* Extern inline instantiations. */
extern inline rspq_write_t rspq_write_begin(uint32_t ovl_id, uint32_t cmd_id, int size);
extern inline void rspq_write_arg(rspq_write_t *w, uint32_t value);
extern inline void rspq_write_end(rspq_write_t *w);
extern inline void rspq_call_deferred(void (*func)(void *), void *arg);

