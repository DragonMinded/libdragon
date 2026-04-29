#ifndef __PIPELINES_H
#define __PIPELINES_H

#include "vertex_layout.h"
#include "magma.h"

#ifdef __cplusplus
extern "C" {
#endif

void update_pipelines_from_layout(vertex_layout *vertex_layout);
const mg_uniform_t *get_matrices_uniform();

#ifdef __cplusplus
}
#endif

#endif
