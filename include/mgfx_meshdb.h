/**
 * @file mgfx_meshdb.h
 * @brief Simple mesh format for use with mgfx.
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MESHDB_H
#define __LIBDRAGON_MGFX_MESHDB_H

#include "mgfx_mesh.h"

typedef struct mgfx_meshdb_s mgfx_meshdb_t;

#ifdef __cplusplus
extern "C" {
#endif

mgfx_meshdb_t *mgfx_meshdb_open(const char *fn);
void mgfx_meshdb_close(mgfx_meshdb_t *meshdb);

uint32_t mgfx_meshdb_get_mesh_count(const mgfx_meshdb_t *meshdb);
const mgfx_mesh_t *mgfx_meshdb_get_by_index(mgfx_meshdb_t *meshdb, uint32_t index);

const mgfx_mesh_t *mgfx_meshdb_lookup(mgfx_meshdb_t *meshdb, const char *name);

#ifdef __cplusplus
}
#endif

#endif
