/**
 * @file libdragon.h
 * @brief Main include file for programs seeking to link against libdragon
 * @ingroup libdragon
 */
#ifndef __LIBDRAGON_LIBDRAGON_H
#define __LIBDRAGON_LIBDRAGON_H

/**
 * @defgroup libdragon libdragon
 * @brief Low level runtime for homebrew development on the N64 platform.
 *
 * libdragon handles the hardware interfaces to the various systems in the N64.
 * The audio interface is handled by the @ref audio.  The controller interface,
 * controller peripheral interface and EEPROM interface are handled by the
 * @ref controller.  The display interface and RDP rasterizer are handled by
 * the @ref display.  System timers are handled by the @ref timer.
 * Low level interfaces such as interrupts, caching operations, exceptions and 
 * the DMA controller are handled by the @ref lowlevel.
 */

/* Easy include wrapper */
#include "n64types.h"
#include "fmath.h"
#include "audio.h"
#include "console.h"
#include "debug.h"
#include "joybus.h"
#include "joybus_accessory.h"
#include "pixelfx.h"
#include "joypad.h"
#include "controller.h"
#include "rtc.h"
#include "mempak.h"
#include "tpak.h"
#include "display.h"
#include "dma.h"
#include "dragonfs.h"
#include "asset.h"
#include "eeprom.h"
#include "eepromfs.h"
#include "graphics.h"
#include "interrupt.h"
#include "n64sys.h"
#include "backtrace.h"
#include "rdp.h"
#include "rsp.h"
#include "timer.h"
#include "exception.h"
#include "dir.h"
#include "mpeg2.h"
#include "throttle.h"
#include "mixer.h"
#include "samplebuffer.h"
#include "wav64.h"
#include "xm64.h"
#include "ym64.h"
#include "rspq.h"
#include "rdpq.h"
#include "rdpq_tri.h"
#include "rdpq_rect.h"
#include "rdpq_attach.h"
#include "rdpq_mode.h"
#include "rdpq_tex.h"
#include "rdpq_sprite.h"
#include "rdpq_text.h"
#include "rdpq_paragraph.h"
#include "rdpq_font.h"
#include "rdpq_debug.h"
#include "rdpq_macros.h"
#include "surface.h"
#include "sprite.h"
#include "debugcpp.h"
#include "dlfcn.h"
#include "model64.h"

#endif
