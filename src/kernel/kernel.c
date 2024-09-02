#include <libdragon.h>
#include "kernel.h"
#include "kernel_internal.h"
#include "backtrace_internal.h"
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
 * This forces a context switch to happen, by invoking the syscall 0x0. The
 * syscall will be intercepted by the generic interrupt handler (inthandler.S)
 * that will call #__kthread_syscall_schedule.
 *
 * This macro is very low level and is called as part of higher-level primitives
 * that force a context switch like #thread_yield.
 *
 * Do not call this under interrupt; use #KTHREAD_SWITCH_ISR instead.
 */
#define KTHREAD_SWITCH()      __asm("syscall 0x0")

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

/** @brief True if the multithreading kernel has been initialized and is running. */
bool __kernel = false;
/** @brief True if a context switch must be done at the end of current interrupt. */
bool __isr_force_schedule = false;
/* Global current interrupt depth (defined in interrupt.c) */ 
extern int __interrupt_depth;
extern int __interrupt_sr;

#ifndef NDEBUG
kthread_t *__kernel_all_threads;
#endif

/** 
 * @brief Boot function for a kthread.
 *
 * It invokes the user entry function, and then kills the thread
 * if the function ever returns.
 */
__attribute__((noreturn))
void __kthread_boot(void)
{
	// Initialize GP register. This is the same as done in the entrypoint
	// and is also used by the backtrace code as marker to stop walking.
	asm volatile ("la $gp, _gp");
	int res = th_cur->user_entry(th_cur->user_data);
	if (DEBUG_KERNEL) debugf("thread end: %s[%p] res=%d\n", th_cur->name, th_cur, res);
	kthread_exit(res);
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

static void kthread_free(kthread_t *th)
{
	if (DEBUG_KERNEL) debugf("[kernel] freeing %s[%p]\n", th->name, th);
	#ifndef NDEBUG
	// Clear memory to avoid dangling pointers
	memset(th, 0, sizeof(kthread_t));
	// Remove the thread from the all-threads list
	kthread_t **p = &__kernel_all_threads;
	while (*p && *p != th)
		p = &((*p)->all_next);
	if (*p) *p = th->all_next;
	#endif
	// Free the thread memory (the start of the heap-allocated block is the stack)
	free(th->stack);
}

/** @brief Add a thread to a linked list */
void __thlist_add(kthread_t **list, kthread_t *th)
{
	assert(!(th->flags & TH_FLAG_INLIST));
	th->flags |= TH_FLAG_INLIST;

	th->next = *list;
	*list = th;	
}

/** @brief Add a thread to a linked list sorted by priority. */
void __thlist_add_pri(kthread_t **list, kthread_t *th)
{
	while (*list && (*list)->pri >= th->pri)
		list = &((*list)->next);
	__thlist_add(list, th);
}

#define __phys_thlist_add_pri(list, th) ({ \
	kthread_t *__list = (list) ? VirtualCachedAddr(list) : NULL; \
	__thlist_add_pri(&__list, (th)); \
	(list) = PhysicalAddr(__list); \
})

/** @brief Peek the head of a thread list (without popping it) */
kthread_t* __thlist_head(kthread_t **list)
{
	if (!list) return NULL;
	return *list;
}

/** @brief Pop the head of a thread list (highest priority one, if the list is sorted) */
kthread_t* __thlist_pop(kthread_t **list)
{
	kthread_t *th = *list;
	if (th)
	{
		assertf(th->flags & TH_FLAG_INLIST, "thread %s[%p] not in list", th->name, th);
		th->flags &= ~TH_FLAG_INLIST;

		*list = th->next;
		th->next = NULL;
	}
	return th;
}

/** @brief Remove an element from a thread list */
bool __thlist_remove(kthread_t **list, kthread_t *th)
{
	while (*list && *list != th)
		list = &((*list)->next);
	if (*list)
	{
		*list = th->next;
		th->next = NULL;
		th->flags &= ~TH_FLAG_INLIST;
		return true;
	}
	return false;
}

