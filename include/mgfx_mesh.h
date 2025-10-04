/**
 * @file mgfx_mesh.h
 * @brief Simple mesh format for use with mgfx.
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MESH_H
#define __LIBDRAGON_MGFX_MESH_H

#include <magma.h>
#include <mgfx_mesh_types.h>

#ifdef __cplusplus
extern "C" {
#endif

mgfx_meshdb_t *mgfx_meshdb_open(const char *fn);
void mgfx_meshdb_close(mgfx_meshdb_t *meshdb);
const mgfx_mesh_t *mgfx_meshdb_lookup(mgfx_meshdb_t *meshdb, const char *name);

inline void mgfx_submesh_draw(const mgfx_submesh_t *submesh)
{
    if (submesh->indices != NULL) {
        mg_draw_indexed(&submesh->input_assembly_parms, submesh->indices, submesh->indices_count, 0);
    } else  {
        mg_draw(&submesh->input_assembly_parms, submesh->vertices_count, 0);
    }
}

#ifdef __cplusplus
}
#endif

#endif
