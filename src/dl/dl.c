#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>
#include <malloc.h>
#include "dl_internal.h"
#include "utils.h"
#include "../../build/dl/dl_symbols.h"

#define DL_CMD_IDLE              0x01
#define DL_CMD_SET_STATUS        0x02
#define DL_CMD_CALL              0x03
#define DL_CMD_JUMP              0x04
#define DL_CMD_RET               0x05
#define DL_CMD_NOOP              0x07
#define DL_CMD_TAS_STATUS        0x08
#define DL_CMD_DMA               0x09

#define dl_terminator(dl)   ({ \
    /* The terminator is usually meant to be written only *after* the last \
       command has been fully written, otherwise the RSP could in theory \
       execute a partial command. Force ordering via a memory barrier. */ \
    MEMORY_BARRIER(); \
    *(uint8_t*)(dl) = 0x01; \
})

#define SP_STATUS_SIG_HIGHPRI_RUNNING         SP_STATUS_SIG3
#define SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING    SP_WSTATUS_SET_SIG3
#define SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING  SP_WSTATUS_CLEAR_SIG3

#define SP_STATUS_SIG_SYNCPOINT               SP_STATUS_SIG4
#define SP_WSTATUS_SET_SIG_SYNCPOINT          SP_WSTATUS_SET_SIG4
#define SP_WSTATUS_CLEAR_SIG_SYNCPOINT        SP_WSTATUS_CLEAR_SIG4

#define SP_STATUS_SIG_BUFDONE                 SP_STATUS_SIG5
#define SP_WSTATUS_SET_SIG_BUFDONE            SP_WSTATUS_SET_SIG5
#define SP_WSTATUS_CLEAR_SIG_BUFDONE          SP_WSTATUS_CLEAR_SIG5

#define SP_STATUS_SIG_HIGHPRI                 SP_STATUS_SIG6
#define SP_WSTATUS_SET_SIG_HIGHPRI            SP_WSTATUS_SET_SIG6
#define SP_WSTATUS_CLEAR_SIG_HIGHPRI          SP_WSTATUS_CLEAR_SIG6

#define SP_STATUS_SIG_MORE                    SP_STATUS_SIG7
#define SP_WSTATUS_SET_SIG_MORE               SP_WSTATUS_SET_SIG7
#define SP_WSTATUS_CLEAR_SIG_MORE             SP_WSTATUS_CLEAR_SIG7

DEFINE_RSP_UCODE(rsp_dl);

typedef struct dl_overlay_t {
    uint32_t code;
    uint32_t data;
    uint32_t data_buf;
    uint16_t code_size;
    uint16_t data_size;
} dl_overlay_t;

typedef struct dl_overlay_header_t {
    uint32_t state_start;
    uint16_t state_size;
    uint16_t command_base;
} dl_overlay_header_t;

typedef struct dl_block_s {
    uint32_t nesting_level;
    uint32_t cmds[];
} dl_block_t;

typedef struct dl_overlay_tables_s {
    uint8_t overlay_table[DL_OVERLAY_TABLE_SIZE];
    dl_overlay_t overlay_descriptors[DL_MAX_OVERLAY_COUNT];
} dl_overlay_tables_t;

typedef struct rsp_dl_s {
    dl_overlay_tables_t tables;
    uint32_t dl_dram_addr;
    uint32_t dl_dram_highpri_addr;
    int16_t current_ovl;
} __attribute__((aligned(16), packed)) rsp_dl_t;

static rsp_dl_t dl_data;
#define dl_data_ptr ((rsp_dl_t*)UncachedAddr(&dl_data))

static uint8_t dl_overlay_count = 0;

