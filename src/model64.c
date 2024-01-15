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
#include "sprite.h"
#include "utils.h"
#include "rdpq_tex.h"

#include "model64_catmull.h"


/** @brief Loading state of a texture_entry_t */
typedef enum {
    ENTRY_STATE_EMPTY = 0,
    ENTRY_STATE_SPRITE_LOADED,
    ENTRY_STATE_FULL
} texture_entry_state_t;

/** @brief A single, possibly empty, shared texture */
typedef struct texture_entry_s {
    char *path;                  ///< Original file path of a texture, used as a key
    texture_entry_state_t state; ///< Is only the sprite loaded or also a GL texture object
    sprite_t *sprite;            ///< A sprite with image data
    GLuint obj;                  ///< Texture object created on first draw
    int ref_count;               ///< Reference count from all models
} texture_entry_t;

/** @brief Contains shared textures and their metadata */
typedef struct texture_table_s {
    texture_entry_t *entries; ///< Path to entry mapping with empty slots
    uint32_t size;            ///< Number of elements allocated for the array above
    int ref_count;            ///< How many models have shared textures
} texture_table_t;

static texture_table_t* shared_textures;

void texture_table_allocate()
{
    shared_textures = calloc(1, sizeof(texture_table_t));
    shared_textures->size = 2;
    shared_textures->entries = calloc(shared_textures->size, sizeof(shared_textures->entries[0]));
    shared_textures->ref_count = 1;
}

void free_texture_entry(texture_entry_t *entry) {
    assertf(entry->ref_count == 0, "Leaked a texture entry %p", entry);
    free(entry->path);
    sprite_free(entry->sprite);
    if (entry->state == ENTRY_STATE_FULL) {
        glDeleteTextures(1, &entry->obj);
    }
    entry->path = NULL;
    entry->state = ENTRY_STATE_EMPTY;
    entry->sprite = NULL;
}

void texture_table_free()
{
    assertf(shared_textures->ref_count == 0, "Tried freeing texture table while still in use");

    for (uint32_t i = 0; i < shared_textures->size; i++) {
        texture_entry_t *entry = &shared_textures->entries[i];
        assertf(entry->state == ENTRY_STATE_EMPTY, "Shared texture %lu=%p was leaked", i, entry);
    }
    free(shared_textures->entries);
    free(shared_textures);
    shared_textures = NULL;
}

uint32_t texture_table_get(const char* path)
{
    for (uint32_t i = 0; i < shared_textures->size; i++) {
        texture_entry_t *entry = &shared_textures->entries[i];
        if (entry->state != ENTRY_STATE_EMPTY) {
            if (strcmp(shared_textures->entries[i].path, path) == 0) {
                return i;
            }
        }
    }

    return TEXTURE_INDEX_MISSING;
}

uint32_t texture_table_add(const char* path, const char* prefix)
{
    uint32_t idx = TEXTURE_INDEX_MISSING;

    for (uint32_t i = 0; i < shared_textures->size; i++) {
        texture_entry_t *entry = &shared_textures->entries[i];
        if (entry->state == ENTRY_STATE_EMPTY) {
            idx = i;
            break;
        }
    }

    if (idx == TEXTURE_INDEX_MISSING) {
        // Table must be full because a free slot wasn't found.
        uint32_t new_size = shared_textures->size * 2;
        shared_textures->entries = realloc(shared_textures->entries, new_size * sizeof(shared_textures->entries[0]));
        assertf(shared_textures->entries, "Entry array allocation failed");

        for (uint32_t i = shared_textures->size; i < new_size; i++) {
            memset(&shared_textures->entries[i], 0, sizeof(shared_textures->entries[i]));
        }

        idx = shared_textures->size;
        shared_textures->size = new_size;
    }

    char prefixed[strlen(prefix) + strlen(path) + 1]; 
    prefixed[0] = '\0';
    strcat(prefixed, prefix);
    strcat(prefixed, path);
    
    sprite_t *sprite = sprite_load(prefixed);

    if (!sprite) {
        assertf(false, "Failed to load texture %s\n", prefixed);
        return TEXTURE_INDEX_MISSING;
    }

    size_t size = strlen(path) + 1;
    char *new = malloc(size);
    strncpy(new, path, size);

    texture_entry_t* entry = &shared_textures->entries[idx];
    entry->path = new;
    entry->sprite = sprite;
    entry->ref_count = 0; // caller will increment ref_count if it stored a reference
    entry->state = ENTRY_STATE_SPRITE_LOADED; // entry->obj gets initialized on first draw

    return idx;
}

