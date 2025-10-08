/**
 * @file interrupt_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal APIs for interrupt handling
 * 
 * This module provides a low-level, faster alternative to the
 * historical #disable_interrupts and #enable_interrupts functions.
 * 
 * Since their usage is different from the public API, we can't replace
 * them directly, but we can still use them internally to speed up performance
 * sensitive code paths.
 */
#ifndef INTERRUPT_INTERNAL_H
#define INTERRUPT_INTERNAL_H

#include "cop0.h"

/**
 * @brief Disable interrupts
 * 
 * This is similar to #disable_interrupts. It does return a status
 * value that must be passed to #__enable_interrupts to restore
 * the previous interrupt state.
 * 
 * @result An opaque value to pass to #__enable_interrupts
 */
__attribute__((always_inline, warn_unused_result)) 
static inline uint32_t __disable_interrupts(void)
{
    uint32_t sr = C0_STATUS();
    C0_WRITE_STATUS(sr & ~C0_STATUS_IE);
    return sr;
}

/**
 * @brief Reenable interrupts
 * 
 * This is similar to #enable_interrupts. If called from a nested
 * disable/enable pair, it will have no effect on the interrupt
 * state.
 *
 * @param sr    The opaque value returned by #__disable_interrupts
 */
__attribute__((always_inline)) 
static inline void __enable_interrupts(uint32_t sr)
{
    C0_WRITE_STATUS(sr);
}

#endif
