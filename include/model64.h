/**
 * @file model64.h
 * @brief 3D model loading, animation and drawing
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @author gamemasterplc <gamemasterplc@gmail.com>
 * @preview
 */
#ifndef __LIBDRAGON_MODEL64_H
#define __LIBDRAGON_MODEL64_H

#include <preview.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Animation slot enumeration for model64 animations */
typedef enum {
    MODEL64_ANIM_SLOT_0 = 0,
    MODEL64_ANIM_SLOT_1 = 1,
    MODEL64_ANIM_SLOT_2 = 2,
    MODEL64_ANIM_SLOT_3 = 3
} model64_anim_slot_t;

/// @cond
struct model64_s;
typedef struct model64_s model64_t;

struct mesh_s;
typedef struct mesh_s mesh_t;

struct primitive_s;
typedef struct primitive_s primitive_t;

struct model64_node_s;
typedef struct model64_node_s model64_node_t;
/// @endcond

PREVIEW_API model64_t *model64_load(const char *fn);
PREVIEW_API model64_t *model64_load_buf(void *buf, int sz);
PREVIEW_API void model64_free(model64_t *model);
PREVIEW_API model64_t *model64_clone(model64_t *model);

/**
 * @brief Return the number of meshes in this model.
 */
PREVIEW_API uint32_t model64_get_mesh_count(model64_t *model);

/**
 * @brief Return the mesh at the specified index.
 */
PREVIEW_API mesh_t *model64_get_mesh(model64_t *model, uint32_t mesh_index);


/**
 * @brief Return the number of nodes in this model.
 */
PREVIEW_API uint32_t model64_get_node_count(model64_t *model);

/**
 * @brief Return the node at the specified index.
 */
PREVIEW_API model64_node_t *model64_get_node(model64_t *model, uint32_t node_index);

/**
 * @brief Return the first node with the specified name in the model.
 */
PREVIEW_API model64_node_t *model64_search_node(model64_t *model, const char *name);

/**
 * @brief Sets the position of a node in a model relative to its parent.
 */
PREVIEW_API void model64_set_node_pos(model64_t *model, model64_node_t *node, float x, float y, float z);

/**
 * @brief Sets the rotation of a node in a model relative to its parent in the form of an euler angle (ZYX rotation order) in radians.
 */
PREVIEW_API void model64_set_node_rot(model64_t *model, model64_node_t *node, float x, float y, float z);

/**
 * @brief Sets the rotation of a node in a model relative to its parent in the form of a quaternion.
 */
PREVIEW_API void model64_set_node_rot_quat(model64_t *model, model64_node_t *node, float x, float y, float z, float w);

/**
 * @brief Sets the scale of a node in a model relative to its parent.
 */
PREVIEW_API void model64_set_node_scale(model64_t *model, model64_node_t *node, float x, float y, float z);

/**
 * @brief Gets the transformation matrix between a model's root node and a node in a model.
 */
PREVIEW_API void model64_get_node_world_mtx(model64_t *model, model64_node_t *node, float dst[16]);

/**
 * @brief Return the number of primitives in this mesh.
 */
PREVIEW_API uint32_t model64_get_primitive_count(mesh_t *mesh);

/**
 * @brief Return the primitive at the specified index.
 */
PREVIEW_API primitive_t *model64_get_primitive(mesh_t *mesh, uint32_t primitive_index);

/**
 * @brief Draw an entire model.
 * 
 * This will draw all nodes that are contained in the given model while applying the relevant node matrices.
 */
PREVIEW_API void model64_draw(model64_t *model);

/**
 * @brief Draw a single mesh.
 * 
 * This will draw all of the given mesh's primitives.
 */
PREVIEW_API void model64_draw_mesh(mesh_t *mesh);

/**
 * @brief Draw a single node.
 * 
 * This will draw a single mesh node.
 */
PREVIEW_API void model64_draw_node(model64_t *model, model64_node_t *node);

/**
 * @brief Draw a single primitive.
 */
PREVIEW_API void model64_draw_primitive(primitive_t *primitive);

PREVIEW_API void model64_anim_play(model64_t *model, const char *anim, model64_anim_slot_t slot, bool paused, float start_time);
PREVIEW_API void model64_anim_stop(model64_t *model, model64_anim_slot_t slot);
PREVIEW_API float model64_anim_get_length(model64_t *model, const char *anim);
PREVIEW_API float model64_anim_get_time(model64_t *model, model64_anim_slot_t slot);
PREVIEW_API float model64_anim_set_time(model64_t *model, model64_anim_slot_t slot, float time);
PREVIEW_API float model64_anim_set_speed(model64_t *model, model64_anim_slot_t slot, float speed);
PREVIEW_API bool model64_anim_set_loop(model64_t *model, model64_anim_slot_t slot, bool loop);
PREVIEW_API bool model64_anim_set_pause(model64_t *model, model64_anim_slot_t slot, bool paused);
PREVIEW_API void model64_update(model64_t *model, float deltatime);
#ifdef __cplusplus
}
#endif

/// @cond
// Lock the types as well: PREVIEW_API only covers functions. This must stay at
// the very end of the header, as it makes any later mention of these names an
// error.
PREVIEW_SYM(model64_t)
PREVIEW_SYM(model64_node_t)
PREVIEW_SYM(model64_anim_slot_t)
PREVIEW_SYM(mesh_t)
PREVIEW_SYM(primitive_t)
/// @endcond

#endif
