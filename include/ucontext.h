#ifndef LIBDRAGON_UCONTEXT_H
#define LIBDRAGON_UCONTEXT_H

#include <libdragon.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stack_s {
  void     *ss_sp;    // stack base or pointer
  size_t    ss_size;  // stack size
  int       ss_flags; // flags
} stack_t;

typedef struct mcontext_s {
  uint32_t s[8]; // $s0-$s7
  uint32_t ra;
  uint32_t a[1]; // $a0 (initial entry argument)
  uint32_t f[12];
  uint32_t fc32;
} mcontext_t;

typedef struct ucontext_s {
    struct ucontext_s *uc_link;  // pointer to the context that will be resumed when this context returns
    stack_t uc_stack; // the stack used by this context
    mcontext_t uc_mcontext;
} ucontext_t;

// @TODO: sync with offsets in ASM
static_assert(offsetof(ucontext_t, uc_stack) == 4);
static_assert(offsetof(ucontext_t, uc_mcontext) == 16);
static_assert(offsetof(mcontext_t, ra) == 32);
static_assert(offsetof(mcontext_t, f) == 40);

// @TODO: allow proper varargs (is that even needed in practice?)
void makecontext(
  ucontext_t *ctx,
  void (*entry)(void),
  int argc,
  uint32_t arg0
);

/**
 * @brief Swaps context
 * @param old_ctx 
 * @param new_ctx 
 */
extern void swapcontext(
    ucontext_t *old_ctx,
    const ucontext_t *new_ctx
);

/**
 * @brief Get current context
 * @param ctx 
 */
extern void getcontext(
    ucontext_t *ctx
);

#ifdef __cplusplus
}
#endif

#endif