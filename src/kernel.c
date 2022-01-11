#include <libdragon.h>
#include "kernel.h"
#include "kernelinternal.h"
#include "timer.h"
#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#define DEBUG_KERNEL   0

/** @brief Enable stack-smashing checks of threads. */
#define KERNEL_CHECKS  1

#define STACK_COOKIE   0xDEADBEEFBAADC0DE
#define STACK_GUARD    64

/** @brief Read the current value of the gp register */
#define REG_GP()     ({ uint64_t gp; __asm("move %0, $28": "=r" (gp)); gp; })

/**
 * @brief Execute a context switch.
 *
 * This forces a context switch to happen, by invoking the syscall 0x1. The
 * syscall will be intercepted by the generic interrupt handler (inthandler.S)
 * that will call #__kthread_syscall_schedule.
 *
 * This macro is very low level and is called as part of higher-level primitives
 * that force a context switch like #thread_yield.
 *
 * Do not call this under interrupt; use #KTHREAD_SWITCH_ISR instead.
 */
#define KTHREAD_SWITCH()      __asm("syscall 0x1")

/**
 * @brief Executed a context switch, during an ongoing interrupt service routine.
 *
 * This forces a context switch to happen just like #KTHREAD_SWITCH, but it is
 * the right version to call if the code is currently servicing an interrupt.
 */
#define KTHREAD_SWITCH_ISR()  ({ __isr_force_schedule = true; })

/** @brief Main thread */
kthread_t th_main;
/** @brief Pointer to the current thread */
kthread_t *th_cur;
/** @brief Pointer to the idle thread */
kthread_t *th_idle;
/** @brief List of ready threads */
static kthread_t *th_ready;
/** @brief Number of live thread */
static int th_count;

kevent_t KEVENT_IRQ_AI = {0};
kevent_t KEVENT_IRQ_DP = {0};
kevent_t KEVENT_IRQ_SP = {0};
kevent_t KEVENT_IRQ_SI = {0};
kevent_t KEVENT_IRQ_VI = {0};
kevent_t KEVENT_IRQ_PI = {0};
kevent_t KEVENT_RESET  = {0};

/** @brief True if the multithreading kernel has been initialized and is running. */
bool __kernel = false;
/** @brief True if a context switch must be done at the end of current interrupt. */
bool __isr_force_schedule = false;
/* Global current interrupt depth (defined in interrupt.c) */ 
extern int __interrupt_depth;

/** 
 * @brief Boot function for a kthread.
 *
 * It invokes the user entry function, and then kills the thread
 * if the function ever returns.
 */
void __kthread_boot(void)
{
	th_cur->user_entry(th_cur->user_data);
	if (DEBUG_KERNEL) debugf("thread end: %s[%p]\n", th_cur->name, th_cur);
	kthread_kill(NULL);
}

void __kthread_check_overflow(kthread_t *th)
{
	// If the current stack pointer is beyond the end of the stack,
	// this is a stack overflow even without checking the integrity of the
	// guard.
	assertf((uintptr_t)th->stack_state >= (uintptr_t)th->stack + STACK_GUARD,
		"stack overflow in thread: %s[%p]\nSP:%p | Stack top: %p | Overflow: %d bytes",
		th->name, th,
		th->stack_state, th->stack+STACK_GUARD,
		th->stack+STACK_GUARD-(void*)th->stack_state);

	// Check if the stack guard has been corrupted. This indicates that
	// has a stack overflow has happened, even if right now the stack has
	// rolled back.
	if (KERNEL_CHECKS)
	{
		uint64_t *cookie = (uint64_t*)th->stack;
		for (int i=0;i<STACK_GUARD/8;i++)
			assertf(cookie[i] == STACK_COOKIE, "stack overflow in thread: %s[%p]\nStack guard is corrupted", th->name, th);		
	}
}

/** @brief Add a thread to a linked list */
void __thlist_add(kthread_t **list, kthread_t *th)
{
	assert(!(th->flags & TH_FLAG_INLIST));
	th->flags |= TH_FLAG_INLIST;

	th->next = *list;
	*list = th;	
}

/** @brief Add a thread to a linked list sorted by priority */
void __thlist_add_pri(kthread_t **list, kthread_t *th)
{
	while (*list && (*list)->pri >= th->pri)
		list = &((*list)->next);
	__thlist_add(list, th);
}

/** @brief Peek the head of a thread list (without popping it) */
kthread_t* __thlist_head(kthread_t **list)
{
	if (!list) return NULL;
	return *list;
}

