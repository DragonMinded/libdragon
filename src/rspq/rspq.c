/**
 * @file rspq.c
 * @brief RSP Command queue
 * @ingroup rsp
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

#define RSPQ_CMD_IDLE              0x01
#define RSPQ_CMD_SET_STATUS        0x02
#define RSPQ_CMD_CALL              0x03
#define RSPQ_CMD_JUMP              0x04
#define RSPQ_CMD_RET               0x05
#define RSPQ_CMD_SWAP_BUFFERS      0x06
#define RSPQ_CMD_NOOP              0x07
#define RSPQ_CMD_TAS_STATUS        0x08
#define RSPQ_CMD_DMA               0x09

#define rspq_terminator(rspq)   ({ \
    /* The terminator is usually meant to be written only *after* the last \
       command has been fully written, otherwise the RSP could in theory \
       execute a partial command. Force ordering via a memory barrier. */ \
    MEMORY_BARRIER(); \
    *(uint8_t*)(rspq) = 0x01; \
})

__attribute__((noreturn))
static void rsp_crash(const char *file, int line, const char *func);
#define RSP_WAIT_LOOP() \
    for (uint32_t __t = TICKS_READ() + TICKS_FROM_MS(50); \
         TICKS_BEFORE(TICKS_READ(), __t) || (rsp_crash(__FILE__,__LINE__,__func__),false); )

DEFINE_RSP_UCODE(rsp_queue);

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
    uint32_t *cur;
    uint32_t *sentinel;
} rspq_ctx_t;

static rsp_queue_t rspq_data;
#define rspq_data_ptr ((rsp_queue_t*)UncachedAddr(&rspq_data))

static uint8_t rspq_overlay_count = 0;

static rspq_block_t *rspq_block;
static int rspq_block_size;

rspq_ctx_t *rspq_ctx;
uint32_t *rspq_cur_pointer;
uint32_t *rspq_cur_sentinel;

rspq_ctx_t lowpri, highpri;

static int rspq_syncpoints_genid;
volatile int rspq_syncpoints_done;

static bool rspq_is_running;
static bool rspq_is_highpri;

static uint64_t dummy_overlay_state;

static void rspq_flush_internal(void);

static void rspq_sp_interrupt(void) 
{
    uint32_t status = *SP_STATUS;
    uint32_t wstatus = 0;

    if (status & SP_STATUS_SIG_SYNCPOINT) {
        wstatus |= SP_WSTATUS_CLEAR_SIG_SYNCPOINT;
        ++rspq_syncpoints_done;
        debugf("syncpoint intr %d\n", rspq_syncpoints_done);
    }

    MEMORY_BARRIER();

    *SP_STATUS = wstatus;
}

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

static uint32_t* rspq_switch_buffer(uint32_t *new, int size, bool clear)
{
    uint32_t* prev = rspq_cur_pointer;

    // Add a terminator so that it's a valid buffer.
    // Notice that the buffer must have been cleared before, as the
    // command queue are expected to always contain 0 on unwritten data.
    // We don't do this for performance reasons.
    assert(size >= RSPQ_MAX_COMMAND_SIZE);
    if (clear) memset(new, 0, size * sizeof(uint32_t));
    rspq_terminator(new);

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
}

