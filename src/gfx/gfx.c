#include <libdragon.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"

DEFINE_RSP_UCODE(rsp_gfx);

static bool __gfx_initialized = 0;

void gfx_init()
{
    if (__gfx_initialized) {
        return;
    }

    gfx_state_t *gfx_state = UncachedAddr(rspq_overlay_get_state(&rsp_gfx));

    memset(gfx_state, 0, sizeof(gfx_state_t));

    rspq_init();
    rspq_overlay_register_static(&rsp_gfx, GFX_OVL_ID);

    __gfx_initialized = 1;
}

void gfx_close()
{
    rspq_overlay_unregister(GFX_OVL_ID);
    __gfx_initialized = 0;
}