/** @brief Pop the head of a thread list */
kthread_t* __thlist_rem(kthread_t **list)
{
	kthread_t *th = *list;
	if (th)
	{
		assert(th->flags & TH_FLAG_INLIST);
		th->flags &= ~TH_FLAG_INLIST;

		*list = th->next;
		th->next = NULL;
	}
	return th;
}

/** 
 * @brief Move all threads from list src to list dst, respecting priority. 
 *
 * @return true if at least a thread of priority equal or higher than the
 *         the current one has been moved.
 **/
bool __thlist_splice_pri(kthread_t **dst, kthread_t **src)
{
	bool highpri = false;
	kthread_t *th;
	while ((th = __thlist_rem(src)))
	{
		highpri = highpri || (th->pri >= th_cur->pri);
		__thlist_add_pri(dst, th);
	}
	return highpri;
}

/** 
 * @brief Kernel scheduler: park the current thread and schedule the next thread
 *
 * This is the main scheduling function. It is invoked by the interrupt handler
 * if explicitly requested via a syscall exception (#KTHREAD_SWITCH) or if
 * an interrupt handler is preempting the current thread (#KTHREAD_SWITCH_ISR).
 *
 * @param stack_state Stack pointer containing the register block for the
 *                    the current thread.
 *
 * @return The stack pointer containing the register block of the thread to
 *         be activated.
 **/
reg_block_t* __kthread_syscall_schedule(reg_block_t *stack_state)
{
	if (th_cur)
	{
		// Save the stack state for the current thread.
		th_cur->stack_state = stack_state;

		// For debugging purposes: check if the current thread has overflown
		// its allocated stack.
		__kthread_check_overflow(th_cur);

	 	if (th_cur->flags & TH_FLAG_ZOMBIE)
	 	{
	 		// If the current thread is marked as zombie, it means that it must
	 		// be freed.
	 		if (DEBUG_KERNEL) debugf("[kernel] killing zombie: %s(%p) PC=%lx\n", th_cur->name, th_cur, stack_state->epc);
			assert(!(th_cur->flags & TH_FLAG_INLIST));
			free(th_cur->stack);
	 	}
		else
		{
			// Save the current thread state.
			if (DEBUG_KERNEL) debugf("[kernel] parking %s(%p) PC=%lx\n", th_cur->name, th_cur, stack_state->epc);

			// Check how we got here. There are two possibilities: explicit
			// syscall forcing a thread switch (THREAD_SWITCH), or an interrupt
			// that awakes a higher priority thread (THREAD_SWITCH_ISR).
			if (C0_GET_CAUSE_EXC_CODE(stack_state->cr) == EXCEPTION_CODE_SYS_CALL)
			{
				// Skip syscall instruction when this thread will be scheduled
				// again. Notice that, given that this is an explicit syscall,
				// the thread must already have been added to a waiting list,
				// otherwise it wouldn't ever be scheduled again.
				assert(th_cur->flags & TH_FLAG_INLIST);
				assertf(*(uint32_t*)th_cur->stack_state->epc == 0x0000004C, "invalid opcode found by __kthread_syscall_schedule:\nexpected 0x0000004C (SYSCALL 0x1), found: %08lx", *(uint32_t*)th_cur->stack_state->epc);
				th_cur->stack_state->epc += 4;
			}
			else
			{
				// If this is a preemptive switch forced by an interrupt,
				// the current thread was running and it's still ready, so
				// add it to the ready list again.
				assertf(!(th_cur->flags & TH_FLAG_INLIST), "thread %s[%p] in list? flags=%x", th_cur->name, th_cur, th_cur->flags);
				__thlist_add_pri(&th_ready, th_cur);
			}

			// Save the current interrupt depth. Interrupt depth is actually
			// per-thread, so we just save/restore it every time a thread is
			// scheduled.
			th_cur->interrupt_depth = __interrupt_depth;
		}
	}

	// Schedule the highest-priority thread that is ready. This would be
	// the first thread in the ready list. There is always at least a thread
	// here: the idle thread.
	th_cur = __thlist_rem(&th_ready);
	assert(th_cur != NULL);
	if (DEBUG_KERNEL) debugf("[kernel] switching to %s(%p) PC=%lx\n", th_cur->name, th_cur, th_cur->stack_state->epc);
	assert(!(th_cur->flags & TH_FLAG_INLIST));

	// Set the current interrupt depth to that of the current thread.
	__interrupt_depth = th_cur->interrupt_depth;

	return th_cur->stack_state;
}

