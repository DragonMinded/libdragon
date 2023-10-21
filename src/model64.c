#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "fmath.h"
#include "n64sys.h"
#include "GL/gl.h"
#include "dragonfs.h"
#include "dma.h"
#include "model64.h"
#include "model64_internal.h"
#include "asset.h"
#include "debug.h"
#include "utils.h"

/** @brief State of an active animation */
typedef struct anim_buf_info_s {
    void *curr_buf;     ///< Data buffer for current frame
    void *next_buf;     ///< Data buffer for next frame
    float time;         ///< Interpolation factor between current and next frame
} anim_buf_info_t;

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
        model->anims[i].tracks = PTR_DECODE(model, model->anims[i].tracks);
        if(!model->anim_data_handle)
        {
            model->anims[i].keyframes = PTR_DECODE(model, model->anims[i].keyframes);
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

static model64_data_t *load_model64_data_buf(void *buf, int sz)
{
    model64_data_t *data = load_model_data_buf(buf, sz);
    assertf(!data->anim_data_handle, "Streaming animations not supported when loading model from buffer");
    return data;
}

static model64_data_t *load_model64_data(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    model64_data_t *data = load_model_data_buf(buf, sz);
    if(!data->anim_data_handle) {
        assertf(strncmp(fn, "rom:/", 5) == 0, "Cannot open %s: models with streamed animations must be stored in ROM (rom:/)", fn);
        char anim_name[strlen(fn)+6];
        sprintf(anim_name, "%s.anim", fn);
        data->anim_data_handle = (void *)dfs_rom_addr(anim_name+5);
    }
    return data;
}

model64_t *model64_load_buf(void *buf, int sz)
{
    model64_data_t *data = load_model64_data_buf(buf, sz);
    data->ref_count++;
    return make_model_instance(data);
}

model64_t *model64_load(const char *fn)
{
    model64_data_t *data = load_model64_data(fn);
    data->ref_count++;
    return make_model_instance(data);
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
        model->anims[i].tracks = PTR_ENCODE(model, model->anims[i].tracks);
        if(!model->anim_data_handle)
        {
            model->anims[i].keyframes = PTR_ENCODE(model, model->anims[i].keyframes);
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

static void free_model64_data(model64_data_t *data)
{
    if(--data->ref_count == 0)
    {
        unload_model_data(data);
    }
}

void model64_free(model64_t *model)
{
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        if(model->active_anims[i]) {
            free(model->active_anims[i]);
        }
    }
    free_model64_data(model->data);
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

static void alloc_anim_slot(model64_t *model, model64_anim_slot_t slot)
{
    uint32_t state_size = sizeof(anim_state_t);
    uint32_t num_tracks = model->data->max_tracks;
    state_size += (num_tracks*4)*sizeof(model64_keyframe_t);
    state_size += sizeof(model64_keyframe_t);
    state_size += num_tracks;
    anim_state_t *anim_state = calloc(1, state_size);
    anim_state->frames = PTR_DECODE(anim_state, sizeof(anim_state_t));
    anim_state->waiting_frame = PTR_DECODE(anim_state->frames, (num_tracks*4)*sizeof(model64_keyframe_t));
    anim_state->buf_idx = PTR_DECODE(anim_state->waiting_frame, sizeof(model64_keyframe_t));
    anim_state->index = -1;
    anim_state->paused = true;
    anim_state->speed = 1.0f;
    model->active_anims[slot] = anim_state;
}


static bool is_anim_slot_valid(model64_anim_slot_t slot)
{
    return (slot >= 0) && (slot < MAX_ACTIVE_ANIMS);
}

void model64_anim_play(model64_t *model, const char *anim, model64_anim_slot_t slot, bool paused, float start_time)
{
    int32_t anim_index = search_anim_index(model, anim);
    assertf(anim_index != -1, "Animation %s was not found\n", anim);
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    model->active_anims[slot]->index = anim_index;
    model->active_anims[slot]->paused = paused;
    model->active_anims[slot]->time = start_time;
    model->active_anims[slot]->loop = false;
    model->active_anims[slot]->invalid_pose = true;
    model->active_anims[slot]->prev_waiting_frame = false;
    model->active_anims[slot]->speed = 1.0f;
}

void model64_anim_stop(model64_t *model, model64_anim_slot_t slot)
{
    assertf(is_anim_slot_valid(slot), "Invalid animation ID");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    model->active_anims[slot]->index = -1;
}

float model64_anim_get_length(model64_t *model, const char *anim)
{
    int32_t anim_index = search_anim_index(model, anim);
    assertf(anim_index != -1, "Animation %s was not found\n", anim);
    return (float)model->data->anims[anim_index].duration;
}

float model64_anim_get_time(model64_t *model, model64_anim_slot_t slot)
{
    assertf(is_anim_slot_valid(slot), "Invalid animation ID");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    return model->active_anims[slot]->time;
}

float model64_anim_set_time(model64_t *model, model64_anim_slot_t slot, float time)
{
    assertf(is_anim_slot_valid(slot), "Invalid animation ID");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    float old_time = model->active_anims[slot]->time;
    model->active_anims[slot]->time = time;
    return old_time;
}

float model64_anim_set_speed(model64_t *model, model64_anim_slot_t slot, float speed)
{
    assertf(is_anim_slot_valid(slot), "Invalid animation ID");
    assertf(speed >= 0, "Speed cannot be negative");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    float old_speed = model->active_anims[slot]->speed; 
    model->active_anims[slot]->speed = speed;
    return old_speed;
}

bool model64_anim_set_loop(model64_t *model, model64_anim_slot_t slot, bool loop)
{
    assertf(is_anim_slot_valid(slot), "Invalid animation ID");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    bool old_loop = model->active_anims[slot]->loop; 
    model->active_anims[slot]->loop = loop;
    return old_loop;
}

bool model64_anim_set_pause(model64_t *model, model64_anim_slot_t slot, bool paused)
{
    assertf(is_anim_slot_valid(slot), "Invalid animation ID");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    bool old_pause = model->active_anims[slot]->paused;
    model->active_anims[slot]->paused = paused;
    return old_pause;
}

static float catmull_calc(float p1, float p2, float p3, float p4, float t)
{
    float a = (-1*p1  +3*p2 -3*p3 + 1*p4) * t*t*t;
    float b = ( 2*p1  -5*p2 +4*p3 - 1*p4) * t*t;
    float c = (  -p1        +  p3)        * t;
    float d = 2*p2;
    return 0.5f * (a+b+c+d);
}

static void catmull_calc_vec(float *p1, float *p2, float *p3, float *p4, float *out, float t, int num_values)
{
    for(int i=0; i<num_values; i++) {
        out[i] = catmull_calc(p1[i], p2[i], p3[i], p4[i], t);
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

static void decode_normalized_u16(uint16_t *in, float *out, size_t count, float min, float max)
{
    for(size_t i=0; i<count; i++) {
        out[i] = ((in[i]/65535.0f)*(max-min))+min;
    }
}

static void decode_quaternion(uint16_t *in, float *out)
{
    uint16_t values[3];
    int max_component;
    values[0] = ((in[0] & 0x1FFF) << 2)|(in[1] >> 14);
    values[1] = ((in[1] & 0x3FFF) << 1)|(in[2] >> 15);
    values[2] = in[2] & 0x7FFF;
    max_component = (in[0] & 0x6000) >> 13;
    float decoded_values[3];
    for(size_t i=0; i<3; i++) {
        decoded_values[i] = (values[i]*0.000043159689f)-0.70710678f;
    }
    float derived_component = sqrtf(1.0f-(decoded_values[0]*decoded_values[0])-(decoded_values[1]*decoded_values[1])-(decoded_values[2]*decoded_values[2]));
    int src_component = 0;
    for(size_t i=0; i<4; i++) {
        if(i == max_component) {
            out[i] = derived_component;
        } else {
            out[i] = decoded_values[src_component];
            src_component++;
        }
    }
}

static void copy_waiting_frame(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    uint16_t track = anim_state->waiting_frame->track;
    uint8_t buffer = anim_state->buf_idx[track];
    memcpy(&anim_state->frames[(track*4)+buffer], anim_state->waiting_frame, sizeof(model64_keyframe_t));
    anim_state->buf_idx[track] = (buffer+1)%4;
}

static void read_waiting_frame(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    if(anim_state->prev_waiting_frame) {
        copy_waiting_frame(model, anim_slot);
    } else {
        anim_state->prev_waiting_frame = true;
    }
    if(model->data->anim_data_handle) {
        uint32_t rom_addr = (uint32_t)model->data->anim_data_handle;
        rom_addr += (uint32_t)curr_anim->keyframes;
        rom_addr += anim_state->frame_idx*sizeof(model64_keyframe_t);
        data_cache_hit_writeback_invalidate(anim_state->waiting_frame, sizeof(model64_keyframe_t));
        dma_read(anim_state->waiting_frame, rom_addr, sizeof(model64_keyframe_t));
    } else {
        memcpy(anim_state->waiting_frame, &curr_anim->keyframes[anim_state->frame_idx], sizeof(model64_keyframe_t));
    }
    anim_state->frame_idx++;
}

static void init_keyframes(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    uint32_t num_tracks = curr_anim->num_tracks;
    uint32_t frame_buf_size = (num_tracks*4)*sizeof(model64_keyframe_t);
    if(model->data->anim_data_handle) {
        uint32_t rom_addr = (uint32_t)model->data->anim_data_handle;
        rom_addr += (uint32_t)curr_anim->keyframes;
        data_cache_hit_writeback_invalidate(anim_state->frames, frame_buf_size);
        dma_read(anim_state->frames, rom_addr, frame_buf_size);
    } else {
        memcpy(anim_state->frames, curr_anim->keyframes, frame_buf_size);
    }
    for(uint32_t i=0; i<num_tracks; i++) {
        anim_state->buf_idx[i] = 0;
    }
    anim_state->frame_idx = num_tracks*4;
    if(anim_state->frame_idx >= curr_anim->num_keyframes) {
        memcpy(anim_state->waiting_frame, &anim_state->frames[(num_tracks*4)-1], frame_buf_size);
        anim_state->frame_idx = curr_anim->num_keyframes-1;
    } else {
        read_waiting_frame(model, anim_slot);
    }
    
}

static void fetch_needed_keyframes(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    if(anim_state->frame_idx >= curr_anim->num_keyframes-1) {
        return;
    }
    while(anim_state->waiting_frame->time_req < anim_state->time) {
        read_waiting_frame(model, anim_slot);
        if(anim_state->frame_idx >= curr_anim->num_keyframes-1) {
            return;
        }
    }
}

static void calc_anim_pose(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    float decoded_values[4][4];
    for(uint32_t i=0; i<curr_anim->num_tracks; i++) {
        uint32_t component = curr_anim->tracks[i] >> 14;
        uint16_t node = curr_anim->tracks[i] & 0x3FFF;
        for(uint32_t j=0; j<4; j++) {
            model64_keyframe_t *frame = &anim_state->frames[(i*4)+((j+anim_state->buf_idx[i]) % 4)];
            switch(component) {
                case ANIM_COMPONENT_POS:
                    decode_normalized_u16(frame->data, &decoded_values[j][0], 3, curr_anim->pos_min, curr_anim->pos_max);
                    break;
                    
                case ANIM_COMPONENT_ROT:
                    decode_quaternion(frame->data, &decoded_values[j][0]);
                    break;
                    
                case ANIM_COMPONENT_SCALE:
                    decode_normalized_u16(frame->data, &decoded_values[j][0], 3, curr_anim->scale_min, curr_anim->scale_max);
                    break;
                  
                default:
                    break;
            }
        }
        float time = anim_state->frames[(i*4)+((1+anim_state->buf_idx[i]) % 4)].time;
        float time_next = anim_state->frames[(i*4)+((2+anim_state->buf_idx[i]) % 4)].time;
        float weight = (anim_state->time-time)/(time_next-time);
        switch(component) {
            case ANIM_COMPONENT_POS:
                catmull_calc_vec(&decoded_values[0][0], &decoded_values[1][0], 
                    &decoded_values[2][0], &decoded_values[3][0], model->transforms[node].transform.pos, weight, 3);
                break;
                
            case ANIM_COMPONENT_ROT:
                catmull_calc_vec(&decoded_values[0][0], &decoded_values[1][0], 
                    &decoded_values[2][0], &decoded_values[3][0], model->transforms[node].transform.rot, weight, 4);
                vec_normalize(model->transforms[node].transform.rot, 4);
                break;
                
            case ANIM_COMPONENT_SCALE:
                catmull_calc_vec(&decoded_values[0][0], &decoded_values[1][0], 
                    &decoded_values[2][0], &decoded_values[3][0], model->transforms[node].transform.scale, weight, 4);
                break;
              
            default:
                break;
        }
    }
}

void model64_update(model64_t *model, float deltatime)
{
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        if(!model->active_anims[i]) {
            continue;
        }
        if(model->active_anims[i]->index == -1 || model->active_anims[i]->paused || model->active_anims[i]->speed == 0) {
            if(model->active_anims[i]->index != -1 && model->active_anims[i]->invalid_pose) {
                init_keyframes(model, i);
                fetch_needed_keyframes(model, i);
                model->active_anims[i]->invalid_pose = false;
            }
            continue;
        }
        model->active_anims[i]->time += model->active_anims[i]->speed*deltatime;
        if(model->active_anims[i]->loop) {
            model64_anim_t *curr_anim = &model->data->anims[model->active_anims[i]->index];
            if(model->active_anims[i]->time >= curr_anim->duration) {
                model->active_anims[i]->time -= curr_anim->duration;
                model->active_anims[i]->invalid_pose = true;
                
            }
        }
        if(model->active_anims[i]->invalid_pose) {
            init_keyframes(model, i);
            model->active_anims[i]->invalid_pose = false;
        }
        fetch_needed_keyframes(model, i);
        calc_anim_pose(model, i);
    }
    update_node_matrices(model);
}