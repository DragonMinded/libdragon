#ifndef __ARRAY_H
#define __ARRAY_H

#include <stdint.h>
#include "GL/gl.h"

typedef enum {
    ARRAY_VERTEX,
    ARRAY_NORMAL,
    ARRAY_COLOR,
    ARRAY_TEXCOORD,
    ARRAY_MTX_INDEX,
    ARRAY_COUNT
} array_type_t;

typedef struct gl_buffer_object_s gl_buffer_object_t;

typedef void (*cpu_read_attrib_func)(void*,const void*,uint32_t);
typedef void (*rsp_read_attrib_func)(void*,const void*,uint32_t);

typedef struct array_s {
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *pointer;
    gl_buffer_object_t *binding;
    bool normalize;
    bool enabled;

    const GLvoid *final_pointer;
    uint16_t final_stride;
    cpu_read_attrib_func cpu_read_func;
    rsp_read_attrib_func rsp_read_func;
} array_t;

#ifdef __cplusplus
extern "C" {
#endif

inline uint32_t array_type_get_stride(array_type_t type)
{
    switch (type)
    {
        case ARRAY_VERTEX:
            return sizeof(int16_t) * 4;
        case ARRAY_NORMAL:
            return 0;
        case ARRAY_COLOR:
            return sizeof(uint32_t);
        case ARRAY_TEXCOORD:
            return sizeof(int16_t) * 2;
        case ARRAY_MTX_INDEX:
            return sizeof(uint8_t);
        default:
            return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif
