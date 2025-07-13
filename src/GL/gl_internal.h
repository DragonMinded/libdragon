/**
 * @file gl_internal.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_INTERNAL
#define __GL_INTERNAL

#include "GL/gl.h"
#include "GL/gl_integration.h"
#include "magma.h"
#include "rdpq.h"

#define CLAMP01(x) CLAMP((x), 0, 1)

#define CLAMPF_TO_BOOL(x)  ((x)!=0.0)

#define CLAMPF_TO_U8(x)  ((x)*0xFF)
#define CLAMPF_TO_I8(x)  ((x)*0x7F)
#define CLAMPF_TO_U16(x) ((x)*0xFFFF)
#define CLAMPF_TO_I16(x) ((x)*0x7FFF)
#define CLAMPF_TO_U32(x) ((x)*0xFFFFFFFF)
#define CLAMPF_TO_I32(x) ((x)*0x7FFFFFFF)

#define FLOAT_TO_U8(x)  (CLAMP((x), 0.f, 1.f)*0xFF)
#define FLOAT_TO_I8(x)  (CLAMP((x), -1.f, 1.f)*0x7F)
#define FLOAT_TO_I16(x)  (CLAMP((x), -1.f, 1.f)*0x7FFF)

#define U8_TO_FLOAT(x) ((x)/(float)(0xFF))
#define U16_TO_FLOAT(x) ((x)/(float)(0xFFFF))
#define U32_TO_FLOAT(x) ((x)/(float)(0xFFFFFFFF))
#define I8_TO_FLOAT(x) MAX((x)/(float)(0x7F),-1.f)
#define I16_TO_FLOAT(x) MAX((x)/(float)(0x7FFF),-1.f)
#define I32_TO_FLOAT(x) MAX((x)/(float)(0x7FFFFFFF),-1.f)

#define gl_set_error(error, message, ...)  ({ \
    state->current_error = error; \
    assertf(error == GL_NO_ERROR, "%s: " message, #error, ##__VA_ARGS__); \
})

#define gl_ensure_no_begin_end() ({ \
    assertf(state, "gl_init() not called"); \
    if (state->begin_end_active) { \
        gl_set_error(GL_INVALID_OPERATION, "%s is not allowed between glBegin/glEnd", __func__); \
    } \
    true; \
})

typedef struct {
    uint32_t flags;
    GLenum current_error;
    color_t clear_color;
    uint16_t clear_depth;
    bool begin_end_active;
} gl_state_t;

#endif
