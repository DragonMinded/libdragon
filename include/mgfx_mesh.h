/**
 * @file mgfx_mesh.h
 * @brief Simple mesh format for use with mgfx.
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MESH_H
#define __LIBDRAGON_MGFX_MESH_H

#include <magma.h>

typedef struct mgfx_submesh_s mgfx_submesh_t;

typedef struct mgfx_mesh_s mgfx_mesh_t;

mgfx_mesh_t *mgfx_mesh_load(const char *fn);
mgfx_mesh_t *mgfx_mesh_load_buf(void *buf, int sz);
void mgfx_mesh_free(mgfx_mesh_t *mesh);
uint32_t mgfx_mesh_get_submesh_count(mgfx_mesh_t *mesh);
mgfx_submesh_t *mgfx_mesh_get_submesh(mgfx_mesh_t *mesh, uint32_t index);
void mgfx_mesh_draw(const mgfx_mesh_t *mesh);

const mg_vertex_layout_t *mgfx_submesh_get_vertex_layout(const mgfx_submesh_t *submesh);
void mgfx_submesh_bind(const mgfx_submesh_t *submesh);
void mgfx_submesh_draw(const mgfx_submesh_t *submesh);

#endif
