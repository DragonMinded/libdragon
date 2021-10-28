#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>

#define DL_BUFFER_SIZE       0x1000
#define DL_MAX_OVERLAY_COUNT 16

DEFINE_RSP_UCODE(rsp_displaylist);

static dl_overlay_t dl_overlay_table[DL_MAX_OVERLAY_COUNT];

typedef struct rsp_dl_s {
	void *dl_dram_addr;
	void *dl_pointers_addr;
    dl_overlay_t overlay_table[DL_MAX_OVERLAY_COUNT];
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

static dl_pointers_t dl_pointers;

#define DL_POINTERS ((volatile dl_pointers_t*)(UncachedAddr(&dl_pointers)))

static void *dl_buffer;
static void *dl_buffer_uncached;

static uint32_t reserved_size;
static bool is_wrapping;

// TODO: Do this at compile time?
void dl_overlay_register(uint8_t id, dl_overlay_t *overlay)
{
    assertf(id > 0 && id < DL_MAX_OVERLAY_COUNT, "Tried to register invalid overlay id: %d", id);
    assert(overlay);

    assertf(dl_buffer == NULL, "dl_overlay_register must be called before dl_init!");

    dl_overlay_table[id] = *overlay;
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
    // TODO: is dma faster/better?
    MEMORY_BARRIER();
    volatile rsp_dl_t *rsp_dl = (volatile rsp_dl_t*)SP_DMEM;
    rsp_dl->dl_dram_addr = PhysicalAddr(dl_buffer);
    rsp_dl->dl_pointers_addr = PhysicalAddr(&dl_pointers);
    for (int i = 0; i < DL_MAX_OVERLAY_COUNT; ++i) {
        rsp_dl->overlay_table[i].code = dl_overlay_table[i].code;
        rsp_dl->overlay_table[i].code_size = dl_overlay_table[i].code_size;
        rsp_dl->overlay_table[i].data = dl_overlay_table[i].data;
        rsp_dl->overlay_table[i].data_size = dl_overlay_table[i].data_size;
    }
    MEMORY_BARRIER();

    rsp_run_async();
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
