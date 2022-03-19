/**
 * @file timer.h
 * @brief Timer Subsystem
 * @ingroup timer
 */
#ifndef __LIBDRAGON_TIMER_H
#define __LIBDRAGON_TIMER_H

#include <stdint.h>
#include "n64sys.h"

/** 
 * @addtogroup timer
 * @{
 */

/** @brief Timer callback function without context */
typedef void (*timer_callback1_t)(int ovfl);
/** @brief Timer callback function with context */
typedef void (*timer_callback2_t)(int ovfl, void *ctx);

/**
 * @brief Timer structure
 */
typedef struct timer_link
{
    /** @brief Absolute ticks value at which the timer expires. */
    uint32_t left;
    /** @brief Ticks to set if continuous */
    uint32_t set;
    /** @brief To correct for drift */
    int ovfl;
    /** @brief Timer flags.  See #TF_ONE_SHOT, #TF_CONTINUOUS, and #TF_DISABLED */
    int flags;
    /** @brief Callback function to call when timer fires */
    union {
        timer_callback1_t callback;
        timer_callback2_t callback_with_context;
    };
    /** @brief Callback context parameter */
    void *ctx;
    /** @brief Link to next timer */
    struct timer_link *next;
} timer_link_t;

/** @brief Timer should fire only once */
#define TF_ONE_SHOT   0
/** @brief Timer should fire at a regular interval */
#define TF_CONTINUOUS 1
/** @brief Timer is enabled or not. Can be used to get a new timer that's not started. */
#define TF_DISABLED 2

/** 
 * @brief Calculate timer ticks based on microseconds 
 *
 * @param[in] us
 *            Microseconds to convert to ticks
 *
 * @return Timer ticks
 */
#define TIMER_TICKS(us) ((int)TIMER_TICKS_LL(us))
/**
 * @brief Calculate microseconds based on timer ticks
 *
 * @param[in] tk
 *            Ticks to convert to microseconds
 *
 * @return Microseconds
 */
#define TIMER_MICROS(tk) ((int)TIMER_MICROS_LL(tk))

/** 
 * @brief Calculate timer ticks based on microseconds 
 *
 * @param[in] us
 *            Microseconds to convert to ticks
 *
 * @return Timer ticks as a long long
 */
#define TIMER_TICKS_LL(us) ((long long)(us) * TICKS_PER_SECOND / 1000000)
/**
 * @brief Calculate microseconds based on timer ticks
 *
 * @param[in] tk
 *            Ticks to convert to microseconds
 *
 * @return Microseconds as a long long
 */
#define TIMER_MICROS_LL(tk) ((long long)(tk) * 1000000 / TICKS_PER_SECOND)

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/* initialize timer subsystem */
void timer_init(void);
/* delete all timers in list */
void timer_close(void);
/* return total ticks since timer was initialized */
long long timer_ticks(void);

/* create a new timer and add to list */
timer_link_t *new_timer(int ticks, int flags, timer_callback1_t callback);
/* create a new timer and add to list */
timer_link_t *new_timer_context(int ticks, int flags, timer_callback2_t callback, void *ctx);

/* start a timer not currently in the list */
void start_timer(timer_link_t *timer, int ticks, int flags, timer_callback1_t callback);
/* start a timer not currently in the list */
void start_timer_context(timer_link_t *timer, int ticks, int flags, timer_callback2_t callback, void *ctx);

/* reset a timer and add to list */
void restart_timer(timer_link_t *timer);
/* remove a timer from the list */
void stop_timer(timer_link_t *timer);
/* remove a timer from the list and delete it */
void delete_timer(timer_link_t *timer);

#ifdef __cplusplus
}
#endif

#endif