/** @brief Command list buffers (full cachelines to avoid false sharing) */
static uint32_t dl_buffers[2][DL_DRAM_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t dl_buf_idx;
static dl_block_t *dl_block;
static int dl_block_size;

uint32_t *dl_cur_pointer;
uint32_t *dl_cur_sentinel;

static uint32_t *dl_old_pointer, *dl_old_sentinel;

static int dl_syncpoints_genid;
volatile int dl_syncpoints_done;

static bool dl_is_running;
static bool dl_is_highpri;

static uint64_t dummy_overlay_state;

static void __dl_highpri_init(void);

static void dl_sp_interrupt(void) 
{
    uint32_t status = *SP_STATUS;
    uint32_t wstatus = 0;

    if (status & SP_STATUS_SIG_SYNCPOINT) {
        wstatus |= SP_WSTATUS_CLEAR_SIG_SYNCPOINT;
        ++dl_syncpoints_done;
        debugf("syncpoint intr %d\n", dl_syncpoints_done);
    }
#if 0
    // Check if we just finished a highpri list
    if (status & SP_STATUS_SIG_HIGHPRI_FINISHED) {
        // Clear the HIGHPRI_FINISHED signal
        wstatus |= SP_WSTATUS_CLEAR_SIG_HIGHPRI_FINISHED;

        // If there are still highpri buffers pending, schedule them right away
        if (++dl_highpri_ridx < dl_highpri_widx)
            wstatus |= SP_WSTATUS_SET_SIG_HIGHPRI;
    }
#endif
    MEMORY_BARRIER();

    *SP_STATUS = wstatus;
}

void dl_start()
{
    if (dl_is_running)
    {
        return;
    }

    rsp_wait();
    rsp_load(&rsp_dl);

    // Load data with initialized overlays into DMEM
    rsp_load_data(dl_data_ptr, sizeof(rsp_dl_t), 0);

    static dl_overlay_header_t dummy_header = (dl_overlay_header_t){
        .state_start = 0,
        .state_size = 7,
        .command_base = 0
    };

    rsp_load_data(&dummy_header, sizeof(dummy_header), DL_OVL_DATA_ADDR);

    MEMORY_BARRIER();

    *SP_STATUS = SP_WSTATUS_CLEAR_SIG0 | 
                 SP_WSTATUS_CLEAR_SIG1 | 
                 SP_WSTATUS_CLEAR_SIG2 | 
                 SP_WSTATUS_CLEAR_SIG3 | 
                 SP_WSTATUS_CLEAR_SIG4 | 
                 SP_WSTATUS_SET_SIG_BUFDONE |
                 SP_WSTATUS_CLEAR_SIG_HIGHPRI |
                 SP_WSTATUS_CLEAR_SIG_MORE;

    MEMORY_BARRIER();

    // Off we go!
    rsp_run_async();
}

void dl_init()
{
    // Do nothing if dl_init has already been called
    if (dl_overlay_count > 0) 
    {
        return;
    }

    // Load initial settings
    memset(dl_data_ptr, 0, sizeof(rsp_dl_t));

    dl_cur_pointer = UncachedAddr(dl_buffers[0]);
    dl_cur_sentinel = dl_cur_pointer + DL_DRAM_BUFFER_SIZE - DL_MAX_COMMAND_SIZE;
    memset(dl_cur_pointer, 0, DL_DRAM_BUFFER_SIZE*sizeof(uint32_t));
    dl_terminator(dl_cur_pointer);
    dl_block = NULL;

    dl_data_ptr->dl_dram_addr = PhysicalAddr(dl_buffers[0]);
    dl_data_ptr->tables.overlay_descriptors[0].data_buf = PhysicalAddr(&dummy_overlay_state);
    dl_data_ptr->tables.overlay_descriptors[0].data_size = sizeof(uint64_t);
    
    dl_syncpoints_genid = 0;
    dl_syncpoints_done = 0;

    dl_overlay_count = 1;
    dl_is_running = false;

    __dl_highpri_init();

    // Activate SP interrupt (used for syncpoints)
    register_SP_handler(dl_sp_interrupt);
    set_SP_interrupt(1);

    dl_start();
}

void dl_stop()
{
    dl_is_running = 0;
}

void dl_close()
{
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    MEMORY_BARRIER();

    dl_stop();
    
    dl_overlay_count = 0;

    set_SP_interrupt(0);
    unregister_SP_handler(dl_sp_interrupt);
}

void* dl_overlay_get_state(rsp_ucode_t *overlay_ucode)
{
    dl_overlay_header_t *overlay_header = (dl_overlay_header_t*)overlay_ucode->data;
    return overlay_ucode->data + (overlay_header->state_start & 0xFFF) - DL_OVL_DATA_ADDR;
}

void dl_overlay_register(rsp_ucode_t *overlay_ucode, uint8_t id)
{
    assertf(dl_overlay_count > 0, "dl_overlay_register must be called after dl_init!");
    assert(overlay_ucode);
    assertf(id < DL_OVERLAY_TABLE_SIZE, "Tried to register id: %d", id);

    // The DL ucode is always linked into overlays for now, so we need to load the overlay from an offset.
    uint32_t dl_ucode_size = rsp_dl_text_end - rsp_dl_text_start;

    assertf(memcmp(rsp_dl_text_start, overlay_ucode->code, dl_ucode_size) == 0, "Common code of overlay does not match!");

    uint32_t overlay_code = PhysicalAddr(overlay_ucode->code + dl_ucode_size);

    uint8_t overlay_index = 0;

    // Check if the overlay has been registered already
    for (uint32_t i = 1; i < dl_overlay_count; i++)
    {
        if (dl_data_ptr->tables.overlay_descriptors[i].code == overlay_code)
        {
            overlay_index = i;
            break;
        }
    }

    // If the overlay has not been registered before, add it to the descriptor table first
    if (overlay_index == 0)
    {
        assertf(dl_overlay_count < DL_MAX_OVERLAY_COUNT, "Only up to %d overlays are supported!", DL_MAX_OVERLAY_COUNT);

        overlay_index = dl_overlay_count++;

        dl_overlay_t *overlay = &dl_data_ptr->tables.overlay_descriptors[overlay_index];
        overlay->code = overlay_code;
        overlay->data = PhysicalAddr(overlay_ucode->data);
        overlay->data_buf = PhysicalAddr(dl_overlay_get_state(overlay_ucode));
        overlay->code_size = ((uint8_t*)overlay_ucode->code_end - overlay_ucode->code) - dl_ucode_size - 1;
        overlay->data_size = ((uint8_t*)overlay_ucode->data_end - overlay_ucode->data) - 1;
    }

    // Let the specified id point at the overlay
    dl_data_ptr->tables.overlay_table[id] = overlay_index * sizeof(dl_overlay_t);

    // Issue a DMA request to update the overlay tables in DMEM.
    // Note that we don't use rsp_load_data() here and instead use the dma command,
    // so we don't need to synchronize with the RSP. All commands queued after this
    // point will be able to use the newly registered overlay.
    dl_dma_to_dmem(0, &dl_data_ptr->tables, sizeof(dl_overlay_tables_t), false);
}

static uint32_t* dl_switch_buffer(uint32_t *dl2, int size, bool clear)
{
    uint32_t* prev = dl_cur_pointer;

    // Add a terminator so that it's a valid buffer.
    // Notice that the buffer must have been cleared before, as the
    // command queue are expected to always contain 0 on unwritten data.
    // We don't do this for performance reasons.
    assert(size >= DL_MAX_COMMAND_SIZE);
    if (clear) memset(dl2, 0, size * sizeof(uint32_t));
    dl_terminator(dl2);

    // Switch to the new buffer, and calculate the new sentinel.
    dl_cur_pointer = dl2;
    dl_cur_sentinel = dl_cur_pointer + size - DL_MAX_COMMAND_SIZE;

    // Return a pointer to the previous buffer
    return prev;
}

static void dl_push_buffer(void) 
{
    assertf(!dl_old_pointer, "internal error: dl_push_buffer called twice");
    dl_old_pointer = dl_cur_pointer;
    dl_old_sentinel = dl_cur_sentinel;
}

static void dl_pop_buffer(void) 
{
    assertf(dl_old_pointer, "internal error: dl_pop_buffer called without dl_push_buffer");
    dl_cur_pointer = dl_old_pointer;
    dl_cur_sentinel = dl_old_sentinel;
    dl_old_pointer = dl_old_sentinel = NULL;
}

/**
 * @brief Allocate a buffer that will be accessed as uncached memory.
 * 
 * @param[in]  size  The size of the buffer to allocate
 *
 * @return a pointer to the start of the buffer (as uncached pointer)
 */
void *malloc_uncached(size_t size)
{
    // Since we will be accessing the buffer as uncached memory, we absolutely
    // need to prevent part of it to ever enter the data cache, even as false
    // sharing with contiguous buffers. So we want the buffer to exclusively
    // cover full cachelines (aligned to 16 bytes, multiple of 16 bytes).
    size = ROUND_UP(size, 16);
    void *mem = memalign(16, size);

    // The memory returned by the system allocator could already be partly in
    // cache. Invalidate it so that we don't risk a writeback in the short future.
    data_cache_hit_invalidate(mem, size);

    // Return the pointer as uncached memory.
    return UncachedAddr(mem);
}

__attribute__((noinline))
void dl_next_buffer(void) {
    // If we are in highpri mode
    if (dl_is_highpri) {
        // The current highpri buffered is now full. The easiest thing to do
        // is to switch to the next one, simply by closing and reopening the
        // highpri mode.
        dl_highpri_end();
        dl_highpri_begin();
        return;
    }

    // If we're creating a block
    if (dl_block) {
        // Allocate next chunk (double the size of the current one).
        // We use doubling here to reduce overheads for large blocks
        // and at the same time start small.
        if (dl_block_size < DL_BLOCK_MAX_SIZE) dl_block_size *= 2;

        // Allocate a new chunk of the block and switch to it.
        uint32_t *dl2 = malloc_uncached(dl_block_size*sizeof(uint32_t));
        uint32_t *prev = dl_switch_buffer(dl2, dl_block_size, true);

        // Terminate the previous chunk with a JUMP op to the new chunk.
        *prev++ = (DL_CMD_JUMP<<24) | PhysicalAddr(dl2);
        dl_terminator(prev);
        return;
    }

    // Wait until the previous buffer is executed by the RSP.
    // We cannot write to it if it's still being executed.
    // FIXME: this should probably transition to a sync-point,
    // so that the kernel can switch away while waiting. Even
    // if the overhead of an interrupt is obviously higher.
    MEMORY_BARRIER();
    while (!(*SP_STATUS & SP_STATUS_SIG_BUFDONE)) { /* idle */ }
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_CLEAR_SIG_BUFDONE;
    MEMORY_BARRIER();

    // Switch current buffer
    dl_buf_idx = 1-dl_buf_idx;
    uint32_t *dl2 = UncachedAddr(&dl_buffers[dl_buf_idx]);
    uint32_t *prev = dl_switch_buffer(dl2, DL_DRAM_BUFFER_SIZE, true);

    // Terminate the previous buffer with an op to set SIG_BUFDONE
    // (to notify when the RSP finishes the buffer), plus a jump to
    // the new buffer.
    *prev++ = (DL_CMD_SET_STATUS<<24) | SP_WSTATUS_SET_SIG_BUFDONE;
    *prev++ = (DL_CMD_JUMP<<24) | PhysicalAddr(dl2);
    dl_terminator(prev);

    MEMORY_BARRIER();
    // Kick the RSP, in case it's sleeping.
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();
}

__attribute__((noinline))
void dl_flush_internal(void)
{
    // Tell the RSP to wake up because there is more data pending.
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();

    // Most of the times, the above is enough. But there is a small and very rare
    // race condition that can happen: if the above status change happens
    // exactly in the few instructions between RSP checking for the status
    // register ("mfc0 t0, COP0_SP_STATUS") RSP halting itself("break"),
    // the call to dl_flush might have no effect (see command_wait_new_input in
    // rsp_dl.S).
    // In general this is not a big problem even if it happens, as the RSP
    // would wake up at the next flush anyway, but we guarantee that dl_flush
    // does actually make the RSP finish the current buffer. To keep this
    // invariant, we wait 10 cycles and then issue the command again. This
    // make sure that even if the race condition happened, we still succeed
    // in waking up the RSP.
    __asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();
}

void dl_flush(void)
{
    // If we are recording a block, flushes can be ignored
    if (dl_block) return;

    dl_flush_internal();
}

/***********************************************************************/

#define DL_HIGHPRI_NUM_BUFS  8
#define DL_HIGHPRI_BUF_SIZE  128

int dl_highpri_widx;
uint32_t *dl_highpri_trampoline;
uint32_t *dl_highpri_buf;
int dl_highpri_used[DL_HIGHPRI_NUM_BUFS];


/*
The trampoline is the "bootstrap" code for the highpri queues. It is
stored in a different memory buffer. The trampoline is made by two fixed
parts (a header and a footer), and a body which is dynamically updated as
more queues are prepared by CPU, and executed by RSP.

The idea of the trampoline is to store a list of pending highpri queues in
its body, in the form of DL_CMD_JUMP commands. Every time the CPU prepares a
new highpri list, it adds a JUMP command in the trampoline body. Every time the
RSP executes a list, it removes the list from the trampoline. Notice that the
CPU treats the trampoline itself as a "critical section": before touching
it, it pauses the RSP, and also verify that the RSP is not executing commands
in the trampoline itself. These safety measures allow both CPU and RSP to
modify the trampoline without risking race conditions.

The way the removal of executed lists happens is peculiar: the trampoline header
is executed after every queue is run, and contains a DL_DMA command which "pops"
the first list from the body by copying the rest of the body over it. It basically
does the moral equivalent of "memmove(body, body+4, body_length)". 

This is an example that shows a possible trampoline:

       HEADER:
00 WSTATUS   SP_WSTATUS_CLEAR_SIG_HIGHPRI | SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING
01 NOP       (to align body)
02 DMA       DEST: Trampoline Body in RDRAM
03           SRC: Trampoline Body + 4 in DMEM
04           LEN: Trampoline Body length (num buffers * 2 * sizeof(uint32_t))
05           FLAGS: DMA_OUT_ASYNC
       
       BODY:
06 JUMP      queue1
07 NOP
08 JUMP      queue2
09 NOP
0A JUMP      12
0B NOP
0C JUMP      12
0D NOP
0E JUMP      12
0F NOP

       FOOTER:
10 JUMP      12
11 NOP
12 WSTATUS   SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING
13 RET       DL_HIGHPRI_CALL_SLOT
14 IDLE

Let's describe all commands one by one.

The first command (index 00) is a DL_CMD_SET_STATUS which clears the SIG_HIGHPRI
and sets SIG_HIGHPRI_RUNNING. This must absolutely be the first command executed
when the highpri mode starts, because otherwise the RSP would go into
an infinite loop (it would find SIG_HIGHPRI always set and calls the list
forever).

The second command (index 01) is a NOP, which is used to align the body to
8 bytes. This is important because the DL_DMA command that follows works only
on 8-byte aligned addresses.

The third command (index 02) is a DL_DMA which is used to remove the first list
from the RDRAM copy of the trampoline body. The first list is the one that will be
executed now, so we need to remove it so that we will not it execute it again
next time. In the above example, the copy will take words in range [08..11]
and copy them over the range [06..0F], effectively scrolling all other
JUMP calls up by one slot. Notice that words 10 and 11 are part of the footer
and they always contain the "empty data" (jump to the exit routine), so that the
body can be emptied correctly even if it was full.

The body covers indices 06-0F. It contains JUMPs to all queues that have been
prepared by the CPU. Each JUMP is followed by a NOP so that they are all
8-byte aligned, and the DL_DMA that pops one queue from the body is able to
work with 8-byte aligned entities. Notice that all highpri queues are
terminated with a JUMP to the *beginning* of the trampoline, so that the
full trampoline is run again after each list.

After the first two JUMPs to actual command queues, the rest of the body
is filled with JUMP to the footer exit code (index 12). This allows the RSP
to quickly jump to the final cleanup code when it's finished executing high
priority queues, without going through all the slots of the trampoline.

The first command in the footer (index 12) is a WSTATUS that clears
SIG_HIGHPRI_RUNNING, so that the CPU is able to later tell that the RSP has
finished running highpri queues.

The second command (index 13) is a RET that will resume executing in the
standard queue. The call slot used is DL_HIGHPRI_CALL_SLOT, which is where the
RSP has saved the current address when switching to highpri mode.

The third command (index 14) is a IDLE which is the standard terminator for
all command queues.

*/

static const uint32_t TRAMPOLINE_HEADER = 6;
static const uint32_t TRAMPOLINE_BODY = DL_HIGHPRI_NUM_BUFS*2;
static const uint32_t TRAMPOLINE_FOOTER = 5;
static const uint32_t TRAMPOLINE_WORDS = TRAMPOLINE_HEADER + TRAMPOLINE_BODY + TRAMPOLINE_FOOTER;

void __dl_highpri_init(void)
{
    dl_is_highpri = false;

    // Allocate the buffers for highpri queues (one contiguous memory area)
    int buf_size = DL_HIGHPRI_NUM_BUFS * DL_HIGHPRI_BUF_SIZE * sizeof(uint32_t);
    dl_highpri_buf = malloc_uncached(buf_size);
    memset(dl_highpri_buf, 0, buf_size);

    // Allocate the trampoline and initialize it
    dl_highpri_trampoline = malloc_uncached(TRAMPOLINE_WORDS*sizeof(uint32_t));
    uint32_t *dlp = dl_highpri_trampoline;

    // Write the trampoline header (6 words). 
    *dlp++ = (DL_CMD_SET_STATUS<<24) | SP_WSTATUS_CLEAR_SIG_HIGHPRI | SP_WSTATUS_SET_SIG_HIGHPRI_RUNNING;
    *dlp++ = DL_CMD_NOOP<<24;
    *dlp++ = (DL_CMD_DMA<<24) | (uint32_t)PhysicalAddr(dl_highpri_trampoline + TRAMPOLINE_HEADER);
    *dlp++ = 0xD8 + (TRAMPOLINE_HEADER+2)*sizeof(uint32_t); // FIXME address of DL_DMEM_BUFFER
    *dlp++ = (DL_HIGHPRI_NUM_BUFS*2) * sizeof(uint32_t) - 1;
    *dlp++ = 0xFFFF8000 | SP_STATUS_DMA_FULL | SP_STATUS_DMA_BUSY; // DMA_OUT_ASYNC

    uint32_t jump_to_footer = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dl_highpri_trampoline + TRAMPOLINE_HEADER + TRAMPOLINE_BODY + 2);

    // Fill the rest of the trampoline with noops
    assert(dlp - dl_highpri_trampoline == TRAMPOLINE_HEADER);
    for (int i = TRAMPOLINE_HEADER; i < TRAMPOLINE_HEADER+TRAMPOLINE_BODY; i+=2) {
        *dlp++ = jump_to_footer;
        *dlp++ = DL_CMD_NOOP<<24;
    }

    // Fill the footer
    *dlp++ = jump_to_footer;
    *dlp++ = DL_CMD_NOOP<<24;
    *dlp++ = (DL_CMD_SET_STATUS<<24) | SP_WSTATUS_CLEAR_SIG_HIGHPRI_RUNNING;
    *dlp++ = (DL_CMD_RET<<24) | (DL_HIGHPRI_CALL_SLOT<<2);
    *dlp++ = (DL_CMD_IDLE<<24);
    assert(dlp - dl_highpri_trampoline == TRAMPOLINE_WORDS);

    dl_data_ptr->dl_dram_highpri_addr = PhysicalAddr(dl_highpri_trampoline);
}