#define __phys_thlist_remove(list, th) ({ \
	kthread_t *__list = (list) ? VirtualCachedAddr(list) : NULL; \
	bool __ret = __thlist_remove(&__list, (th)); \
	(list) = PhysicalAddr(__list); \
	__ret; \
})

/** 
 * @brief Move all threads from list src to list dst, respecting priority. 
 * 
 * NOTE: both lists must be sorted by priority, otherwise result is not
 * guaranteed to be sorted.
 *
 * @return true if at least a thread of priority equal or higher than the
 *         the current one has been moved.
 **/
bool __thlist_splice_pri(kthread_t **dst, kthread_t **src)
{
	bool highpri = false;
	kthread_t *th;
	while ((th = __thlist_pop(src)))
	{
		highpri = highpri || (th->pri >= th_cur->pri);
		__thlist_add_pri(dst, th);
		dst = &th->next;
	}
	return highpri;
}

#define __phys_thlist_splice_pri(dst, src) ({ \
	kthread_t *__src = (src) ? VirtualCachedAddr(src) : NULL; \
	bool __ret = __thlist_splice_pri(dst, &__src); \
	(src) = PhysicalAddr(__src); \
	__ret; \
})

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
			assert(th_cur->flags & TH_FLAG_DETACHED);
			kthread_free(th_cur);
	 	}
		else if (th_cur->flags & TH_FLAG_WAITFORJOIN)
		{
			// Nothing to do. This is a non-detached thread that just exited without a joiner.
			// We can just forget about it, it wil get freed when someone calls kthread_join().
			assert(!(th_cur->flags & TH_FLAG_INLIST));
			assertf(!(th_cur->flags & TH_FLAG_DETACHED), "thread %s[%p] is waiting for a joiner, but is detached", th_cur->name, th_cur);
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
				assertf(*(uint32_t*)th_cur->stack_state->epc == 0x0000000C, "invalid opcode found by __kthread_syscall_schedule:\nexpected 0x0000000C (SYSCALL 0x0), found: %08lx", *(uint32_t*)th_cur->stack_state->epc);
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
			th_cur->tls.interrupt_depth = __interrupt_depth;
			th_cur->tls.interrupt_sr = __interrupt_sr;
		}
	}

	// Schedule the highest-priority thread that is ready, and is not waiting for
	// a join. This would be the first thread in the ready list. There is always
	// at least a thread here that matches this condition: the idle thread.
	// Wait-for-join threads are purposedly skipped and we lose the reference to
	// them: it's required for someone to call kthread_join() to free them.
	do {
		th_cur = __thlist_pop(&th_ready);
		assert(th_cur != NULL);
	} while (th_cur->flags & (TH_FLAG_WAITFORJOIN | TH_FLAG_SUSPENDED));
	if (DEBUG_KERNEL) debugf("[kernel] switching to %s(%p) PC=%lx SR=%lx\n", th_cur->name, th_cur, th_cur->stack_state->epc, th_cur->stack_state->sr);
	assert(!(th_cur->flags & TH_FLAG_INLIST));

	// Set the current interrupt depth to that of the current thread.
	__interrupt_depth = th_cur->tls.interrupt_depth;
	__interrupt_sr = th_cur->tls.interrupt_sr;
	#ifdef __NEWLIB__
	_REENT = th_cur->tls.reent_ptr;
	#endif

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
__attribute__((noreturn))
static int __kthread_idle(void *arg)
{
	// When this thread is scheduled, there are no
	// ready threads and all threads are probably waiting for some
	// hardware event to happen. Just spin-wait here.
	while(1) {}
}

