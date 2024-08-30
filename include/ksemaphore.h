/**
 * @file ksemaphore.h
 * @brief Kernel Semaphore prmitive
 * @ingroup kernel
 * 
 */

#ifndef LIBDRAGON_KERNEL_KSEMAPHORE_H
#define LIBDRAGON_KERNEL_KSEMAPHORE_H

#include <stdint.h>
#include "kernel.h"

/** @brief Kernel Semaphore structure
 * 
 * This structure represents a semaphore in the kernel. Semaphores are
 * used to synchronize access to shared resources between threads.
 */
typedef struct {
    kmutex_t mutex;
    kcond_t cond;
    int count;
} ksemaphore_t;

/**
 * @brief Initialize a semaphore
 * 
 * This function initializes a semaphore with the specified value.
 * 
 * @param[in] sem      Pointer to the semaphore structure
 * @param[in] value    Initial value of the semaphore
 */
void ksemaphore_init(ksemaphore_t *sem, int value);

/**
 * @brief Destroy a semaphore
 * 
 * This function destroys a semaphore, freeing all the resources
 * associated with it.
 * 
 * @param[in] sem      Pointer to the semaphore structure
 */
void ksemaphore_destroy(ksemaphore_t *sem);

/**
 * @brief Post to a semaphore
 * 
 * This function increments the value of a semaphore and signals
 * any threads waiting on the semaphore.
 * 
 * @param sem       Pointer to the semaphore structure
 */
void ksemaphore_post(ksemaphore_t *sem);

/**
 * @brief Wait on a semaphore
 * 
 * This function waits on a semaphore. If the semaphore's value is
 * greater than 0, the function will decrement the value and return
 * immediately. If the value is 0, the function will block until
 * the semaphore's value is greater than 0.
 * 
 * @param[in] sem      Pointer to the semaphore structure
 */
void ksemaphore_wait(ksemaphore_t *sem);

/**
 * @brief Try to wait on a semaphore, up to a timeout
 * 
 * This function tries to wait on a semaphore, up to a specified
 * timeout. If the semaphore's value is greater than 0, the function
 * will decrement the value and return true. If the value is 0, the
 * function will block until the semaphore's value is greater than 0,
 * or the timeout expires.
 * 
 * As a special case, if the timeout is 0, the function will not block.
 * 
 * @param sem       Pointer to the semaphore structure
 * @param ticks     Timeout in ticks
 * @return true     If the semaphore was successfully waited on
 * @return false    If the semaphore was not successfully waited on
 */
bool ksemaphore_try_wait(ksemaphore_t *sem, uint32_t ticks);

#endif



