#include <libdragon.h>

static coroutine_t *corot_current = NULL;
static ucontext_t sched_ctx;

static void corot_entry(uintptr_t corot_ptr)
{
    coroutine_t *co = (coroutine_t *)corot_ptr;
    co->fn(co->arg);
    co->finished = true;

    // Yield back to the caller forever to avoid returning on a dead stack.
    for (;;) {
      corot_yield();
    }
}

coroutine_t *corot_create(void (*fn)(void *), void *arg, size_t stack_size)
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
    makecontext(&co->ctx, (void (*) (void))corot_entry, 1, (uintptr_t)co);
    return co;
}

void corot_resume(coroutine_t *co)
{
    if (!co || co->finished) return;

    // sleep
    if(get_ticks() < co->wakeup_ticks)return;
    co->wakeup_ticks = 0;

    coroutine_t *prev = corot_current;
    co->caller = prev;
    corot_current = co;

    if (prev) {
        swapcontext(&prev->ctx, &co->ctx);
    } else {
        swapcontext(&sched_ctx, &co->ctx);
    }

    corot_current = prev;
}

void corot_sleep(uint64_t ticks)
{
  assert(corot_current);
  corot_current->wakeup_ticks = get_ticks() + ticks;
  corot_yield();
}

void corot_yield(void)
{
    if (!corot_current) return;

    coroutine_t *cur = corot_current;
    coroutine_t *caller = cur->caller;
    corot_current = caller;

    if (caller) {
        swapcontext(&cur->ctx, &caller->ctx);
    } else {
        swapcontext(&cur->ctx, &sched_ctx);
    }    
}

void corot_destroy(coroutine_t *co)
{
    if (!co) return;
    free(co->stack);
    free(co);
}

coroutine_t* corot_get_current(void) 
{
  return corot_current;
}
