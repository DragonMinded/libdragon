#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>
#include <malloc.h>
#include "dl_internal.h"
#include "utils.h"
#include "../../build/dl/dl_symbols.h"

#define DL_CMD_IDLE             0x01
#define DL_CMD_WSTATUS          0x02
#define DL_CMD_CALL             0x03
#define DL_CMD_JUMP             0x04
#define DL_CMD_RET              0x05
#define DL_CMD_NOOP             0x07
#define DL_CMD_DMA              0x08
#define DL_CMD_TEST_AND_WSTATUS 0x09

#define dl_terminator(dl)   ({ \
    /* The terminator is usually meant to be written only *after* the last \
       command has been fully written, otherwise the RSP could in theory \
       execute a partial command. Force ordering via a memory barrier. */ \
    MEMORY_BARRIER(); \
    *(uint8_t*)(dl) = 0x01; \
})

#define SP_STATUS_SIG_SYNCPOINT        SP_STATUS_SIG4
#define SP_WSTATUS_SET_SIG_SYNCPOINT   SP_WSTATUS_SET_SIG4
#define SP_WSTATUS_CLEAR_SIG_SYNCPOINT SP_WSTATUS_CLEAR_SIG4

#define SP_STATUS_SIG_BUFDONE          SP_STATUS_SIG5
#define SP_WSTATUS_SET_SIG_BUFDONE     SP_WSTATUS_SET_SIG5
#define SP_WSTATUS_CLEAR_SIG_BUFDONE   SP_WSTATUS_CLEAR_SIG5

#define SP_STATUS_SIG_HIGHPRI          SP_STATUS_SIG6
#define SP_WSTATUS_SET_SIG_HIGHPRI     SP_WSTATUS_SET_SIG6
#define SP_WSTATUS_CLEAR_SIG_HIGHPRI   SP_WSTATUS_CLEAR_SIG6

#define SP_STATUS_SIG_MORE             SP_STATUS_SIG7
#define SP_WSTATUS_SET_SIG_MORE        SP_WSTATUS_SET_SIG7
#define SP_WSTATUS_CLEAR_SIG_MORE      SP_WSTATUS_CLEAR_SIG7

DEFINE_RSP_UCODE(rsp_dl);

typedef struct dl_overlay_t {
    void* code;
    void* data;
    void* data_buf;
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
    void *dl_dram_addr;
    void *dl_dram_highpri_addr;
    int16_t current_ovl;
} __attribute__((aligned(8), packed)) rsp_dl_t;

static rsp_dl_t dl_data;
#define dl_data_ptr ((rsp_dl_t*)UncachedAddr(&dl_data))

static uint8_t dl_overlay_count = 0;

