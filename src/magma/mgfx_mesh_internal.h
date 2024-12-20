#ifndef __LIBDRAGON_MGFX_MESH_INTERNAL_H
#define __LIBDRAGON_MGFX_MESH_INTERNAL_H

#include "magma_types.h"

#define MGFX_MESH_VERSION       1
#define MGFX_MESH_MAGIC         "MGM"
#define MGFX_MESH_MAGIC_OWNED   "MGO"
#define MGFX_MESH_MAGIC_LOADED  "MGL"
#define MGFX_MESH_MAGIC_LEN     3

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
    char magic[MGFX_MESH_MAGIC_LEN];
    char version;
    uint32_t submesh_count;
    mgfx_submesh_t submeshes[];
} mgfx_mesh_t;

#endif