void dl_highpri_begin(void)
{
    assertf(!dl_is_highpri, "already in highpri mode");
    assertf(!dl_block, "cannot switch to highpri mode while creating a block");

    // Get the first buffer available for the new highpri queue
    int bufidx = dl_highpri_widx % DL_HIGHPRI_NUM_BUFS;
    uint32_t *dlh = &dl_highpri_buf[bufidx * DL_HIGHPRI_BUF_SIZE];

    debugf("dl_highpri_begin %p\n", dlh);

    // Clear the buffer. This clearing itself can be very slow compared to the
    // total time of dl_highpri_begin, so keep track of how much this buffer was
    // used last time, and only clear the part that was really used.
    memset(dlh, 0, dl_highpri_used[bufidx]);

    // Switch to the new buffer.
    dl_push_buffer();
    dl_switch_buffer(dlh, DL_HIGHPRI_BUF_SIZE-2, false);

    // Check if the RSP is running a highpri queue.
    if (!(*SP_STATUS & (SP_STATUS_SIG_HIGHPRI_RUNNING|SP_STATUS_SIG_HIGHPRI))) {    
        dl_highpri_trampoline[TRAMPOLINE_HEADER] = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dlh);
        MEMORY_BARRIER();
        *SP_STATUS = SP_WSTATUS_SET_SIG_HIGHPRI;
    } else {
        // Try pausing the RSP while it's executing code which is *outside* the
        // trampoline. We're going to modify the trampoline and we want to do it
        // while the RSP is not running there otherwise we risk race conditions.
    try_pause_rsp:
        rsp_pause(true);

        uint32_t dl_rdram_ptr = (((uint32_t)((volatile rsp_dl_t*)SP_DMEM)->dl_dram_addr) & 0x00FFFFFF);
        if (dl_rdram_ptr >= PhysicalAddr(dl_highpri_trampoline) && dl_rdram_ptr < PhysicalAddr(dl_highpri_trampoline+TRAMPOLINE_WORDS)) {
            debugf("SP PC in highpri trampoline... retrying\n");
            rsp_pause(false);
            wait_ticks(40);
            goto try_pause_rsp;
        }

        // Check the trampoline body. Search for the first JUMP to the footer
        // slot. We are going to replace it to a jump to our new queue.
        uint32_t jump_to_footer = dl_highpri_trampoline[TRAMPOLINE_HEADER + TRAMPOLINE_BODY];
        debugf("Trampoline %p (fetching at [%08lx]%08lx, PC:%lx)\n", dl_highpri_trampoline, dl_rdram_ptr, *(uint32_t*)(((uint32_t)(dl_rdram_ptr))|0xA0000000), *SP_PC);
        for (int i=TRAMPOLINE_HEADER; i<TRAMPOLINE_HEADER+TRAMPOLINE_BODY+2; i++)
            debugf("%x: %08lx %s\n", i, dl_highpri_trampoline[i], dl_highpri_trampoline[i]==jump_to_footer ? "*" : "");
        int tramp_widx = TRAMPOLINE_HEADER;
        while (dl_highpri_trampoline[tramp_widx] != jump_to_footer) {
            tramp_widx += 2;
            if (tramp_widx >= TRAMPOLINE_WORDS - TRAMPOLINE_FOOTER) {
                debugf("Highpri trampoline is full... retrying\n");
                rsp_pause(false);
                wait_ticks(400);
                goto try_pause_rsp;
            }
        }

        // Write the DL_CMD_JUMP to the new list
        dl_highpri_trampoline[tramp_widx] = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dlh);

        // At the beginning of the function, we found that the RSP was already
        // in highpri mode. Meanwhile, the RSP has probably advanced a few ops
        // (even if it was paused most of the time, it might have been unpaused
        // during retries, etc.). So it could have even exited highpri mode
        // (if it was near to completion).
        // So check again and if it's not in highpri mode, start it.
        MEMORY_BARRIER();
        if (!(*SP_STATUS & SP_STATUS_SIG_HIGHPRI_RUNNING))
            *SP_STATUS = SP_WSTATUS_SET_SIG_HIGHPRI;
        MEMORY_BARRIER();

        debugf("tramp_widx: %x\n", tramp_widx);

        // Unpause the RSP. We've done modifying the trampoline so it's safe now.
        rsp_pause(false);
    }

    dl_is_highpri = true;
}

