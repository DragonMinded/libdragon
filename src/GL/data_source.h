/**
 * @file data_source.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __DATA_SOURCE_H
#define __DATA_SOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include "indices.h"
#include "array.h"
#include "array_convert.h"

typedef enum {
    ARRAY_MASK_VERTEX = 1 << ARRAY_VERTEX,
    ARRAY_MASK_NORMAL = 1 << ARRAY_NORMAL,
    ARRAY_MASK_COLOR = 1 << ARRAY_COLOR,
    ARRAY_MASK_TEXCOORD = 1 << ARRAY_TEXCOORD,
    ARRAY_MASK_MTX_INDEX = 1 << ARRAY_MTX_INDEX,
} array_mask_t;

/**
 * @brief This is an abstraction that represents the source of RSP-compatible vertex data.
 * 
 * Internally, it sources the data from the GL arrays and converts it using #array_convert.
 */
typedef struct data_source_s {
    const array_t *arrays;      ///< The list of GL arrays.
    array_mask_t array_mask;    ///< A bit mask that describes which entries in #arrays are actually used by this data source.
    data_layout_t layout;       ///< The cached output data layout.
    bool arrays_dirty;          ///< Internal dirty flag that is set to true whenever any of the array states change.
    bool is_fully_internal;     ///< Cached flag that tells whether all arrays point to VBOs.
} data_source_t;

#ifdef __cplusplus
extern "C" {
#endif

void data_source_init(data_source_t *data_source, const array_t *arrays, array_mask_t array_mask);
void data_source_invalidate_arrays(data_source_t *data_source);

/** @brief Returns whether all arrays point to VBOs. Only vertex data from VBOs may be cached, since otherwise it cannot be guaranteed that it has not changed. */
bool data_source_is_fully_internal(data_source_t *data_source);

/** @brief Pulls RSP-compatible vertex data at the specified index bounds into a buffer, converting it from the input format. */
void data_source_pull(data_source_t *data_source, void *buffer, index_bounds_t bounds);

/** @brief Returns the output vertex stride. */
uint32_t data_source_get_stride(data_source_t *data_source);

#ifdef __cplusplus
}
#endif

#endif
