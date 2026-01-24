#include <libdragon.h>

void makecontext(
    ucontext_t *ctx,
    void (*entry)(void),
    int argc,
    uint32_t arg0
) {
  assert(argc <= 1);

  memset(&ctx->uc_mcontext, 0, sizeof(ctx->uc_mcontext));
  ctx->uc_mcontext.ra = (uint32_t)entry;
  ctx->uc_stack.ss_sp = (char*)ctx->uc_stack.ss_sp + ctx->uc_stack.ss_size;
  ctx->uc_mcontext.a[0] = (uint32_t)arg0;
} 