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
 */

/**
 * @mainpage libdragon
 *
 * libdragon provides a low level API for all hardware features of the N64. 
 * Currently, it provides basic console support, rudimentary higher level controller functions, 
 * a read only filesystem for appending data to the end of a rom, audio support and some 2D graphics 
 * functionality. It also includes bindings to newlib in order to provide a posix interface 
 * to filesystems.
 *
 * libdragon is currently made up of three core components: the @ref libdragon library, @ref system and
 * @ref dfs.  The core library provides drivers for each hardware component in the N64 and low level API
 * for interacting with the components.  The system hooks provide a translation layer between libdragon
 * and newlib for the purpose of POSIX compatibility.  DragonFS provides the read only filesystem support
 * and is hooked into newlib using the system hooks.
 *
 * Please see the documentation in individual modules for additional information.
 */

/* Easy include wrapper */
#include "audio.h"
#include "console.h"
#include "controller.h"
#include "display.h"
#include "dma.h"
#include "dragonfs.h"
#include "graphics.h"
#include "interrupt.h"
#include "n64sys.h"
#include "rdp.h"
#include "timer.h"
#include "exception.h"
#include "dir.h"

#endif
