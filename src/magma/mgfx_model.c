#include "mgfx_model.h"
#include "mgfx_model_internal.h"
#include "asset.h"
#include "dragonfs.h"

static mgfx_registry_t global_registry;

#define PTR_DECODE(model, ptr)    ((void*)(((uint8_t*)(model)) + (uint32_t)(ptr)))
#define PTR_ENCODE(model, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(model)))

static void load_meshes(mgfx_model_t *model)
{
    model->meshes = PTR_DECODE(model, model->meshes);
    for (size_t i = 0; i < model->meshes_count; i++) {
        mgfx_mesh_t *mesh = &model->meshes[i];
        mesh->submeshes = PTR_DECODE(model, mesh->submeshes);
        for (size_t j = 0; j < mesh->submesh_count; j++) {
            mgfx_submesh_t *submesh = &mesh->submeshes[j];
            submesh->vertex_layout.attributes = PTR_DECODE(model, submesh->vertex_layout.attributes);
            submesh->vertices = PTR_DECODE(model, submesh->vertices);
            submesh->indices = PTR_DECODE(model, submesh->indices);
            if (submesh->mtx_indices) {
                submesh->mtx_indices = PTR_DECODE(model, submesh->mtx_indices);
            }
        }
    }
}

static void load_nodes(mgfx_model_t *model)
{
    model->nodes = PTR_DECODE(model, model->nodes);
    for (size_t i = 0; i < model->nodes_count; i++) {
        mgfx_node_data_t *node = &model->nodes[i];
        if (node->name) {
            node->name = PTR_DECODE(model, node->name);
        }
        node->children_indices = PTR_DECODE(model, node->children_indices);
    }
}

static void load_materials(mgfx_model_t *model)
{
    model->materials = PTR_DECODE(model, model->materials);
    for (size_t i = 0; i < model->materials_count; i++) {
        model->materials[i] = PTR_DECODE(model, model->materials[i]);
    }
    
    model->matdb = PTR_DECODE(model, model->matdb);
}

static void load_animations(mgfx_model_t *model) 
{
    model->animations = PTR_DECODE(model, model->animations);
    for (size_t i = 0; i < model->animations_count; i++) {
        mgfx_animation_t *animation = &model->animations[i];
        if (animation->name) {
            animation->name = PTR_DECODE(model, animation->name);
        }
        animation->tracks = PTR_DECODE(model, animation->tracks);
        if (animation->keyframes) {
            animation->keyframes = PTR_DECODE(model, animation->keyframes);
        }
    }
}

static void load_skins(mgfx_model_t *model)
{
    model->skins = PTR_DECODE(model, model->skins);    
}

static mgfx_model_t *load_model(void *buf, int sz, const char* fn)
{
    mgfx_model_t *model = buf;
    assertf(sz >= sizeof(mgfx_model_t), "mgfx model buffer is too small: %d", sz);
    assertf(memcmp(model->id, MGFX_MODEL_ID, MGFX_MODEL_ID_LEN) == 0, "Invalid mgfx model header: %s", fn);
    assertf(model->version == MGFX_MODEL_VERSION, "Invalid mgfx model version (%d): %s", model->version, fn);

    load_meshes(model);
    load_nodes(model);
    load_materials(model);
    load_animations(model);
    load_skins(model);

    data_cache_hit_writeback(model, sz);

    return model;
}

static void mark_model_as_owned(mgfx_model_t *model)
{
    model->id[2] = 'O';
}

static bool is_model_owned(mgfx_model_t *model)
{
    return model->id[2] == 'O';
}

mgfx_model_t *mgfx_model_load(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    mgfx_model_t *model = load_model(buf, sz, fn);

    if (model->animation_stream) {
        assertf(strncmp(fn, "rom:/", 5) == 0, "Cannot open %s: models with streamed animations must be stored in ROM (rom:/)", fn);
        char anim_name[strlen(fn)+6];
        sprintf(anim_name, "%s.anim", fn);
        model->animation_stream = dfs_rom_addr(anim_name+5);
    }

    mark_model_as_owned(model);
    return model;
}

mgfx_model_t *mgfx_model_load_buf(void *buf, int sz)
{
    mgfx_model_t *model = load_model(buf, sz, "<buffer>");
    assertf(!model->animation_stream, "Streaming animations not supported when loading model from buffer");
    return model;
}

static void free_state(mgfx_model_state_t *state, mgfx_model_t *model)
{
    for (size_t i = 0; i < model->materials_count; i++) {
        if (state->materials[i].rdpq_mat)
            rdpq_mat_free(state->materials[i].rdpq_mat);
    }
    for (size_t i = 0; i < model->meshes_count; i++) {
        for (size_t j = 0; j < model->meshes[i].submesh_count; j++) {
            if (state->meshes[i].submesh_blocks[j])
                rspq_block_free(state->meshes[i].submesh_blocks[j]);
        }
    }
    
    free(state);
}

static void unload_meshes(mgfx_model_t *model)
{
    for (size_t i = 0; i < model->meshes_count; i++) {
        mgfx_mesh_t *mesh = &model->meshes[i];
        for (size_t j = 0; j < mesh->submesh_count; j++) {
            mgfx_submesh_t *submesh = &mesh->submeshes[j];
            submesh->vertex_layout.attributes = PTR_ENCODE(model, submesh->vertex_layout.attributes);
            submesh->vertices = PTR_ENCODE(model, submesh->vertices);
            submesh->indices = PTR_ENCODE(model, submesh->indices);
            if (submesh->mtx_indices) {
                submesh->mtx_indices = PTR_ENCODE(model, submesh->mtx_indices);
            }
        }
        mesh->submeshes = PTR_ENCODE(model, mesh->submeshes);
    }
    model->meshes = PTR_ENCODE(model, model->meshes);
}

static void unload_nodes(mgfx_model_t *model)
{
    for (size_t i = 0; i < model->nodes_count; i++) {
        mgfx_node_data_t *node = &model->nodes[i];
        if (node->name) {
            node->name = PTR_ENCODE(model, node->name);
        }
        node->children_indices = PTR_ENCODE(model, node->children_indices);
    }
    model->nodes = PTR_ENCODE(model, model->nodes);
}

static void unload_materials(mgfx_model_t *model)
{
    for (size_t i = 0; i < model->materials_count; i++) {
        model->materials[i] = PTR_ENCODE(model, model->materials[i]);
    }
    model->materials = PTR_ENCODE(model, model->materials);
    model->matdb = PTR_ENCODE(model, model->matdb);
}

static void unload_animations(mgfx_model_t *model)
{
    for (size_t i = 0; i < model->animations_count; i++) {
        mgfx_animation_t *animation = &model->animations[i];
        if (animation->name) {
            animation->name = PTR_ENCODE(model, animation->name);
        }
        animation->tracks = PTR_ENCODE(model, animation->tracks);
        if (animation->keyframes) {
            animation->keyframes = PTR_ENCODE(model, animation->keyframes);
        }
    }
    model->animations = PTR_ENCODE(model, model->animations);
}

static void unload_skins(mgfx_model_t *model)
{
    model->skins = PTR_ENCODE(model, model->skins);    
}

static void unload_model(mgfx_model_t *model)
{
    unload_meshes(model);
    unload_nodes(model);
    unload_materials(model);
    unload_animations(model);
    unload_skins(model);
}

void mgfx_model_free(mgfx_model_t *model)
{
    if (model->state) {
        free_state(model->state, model);
    }

    unload_model(model);

    if (is_model_owned(model)) {
        free(model);
    }
}

static uint32_t get_buffer_count(mgfx_instance_config_t *config)
{
    return config->buffer_count > 0 ? config->buffer_count : 1;
}

static void update_node_matrix(mgfx_node_t *node)
{
    if ((node->flags & MGFX_NODE_FLAG_MTX_DIRTY) == 0)
        return;
    
    fm_mat4_from_srt(&node->transform.matrix, 
        &node->transform.scale, 
        &node->transform.rotation, 
        &node->transform.position);
    
    node->flags &= ~MGFX_NODE_FLAG_MTX_DIRTY;
}

mgfx_instance_t *mgfx_model_instantiate(mgfx_model_t *model, mgfx_instance_config_t *config)
{
    size_t instance_size = sizeof(mgfx_instance_t) + model->nodes_count * sizeof(mgfx_node_t);
    mgfx_instance_t *instance = calloc(1, instance_size);

    instance->model = model;
    instance->nodes = (mgfx_node_t*)&instance[1];
    instance->buffer_count = get_buffer_count(config);

    for (size_t i = 0; i < model->nodes_count; i++) {
        mgfx_node_t *node = &instance->nodes[i];
        node->data = &model->nodes[i];
        node->instance = instance;
        memcpy(&node->transform, &node->data->transform, sizeof(mgfx_transform_t));
    }
    
    return instance;
}

static void free_node(mgfx_node_t *node)
{
    if (node->matrices) {
        free_uncached(node->matrices);
    }
}

void mgfx_instance_free(mgfx_instance_t *instance)
{
    for (size_t i = 0; i < instance->model->nodes_count; i++) {
        free_node(&instance->nodes[i]);
    }
    
    free(instance);
}

mgfx_model_t *mgfx_instance_get_model(mgfx_instance_t *instance)
{
    return instance->model;
}

mgfx_node_t *mgfx_instance_get_root_node(mgfx_instance_t *instance)
{
    mgfx_model_t *model = mgfx_instance_get_model(instance);
    return &instance->nodes[model->root_node_index];
}

