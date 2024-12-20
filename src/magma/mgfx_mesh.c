#include "mgfx_mesh.h"
#include "mgfx_mesh_internal.h"
#include "asset.h"
#include "assert.h"
#include "string.h"

#define PTR_DECODE(mesh, ptr)    ((void*)(((uint8_t*)(mesh)) + (uint32_t)(ptr)))
#define PTR_ENCODE(mesh, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(mesh)))

static mgfx_mesh_t *mgfx_mesh_load_internal(void *buf, int sz, const char *fn)
{
    mgfx_mesh_t *mesh = buf;
    if (memcmp(mesh->magic, MGFX_MESH_MAGIC, MGFX_MESH_MAGIC_LEN) != 0) {
        assertf("Invalid mgfx mesh file: %s", fn);
    }
    assertf(mesh->version == MGFX_MESH_VERSION, 
        "Invalid mgfx mesh version in file %s: %d, expected %d\n"
        "Please regenerate the file!",
        fn, mesh->version, MGFX_MESH_VERSION);
    
    for (int i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];

        submesh->vertex_layout.attributes = PTR_DECODE(mesh, submesh->vertex_layout.attributes);
        submesh->vertices = PTR_DECODE(mesh, submesh->vertices);
        submesh->indices = PTR_DECODE(mesh, submesh->indices);
    }

    data_cache_hit_writeback(mesh, sz);
    return mesh;
}

static void mgfx_mesh_unload(mgfx_mesh_t *mesh)
{
    for (int i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];

        submesh->vertex_layout.attributes = PTR_ENCODE(mesh, submesh->vertex_layout.attributes);
        submesh->vertices = PTR_ENCODE(mesh, submesh->vertices);
        submesh->indices = PTR_ENCODE(mesh, submesh->indices);
    }

    memcpy(mesh->magic, MGFX_MESH_MAGIC, MGFX_MESH_MAGIC_LEN);
}

mgfx_mesh_t *mgfx_mesh_load(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    mgfx_mesh_t *mesh = mgfx_mesh_load_internal(buf, sz, fn);
    memcpy(mesh->magic, MGFX_MESH_MAGIC_OWNED, MGFX_MESH_MAGIC_LEN);
    return mesh;
}

mgfx_mesh_t *mgfx_mesh_load_buf(void *buf, int sz)
{
    mgfx_mesh_t *mesh = mgfx_mesh_load_internal(buf, sz, "<in-memory buffer>");
    memcpy(mesh->magic, MGFX_MESH_MAGIC_LOADED, MGFX_MESH_MAGIC_LEN);
    return mesh;
}

void mgfx_mesh_free(mgfx_mesh_t *mesh)
{
    if (memcmp(mesh->magic, MGFX_MESH_MAGIC_OWNED, MGFX_MESH_MAGIC_LEN) == 0) {
        free(mesh);
    } else {
        mgfx_mesh_unload(mesh);
    }
}

mgfx_submesh_t *mgfx_mesh_get_submesh(mgfx_mesh_t *mesh, uint32_t index)
{
    return &mesh->submeshes[index];
}

void mgfx_mesh_draw(const mgfx_mesh_t *mesh)
{
    for (int i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_bind(&mesh->submeshes[i]);
        mgfx_submesh_draw(&mesh->submeshes[i]);
    }
}

const mg_vertex_layout_t *mgfx_submesh_get_vertex_layout(const mgfx_submesh_t *submesh)
{
    return &submesh->vertex_layout;
}

void mgfx_submesh_bind(const mgfx_submesh_t *submesh)
{
    mg_bind_vertex_buffer(submesh->vertices);
}

void mgfx_submesh_draw(const mgfx_submesh_t *submesh)
{
    if (submesh->indices != NULL) {
        mg_draw_indexed(&submesh->input_assembly_parms, submesh->indices, submesh->indices_count, 0);
    } else  {
        mg_draw(&submesh->input_assembly_parms, submesh->vertices_count, 0);
    }
}
