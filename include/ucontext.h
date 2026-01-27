/**
 * @file ucontext.h
 * @author Max Bebök <beboek.max@gmail.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief ucontext POSIX API
 * @ingroup lowlevel
 */

#ifndef LIBDRAGON_UCONTEXT_H
#define LIBDRAGON_UCONTEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Defines the stack used for a context.
 */
typedef struct stack_s {
  void     *ss_sp;        ///< stack base or pointer
  size_t    ss_size;      ///< stack size
  int       ss_flags;     ///< flags
} stack_t;

/**
 * Hardware specific data that needs backing up.
 * This contains relevant register that are not baked up by
 * the compiler itself during a function call.
 */
typedef struct mcontext_s {
  uint64_t s[8];       ///< saved saved registers $s0-$s7
  uint64_t fp;         ///< saved frame pointer
  uint32_t ra;         ///< saved return address
  uint32_t a[1];       ///< saved initial entry argument $a0  
  uint64_t f[12];      ///< saved floating point registers $f0-$f11
  uint32_t fc32;       ///< saved floating point control register 32
} mcontext_t;

/**
 * System-V compatible struct that defines a context.
 * This includes an allocated stack, as well as currently used registers.
 * 
 * It can be created via makecontext, and later switched to with swapcontext.
 */
typedef struct ucontext_s {
    struct ucontext_s *uc_link;   ///< pointer to the context that will be resumed when this context returns
    stack_t uc_stack;             ///< stack used for the context
    mcontext_t uc_mcontext;       ///< machine context for the context
} ucontext_t;

// @TODO: allow proper varargs (is that even needed in practice?)

/**
 * @brief creates a new context to be used with swapcontext.
 * 
 * @param ctx context to write data to
 * @param entry entry point when first switched to
 * @param argc number of arguments to pass into the initial entry
 * @param arg0 arguments for the initial entry
 */
void makecontext(ucontext_t *ctx, void (*entry)(void), int argc, uint32_t arg0);

/**
 * @brief Swaps context
 * @param old_ctx 
 * @param new_ctx 
 */
extern void swapcontext(ucontext_t *old_ctx, const ucontext_t *new_ctx);

#ifdef __cplusplus
}
#endif

#endif