void texture_table_inc_ref_count(uint32_t idx)
{
    if (idx == TEXTURE_INDEX_MISSING) return;
    assert(idx < shared_textures->size);
    assert(shared_textures->entries[idx].ref_count >= 0);
    shared_textures->entries[idx].ref_count++;
}

void texture_table_dec_ref_count(uint32_t idx)
{
    if (idx == TEXTURE_INDEX_MISSING) return;

    assert(idx < shared_textures->size);

    texture_entry_t* entry = &shared_textures->entries[idx];
    assert(entry->ref_count > 0);

    if (--entry->ref_count == 0) {
        free_texture_entry(entry);
    }
}

#define PTR_DECODE(model, ptr)    ((void*)(((uint8_t*)(model)) + (uint32_t)(ptr)))
#define PTR_ENCODE(model, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(model)))

static model64_data_t *load_model_data_buf(void *buf, int sz, const char* prefix)
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
    model->texture_paths = PTR_DECODE(model, model->texture_paths);
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
    if (model->num_textures > 0) {
        assertf(prefix, "Trying to load a textured model from memory, only file system supported");
        if (shared_textures) {
            shared_textures->ref_count++;
        } else {
            texture_table_allocate();
        }
    }
    for (uint32_t i = 0; i < model->num_textures; i++)
    {
        model->texture_paths[i] = PTR_DECODE(model, model->texture_paths[i]);
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

            if (primitive->local_texture != TEXTURE_INDEX_MISSING) {
                uint32_t idx = texture_table_get(model->texture_paths[primitive->local_texture]);

                if (idx == TEXTURE_INDEX_MISSING) {
                    idx = texture_table_add(model->texture_paths[primitive->local_texture], prefix);
                }

                assert(idx != TEXTURE_INDEX_MISSING);

                primitive->shared_texture = idx;
                texture_table_inc_ref_count(idx);
            }
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
    model64_data_t *data = load_model_data_buf(buf, sz, NULL);
    assertf(!data->anim_data_handle, "Streaming animations not supported when loading model from buffer");
    return data;
}

static model64_data_t *load_model64_data(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);

    // Path will have at least a file system prefix so a slash will be found.
    char* slash = strrchr(fn, '/');
    char prefix[slash - fn + 2];
    memcpy(prefix, fn, slash - fn + 1);
    prefix[sizeof(prefix)-1] = '\0';

    model64_data_t *data = load_model_data_buf(buf, sz, prefix);
    if(data->anim_data_handle) {
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
    return make_model_instance(data);
}

model64_t *model64_load(const char *fn)
{
    model64_data_t *data = load_model64_data(fn);
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
            texture_table_dec_ref_count(primitive->shared_texture);
            primitive->shared_texture = TEXTURE_INDEX_MISSING;
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
        bool had_textures = data->num_textures > 0;
        unload_model_data(data);
        if (had_textures) {
            if (--shared_textures->ref_count == 0) {
                texture_table_free();
            }
        }
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
    if (primitive->shared_texture != TEXTURE_INDEX_MISSING) {
        texture_entry_t *entry = &shared_textures->entries[primitive->shared_texture];

        if (entry->state == ENTRY_STATE_SPRITE_LOADED) {
            glGenTextures(1, &entry->obj);
            glBindTexture(GL_TEXTURE_2D, entry->obj);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            // If a dimension is not a power of two then clamp and otherwise repeat.
            float rs = (entry->sprite->width & (entry->sprite->width-1)) ? 1 : REPEAT_INFINITE;
            float rt = (entry->sprite->height & (entry->sprite->height-1)) ? 1 : REPEAT_INFINITE;
            glSpriteTextureN64(GL_TEXTURE_2D, entry->sprite, &(rdpq_texparms_t){.s.repeats = rs, .t.repeats = rt});

            entry->state = ENTRY_STATE_FULL;
        }

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, entry->obj);
    }

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
    state_size += (num_tracks*4)*sizeof(decoded_keyframe_t);
    state_size += sizeof(model64_keyframe_t);
    anim_state_t *anim_state = calloc(1, state_size);
    anim_state->frames = PTR_DECODE(anim_state, sizeof(anim_state_t));
    anim_state->curr_frame = PTR_DECODE(anim_state->frames, (num_tracks*4)*sizeof(decoded_keyframe_t));
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
    assertf(start_time >= 0, "Start time must be strictly non-negative");
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
    assertf(time >= 0, "Time must be strictly non-negative");
    if(!model->active_anims[slot]) {
        alloc_anim_slot(model, slot);
    }
    float old_time = model->active_anims[slot]->time;
    if(time < old_time) {
        model->active_anims[slot]->invalid_pose = true;
    }
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

