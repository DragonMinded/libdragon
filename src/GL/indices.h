/**
 * @file indices.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __INDICES_H
#define __INDICES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/** @brief Describes a range of indices. */
typedef struct {
    uint16_t first; ///< The first index contained in the range
    uint16_t count; ///< The number of indices contained in the range
} index_bounds_t;

/** @brief Scans the specified list of indices and determines the lowest and highest indices found. */
index_bounds_t find_index_bounds(const uint16_t *indices, uint32_t count);

inline uint16_t bounds_get_lower_inclusive(index_bounds_t bounds)
{
    return bounds.first;
}

inline uint16_t bounds_get_upper_exclusive(index_bounds_t bounds)
{
    return bounds.first + bounds.count;
}

/** @brief Returns whether index range b is fully within a */
inline bool are_bounds_included(index_bounds_t a, index_bounds_t b)
{
    return bounds_get_lower_inclusive(a) <= bounds_get_lower_inclusive(b) &&
        bounds_get_upper_exclusive(a) >= bounds_get_upper_exclusive(b);
}

#endif
