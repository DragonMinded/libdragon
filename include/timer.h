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
 * @defgroup timer Timer Subsystem
 * @ingroup libdragon
 * @brief Interface to the timer module in the MIPS r4300 processor.
 *
 * The timer subsystem allows code to receive a callback after a specified
 * number of ticks or microseconds.  It interfaces with the MIPS
 * coprocessor 0 to handle the timer interrupt and provide useful timing
 * services.
 *
 * Before attempting to use the timer subsystem, code should call #timer_init.
 * After the timer subsystem has been initialized, a new one-shot or
 * continuous timer can be created with #new_timer.  To remove an expired
 * one-shot timer or a recurring timer, use #delete_timer.  To temporarily
 * stop a timer, use #stop_timer.  To restart a stopped timer or an expired
 * one-shot timer, use #start_timer.  Once code no longer needs the timer
 * subsystem, a call to #timer_close will free all continuous timers and shut
 * down the timer subsystem.  Note that timers removed with #stop_timer or
 * expired one-short timers will not be removed automatically and are the
 * responsibility of the calling code to be freed, regardless of a call to
 * #timer_close.
 *
 * Because the MIPS internal counter wraps around after ~90 seconds (see
 * TICKS_READ), it's not possible to schedule a timer more than 90 seconds
 * in the future.
 *
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

/**
 * @brief Initialize the timer subsystem
 *
 * This function will reset the COP0 ticks counter to 0. Even if you
 * later access the hardware counter directly (via TICKS_READ()), it should not
 * be a problem if you call timer_init() early in the application main.
 *
 * Do not modify the COP0 ticks counter after calling this function. Doing so
 * will impede functionality of the timer module.
 * 
 * The timer subsystem tracks the number of times #timer_init is called
 * and will only initialize the subsystem on the first call. This reference
 * count also applies to #timer_close, which will only close the subsystem
 * if it is called the same number of times as #timer_init.
 */
void timer_init(void);

/**
 * @brief Free and close the timer subsystem
 *
 * This function will ensure all recurring timers are deleted from the list 
 * before closing.  One-shot timers that have expired will need to be
 * manually deleted with #delete_timer.
 * 
 * The timer subsystem tracks the number of times #timer_init is called
 * and will only close the subsystem if #timer_close is called the same
 * number of times.
 */
void timer_close(void);

/**
 * @brief Return total ticks since timer was initialized, as a 64-bit counter.
 *
 * @return Then number of ticks since the timer was initialized
 *
 */
long long timer_ticks(void);


/**
 * @brief Create a new timer and add to list
 * 
 * If you need to associate some data with the timer, consider using
 * #new_timer_context to include a pointer in the callback.
 *
 * @param[in] ticks
 *            Number of ticks before the timer should fire
 * @param[in] flags
 *            Timer flags.  See #TF_ONE_SHOT, #TF_CONTINUOUS and #TF_DISABLED
 * @param[in] callback
 *            Callback function to call when the timer expires
 *
 * @return A pointer to the timer structure created
 */
timer_link_t *new_timer(int ticks, int flags, timer_callback1_t callback);

/**
 * @brief Create a new timer with context and add to list
 * 
 * If you don't need the context, consider using #new_timer instead.
 *
 * @param[in] ticks
 *            Number of ticks before the timer should fire
 * @param[in] flags
 *            Timer flags.  See #TF_ONE_SHOT, #TF_CONTINUOUS and #TF_DISABLED
 * @param[in] callback
 *            Callback function to call when the timer expires
 * @param[in] ctx
 * 			  Opaque pointer to pass as an argument to callback
 *
 * @return A pointer to the timer structure created
 */
timer_link_t *new_timer_context(int ticks, int flags, timer_callback2_t callback, void *ctx);


/**
 * @brief Start a timer (not currently in the list)
 * 
 * If you need to associate some data with the timer, consider using
 * #start_timer_context to include a pointer in the callback.
 *
 * @param[in] timer
 *            Pointer to timer structure to reinsert and start
 * @param[in] ticks
 *            Number of ticks before the timer should fire
 * @param[in] flags
 *            Timer flags.  See #TF_ONE_SHOT, #TF_CONTINUOUS, and #TF_DISABLED
 * @param[in] callback
 *            Callback function to call when the timer expires
 */
void start_timer(timer_link_t *timer, int ticks, int flags, timer_callback1_t callback);

/**
 * @brief Start a timer (not currently in the list) with context
 * 
 * If you don't need the context, consider using #start_timer instead.
 *
 * @param[in] timer
 *            Pointer to timer structure to reinsert and start
 * @param[in] ticks
 *            Number of ticks before the timer should fire
 * @param[in] flags
 *            Timer flags.  See #TF_ONE_SHOT, #TF_CONTINUOUS, and #TF_DISABLED
 * @param[in] callback
 *            Callback function to call when the timer expires
 * @param[in] ctx
 *            Opaque pointer to pass as an argument to callback
 */
void start_timer_context(timer_link_t *timer, int ticks, int flags, timer_callback2_t callback, void *ctx);


/**
 * @brief Reset a timer and add to list
 *
 * @param[in] timer
 *            Pointer to timer structure to reinsert and start
 */
void restart_timer(timer_link_t *timer);

/**
 * @brief Stop a timer and remove it from the list
 *
 * @note This function does not free a timer structure, use #delete_timer
 *       to do this.
 * 
 * @note It is safe to call this function from a timer callback, including
 *       to stop a timer from its own callback.
 *
 * @param[in] timer
 *            Timer structure to stop and remove
 */
void stop_timer(timer_link_t *timer);

/**
 * @brief Remove a timer from the list and delete it
 *
 * @note It is not safe to call this function from a timer callback.

 * @param[in] timer
 *            Timer structure to stop, remove and free
 */
void delete_timer(timer_link_t *timer);

#ifdef __cplusplus
}
#endif

#endif
