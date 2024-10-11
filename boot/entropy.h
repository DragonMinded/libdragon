#ifndef BOOT_ENTROPY_H
#define BOOT_ENTROPY_H

#include <stdint.h>

#ifdef ENTROPY_FAR
#define ENTROPY_API __attribute__((far))
#else
#define ENTROPY_API
#endif

register uint32_t __entropy_state asm("k1");

static inline void entropy_init(void)
{
    __entropy_state = 0;
}

ENTROPY_API void entropy_add(uint32_t value);
ENTROPY_API uint32_t entropy_get(void);

#endif /* BOOT_ENTROPY_H */
