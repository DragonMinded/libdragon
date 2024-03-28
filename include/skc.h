/**
 * @file skc.h
 * @brief SKC - Secure Kernel Calls
 * @ingroup bb
 * 
 * This module defines the interfaces to do the secure kernel calls (SKC)
 * on the iQue Player. These are basically syscalls into the kernel to 
 * perform security-sensitive operations like cryptography, launching
 * an application or so on.
 */

/**
 * @defgroup bb iQue Player
 * @ingroup libdragon
 * @brief Modules to interact with the iQue Player hardware
 */

#ifndef LIBDRAGON_BB_SKC_H
#define LIBDRAGON_BB_SKC_H

#include <stdint.h>

/**
 * @brief Retrieve the console unique ID
 * 
 * @param id        Will be filled with the console unique ID 
 * @return int      0 on success, negative on failure
 */
int skc_get_id(uint32_t *id);

/**
 * @brief Exit the application immediately, jumping back to iQue OS.
 */
void skc_exit(void) __attribute__((noreturn));

/**
 * @brief Reset the watchdog timer to keep the application alive
 * 
 * On iQue, applications running as trials are expected to call this function
 * to keep themselves alive. Otherwise, the secure kernel (via the kernel
 * timer) might reset them.
 */
void skc_keep_alive(void);

#endif
