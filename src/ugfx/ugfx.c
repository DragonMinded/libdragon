#include <libdragon.h>
#include <stdlib.h>
#include <string.h>

#include "ugfx_internal.h"

DEFINE_RSP_UCODE(rsp_ugfx);

uint8_t __ugfx_dram_buffer[UGFX_RDP_DRAM_BUFFER_SIZE];

static bool __ugfx_initialized = 0;

void ugfx_init()
{
    if (__ugfx_initialized) {
        return;
    }

    ugfx_state_t *ugfx_state = UncachedAddr(dl_overlay_get_state(&rsp_ugfx));

    memset(ugfx_state, 0, sizeof(ugfx_state_t));

    ugfx_state->dram_buffer = PhysicalAddr(__ugfx_dram_buffer);
    ugfx_state->dram_buffer_size = UGFX_RDP_DRAM_BUFFER_SIZE;

    dl_init();
    dl_overlay_register(&rsp_ugfx, 2);
    dl_overlay_register(&rsp_ugfx, 3);

    __ugfx_initialized = 1;
}

void ugfx_close()
{
    __ugfx_initialized = 0;
}
