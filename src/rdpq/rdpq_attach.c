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

static bool __rdpq_clear_z_with_rsp(const surface_t *surf_z, uint16_t zvalue)
{
    static uint8_t temp_buffer[1280] __attribute__((aligned(16)));

    int nbytes = surf_z->height * surf_z->stride;
    uint32_t rsp_size;
    if ((nbytes & 0xFFF) == 0) 
        rsp_size = (((nbytes >> 12) - 1) << 12) | 0xFFF;
    else if ((nbytes & 0x7FF) == 0)
        rsp_size = (((nbytes >> 11) - 1) << 12) | 0x7FF;
    else if ((nbytes & 0x3FF) == 0)
        rsp_size = (((nbytes >> 10) - 1) << 12) | 0x3FF;
    else if ((nbytes & 0x1FF) == 0)
        rsp_size = (((nbytes >>  9) - 1) << 12) | 0x1FF;
    else if ((nbytes & 0x0FF) == 0)
        rsp_size = (((nbytes >>  8) - 1) << 12) | 0x0FF;
    else
        return false;

    // We need a RDP fence here because the RDP might be drawing to the Z buffer
    // at this point. So first force the RSP to wait for the RDP to finish.
    rdpq_fence();
    rspq_write(RDPQ_OVL_ID, RDPQ_CMD_CLEAR_ZBUFFER, 
        PhysicalAddr(surf_z->buffer), rsp_size, PhysicalAddr(temp_buffer), zvalue);
    return true;
}

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
            if (!__rdpq_clear_z_with_rsp(surf_z, ZBUF_MAX)) {
                rdpq_set_color_image(surf_z);
                rdpq_set_mode_fill(color_from_packed16(ZBUF_MAX));
                rdpq_fill_rectangle(0, 0, surf_z->width, surf_z->height);
            }
        }
    }

    rdpq_set_z_image(surf_z);
    rdpq_set_color_image(surf_color);

    if (clear_clr) {
        rdpq_set_mode_fill(color_from_packed32(0x000000FF));
        rdpq_fill_rectangle(0, 0, surf_color->width, surf_color->height);
    }

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

    // Try first to clear the Z buffer using the RSP. It should always be
    // possible unless the surface has some very unexpected size.
    if (z && __rdpq_clear_z_with_rsp(surf_z, *z))
        return;

    // Disable autoscissor, so that when we attach to the Z buffer, we 
    // keep the previous scissor rect. This is probably expected by the user
    // for symmetry with rdpq_clear that does respect the scissor rect.
    uint32_t old_cfg = rdpq_config_disable(RDPQ_CFG_AUTOSCISSOR);
    rdpq_attach(surf_z, NULL);
        rdpq_mode_push();
            __rdpq_set_mode_fill();
            if (z) rdpq_set_fill_color(color_from_packed16(*z));
            rdpq_fill_rectangle(0, 0, surf_z->width, surf_z->height);
        rdpq_mode_pop();
    rdpq_detach();
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
