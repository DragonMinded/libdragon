#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>
#include "dl_internal.h"
#include "utils.h"
#include "../../build/dl/dl_symbols.h"

#define DL_CMD_NOOP             0x07
#define DL_CMD_WSTATUS          0x02
#define DL_CMD_CALL             0x03
#define DL_CMD_JUMP             0x04
#define DL_CMD_RET              0x05

#define SP_STATUS_SIG_BUFDONE         SP_STATUS_SIG5
#define SP_WSTATUS_SET_SIG_BUFDONE    SP_WSTATUS_SET_SIG5
#define SP_WSTATUS_CLEAR_SIG_BUFDONE  SP_WSTATUS_CLEAR_SIG5

#define SP_STATUS_SIG_HIGHPRI         SP_STATUS_SIG6
#define SP_WSTATUS_SET_SIG_HIGHPRI    SP_WSTATUS_SET_SIG6
#define SP_WSTATUS_CLEAR_SIG_HIGHPRI  SP_WSTATUS_CLEAR_SIG6

#define SP_STATUS_SIG_MORE            SP_STATUS_SIG7
#define SP_WSTATUS_SET_SIG_MORE       SP_WSTATUS_SET_SIG7
#define SP_WSTATUS_CLEAR_SIG_MORE     SP_WSTATUS_CLEAR_SIG7

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

typedef struct rsp_dl_s {
    uint8_t overlay_table[DL_OVERLAY_TABLE_SIZE];
    dl_overlay_t overlay_descriptors[DL_MAX_OVERLAY_COUNT];
    void *dl_dram_addr;
    void *dl_dram_highpri_addr;
    int16_t current_ovl;
} __attribute__((aligned(8), packed)) rsp_dl_t;

static rsp_dl_t dl_data;
static uint8_t dl_overlay_count = 0;

static uint32_t dl_buffers[2][DL_DRAM_BUFFER_SIZE];
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
    debugf("dl_sp_interrupt(): %d\n", dl_syncpoints_done);
}

void dl_init()
{
    // Load initial settings
    memset(&dl_data, 0, sizeof(dl_data));

    dl_cur_pointer = UncachedAddr(dl_buffers[0]);
    memset(dl_cur_pointer, 0, DL_DRAM_BUFFER_SIZE*sizeof(uint32_t));
    dl_terminator(dl_cur_pointer);
    dl_cur_sentinel = dl_cur_pointer + DL_DRAM_BUFFER_SIZE - DL_MAX_COMMAND_SIZE;
    dl_block = NULL;

    dl_data.dl_dram_addr = PhysicalAddr(dl_buffers[0]);
    dl_data.overlay_descriptors[0].data_buf = PhysicalAddr(&dummy_overlay_state);
    dl_data.overlay_descriptors[0].data_size = sizeof(uint64_t);
    
    dl_syncpoints_genid = 0;
    dl_syncpoints_done = 0;

    dl_overlay_count = 1;

    // Activate SP interrupt (used for syncpoints)
    register_SP_handler(dl_sp_interrupt);
    set_SP_interrupt(1);
}

void dl_close()
{
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    dl_is_running = 0;

    set_SP_interrupt(0);
    unregister_SP_handler(dl_sp_interrupt);
}

void* dl_overlay_get_state(rsp_ucode_t *overlay_ucode)
{
    dl_overlay_header_t *overlay_header = (dl_overlay_header_t*)overlay_ucode->data;
    return overlay_ucode->data + (overlay_header->state_start & 0xFFF) - DL_OVL_DATA_ADDR;
}

uint8_t dl_overlay_add(rsp_ucode_t *overlay_ucode)
{
    assertf(dl_overlay_count > 0, "dl_overlay_add must be called after dl_init!");
    
    assertf(dl_overlay_count < DL_MAX_OVERLAY_COUNT, "Only up to %d overlays are supported!", DL_MAX_OVERLAY_COUNT);

    assert(overlay_ucode);

    dl_overlay_t *overlay = &dl_data.overlay_descriptors[dl_overlay_count];

    // The DL ucode is always linked into overlays for now, so we need to load the overlay from an offset.
    // TODO: Do this some other way.
    uint32_t dl_ucode_size = rsp_dl_text_end - rsp_dl_text_start;

    overlay->code = PhysicalAddr(overlay_ucode->code + dl_ucode_size);
    overlay->data = PhysicalAddr(overlay_ucode->data);
    overlay->data_buf = PhysicalAddr(dl_overlay_get_state(overlay_ucode));
    overlay->code_size = ((uint8_t*)overlay_ucode->code_end - overlay_ucode->code) - dl_ucode_size - 1;
    overlay->data_size = ((uint8_t*)overlay_ucode->data_end - overlay_ucode->data) - 1;

    return dl_overlay_count++;
}

