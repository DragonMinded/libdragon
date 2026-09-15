/**
 * @file gl_integration.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Integration hooks for OpenGL context and lifecycle management.
 * @preview
 */
#ifndef __LIBDRAGON_GL_INTEGRATION
#define __LIBDRAGON_GL_INTEGRATION

#include "preview.h"
LIBDRAGON_PREVIEW_HEADER

#ifdef __cplusplus
extern "C" {
#endif

void gl_init(void);
void gl_close(void);

void gl_context_begin(void);
void gl_context_end(void);

#ifdef __cplusplus
}
#endif

#endif