static void rspq_init_context(rspq_ctx_t *ctx, int buf_size)
{
    ctx->buffers[0] = malloc_uncached(buf_size * sizeof(uint32_t));
    ctx->buffers[1] = malloc_uncached(buf_size * sizeof(uint32_t));
    memset(ctx->buffers[0], 0, buf_size * sizeof(uint32_t));
    memset(ctx->buffers[1], 0, buf_size * sizeof(uint32_t));
    rspq_terminator(ctx->buffers[0]);
    rspq_terminator(ctx->buffers[1]);
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
        uint32_t *prev = rspq_switch_buffer(rspq2, rspq_block_size, true);

        // Terminate the previous chunk with a JUMP op to the new chunk.
        *prev++ = (RSPQ_CMD_JUMP<<24) | PhysicalAddr(rspq2);
        rspq_terminator(prev);
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
        RSP_WAIT_LOOP() {
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
    uint32_t *prev = rspq_switch_buffer(new, rspq_ctx->buf_size, true);

    // Terminate the previous buffer with an op to set SIG_BUFDONE
    // (to notify when the RSP finishes the buffer), plus a jump to
    // the new buffer.
    *prev++ = (RSPQ_CMD_SET_STATUS<<24) | rspq_ctx->sp_wstatus_set_bufdone;
    *prev++ = (RSPQ_CMD_JUMP<<24) | PhysicalAddr(new);
    rspq_terminator(prev);

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

#if 1
__attribute__((noreturn))
static void rsp_crash(const char *file, int line, const char *func)
{
    uint32_t status = *SP_STATUS;
    MEMORY_BARRIER();

    console_init();
    console_set_debug(true);
    console_set_render_mode(RENDER_MANUAL);

    printf("RSP CRASH @ %s (%s:%d)\n", func, file, line);

    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    while (!(*SP_STATUS & SP_STATUS_HALTED)) {}
    while (*SP_STATUS & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL)) {}
    MEMORY_BARRIER();
    uint32_t pc = *SP_PC;  // can only read after halt
    MEMORY_BARRIER();

    printf("PC:%03lx   STATUS:%04lx | ", pc, status);
    if (status & (1<<0)) printf("halt ");
    if (status & (1<<1)) printf("broke ");
    if (status & (1<<2)) printf("dma_busy ");
    if (status & (1<<3)) printf("dma_full ");
    if (status & (1<<4)) printf("io_full ");
    if (status & (1<<5)) printf("single_step ");
    if (status & (1<<6)) printf("irq_on_break ");
    if (status & (1<<7)) printf("sig0 ");
    if (status & (1<<8)) printf("sig1 ");
    if (status & (1<<9)) printf("sig2 ");
    if (status & (1<<10)) printf("sig3 ");
    if (status & (1<<11)) printf("sig4 ");
    if (status & (1<<12)) printf("sig5 ");
    if (status & (1<<13)) printf("sig6 ");
    if (status & (1<<14)) printf("sig7 ");
    printf("\n");

    printf("COP0 registers:\n");
    printf("-----------------------------------------\n");
    printf("$c0  | COP0_DMA_SPADDR   | %08lx\n", *((volatile uint32_t*)0xA4040000));
    printf("$c1  | COP0_DMA_RAMADDR  | %08lx\n", *((volatile uint32_t*)0xA4040004));
    printf("$c2  | COP0_DMA_READ     | %08lx\n", *((volatile uint32_t*)0xA4040008));
    printf("$c3  | COP0_DMA_WRITE    | %08lx\n", *((volatile uint32_t*)0xA404000C));
    printf("$c4  | COP0_SP_STATUS    | %08lx\n", *((volatile uint32_t*)0xA4040010));
    printf("$c5  | COP0_DMA_FULL     | %08lx\n", *((volatile uint32_t*)0xA4040014));
    printf("$c6  | COP0_DMA_BUSY     | %08lx\n", *((volatile uint32_t*)0xA4040018));
    printf("$c7  | COP0_SEMAPHORE    | %08lx\n", *((volatile uint32_t*)0xA404001C));
    printf("-----------------------------------------\n");
    printf("$c8  | COP0_DP_START     | %08lx\n", *((volatile uint32_t*)0xA4100000));
    printf("$c9  | COP0_DP_END       | %08lx\n", *((volatile uint32_t*)0xA4100004));
    printf("$c10 | COP0_DP_CURRENT   | %08lx\n", *((volatile uint32_t*)0xA4100008));
    printf("$c11 | COP0_DP_STATUS    | %08lx\n", *((volatile uint32_t*)0xA410000C));
    printf("$c12 | COP0_DP_CLOCK     | %08lx\n", *((volatile uint32_t*)0xA4100010));
    printf("$c13 | COP0_DP_BUSY      | %08lx\n", *((volatile uint32_t*)0xA4100014));
    printf("$c14 | COP0_DP_PIPE_BUSY | %08lx\n", *((volatile uint32_t*)0xA4100018));
    printf("$c15 | COP0_DP_TMEM_BUSY | %08lx\n", *((volatile uint32_t*)0xA410001C));
    printf("-----------------------------------------\n");

    rsp_queue_t *rspq = (rsp_queue_t*)SP_DMEM;
    printf("RSPQ: Normal  DRAM address: %08lx\n", rspq->rspq_dram_lowpri_addr);
    printf("RSPQ: Highpri DRAM address: %08lx\n", rspq->rspq_dram_highpri_addr);
    printf("RSPQ: Current DRAM address: %08lx\n", rspq->rspq_dram_addr);
    printf("RSPQ: Overlay: %x\n", rspq->current_ovl);
    debugf("RSPQ: Command queue:\n");
    for (int j=0;j<4;j++) {        
        for (int i=0;i<16;i++)
            debugf("%08lx ", SP_DMEM[0xF8+i+j*16]);
        debugf("\n");
    }

    console_render();
    abort();
}

#endif

void rspq_highpri_begin(void)
{
    assertf(!rspq_is_highpri, "already in highpri mode");
    assertf(!rspq_block, "cannot switch to highpri mode while creating a block");

    rspq_switch_context(&highpri);

    // If we're continuing on the same buffer another highpri sequence,
    // try to erase the highpri epilog. This allows to enqueue more than one
    // highpri sequence, because otherwise the SIG_HIGHPRI would get turn off
    // in the first, and then never turned on back again.
    // 
    // Notice that there is tricky timing here. The epilog starts with a jump
    // instruction so that it is refetched via DMA just before being executed.
    // There are three cases:
    //   * We manage to clear the epilog before it is refetched and run. The
    //     RSP will find the epilog fully NOP-ed, and will transition to next
    //     highpri queue.
    //   * We do not manage to clear the epilog before it is refetched. The
    //     RSP will execute the epilog and switch back to LOWPRI. But we're going
    //     to set SIG_HIGHPRI on soon, and so it will switch again to HIGHPRI.
    //   * We clear the epilog while the RSP is fetching it. The RSP will see
    //     the epilog half-cleared. Since we're forcing a strict left-to-right
    //     zeroing with memory barriers, the RSP will either see zeroes followed
    //     by a partial epilog, or a few NOPs followed by some zeroes. In either
    //     case, the zeros will force the RSP to fetch it again, and the second
    //     time will see the fully NOP'd epilog and continue to next highpri.
    if (rspq_cur_pointer[0]>>24 == RSPQ_CMD_IDLE && rspq_cur_pointer[-3]>>24 == RSPQ_CMD_SWAP_BUFFERS) {
        uint32_t *cur = rspq_cur_pointer;
        cur[-5] = 0; MEMORY_BARRIER();
        cur[-4] = 0; MEMORY_BARRIER();
        cur[-3] = 0; MEMORY_BARRIER();
        cur[-2] = 0; MEMORY_BARRIER();
        cur[-1] = 0; MEMORY_BARRIER();
        cur[-5] = RSPQ_CMD_NOOP<<24; MEMORY_BARRIER();
        cur[-4] = RSPQ_CMD_NOOP<<24; MEMORY_BARRIER();
        cur[-3] = RSPQ_CMD_NOOP<<24; MEMORY_BARRIER();
        cur[-2] = RSPQ_CMD_NOOP<<24; MEMORY_BARRIER();
        cur[-1] = RSPQ_CMD_NOOP<<24; MEMORY_BARRIER();
    }

    *rspq_cur_pointer++ = (RSPQ_CMD_SET_STATUS<<24) | SP_WSTATUS_CLEAR_SIG_HIGHPRI | SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING;
    rspq_terminator(rspq_cur_pointer);
 
    *SP_STATUS = SP_WSTATUS_SET_SIG_HIGHPRI;
    rspq_is_highpri = true;
    rspq_flush_internal();
}

void rspq_highpri_end(void)
{
    assertf(rspq_is_highpri, "not in highpri mode");

    // Write the highpri epilog.
    // The queue currently contains a RSPQ_CMD_IDLE (terminator) followed by a 0
    // (standard termination sequence). We want to write the epilog atomically
    // with respect to RSP: we need to avoid the RSP to see a partially written
    // epilog, which would force it to refetch it and possibly create a race
    // condition with a new highpri sequence.
    // So we leave the IDLE+0 where they are, write the epilog just after it,
    // and finally write a JUMP to it. The JUMP is required so that the RSP
    // always refetch the epilog when it gets to it (see #rspq_highpri_begin).
    uint32_t *end = rspq_cur_pointer;

    rspq_cur_pointer += 2;
    *rspq_cur_pointer++ = (RSPQ_CMD_SET_STATUS<<24) | SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING;
    *rspq_cur_pointer++ = (RSPQ_CMD_SWAP_BUFFERS<<24) | (RSPQ_LOWPRI_CALL_SLOT<<2);
      *rspq_cur_pointer++ = RSPQ_HIGHPRI_CALL_SLOT<<2;
      *rspq_cur_pointer++ = SP_STATUS_SIG_HIGHPRI;
    rspq_terminator(rspq_cur_pointer);

    MEMORY_BARRIER();

    *end = (RSPQ_CMD_JUMP<<24) | PhysicalAddr(end+2);
    rspq_terminator(end+1);

    rspq_flush_internal();

    rspq_switch_context(&lowpri);
    rspq_is_highpri = false;
}

void rspq_highpri_sync(void)
{
    assertf(!rspq_is_highpri, "this function can only be called outside of highpri mode");

    RSP_WAIT_LOOP() {
        if (!(*SP_STATUS & (SP_STATUS_SIG_HIGHPRI | SP_STATUS_SIG_HIGHPRI_RUNNING)))
            break;
    }
}

void rspq_block_begin(void)
{
    assertf(!rspq_block, "a block was already being created");
    assertf(!rspq_is_highpri, "cannot create a block in highpri mode");

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
    *rspq_cur_pointer++ = (RSPQ_CMD_RET<<24) | (rspq_block->nesting_level<<2);
    rspq_terminator(rspq_cur_pointer);

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

        // Ignore the terminator
        if (cmd>>24 == RSPQ_CMD_IDLE)
            cmd = *--ptr;

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
    assertf(!rspq_is_highpri, "block run is not supported in highpri mode");

    // Write the CALL op. The second argument is the nesting level
    // which is used as stack slot in the RSP to save the current
    // pointer position.
    uint32_t *rspq = rspq_write_begin();
    *rspq++ = (RSPQ_CMD_CALL<<24) | PhysicalAddr(block->cmds);
    *rspq++ = block->nesting_level << 2;
    rspq_write_end(rspq);

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
    uint32_t *rspq = rspq_write_begin();
    *rspq++ = cmd;
    rspq_write_end(rspq);
}

void rspq_queue_u64(uint64_t cmd)
{
    uint32_t *rspq = rspq_write_begin();
    *rspq++ = cmd >> 32;
    *rspq++ = cmd & 0xFFFFFFFF;
    rspq_write_end(rspq);
}

void rspq_noop()
{
    rspq_queue_u32(RSPQ_CMD_NOOP << 24);
}

rspq_syncpoint_t rspq_syncpoint(void)
{   
    assertf(!rspq_block, "cannot create syncpoint in a block");
    uint32_t *rspq = rspq_write_begin();
    *rspq++ = ((RSPQ_CMD_TAS_STATUS << 24) | SP_WSTATUS_SET_INTR | SP_WSTATUS_SET_SIG_SYNCPOINT);
    *rspq++ = SP_STATUS_SIG_SYNCPOINT;
    rspq_write_end(rspq);
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
    RSP_WAIT_LOOP() {
        if (rspq_check_syncpoint(sync_id))
            break;
    }
}

void rspq_signal(uint32_t signal)
{
    const uint32_t allows_mask = SP_WSTATUS_CLEAR_SIG0|SP_WSTATUS_SET_SIG0|SP_WSTATUS_CLEAR_SIG1|SP_WSTATUS_SET_SIG1;
    assertf((signal & allows_mask) == signal, "rspq_signal called with a mask that contains bits outside SIG0-1: %lx", signal);

    rspq_queue_u32((RSPQ_CMD_SET_STATUS << 24) | signal);
}

static void rspq_dma(void *rdram_addr, uint32_t dmem_addr, uint32_t len, uint32_t flags)
{
    uint32_t *rspq = rspq_write_begin();
    *rspq++ = (RSPQ_CMD_DMA << 24) | PhysicalAddr(rdram_addr);
    *rspq++ = dmem_addr;
    *rspq++ = len;
    *rspq++ = flags;
    rspq_write_end(rspq);
}

void rspq_dma_to_rdram(void *rdram_addr, uint32_t dmem_addr, uint32_t len, bool is_async)
{
    rspq_dma(rdram_addr, dmem_addr, len - 1, 0xFFFF8000 | (is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL));
}

void rspq_dma_to_dmem(uint32_t dmem_addr, void *rdram_addr, uint32_t len, bool is_async)
{
    rspq_dma(rdram_addr, dmem_addr, len - 1, is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL);
}
