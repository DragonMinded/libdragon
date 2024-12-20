/**
 * @file mgfx_meshdb.h
 * @brief Mesh database file format.
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MESHDB_H
#define __LIBDRAGON_MGFX_MESHDB_H

#include <mgfx_mesh.h>

typedef struct mgfx_meshdb_s mgfx_meshdb_t;

mgfx_meshdb_t *mgfx_meshdb_load(const char *fn);
mgfx_meshdb_t *mgfx_meshdb_load_buf(void *buf, int sz);
void mgfx_meshdb_free(mgfx_meshdb_t *mesh);
mgfx_mesh_t *mgfx_meshdb_get(const char *name);

#endif