/**
 * @brief Idle thread function
 *
 * Since there must always be something run by the CPU, the kernel creates
 * a "idle" thread: a lowest-priority thread that is run only when all
 * user-created threads are blocked.
 *
 * This thread does absolutely nothing. It just waits for any other thread
 * to wake up so that it will be preempted.
 */
void __kthread_idle(void *arg)
{
	// When this thread is scheduled, there are no
	// ready threads and all threads are probably waiting for some
	// hardware event to happen. Just spin-wait here.
	while(1) {}
}

kthread_t* kernel_init(void)
{
	assert(!__kernel);
	th_ready = NULL;  // empty ready list
	th_count = 1; // start with the main thread

	// Configure the main thread
	memset(&th_main, 0, sizeof(th_main));
	th_main.pri = 0;
	th_main.name = "main";

	// NOTE: keep this in sync with system.c
	#define STACK_SIZE 0x10000
	th_main.stack = (char*)0x80000000 + get_memory_size() - STACK_SIZE;

	uint64_t *s = (uint64_t*)th_main.stack;
	for (int i=0;i<STACK_GUARD/8;i++)
		s[i] = STACK_COOKIE;

	// The main thread is the currently scheduled one.
	th_cur = &th_main;

	// Allocate the idle thread
	th_idle = kthread_new("idle", 4096, -1, __kthread_idle, 0);

	__kernel = true;
	return th_cur;
}

void kernel_close(void)
{
	assert(__kernel);
	assertf(th_cur == &th_main, "kthread_close can only be called from main thread");

	// Kill the idle thread to release the memory
	kthread_kill(th_idle);
	th_idle = NULL;

	assertf(th_count == 1, "not all threads were killed");

	th_cur = NULL;
	__kernel = false;
	__isr_force_schedule = false;
}

kthread_t* kthread_new(const char *name, int stack_size, int8_t pri, void (*user_entry)(void*), void *user_data)
{
	assert((stack_size % 8) == 0);

	// Allocate memory for the stack, the stack guard area, and the kthread_t
	// structure
	//
	// |             |                        |             |
	// | Stack guard |   Stack                | kthread_t   |
	// |             |                        |             |
	//
	//
	// For the kthread structure, we will use the final part of
	// the buffer: since the stack grows downward, this makes sure that
	// overflowing the stack doesn't corrupt the thread structure, which might
	// make debugging more difficult and might even cause the kernel to crash.
	void *thmem = malloc(STACK_GUARD + stack_size + sizeof(kthread_t));
	assertf(thmem, "out of free memory");
	kthread_t *th = thmem + STACK_GUARD + stack_size;

	// Initialize the thread structure
	memset(th, 0, sizeof(kthread_t));
	th->stack = thmem;

	th->name = name;
	th->user_entry = user_entry;
	th->user_data = user_data;
	th->pri = pri;

	// Initialize the stack guard
	uint64_t *s = (uint64_t*)th->stack;
	for (int i=0;i<STACK_GUARD/8;i++)
		s[i] = STACK_COOKIE;

	// Top of the stack is the end of the stack area, so where the kthread_t
	// structure is. Remember that the stack grows downward.
	void *top_stack = th;

	// Put the initial regblock at the top of the stack. Since threads are
	// always scheduled under interrupts, the top of the stack of a non-running
	// thread must always contain a reg_block_t, that is a dump of all its
	// registers. We thus initialize all the registers to the value we
	// except at thread start (which is 0 for most of them).
	th->stack_state = (reg_block_t*)(top_stack - sizeof(reg_block_t));
	assert(((uint32_t)th->stack_state & 7) == 0);
	memset(th->stack_state, 0, sizeof(reg_block_t));

	// EPC: __thread_boot function (which will then invoke the user entry point)
	th->stack_state->epc = (uint32_t)&__kthread_boot;

	// SR: start in exception mode (we will be within an exception during scheduling),
	// and with interrupts enabled.
	th->stack_state->sr = C0_STATUS() | C0_STATUS_EXL | C0_STATUS_IE;

	// GP: this is needed by the compiler to point to the global memory pool,
	// so let's initialize to the same value it has in this thread.
	th->stack_state->gpr[28] = REG_GP();

	// SP: top of stack
	th->stack_state->gpr[29] = (int64_t)(int32_t)top_stack;

	disable_interrupts();
	th_count++;
	__thlist_add_pri(&th_ready, th);
	enable_interrupts();

	// If the new thread has a priority higher or equal to the current one,
	// yield immediately so that it will get started. This preserves the basic
	// invariant that a higher priority thread is immediately scheduled whenever
	// it becomes ready.
	if (th->pri >= th_cur->pri)
		kthread_yield();

	return th;
}

