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

///@cond
typedef struct {
    void *ticket;
    void *ticket_certs[5];
    void *ticket_cmd[5];
} bb_ticket_bundle_t;
///@endcond

/**
 * @brief Retrieve the console unique ID
 * 
 * @param id        Will be filled with the console unique ID 
 * @return int      0 on success, negative on failure
 */
int skc_get_id(uint32_t *id);

/**
 * @brief Launch an application, performing some verifications
 * 
 * @param address   Entrypoint of the application in RDRAM
 * @return int      -1 on error, or if the application exits and returns.
 */
int skc_launch(void *address);

/**
 * @brief Prepare SK for launching an application. 
 * 
 * @param bundle            TBC
 * @param crls              TBC
 * @param recrypt_list      TBC
 * @return int              Negative numbers for errors, 0 on success
 */
int skc_launch_setup(bb_ticket_bundle_t *bundle, void *crls, void *recrypt_list);

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
