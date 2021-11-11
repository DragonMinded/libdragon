#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>

#define DL_BUFFER_SIZE        0x1000
#define DL_OVERLAY_TABLE_SIZE 16
#define DL_MAX_OVERLAY_COUNT  8

DEFINE_RSP_UCODE(rsp_displaylist);

typedef struct dl_overlay_t {
    void* code;
    void* data;
    void* data_buf;
    uint16_t code_size;
    uint16_t data_size;
} dl_overlay_t;

typedef struct rsp_dl_s {
	void *dl_dram_addr;
	void *dl_pointers_addr;
    uint8_t overlay_table[DL_OVERLAY_TABLE_SIZE];
    dl_overlay_t overlay_descriptors[DL_MAX_OVERLAY_COUNT];
} __attribute__((aligned(8), packed)) rsp_dl_t;

typedef struct dma_safe_pointer_t {
    uint32_t padding;
    uint32_t value;
} __attribute__((aligned(8))) dma_safe_pointer_t;

typedef struct dl_pointers_t {
    dma_safe_pointer_t read;
    dma_safe_pointer_t write;
    dma_safe_pointer_t wrap;
} dl_pointers_t;

static rsp_dl_t dl_data;
static uint8_t dl_overlay_count = 0;

static dl_pointers_t dl_pointers;

#define DL_POINTERS ((volatile dl_pointers_t*)(UncachedAddr(&dl_pointers)))

static void *dl_buffer;
static void *dl_buffer_uncached;

static bool dl_is_running;

static uint32_t reserved_size;
static bool is_wrapping;

uint8_t dl_overlay_add(void* code, void *data, uint16_t code_size, uint16_t data_size, void *data_buf)
{
    assertf(dl_overlay_count < DL_MAX_OVERLAY_COUNT, "Only up to %d overlays are supported!", DL_MAX_OVERLAY_COUNT);

    assert(code);
    assert(data);

    dl_overlay_t *overlay = &dl_data.overlay_descriptors[dl_overlay_count];

    // The DL ucode is always linked into overlays for now, so we need to load the overlay from an offset.
    // TODO: Do this some other way.
    uint32_t dl_ucode_size = rsp_displaylist_text_end - rsp_displaylist_text_start;

    overlay->code = PhysicalAddr(code + dl_ucode_size);
    overlay->data = PhysicalAddr(data);
    overlay->data_buf = PhysicalAddr(data_buf);
    overlay->code_size = code_size - dl_ucode_size - 1;
    overlay->data_size = data_size - 1;

    return dl_overlay_count++;
}

void dl_overlay_register_id(uint8_t overlay_index, uint8_t id)
{
    assertf(overlay_index < DL_MAX_OVERLAY_COUNT, "Tried to register invalid overlay index: %d", overlay_index);
    assertf(id < DL_OVERLAY_TABLE_SIZE, "Tried to register id: %d", id);

    assertf(dl_buffer != NULL, "dl_overlay_register must be called after dl_init!");

    dl_data.overlay_table[id] = overlay_index * sizeof(dl_overlay_t);
}

void dl_init()
{
    if (dl_buffer != NULL) {
        return;
    }

    dl_buffer = malloc(DL_BUFFER_SIZE);
    dl_buffer_uncached = UncachedAddr(dl_buffer);

    DL_POINTERS->read.value = 0;
    DL_POINTERS->write.value = 0;
    DL_POINTERS->wrap.value = DL_BUFFER_SIZE;

    rsp_wait();
    rsp_load(&rsp_displaylist);

    // Load initial settings
    dl_data.dl_dram_addr = PhysicalAddr(dl_buffer);
    dl_data.dl_pointers_addr = PhysicalAddr(&dl_pointers);

    memset(&dl_data.overlay_table, 0, sizeof(dl_data.overlay_table));
    memset(&dl_data.overlay_descriptors, 0, sizeof(dl_data.overlay_descriptors));
    
    dl_overlay_count = 0;
}