/** @brief Command list buffers (full cachelines to avoid false sharing) */
static uint32_t dl_buffers[2][DL_DRAM_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t dl_buf_idx;
static uint32_t *dl_buffer_ptr, *dl_buffer_sentinel;
static dl_block_t *dl_block;
static int dl_block_size;

uint32_t *dl_cur_pointer;
uint32_t *dl_cur_sentinel;

static int dl_syncpoints_genid;
volatile int dl_syncpoints_done;

static bool dl_is_running;

static uint64_t dummy_overlay_state;

static void dl_sp_interrupt(void) 
{
    ++dl_syncpoints_done;
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_CLEAR_SIG_SYNCPOINT;
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
    rsp_load_data(PhysicalAddr(dl_data_ptr), sizeof(rsp_dl_t), 0);

    static const dl_overlay_header_t dummy_header = (dl_overlay_header_t){
        .state_start = 0,
        .state_size = 7,
        .command_base = 0
    };

    rsp_load_data(PhysicalAddr(&dummy_header), sizeof(dummy_header), DL_OVL_DATA_ADDR);

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
    dl_is_running = 0;

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
    // TODO: Do this some other way.
    uint32_t dl_ucode_size = rsp_dl_text_end - rsp_dl_text_start;
    void *overlay_code = PhysicalAddr(overlay_ucode->code + dl_ucode_size);

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
    dl_dma((uint32_t)&dl_data_ptr->tables, 0, sizeof(dl_overlay_tables_t) - 1, SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL);
}

static uint32_t* dl_switch_buffer(uint32_t *dl2, int size)
{
    uint32_t* prev = dl_cur_pointer;

    // Clear the new buffer, and add immediately a terminator
    // so that it's a valid buffer.
    assert(size >= DL_MAX_COMMAND_SIZE);
    memset(dl2, 0, size*sizeof(uint32_t));
    dl_terminator(dl2);

    // Switch to the new buffer, and calculate the new sentinel.
    dl_cur_pointer = dl2;
    dl_cur_sentinel = dl_cur_pointer + size - DL_MAX_COMMAND_SIZE;

    // Return a pointer to the previous buffer
    return prev;
}

/**
 * @brief Allocate a buffer that will be accessed as uncached memory.
 * 
 * @param[in]  size  The size of the buffer to allocate
 *
 * @return a point to the start of the buffer (as uncached pointer)
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
    // If we're creating a block
    if (dl_block) {
        // Allocate next chunk (double the size of the current one).
        // We use doubling here to reduce overheads for large blocks
        // and at the same time start small.
        if (dl_block_size < DL_BLOCK_MAX_SIZE) dl_block_size *= 2;

        // Allocate a new chunk of the block and switch to it.
        uint32_t *dl2 = malloc_uncached(dl_block_size*sizeof(uint32_t));
        uint32_t *prev = dl_switch_buffer(dl2, dl_block_size);

        // Terminate the previous chunk with a JUMP op to the new chunk.
        *prev++ = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dl2);
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
    uint32_t *prev = dl_switch_buffer(dl2, DL_DRAM_BUFFER_SIZE);

    // Terminate the previous buffer with an op to set SIG_BUFDONE
    // (to notify when the RSP finishes the buffer), plus a jump to
    // the new buffer.
    *prev++ = (DL_CMD_WSTATUS<<24) | SP_WSTATUS_SET_SIG_BUFDONE;
    *prev++ = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dl2);
    dl_terminator(prev);

    MEMORY_BARRIER();
    // Kick the RSP, in case it's sleeping.
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
    MEMORY_BARRIER();
}

__attribute__((noinline))
void dl_flush(void)
{
    // If we are recording a block, flushes can be ignored
    if (dl_block) return;

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

void dl_block_begin(void)
{
    assertf(!dl_block, "a block was already being created");

    // Allocate a new block (at minimum size) and initialize it.
    dl_block_size = DL_BLOCK_MIN_SIZE;
    dl_block = malloc_uncached(sizeof(dl_block_t) + dl_block_size*sizeof(uint32_t));
    dl_block->nesting_level = 0;

    // Save the current pointer/sentinel for later restore
    dl_buffer_sentinel = dl_cur_sentinel;
    dl_buffer_ptr = dl_cur_pointer;

    // Switch to the block buffer. From now on, all dl_writes will
    // go into the block.
    dl_switch_buffer(dl_block->cmds, dl_block_size);
}

dl_block_t* dl_block_end(void)
{
    assertf(dl_block, "a block was not being created");

    // Terminate the block with a RET command, encoding
    // the nesting level which is used as stack slot by RSP.
    *dl_cur_pointer++ = (DL_CMD_RET<<24) | (dl_block->nesting_level<<2);
    dl_terminator(dl_cur_pointer);

    // Switch back to the normal display list
    dl_cur_pointer = dl_buffer_ptr;
    dl_cur_sentinel = dl_buffer_sentinel;

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
    *dl++ = (DL_CMD_CALL<<24) | (uint32_t)PhysicalAddr(block->cmds);
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
    *dl++ = (DL_CMD_TEST_AND_WSTATUS << 24) | SP_WSTATUS_SET_INTR | SP_WSTATUS_SET_SIG_SYNCPOINT;
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
    // Make sure the RSP is running, otherwise we might be blocking forever.
    dl_flush();

    // Spinwait until the the syncpoint is reached.
    // TODO: with the kernel, it will be possible to wait for the RSP interrupt
    // to happen, without spinwaiting.
    while (!dl_check_syncpoint(sync_id)) { /* spinwait */ }
}

void dl_signal(uint32_t signal)
{
    dl_queue_u32((DL_CMD_WSTATUS << 24) | signal);
}

void dl_dma(uint32_t rdram_addr, uint32_t dmem_addr, uint32_t len, uint32_t flags)
{
    uint32_t *dl = dl_write_begin();
    *dl++ = (DL_CMD_DMA << 24) | (uint32_t)PhysicalAddr(rdram_addr);
    *dl++ = dmem_addr;
    *dl++ = len;
    *dl++ = flags;
    dl_write_end(dl);
}
