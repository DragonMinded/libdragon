#ifndef __FNV1A_H
#define __FNV1A_H

#include <stdint.h>

inline uint32_t fnv1a_init()
{
    return 0x811c9dc5; // FNV offset basis
}

inline void fnv1a_step(uint32_t *hash, uint32_t v)
{
    *hash ^= v;
    *hash *= 0x01000193; // FNV prime
}

#endif
