#ifndef __MGFX_MODEL_INTERNAL_H
#define __MGFX_MODEL_INTERNAL_H

#include "mgfx_model.h"
#include "rspq.h"
#include "hashtable_internal.h"
#include "mgfx.h"

#define MGFX_MODEL_VERSION  1
#define MGFX_MODEL_ID_LEN   3
#define MGFX_MODEL_ID       "FXM"

#define MGFX_ANIM_COMPONENT_POS     0
#define MGFX_ANIM_COMPONENT_ROT     1
#define MGFX_ANIM_COMPONENT_SCALE   2

#define MGFX_NODE_FLAG_MTX_DIRTY        (1<<0)

typedef uint32_t pipeline_key;

typedef struct mgfx_transform_s {
    fm_vec3_t position;
    fm_quat_t rotation;
    fm_vec3_t scale;
    fm_mat4_t matrix;
} mgfx_transform_t;

typedef struct mgfx_node_data_s {
    char *name;
    int32_t parent_index;
    uint32_t children_count;
    uint32_t *children_indices;
    int32_t mesh_index;
    int32_t skin_index;
    mgfx_transform_t transform;
} mgfx_node_data_t;

typedef struct mgfx_mesh_state_s {
    rspq_block_t **submesh_blocks;
} mgfx_mesh_state_t;

typedef struct mgfx_material_state_s {
    mg_pipeline_t *pipeline;
    rdpq_mat_t *rdpq_mat;
} mgfx_material_state_t;

typedef struct mgfx_model_state_s {
    mgfx_mesh_state_t *meshes;
    mgfx_material_state_t *materials;
} mgfx_model_state_t;

typedef struct mgfx_model_s {
    uint8_t id[MGFX_MODEL_ID_LEN];
    uint8_t version;
    uint32_t meshes_count;
    mgfx_mesh_t *meshes;
    uint32_t nodes_count;
    mgfx_node_data_t *nodes;
    uint32_t root_node_index;
    uint32_t max_node_depth;
    uint32_t materials_count;
    char **materials;
    rdpq_matdb_t *matdb;
    uint32_t animations_count;
    mgfx_animation_t *animations;
    uint32_t skins_count;
    mgfx_skin_t *skins;
    uint32_t animation_stream;
    mgfx_model_state_t *state;
} mgfx_model_t;

typedef struct mgfx_skin_s {
    uint32_t joints_count;
    uint32_t joint_indices;
} mgfx_skin_t;

/** @brief A keyframe of an animation */
typedef struct mgfx_keyframe_s {
    float time;         ///< Time of keyframe
    float time_req;     ///< Time keyframe was requested
    uint16_t track;     ///< Track keyframe applies to
    uint16_t data[3];   ///< Data for keyframe
} mgfx_keyframe_t;

typedef uint16_t mgfx_track_t;

typedef struct mgfx_animation_s {
    char *name;                     ///< Name of the animation
    float pos_f1;                   ///< Scale of position components of animation
    float pos_f2;                   ///< Minimum position of animation
    float scale_f1;                 ///< Scale of scale components of animation
    float scale_f2;                 ///< Minimum scale of animation
    float duration;                 ///< Duration of animation
    uint32_t tracks_count;          ///< Number of tracks targeted by animation
    mgfx_track_t *tracks;           ///< Pointer to animation tracks
    uint32_t keyframes_count;       ///< Number of keyframes in animation
    mgfx_keyframe_t *keyframes;     ///< Pointer to animation keyframes, or NULL if animations are streamed
} mgfx_animation_t;

/** @brief Decoded data for a keyframe */
typedef struct decoded_keyframe_s {
    float time;     ///< Time of keyframe
    float data[4];  ///< Data for keyframe
} decoded_keyframe_t;

typedef struct mgfx_animation_instance_s {
    int32_t index;                      ///< Index of animation playing
    float time;                         ///< Current time of animation
    bool invalid_pose;                  ///< Whether this animation needs to recalculate a pose
    bool loop;                          ///< Whether this animation loops
    bool paused;                        ///< Whether this animation is active
    bool prev_waiting_frame;            ///< Whether there is a previous waiting frame
    bool done_decoding;                 ///< Whether there are more keyframes to read
    float speed;                        ///< The speed of an animation
    uint32_t frame_idx;                 ///< Index of next keyframe to read
    decoded_keyframe_t *frames;         ///< Buffer for decoded keyframes
    mgfx_keyframe_t *curr_frame;        ///< Buffer for keyframe waiting to be copied
} mgfx_animation_instance_t;

typedef struct mgfx_node_s {
    mgfx_node_data_t *data;
    mgfx_instance_t *instance;
    mgfx_transform_t transform;
    uint32_t flags;
    mgfx_matrices_t *matrices;
} mgfx_node_t;

typedef struct mgfx_instance_s {
    mgfx_model_t *model;
    mgfx_node_t *nodes;
    uint32_t buffer_count;
} mgfx_instance_t;

// TODO: better name
typedef struct mgfx_registry_s {
    hashtable_t pipelines;
} mgfx_registry_t;

#endif