void dl_highpri_end(void)
{
    assertf(dl_is_highpri, "not in highpri mode");

    // Terminate the highpri queue with a jump back to the trampoline.
    *dl_cur_pointer++ = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dl_highpri_trampoline);
    dl_terminator(dl_cur_pointer);

    debugf("dl_highpri_end %p\n", dl_cur_pointer+1);

    // Keep track of how much of this buffer was actually written to. This will
    // speed up next call to dl_highpri_begin, as we will clear only the
    // used portion of the buffer.
    int bufidx = dl_highpri_widx % DL_HIGHPRI_NUM_BUFS;
    uint32_t *dlh = &dl_highpri_buf[bufidx * DL_HIGHPRI_BUF_SIZE];
    dl_highpri_used[bufidx] = dl_cur_pointer + 1 - dlh;
    dl_highpri_widx++;

    // Pop back to the standard queue
    dl_pop_buffer();

    // Kick the RSP in case it was idling: we want to run this highpri
    // queue as soon as possible
    dl_flush();
    dl_is_highpri = false;
}

void dl_highpri_sync(void)
{
    void* ptr = 0;

    while (*SP_STATUS & (SP_STATUS_SIG_HIGHPRI_RUNNING|SP_STATUS_SIG_HIGHPRI)) 
    { 
        rsp_pause(true);
        void *ptr2 = (void*)(((uint32_t)((volatile rsp_dl_t*)SP_DMEM)->dl_dram_addr) & 0x00FFFFFF);
        if (ptr2 != ptr) {
            debugf("RSP: fetching at %p\n", ptr2);
            ptr = ptr2;
        }
        rsp_pause(false);
        wait_ticks(40);
    }
}


