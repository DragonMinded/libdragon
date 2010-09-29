#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

/* Timer linked list */
static _timer_link *TI_timers = 0;
static long long total_ticks;

#define read_count(x) asm volatile("mfc0 %0,$9\n\t nop \n\t" : "=r" (x) : )
#define write_count(x) asm volatile("mtc0 %0,$9\n\t nop \n\t" :  : "r" (x) )
#define write_compare(x) asm volatile("mtc0 %0,$11\n\t nop \n\t" :  : "r" (x) )

/* Process linked list of timers */
static int __proc_timers(_timer_link * head)
{
	_timer_link *last = 0;
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

/* Called whenever Compare == Count */
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

/* initialize timer subsystem - register timer callback */
void timer_init(void)
{
	total_ticks = 0;
	write_count(0);
	write_compare(0x7FFFFFFF);
    register_TI_handler(timer_callback);
}

/* create a new timer and add to list */
_timer_link *new_timer(int ticks, int flags, void (*callback)(int ovfl))
{
	_timer_link *timer = malloc(sizeof(_timer_link));
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

/* start a timer not currently in the list */
void start_timer(_timer_link *timer, int ticks, int flags, void (*callback)(int ovfl))
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

/* remove a timer from the list */
void stop_timer(_timer_link *timer)
{
	_timer_link *head;
	_timer_link *last = 0;

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

				enable_interrupts();
				return;
			}

			last = head;
			head = head->next;
		}
		enable_interrupts();
	}
}

/* remove a timer from the list and delete it */
void delete_timer(_timer_link *timer)
{
	if (timer)
	{
		stop_timer(timer);
		free(timer);
	}
}

/* delete all timers in list */
void timer_close(void)
{
	disable_interrupts();
	_timer_link *head = TI_timers;
	while (head)
	{
		_timer_link *last = head;
		head = head->next;
		free(last);
	}
	TI_timers = 0;
	enable_interrupts();
}

/* return total ticks since timer was initialized */
long long timer_ticks(void)
{
	disable_interrupts();
	timer_callback();					// force processing the timers
	enable_interrupts();
	return total_ticks;
}
