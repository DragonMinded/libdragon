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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Attach the RDP to a color surface (and optionally a Z buffer)
 *
 * This function configures the new render targets the RDP will draw to. It accepts
 * both a color buffer and optionally a Z buffer, both of which in terms of
 * surface_t pointers.
 * 
 * For instance, it can be used with framebuffers acquired by calling #display_get,
 * or to render to an offscreen buffer created with #surface_alloc or #surface_make.
 * 
 * This function should be called before any rendering operations to ensure that the RDP
 * has a valid render target to operate on. It also resets the scissor rectangle
 * to match the buffer being passed, so that the whole buffer will be writable
 * after attaching to it.
 * 
 * The previous render targets are stored away in a small stack, so that they can be
 * restored later when #rdpq_detach is called. This allows to temporarily switch
 * rendering to an offscreen surface, and then restore the main render target.
 * 
 * @param[in] surf_color
 *            The surface to render to. Supported formats are: #FMT_RGBA32, #FMT_RGBA16,
 *            #FMT_CI8, #FMT_I8.
 * @param[in] surf_z
 *            The Z-buffer to render to (can be NULL if no Z-buffer is required).
 *            The only supported format is #FMT_RGBA16.
 *            
 * @see #display_get
 * @see #surface_alloc
 */
void rdpq_attach(const surface_t *surf_color, const surface_t *surf_z);

/**
 * @brief Attach the RDP to a surface and clear it
 *
 * This function is similar to #rdpq_attach, but it also clears the surface
 * to full black (color 0) immediately after attaching. If a z-buffer is
 * specified, it is also cleared (to 0xFFFC).
 * 
 * This function is just a shortcut for calling #rdpq_attach, #rdpq_clear and
 * #rdpq_clear_z.
 * 
 * @param[in] surf_color
 *            The surface to render to.
 * @param[in] surf_z
 *            The Z-buffer to render to (can be NULL if no Z-buffer is required).
 *            
 * @see #display_get
 * @see #surface_alloc
 * @see #rdpq_clear
 * @see #rdpq_clear_z
 */
void rdpq_attach_clear(const surface_t *surf_color, const surface_t *surf_z);

/**
 * @brief Clear the current render target with the specified color.
 * 
 * Note that this function will respect the current scissor rectangle, if
 * configured.
 * 
 * @param[in] color
 *            Color to use to clear the surface
 */
inline void rdpq_clear(color_t color) {
    extern void __rdpq_clear(const color_t *color);
    __rdpq_clear(&color);
}

/**
 * @brief Reset the current Z buffer to a given value.
 * 
 * This function clears the Z-buffer with the specified packed 16-bit value. This
 * value is composed as follows:
 * 
 * * The top 16-bit contains the Z value in a custom floating point format.
 * * The bottom 2-bits (plus the 2 hidden bits) contain the Delta-Z value. The
 *   Delta-Z value to use while clearing does not matter in practice for
 *   normal Z buffer usages, so it can be left as 0.
 * 
 * The default value to use for clearing the Z-buffer is #ZBUF_MAX. To set the
 * clear value to a custom Z value, use the #ZBUF_VAL macro.
 * 
 * Note that this function will respect the current scissor rectangle, if
 * configured.
 * 
 * @param[in] z
 *            Value to reset the Z buffer to
 */
inline void rdpq_clear_z(uint16_t z) {
    extern void __rdpq_clear_z(const uint16_t *z);
    __rdpq_clear_z(&z);
}

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
 * A common use case is detaching from the main framebuffer (obtained via #display_get),
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

#ifdef __cplusplus
}
#endif

#endif /* LIBDRAGON_RDPQ_ATTACH_H */
