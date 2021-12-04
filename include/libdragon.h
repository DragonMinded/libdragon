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
 *
 * libdragon makes every effort to be self-sufficient and self-configured.  However,
 * when operating in unexpected environments such as with a non-6102 CIC, additional
 * setup may be required.  Please see the documentation for #sys_set_boot_cic to
 * inform libdragon of a nonstandard CIC.
 */

/* Easy include wrapper */
#include "audio.h"
#include "console.h"
#include "debug.h"
#include "joybus.h"
#include "controller.h"
#include "rtc.h"
#include "mempak.h"
#include "tpak.h"
#include "display.h"
#include "dma.h"
#include "dragonfs.h"
#include "eeprom.h"
#include "eepromfs.h"
#include "graphics.h"
#include "interrupt.h"
#include "n64sys.h"
#include "rdp.h"
#include "rsp.h"
#include "timer.h"
#include "exception.h"
#include "dir.h"
#include "mixer.h"
#include "samplebuffer.h"
#include "wav64.h"
#include "xm64.h"
#include "ym64.h"

#endif
