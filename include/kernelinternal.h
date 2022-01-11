/**
 * @file kernelinternal.h
 * @brief Internal Kernel Definitions
 * @ingroup kernel
 */
#ifndef __LIBDRAGON_KERNELINTERNAL_H
#define __LIBDRAGON_KERNELINTERNAL_H

#include "kernel.h"

/**
 * @addtogroup kernel
 * @{
 */

#define TH_FLAG_ZOMBIE  (1<<0)
#define TH_FLAG_INLIST  (1<<1)

/** Kernel initialization flag. */
extern bool __kernel;

/** Trigger the specified event, broadcasted to all threads waiting for it. */
void kevent_trigger(kevent_t* evt);

/** Same as #event_trigger, but to be called under interrupt. */
void kevent_trigger_isr(kevent_t* evt);

/** @} */ /* kernel */

#endif
