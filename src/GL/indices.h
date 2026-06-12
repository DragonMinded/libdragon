#ifndef __INDICES_H
#define __INDICES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

typedef struct {
    uint16_t first;
    uint16_t count;
} index_bounds_t;

index_bounds_t find_index_bounds(const uint16_t *indices, uint32_t count);

inline uint16_t bounds_get_lower_inclusive(index_bounds_t bounds)
{
    return bounds.first;
}

inline uint16_t bounds_get_upper_exclusive(index_bounds_t bounds)
{
    return bounds.first + bounds.count;
}

inline bool are_bounds_included(index_bounds_t a, index_bounds_t b)
{
    return bounds_get_lower_inclusive(a) <= bounds_get_lower_inclusive(b) &&
        bounds_get_upper_exclusive(a) >= bounds_get_upper_exclusive(b);
}

#endif
