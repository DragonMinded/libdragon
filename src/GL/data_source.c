#include "data_source.h"
#include "gl_internal.h"
#include "array.h"
#include "array_object.h"
#include "array_convert.h"

void data_source_init(data_source_t *data_source, const array_t *arrays, array_mask_t array_mask)
{
    *data_source = (data_source_t){0};
    data_source->arrays = arrays;
    data_source->array_mask = array_mask;
    data_source->arrays_dirty = true;
}

void data_source_invalidate_arrays(data_source_t *data_source)
{
    data_source->arrays_dirty = true;
}

static bool is_array_included(data_source_t *data_source, array_type_t type)
{
    return (data_source->array_mask & (1<<type)) != 0;
}

static bool is_array_used(data_source_t *data_source, array_type_t type)
{
    return is_array_included(data_source, type) && data_source->arrays[type].enabled;
}

static bool is_array_external(const array_t *array) {
    return array->binding == NULL;
}

static bool calculate_is_fully_internal(data_source_t *data_source)
{
    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        if (!is_array_used(data_source, i)) continue;

        if (is_array_external(&data_source->arrays[i])) {
            return false;
        }
    }
    
    return true;
}

static void update_is_fully_internal(data_source_t *data_source)
{
    data_source->is_fully_internal = calculate_is_fully_internal(data_source);
}

static void update_layout(data_source_t *data_source)
{
    uint32_t stride = 0;
    uint32_t array_count = 0;

    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        if (!is_array_used(data_source, i)) continue;

        uint32_t alignment = array_type_get_alignment(i);
        stride = ROUND_UP(stride, alignment);

        uint32_t offset = stride;

        data_source->layout.offsets[array_count++] = offset;
        stride += array_type_get_stride(i);
    }

    data_source->layout.stride = stride;
}

static void update_state_from_arrays(data_source_t *data_source)
{
    if (!data_source->arrays_dirty) {
        return;
    }

    update_is_fully_internal(data_source);
    update_layout(data_source);

    data_source->arrays_dirty = false;
}

bool data_source_is_fully_internal(data_source_t *data_source)
{
    update_state_from_arrays(data_source);
    return data_source->is_fully_internal;
}

static void get_array_convert_parms(array_convert_parms_t *parms, data_source_t *data_source, void *buffer, index_bounds_t bounds)
{
    parms->array_count = 0;

    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        if (!is_array_used(data_source, i)) continue;

        parms->arrays[parms->array_count++] = &data_source->arrays[i];
    }

    parms->out_layout = &data_source->layout;
    parms->range = bounds;
    parms->out_buffer = buffer;
}

void data_source_pull(data_source_t *data_source, void *buffer, index_bounds_t bounds)
{
    update_state_from_arrays(data_source);

    array_convert_parms_t convert_parms;
    get_array_convert_parms(&convert_parms, data_source, buffer, bounds);
    array_convert(&convert_parms);
}

uint32_t data_source_get_stride(data_source_t *data_source)
{
    update_state_from_arrays(data_source);
    return data_source->layout.stride;
}
