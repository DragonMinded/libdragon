/**
 * @file rspq.c
 * @brief RSP Command queue
 * @ingroup rsp
 */

/**
 * ## RSP Queue Architecture
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
 * ## RSP Queue internal commands
 *
 * To manage the queue and implement all the various features, rspq reserves
 * for itself the overlay ID 0x0 to implement internal commands.
 * 
 * ### CMD 0x00: INVALID
 * 
 * Reserved ID for invalid command. This is used as a marker so that RSP knows
 * when it has caught up with CPU and reached an empty portion of the buffer.
 * 
 * ### CMD 0x01: NOOP
 * 
 * This commands does nothing. It can be useful for debugging purposes.
 * 
 * ### CMD 0x02: JUMP
 * 
 * This commands tells the RSP to start fetching commands from a new address.
 * It is mainly used internally to implement the queue as a ring buffer (jumping
 * at the start when we reach the end of the buffer).
 * 
 * ### CMD 0x03: CALL
 * 
 * This command is used by the block functions to implement the execution of
 * a block. It tells RSP to starts fetching commands from the block address,
 * saving the current address in an internal save slot in DMEM, from which
 * it will be recovered by CMD_RET. Using multiple slots allow for nested
 * calls.
 * 
 * ### CMD 0x04: RET
 * 
 * This command tells the RSP to recover the buffer address from a save slot
 * (from which it was currently saved by a CALL command) and begin fetching
 * commands from there. It is used to finish the execution of a block.
 * 
 * ### CMD 0x05: DMA
 * 
 * This commands runs a DMA transfer (either DRAM to DMEM, or DMEM to DRAM).
 * It is used by #rspq_overLay_register to register a new overlay table into
 * DMEM while the RSP is already running (to allow for overlays to be
 * registered even after boot), and can be used by the users to perform
 * manual DMA transfers to and from DMEM without risking a conflict with the
 * RSP itself.
 * 
 * ### CMD 0x06: WRITE_STATUS
 * 
 * This command asks the RSP to write to the SP_STATUS register. It is normally
 * used to set/clear signals or to raise RSP interrupts.
 * 
 * ### CMD 0x07: SWAP_BUFFERS
 * 
 * This command is used as part of the highpri feature. It allows to switch
 * between lowpri and highpri queue, by saving the current buffer pointer
 * in a special save slot, and restoring the buffer pointer of the other
 * queue from another slot. It is used internally by RSP to switch to highpri
 * when the SIG_HIGHPRI is found set; then it is explicitly enqueued by the
 * CPU when the highpri queue is finished (in #rspq_highpri_end) to switch
 * back to lowpri.
 * 
 * ### CMD 0x08: TEST_WRITE_STATUS
 * 
 * This commands does a test-and-write sequence on the SP_STATUS register: first,
 * it waits for a certain mask of bits to become zero, looping on it. Then
 * it writes a mask to the register. It is used as part of the syncpoint
 * feature to raise RSP interrupts, while waiting for the previous
 * interrupt to be processed (coalescing interrupts would cause syncpoints
 * to be missed).
 * 
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>
#include <malloc.h>
#include "rspq_internal.h"
#include "utils.h"
#include "../../build/rspq/rspq_symbols.h"

#define RSPQ_CMD_NOOP              0x01
#define RSPQ_CMD_JUMP              0x02
#define RSPQ_CMD_CALL              0x03
#define RSPQ_CMD_RET               0x04
#define RSPQ_CMD_DMA               0x05
#define RSPQ_CMD_WRITE_STATUS      0x06
#define RSPQ_CMD_SWAP_BUFFERS      0x07
#define RSPQ_CMD_TEST_WRITE_STATUS 0x08

#define rspq_append1(ptr, cmd, arg1) ({ \
    ((volatile uint32_t*)(ptr))[0] = ((cmd)<<24) | (arg1); \
    ptr += 1; \
})

#define rspq_append2(ptr, cmd, arg1, arg2) ({ \
    ((volatile uint32_t*)(ptr))[1] = (arg2); \
    ((volatile uint32_t*)(ptr))[0] = ((cmd)<<24) | (arg1); \
    ptr += 2; \
})

#define rspq_append3(ptr, cmd, arg1, arg2, arg3) ({ \
    ((volatile uint32_t*)(ptr))[1] = (arg2); \
    ((volatile uint32_t*)(ptr))[2] = (arg3); \
    ((volatile uint32_t*)(ptr))[0] = ((cmd)<<24) | (arg1); \
    ptr += 3; \
})

static void rspq_crash_handler(rsp_snapshot_t *state);

DEFINE_RSP_UCODE(rsp_queue, 
    .crash_handler = rspq_crash_handler);

typedef struct rspq_overlay_t {
    uint32_t code;
    uint32_t data;
    uint32_t data_buf;
    uint16_t code_size;
    uint16_t data_size;
} rspq_overlay_t;

typedef struct rspq_overlay_header_t {
    uint32_t state_start;
    uint16_t state_size;
    uint16_t command_base;
} rspq_overlay_header_t;

typedef struct rspq_block_s {
    uint32_t nesting_level;
    uint32_t cmds[];
} rspq_block_t;

typedef struct rspq_overlay_tables_s {
    uint8_t overlay_table[RSPQ_OVERLAY_TABLE_SIZE];
    rspq_overlay_t overlay_descriptors[RSPQ_MAX_OVERLAY_COUNT];
} rspq_overlay_tables_t;

typedef struct rsp_queue_s {
    rspq_overlay_tables_t tables;
    uint32_t rspq_pointer_stack[RSPQ_MAX_BLOCK_NESTING_LEVEL];
    uint32_t rspq_dram_lowpri_addr;
    uint32_t rspq_dram_highpri_addr;
    uint32_t rspq_dram_addr;
    int16_t current_ovl;
    uint16_t primode_status_check;
} __attribute__((aligned(16), packed)) rsp_queue_t;



typedef struct {
    void *buffers[2];
    int buf_size;
    int buf_idx;
    uint32_t sp_status_bufdone, sp_wstatus_set_bufdone, sp_wstatus_clear_bufdone;
    volatile uint32_t *cur;
    volatile uint32_t *sentinel;
} rspq_ctx_t;

static rsp_queue_t rspq_data;
#define rspq_data_ptr ((rsp_queue_t*)UncachedAddr(&rspq_data))

static uint8_t rspq_overlay_count = 0;

static rspq_block_t *rspq_block;
static int rspq_block_size;

rspq_ctx_t *rspq_ctx;
volatile uint32_t *rspq_cur_pointer;
volatile uint32_t *rspq_cur_sentinel;

rspq_ctx_t lowpri, highpri;

static int rspq_syncpoints_genid;
volatile int rspq_syncpoints_done;

static bool rspq_is_running;

static uint64_t dummy_overlay_state;

static void rspq_flush_internal(void);

static void rspq_sp_interrupt(void) 
{
    uint32_t status = *SP_STATUS;
    uint32_t wstatus = 0;

    if (status & SP_STATUS_SIG_SYNCPOINT) {
        wstatus |= SP_WSTATUS_CLEAR_SIG_SYNCPOINT;
        ++rspq_syncpoints_done;
    }

    MEMORY_BARRIER();

    if (wstatus)
        *SP_STATUS = wstatus;
}

static void rspq_crash_handler(rsp_snapshot_t *state)
{
    rsp_queue_t *rspq = (rsp_queue_t*)state->dmem;
    printf("RSPQ: Normal  DRAM address: %08lx\n", rspq->rspq_dram_lowpri_addr);
    printf("RSPQ: Highpri DRAM address: %08lx\n", rspq->rspq_dram_highpri_addr);
    printf("RSPQ: Current DRAM address: %08lx + %lx = %08lx\n", 
        rspq->rspq_dram_addr, state->gpr[28], rspq->rspq_dram_addr + state->gpr[28]);
    printf("RSPQ: Overlay: %x\n", rspq->current_ovl);
    debugf("RSPQ: Command queue:\n");
    for (int j=0;j<4;j++) {        
        for (int i=0;i<16;i++)
            debugf("%08lx%c", SP_DMEM[0x140/4+i+j*16], state->gpr[28] == (j*16+i)*4 ? '*' : ' ');
        debugf("\n");
    }
    debugf("RSPQ: RDRAM Command queue:\n");
    uint32_t *q = (uint32_t*)(0xA0000000 | (rspq->rspq_dram_addr & 0xFFFFFF));
    for (int j=0;j<4;j++) {        
        for (int i=0;i<16;i++)
            debugf("%08lx ", q[i+j*16]);
        debugf("\n");
    }
}

__attribute__((noinline))
static void rspq_switch_context(rspq_ctx_t *new)
{
    if (rspq_ctx) {    
        rspq_ctx->cur = rspq_cur_pointer;
        rspq_ctx->sentinel = rspq_cur_sentinel;
    }

    rspq_ctx = new;
    rspq_cur_pointer = rspq_ctx ? rspq_ctx->cur : NULL;
    rspq_cur_sentinel = rspq_ctx ? rspq_ctx->sentinel : NULL;
}

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
    rspq_cur_sentinel = new + size - RSPQ_MAX_COMMAND_SIZE;

    // Return a pointer to the previous buffer
    return prev;
}


void rspq_start(void)
{
    if (rspq_is_running)
        return;

    rsp_wait();
    rsp_load(&rsp_queue);

    // Load data with initialized overlays into DMEM
    rsp_load_data(rspq_data_ptr, sizeof(rsp_queue_t), 0);

    static rspq_overlay_header_t dummy_header = (rspq_overlay_header_t){
        .state_start = 0,
        .state_size = 7,
        .command_base = 0
    };

    rsp_load_data(&dummy_header, sizeof(dummy_header), RSPQ_OVL_DATA_ADDR);

    MEMORY_BARRIER();

    *SP_STATUS = SP_WSTATUS_CLEAR_SIG0 | 
                 SP_WSTATUS_CLEAR_SIG1 | 
                 SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING | 
                 SP_WSTATUS_CLEAR_SIG_SYNCPOINT |
                 SP_WSTATUS_SET_SIG_BUFDONE_LOW |
                 SP_WSTATUS_SET_SIG_BUFDONE_HIGH |
                 SP_WSTATUS_CLEAR_SIG_HIGHPRI |
                 SP_WSTATUS_CLEAR_SIG_MORE;

    MEMORY_BARRIER();

    // Off we go!
    rsp_run_async();

    // Disable INTR_ON_BREAK as that it is not useful in the RSPQ engine, and
    // might even cause excessive interrupts.
    *SP_STATUS = SP_WSTATUS_CLEAR_INTR_BREAK;
}

static void rspq_init_context(rspq_ctx_t *ctx, int buf_size)
{
    ctx->buffers[0] = malloc_uncached(buf_size * sizeof(uint32_t));
    ctx->buffers[1] = malloc_uncached(buf_size * sizeof(uint32_t));
    memset(ctx->buffers[0], 0, buf_size * sizeof(uint32_t));
    memset(ctx->buffers[1], 0, buf_size * sizeof(uint32_t));
    ctx->buf_idx = 0;
    ctx->buf_size = buf_size;
    ctx->cur = ctx->buffers[0];
    ctx->sentinel = ctx->cur + buf_size - RSPQ_MAX_COMMAND_SIZE;
}

void rspq_init(void)
{
    // Do nothing if rspq_init has already been called
    if (rspq_overlay_count > 0) 
        return;

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

    // Load initial settings
    memset(rspq_data_ptr, 0, sizeof(rsp_queue_t));
    rspq_data_ptr->rspq_dram_lowpri_addr = PhysicalAddr(lowpri.cur);
    rspq_data_ptr->rspq_dram_highpri_addr = PhysicalAddr(highpri.cur);
    rspq_data_ptr->rspq_dram_addr = rspq_data_ptr->rspq_dram_lowpri_addr;
    rspq_data_ptr->tables.overlay_descriptors[0].data_buf = PhysicalAddr(&dummy_overlay_state);
    rspq_data_ptr->tables.overlay_descriptors[0].data_size = sizeof(uint64_t);
    rspq_data_ptr->current_ovl = 0;
    rspq_data_ptr->primode_status_check = SP_STATUS_SIG_HIGHPRI;
    rspq_overlay_count = 1;
    
    // Init syncpoints
    rspq_syncpoints_genid = 0;
    rspq_syncpoints_done = 0;

    // Init blocks
    rspq_block = NULL;
    rspq_is_running = false;

    // Register asserts
    rsp_ucode_register_assert(&rsp_queue, ASSERT_INVALID_OVERLAY, "Invalid overlay", NULL);
    rsp_ucode_register_assert(&rsp_queue, ASSERT_INVALID_COMMAND, "Invalid command", NULL);
    rsp_ucode_register_assert(&rsp_queue, ASSERT_GP_BACKWARD, "GP moved backward", NULL);

    // Activate SP interrupt (used for syncpoints)
    register_SP_handler(rspq_sp_interrupt);
    set_SP_interrupt(1);

    rspq_start();
}

void rspq_stop()
{
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    MEMORY_BARRIER();

    rspq_is_running = 0;
}

void rspq_close()
{
    rspq_stop();
    
    rspq_overlay_count = 0;

    set_SP_interrupt(0);
    unregister_SP_handler(rspq_sp_interrupt);
}

void* rspq_overlay_get_state(rsp_ucode_t *overlay_ucode)
{
    rspq_overlay_header_t *overlay_header = (rspq_overlay_header_t*)overlay_ucode->data;
    return overlay_ucode->data + (overlay_header->state_start & 0xFFF) - RSPQ_OVL_DATA_ADDR;
}

void rspq_overlay_register(rsp_ucode_t *overlay_ucode, uint8_t id)
{
    assertf(rspq_overlay_count > 0, "rspq_overlay_register must be called after rspq_init!");
    assert(overlay_ucode);
    assertf(id < RSPQ_OVERLAY_TABLE_SIZE, "Tried to register id: %d", id);

    // The RSPQ ucode is always linked into overlays for now, so we need to load the overlay from an offset.
    uint32_t rspq_ucode_size = rsp_queue_text_end - rsp_queue_text_start;

    assertf(memcmp(rsp_queue_text_start, overlay_ucode->code, rspq_ucode_size) == 0, "Common code of overlay does not match!");

    uint32_t overlay_code = PhysicalAddr(overlay_ucode->code + rspq_ucode_size);

    uint8_t overlay_index = 0;

    // Check if the overlay has been registered already
    for (uint32_t i = 1; i < rspq_overlay_count; i++)
    {
        if (rspq_data_ptr->tables.overlay_descriptors[i].code == overlay_code)
        {
            overlay_index = i;
            break;
        }
    }

    // If the overlay has not been registered before, add it to the descriptor table first
    if (overlay_index == 0)
    {
        assertf(rspq_overlay_count < RSPQ_MAX_OVERLAY_COUNT, "Only up to %d overlays are supported!", RSPQ_MAX_OVERLAY_COUNT);

        overlay_index = rspq_overlay_count++;

        rspq_overlay_t *overlay = &rspq_data_ptr->tables.overlay_descriptors[overlay_index];
        overlay->code = overlay_code;
        overlay->data = PhysicalAddr(overlay_ucode->data);
        overlay->data_buf = PhysicalAddr(rspq_overlay_get_state(overlay_ucode));
        overlay->code_size = ((uint8_t*)overlay_ucode->code_end - overlay_ucode->code) - rspq_ucode_size - 1;
        overlay->data_size = ((uint8_t*)overlay_ucode->data_end - overlay_ucode->data) - 1;
    }

    // Let the specified id point at the overlay
    rspq_data_ptr->tables.overlay_table[id] = overlay_index * sizeof(rspq_overlay_t);

    // Issue a DMA request to update the overlay tables in DMEM.
    // Note that we don't use rsp_load_data() here and instead use the dma command,
    // so we don't need to synchronize with the RSP. All commands queued after this
    // point will be able to use the newly registered overlay.
    rspq_dma_to_dmem(0, &rspq_data_ptr->tables, sizeof(rspq_overlay_tables_t), false);
}

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

    // Wait until the previous buffer is executed by the RSP.
    // We cannot write to it if it's still being executed.
    // FIXME: this should probably transition to a sync-point,
    // so that the kernel can switch away while waiting. Even
    // if the overhead of an interrupt is obviously higher.
    MEMORY_BARRIER();
    if (!(*SP_STATUS & rspq_ctx->sp_status_bufdone)) {
        rspq_flush_internal();
        RSP_WAIT_LOOP(200) {
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

    MEMORY_BARRIER();
    // Kick the RSP, in case it's sleeping.
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();
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
}

void rspq_highpri_begin(void)
{
    assertf(rspq_ctx != &highpri, "already in highpri mode");
    assertf(!rspq_block, "cannot switch to highpri mode while creating a block");

    rspq_switch_context(&highpri);

    // If we're continuing on the same buffer another highpri sequence,
    // try to skip the highpri epilog and jump to the buffer continuation.
    // This is a small performance gain (the RSP doesn't need to exit and re-enter
    // the highpri mode) but it also allows to enqueue more than one highpri
    // sequence, since we only have a single SIG_HIGHPRI and there would be no
    // way to tell the RSP "there are 3 sequences pending, so exit and re-enter
    // three times".
    // 
    // To skip the epilog we write a single atomic word at the start of the
    // epilog, changing it with a JUMP to the buffer continuation. This operation
    // is completely safe because the RSP either see the memory before the
    // change (it sees the epilog) or after the change (it sees the new JUMP).
    // 
    // In the first case, it will run the epilog and then reenter the highpri
    // mode soon (as we're turning on SIG_HIGHPRI anyway). In the second case,
    // it's going to see the JUMP, skip the epilog and continue. The SIG_HIGHPRI
    // bit will be set but this function, and reset at the beginning of the new
    // segment, but it doesn't matter at this point.
    if (rspq_cur_pointer[-3]>>24 == RSPQ_CMD_SWAP_BUFFERS) {
        volatile uint32_t *epilog = rspq_cur_pointer-4;
        rspq_append1(epilog, RSPQ_CMD_JUMP, PhysicalAddr(rspq_cur_pointer));
    }

    rspq_append1(rspq_cur_pointer, RSPQ_CMD_WRITE_STATUS, SP_WSTATUS_CLEAR_SIG_HIGHPRI | SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING);
    MEMORY_BARRIER();
 
    *SP_STATUS = SP_WSTATUS_SET_SIG_HIGHPRI;
    rspq_flush_internal();
}

void rspq_highpri_end(void)
{
    assertf(rspq_ctx == &highpri, "not in highpri mode");

    // Write the highpri epilog. The epilog starts with a JUMP to the next
    // instruction because we want to force the RSP to reload the buffer
    // from RDRAM in case the epilog has been overwritten by a new highpri
    // queue (see rsqp_highpri_begin).
    rspq_append1(rspq_cur_pointer, RSPQ_CMD_JUMP, PhysicalAddr(rspq_cur_pointer+1));
    rspq_append1(rspq_cur_pointer, RSPQ_CMD_WRITE_STATUS, SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING);
    rspq_append3(rspq_cur_pointer, RSPQ_CMD_SWAP_BUFFERS, RSPQ_LOWPRI_CALL_SLOT<<2, RSPQ_HIGHPRI_CALL_SLOT<<2, SP_STATUS_SIG_HIGHPRI);
    rspq_flush_internal();
    rspq_switch_context(&lowpri);
}

void rspq_highpri_sync(void)
{
    assertf(rspq_ctx != &highpri, "this function can only be called outside of highpri mode");

    RSP_WAIT_LOOP(200) {
        if (!(*SP_STATUS & (SP_STATUS_SIG_HIGHPRI | SP_STATUS_SIG_HIGHPRI_RUNNING)))
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

    // Switch to the block buffer. From now on, all rspq_writes will
    // go into the block.
    rspq_switch_context(NULL);
    rspq_switch_buffer(rspq_block->cmds, rspq_block_size, true);
}

rspq_block_t* rspq_block_end(void)
{
    assertf(rspq_block, "a block was not being created");

    // Terminate the block with a RET command, encoding
    // the nesting level which is used as stack slot by RSP.
    rspq_append1(rspq_cur_pointer, RSPQ_CMD_RET, rspq_block->nesting_level<<2);

    // Switch back to the normal display list
    rspq_switch_context(&lowpri);

    // Return the created block
    rspq_block_t *b = rspq_block;
    rspq_block = NULL;
    return b;
}

void rspq_block_free(rspq_block_t *block)
{
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
            free(CachedAddr(start));
            // Get the pointer to the next chunk
            start = UncachedAddr(0x80000000 | (cmd & 0xFFFFFF));
            if (size < RSPQ_BLOCK_MAX_SIZE) size *= 2;
            ptr = (uint32_t*)start + size;
            continue;
        }
        // If the last command is a RET
        if (cmd>>24 == RSPQ_CMD_RET) {
            // This is the last chunk, free it and exit
            free(CachedAddr(start));
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

    // Write the CALL op. The second argument is the nesting level
    // which is used as stack slot in the RSP to save the current
    // pointer position.
    rspq_write(RSPQ_CMD_CALL, PhysicalAddr(block->cmds), block->nesting_level << 2);

    // If this is CALL within the creation of a block, update
    // the nesting level. A block's nesting level must be bigger
    // than the nesting level of all blocks called from it.
    if (rspq_block && rspq_block->nesting_level <= block->nesting_level) {
        rspq_block->nesting_level = block->nesting_level + 1;
        assertf(rspq_block->nesting_level < RSPQ_MAX_BLOCK_NESTING_LEVEL,
            "reached maximum number of nested block runs");
    }
}


void rspq_queue_u32(uint32_t cmd)
{
    rspq_write(cmd>>24, cmd & 0x00FFFFFF);
}

void rspq_queue_u64(uint64_t cmd)
{
    rspq_write(cmd>>56, (cmd >> 32) & 0x00FFFFFF, cmd & 0xFFFFFFFF);
}

void rspq_noop()
{
    rspq_write(RSPQ_CMD_NOOP);
}

rspq_syncpoint_t rspq_syncpoint(void)
{   
    assertf(!rspq_block, "cannot create syncpoint in a block");
    rspq_write(RSPQ_CMD_TEST_WRITE_STATUS, 
        SP_WSTATUS_SET_INTR | SP_WSTATUS_SET_SIG_SYNCPOINT,
        SP_STATUS_SIG_SYNCPOINT);
    return ++rspq_syncpoints_genid;
}

bool rspq_check_syncpoint(rspq_syncpoint_t sync_id) 
{
    return sync_id <= rspq_syncpoints_done;
}

void rspq_wait_syncpoint(rspq_syncpoint_t sync_id)
{
    if (rspq_check_syncpoint(sync_id))
        return;

    assertf(get_interrupts_state() == INTERRUPTS_ENABLED,
        "deadlock: interrupts are disabled");

    // Make sure the RSP is running, otherwise we might be blocking forever.
    rspq_flush_internal();

    // Spinwait until the the syncpoint is reached.
    // TODO: with the kernel, it will be possible to wait for the RSP interrupt
    // to happen, without spinwaiting.
    RSP_WAIT_LOOP(200) {
        if (rspq_check_syncpoint(sync_id))
            break;
    }
}

void rspq_signal(uint32_t signal)
{
    const uint32_t allowed_mask = SP_WSTATUS_CLEAR_SIG0|SP_WSTATUS_SET_SIG0|SP_WSTATUS_CLEAR_SIG1|SP_WSTATUS_SET_SIG1;
    assertf((signal & allowed_mask) == signal, "rspq_signal called with a mask that contains bits outside SIG0-1: %lx", signal);

    rspq_write(RSPQ_CMD_WRITE_STATUS, signal);
}

static void rspq_dma(void *rdram_addr, uint32_t dmem_addr, uint32_t len, uint32_t flags)
{
    rspq_write(RSPQ_CMD_DMA, PhysicalAddr(rdram_addr), dmem_addr, len, flags);
}

void rspq_dma_to_rdram(void *rdram_addr, uint32_t dmem_addr, uint32_t len, bool is_async)
{
    rspq_dma(rdram_addr, dmem_addr, len - 1, 0xFFFF8000 | (is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL));
}

void rspq_dma_to_dmem(uint32_t dmem_addr, void *rdram_addr, uint32_t len, bool is_async)
{
    rspq_dma(rdram_addr, dmem_addr, len - 1, is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL);
}
