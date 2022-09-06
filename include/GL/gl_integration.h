#ifndef __LIBDRAGON_GL_INTEGRATION
#define __LIBDRAGON_GL_INTEGRATION

#include <surface.h>

typedef surface_t*(*gl_open_surf_func_t)(void);
typedef void(*gl_close_surf_func_t)(surface_t*);

#ifdef __cplusplus
extern "C" {
#endif

void gl_init_with_callbacks(gl_open_surf_func_t open_surface, gl_close_surf_func_t close_surface);
void gl_init();
void gl_close();
void gl_swap_buffers();

#ifdef __cplusplus
}
#endif

#endif