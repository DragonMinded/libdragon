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
#define DL_CMD_JUMP             0x04

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
uint32_t *dl_cur_pointer;
uint32_t *dl_sentinel;

static int dl_syncpoints_genid;
static volatile int dl_syncpoints_done;

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
    dl_sentinel = dl_cur_pointer + DL_DRAM_BUFFER_SIZE - DL_MAX_COMMAND_SIZE;

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

__attribute__((noinline))
void dl_write_end(uint32_t *dl) {
    dl_terminator(dl);
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;

    dl_cur_pointer = dl;
    if (dl_cur_pointer > dl_sentinel) {
        extern void dl_next_buffer(void);
        dl_next_buffer();
    }
}

void dl_next_buffer() {
    while (!(*SP_STATUS & SP_STATUS_SIG_BUFDONE)) { /* idle */ }
    *SP_STATUS = SP_WSTATUS_CLEAR_SIG_BUFDONE;

    // TODO: wait for buffer to be usable
    // TODO: insert signal command at end of buffer
    dl_buf_idx = 1-dl_buf_idx;

    uint32_t *dl2 = UncachedAddr(&dl_buffers[dl_buf_idx]);
    memset(dl2, 0, DL_DRAM_BUFFER_SIZE*sizeof(uint32_t));
    dl_terminator(dl2);

    *dl_cur_pointer++ = (DL_CMD_WSTATUS<<24) | SP_WSTATUS_SET_SIG_BUFDONE;
    *dl_cur_pointer++ = (DL_CMD_JUMP<<24) | (uint32_t)PhysicalAddr(dl2);
    dl_terminator(dl_cur_pointer);
    *SP_STATUS = SP_WSTATUS_SET_SIG_MORE | SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;

    dl_cur_pointer = dl2;
    dl_sentinel = dl_cur_pointer + DL_DRAM_BUFFER_SIZE - DL_MAX_COMMAND_SIZE;
}


#if 0



uint32_t* dl_write_begin(uint32_t size)
{
    assert((size % sizeof(uint32_t)) == 0);
    assertf(size <= DL_MAX_COMMAND_SIZE, "Command is too big! DL_MAX_COMMAND_SIZE needs to be adjusted!");
    assertf(dl_is_running, "dl_start() needs to be called before queueing commands!");

    reserved_size = size;
    uint32_t wp = DL_POINTERS->write.value;

    if (wp < sentinel) {
        return (uint32_t*)(dl_buffer_uncached + wp);
    }

    uint32_t write_start;
    bool wrap;
    uint32_t safe_end;

    while (1) {
        uint32_t rp = DL_POINTERS->read.value;

        // Is the write pointer ahead of the read pointer?
        if (wp >= rp) {
            // Enough space left at the end of the buffer?
            if (wp + size <= DL_DRAM_BUFFER_SIZE) {
                wrap = false;
                write_start = wp;
                safe_end = DL_DRAM_BUFFER_SIZE;
                break;

            // Not enough space left -> we need to wrap around
            // Enough space left at the start of the buffer?
            } else if (size < rp) {
                wrap = true;
                write_start = 0;
                safe_end = rp;
                break;
            }
        
        // Read pointer is ahead
        // Enough space left between write and read pointer?
        } else if (size < rp - wp) {
            wrap = false;
            write_start = wp;
            safe_end = rp;
            break;
        }

        // Not enough space left anywhere -> buffer is full.
        // Repeat the checks until there is enough space.
    }

    sentinel = safe_end >= DL_MAX_COMMAND_SIZE ? safe_end - DL_MAX_COMMAND_SIZE : 0;

    is_wrapping = wrap;

    return (uint32_t*)(dl_buffer_uncached + write_start);
}

void dl_write_end()
{
    uint32_t wp = DL_POINTERS->write.value;

    if (is_wrapping) {
        is_wrapping = false;

        // Pad the end of the buffer with zeroes
        uint32_t *ptr = (uint32_t*)(dl_buffer_uncached + wp);
        uint32_t size = DL_DRAM_BUFFER_SIZE - wp;
        for (uint32_t i = 0; i < size; i++)
        {
            ptr[i] = 0;
        }

        // Return the write pointer back to the start of the buffer
        wp = 0;
    }

    // Advance the write pointer
    wp += reserved_size;

    MEMORY_BARRIER();

    // Store the new write pointer
    DL_POINTERS->write.value = wp;

    MEMORY_BARRIER();

    // Make rsp leave idle mode
    *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE | SP_WSTATUS_SET_SIG0;
}
#endif

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
    while (dl_check_syncpoint(sync_id)) { /* spinwait */ }
}

void dl_signal(uint32_t signal)
{
    dl_queue_u32((DL_CMD_WSTATUS << 24) | signal);
}
