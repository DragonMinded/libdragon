#ifndef __LIBDRAGON_ASYNC_H
#define __LIBDRAGON_ASYNC_H

#include <async_.h>
#include <stdint.h>
#include <stdbool.h>


/**
 * @defgroup async Async routines
 * @ingroup libdragon
 * @brief Interface to the timer module in the MIPS r4300 processor.
 *
 * Stackless coroutine implementation based on async.h Any async task
 * scheduled via `async_schedule` is executed until completion. The system runs
 * with interrupts disabled and they are acknowledged in a tight loop without
 * any context switch. This trades some CPU overhead for added latency. Probably
 * the most critical one is audio latency as it will can create an audible click.
 * To combat this, one can simply insert async_yield througout long running
 * tasks.
 *
 * @{
 */

/**
 * @brief Async context for interrupt handlers.
 */
typedef struct {
    async_state;
} async_interrupt_ctx_t;

/**
 * @brief Initialize async routines. This basically disables interrupts system-wide
 * so the interrupts are acknowledged in a tight loop rather than going through
 * a full context switch.
 */
void async_init(void);

/**
 * @brief This is a basic async scheduler. Runs the given task till completion
 * with the given initial state.
 *
 * @param task              Task to schedule
 * @param initial_state     Initial struct with async_state to keep track of task
 * local variables.
 */
void async_schedule(async (*task)(void *st), void *initial_state);

/**
 * @brief Terminate the async system
 */
void async_close(void);

#endif

/** @} */ /* async */