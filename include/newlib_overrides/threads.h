/**
 * @file threads.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Newlib override for threads.h, for C11 thread implementation.
 */
#ifndef LIBDRAGON_KERNEL_CTHREADS_H
#define LIBDRAGON_KERNEL_CTHREADS_H

#include <time.h>
#include "n64sys.h"
#include "kernel.h"

/** @brief Default stack size for threads */
#ifndef CTHREADS_DEFAULT_STACK_SIZE
#define CTHREADS_DEFAULT_STACK_SIZE     (4*1024)
#endif

/** @brief Thread handle type */
typedef kthread_t* thrd_t;

enum {
    thrd_success = 0,
    thrd_nomem = -1,
    thrd_timedout = -2,
    thrd_busy = -3,
    thrd_error = -999,
};

/** @brief Thread start function type */
typedef int (*thrd_start_t)(void*);

/** @brief Create a thread with extended parameters */
int thrd_create_ex(thrd_t *thr, const char *name, int stack_size, int pri, thrd_start_t func, void *arg);

/** @brief Create a thread with default parameters */
inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    return thrd_create_ex(thr, "<unnamed>", CTHREADS_DEFAULT_STACK_SIZE, 0, func, arg);
}

/** @brief Compare two thread handles for equality */
inline int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return lhs == rhs;
}

/** @brief Get the current thread handle */
inline thrd_t thrd_current(void) {
    return kthread_current();
}

/** @brief Yield the current thread */
inline void thrd_yield(void) {
    kthread_yield();
}

/** @brief Exit the current thread */
__attribute__((noreturn))
inline void thrd_exit(int res)
{
    kthread_exit(res);
}

/** @brief Detach a thread */
inline int thrd_detach(thrd_t thr)
{
    kthread_detach(thr);
    return thrd_success;
}

/** @brief Join a thread */
int thrd_join(thrd_t thr, int *res);
/** @brief Sleep for a specified duration */
int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

/** @brief Mutex type */
typedef kmutex_t mtx_t;

enum {
    mtx_plain = 1<<0,
    mtx_recursive = 1<<1,
    mtx_timed = 1<<2,
};

/** @brief Initialize a mutex */
int mtx_init(mtx_t *mutex, int type);

/** @brief Lock a mutex */
inline int mtx_lock(mtx_t* mutex) {
    kmutex_lock(mutex);
    return thrd_success;
}
/** @brief Try to lock a mutex */
inline int mtx_trylock(mtx_t *mutex) {
    return kmutex_try_lock(mutex, 0) ? thrd_success : thrd_busy;
}

/** @brief Lock a mutex with timeout */
int mtx_timedlock(mtx_t *restrict mutex, const struct timespec *restrict time_point);

/** @brief Unlock a mutex */
inline int mtx_unlock(mtx_t *mutex) {
    kmutex_unlock(mutex);
    return thrd_success;
}
/** @brief Destroy a mutex */
inline void mtx_destroy(mtx_t *mutex) {
    kmutex_destroy(mutex);
}

/** @brief Condition variable type */
typedef kcond_t cnd_t;

/** @brief Initialize a condition variable */
inline int cnd_init(cnd_t *cond) {
    kcond_init(cond);
    return thrd_success;
}
/** @brief Signal a condition variable */
inline int cnd_signal(cnd_t *cond) {
    kcond_signal(cond);
    return thrd_success;
}
/** @brief Broadcast to a condition variable */
inline int cnd_broadcast(cnd_t *cond) {
    kcond_broadcast(cond);
    return thrd_success;
}
/** @brief Wait on a condition variable */
inline int cnd_wait(cnd_t *cond, mtx_t *mutex) {
    kcond_wait(cond, mutex);
    return thrd_success;
}

/** @brief Wait on a condition variable with timeout */
int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *time_point);

/** @brief Destroy a condition variable */
inline void cnd_destroy(cnd_t *cond) {
    kcond_destroy(cond);
}

#endif
