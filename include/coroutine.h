#ifndef LIBDRAGON_COROUTINE_H
#define LIBDRAGON_COROUTINE_H

#include "ucontext.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct coroutine {
    ucontext_t ctx;
    void (*fn)(void *);
    void *arg;
    void *stack;
    size_t stack_size;
    int finished;
    uint64_t wakeup_ticks;
    struct coroutine *caller;
} coroutine_t;

coroutine_t *corot_create(void (*fn)(void *), void *arg, size_t stack_size);

void corot_resume(coroutine_t *co);

void corot_yield(void);

void corot_sleep(uint64_t ticks);

void corot_destroy(coroutine_t *co);

inline static int corot_finished(coroutine_t *co)
{
  return co ? co->finished : 1;
}

#ifdef __cplusplus
}
#endif

#endif

