#ifndef __LIBDRAGON_MGFX_MESH_INTERNAL_H
#define __LIBDRAGON_MGFX_MESH_INTERNAL_H

#include "magma_types.h"

#define MGFX_MESHDB_VERSION       1
#define MGFX_MESHDB_MAGIC         "MGM"
#define MGFX_MESHDB_MAGIC_LEN     3

typedef struct mgfx_mesh_entry_s
{
    char *name;
    mgfx_mesh_t *mesh;
} mgfx_mesh_entry_t;

typedef struct mgfx_meshdb_s 
{
    char magic[MGFX_MESHDB_MAGIC_LEN];
    char version;
    uint32_t mesh_count;
    mgfx_mesh_entry_t meshes[];
} mgfx_meshdb_t;

#endif
