#ifndef __LIBDRAGON_TIMER_H
#define __LIBDRAGON_TIMER_H

typedef struct timer_link
{
	int left;							// ticks left until callback
	int set;							// ticks to set if continuous
	int ovfl;							// to correct for drift
	int flags;							// flags
    void (*callback)(int ovfl);			// callback function
    struct timer_link *next;
} _timer_link;

#define TF_ONE_SHOT   0
#define TF_CONTINUOUS 1

#define TIMER_TICKS(us) ((int)((long)(us) * 46875LL / 1000LL))
#define TIMER_MICROS(tk) ((int)((long)(tk) * 1000LL / 46875LL))

#ifdef __cplusplus
extern "C" {
#endif

/* initialize timer subsystem */
void timer_init(void);
/* create a new timer and add to list */
_timer_link *new_timer(int ticks, int flags, void (*callback)(int ovfl));
/* start a timer not currently in the list */
void start_timer(_timer_link *timer, int ticks, int flags, void (*callback)(int ovfl));
/* remove a timer from the list */
void stop_timer(_timer_link *timer);
/* remove a timer from the list and delete it */
void delete_timer(_timer_link *timer);
/* delete all timers in list */
void timer_close(void);
/* return total ticks since timer was initialized */
long timer_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
