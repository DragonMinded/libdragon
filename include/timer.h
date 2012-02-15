/**
 * @file timer.h
 * @brief Timer Subsystem
 * @ingroup timer
 */
#ifndef __LIBDRAGON_TIMER_H
#define __LIBDRAGON_TIMER_H

/** 
 * @addtogroup timer
 * @{
 */

/**
 * @brief Timer structure
 */
typedef struct timer_link
{
    /** @brief Ticks left until callback */
    int left;
    /** @brief Ticks to set if continuous */
    int set;
    /** @brief To correct for drift */
    int ovfl;
    /** @brief Timer flags.  See #TF_ONE_SHOT and #TF_CONTINUOUS */
    int flags;
    /** @brief Callback function to call when timer fires */
    void (*callback)(int ovfl);
    /** @brief Link to next timer */
    struct timer_link *next;
} timer_link_t;

/** @brief Timer should fire only once */
#define TF_ONE_SHOT   0
/** @brief Timer should fire at a regular interval */
#define TF_CONTINUOUS 1

/** 
 * @brief Calculate timer ticks based on microseconds 
 *
 * @param[in] us
 *            Microseconds to convert to ticks
 *
 * @return Timer ticks
 */
#define TIMER_TICKS(us) ((int)((long long)(us) * 46875LL / 1000LL))
/**
 * @brief Calculate microseconds based on timer ticks
 *
 * @param[in] tk
 *            Ticks to convert to microseconds
 *
 * @return Microseconds
 */
#define TIMER_MICROS(tk) ((int)((long long)(tk) * 1000LL / 46875LL))

/** 
 * @brief Calculate timer ticks based on microseconds 
 *
 * @param[in] us
 *            Microseconds to convert to ticks
 *
 * @return Timer ticks as a long long
 */
#define TIMER_TICKS_LL(us) ((long long)(us) * 46875LL / 1000LL)
/**
 * @brief Calculate microseconds based on timer ticks
 *
 * @param[in] tk
 *            Ticks to convert to microseconds
 *
 * @return Microseconds as a long long
 */
#define TIMER_MICROS_LL(tk) ((long long)(tk) * 1000LL / 46875LL)

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/* initialize timer subsystem */
void timer_init(void);
/* create a new timer and add to list */
timer_link_t *new_timer(int ticks, int flags, void (*callback)(int ovfl));
/* start a timer not currently in the list */
void start_timer(timer_link_t *timer, int ticks, int flags, void (*callback)(int ovfl));
/* remove a timer from the list */
void stop_timer(timer_link_t *timer);
/* remove a timer from the list and delete it */
void delete_timer(timer_link_t *timer);
/* delete all timers in list */
void timer_close(void);
/* return total ticks since timer was initialized */
long long timer_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