/***********************************************************************/

void dl_block_begin(void)
{
    assertf(!dl_block, "a block was already being created");
    assertf(!dl_is_highpri, "cannot create a block in highpri mode");

    // Allocate a new block (at minimum size) and initialize it.
    dl_block_size = DL_BLOCK_MIN_SIZE;
    dl_block = malloc_uncached(sizeof(dl_block_t) + dl_block_size*sizeof(uint32_t));
    dl_block->nesting_level = 0;

    // Switch to the block buffer. From now on, all dl_writes will
    // go into the block.
    dl_push_buffer();
    dl_switch_buffer(dl_block->cmds, dl_block_size, true);
}

dl_block_t* dl_block_end(void)
{
    assertf(dl_block, "a block was not being created");

    // Terminate the block with a RET command, encoding
    // the nesting level which is used as stack slot by RSP.
    *dl_cur_pointer++ = (DL_CMD_RET<<24) | (dl_block->nesting_level<<2);
    dl_terminator(dl_cur_pointer);

    // Switch back to the normal display list
    dl_pop_buffer();

    // Return the created block
    dl_block_t *b = dl_block;
    dl_block = NULL;
    return b;
}

void dl_block_free(dl_block_t *block)
{
    // Start from the commands in the first chunk of the block
    int size = DL_BLOCK_MIN_SIZE;
    void *start = block;
    uint32_t *ptr = block->cmds + size;
    while (1) {
        // Rollback until we find a non-zero command
        while (*--ptr == 0x00) {}
        uint32_t cmd = *ptr;

        // Ignore the terminator
        if (cmd>>24 == DL_CMD_IDLE)
            cmd = *--ptr;

        // If the last command is a JUMP
        if (cmd>>24 == DL_CMD_JUMP) {
            // Free the memory of the current chunk.
            free(CachedAddr(start));
            // Get the pointer to the next chunk
            start = UncachedAddr(0x80000000 | (cmd & 0xFFFFFF));
            if (size < DL_BLOCK_MAX_SIZE) size *= 2;
            ptr = (uint32_t*)start + size;
            continue;
        }
        // If the last command is a RET
        if (cmd>>24 == DL_CMD_RET) {
            // This is the last chunk, free it and exit
            free(CachedAddr(start));
            return;
        }
        // The last command is neither a JUMP nor a RET:
        // this is an invalid chunk of a block, better assert.
        assertf(0, "invalid terminator command in block: %08lx\n", cmd);
    }
}

