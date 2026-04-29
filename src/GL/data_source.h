#ifndef __DATA_SOURCE_H
#define __DATA_SOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "indices.h"
#include "array.h"

typedef enum {
    ARRAY_MASK_VERTEX = 1 << ATTRIB_VERTEX,
    ARRAY_MASK_NORMAL = 1 << ATTRIB_NORMAL,
    ARRAY_MASK_COLOR = 1 << ATTRIB_COLOR,
    ARRAY_MASK_TEXCOORD = 1 << ATTRIB_TEXCOORD,
    ARRAY_MASK_MTX_INDEX = 1 << ATTRIB_MTX_INDEX,
} array_mask_t;

typedef struct {
    uint32_t offsets[ATTRIB_COUNT];
    uint32_t stride;
} data_layout_t;

typedef struct data_source_s {
    const gl_array_t *arrays;
    array_mask_t array_mask;
    data_layout_t layout;
    bool arrays_dirty;
    bool is_fully_internal;
} data_source_t;

#ifdef __cplusplus
extern "C" {
#endif

void data_source_init(data_source_t *data_source, const gl_array_t *arrays, array_mask_t array_mask);
void data_source_set_arrays_dirty(data_source_t *data_source);
bool data_source_is_fully_internal(data_source_t *data_source);
void data_source_pull(data_source_t *data_source, void *buffer, index_bounds_t bounds);
uint32_t data_source_get_stride(data_source_t *data_source);

#ifdef __cplusplus
}
#endif

#endif