void kthread_kill(kthread_t *th)
{
	if (th == NULL) th = th_cur;
	
	if (DEBUG_KERNEL) debugf("killing: %s[%p] (flags:%x)\n", th->name, th, th->flags);
	
	disable_interrupts();
	th_count--;

	// In general, we cannot just free the memory. If the thread is enqueued
	// in some list (eg: waiting on a mailbox), we would need to remove it
	// from that list before freeing it, but we don't have a way to do it
	// (a thread doesn't have a reference to the list it's in, just a next
	// pointer).
	// So just mark the thread as zombie. This will cause the scheduler to
	// free it as soon as the thread is first scheduled.
	th->flags |= TH_FLAG_ZOMBIE;

	// If we're trying to kill ourselves, it's sufficient to force a context
	// switch now.
	if (th == th_cur)
		KTHREAD_SWITCH();
	enable_interrupts();
}

void kthread_yield(void)
{
	// Check if there's a ready thread that has priority
	// higher than or equal to the current thread, otherwise it's
	// useless to force a context switch: the current thread would
	// be rescheduled again.
	kthread_t *th = __thlist_head(&th_ready);
	if (th && th->pri >= th_cur->pri)
	{
		if (DEBUG_KERNEL) debugf("yielding: %s[%p] (flags:%x, status:%lx)\n", th_cur->name, th_cur, th_cur->flags, C0_STATUS());
		disable_interrupts();

		__thlist_add_pri(&th_ready, th_cur);
		KTHREAD_SWITCH();

		enable_interrupts();
	}
}

void kthread_set_pri(kthread_t *th, int8_t pri)
{
	assertf(pri >= 0, "thread priority cannot be negative");
	if (th == NULL) th = th_cur;
	th->pri = pri;

	// Yield in case the priority change has immediate effect
	kthread_yield();
}

void kthread_sleep(uint32_t ticks)
{
	kthread_t *th = th_cur;

	// Timer callback. This will be invoked when the timer elapses after the
	// requested delay.
	void cb(int ovlf)
	{
		if (DEBUG_KERNEL) debugf("[kernel] sleep finished %s[%p]\n", th->name, th);

		// Put the thread again in the ready list, and forces a context switch
		th->flags &= ~TH_FLAG_INLIST;
		__thlist_add_pri(&th_ready, th);
		KTHREAD_SWITCH_ISR();
	}

	if (DEBUG_KERNEL) debugf("[kernel] sleeping %ld %s[%p]\n", ticks, th->name, th);
	disable_interrupts();

	// Start a timer for the specified delay.
	timer_link_t timer;
	start_timer(&timer, ticks, TF_ONE_SHOT, cb);

	// Mark the thread as being in a list to avoid the scheduler to panic.
	// This is not "technically true", but it's "semantically true": when
	// we say "a thread is in a list" it means that it's registered somewhere
	// so that there's a way to wake it up in the future. In this case,
	// the timer callback will do it, so we can reassure the kernel scheduler
	// that everything is fine.
	th->flags |= TH_FLAG_INLIST;

	// Context switch
	KTHREAD_SWITCH();

	enable_interrupts();
}

void kthread_wait_event(kevent_t *evt)
{
	// Simplified API to wait for a single event. Just allocate a mailbox
	// on the stack, attach to the event, and receive from it.
	kmbox_t *mbox = kmbox_new_stack(1);
	kmbox_attach_event(mbox, evt);
	kmbox_recv(mbox);
	kmbox_detach_event(mbox, evt);
}

kmbox_t* kmbox_new(int max_mails)
{
	// Allocate memory for the mailbox structure plus the mail array
	int sz = sizeof(kmbox_t) + (max_mails)*sizeof(void*);
	kmbox_t *mb = malloc(sz);
	assertf(mb, "out of free memory");
	memset(mb, 0, sz);
	mb->max = max_mails;
	return mb;
}

void kmbox_free(kmbox_t *mbox)
{
	assertf(mbox->evts == 0, "mbox_free() called, but missing %d mbox_detach_event() calls", mbox->evts);
	free(mbox);
}

bool kmbox_empty(kmbox_t *mbox)
{
	return mbox->mails[mbox->r] == NULL;
}

