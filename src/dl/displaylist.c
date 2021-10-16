#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libdragon.h>

#define DL_BUFFER_SIZE  0x1000

DEFINE_RSP_UCODE(rsp_displaylist);

typedef struct rsp_dl_s {
	void *dl_dram_addr;
	uint32_t dl_dram_size;
	void *dl_pointers_addr;
} __attribute__((packed)) rsp_dl_t;

_Static_assert(sizeof(rsp_dl_t) == 3*4);

typedef struct {
    uint32_t padding;
    uint32_t value;
} __attribute__((aligned(8))) dma_safe_pointer_t;

static struct {
    dma_safe_pointer_t read;
    dma_safe_pointer_t write;
    dma_safe_pointer_t wrap;
} dl_pointers;

static void *dl_buffer;
static void *dl_buffer_uncached;

static uint32_t reserved_size;
static bool is_wrapping;

void dl_init()
{
    if (dl_buffer != NULL) {
        return;
    }

    dl_buffer = malloc(DL_BUFFER_SIZE);
    dl_buffer_uncached = UncachedAddr(dl_buffer);

    dl_pointers.wrap.value = DL_BUFFER_SIZE;

    rsp_wait();
    rsp_load(&rsp_displaylist);

    // Load initial settings
    MEMORY_BARRIER();
    volatile rsp_dl_t *rsp_dl = (volatile rsp_dl_t*)SP_DMEM;
    rsp_dl->dl_dram_addr = PhysicalAddr(dl_buffer);
    rsp_dl->dl_dram_size = DL_BUFFER_SIZE;
    rsp_dl->dl_pointers_addr = PhysicalAddr(&dl_pointers);
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

    uint32_t wp = dl_pointers.write.value;

    uint32_t write_start;
    bool wrap;

    // TODO: make the loop tighter?
    while (1) {
        uint32_t rp = dl_pointers.read.value;

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
    uint32_t wp = dl_pointers.write.value;

    if (is_wrapping) {
        // We had to wrap around -> Store the wrap pointer
        dl_pointers.wrap.value = wp;
        // Return the write pointer back to the start of the buffer
        wp = 0;
    }

    // Advance the write pointer
    wp += reserved_size;

    // Ensure that the wrap pointer is never smaller than the write pointer
    if (wp > dl_pointers.wrap.value) {
        dl_pointers.wrap.value = wp;
    }

    MEMORY_BARRIER();

    // Store the new write pointer
    dl_pointers.write.value = wp;

    MEMORY_BARRIER();

    // Make rsp leave idle mode
    // TODO: need to advance PC?
    *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_SET_SIG0;
}