void dl_start()
{
    if (dl_is_running)
    {
        return;
    }

    // Load data with initialized overlays into DMEM
    data_cache_hit_writeback(&dl_data, sizeof(dl_data));
    rsp_load_data(PhysicalAddr(&dl_data), sizeof(dl_data), 0);

    // Off we go!
    rsp_run_async();

    dl_is_running = 1;
}

void dl_close()
{
    if (dl_buffer == NULL) {
        return;
    }

    *SP_STATUS = SP_WSTATUS_SET_HALT;

    free(dl_buffer);
    dl_buffer = NULL;
    dl_buffer_uncached = NULL;
    dl_is_running = 0;
}

uint32_t* dl_write_begin(uint32_t size)
{
    assert((size % sizeof(uint32_t)) == 0);

    uint32_t wp = DL_POINTERS->write.value;

    uint32_t write_start;
    bool wrap;

    // TODO: make the loop tighter?
    while (1) {
        uint32_t rp = DL_POINTERS->read.value;

        // Is the write pointer ahead of the read pointer?
        if (wp >= rp) {
            // Enough space left at the end of the buffer?
            if (wp + size <= DL_BUFFER_SIZE) {
                wrap = false;
                write_start = wp;
                break;

            // Not enough space left -> we need to wrap around
            // Enough space left at the start of the buffer?
            } else if (size < rp) {
                wrap = true;
                write_start = 0;
                break;
            }
        
        // Read pointer is ahead
        // Enough space left between write and read pointer?
        } else if (size < rp - wp) {
            wrap = false;
            write_start = wp;
            break;
        }

        // Not enough space left anywhere -> buffer is full.
        // Repeat the checks until there is enough space.
    }

    is_wrapping = wrap;
    reserved_size = size;

    return (uint32_t*)(dl_buffer_uncached + write_start);
}

void dl_write_end()
{
    uint32_t wp = DL_POINTERS->write.value;

    if (is_wrapping) {
        // We had to wrap around -> Store the wrap pointer
        DL_POINTERS->wrap.value = wp;
        // Return the write pointer back to the start of the buffer
        wp = 0;
    }

    // Advance the write pointer
    wp += reserved_size;

    // Ensure that the wrap pointer is never smaller than the write pointer
    if (wp > DL_POINTERS->wrap.value) {
        DL_POINTERS->wrap.value = wp;
    }

    MEMORY_BARRIER();

    // Store the new write pointer
    DL_POINTERS->write.value = wp;

    MEMORY_BARRIER();

    // Make rsp leave idle mode
    *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE | SP_WSTATUS_SET_SIG0;
}

// TODO: Find a way to pack commands that are smaller than 4 bytes

void dl_queue_u8(uint8_t cmd)
{
    *dl_write_begin(sizeof(uint32_t)) = (uint32_t)cmd << 24;
    dl_write_end();
}

void dl_queue_u16(uint16_t cmd)
{
    *dl_write_begin(sizeof(uint32_t)) = (uint32_t)cmd << 16;
    dl_write_end();
}

void dl_queue_u32(uint32_t cmd)
{
    *dl_write_begin(sizeof(uint32_t)) = cmd;
    dl_write_end();
}

void dl_queue_u64(uint64_t cmd)
{
    uint32_t *ptr = dl_write_begin(sizeof(uint64_t));
    ptr[0] = cmd >> 32;
    ptr[1] = cmd & 0xFFFFFFFF;
    dl_write_end();
}

void dl_noop()
{
    dl_queue_u8(DL_MAKE_COMMAND(DL_OVERLAY_DEFAULT, DL_CMD_NOOP));
}

void dl_interrupt()
{
    dl_queue_u8(DL_MAKE_COMMAND(DL_OVERLAY_DEFAULT, DL_CMD_INTERRUPT));
}
