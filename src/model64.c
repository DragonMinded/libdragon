#include <stdlib.h>
#include <string.h>
#include "n64sys.h"
#include "GL/gl.h"
#include "model64.h"
#include "model64_internal.h"
#include "asset.h"
#include "debug.h"

#define PTR_DECODE(model, ptr)    ((void*)(((uint8_t*)(model)) + (uint32_t)(ptr)))
#define PTR_ENCODE(model, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(model)))

model64_t *model64_load_buf(void *buf, int sz)
{
    model64_t *model = buf;
    assertf(sz >= sizeof(model64_t), "Model buffer too small (sz=%d)", sz);
    if(model->magic == MODEL64_MAGIC_LOADED) {
        assertf(0, "Trying to load already loaded model data (buf=%p, sz=%08x)", buf, sz);
    }
    assertf(model->magic == MODEL64_MAGIC, "invalid model data (magic: %08lx)", model->magic);
    model->meshes = PTR_DECODE(model, model->meshes);
    for (int i = 0; i < model->num_meshes; i++)
    {
        model->meshes[i].primitives = PTR_DECODE(model, model->meshes[i].primitives);
        for (int j = 0; j < model->meshes[i].num_primitives; j++)
        {
            primitive_t *primitive = &model->meshes[i].primitives[j];
            primitive->position.pointer = PTR_DECODE(model, primitive->position.pointer);
            primitive->color.pointer = PTR_DECODE(model, primitive->color.pointer);
            primitive->texcoord.pointer = PTR_DECODE(model, primitive->texcoord.pointer);
            primitive->normal.pointer = PTR_DECODE(model, primitive->normal.pointer);
            primitive->mtx_index.pointer = PTR_DECODE(model, primitive->mtx_index.pointer);
            primitive->indices = PTR_DECODE(model, primitive->indices);
        }
    }
    
    data_cache_hit_writeback(model, sz);
    return model;
}

model64_t *model64_load(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    model64_t *model = model64_load_buf(buf, sz);
    model->magic = MODEL64_MAGIC_OWNED;
    return model;
}

static void model64_unload(model64_t *model)
{
    for (int i = 0; i < model->num_meshes; i++)
    {
        for (int j = 0; j < model->meshes[i].num_primitives; j++)
        {
            primitive_t *primitive = &model->meshes[i].primitives[j];
            primitive->position.pointer = PTR_ENCODE(model, primitive->position.pointer);
            primitive->color.pointer = PTR_ENCODE(model, primitive->color.pointer);
            primitive->texcoord.pointer = PTR_ENCODE(model, primitive->texcoord.pointer);
            primitive->normal.pointer = PTR_ENCODE(model, primitive->normal.pointer);
            primitive->mtx_index.pointer = PTR_ENCODE(model, primitive->mtx_index.pointer);
            primitive->indices = PTR_ENCODE(model, primitive->indices);
        }
        model->meshes[i].primitives = PTR_ENCODE(model, model->meshes[i].primitives);
    }
    model->meshes = PTR_ENCODE(model, model->meshes);
}

void model64_free(model64_t *model)
{
    model64_unload(model);
    if (model->magic == MODEL64_MAGIC_OWNED) {
        #ifndef NDEBUG
        // To help debugging, zero the model structure
        memset(model, 0, sizeof(model64_t));
        #endif

        free(model);
    }
}

uint32_t model64_get_mesh_count(model64_t *model)
{
    return model->num_meshes;
}
mesh_t *model64_get_mesh(model64_t *model, uint32_t mesh_index)
{
    return &model->meshes[mesh_index];
}

uint32_t model64_get_primitive_count(mesh_t *mesh)
{
    return mesh->num_primitives;
}
primitive_t *model64_get_primitive(mesh_t *mesh, uint32_t primitive_index)
{
    return &mesh->primitives[primitive_index];
}

void model64_draw_primitive(primitive_t *primitive)
{
    if (primitive->position.size > 0) {
        glEnableClientState(GL_VERTEX_ARRAY);
        if (primitive->position.type == GL_HALF_FIXED_N64) {
            glVertexHalfFixedPrecisionN64(primitive->vertex_precision);
        }
        glVertexPointer(primitive->position.size, primitive->position.type, primitive->position.stride, primitive->position.pointer);
    } else {
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    
    if (primitive->color.size > 0) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(primitive->color.size, primitive->color.type, primitive->color.stride, primitive->color.pointer);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    
    if (primitive->texcoord.size > 0) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        if (primitive->texcoord.type == GL_HALF_FIXED_N64) {
            glTexCoordHalfFixedPrecisionN64(primitive->texcoord_precision);
        }
        glTexCoordPointer(primitive->texcoord.size, primitive->texcoord.type, primitive->texcoord.stride, primitive->texcoord.pointer);
    } else {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    
    if (primitive->normal.size > 0) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(primitive->normal.type, primitive->normal.stride, primitive->normal.pointer);
    } else {
        glDisableClientState(GL_NORMAL_ARRAY);
    }
    
    if (primitive->mtx_index.size > 0) {
        glEnableClientState(GL_MATRIX_INDEX_ARRAY_ARB);
        glMatrixIndexPointerARB(primitive->mtx_index.size, primitive->mtx_index.type, primitive->mtx_index.stride, primitive->mtx_index.pointer);
    } else {
        glDisableClientState(GL_MATRIX_INDEX_ARRAY_ARB);
    }

    if (primitive->num_indices > 0) {
        glDrawElements(primitive->mode, primitive->num_indices, primitive->index_type, primitive->indices);
    } else {
        glDrawArrays(primitive->mode, 0, primitive->num_vertices);
    }
}

void model64_draw_mesh(mesh_t *mesh)
{
    for (uint32_t i = 0; i < model64_get_primitive_count(mesh); i++)
    {
        model64_draw_primitive(model64_get_primitive(mesh, i));
    }
}

void model64_draw(model64_t *model)
{
    for (uint32_t i = 0; i < model64_get_mesh_count(model); i++) {
        model64_draw_mesh(model64_get_mesh(model, i));
    }
}