static void decode_normalized_u16(uint16_t *in, float *out, size_t count, float f1, float f2)
{
    for(size_t i=0; i<count; i++) {
        out[i] = (in[i]*f1)+f2;
    }
}

static void decode_quaternion(uint16_t *in, float *out)
{
    uint16_t values[3];
    int max_component;
    values[0] = in[0] & 0x7FFF;
    values[1] = in[1] & 0x7FFF;
    values[2] = in[2] & 0x7FFF;
    max_component = (in[0]>>15) | (in[1]>>15<<1);
    float decoded_values[3];
    for(size_t i=0; i<3; i++) {
        decoded_values[i] = (values[i]*0.000043159689f)-0.70710678f;
    }
    int16_t sign = (int16_t)in[2];
    float mag2_decoded = (decoded_values[0]*decoded_values[0])+(decoded_values[1]*decoded_values[1])+(decoded_values[2]*decoded_values[2]);
    float derived_component = copysignf(sqrtf(1.0f - mag2_decoded), (float)sign);
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

static void decode_cur_keyframe(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    uint16_t track = anim_state->curr_frame->track;
    decoded_keyframe_t keyframe;
    keyframe.time = anim_state->curr_frame->time;
    switch(curr_anim->tracks[track] >> 14) {
        case ANIM_COMPONENT_POS:
            decode_normalized_u16(anim_state->curr_frame->data, keyframe.data, 3, curr_anim->pos_f1, curr_anim->pos_f2);
            keyframe.data[3] = 0.0f;
            break;
            
        case ANIM_COMPONENT_ROT:
            decode_quaternion(anim_state->curr_frame->data, keyframe.data);
            break;
            
        case ANIM_COMPONENT_SCALE:
            decode_normalized_u16(anim_state->curr_frame->data, keyframe.data, 3, curr_anim->scale_f1, curr_anim->scale_f2);
            keyframe.data[3] = 0.0f;
            break;
          
        default:
            keyframe.data[0] = keyframe.data[1] = keyframe.data[2] = keyframe.data[3] = 0.0f;
            break;
    }
    anim_state->frames[track*4] = anim_state->frames[(track*4)+1];
    anim_state->frames[(track*4)+1] = anim_state->frames[(track*4)+2];
    anim_state->frames[(track*4)+2] = anim_state->frames[(track*4)+3];
    anim_state->frames[(track*4)+3] = keyframe;
}

static bool read_keyframe(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    if(anim_state->prev_waiting_frame) {
        if(!anim_state->done_decoding) {
            decode_cur_keyframe(model, anim_slot);
        }
    } else {
        anim_state->prev_waiting_frame = true;
    }
    if(anim_state->frame_idx >= curr_anim->num_keyframes) {
        anim_state->done_decoding = true;
        return false;
    }
    if(model->data->anim_data_handle) {
        uint32_t rom_addr = (uint32_t)model->data->anim_data_handle;
        rom_addr += (uint32_t)curr_anim->keyframes;
        rom_addr += anim_state->frame_idx*sizeof(model64_keyframe_t);
        data_cache_hit_writeback_invalidate(anim_state->curr_frame, sizeof(model64_keyframe_t));
        dma_read(anim_state->curr_frame, rom_addr, sizeof(model64_keyframe_t));
    } else {
        memcpy(anim_state->curr_frame, &curr_anim->keyframes[anim_state->frame_idx], sizeof(model64_keyframe_t));
    }
    anim_state->frame_idx++;
    return true;
}

static void init_keyframes(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    uint32_t num_tracks = curr_anim->num_tracks;
    anim_state->frame_idx = 0;
    anim_state->prev_waiting_frame = false;
    anim_state->done_decoding = false;
    for(size_t i=0; i<(num_tracks*4)+1; i++) {
        read_keyframe(model, anim_slot);
    }
}

static void fetch_needed_keyframes(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    while(anim_state->curr_frame->time_req < anim_state->time) {
        if(!read_keyframe(model, anim_slot)) {
            return;
        }
    }
}

static void calc_anim_pose(model64_t *model, model64_anim_slot_t anim_slot)
{
    anim_state_t *anim_state = model->active_anims[anim_slot];
    model64_anim_t *curr_anim = &model->data->anims[anim_state->index];
    for(uint32_t i=0; i<curr_anim->num_tracks; i++) {
        decoded_keyframe_t *curr_frame = &anim_state->frames[i*4];
        uint32_t component = curr_anim->tracks[i] >> 14;
        uint16_t node = curr_anim->tracks[i] & 0x3FFF;
        float time = curr_frame[1].time;
        float time_next = curr_frame[2].time;
        float weight = (time == time_next) ? 0 : (anim_state->time-time)/(time_next-time);
        assert(weight <= 1.0f && weight >= 0.0f);
        float *out;
        size_t out_count;
        switch(component) {
            case ANIM_COMPONENT_POS:
                out = model->transforms[node].transform.pos;
                out_count = 3;
                break;
                
            case ANIM_COMPONENT_ROT:
                out = model->transforms[node].transform.rot;
                out_count = 4;
                break;
                
            case ANIM_COMPONENT_SCALE:
                out = model->transforms[node].transform.scale;
                out_count = 3;
                break;
              
            default:
                out = NULL;
                break;
        }
        if(out == NULL) {
            continue;
        }
        if(component == ANIM_COMPONENT_ROT) {
            quat_lerp(out, curr_frame[1].data, curr_frame[2].data, weight);
        } else {
            catmull_calc_vec(curr_frame[0].data, curr_frame[1].data, curr_frame[2].data, curr_frame[3].data, out, weight, out_count);
        }
        if(i == curr_anim->num_tracks-1 || (curr_anim->tracks[i+1] & 0x3FFF) != node) {
            calc_node_local_matrix(model, node);
        }
    }
}

void model64_update(model64_t *model, float deltatime)
{
    assertf(deltatime >= 0, "Delta time must not be negative");
    for(int i=0; i<MAX_ACTIVE_ANIMS; i++) {
        if(!model->active_anims[i]) {
            continue;
        }
        if(model->active_anims[i]->index == -1 || model->active_anims[i]->paused || model->active_anims[i]->speed == 0) {
            if(model->active_anims[i]->index != -1 && model->active_anims[i]->invalid_pose) {
                model64_anim_t *curr_anim = &model->data->anims[model->active_anims[i]->index];
                if(model->active_anims[i]->time >= curr_anim->duration) {
                    model->active_anims[i]->time = curr_anim->duration;
                }
                init_keyframes(model, i);
                fetch_needed_keyframes(model, i);
                calc_anim_pose(model, i);
                model->active_anims[i]->invalid_pose = false;
            }
            continue;
        }
        model64_anim_t *curr_anim = &model->data->anims[model->active_anims[i]->index];
        model->active_anims[i]->time += model->active_anims[i]->speed*deltatime;
        if(model->active_anims[i]->time >= curr_anim->duration) {
            if(model->active_anims[i]->loop) {
                while(model->active_anims[i]->time >= curr_anim->duration) {
                    model->active_anims[i]->time -= curr_anim->duration;
                }
                model->active_anims[i]->invalid_pose = true;
            } else {
                model->active_anims[i]->time = curr_anim->duration;
                model->active_anims[i]->paused = true;
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