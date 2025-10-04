#include "mgfx_mesh.h"
#include "mgfx_mesh_internal.h"
#include "asset.h"
#include "assert.h"
#include "string.h"

#define PTR_DECODE(mesh, ptr)    ((void*)(((uint8_t*)(mesh)) + (uint32_t)(ptr)))
#define PTR_ENCODE(mesh, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(mesh)))

mgfx_meshdb_t *mgfx_meshdb_open(const char *fn)
{
    int sz;
    mgfx_meshdb_t *meshdb = asset_load(fn, &sz);
    assertf(memcmp(meshdb->magic, MGFX_MESH_MAGIC, MGFX_MESH_MAGIC_LEN) == 0, "Invalid mgfx mesh file: %s", fn);
    assertf(meshdb->version == MGFX_MESH_VERSION, 
        "Invalid mgfx mesh version in file %s: %d, expected %d\n"
        "Please regenerate the file!",
        fn, meshdb->version, MGFX_MESH_VERSION);

    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        mgfx_mesh_entry_t *entry = &meshdb->meshes[i];
        entry->name = PTR_DECODE(meshdb, entry->name);
        entry->mesh = PTR_DECODE(meshdb, entry->mesh);

        mgfx_mesh_t *mesh = entry->mesh;
        for (size_t j = 0; j < mesh->submesh_count; j++)
        {
            mgfx_submesh_t *submesh = &mesh->submeshes[j];

            submesh->vertex_layout.attributes = PTR_DECODE(meshdb, submesh->vertex_layout.attributes);
            submesh->vertices = PTR_DECODE(meshdb, submesh->vertices);
            submesh->indices = PTR_DECODE(meshdb, submesh->indices);
        }
    }
    
    data_cache_hit_writeback(meshdb, sz);
    return meshdb;
}

void mgfx_meshdb_close(mgfx_meshdb_t *meshdb)
{
    free(meshdb);
}

const mgfx_mesh_t *mgfx_meshdb_lookup(mgfx_meshdb_t *meshdb, const char *name)
{
    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        mgfx_mesh_entry_t *entry = &meshdb->meshes[i];
        if (strcmp(entry->name, name) == 0) {
            return entry->mesh;
        }
    }
    return NULL;
}
