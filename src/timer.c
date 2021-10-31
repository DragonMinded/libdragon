/**
 * @file timer.c
 * @brief Timer Subsystem
 * @ingroup timer
 */
#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

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

/** @brief Internal linked list of timers */
static timer_link_t *TI_timers = 0;

/** @brief Higher-part of 64-bit tick counter */
volatile uint32_t ticks64_high;

/** @brief Time at which interrupts were disabled */
extern volatile uint32_t interrupt_disabled_tick;

/** @brief Timer is the special overflow timer. */
#define TF_OVERFLOW    0x40

/** @brief Timer has been called once in this interrupt. */
#define TF_CALLED      0x80

/** @brief Update the compare register to match the first expiring timer. */
static void timer_update_compare(timer_link_t *head) {
	uint32_t now = TICKS_READ();
	uint32_t smallest = 0xFFFFFFFF;

	while (head)
	{
		/* See how much time is left before the timer expires. Notice that
		   the subtraction is also safe with overflows. */
		uint32_t left = head->left - now;
		if (left < smallest)
			smallest = left;

		/* Go to next */
		head = head->next;
	}

	/* set compare to shortest time left */
	C0_WRITE_COMPARE(now + smallest);
}

/**
 * @brief Process linked list of timers
 *
 * Walk the linked list of timers and call the callbacks of any that
 * have expired.
 *
 * @note This function will remove one-shot timers from the list after
 *       they have fired.
 *
 * @param[in] head
 *            Head of the linked list of timers
 *
 * @retval 1 The list needs reprocessing
 * @retval 0 All timer operations were handled successfully
 */
static int __proc_timers(timer_link_t * thead)
{
	timer_link_t *head = thead;
	timer_link_t *last = 0;
	uint32_t start = C0_COMPARE();
	uint32_t now = TICKS_READ();

	while (head)
	{
		/* Consider a timer as expired if its deadline is at the
		 * COMPARE value (time at which the interrupt triggered) or up to 5
		 * microseconds after. This 5 microseconds window is useful to cluster
		 * timers that expire close to each other; eg: if the client creates
		 * many timers with the same period, they will be created in a fast
		 * sequence and have a little delay between each other. */
		if (!(head->flags & TF_CALLED) && 
			!(head->flags & TF_DISABLED) &&
			TICKS_DISTANCE(start, head->left) >= 0 && 
			TICKS_DISTANCE(head->left, now+TIMER_TICKS(5)) >= 0)
		{
			/* yes - timed out, do callback */
			head->ovfl = TICKS_DISTANCE(head->left, now);
			if (head->callback)
				head->callback(head->ovfl);

			/* reset ticks if continuous */
			if (head->flags & TF_CONTINUOUS)
			{
				head->left += head->set;
				last = head;

				/* Special case: the internal overflow timer has a period
				 * of 2**32, so next occurrence will look exactly like the
				 * current one. Since we're going to reprocess the list, we
				 * would keep executing it many times until we eventually
				 * exit the 5 microseconds window.
				 * So we mark this timer as already called (TF_CALLED) and
				 * avoid calling it again. 
				 *
				 * Notice that we do this only for overflow because other timers
				 * with a short period might actually be called multiple times
				 * under interrupt. For instance, if a continuous timer with
				 * a short period is followed by a very slow one-shot timer,
				 * when the latter is finished the former might need to fire again.
				 */
				if (head->flags & TF_OVERFLOW)
					head->flags |= TF_CALLED;
			}
			else
			{
				/* one-shot, remove from list */
				if (last)
					last->next = head->next;
				else
					TI_timers = head->next;
			}

			/* Go through timer list again. If the callback was slow, maybe
			   other timers have expired. */
			return 1;
		}

		/* Go to next */
		last = head;
		head = head->next;
	}

	/* Clear the TF_CALLED flag from the overflow timer (if any) */
	while (thead) {
		thead->flags &= ~TF_CALLED;
		thead = thead->next;
	}
	return 0;							// exit timer callback
}

/**
 * @brief Timer callback function
 *
 * This function is called by the interrupt controller whenever 
 * compare == count.
 */
static void timer_callback(void)
{
	while (__proc_timers(TI_timers))
		{}

	// Update counter for next interrupt.
	timer_update_compare(TI_timers);
}

/**
 * @brief Timer callback overflow function
 *
 * This function is the callback of the internal overflow timer, which
 * is configured by timer_init() and is used to create a 64-bit timer
 * accessed by timer_ticks().
 */
static void timer_callback_overflow(int ovfl)
{
	ticks64_high++;
}

/**
 * @brief Initialize the timer subsystem
 *
 * This function will reset the COP0 ticks counter to 0. Even if you
 * later access the hardware counter directly (via TICKS_READ()), it should not
 * be a problem if you call timer_init() early in the application main.
 *
 * Do not modify the COP0 ticks counter after calling this function. Doing so
 * will impede functionality of the timer module.
 */
void timer_init(void)
{
	assertf(!TI_timers, "timer module already initialized");
	/* Create first timer for overflows: expires when counter is 0 and
	 * has a period of 2**32. */
	timer_link_t *timer = malloc(sizeof(timer_link_t));
	if (timer)
	{
		timer->left = 0;
		timer->set = 0;
		timer->flags = TF_CONTINUOUS | TF_OVERFLOW;
		timer->callback = timer_callback_overflow;
		timer->next = NULL;

		TI_timers = timer;
	}

	/* Reset the count and compare registers. Avoid to accidentally trigger
	   an interrupt by setting count to 1 and compare to 0. Also enable
	   timer interrupts in COP0. */
	disable_interrupts();
	ticks64_high = 0;
	C0_WRITE_COUNT(1);
	C0_WRITE_COMPARE(0);
	C0_WRITE_STATUS(C0_STATUS() | C0_INTERRUPT_TIMER);
	register_TI_handler(timer_callback);
	enable_interrupts();
}

/**
 * @brief Create a new timer and add to list
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
timer_link_t *new_timer(int ticks, int flags, void (*callback)(int ovfl))
{
	assertf(TI_timers, "timer module not initialized");
	timer_link_t *timer = malloc(sizeof(timer_link_t));
	if (timer)
	{
		timer->left = TICKS_READ() + (int32_t)ticks;
		timer->set = ticks;
		timer->flags = flags;
		timer->callback = callback;

		if (flags & TF_DISABLED)
			return timer;

		disable_interrupts();

		timer->next = TI_timers;
		TI_timers = timer;
		timer_update_compare(TI_timers);

		enable_interrupts();
	}
	return timer;
}

/**
 * @brief Start a timer not currently in the list
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
void start_timer(timer_link_t *timer, int ticks, int flags, void (*callback)(int ovfl))
{
	assertf(TI_timers, "timer module not initialized");
	if (timer)
	{
		timer->left = TICKS_READ() + (int32_t)ticks;
		timer->set = ticks;
		timer->flags = flags;
		timer->callback = callback;

		if (flags & TF_DISABLED)
			return;

		disable_interrupts();

		timer->next = TI_timers;
		TI_timers = timer;
		timer_update_compare(TI_timers);

		enable_interrupts();
	}
}

/**
 * @brief Reset a timer and add to list
 *
 * @param[in] timer
 *            Pointer to timer structure to reinsert and start
 */
void restart_timer(timer_link_t *timer)
{
	if (timer)
	{
		timer->left = TICKS_READ() + (int32_t)timer->set;
		timer->flags &= ~TF_DISABLED;

		disable_interrupts();

		timer->next = TI_timers;
		TI_timers = timer;
		timer_update_compare(TI_timers);

		enable_interrupts();
	}
}

/**
 * @brief Stop a timer and remove it from the list
 *
 * @note This function does not free a timer structure, use #delete_timer
 *       to do this.
 *
 * @param[in] timer
 *            Timer structure to stop and remove
 */
void stop_timer(timer_link_t *timer)
{
	timer_link_t *head;
	timer_link_t *last = 0;

	assertf(TI_timers, "timer module not initialized");
	if (timer)
	{
		disable_interrupts();
		head = TI_timers;
		while (head)
		{
			if (head == timer)
			{
				/* remove from list */
				if (last)
					last->next = head->next;
				else
					TI_timers = head->next;

				break;
			}

			last = head;
			head = head->next;
		}
		timer_update_compare(TI_timers);
		enable_interrupts();
	}
}

/**
 * @brief Remove a timer from the list and delete it
 *
 * @param[in] timer
 *            Timer structure to stop, remove and free
 */
void delete_timer(timer_link_t *timer)
{
	assertf(TI_timers, "timer module not initialized");
	if (timer)
	{
		stop_timer(timer);
		free(timer);
	}
}

/**
 * @brief Free and close the timer subsystem
 *
 * This function will ensure all recurring timers are deleted from the list 
 * before closing.  One-shot timers that have expired will need to be
 * manually deleted with #delete_timer.
 */
void timer_close(void)
{
	assertf(TI_timers, "timer module not initialized");
	disable_interrupts();
	
	/* Disable generation of timer interrupt. */
	C0_WRITE_STATUS(C0_STATUS() & ~C0_INTERRUPT_TIMER);

	unregister_TI_handler(timer_callback);

	timer_link_t *head = TI_timers;
	while (head)
	{
		timer_link_t *last = head;
		head = head->next;

		if (last->flags & TF_CONTINUOUS)
		{
			/* Only free if it is a continuous timer as one-shot timers are
			 * freed by the user.  If we free a timer here, the user will
			 * never know if a one shot expired and needs to be removed or
			 * was removed automatically by timer_close.  We avoid this race
			 * condition by ensuring that the timer system never frees a 
			 * one shot timer.
			 */
			free(last);
		}
	}
	TI_timers = 0;
	enable_interrupts();
}

/**
 * @brief Return total ticks since timer was initialized, as a 64-bit counter.
 *
 * @return Then number of ticks since the timer was initialized
 *
 */
long long timer_ticks(void)
{
	uint32_t low, high;
	assertf(TI_timers, "timer module not initialized");

	/* Check whether interrupts are enabled or not. We need a different strategy
	 * to account for race conditions. */
	if (C0_STATUS() & C0_STATUS_IE) {
		/* Read the hardware counter twice, and fetch the high part counter
		 * in between. In the unlikely case that the counter overflows exactly
		 * during the sequence, it means that there's a race condition and
		 * we can't really know whether high and low are coherent -- but in
		 * that case, we just repeat the sequence again to avoid the ambiguity. */
		uint32_t pre;
		do {
			pre = TICKS_READ();
			MEMORY_BARRIER();
			high = ticks64_high;
			MEMORY_BARRIER();
			low = TICKS_READ();
		} while ((int32_t)pre < 0 && (int32_t)low >= 0);

	} else {
		/* Interrupts are currently disabled. If they've been disabled for more
		 * than 2**32 ticks, it's game over because we can't know how many times
		 * the counter has overflown.
		 * So assuming they were disabled for not too long, check whether there
		 * was a counter overflow between now and when they were disabled. 
		 * If there was, increment high. */
		low = TICKS_READ();
		high = ticks64_high;
		if (interrupt_disabled_tick > low)
			high++;
	}

	return ((uint64_t)high << 32) + low;
}

/** @} */
