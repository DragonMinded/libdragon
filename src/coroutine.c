/**
 * @file coroutine.c
 * @author Max Bebök <beboek.max@gmail.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Cooperative coroutines implementation
 * @ingroup lowlevel
 */

#include "coroutine.h"
#include "utils.h"
#include "n64sys.h"
#include "debug.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

/** @brief Coroutine */
typedef struct coroutine_s {
    ucontext_t ctx;                 ///< Context for the coroutine
    void (*fn)(void *);             ///< Function to execute in the coroutine
    void *arg;                      ///< Argument to pass to the function
    void *stack;                    ///< Stack for the coroutine
    size_t stack_size;              ///< Size of the stack
    bool finished;                  ///< Whether the coroutine has finished
    uint64_t wakeup_ticks;          ///< Time to wake up the coroutine
    struct coroutine_s *caller;     ///< Caller of the coroutine
} coroutine_t;

static coroutine_t *coro_current = NULL;
static ucontext_t sched_ctx;

static void coro_entry(uintptr_t coro_ptr)
{
    coroutine_t *co = (coroutine_t *)coro_ptr;
    co->fn(co->arg);
    co->finished = true;

    // Yield back to the caller forever to avoid returning on a dead stack.
    for (;;) {
      coro_yield();
    }
}

coroutine_t *coro_create(void (*fn)(void *), void *arg, size_t stack_size)
{
    if (!fn || stack_size == 0) return NULL;

    coroutine_t *co = (coroutine_t *)aligned_alloc(8, sizeof(*co));
    if (!co) return NULL;

    memset(co, 0, sizeof(*co));

    co->stack = malloc(stack_size);
    if (!co->stack) {
        free(co);
        return NULL;
    }

    co->fn = fn;
    co->arg = arg;
    co->stack_size = stack_size;

    co->ctx.uc_stack.ss_sp = co->stack;
    co->ctx.uc_stack.ss_size = stack_size;
    co->ctx.uc_link = NULL;
    makecontext(&co->ctx, (void (*) (void))coro_entry, 1, (uintptr_t)co);
    return co;
}

void coro_resume(coroutine_t *co)
{
    if (!co || co->finished) return;

    // sleep
    if(get_ticks() < co->wakeup_ticks)return;
    co->wakeup_ticks = 0;

    coroutine_t *prev = coro_current;
    co->caller = prev;
    coro_current = co;

    if (prev) {
        swapcontext(&prev->ctx, &co->ctx);
    } else {
        swapcontext(&sched_ctx, &co->ctx);
    }

    coro_current = prev;
}

void coro_sleep(uint64_t ticks)
{
  assert(coro_current);
  coro_current->wakeup_ticks = get_ticks() + ticks;
  coro_yield();
}

void coro_yield(void)
{
    if (!coro_current) return;

    coroutine_t *cur = coro_current;
    coroutine_t *caller = cur->caller;
    coro_current = caller;

    if (caller) {
        swapcontext(&cur->ctx, &caller->ctx);
    } else {
        swapcontext(&cur->ctx, &sched_ctx);
    }    
}

void coro_destroy(coroutine_t *co)
{
    if (!co) return;
    free(co->stack);
    free(co);
}

bool coro_finished(coroutine_t *co)
{
    return co->finished;
}

coroutine_t* coro_get_current(void) 
{
  return coro_current;
}