void dl_block_run(dl_block_t *block)
{
    // Write the CALL op. The second argument is the nesting level
    // which is used as stack slot in the RSP to save the current
    // pointer position.
    uint32_t *dl = dl_write_begin();
    *dl++ = (DL_CMD_CALL<<24) | PhysicalAddr(block->cmds);
    *dl++ = block->nesting_level << 2;
    dl_write_end(dl);

    // If this is CALL within the creation of a block, update
    // the nesting level. A block's nesting level must be bigger
    // than the nesting level of all blocks called from it.
    if (dl_block && dl_block->nesting_level <= block->nesting_level) {
        dl_block->nesting_level = block->nesting_level + 1;
        assertf(dl_block->nesting_level < DL_MAX_BLOCK_NESTING_LEVEL,
            "reached maximum number of nested block runs");
    }
}


void dl_queue_u8(uint8_t cmd)
{
    uint32_t *dl = dl_write_begin();
    *dl++ = (uint32_t)cmd << 24;
    dl_write_end(dl);
}

void dl_queue_u16(uint16_t cmd)
{
    uint32_t *dl = dl_write_begin();
    *dl++ = (uint32_t)cmd << 16;
    dl_write_end(dl);
}

void dl_queue_u32(uint32_t cmd)
{
    uint32_t *dl = dl_write_begin();
    *dl++ = cmd;
    dl_write_end(dl);
}

