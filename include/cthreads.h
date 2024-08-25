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

#endif
