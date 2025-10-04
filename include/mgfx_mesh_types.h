/**
 * @file mgfx_mesh_types.h
 * @brief Type definitions for mgfx
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MGFX_MESH_TYPES_H
#define __LIBDRAGON_MGFX_MESH_TYPES_H

#include <magma_types.h>

typedef struct mgfx_meshdb_s mgfx_meshdb_t;

typedef struct mgfx_submesh_s
{
    mg_vertex_layout_t vertex_layout;
    mg_input_assembly_parms_t input_assembly_parms;
    uint32_t vertices_count;
    uint32_t indices_count;
    void *vertices;
    uint16_t *indices;
} mgfx_submesh_t;

typedef struct mgfx_mesh_s
{
    uint32_t submesh_count;
    mgfx_submesh_t submeshes[];
} mgfx_mesh_t;

#endif