void dl_queue_u64(uint64_t cmd)
{
    uint32_t *dl = dl_write_begin();
    *dl++ = cmd >> 32;
    *dl++ = cmd & 0xFFFFFFFF;
    dl_write_end(dl);
}

void dl_noop()
{
    dl_queue_u8(DL_CMD_NOOP);
}

dl_syncpoint_t dl_syncpoint(void)
{   
    assertf(!dl_block, "cannot create syncpoint in a block");
    uint32_t *dl = dl_write_begin();
    *dl++ = ((DL_CMD_TAS_STATUS << 24) | SP_WSTATUS_SET_INTR | SP_WSTATUS_SET_SIG_SYNCPOINT);
    *dl++ = SP_STATUS_SIG_SYNCPOINT;
    dl_write_end(dl);
    return ++dl_syncpoints_genid;
}

bool dl_check_syncpoint(dl_syncpoint_t sync_id) 
{
    return sync_id <= dl_syncpoints_done;
}

void dl_wait_syncpoint(dl_syncpoint_t sync_id)
{
    assertf(get_interrupts_state() == INTERRUPTS_ENABLED,
        "deadlock: interrupts are disabled");

    // Make sure the RSP is running, otherwise we might be blocking forever.
    dl_flush_internal();

    // Spinwait until the the syncpoint is reached.
    // TODO: with the kernel, it will be possible to wait for the RSP interrupt
    // to happen, without spinwaiting.
    while (!dl_check_syncpoint(sync_id)) { /* spinwait */ }
}