bool kmbox_full(kmbox_t *mbox)
{
	return mbox->mails[mbox->w] != NULL;
}

/**
 * @brief  Enqueue a mail into a mailbox (lower-level primitive).
 *
 * @note   This function must be called with interrupts disabled, as it
 *         manipulates the ready list.
 *
 * @return 0 if the mail was not sent (mbox full)
 *         1 if the mail was sent
 *         2 if mail was sent and a higher-priority thread was awaken,
 *         so a context switch should be done as soon as possible.
 */
int __kmbox_try_send(kmbox_t *mbox, void *mail)
{
	// If the mbox is full, there's nothing to do
	if (kmbox_full(mbox))
		return 0;

	// Enqueue the new mail
	mbox->mails[mbox->w++] = mail;
	if (mbox->w == mbox->max)
		mbox->w = 0;

	// Awake pending threads. If a higher-priority thread is awaken,
	// notify it to the caller.
	return __thlist_splice_pri(&th_ready, &mbox->wait_list) ? 2 : 1;
}

bool kmbox_try_send(kmbox_t *mbox, void *mail)
{
	int ret;

	disable_interrupts();
	ret = __kmbox_try_send(mbox, mail);
	if (ret == 2)
		kthread_yield();
	enable_interrupts();

	return ret != 0;
}

void kmbox_send(kmbox_t *mbox, void *mail)
{
	disable_interrupts();
	while (!kmbox_try_send(mbox, mail))
	{
		__thlist_add(&mbox->wait_list, th_cur);
		KTHREAD_SWITCH();
	}
	enable_interrupts();
}

void* kmbox_try_recv(kmbox_t *mbox)
{
	void *mail = NULL;
	disable_interrupts();
	if (!kmbox_empty(mbox))
	{
		mail = mbox->mails[mbox->r];
		mbox->mails[mbox->r++] = NULL;
		if (mbox->r == mbox->max) mbox->r = 0;
		if (__thlist_splice_pri(&th_ready, &mbox->wait_list)) {
			if (DEBUG_KERNEL) debugf("kmbox_try_recv: switching out\n");
			kthread_yield();
			if (DEBUG_KERNEL) debugf("kmbox_try_recv: back %p\n", mail);
		}
	}
	enable_interrupts();
	return mail;
}

void* kmbox_recv(kmbox_t *mbox)
{
	void* mail;
	disable_interrupts();
	while (!(mail = kmbox_try_recv(mbox)))
	{
		__thlist_add(&mbox->wait_list, th_cur);
		KTHREAD_SWITCH();		
	}
	enable_interrupts();
	return mail;
}

/** @brief Underlying implementation for #kevent_trigger and #kevent_trigger_isr */
void __kevent_trigger(kevent_t *evt, bool isr)
{
	bool should_yield = false;

	disable_interrupts();
	for (int i = 0; i < MAX_MAILBOXES_PER_EVENT; i++)
	{
		// Send the event to the attached mailboxes, and remember
		// if we have woken up a higher-priority thread.
		if (evt->mboxes[i] && __kmbox_try_send(evt->mboxes[i], evt) == 2)
			should_yield = true;
	}

	// If a higher-priority thread has been waken up,
	// force a context switch. Depending on whether we are
	// running under interrupt or not, call the correct function.
	if (should_yield)
	{
		if (isr)
			KTHREAD_SWITCH_ISR();
		else
			kthread_yield();
	}

	enable_interrupts();
}

void kevent_trigger(kevent_t *evt)     { __kevent_trigger(evt, false); }
void kevent_trigger_isr(kevent_t *evt) { __kevent_trigger(evt, true);  }

void kmbox_attach_event(kmbox_t *mbox, kevent_t *evt)
{
	disable_interrupts();
	for (int i=0;i<MAX_MAILBOXES_PER_EVENT;i++)
	{
		// Find a free slot for the mailbox in the event.
		if (!evt->mboxes[i])
		{
			evt->mboxes[i] = mbox;
			mbox->evts++;
			break;
		}
	}
	enable_interrupts();
}

void kmbox_detach_event(kmbox_t *mbox, kevent_t *evt)
{
	disable_interrupts();
	for (int i=0;i<MAX_MAILBOXES_PER_EVENT;i++)
	{
		// Search for the mailbox in the event, and remove it
		if (evt->mboxes[i] == mbox)
		{
			evt->mboxes[i] = NULL;
			mbox->evts--;
			break;
		}
	}
	enable_interrupts();
}
