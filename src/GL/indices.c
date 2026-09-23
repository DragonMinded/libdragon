/**
 * @file indices.c
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#include "indices.h"
#include "debug.h"

index_bounds_t find_index_bounds(const uint16_t *indices, uint32_t count)
{
    assertf(count > 0, "Count must be greater than 0!");

    uint16_t min = USHRT_MAX;
    uint16_t max = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (indices[i] < min) min = indices[i];
        if (indices[i] > max) max = indices[i];
    }

    return (index_bounds_t) { min, max - min + 1 };
}

extern inline uint16_t bounds_get_lower_inclusive(index_bounds_t bounds);
extern inline uint16_t bounds_get_upper_exclusive(index_bounds_t bounds);
extern inline bool are_bounds_included(index_bounds_t a, index_bounds_t b);
