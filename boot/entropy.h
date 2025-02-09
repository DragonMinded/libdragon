#ifndef BOOT_ENTROPY_H
#define BOOT_ENTROPY_H

#include <stdint.h>

#ifdef ENTROPY_FAR
#define ENTROPY_API __attribute__((far))
#else
#define ENTROPY_API
#endif

// Before boot, store the entropy state in low-mem. We will try as best-effort
// to recover it after warm boot (assuming it wasn't destroyed by the running
// application) so that we don't start from scatch (especially given that
// warm boots don't perform RDRAM init so the entropy pool is low).
#define RDRAM_ENTROPY_STATE   ((volatile uint32_t*)0x800001A4)

register uint32_t __entropy_state asm("k1");

static inline void entropy_init(void)
{
    __entropy_state = 0;
}

ENTROPY_API void entropy_add(uint32_t value);
ENTROPY_API uint32_t entropy_get(void);

#endif /* BOOT_ENTROPY_H */
