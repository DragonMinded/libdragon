/**
 * @file kernel_internal.h
 * @brief Internal Kernel Definitions
 * @ingroup kernel
 */
#ifndef __LIBDRAGON_KERNEL_INTERNAL_H
#define __LIBDRAGON_KERNEL_INTERNAL_H

#include "kernel.h"

/**
 * @addtogroup kernel
 * @{
 */

#define TH_FLAG_ZOMBIE      (1<<0)      ///< The thread is a zombie (dead, waiting for cleanup)
#define TH_FLAG_INLIST      (1<<1)      ///< The thread is in a list (ready or waiting)
#define TH_FLAG_DETACHED    (1<<2)      ///< The thread is detached (no one will join it)
#define TH_FLAG_WAITFORJOIN (1<<3)      ///< The non-detached thread is finished and is waiting for a join

/**
 * @brief a kernel thread for parallel execution
 *
 * This structure represents a thread that can be scheduled
 * for parallel execution. Create a thread with #kthread_new,
 * allocating the required memory for the stack.
 *
 * Normally, there's no need to explicitly manage the lifetime
 * or state of a thread. Once the thread is created, it is
 * immediately started, and the kernel will scheduled it when
 * required. If the thread exits (by simply returning by its
 * entry point function), it will be cleaned up and the memory
 * released.
 *
 * #kthread_new returns a pointer to the thread (a pointer
 * to kthread_t) that can be used to externally manage the
 * thread like changing its priority or killing it.
 */
typedef struct kthread_s
{
	/** Pointer to the top of the stack, which contains the thread register state */
	reg_block_t* stack_state;
	/** Current state of interrupt depth for this thread */
	int interrupt_depth;
	/** Size of the stack in bytes */
	int stack_size;
	/** Name of thread (for debugging purposes) */
	const char *name;
	/** Internal flags */
	uint8_t flags;
	/** Priority of the thread (0=lowest, use positive numbers only) */
	int8_t pri;
    /** Thread that is waiting for this one to finish */
	struct kthread_s *joiner;
    /** Result code for the thread that was joined */
    int joined_result;
	/** Intrusive link to next thread in a waiting list */
	struct kthread_s *next;
	/** Entry point function for the thread */
	int (*user_entry)(void*);
	/** Custom argument to be passed to the entry point */
	void *user_data;
	/** Pointer to the stack buffer */
	void *stack;
} kthread_t;

/** Kernel initialization flag. */
extern bool __kernel;

/** Trigger the specified event, broadcasted to all threads waiting for it. */
void kevent_trigger(kevent_t* evt);

/** Same as #kevent_trigger, but to be called under interrupt. */
void kevent_trigger_isr(kevent_t* evt);

/** @brief Syscall handler to run the scheduler and optionally switch current thread */
reg_block_t* __kthread_syscall_schedule(reg_block_t *stack_state);

/** @brief Internal thread creation function with also flags */
kthread_t* __kthread_new_internal(const char *name, int stack_size, int8_t pri, uint8_t flag, int (*user_entry)(void*), void *user_data);

/** @} */ /* kernel */

#endif
