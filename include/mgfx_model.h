#ifndef __MGFX_MODEL_H
#define __MGFX_MODEL_H

#include <stdint.h>

#include "mgfx_mesh_types.h"
#include "rdpq_mat.h"
#include "fgeom.h"

typedef struct mgfx_camera_s mgfx_camera_t;
typedef struct mgfx_model_s mgfx_model_t;
typedef struct mgfx_instance_s mgfx_instance_t;
typedef struct mgfx_node_s mgfx_node_t;
typedef struct mgfx_skin_s mgfx_skin_t;
typedef struct mgfx_animation_s mgfx_animation_t;
typedef struct mgfx_material_s mgfx_material_t;

typedef struct mgfx_instance_config_s {
    uint32_t buffer_count;
} mgfx_instance_config_t;

#ifdef __cplusplus
extern "C" {
#endif

mgfx_model_t *mgfx_model_load(const char *fn);
mgfx_model_t *mgfx_model_load_buf(void *buf, int sz);
void mgfx_model_free(mgfx_model_t *model);
mgfx_instance_t *mgfx_model_instantiate(mgfx_model_t *model, mgfx_instance_config_t *config);

void mgfx_instance_free(mgfx_instance_t *instance);
mgfx_model_t *mgfx_instance_get_model(mgfx_instance_t *instance);
mgfx_node_t *mgfx_instance_get_root_node(mgfx_instance_t *instance);
mgfx_node_t *mgfx_instance_find_node(mgfx_instance_t *instance, const char *name);
void mgfx_instance_draw(mgfx_instance_t *instance);
void mgfx_instance_update(mgfx_instance_t *instance, float deltatime);

mgfx_node_t *mgfx_node_get_parent(mgfx_node_t *node);
uint32_t mgfx_node_get_children_count(const mgfx_node_t *node);
mgfx_node_t *mgfx_node_get_child(mgfx_node_t *node, uint32_t index);

const fm_vec3_t *mgfx_node_get_pos(mgfx_node_t *node);
void mgfx_node_set_pos(mgfx_node_t *node, const fm_vec3_t *pos);
const fm_quat_t *mgfx_node_get_rot(mgfx_node_t *node);
void mgfx_node_set_rot(mgfx_node_t *node, const fm_quat_t *rot);
const fm_vec3_t *mgfx_node_get_scale(mgfx_node_t *node);
void mgfx_node_set_scale(mgfx_node_t *node, const fm_vec3_t *scale);

uint32_t mgfx_model_get_meshes_count(const mgfx_model_t *model);
mgfx_mesh_t *mgfx_model_get_mesh(mgfx_model_t *model, uint32_t index);
mgfx_mesh_t *mgfx_model_find_mesh(mgfx_model_t *model, const char *name);

uint32_t mgfx_model_get_animations_count(const mgfx_model_t *model);
mgfx_animation_t *mgfx_model_get_animation(mgfx_model_t *model, uint32_t index);
mgfx_animation_t *mgfx_model_find_animation(mgfx_model_t *model, const char *name);

uint32_t mgfx_model_get_skins_count(const mgfx_model_t *model);
mgfx_skin_t *mgfx_model_get_skin(mgfx_model_t *model, uint32_t index);
mgfx_skin_t *mgfx_model_find_skin(mgfx_model_t *model, const char *name);

rdpq_matdb_t *mgfx_model_get_matdb(mgfx_model_t *model);

// TODO: animations
//    a) create mgfx_animation_instance_t from mgfx_animation_t and attach mgfx_instance_t
//          -> similar to T3D
//    b) pass mgfx_animation_t into mgfx_instance_t to get animation_handle_t or something
//          -> might be more intuitive
//          -> how to handle blending?

#ifdef __cplusplus
}
#endif

#endif
