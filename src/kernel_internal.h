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

#define TH_FLAG_ZOMBIE  (1<<0)      ///< The thread is a zombie (dead, waiting for cleanup)
#define TH_FLAG_INLIST  (1<<1)      ///< The thread is in a list (ready or waiting)

/** Kernel initialization flag. */
extern bool __kernel;

/** Trigger the specified event, broadcasted to all threads waiting for it. */
void kevent_trigger(kevent_t* evt);

/** Same as #kevent_trigger, but to be called under interrupt. */
void kevent_trigger_isr(kevent_t* evt);

/** @brief Syscall handler to run the scheduler and optionally switch current thread */
reg_block_t* __kthread_syscall_schedule(reg_block_t *stack_state);

/** @} */ /* kernel */

#endif
