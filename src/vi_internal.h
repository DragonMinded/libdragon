#ifndef __LIBDRAGON_VI_INTERNAL_H
#define __LIBDRAGON_VI_INTERNAL_H

#include "vi.h"

/**
 * @brief Install a custom line handler
 * 
 * This is an advanced function that installs a custom callback that will be
 * run at the beginning of a specific screen line.
 * 
 * The main usage of this function is to implement raster effects, that is,
 * changing VI configuration at the beginning of a specific raster line. This
 * is extremely finnicky and timing sensitive: you need to be sure to modify
 * the VI before the horizontal blank period is finished, and that period
 * is extremely short. So normally, you'll want to install the handler the
 * line before, spin-wait in the handler until the next line begins, and
 * only then directly write to the VI registers. In this case, you can also use
 * #vi_stabilize to ensure that registers are automatically reset at the
 * beginning of a frame, if needed.
 * 
 * @note Do *NOT* use this function to implement a frame buffer switch. If you
 *       just want to switch framebuffers at the beginning of a frame, use
 *       #vi_install_vblank_handler instead, as it is far less time-sensitive,
 *       and also more efficient.
 * 
 * @param line          Line number to install the handler for. Use 0 for vblank.
 *                      Use -1 to remove the couple handler+arg from all lines
 *                      they were installed for.
 * @param handler       Function to call at the beginning of the line
 * @param arg           Argument to pass to the handler
 */
void vi_set_line_interrupt(int line, void (*handler)(void*), void *arg);

/**
 * @brief Force the end of any batched VI calls
 * 
 * This can be useful in emergency situations to unblock the VI. For instance,
 * an assertion that triggers within vi_write_begin() can be unblocked by this
 * call.
 */
void vi_write_end_forced(void);

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif
