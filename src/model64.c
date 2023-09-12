#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "n64sys.h"
#include "GL/gl.h"
#include "model64.h"
#include "model64_internal.h"
#include "asset.h"
#include "debug.h"

#define PTR_DECODE(model, ptr)    ((void*)(((uint8_t*)(model)) + (uint32_t)(ptr)))
#define PTR_ENCODE(model, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(model)))

static model64_data_t *load_model_data_buf(void *buf, int sz)
{
    model64_data_t *model = buf;
    assertf(sz >= sizeof(model64_data_t), "Model buffer too small (sz=%d)", sz);
    if(model->magic == MODEL64_MAGIC_LOADED) {
        assertf(0, "Trying to load already loaded model data (buf=%p, sz=%08x)", buf, sz);
    }
    assertf(model->magic == MODEL64_MAGIC, "invalid model data (magic: %08lx)", model->magic);
    model->nodes = PTR_DECODE(model, model->nodes);
    model->meshes = PTR_DECODE(model, model->meshes);
    if(model->skins)
    {
        model->skins = PTR_DECODE(model, model->skins);
        for(uint32_t i=0; i<model->num_skins; i++)
        {
            model->skins[i].joints = PTR_DECODE(model->skins, model->skins[i].joints);
        }
    }
    for(uint32_t i=0; i<model->num_nodes; i++)
    {
        if(model->nodes[i].name)
        {
            model->nodes[i].name = PTR_DECODE(model->nodes, model->nodes[i].name);
        }
        if((uint32_t)model->nodes[i].mesh != model->num_meshes)
        {
            model->nodes[i].mesh = &model->meshes[(uint32_t)model->nodes[i].mesh];
        }
        else
        {
            model->nodes[i].mesh = NULL;
        }
        model->nodes[i].children = PTR_DECODE(model->nodes, model->nodes[i].children);
        if((uint32_t)model->nodes[i].skin != model->num_skins)
        {
            model->nodes[i].skin = &model->skins[(uint32_t)model->nodes[i].skin];
        }
        else
        {
            model->nodes[i].skin = NULL;
        }
    }
    for (uint32_t i = 0; i < model->num_meshes; i++)
    {
        model->meshes[i].primitives = PTR_DECODE(model->meshes, model->meshes[i].primitives);
        for (uint32_t j = 0; j < model->meshes[i].num_primitives; j++)
        {
            primitive_t *primitive = &model->meshes[i].primitives[j];
            primitive->position.pointer = PTR_DECODE(model->meshes, primitive->position.pointer);
            primitive->color.pointer = PTR_DECODE(model->meshes, primitive->color.pointer);
            primitive->texcoord.pointer = PTR_DECODE(model->meshes, primitive->texcoord.pointer);
            primitive->normal.pointer = PTR_DECODE(model->meshes, primitive->normal.pointer);
            primitive->mtx_index.pointer = PTR_DECODE(model->meshes, primitive->mtx_index.pointer);
            primitive->indices = PTR_DECODE(model->meshes, primitive->indices);
        }
    }
    model->magic = MODEL64_MAGIC_LOADED;
    model->ref_count = 1;
    data_cache_hit_writeback(model, sz);
    return model;
}

static size_t get_model_instance_size(model64_data_t *model_data)
{
    return sizeof(model64_t)+(model_data->num_nodes*sizeof(node_transform_state_t));
}

static void multiply_node_mtx(float parent[16], float child[16])
{
    
    for (int i = 0; i < 4; ++i)
    {
        float l0 = child[i * 4 + 0];
        float l1 = child[i * 4 + 1];
        float l2 = child[i * 4 + 2];

        float r0 = l0 * parent[0] + l1 * parent[4] + l2 * parent[8];
        float r1 = l0 * parent[1] + l1 * parent[5] + l2 * parent[9];
        float r2 = l0 * parent[2] + l1 * parent[6] + l2 * parent[10];

        child[i * 4 + 0] = r0;
        child[i * 4 + 1] = r1;
        child[i * 4 + 2] = r2;
    }

    child[12] += parent[12];
    child[13] += parent[13];
    child[14] += parent[14];
}

static void mtx_copy(float dst[16], float src[16])
{
	memcpy(dst, src, 16*sizeof(float));
}

static void transform_calc_matrix(node_transform_t *transform)
{
    float tx = transform->pos[0];
    float ty = transform->pos[1];
    float tz = transform->pos[2];

    float qx = transform->rot[0];
    float qy = transform->rot[1];
    float qz = transform->rot[2];
    float qw = transform->rot[3];

    float sx = transform->scale[0];
    float sy = transform->scale[1];
    float sz = transform->scale[2];
    
    transform->mtx[0] = (1 - 2 * qy*qy - 2 * qz*qz) * sx;
    transform->mtx[1] = (2 * qx*qy + 2 * qz*qw) * sx;
    transform->mtx[2] = (2 * qx*qz - 2 * qy*qw) * sx;
    transform->mtx[3] = 0.f;

    transform->mtx[4] = (2 * qx*qy - 2 * qz*qw) * sy;
    transform->mtx[5] = (1 - 2 * qx*qx - 2 * qz*qz) * sy;
    transform->mtx[6] = (2 * qy*qz + 2 * qx*qw) * sy;
    transform->mtx[7] = 0.f;

    transform->mtx[8] = (2 * qx*qz + 2 * qy*qw) * sz;
    transform->mtx[9] = (2 * qy*qz - 2 * qx*qw) * sz;
    transform->mtx[10] = (1 - 2 * qx*qx - 2 * qy*qy) * sz;
    transform->mtx[11] = 0.f;

    transform->mtx[12] = tx;
    transform->mtx[13] = ty;
    transform->mtx[14] = tz;
    transform->mtx[15] = 1.f;
}

static void calc_node_local_matrix(model64_t *model, uint32_t node)
{
    transform_calc_matrix(&model->transforms[node].transform);
}

static void calc_node_world_matrix(model64_t *model, uint32_t node)
{
    model64_node_t *node_ptr = model64_get_node(model, node);
    node_transform_state_t *xform = &model->transforms[node];
    mtx_copy(xform->world_mtx, xform->transform.mtx);
    uint32_t parent_node = node_ptr->parent;
    
    while(parent_node != model->data->num_nodes) {
        model64_node_t *parent_ptr = model64_get_node(model, parent_node);
        node_transform_state_t *parent_xform = &model->transforms[parent_node];
        multiply_node_mtx(parent_xform->transform.mtx, xform->world_mtx);
        parent_node = parent_ptr->parent;
    }
}
static void update_node_matrices(model64_t *model)
{
    for(uint32_t i=0; i<model->data->num_nodes; i++) {
        calc_node_world_matrix(model, i);
    }
}

static void init_model_transforms(model64_t *model)
{
    for(uint32_t i=0; i<model->data->num_nodes; i++) {
        model->transforms[i].transform = model->data->nodes[i].transform;
        calc_node_local_matrix(model, i);
    }
    update_node_matrices(model);
}

static model64_t *make_model_instance(model64_data_t *model_data)
{
    model64_t *instance = calloc(1, get_model_instance_size(model_data));
    instance->data = model_data;
    instance->transforms = (node_transform_state_t *)&instance[1];
    init_model_transforms(instance);
    return instance;
}

model64_t *model64_load_buf(void *buf, int sz)
{
    model64_data_t *data = load_model_data_buf(buf, sz);
    return make_model_instance(data);
}

model64_t *model64_load(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    model64_t *model = model64_load_buf(buf, sz);
    model->data->magic = MODEL64_MAGIC_OWNED;
    return model;
}

model64_t *model64_clone(model64_t *model)
{
    model->data->ref_count++;
    return make_model_instance(model->data);
}

static void unload_model_data(model64_data_t *model)
{
    for(uint32_t i=0; i<model->num_nodes; i++)
    {
        model->nodes[i].children = PTR_ENCODE(model->nodes, model->nodes[i].children);
        if(model->nodes[i].skin)
        {
            model->nodes[i].skin = (model64_skin_t *)(model->nodes[i].skin-model->skins);
        }
        else
        {
            model->nodes[i].skin = (model64_skin_t *)model->num_skins;
        }
        if(model->nodes[i].mesh)
        {
            model->nodes[i].mesh = (mesh_t *)(model->nodes[i].mesh-model->meshes);
        }
        else
        {
            model->nodes[i].mesh = (mesh_t *)(model->num_meshes);
        }
        if(model->nodes[i].name)
        {
            model->nodes[i].name = PTR_ENCODE(model->nodes, model->nodes[i].name);
        }
    }
    for (uint32_t i = 0; i < model->num_meshes; i++)
    {
        for (uint32_t j = 0; j < model->meshes[i].num_primitives; j++)
        {
            primitive_t *primitive = &model->meshes[i].primitives[j];
            primitive->position.pointer = PTR_ENCODE(model->meshes, primitive->position.pointer);
            primitive->color.pointer = PTR_ENCODE(model->meshes, primitive->color.pointer);
            primitive->texcoord.pointer = PTR_ENCODE(model->meshes, primitive->texcoord.pointer);
            primitive->normal.pointer = PTR_ENCODE(model->meshes, primitive->normal.pointer);
            primitive->mtx_index.pointer = PTR_ENCODE(model->meshes, primitive->mtx_index.pointer);
            primitive->indices = PTR_ENCODE(model->meshes, primitive->indices);
        }
        model->meshes[i].primitives = PTR_ENCODE(model->meshes, model->meshes[i].primitives);
    }
    model->nodes = PTR_ENCODE(model, model->nodes);
    model->meshes = PTR_ENCODE(model, model->meshes);
    if(model->skins)
    {
        for(uint32_t i=0; i<model->num_skins; i++)
        {
            model->skins[i].joints = PTR_ENCODE(model->skins, model->skins[i].joints);
        }
        model->skins = PTR_ENCODE(model, model->skins);
    }
    if(model->magic == MODEL64_MAGIC_OWNED) {
        #ifndef NDEBUG
        // To help debugging, zero the model data structure
        memset(model, 0, sizeof(model64_data_t));
        #endif

        free(model);
    }
}

