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
 * @{
 */

/** @brief Internal linked list of timers */
static timer_link_t *TI_timers = 0;
/** @brief Total ticks elapsed since timer subsystem initialization */
static long long total_ticks;

/**
 * @brief Read the count out of the count register
 *
 * @param[out] x
 *             Variable to place count into
 */
#define read_count(x) asm volatile("mfc0 %0,$9\n\t nop \n\t" : "=r" (x) : )
/**
 * @brief Write the count to the count register
 *
 * @param[in] x
 *            Value to write into the count register
 */
#define write_count(x) asm volatile("mtc0 %0,$9\n\t nop \n\t" :  : "r" (x) )
/**
 * @brief Set the compare register
 *
 * This sets up the compare register so that when the count register equals the compare
 * register, an interrupt will be generated.
 *
 * @param[in] x
 *            Value to write into the compare register
 */
#define write_compare(x) asm volatile("mtc0 %0,$11\n\t nop \n\t" :  : "r" (x) )

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
static int __proc_timers(timer_link_t * head)
{
	timer_link_t *last = 0;
    int smallest = 0x3FFFFFFF;			// ~ 22.9 secs
	int start, now;

	read_count(start);
	total_ticks += start;

    while (head)
    {
		head->left -= start;
		// within ~5us?
		if (head->left < 234)
		{
			/* yes - timed out, do callback */
			head->ovfl = head->left;
			if (head->callback)
				head->callback(head->ovfl);

			/* reset ticks if continuous */
			if (head->flags & TF_CONTINUOUS)
			{
				head->left = head->set + head->ovfl;
				if (head->left < smallest)
					smallest = head->left;
				last = head;
			}
			else
			{
				/* one-shot, remove from list */
				if (last)
					last->next = head->next;
				else
					TI_timers = head->next;
			}
		}
		else
		{
			/* no, just check if remaining time is smallest */
			if (head->left < smallest)
				smallest = head->left;
			last = head;
		}

        /* Go to next */
	    head = head->next;
    }

	/* check if shortest time left < 5us */
	read_count(now);
	if (smallest < (now - start + 234))
	{
		total_ticks += (now - start);
		write_count(now - start);
		return 1;						// reprocess the list
	}
	/* set compare to shortest time left */
	write_count(0);
	write_compare(smallest - (now - start));
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
	if (TI_timers)
	{
		while (__proc_timers(TI_timers)) ;
	}
	else
	{
		int now;
		read_count(now);
		total_ticks += now;
		write_count(0);
		write_compare(0x7FFFFFFF);
	}
}

/**
 * @brief Initialize the timer subsystem
 */
void timer_init(void)
{
	total_ticks = 0;
	write_count(0);
	write_compare(0x7FFFFFFF);
    register_TI_handler(timer_callback);
}

/**
 * @brief Create a new timer and add to list
 *
 * @param[in] ticks
 *            Number of ticks before the timer should fire
 * @param[in] flags
 *            Timer flags.  See #TF_ONE_SHOT and #TF_CONTINUOUS
 * @param[in] callback
 *            Callback function to call when the timer expires
 *
 * @return A pointer to the timer structure created
 */
timer_link_t *new_timer(int ticks, int flags, void (*callback)(int ovfl))
{
	timer_link_t *timer = malloc(sizeof(timer_link_t));
	if (timer)
	{
		timer->left = ticks;
		timer->set = ticks;
		timer->flags = flags;
		timer->callback = callback;

		disable_interrupts();

		timer->next = TI_timers;
		TI_timers = timer;
		if (timer->next == 0)
		{
			/* first timer added to list */
			write_count(0);
			write_compare(timer->left);
		}
		else
			timer_callback();			// force processing the timers

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
 *            Timer flags.  See #TF_ONE_SHOT and #TF_CONTINUOUS
 * @param[in] callback
 *            Callback function to call when the timer expires
 */
void start_timer(timer_link_t *timer, int ticks, int flags, void (*callback)(int ovfl))
{
	if (timer)
	{
		timer->left = ticks;
		timer->set = ticks;
		timer->flags = flags;
		timer->callback = callback;

		disable_interrupts();

		timer->next = TI_timers;
		TI_timers = timer;
		if (timer->next == 0)
		{
			/* first timer added to list */
			write_count(0);
			write_compare(timer->left);
		}
		else
			timer_callback();			// force processing the timers

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
	disable_interrupts();
    
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
 * @brief Return total ticks since timer was initialized
 *
 * @return Then number of ticks since the timer was initialized
 */
long long timer_ticks(void)
{
	disable_interrupts();
	timer_callback();					// force processing the timers
	enable_interrupts();
	return total_ticks;
}

/** @} */
