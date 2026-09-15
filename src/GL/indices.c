#include "indices.h"

index_bounds_t find_index_bounds(const uint16_t *indices, uint32_t count)
{
    uint16_t min = USHRT_MAX;
    uint16_t max = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (indices[i] < min) min = indices[i];
        if (indices[i] > max) max = indices[i];
    }

    return (index_bounds_t) { min, max - min + 1 };
}