mgfx_node_t *mgfx_instance_find_node(mgfx_instance_t *instance, const char *name)
{
    for (size_t i = 0; i < instance->model->nodes_count; i++) {
        if (strcmp(instance->model->nodes[i].name, name) == 0) {
            return &instance->nodes[i];
        }
    }
    return NULL;
}

static size_t get_model_state_size(mgfx_model_t *model)
{
    size_t size = sizeof(mgfx_model_state_t);
    size += sizeof(mgfx_mesh_state_t) * model->meshes_count;
    for (size_t i = 0; i < model->meshes_count; i++) {
        size += sizeof(rspq_block_t*) * model->meshes[i].submesh_count;
    }
    size += sizeof(mgfx_material_state_t) * model->materials_count;
    return size;
}

static mgfx_model_state_t *create_model_state(mgfx_model_t *model)
{
    size_t state_size = get_model_state_size(model);
    mgfx_model_state_t *state = calloc(1, state_size);
    state->meshes = (mgfx_mesh_state_t*)&state[1];
    state->materials = (mgfx_material_state_t*)&state->meshes[model->meshes_count];
    rspq_block_t **next_submesh_blocks_ptr = (rspq_block_t**)&state->materials[model->materials_count];
    for (size_t i = 0; i < model->meshes_count; i++) {
        state->meshes[i].submesh_blocks = next_submesh_blocks_ptr;
        next_submesh_blocks_ptr += model->meshes[i].submesh_count;
    }
    return state;
}

static mgfx_model_state_t *get_model_state(mgfx_model_t *model)
{
    if (model->state == NULL)
        model->state = create_model_state(model);
    
    return model->state;
}

static void update_node_matrices(mgfx_node_t *node)
{
    
}

static void update_matrices(mgfx_instance_t *instance)
{
    
}

static void draw_node(mgfx_node_t *node)
{
    if (node->data->mesh_index < 0)
        return;

    if (node->data->skin_index >= 0) {
        // TODO
    } else {
        // TODO
    }
}

void mgfx_instance_draw(mgfx_instance_t *instance)
{
    update_matrices(instance);
    for (size_t i = 0; i < instance->model->nodes_count; i++) {
        mgfx_node_t *node = &instance->nodes[i];
        draw_node(node);
    }
}

void mgfx_instance_update(mgfx_instance_t *instance, float deltatime)
{
    // TODO
}

mgfx_node_t *mgfx_node_get_parent(mgfx_node_t *node)
{
    if (node->data->parent_index)
        return NULL;
    
    return &node->instance->nodes[node->data->parent_index];
}

uint32_t mgfx_node_get_children_count(const mgfx_node_t *node)
{
    return node->data->children_count;
}

mgfx_node_t *mgfx_node_get_child(mgfx_node_t *node, uint32_t index)
{
    uint32_t children_count = mgfx_node_get_children_count(node);
    assertf(index < children_count, "Child index %ld is out of range: node has %ld children", index, children_count);
    uint32_t child_index = node->data->children_indices[index];
    return &node->instance->nodes[child_index];
}

static void mark_matrix_dirty(mgfx_node_t *node)
{
    node->flags |= MGFX_NODE_FLAG_MTX_DIRTY;
}

const fm_vec3_t *mgfx_node_get_pos(mgfx_node_t *node)
{
    return &node->transform.position;
}

void mgfx_node_set_pos(mgfx_node_t *node, const fm_vec3_t *pos)
{
    node->transform.position = *pos;
    mark_matrix_dirty(node);
}

const fm_quat_t *mgfx_node_get_rot(mgfx_node_t *node)
{
    return &node->transform.rotation;
}

void mgfx_node_set_rot(mgfx_node_t *node, const fm_quat_t *rot)
{
    node->transform.rotation = *rot;
    mark_matrix_dirty(node);
}

const fm_vec3_t *mgfx_node_get_scale(mgfx_node_t *node)
{
    return &node->transform.scale;
}

void mgfx_node_set_scale(mgfx_node_t *node, const fm_vec3_t *scale)
{
    node->transform.scale = *scale;
    mark_matrix_dirty(node);
}

uint32_t mgfx_model_get_meshes_count(const mgfx_model_t *model)
{
    return model->meshes_count;
}

mgfx_mesh_t *mgfx_model_get_mesh(mgfx_model_t *model, uint32_t index)
{
    assertf(index < model->meshes_count, "Mesh index %ld is out of range: model has %ld meshes", index, model->meshes_count);
    return &model->meshes[index];
}

mgfx_mesh_t *mgfx_model_find_mesh(mgfx_model_t *model, const char *name)
{
    for (size_t i = 0; i < model->meshes_count; i++) {
        mgfx_mesh_t *mesh = &model->meshes[i];
        // TODO: add name to mgfx_mesh_t
    }
    return NULL;
}
