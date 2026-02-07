/**
 * @file coroutine.h
 * @author Max Bebök <beboek.max@gmail.com>
 * @brief Cooperative coroutines API
 * @ingroup lowlevel
 */

#ifndef LIBDRAGON_COROUTINE_H
#define LIBDRAGON_COROUTINE_H

#include "ucontext.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct coroutine_s coroutine_t;
///@endcond

/**
 * @brief Creates a new coroutine
 * This creates a new execution context for the specified function.
 * After creation, the coroutine can be executed with coro_resume.
 * 
 * @param fn function to use for the coroutine
 * @param arg argument to pass into the function
 * @param stack_size stack size to be allocated
 * @return coroutine_t* created coroutine
 */
coroutine_t *coro_create(void (*fn)(void *), void *arg, size_t stack_size);

/**
 * @brief Resumes execution
 * 
 * Continues to execute the coroutine until the next yield.
 * If the function reached the end, it will no longer be called.
 * To check if a coroutine is done, use coro_finished.
 * 
 * @param co coroutine to resume
 */
void coro_resume(coroutine_t *co);

/**
 * @brief Yields and gives back control to the caller context.
 * 
 * This can be used within the context of a coroutine to pause execution.
 * When doing so, it gives back control to the code that called coro_resume.
 */
void coro_yield(void);

/**
 * @brief Puts the current coroutine to sleep.
 * 
 * This function will yield, and prevent execution of the current coroutine until
 * the given time as passed.
 * Note that the time is a minimum, and the actual time depends on how often you
 * try to execute the function via coro_resume.
 * 
 * This function should be preferred over spin-waiting inside the coroutine,
 * since it avoids a context switch alltogether if the time has not passed yet.
 * 
 * @param ticks minimum time to wait in ticks
 */
void coro_sleep(uint64_t ticks);

/**
 * @brief Frees all resources associated with a coroutine.
 *
 * After this function is called it is no longer safe to resume execution,
 * or interact with the coroutine in any way.
 * 
 * @param co coroutine to free
 */
void coro_destroy(coroutine_t *co);

/**
 * @brief Checks if a coroutine has finished execution.
 * 
 * @param co coroutine to check
 * @return bool true if finished
 */
bool coro_finished(coroutine_t *co);

/**
 * @brief Returns the currently active coroutine
 * 
 * If currently inside a coroutine, it will return the handle.
 * When outside, it returns NULL.
 * 
 * This can also be used to detect and change behaviour of a function
 * depending on if it's running in a coroutine or not.
 * 
 * @return coroutine_t* active coroutine, NULL if in main context
 */
coroutine_t* coro_get_current(void);

#ifdef __cplusplus
}
#endif

#endif

