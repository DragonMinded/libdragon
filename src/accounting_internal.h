/**
 * @file accounting_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal time accounting helpers (not part of the public API)
 *
 * This module implements a very low-overhead bucket-based "CPU/system time"
 * accounting using the COP0 count register (C0_COUNT).
 *
 * Key property: at any instant, time is attributed to exactly one bucket
 * (the "current category"). Interrupt handler switches category to IRQ and
 * restores it on exit, preventing double accounting (eg: IRQ time inside a
 * busy-wait loop is not counted twice).
 */
#ifndef LIBDRAGON_ACCOUNTING_INTERNAL_H
#define LIBDRAGON_ACCOUNTING_INTERNAL_H

#include <stdint.h>

/** @brief Accounting categories */
typedef enum {
    ACCT_CAT_USER         = 0,  ///< User time. This is the default category at boot.
    ACCT_CAT_IRQ          = 1,  ///< IRQ time
    ACCT_CAT_RSP          = 2,  ///< RSP time (#rsp_wait)
    ACCT_CAT_DISPLAY      = 3,  ///< Display time (#display_get)
    ACCT_CAT_RSPQ         = 4,  ///< RSPQ time (various spinwaits)
    ACCT_CAT_VI           = 5,  ///< VI time (#vi_wait_vblank)
    ACCT_CAT_JOYBUS       = 6,  ///< Joybus time (#joybus_exec)
    ACCT_CAT_PI           = 7,  ///< PI wait time (DMA and I/O)

    /** Keep this last. Buckets are addressed by index. */
    ACCT_CAT_MAX          = 8,
} acct_category_t;

/**
 * @brief Switch to a new accounting category.
 * 
 * This function switches the current accounting category to the specified
 * one, returning the previous category.
 * 
 * From this moment on, clock time will be attributed to the new category.
 * Normally, most code will want to use #ACCT_SCOPE macro instead, which
 * works as a RAII-like scope guard.
 * 
 * @param new_cat               New category to switch to
 * @return                      Previous category
 */
acct_category_t acct_switch(acct_category_t new_cat);

/**
 * @brief Create a scope that attributes time to a specific category.
 * 
 * This macro creates a scope in which time is attributed to the specified
 * category, until the scope is exited. For instance:
 * 
 * \code{.c}
 *      ACCT_SCOPE(ACCT_CAT_VI) {
 *          // time spent here is attributed to ACCT_CAT_VI
 *      }
 * \endcode
 * 
 * Internally, this macro uses #acct_switch to change category on scope entry,
 * and restores the previous category on scope exit.
 * 
 * \hideinitializer
 */
#define ACCT_SCOPE(cat) \
    for (uint8_t __acct_prev = acct_switch((uint8_t)(cat)), __acct_once = 0; \
         __acct_once == 0; \
         __acct_once = 1, acct_switch(__acct_prev))

/**
 * @brief Read the accumulated ticks for a category.
 */
uint64_t acct_get_ticks(acct_category_t cat);

#endif
