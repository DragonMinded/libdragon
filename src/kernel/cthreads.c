#include "cthreads.h"
#include "kernel.h"
#include "kernel_internal.h"
#include "interrupt.h"
#include <sys/time.h>

int thrd_create_ex(thrd_t *thr, const char *name, int stack_size, int pri, thrd_start_t func, void *arg)
{
    kthread_t *th = __kthread_new_internal(name, stack_size, pri, 0, func, arg);
    if (th == NULL)
        return thrd_nomem;
    *thr = th;
    return thrd_success;
}

int thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    uint64_t ticks = TICKS_FROM_US((uint64_t)duration->tv_sec * 1000000 + (uint64_t)duration->tv_nsec / 1000);
    kthread_sleep(ticks);
    if (remaining) memset(remaining, 0, sizeof(struct timespec));
    return 0;
}

int thrd_join(thrd_t thr, int *res) 
{
    int result = kthread_join(thr);
    if (res) *res = result;
    return thrd_success;
}

int mtx_init(mtx_t *mtx, int type)
{
    kmutex_init(mtx, type & mtx_recursive ? KMUTEX_RECURSIVE : KMUTEX_STANDARD);
    return thrd_success;
}

static uint64_t ticks_until(const struct timespec *time_point)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    // calculate the remaining time
    struct timespec remaining;
    remaining.tv_sec = time_point->tv_sec - now.tv_sec;
    remaining.tv_nsec = time_point->tv_nsec - now.tv_usec * 1000;

    // convert to ticks
    return TICKS_FROM_US((uint64_t)remaining.tv_sec * 1000000 + (uint64_t)remaining.tv_nsec / 1000);
}

int mtx_timedlock(mtx_t *restrict mutex, const struct timespec *restrict time_point) {
    return kmutex_try_lock(mutex, ticks_until(time_point)) ? thrd_success : thrd_busy;
}

int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *time_point) {
    return kcond_wait_timeout(cond, mutex, ticks_until(time_point)) ? thrd_success : thrd_busy;
}

extern inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
extern inline thrd_t thrd_current(void);
extern inline inline void thrd_yield(void);
extern inline int thrd_equal(thrd_t lhs, thrd_t rhs);
