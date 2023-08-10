/**
 * @file rdpq_attach.c
 * @brief RDP Command queue: surface attachment API
 * @ingroup rdp
 */

#include "rdpq.h"
#include "rdpq_mode.h"
#include "rdpq_rect.h"
#include "rdpq_attach.h"
#include "rdpq_internal.h"
#include "rspq.h"
#include "display.h"
#include "debug.h"

/** @brief Size of the internal stack of attached surfaces */
#define ATTACH_STACK_SIZE   4

static const surface_t* attach_stack[ATTACH_STACK_SIZE][2] = { { NULL, NULL } };
static int attach_stack_ptr = 0;

bool rdpq_is_attached(void)
{
    return attach_stack_ptr > 0;
}

static void attach(const surface_t *surf_color, const surface_t *surf_z, bool clear_clr, bool clear_z)
{
    assertf(attach_stack_ptr < ATTACH_STACK_SIZE, "Too many nested attachments");

    attach_stack[attach_stack_ptr][0] = surf_color;
    attach_stack[attach_stack_ptr][1] = surf_z;
    attach_stack_ptr++;

    if (clear_clr || clear_z)
        rdpq_mode_push();

    if (surf_z) {
        assertf(surf_z-> width == surf_color->width && surf_z->height == surf_color->height,
            "Color and Z buffers must have the same size");
        
        if (clear_z) {
            rdpq_set_color_image(surf_z);
            rdpq_set_mode_fill(color_from_packed16(0xFFFC));
            rdpq_fill_rectangle(0, 0, surf_z->width, surf_z->height);
        }
    }
    rdpq_set_z_image(surf_z);

    if (clear_clr) {
        rdpq_set_color_image(surf_color);
        rdpq_set_mode_fill(color_from_packed32(0x000000FF));
        rdpq_fill_rectangle(0, 0, surf_color->width, surf_color->height);
    }
    rdpq_set_color_image(surf_color);

    if (clear_clr || clear_z)
        rdpq_mode_pop();
}

static void detach(void)
{
    const surface_t *color = NULL, *z = NULL;

    // Reattach to the previous surface in the stack (if any)
    attach_stack_ptr--;
    if (attach_stack_ptr > 0) {
        color = attach_stack[attach_stack_ptr-1][0];
        z = attach_stack[attach_stack_ptr-1][1];
    }
    rdpq_set_z_image(z);
    rdpq_set_color_image(color);
    rspq_flush();
}

void rdpq_attach(const surface_t *surf_color, const surface_t *surf_z)
{
    assertf(__rdpq_inited, "rdpq not initialized: please call rdpq_init()");
    attach(surf_color, surf_z, false, false);
}

void rdpq_attach_clear(const surface_t *surf_color, const surface_t *surf_z)
{
    attach(surf_color, surf_z, true, true);
}

/** @brief Like #rdpq_clear, but with optional fill color configuration */
void __rdpq_clear(const color_t *clr)
{
    extern void __rdpq_set_mode_fill(void);
    assertf(rdpq_is_attached(), "No render target is currently attached");

    rdpq_mode_push();
        __rdpq_set_mode_fill();
        if (clr) rdpq_set_fill_color(*clr);
        rdpq_fill_rectangle(0, 0, attach_stack[attach_stack_ptr-1][0]->width, attach_stack[attach_stack_ptr-1][0]->height);
    rdpq_mode_pop();
}

/** @brief Like #rdpq_clear_z, but with optional fill z value configuration */
void __rdpq_clear_z(const uint16_t *z)
{
    extern void __rdpq_set_mode_fill(void);
    assertf(rdpq_is_attached(), "No render target is currently attached");

    const surface_t *surf_z = attach_stack[attach_stack_ptr-1][1];
    assertf(surf_z, "No Z buffer is currently attached");

    // Disable autoscissor, so that when we attach to the Z buffer, we 
    // keep the previous scissor rect. This is probably expected by the user
    // for symmetry with rdpq_clear that does respect the scissor rect.
    uint32_t old_cfg = rdpq_config_disable(RDPQ_CFG_AUTOSCISSOR);
    attach(surf_z, NULL, false, false);
        rdpq_mode_push();
            __rdpq_set_mode_fill();
            if (z) rdpq_set_fill_color(color_from_packed16(*z));
            rdpq_fill_rectangle(0, 0, surf_z->width, surf_z->height);
        rdpq_mode_pop();
    // Use detach instead of rdpq_detach to avoid issuing a full sync here.
    // TODO: Once we've proven that the RDP doesn't require a full sync for reusing an offscreen framebuffer as a texture, 
    //       remove the full sync from rdpq_detach and change this back (don't forget the call to "attach" above).
    detach();
    rdpq_config_set(old_cfg);
}

void rdpq_detach_cb(void (*cb)(void*), void *arg)
{
    assertf(rdpq_is_attached(), "No render target is currently attached");

    rdpq_sync_full(cb, arg);
    detach();
}

void rdpq_detach_show(void)
{
    assertf(rdpq_is_attached(), "No render target is currently attached");
    rdpq_detach_cb((void (*)(void*))display_show, (void*)attach_stack[attach_stack_ptr-1][0]);
}

const surface_t* rdpq_get_attached(void)
{
    if (rdpq_is_attached()) {
        return attach_stack[attach_stack_ptr-1][0];
    } else {
        return NULL;
    }
}

/* Extern inline instantiations. */
extern inline void rdpq_clear(color_t color);
extern inline void rdpq_clear_z(uint16_t z);
extern inline void rdpq_detach(void);
extern inline void rdpq_detach_wait(void);
