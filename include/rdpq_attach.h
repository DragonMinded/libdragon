/**
 * @file rdpq_attach.h
 * @brief RDP Command queue: surface attachment API
 * @ingroup rdp
 * 
 * This module implements a higher level API for attaching surfaces to the RDP.
 * 
 * It offers a more common lock/unlock-style API to select render targets that help
 * catching mistakes compared to the raw commands such as #rdpq_set_color_image
 * or #rdpq_sync_full. 
 * 
 * Moreover, a small render target stack is kept internally so to make it easier to
 * temporarily switch rendering to an offscreen surface, and then restore the main
 * render target.
 */

#ifndef LIBDRAGON_RDPQ_ATTACH_H
#define LIBDRAGON_RDPQ_ATTACH_H

#include "rspq.h"

/**
 * @brief Attach the RDP to a surface
 *
 * This function allows the RDP to operate on surfaces, that is memory buffers
 * that can be used as render targets. For instance, it can be used with
 * framebuffers acquired by calling #display_lock, or to render to an offscreen
 * buffer created with #surface_alloc or #surface_make.
 * 
 * This should be performed before any rendering operations to ensure that the RDP
 * has a valid output buffer to operate on. 
 * 
 * The current render target is stored away in a small stack, so that it can be
 * restored later with #rdpq_detach. This allows to temporarily switch rendering
 * to an offscreen surface, and then restore the main render target.
 * 
 * @param[in] surface
 *            The surface to render to
 *            
 * @see display_lock
 * @see surface_alloc
 */
void rdpq_attach(const surface_t *surface);

/**
 * @brief Detach the RDP from the current surface, and restore the previous one
 *
 * This function detaches the RDP from the current surface. Using a small internal
 * stack, the previous render target is restored (if any).
 * 
 * Notice that #rdpq_detach does not wait for the RDP to finish rendering, like any
 * other rdpq function. If you need to ensure that the RDP has finished rendering,
 * either call #rspq_wait afterwards, or use the #rdpq_detach_wait function.
 * 
 * A common use case is detaching from the main framebuffer (obtained via #display_lock),
 * and then displaying it via #display_show. For this case, consider using
 * #rdpq_detach_show which basically schedules the #display_show to happen automatically
 * without blocking the CPU.
 * 
 * @see #rdpq_attach
 * @see #rdpq_detach_show
 * @see #rdpq_detach_wait
 */
inline void rdpq_detach(void)
{
    extern void rdpq_detach_cb(void (*cb)(void*), void *arg);
    rdpq_detach_cb(NULL, NULL);
}

/**
 * @brief Check if the RDP is currently attached to a surface
 * 
 * @return true if it is attached, false otherwise.
 */
bool rdpq_is_attached(void);

/**
 * @brief Detach the RDP from the current framebuffer, and show it on screen
 * 
 * This function runs a #rdpq_detach on the surface, and then schedules in
 * background for the surface to be displayed on screen after the RDP has
 * finished drawing to it.
 * 
 * The net result is similar to calling #rdpq_detach_wait and then #display_show
 * manually, but it is more efficient because it does not block the CPU. Thus,
 * if this function is called at the end of the frame, the CPU can immediately
 * start working on the next one (assuming there is a free framebuffer available).
 * 
 * @see #rdpq_detach_wait
 * @see #display_show
 */
void rdpq_detach_show(void);

/**
 * @brief Detach the RDP from the current surface, waiting for RDP to finish drawing.
 *
 * This function is similar to #rdpq_detach, but also waits for the RDP to finish
 * drawing to the surface.
 * 
 * @see #rdpq_detach
 */
inline void rdpq_detach_wait(void)
{
    rdpq_detach();
    rspq_wait();
}

/**
 * @brief Detach the RDP from the current surface, and call a callback when
 *        the RDP has finished drawing to it.
 *
 * This function is similar to #rdpq_detach: it does not block the CPU, but
 * schedules for a callback to be called (under interrupt) when the RDP has
 * finished drawing to the surface.
 * 
 * @param[in] cb
 *            Callback that will be called when the RDP has finished drawing to the surface.
 * @param[in] arg
 *            Argument to the callback.
 *            
 * @see #rdpq_detach
 */
void rdpq_detach_cb(void (*cb)(void*), void *arg);

/**
 * @brief Get the surface that is currently attached to the RDP
 * 
 * @return A pointer to the surface that is currently attached to the RDP,
 *         or NULL if none is attached.
 * 
 * @see #rdpq_attach
 */
const surface_t* rdpq_get_attached(void);

#endif /* LIBDRAGON_RDPQ_ATTACH_H */
