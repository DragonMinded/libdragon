#include "rdpq_mode.h"
#include "rspq.h"
#include "rdpq_internal.h"

__attribute__((noinline))
void __rdpq_fixup_mode(uint32_t cmd_id, uint32_t w0, uint32_t w1)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (cmd_id, w0, w1),
        (RDPQ_CMD_SET_COMBINE_MODE_RAW, w0, w1), (RDPQ_CMD_SET_OTHER_MODES, w0, w1)
    );
}

void rdpq_mode_push(void)
{
    __rdpq_write8(RDPQ_CMD_PUSH_RENDER_MODE, 0, 0);
}

void rdpq_mode_pop(void)
{
    __rdpq_fixup_mode(RDPQ_CMD_POP_RENDER_MODE, 0, 0);
}

/* Extern inline instantiations. */
extern inline void rdpq_set_mode_fill(color_t color);
extern inline void rdpq_mode_combiner_func(rdpq_combiner_t comb);
