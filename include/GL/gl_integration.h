#ifndef __LIBDRAGON_GL_INTEGRATION
#define __LIBDRAGON_GL_INTEGRATION

#include <surface.h>

#ifdef __cplusplus
extern "C" {
#endif

void gl_init();
void gl_close();
void gl_swap_buffers();

#ifdef __cplusplus
}
#endif

#endif