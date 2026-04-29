#ifndef __ARRAY_H
#define __ARRAY_H

#include <stdint.h>
#include "GL/gl.h"
#include "types.h"

typedef enum {
    ATTRIB_VERTEX,
    ATTRIB_NORMAL,
    ATTRIB_COLOR,
    ATTRIB_TEXCOORD,
    ATTRIB_MTX_INDEX,
    ATTRIB_COUNT
} gl_array_type_t;

typedef struct gl_buffer_object_s gl_buffer_object_t;

typedef void (*cpu_read_attrib_func)(void*,const void*,uint32_t);
typedef void (*rsp_read_attrib_func)(void*,const void*,uint32_t);

typedef struct gl_array_s {
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
} gl_array_t;

#ifdef __cplusplus
extern "C" {
#endif

inline uint32_t array_type_get_stride(gl_array_type_t type)
{
    switch (type)
    {
        case ATTRIB_VERTEX:
            return sizeof(int16_t) * 4;
        case ATTRIB_NORMAL:
            return 0;
        case ATTRIB_COLOR:
            return sizeof(uint32_t);
        case ATTRIB_TEXCOORD:
            return sizeof(int16_t) * 2;
        case ATTRIB_MTX_INDEX:
            return sizeof(uint8_t);
        default:
            return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif
