/**
 * @file timer.c
 * @brief Timer Subsystem
 * @ingroup timer
 */
#include <malloc.h>
#include "timer.h"
#include "interrupt.h"
#include "debug.h"
#include "regsinternal.h"
#include "utils.h"

/** @brief Refcount of #timer_init vs #timer_close calls. */
static int timer_init_refcount = 0;

/** @brief Internal linked list of timers */
static timer_link_t *TI_timers = NULL;

/** @brief Timer callback expects a context parameter */
#define TF_CONTEXT     0x20

/** @brief Timer is the special overflow timer. */
#define TF_OVERFLOW    0x40

/** @brief Timer has been called once in this interrupt. */
#define TF_CALLED      0x80

/** @brief Update the compare register to match the first expiring timer. */
__attribute__((noinline))
static void timer_update_compare(timer_link_t *head, uint32_t now)
{
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

			/* invoke the appropriate callback function */
			if (head->flags & TF_CONTEXT && head->callback_with_context)
				head->callback_with_context(head->ovfl, head->ctx);
			else if (head->callback)
				head->callback(head->ovfl);

			if (head->flags & TF_DISABLED)
			{
				/* Timer was disabled during the callback. We need to
				 * reprocess the list to see if there are other timers
				 * that need to be called. */
				return 1;
			}

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
 * @brief Poll the timer list and run callbacks for expired timers
 *
 * This function is called by the interrupt handler whenever 
 * compare == count, and also when inserting into or removing
 * from the timers list to improve handling timers with tiny delays
 */
static void timer_poll(void)
{
	uint32_t loop_count = 0;
	while (__proc_timers(TI_timers)) {
		++loop_count; (void)loop_count; // avoid warning (loop_count is used in assertf)
		assertf(loop_count < 1000, "timer interrupt is stuck in an infinite loop.\n"
			"Check continuous timers with a very short period.\n");
	}

	// Update counter for next interrupt.
	timer_update_compare(TI_timers, TICKS_READ());
}

void timer_init(void)
{
	// Just increment the refcount if already initialized.
	if (timer_init_refcount++ > 0) { return; }

	// Reset the compare register and enable timer interrupts in COP0.
	// Do not write the COUNT register to avoid interfering with get_ticks().
	disable_interrupts();
	C0_WRITE_COMPARE(0);
	set_TI_interrupt(1);
	register_TI_handler(timer_poll);
	enable_interrupts();
}

timer_link_t *new_timer(int ticks, int flags, timer_callback1_t callback)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");
	timer_link_t *timer = malloc(sizeof(timer_link_t));
	if (timer)
	{
		disable_interrupts();

		uint32_t now = TICKS_READ();
		timer->left = now + (int32_t)ticks;
		timer->set = ticks;
		timer->flags = flags;
		timer->callback = callback;
		timer->ctx = NULL;

		if (!(flags & TF_DISABLED))
		{
			timer->next = TI_timers;
			TI_timers = timer;
			timer_update_compare(TI_timers, now);
			timer_poll();
		}

		enable_interrupts();
	}
	return timer;
}

timer_link_t *new_timer_context(int ticks, int flags, timer_callback2_t callback, void *ctx)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");
	timer_link_t *timer = malloc(sizeof(timer_link_t));
	if (timer)
	{
		disable_interrupts();

		uint32_t now = TICKS_READ();
		timer->left = now + (int32_t)ticks;
		timer->set = ticks;
		timer->flags = flags | TF_CONTEXT;
		timer->callback_with_context = callback;
		timer->ctx = ctx;

		if (!(flags & TF_DISABLED))
		{
			timer->next = TI_timers;
			TI_timers = timer;
			timer_update_compare(TI_timers, now);
			timer_poll();
		}

		enable_interrupts();
	}
	return timer;
}

void start_timer(timer_link_t *timer, int ticks, int flags, timer_callback1_t callback)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");
	if (timer)
	{
		disable_interrupts();

		uint32_t now = TICKS_READ();
		timer->left = now + (int32_t)ticks;
		timer->set = ticks;
		timer->flags = flags;
		timer->callback = callback;
		timer->ctx = NULL;

		if (!(flags & TF_DISABLED))
		{
			timer->next = TI_timers;
			TI_timers = timer;
			timer_update_compare(TI_timers, now);
			timer_poll();
		}

		enable_interrupts();
	}
}

void start_timer_context(timer_link_t *timer, int ticks, int flags, timer_callback2_t callback, void *ctx)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");
	if (timer)
	{
		disable_interrupts();

		uint32_t now = TICKS_READ();
		timer->left = now + (int32_t)ticks;
		timer->set = ticks;
		timer->flags = flags | TF_CONTEXT;
		timer->callback_with_context = callback;
		timer->ctx = ctx;

		if (flags & TF_DISABLED)
		{
			timer->next = TI_timers;
			TI_timers = timer;
			timer_update_compare(TI_timers, now);
			timer_poll();
		}

		enable_interrupts();
	}
}

void restart_timer(timer_link_t *timer)
{
	if (timer)
	{
		disable_interrupts();

		uint32_t now = TICKS_READ();
		timer->left = now + (int32_t)timer->set;
		timer->flags &= ~TF_DISABLED;

		timer->next = TI_timers;
		TI_timers = timer;
		timer_update_compare(TI_timers, now);
		timer_poll();

		enable_interrupts();
	}
}

void stop_timer(timer_link_t *timer)
{
	timer_link_t *head;
	timer_link_t *last = 0;

	assertf(timer_init_refcount > 0, "timer module not initialized");
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
		timer->flags |= TF_DISABLED;
		timer_update_compare(TI_timers, TICKS_READ());
		enable_interrupts();
	}
}

void delete_timer(timer_link_t *timer)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");
	if (timer)
	{
		stop_timer(timer);
		free(timer);
	}
}

void timer_close(void)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");

	// Do nothing if there are still dangling references.
	if (--timer_init_refcount > 0) { return; }

	disable_interrupts();
	
	/* Disable generation of timer interrupt. */
	set_TI_interrupt(0);
	unregister_TI_handler(timer_poll);

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

long long timer_ticks(void)
{
	assertf(timer_init_refcount > 0, "timer module not initialized");
	return get_ticks();
}
