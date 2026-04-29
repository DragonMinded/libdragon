#include "array_convert.h"

void array_convert(array_convert_parms_t *parms)
{
    for (size_t i = 0; i < parms->range.count; i++)
    {
        for (size_t j = 0; j < parms->array_count; j++)
        {
            uint8_t *dst = ((uint8_t*)parms->out_buffer) + parms->out_layout->offsets[j] + i * parms->out_layout->stride;
            const gl_array_t *array = parms->arrays[j];
            const uint8_t *src = ((const uint8_t*)array->final_pointer) + (i+parms->range.first) * array->final_stride;
            array->rsp_read_func(dst, src, array->size);
        }
    }
}
