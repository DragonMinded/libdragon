#ifndef __ARRAY_CONVERT_H
#define __ARRAY_CONVERT_H

#include "indices.h"
#include "array.h"
#include "data_source.h"

typedef struct array_convert_parms_s {
    const gl_array_t *arrays[ATTRIB_COUNT];
    uint32_t array_count;
    const data_layout_t *out_layout;
    index_bounds_t range;
    void *out_buffer;
} array_convert_parms_t;

#ifdef __cplusplus
extern "C" {
#endif

void array_convert(array_convert_parms_t *parms);

#ifdef __cplusplus
}
#endif

#endif
