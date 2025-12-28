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

inline void mgfx_submesh_draw(const mgfx_submesh_t *submesh)
{
    mg_input_assembly_parms_t assembly_parms = {
        .primitive_topology = submesh->primitive_topology,
        .primitive_restart_enabled = submesh->primitive_restart_enabled
    };

    // TODO: Matrix indices?

    if (submesh->indices != NULL) {
        mg_draw_indexed(&assembly_parms, submesh->indices, submesh->indices_count, 0);
    } else  {
        mg_draw(&assembly_parms, submesh->vertices_count, 0);
    }
}

#ifdef __cplusplus
}
#endif

#endif