void model64_free(model64_t *model)
{
    if(--model->data->ref_count == 0)
    {
        unload_model_data(model->data);
    }
    free(model);
}

uint32_t model64_get_mesh_count(model64_t *model)
{
    return model->data->num_meshes;
}

mesh_t *model64_get_mesh(model64_t *model, uint32_t mesh_index)
{
    return &model->data->meshes[mesh_index];
}

uint32_t model64_get_node_count(model64_t *model)
{
    return model->data->num_nodes;
}

model64_node_t *model64_get_node(model64_t *model, uint32_t node_index)
{
    return &model->data->nodes[node_index];
}

model64_node_t *model64_search_node(model64_t *model, const char *name)
{
    for(uint32_t i=0; i<model->data->num_nodes; i++)
    {
        char *node_name = model->data->nodes[i].name;
        if(node_name && !strcmp(node_name, name))
        {
            return &model->data->nodes[i];
        }
    }
    return NULL;
}

static uint32_t get_node_idx(model64_t *model, model64_node_t *node)
{
    return node - model->data->nodes;
}

void model64_set_node_pos(model64_t *model, model64_node_t *node, float x, float y, float z)
{
    uint32_t node_idx = get_node_idx(model, node);
    assertf(node_idx < model->data->num_nodes, "Setting position of invalid node.");
    float *dst = model->transforms[node_idx].transform.pos;
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
    calc_node_local_matrix(model, node_idx);
    update_node_matrices(model);
}

void model64_set_node_rot(model64_t *model, model64_node_t *node, float x, float y, float z)
{
    float cr = cosf(x * 0.5);
    float sr = sinf(x * 0.5);
    float cp = cosf(y * 0.5);
    float sp = sinf(y * 0.5);
    float cy = cosf(z * 0.5);
    float sy = sinf(z * 0.5);
    float w = cr * cp * cy + sr * sp * sy;
    x = sr * cp * cy - cr * sp * sy;
    y = cr * sp * cy + sr * cp * sy;
    z = cr * cp * sy - sr * sp * cy;
    model64_set_node_rot_quat(model, node, x, y, z, w);
}

void model64_set_node_rot_quat(model64_t *model, model64_node_t *node, float x, float y, float z, float w)
{
    uint32_t node_idx = get_node_idx(model, node);
    assertf(node_idx < model->data->num_nodes, "Setting rotation of invalid node.");
    float *dst = model->transforms[node_idx].transform.rot;
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
    dst[3] = w;
    calc_node_local_matrix(model, node_idx);
    update_node_matrices(model);
}

void model64_set_node_scale(model64_t *model, model64_node_t *node, float x, float y, float z)
{
    uint32_t node_idx = get_node_idx(model, node);
    assertf(node_idx < model->data->num_nodes, "Setting scale of invalid node.");
    float *dst = model->transforms[node_idx].transform.scale;
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
    calc_node_local_matrix(model, node_idx);
    update_node_matrices(model);
}

void model64_get_node_world_mtx(model64_t *model, model64_node_t *node, float dst[16])
{
    uint32_t node_idx = get_node_idx(model, node);
    assertf(node_idx < model->data->num_nodes, "Grabbing world matrix of invalid node.");
    mtx_copy(dst, model->transforms[node_idx].world_mtx);
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

void model64_draw_node(model64_t *model, model64_node_t *node)
{
    uint32_t node_idx = get_node_idx(model, node);
    assertf(node_idx < model->data->num_nodes, "Drawing invalid node.");
    if(node->mesh)
    {
        if(node->skin)
        {
            glMatrixMode(GL_MATRIX_PALETTE_ARB);
            for(uint32_t i=0; i<node->skin->num_joints; i++)
            {
                glCurrentPaletteMatrixARB(i);
                glCopyMatrixN64(GL_MODELVIEW); //Copy matrix at top of modelview stack to matrix palette
                glMultMatrixf(model->transforms[node->skin->joints[i].node_idx].world_mtx);
                glMultMatrixf(node->skin->joints[i].inverse_bind_mtx);
            }
            glEnable(GL_MATRIX_PALETTE_ARB);
            model64_draw_mesh(node->mesh);
            glDisable(GL_MATRIX_PALETTE_ARB);
            glMatrixMode(GL_MODELVIEW);
        }
        else
        {
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glMultMatrixf(model->transforms[node_idx].world_mtx);
            model64_draw_mesh(node->mesh);
            glPopMatrix();
        }
    }
}

void model64_draw(model64_t *model)
{
    for (uint32_t i = 0; i < model64_get_node_count(model); i++)
    {
        model64_draw_node(model, model64_get_node(model, i));
    }
}