void dl_overlay_register_id(uint8_t overlay_index, uint8_t id)
{
    assertf(dl_overlay_count > 0, "dl_overlay_register must be called after dl_init!");

    assertf(overlay_index < DL_MAX_OVERLAY_COUNT, "Tried to register invalid overlay index: %d", overlay_index);
    assertf(id < DL_OVERLAY_TABLE_SIZE, "Tried to register id: %d", id);


    dl_data.overlay_table[id] = overlay_index * sizeof(dl_overlay_t);
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
    data_cache_hit_writeback(&dl_data, sizeof(dl_data));
    rsp_load_data(PhysicalAddr(&dl_data), sizeof(dl_data), 0);

    static const dl_overlay_header_t dummy_header = (dl_overlay_header_t){
        .state_start = 0,
        .state_size = 7,
        .command_base = 0
    };

    rsp_load_data(PhysicalAddr(&dummy_header), sizeof(dummy_header), DL_OVL_DATA_ADDR);

    *SP_STATUS = SP_WSTATUS_CLEAR_SIG0 | 
                 SP_WSTATUS_CLEAR_SIG1 | 
                 SP_WSTATUS_CLEAR_SIG2 | 
                 SP_WSTATUS_CLEAR_SIG3 | 
                 SP_WSTATUS_CLEAR_SIG4 | 
                 SP_WSTATUS_SET_SIG_BUFDONE |
                 SP_WSTATUS_CLEAR_SIG_HIGHPRI |
                 SP_WSTATUS_CLEAR_SIG_MORE;

    // Off we go!
    rsp_run_async();

    dl_is_running = 1;
}

static uint32_t* dl_switch_buffer(uint32_t *dl2, int size)
{
    uint32_t* prev = dl_cur_pointer;

    // Clear the new buffer, and add immediately a terminator
    // so that it's a valid buffer.
    memset(dl2, 0, size*sizeof(uint32_t));
    dl_terminator(dl2);

    // Switch to the new buffer, and calculate the new sentinel.
    dl_cur_pointer = dl2;
    dl_cur_sentinel = dl_cur_pointer + size - DL_MAX_COMMAND_SIZE;

    // Return a pointer to the previous buffer
    return prev;
}

static void dl_next_buffer(void) {
    // If we're creating a block
    if (dl_block) {
        // Allocate next chunk (double the size of the current one).
        // We use doubling here to reduce overheads for large blocks
        // and at the same time start small.
        if (dl_block_size < DL_BLOCK_MAX_SIZE) dl_block_size *= 2;

        // Allocate a new chunk of the block and switch to it.
        uint32_t *dl2 = UncachedAddr(malloc(dl_block_size));
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
    while (!(*SP_STATUS & SP_STATUS_SIG_BUFDONE)) { /* idle */ }
    *SP_STATUS = SP_WSTATUS_CLEAR_SIG_BUFDONE;

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

    // Kick the RSP, in case it's sleeping.
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
}

void dl_write_end(uint32_t *dl) {
    // Terminate the buffer (so that the RSP will sleep in case
    // it catches up with us).
    dl_terminator(dl);

    // Kick the RSP if it's idle.
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;

    // Update the pointer and check if we went past the sentinel,
    // in which case it's time to switch to the next buffer.
    dl_cur_pointer = dl;
    if (dl_cur_pointer > dl_cur_sentinel) {
        dl_next_buffer();
    }
}

void dl_block_begin(void)
{
    assertf(!dl_block, "a block was already being created");

    // Allocate a new block (at minimum size) and initialize it.
    dl_block_size = DL_BLOCK_MIN_SIZE;
    dl_block = UncachedAddr(malloc(sizeof(dl_block_t) + dl_block_size));
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
        if (cmd>>24 == 0x01)
            cmd = *--ptr;

        // If the last command is a JUMP
        if (cmd>>24 == DL_CMD_JUMP) {
            // Free the memory of the current chunk.
            free(start);
            // Get the pointer to the next chunk
            start = UncachedAddr(0x80000000 | (cmd & 0xFFFFFF));
            if (size < DL_BLOCK_MAX_SIZE) size *= 2;
            ptr = start;
        }
        // If the last command is a RET
        if (cmd>>24 == DL_CMD_RET) {
            // This is the last chunk, free it and exit
            free(start);
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

int dl_syncpoint(void)
{   
    // TODO: cannot use in compiled lists
    dl_queue_u32((DL_CMD_WSTATUS << 24) | SP_WSTATUS_SET_INTR);
    return ++dl_syncpoints_genid;
}

bool dl_check_syncpoint(int sync_id) 
{
    return sync_id <= dl_syncpoints_done;
}

void dl_wait_syncpoint(int sync_id)
{
    while (!dl_check_syncpoint(sync_id)) { /* spinwait */ }
}

void dl_signal(uint32_t signal)
{
    dl_queue_u32((DL_CMD_WSTATUS << 24) | signal);
}
