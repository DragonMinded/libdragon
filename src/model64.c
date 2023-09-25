#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fmath.h"
#include "n64sys.h"
#include "GL/gl.h"
#include "dragonfs.h"
#include "dma.h"
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
    model->skins = PTR_DECODE(model, model->skins);
    model->anims = PTR_DECODE(model, model->anims);
    for(uint32_t i=0; i<model->num_skins; i++)
    {
        model->skins[i].joints = PTR_DECODE(model, model->skins[i].joints);
    }
    for(uint32_t i=0; i<model->num_nodes; i++)
    {
        if(model->nodes[i].name)
        {
            model->nodes[i].name = PTR_DECODE(model, model->nodes[i].name);
        }
        if(model->nodes[i].mesh)
        {
            model->nodes[i].mesh = PTR_DECODE(model, model->nodes[i].mesh);
        }
        model->nodes[i].children = PTR_DECODE(model, model->nodes[i].children);
        if(model->nodes[i].skin)
        {
            model->nodes[i].skin = PTR_DECODE(model, model->nodes[i].skin);
        }
    }
    for (uint32_t i = 0; i < model->num_meshes; i++)
    {
        model->meshes[i].primitives = PTR_DECODE(model, model->meshes[i].primitives);
        for (uint32_t j = 0; j < model->meshes[i].num_primitives; j++)
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
    for (uint32_t i = 0; i < model->num_anims; i++)
    {
        if(model->anims[i].name)
        {
            model->anims[i].name = PTR_DECODE(model, model->anims[i].name);
        }
        model->anims[i].channels = PTR_DECODE(model, model->anims[i].channels);
        if(model->stream_buf_size == 0)
        {
            model->anims[i].data = PTR_DECODE(model, model->anims[i].data);
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
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        instance->active_anims[i].index = -1;
        instance->active_anims[i].paused = true;
        instance->active_anims[i].speed = 1.0f;
    }
    return instance;
}

model64_data_t *model64_load_data_buf(void *buf, int sz)
{
    model64_data_t *data = load_model_data_buf(buf, sz);
    assertf(data->stream_buf_size == 0, "Streaming animations not supported when loading model from buffer");
    return data;
}

model64_data_t *model64_load_data(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    model64_data_t *data = load_model_data_buf(buf, sz);
    if(data->stream_buf_size != 0) {
        assertf(strncmp(fn, "rom:/", 5) == 0, "Cannot open %s: models with streamed animations must be stored in ROM (rom:/)", fn);
        char anim_name[strlen(fn)+6];
        sprintf(anim_name, "%s.anim", fn);
        data->anim_data_handle = (void *)dfs_rom_addr(anim_name+5);
    }
    return data;
}

model64_t *model64_load_buf(void *buf, int sz)
{
    model64_data_t *data = model64_load_data_buf(buf, sz);
    return model64_create(data);
}

model64_t *model64_load(const char *fn)
{
    model64_data_t *data = model64_load_data(fn);
    return model64_create(data);
}

model64_t *model64_create(model64_data_t *data)
{
    data->ref_count++;
    return make_model_instance(data);
}

model64_t *model64_clone(model64_t *model)
{
    return model64_create(model->data);
}

static void unload_model_data(model64_data_t *model)
{
    for(uint32_t i=0; i<model->num_nodes; i++)
    {
        model->nodes[i].children = PTR_ENCODE(model, model->nodes[i].children);
        if(model->nodes[i].skin)
        {
            model->nodes[i].skin = PTR_ENCODE(model, model->nodes[i].skin);
        }
        if(model->nodes[i].mesh)
        {
            model->nodes[i].mesh = PTR_ENCODE(model, model->nodes[i].mesh);
        }
        if(model->nodes[i].name)
        {
            model->nodes[i].name = PTR_ENCODE(model, model->nodes[i].name);
        }
    }
    for (uint32_t i = 0; i < model->num_meshes; i++)
    {
        for (uint32_t j = 0; j < model->meshes[i].num_primitives; j++)
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
    for(uint32_t i=0; i<model->num_skins; i++)
    {
        model->skins[i].joints = PTR_ENCODE(model, model->skins[i].joints);
    }
    for (uint32_t i = 0; i < model->num_anims; i++)
    {
        if(model->anims[i].name)
        {
            model->anims[i].name = PTR_ENCODE(model, model->anims[i].name);
        }
        model->anims[i].channels = PTR_ENCODE(model, model->anims[i].channels);
        if(model->stream_buf_size == 0)
        {
            model->anims[i].data = PTR_ENCODE(model, model->anims[i].data);
        }
    }
    model->nodes = PTR_ENCODE(model, model->nodes);
    model->meshes = PTR_ENCODE(model, model->meshes);
    model->skins = PTR_ENCODE(model, model->skins);
    model->anims = PTR_ENCODE(model, model->anims);
    if(model->magic == MODEL64_MAGIC_OWNED) {
        #ifndef NDEBUG
        // To help debugging, zero the model data structure
        memset(model, 0, sizeof(model64_data_t));
        #endif

        free(model);
    }
}

void model64_free_data(model64_data_t *data)
{
    if(--data->ref_count == 0)
    {
        unload_model_data(data);
    }
}

void model64_free(model64_t *model)
{
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        if(model->active_anims[i].stream_buf[0]) {
            free_uncached(model->active_anims[i].stream_buf[0]);
            free_uncached(model->active_anims[i].stream_buf[1]);
        }
    }
    model64_free_data(model->data);
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
    float cr, sr, cp, sp, cy, sy;
    fm_sincosf(x*0.5f, &sr, &cr);
    fm_sincosf(y*0.5f, &sp, &cp);
    fm_sincosf(z*0.5f, &sy, &cy);
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

static int32_t search_anim_index(model64_t *model, const char *name)
{
    if(!name) {
        return -1;
    }
    for(uint32_t i=0; i<model->data->num_anims; i++) {
        if(model->data->anims[i].name && !strcmp(name, model->data->anims[i].name)) {
            return i;
        }
    }
    return -1;
}

static int search_free_anim(model64_t *model)
{
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        if(model->active_anims[i].index == -1) {
            return i;
        }
    }
    return -1;
}

static bool is_animid_valid(model64_t *model, int id)
{
    return (id >= 0) && (id < MAX_ACTIVE_ANIMS) && model->active_anims[id].index != -1;
}

int model64_anim_play(model64_t *model, const char *anim, bool paused, float start_time)
{
    int slot = search_free_anim(model);
    if(slot == -1) {
        return -1;
    }
    int32_t anim_index = search_anim_index(model, anim);
    assertf(anim_index != -1, "Invalid animation name");
    if(model->data->stream_buf_size != 0 && !model->active_anims[slot].stream_buf[0]) {
        model->active_anims[slot].stream_buf[0] = malloc_uncached(model->data->stream_buf_size);
        model->active_anims[slot].stream_buf[1] = malloc_uncached(model->data->stream_buf_size);
    }
    model->active_anims[slot].index = anim_index;
    model->active_anims[slot].paused = paused;
    model->active_anims[slot].time = start_time;
    model->active_anims[slot].loop = false;
    model->active_anims[slot].speed = 1.0f;
    return slot;
}

void model64_anim_stop(model64_t *model, int anim_id)
{
    assertf(is_animid_valid(model, anim_id), "Invalid animation ID");
    model->active_anims[anim_id].index = -1;
}

float model64_anim_get_length(model64_t *model, const char *anim)
{
    int32_t anim_index = search_anim_index(model, anim);
    assertf(anim_index != -1, "Invalid animation name");
    model64_anim_t *curr_anim = &model->data->anims[anim_index];
    return curr_anim->num_frames/curr_anim->frame_rate;
}

float model64_anim_get_time(model64_t *model, int anim_id)
{
    assertf(is_animid_valid(model, anim_id), "Invalid animation ID");
    return model->active_anims[anim_id].time;
}

float model64_anim_set_time(model64_t *model, int anim_id, float time)
{
    assertf(is_animid_valid(model, anim_id), "Invalid animation ID");
    float old_time = model->active_anims[anim_id].time;
    model->active_anims[anim_id].time = time;
    return old_time;
}

float model64_anim_set_speed(model64_t *model, int anim_id, float speed)
{
    assertf(is_animid_valid(model, anim_id), "Invalid animation ID");
    float old_speed = model->active_anims[anim_id].speed; 
    model->active_anims[anim_id].speed = speed;
    return old_speed;
}

bool model64_anim_set_loop(model64_t *model, int anim_id, bool loop)
{
    assertf(is_animid_valid(model, anim_id), "Invalid animation ID");
    bool old_loop = model->active_anims[anim_id].loop; 
    model->active_anims[anim_id].loop = loop;
    return old_loop;
}

bool model64_anim_set_pause(model64_t *model, int anim_id, bool paused)
{
    assertf(is_animid_valid(model, anim_id), "Invalid animation ID");
    bool old_pause = model->active_anims[anim_id].paused;
    model->active_anims[anim_id].paused = paused;
    return old_pause;
}

static void read_model_anim_buf(model64_t *model, anim_buf_info_t *buf_info, int anim_slot)
{
    anim_state_t *anim_slot_ptr = &model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_slot_ptr->index];
    float anim_duration = curr_anim->num_frames/curr_anim->frame_rate;
    uint32_t curr_data_idx;
    uint32_t next_data_idx;
    uint32_t frame_size = curr_anim->frame_size;
    
    if(anim_slot_ptr->time < 0) {
        curr_data_idx = next_data_idx = 0;
        buf_info->time = 0;
    } else if(anim_slot_ptr->time > anim_duration-(1/curr_anim->frame_rate)) {
        curr_data_idx = next_data_idx = curr_anim->num_frames-1;
        buf_info->time = 0;
    } else {
        curr_data_idx = anim_slot_ptr->time*curr_anim->frame_rate;
        next_data_idx = curr_data_idx+1;
        buf_info->time = (anim_slot_ptr->time-(curr_data_idx/curr_anim->frame_rate))*curr_anim->frame_rate;
    }
    if(model->data->stream_buf_size > 0) {
        uint32_t rom_addr = (uint32_t)model->data->anim_data_handle;
        rom_addr += (uint32_t)curr_anim->data;
        dma_read(anim_slot_ptr->stream_buf[0], rom_addr+(curr_data_idx*frame_size), frame_size);
        buf_info->curr_buf = anim_slot_ptr->stream_buf[0];
        dma_read(anim_slot_ptr->stream_buf[1], rom_addr+(next_data_idx*frame_size), frame_size);
        buf_info->next_buf = anim_slot_ptr->stream_buf[1];
    } else {
        uint8_t *buf_base = curr_anim->data;
        buf_info->curr_buf = &buf_base[curr_data_idx*frame_size];
        buf_info->next_buf = &buf_base[next_data_idx*frame_size];
    }
}

static void vec_lerp(float *out, float *in1, float *in2, size_t count, float time)
{
    for(size_t i=0; i<count; i++) {
        out[i] = ((1-time)*in1[i])+(time*in2[i]);
    }
}

static void vec_normalize(float *vec, size_t count)
{
    float mag2 = 0.0f;
    for(size_t i=0; i<count; i++) {
        mag2 += vec[i]*vec[i];
    }
    float scale = 1.0f/sqrtf(mag2);
    for(size_t i=0; i<count; i++) {
        mag2 *= scale;
    }
}

static void quat_lerp(float *out, float *in1, float *in2, float time)
{
    float dot = (in1[0]*in2[0])+(in1[1]*in2[1])+(in1[2]*in2[2])+(in1[3]*in2[3]);
    float out_scale  = (dot >= 0) ? 1.0f : -1.0f;
    for(size_t i=0; i<4; i++) {
        out[i] = ((1-time)*in1[i])+(out_scale*time*in2[i]);
    }
    vec_normalize(out, 4);
}

static void calc_anim_pose(model64_t *model, int anim_slot)
{
    model64_anim_t *curr_anim = &model->data->anims[model->active_anims[anim_slot].index];
    anim_buf_info_t anim_buf_info;
    read_model_anim_buf(model, &anim_buf_info, anim_slot);
    uint8_t *curr_buf = anim_buf_info.curr_buf;
    uint8_t *next_buf = anim_buf_info.next_buf;
    for(uint32_t i=0; i<curr_anim->num_channels; i++) {
        uint32_t data_size = 0;
        uint32_t component = curr_anim->channels[i] >> 30;
        uint32_t node_index = curr_anim->channels[i] & 0x3FFFFFFF;
        switch(component) {
            case ANIM_COMPONENT_POS:
                vec_lerp(model->transforms[node_index].transform.pos, (float *)curr_buf, (float *)next_buf, 3, anim_buf_info.time);
                data_size = 3*sizeof(float); 
                break;
                
            case ANIM_COMPONENT_ROT:
                quat_lerp(model->transforms[node_index].transform.rot, (float *)curr_buf, (float *)next_buf, anim_buf_info.time);
                data_size = 4*sizeof(float); 
                break;
                
            case ANIM_COMPONENT_SCALE:
                vec_lerp(model->transforms[node_index].transform.scale, (float *)curr_buf, (float *)next_buf, 3, anim_buf_info.time);
                data_size = 3*sizeof(float); 
                break;
        }
        calc_node_local_matrix(model, node_index);
        curr_buf += data_size;
        next_buf += data_size;
    }
    update_node_matrices(model);
}

void model64_update(model64_t *model, float dt)
{
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        if(model->active_anims[i].index == -1 || model->active_anims[i].paused || model->active_anims[i].speed == 0) {
            continue;
        }
        model->active_anims[i].time += model->active_anims[i].speed*dt;
        if(model->active_anims[i].loop) {
            model64_anim_t *curr_anim = &model->data->anims[model->active_anims[i].index];
            float anim_duration = curr_anim->num_frames/curr_anim->frame_rate;
            model->active_anims[i].time = fm_fmodf(model->active_anims[i].time, anim_duration);
            if(model->active_anims[i].time < 0) {
                model->active_anims[i].time += anim_duration;
            }
        }
        calc_anim_pose(model, i);
    }
}