kthread_t* kernel_init(void)
{
	assert(!__kernel);
	#ifdef __NEWLIB__
	// Check if __malloc_lock is a nop. This happens with old toolchains where
	// newlib was compiled with --disable-threads.
	extern void __malloc_lock(void);
	assertf(*(uint32_t*)__malloc_lock != 0x03e00008, 
		"Toolchain is outdated and does not support multithreading -- upgrade toolchain to the latest version");
	// Protect from link errors when building with old toolchains without retargetable locks.
	// Those should fall into the previous check anyway.
	#ifdef _RETARGETABLE_LOCKING
	// Check that retargetable locks are correctly implemented. Since a simple change
	// in linker command can cause them to disappear, better have an assert here.
	extern void __retarget_lock_acquire_recursive(_LOCK_T lock);
	assertf(*(uint32_t*)__retarget_lock_acquire_recursive != 0x03e00008, 
		"Build error: retargetable locks not defined -- check that the linker invokation is correct");
	#endif
	#endif
	th_ready = NULL;  // empty ready list
	th_count = 1; // start with the main thread

	// Configure the main thread
	memset(&th_main, 0, sizeof(th_main));
	th_main.pri = 0;
	th_main.name = "main";
	th_main.stack_size = 0x10000; // see STACK_SIZE in system.c
	th_main.flags = TH_FLAG_DETACHED; // main thread cannot be joined
	#ifdef __NEWLIB__
	th_main.tls.reent_ptr = _REENT;
	#endif

	// NOTE: keep this in sync with system.c
	#define STACK_SIZE 0x10000
	th_main.stack = (char*)0x80000000 + get_memory_size() - STACK_SIZE;

	uint64_t *s = (uint64_t*)th_main.stack;
	for (int i=0;i<STACK_GUARD/8;i++)
		s[i] = STACK_COOKIE;

	#ifndef NDEBUG
	assertf(__kernel_all_threads == NULL, "all threads list not empty");
	__kernel_all_threads = &th_main;
	#endif

	// The main thread is the currently scheduled one.
	th_cur = &th_main;

	// Allocate the idle thread
	th_idle = kthread_new("idle", 4096, -128, __kthread_idle, 0);
	kthread_detach(th_idle);

	// Initialize IRQ condition variables
	__kirq_init();

	__kernel = true;
	return th_cur;
}

void kernel_close(void)
{
	assert(__kernel);
	assertf(th_cur == &th_main, "kthread_close can only be called from main thread");

	// Kill the idle thread to release the memory
	kthread_kill(th_idle, 0);
	th_idle = NULL;

	assertf(th_count == 1, "not all threads were killed");

	th_cur = NULL;
	__kernel = false;
	__isr_force_schedule = false;
}

kthread_t* __kthread_new_internal(const char *name, int stack_size, int8_t pri, uint8_t flag, int (*user_entry)(void*), void *user_data)
{
	assert((stack_size % 8) == 0);

	// Allocate memory for the stack, the stack guard area, and the kthread_t
	// structure
	//
	// |             |                        |             |
	// | Stack guard |   Stack                | kthread_t   | Extra (TLS, etc)
	// |             |                        |             |
	//
	//
	// For the kthread structure, we will use the final part of
	// the buffer: since the stack grows downward, this makes sure that
	// overflowing the stack doesn't corrupt the thread structure, which might
	// make debugging more difficult and might even cause the kernel to crash.

	int extra_size = 0;
	#ifdef __NEWLIB__
	extra_size += sizeof(struct _reent);
	#endif
	void *thmem = malloc(STACK_GUARD + stack_size + sizeof(kthread_t) + extra_size);
	assertf(thmem, "out of free memory");
	kthread_t *th = thmem + STACK_GUARD + stack_size;
	void *extra = thmem + STACK_GUARD + stack_size + sizeof(kthread_t);

	// Initialize the thread structure
	memset(th, 0, sizeof(kthread_t));
	th->stack = thmem;

	th->name = name;
	th->user_entry = user_entry;
	th->user_data = user_data;
	th->pri = pri;
	th->stack_size = stack_size;

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
	th->stack_state->gp = REG_GP();

	// SP: top of stack
	th->stack_state->sp = (int64_t)(int32_t)top_stack;

	disable_interrupts();
	th_count++;
	__thlist_add_pri(&th_ready, th);
	#ifndef NDEBUG
	// Add the thread to the all-threads list for debugging purposes
	th->all_next = __kernel_all_threads;
	__kernel_all_threads = th;
	#endif
	enable_interrupts();

	// TLS initial configuration
	#ifdef __NEWLIB__
	th->tls.reent_ptr = extra;
	_REENT_INIT_PTR(th->tls.reent_ptr);
	#endif

	// If the new thread has a priority higher or equal to the current one,
	// yield immediately so that it will get started. This preserves the basic
	// invariant that a higher priority thread is immediately scheduled whenever
	// it becomes ready.
	if (th->pri >= th_cur->pri)
		kthread_yield();

	return th;
}

kthread_t* kthread_new(const char *name, int stack_size, int8_t pri, int (*user_entry)(void*), void *user_data)
{
	return __kthread_new_internal(name, stack_size, pri, 0, user_entry, user_data);	
}

kthread_t* kthread_current(void)
{
	return th_cur;
}

void kthread_kill(kthread_t *th, int res)
{
	if (th == NULL) th = th_cur;
	
	if (DEBUG_KERNEL) debugf("killing: %s[%p] (flags:%x)\n", th->name, th, th->flags);
	
	disable_interrupts();
	th_count--;

	if (th->flags & TH_FLAG_DETACHED) {
		// In general, we cannot just free the memory. If the thread is enqueued
		// in some list (eg: waiting on a mailbox), we would need to remove it
		// from that list before freeing it, but we don't have a way to do it
		// (a thread doesn't have a reference to the list it's in, just a next
		// pointer).
		// So just mark the thread as zombie. This will cause the scheduler to
		// free it as soon as the thread is first scheduled.
		th->flags |= TH_FLAG_ZOMBIE;
	} else {
		if (th->joiner) {
			th->joiner->joined_result = res;
			__thlist_add_pri(&th_ready, th->joiner);
		} else {
			th->flags |= TH_FLAG_WAITFORJOIN;
			th->joined_result = res;
		}
	}

	// If we're trying to kill ourselves, it's sufficient to force a context
	// switch now. This thread will be killed immediately by the scheduler.
	if (th == th_cur)
		KTHREAD_SWITCH();

	enable_interrupts();
}

void kthread_exit(int res)
{
	kthread_kill(NULL, res);
	__builtin_unreachable();
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

void kthread_suspend(kthread_t *th)
{
	disable_interrupts();
	th->flags |= TH_FLAG_SUSPENDED;
	enable_interrupts();
}

void kthread_resume(kthread_t *th)
{
	disable_interrupts();
	th->flags &= ~TH_FLAG_SUSPENDED;
	enable_interrupts();
}


#define kernel_preempt_disable() asm volatile ("addiu $k1, 1")
#define kernel_preempt_enable()  asm volatile ("addi $k1, -1")

void kthread_detach(kthread_t *th)
{
	if (th == NULL) th = th_cur;
	// disable_interrupts();
	kernel_preempt_disable();
	assertf((th->flags & TH_FLAG_WAITFORJOIN) == 0, "cannot detach thread %s[%p] which is already exited", th->name, th);
	th->flags |= TH_FLAG_DETACHED;
	kernel_preempt_enable();
	// enable_interrupts();
}

int kthread_join(kthread_t *th)
{
	disable_interrupts();
	assertf(!(th->flags & TH_FLAG_DETACHED), "cannot join a detached thread %s[%p]", th->name, th);

	if (th->flags & TH_FLAG_WAITFORJOIN)
	{
		th_cur->joined_result = th->joined_result;
		kthread_free(th);
	}
	else
	{
		// Add the current thread to the joiner list, and then force a context switch.
		assertf(th->joiner == NULL, "thread %s[%p] already joined by %s[%p]", th->name, th, th->joiner->name, th->joiner);
		th->joiner = th_cur;
		KTHREAD_SWITCH();
	}

	enable_interrupts();
	return th_cur->joined_result;
}

bool kthread_try_join(kthread_t *th, int *res)
{
	bool joined = false;

	disable_interrupts();
	assertf((th->flags & TH_FLAG_DETACHED), "cannot join a detached thread %s[%p]", th->name, th);

	if (th->flags & TH_FLAG_WAITFORJOIN)
	{
		th_cur->joined_result = th->joined_result;
		if (res) *res = th->joined_result;
		kthread_free(th);
		joined = true;
	}

	enable_interrupts();
	return joined;
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

int kthread_backtrace(kthread_t *th, void *buffer, int size)
{
	if (th == NULL) th = th_cur;
	
	if (th == th_cur)
		return backtrace(buffer, size);

	return __backtrace_from(buffer, size, 
		(void*)(uint32_t)th->stack_state->epc, 
		(void*)(uint32_t)th->stack_state->sp,
		(void*)(uint32_t)th->stack_state->fp,
		(void*)(uint32_t)th->stack_state->ra);
}

const char *kthread_name(kthread_t *th)
{
	if (th == NULL) th = th_cur;
	return th->name;
}

void kmutex_init(kmutex_t *mutex, uint8_t flags)
{
	assertf(mutex, "out of free memory");
	((uint32_t*)mutex)[0] = 0;
	((uint32_t*)mutex)[1] = 0;
	mutex->flags = flags;
}

void kmutex_destroy(kmutex_t *mutex)
{
	kthread_t *owner = (void*)(mutex->owner | 0x80000000);
	assertf(!mutex->owner, "kmutex_destroy() called, but mutex is locked by %s[%p]", owner->name, owner);
	assertf(!mutex->waiting, "kmutex_destroy() called, but threads are waiting");
}

void kmutex_lock(kmutex_t *mutex)
{
	kthread_t *th = th_cur;

	disable_interrupts();
	if (mutex->owner == PhysicalAddr(th))
	{
		assertf(mutex->flags & KMUTEX_RECURSIVE, "a non-recursive mutex cannot be locked twice");
		mutex->counter++;
	}
	else
	{
		while (mutex->owner)
		{
			__phys_thlist_add_pri(mutex->waiting, th);
			KTHREAD_SWITCH();
		}
		mutex->owner = PhysicalAddr(th);
		mutex->counter = 1;
	}
	enable_interrupts();
}

bool kmutex_try_lock(kmutex_t *mutex, uint32_t ticks)
{
	kthread_t *th = th_cur;
	bool locked = false;

	disable_interrupts();
	if (mutex->owner == PhysicalAddr(th))
	{
		assertf(mutex->flags & KMUTEX_RECURSIVE, "a non-recursive mutex cannot be locked twice");
		mutex->counter++;
		locked = true;
	}
	else if (!mutex->owner)
	{
		mutex->owner = PhysicalAddr(th);
		mutex->counter = 1;
		locked = true;
	}
	else if (ticks > 0)
	{
		bool timeout = false;

		// Timer callback. This will be invoked when the timer elapses after the
		// requested delay.
		void cb(int ovlf)
		{
			if (DEBUG_KERNEL) debugf("[kernel] mutex timeout %s[%p]\n", th->name, th);

			// Put the thread again in the ready list, and forces a context switch.
			// Avoid race conditions here: if the thread is not anymore in the wait list,
			// don't add it to the ready list.
			timeout = true;
			if (__phys_thlist_remove(mutex->waiting, th))
				__thlist_add_pri(&th_ready, th);
			KTHREAD_SWITCH_ISR();
		}

		// Start a timer for the specified delay.
		timer_link_t timer;
		start_timer(&timer, ticks, TF_ONE_SHOT, cb);

		while (mutex->owner && !timeout)
		{
			__phys_thlist_add_pri(mutex->waiting, th);
			KTHREAD_SWITCH();
		}
		if (!timeout)
		{
			stop_timer(&timer);
			mutex->owner = PhysicalAddr(th);
			mutex->counter = 1;
			locked = true;
		}
	}

	enable_interrupts();
	return locked;
}

__attribute__((noinline))
static bool kmutex_unlock_internal(kmutex_t *mutex)
{
	bool highpri = false;

	mutex->counter--;
	if (mutex->counter == 0)
	{
		mutex->owner = 0;
		highpri = __phys_thlist_splice_pri(&th_ready, mutex->waiting);
	}
	return highpri;
}

void kmutex_unlock(kmutex_t *mutex)
{
	kthread_t *th = th_cur;

	disable_interrupts();
	assertf(mutex->owner == PhysicalAddr(th), "mutex_unlock() called, but mutex is not locked by %s[%p]", th->name, th);
	assertf(mutex->counter > 0, "mutex_unlock() called, but mutex is not locked");

	// Unlock the mutex, and yield in case a higher priority thread is awaken
	if (kmutex_unlock_internal(mutex))
		kthread_yield();

	enable_interrupts();
}

void kcond_init(kcond_t *cond)
{
	memset(cond, 0, sizeof(kcond_t));
}

void kcond_destroy(kcond_t *cond)
{
	assertf(cond->waiting == NULL, "kcond_free() called, but some threads were waiting");
}

void kcond_signal(kcond_t *cond)
{
	disable_interrupts();
	// Wake up the highest-priority thread in the waiting list
	kthread_t *th = __thlist_pop(&cond->waiting);
	if (th) {
		__thlist_add_pri(&th_ready, th);
		if (th_cur->pri < th->pri)
			kthread_yield();
	}
	enable_interrupts();
}

void kcond_broadcast(kcond_t *cond)
{
	disable_interrupts();
	// Wake up all threads in the waiting list
	if (__thlist_splice_pri(&th_ready, &cond->waiting))
		kthread_yield();
	enable_interrupts();
}

void __kcond_broadcast_isr(kcond_t* cond)
{
	// This is a special version of kcond_broadcast that can be called
	// from an interrupt handler.
	if (__thlist_splice_pri(&th_ready, &cond->waiting))
		KTHREAD_SWITCH_ISR();
}

void kcond_wait(kcond_t *cond, kmutex_t *mutex)
{
	kthread_t *th = th_cur;

	disable_interrupts();
	if (mutex) {
		assertf(mutex->owner == PhysicalAddr(th), "kcond_wait() called, but mutex is not locked by %s[%p]", th->name, th);
		assertf(mutex->counter != 1, "kcond_wait() called, but mutex is locked multiple times");

		// Unlock the mutex, and put the thread in the cond waiting list
		kmutex_unlock_internal(mutex);
	}
	__thlist_add_pri(&cond->waiting, th);

	// Context switch
	KTHREAD_SWITCH();

	// Try to re-lock the mutex
	if (mutex) kmutex_lock(mutex);
	enable_interrupts();
}

bool kcond_wait_timeout(kcond_t *cond, kmutex_t *mutex, uint32_t ticks)
{
	kthread_t *th = th_cur;
	bool timeout = false;

	disable_interrupts();
	assertf(mutex->owner == PhysicalAddr(th), "kcond_wait_timeout() called, but mutex is not locked by %s[%p]", th->name, th);
	assertf(mutex->counter != 1, "kcond_wait_timeout() called, but mutex is locked multiple times");

	// Unlock the mutex, and put the thread in the cond waiting list
	kmutex_unlock_internal(mutex);
	__thlist_add_pri(&cond->waiting, th);

	// Timer callback. This will be invoked when the timer elapses after the
	// requested delay.
	void cb(int ovlf)
	{
		if (DEBUG_KERNEL) debugf("[kernel] cond timeout %s[%p]\n", th->name, th);

		// Put the thread again in the ready list, and forces a context switch.
		// Avoid race conditions here: if the thread is not anymore in the wait list,
		// don't add it to the ready list.
		timeout = true;
		if (__thlist_remove(&cond->waiting, th))
			__thlist_add_pri(&th_ready, th);
		KTHREAD_SWITCH_ISR();
	}

	// Start a timer for the specified delay.
	timer_link_t timer;
	start_timer(&timer, ticks, TF_ONE_SHOT, cb);

	// Context switch
	KTHREAD_SWITCH();

	if (!timeout) stop_timer(&timer);

	kmutex_lock(mutex);
	enable_interrupts();
	return !timeout;
}
