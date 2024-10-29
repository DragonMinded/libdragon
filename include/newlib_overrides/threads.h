#ifndef LIBDRAGON_KERNEL_CTHREADS_H
#define LIBDRAGON_KERNEL_CTHREADS_H

#include <time.h>
#include "n64sys.h"
#include "kernel.h"

#ifndef CTHREADS_DEFAULT_STACK_SIZE
#define CTHREADS_DEFAULT_STACK_SIZE     (4*1024)
#endif

typedef kthread_t* thrd_t;

enum {
    thrd_success = 0,
    thrd_nomem = -1,
    thrd_timedout = -2,
    thrd_busy = -3,
    thrd_error = -999,
};

typedef int (*thrd_start_t)(void*);

int thrd_create_ex(thrd_t *thr, const char *name, int stack_size, int pri, thrd_start_t func, void *arg);

inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    return thrd_create_ex(thr, "<unnamed>", CTHREADS_DEFAULT_STACK_SIZE, 0, func, arg);
}

inline int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return lhs == rhs;
}

inline thrd_t thrd_current(void) {
    return kthread_current();
}

inline void thrd_yield(void) {
    kthread_yield();
}

__attribute__((noreturn))
inline void thrd_exit(int res)
{
    kthread_exit(res);
}

inline int thrd_detach(thrd_t thr)
{
    kthread_detach(thr);
    return thrd_success;
}

int thrd_join(thrd_t thr, int *res);
int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

typedef kmutex_t mtx_t;

enum {
    mtx_plain = 1<<0,
    mtx_recursive = 1<<1,
    mtx_timed = 1<<2,
};

int mtx_init(mtx_t *mutex, int type);

inline int mtx_lock(mtx_t* mutex) {
    kmutex_lock(mutex);
    return thrd_success;
}
inline int mtx_trylock(mtx_t *mutex) {
    return kmutex_try_lock(mutex, 0) ? thrd_success : thrd_busy;
}

int mtx_timedlock(mtx_t *restrict mutex, const struct timespec *restrict time_point);

inline int mtx_unlock(mtx_t *mutex) {
    kmutex_unlock(mutex);
    return thrd_success;
}
inline void mtx_destroy(mtx_t *mutex) {
    kmutex_destroy(mutex);
}

typedef kcond_t cnd_t;

inline int cnd_init(cnd_t *cond) {
    kcond_init(cond);
    return thrd_success;
}
inline int cnd_signal(cnd_t *cond) {
    kcond_signal(cond);
    return thrd_success;
}
inline int cnd_broadcast(cnd_t *cond) {
    kcond_broadcast(cond);
    return thrd_success;
}
inline int cnd_wait(cnd_t *cond, mtx_t *mutex) {
    kcond_wait(cond, mutex);
    return thrd_success;
}

int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *time_point);

inline void cnd_destroy(cnd_t *cond) {
    kcond_destroy(cond);
}

#endif