void dl_signal(uint32_t signal)
{
    const uint32_t allows_mask = SP_WSTATUS_CLEAR_SIG0|SP_WSTATUS_SET_SIG0|SP_WSTATUS_CLEAR_SIG1|SP_WSTATUS_SET_SIG1|SP_WSTATUS_CLEAR_SIG2|SP_WSTATUS_SET_SIG2;
    assertf((signal & allows_mask) == signal, "dl_signal called with a mask that contains bits outside SIG0-2: %lx", signal);

    dl_queue_u32((DL_CMD_SET_STATUS << 24) | signal);
}

static void dl_dma(void *rdram_addr, uint32_t dmem_addr, uint32_t len, uint32_t flags)
{
    uint32_t *dl = dl_write_begin();
    *dl++ = (DL_CMD_DMA << 24) | PhysicalAddr(rdram_addr);
    *dl++ = dmem_addr;
    *dl++ = len;
    *dl++ = flags;
    dl_write_end(dl);
}

void dl_dma_to_rdram(void *rdram_addr, uint32_t dmem_addr, uint32_t len, bool is_async)
{
    dl_dma(rdram_addr, dmem_addr, len - 1, 0xFFFF8000 | (is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL));
}

void dl_dma_to_dmem(uint32_t dmem_addr, void *rdram_addr, uint32_t len, bool is_async)
{
    dl_dma(rdram_addr, dmem_addr, len - 1, is_async ? 0 : SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL);
}
