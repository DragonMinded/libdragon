/**
 * @file rdpq_attach.c
 * @brief RDP Command queue: surface attachment API
 * @ingroup rdp
 */

#include "rdpq.h"
#include "rdpq_attach.h"
#include "debug.h"

/** @brief Size of the internal stack of attached surfaces */
#define ATTACH_STACK_SIZE   4

static surface_t* attach_stack[ATTACH_STACK_SIZE] = { NULL };
static int attach_stack_ptr = 0;

bool rdpq_is_attached(void)
{
    return attach_stack_ptr > 0;
}

void rdpq_attach(surface_t *surface)
{
    assertf(!rdpq_is_attached(), "A render target is already attached");
    assertf(attach_stack_ptr < ATTACH_STACK_SIZE, "Too many nested attachments");

    attach_stack[attach_stack_ptr++] = surface;
    rdpq_set_color_image(surface);
}

void rdpq_detach_cb(void (*cb)(void*), void *arg)
{
    assertf(rdpq_is_attached(), "No render target is currently attached");

    rdpq_sync_full(cb, arg);

    // Reattach to the previous surface in the stack (if any)
    attach_stack_ptr--;
    if (attach_stack_ptr > 0)
        rdpq_set_color_image(attach_stack[attach_stack_ptr-1]);

    rspq_flush();
}

void rdpq_detach_show(void)
{
    assertf(rdpq_is_attached(), "No render target is currently attached");
    rdpq_detach_cb((void (*)(void*))display_show, attach_stack[attach_stack_ptr-1]);
}

extern inline void rdpq_detach(void);
extern inline void rdpq_detach_wait(void);
