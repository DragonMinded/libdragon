/**
 * @file kirq.h
 * @brief Kernel IRQ Wait Functions
 * @ingroup kernel
 * 
 * This module contains functions that allow to wait for a specific interrupt
 * to be triggered, while yielding execution to other threads.
 * 
 * This can be useful for all situations where blocking code is waiting for the
 * hardware to perform some operations. Normally, the hardware reports back
 * completion of the operation by triggering an interrupt, so it can be useful
 * to use the CPU time to execute other threads while waiting for the interrupt.
 * 
 * All functions are designed to be nops when the kernel is not initialized,
 * so that the spin loops will also work without multithreading (they just
 * won't be able to yield).
 */

#ifndef LIBDRAGON_KERNEL_KIRQ_H
#define LIBDRAGON_KERNEL_KIRQ_H

#include <stdint.h>

///@cond
typedef struct kcond_s kcond_t;
///@endcond

/** @brief Kirq waiting structure 
 * 
 * Use on of the kirq_begin_wait functions to obtain an object of this
 * type, and then pass it to kirq_wait to wait for the interrupt (one or
 * multiple times).
 */
typedef struct {
    int64_t counter;
    kcond_t *cond;
} kirq_wait_t;

/**
 * @brief Create an kirq wait object for SP interrupts
 * 
 * @return kirq_wait_t      The wait object
 */
kirq_wait_t kirq_begin_wait_sp(void);

/**
 * @brief Create an kirq wait object for DP interrupts
 * 
 * @return kirq_wait_t      The wait object
 */
kirq_wait_t kirq_begin_wait_dp(void);

/**
 * @brief Create an kirq wait object for SI interrupts
 * 
 * @return kirq_wait_t      The wait object
 */
kirq_wait_t kirq_begin_wait_si(void);

/**
 * @brief Create an kirq wait object for AI interrupts
 * 
 * @return kirq_wait_t      The wait object
 */
kirq_wait_t kirq_begin_wait_ai(void);

/**
 * @brief Create an kirq wait object for VI interrupts
 * 
 * @return kirq_wait_t      The wait object
 */
kirq_wait_t kirq_begin_wait_vi(void);

/**
 * @brief Create an kirq wait object for PI interrupts
 * 
 * @return kirq_wait_t      The wait object
 */
kirq_wait_t kirq_begin_wait_pi(void);

/**
 * @brief Wait until the interrupt is triggered
 * 
 * This function will block the current thread until the interrupt
 * is triggered. It can be called multiple times on the same wait object,
 * to wait for further interrupts, and it guarantees that none of them
 * will be missed.
 * 
 * @param wait 
 */
void kirq_wait(kirq_wait_t *wait);

#endif
