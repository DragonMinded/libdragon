#ifndef __LIBDRAGON_MODEL64_INTERNAL_H
#define __LIBDRAGON_MODEL64_INTERNAL_H

/** @brief model64 file magic header */
#define MODEL64_MAGIC           0x4D444C48 // "MDLH"
/** @brief model64 loaded model buffer magic */
#define MODEL64_MAGIC_LOADED    0x4D444C4C // "MDLL"
/** @brief model64 owned model buffer magic */
#define MODEL64_MAGIC_OWNED     0x4D444C4F // "MDLO"
/** @brief Current version of model64 */
#define MODEL64_VERSION         1

/** @brief Parameters for a single vertex attribute (part of #primitive_t) */
typedef struct attribute_s {
    uint32_t size;                  ///< Number of components per vertex. If 0, this attribute is not defined
    uint32_t type;                  ///< The data type of each component (for example GL_FLOAT)
    uint32_t stride;                ///< The byte offset between consecutive vertices. If 0, the values are tightly packed
    void *pointer;                  ///< Pointer to the first value
} attribute_t;

/** @brief A single draw call that makes up part of a mesh (part of #mesh_t) */
typedef struct primitive_s {
    uint32_t mode;                  ///< Primitive assembly mode (for example GL_TRIANGLES)
    attribute_t position;           ///< Vertex position attribute, if defined
    attribute_t color;              ///< Vertex color attribyte, if defined
    attribute_t texcoord;           ///< Texture coordinate attribute, if defined
    attribute_t normal;             ///< Vertex normals, if defined
    attribute_t mtx_index;          ///< Matrix indices (aka bones), if defined
    uint32_t vertex_precision;      ///< If the vertex positions use fixed point values, this defines the precision
    uint32_t texcoord_precision;    ///< If the texture coordinates use fixed point values, this defines the precision
    uint32_t index_type;            ///< Data type of indices (for example GL_UNSIGNED_SHORT)
    uint32_t num_vertices;          ///< Number of vertices
    uint32_t num_indices;           ///< Number of indices
    void *indices;                  ///< Pointer to the first index value. If NULL, indices are not used
} primitive_t;

typedef struct node_transform_s {
    float pos[3];
    float rot[4];
    float scale[3];
    float mtx[16];
} node_transform_t;

typedef struct mesh_transform_state_s {
    node_transform_t transform;
    float world_mtx[16];
} node_transform_state_t;

/** @brief A node of the model */
typedef struct mesh_s {
    uint32_t num_primitives;        ///< Number of primitives
    primitive_t *primitives;        ///< Pointer to the first primitive
} mesh_t;

/** @brief A node of the model */
typedef struct model64_joint_s {
    uint32_t node_idx;              ///< Index of relevant node
    float inverse_bind_mtx[16];
} model64_joint_t;

/** @brief A node of the model */
typedef struct model64_skin_s {
    uint32_t num_joints;        ///< Number of joints
    model64_joint_t *joints;    ///< Pointer to the first joint index
} model64_skin_t;

/** @brief A node of the model */
typedef struct model64_node_s {
    char *name;
    mesh_t *mesh;
    model64_skin_t *skin;
    node_transform_t transform;
    uint32_t parent;
    uint32_t num_children;
    uint32_t *children;
} model64_node_t;

/** @brief A model64 file containing a model */
typedef struct model64_data_s {
    uint32_t magic;                 ///< Magic header (MODEL64_MAGIC)
    uint32_t ref_count;
    uint32_t version;               ///< Version of this file
    uint32_t header_size;           ///< Size of the header in bytes
    uint32_t mesh_size;             ///< Size of a mesh header in bytes
    uint32_t primitive_size;        ///< Size of a primitive header in bytes
    uint32_t node_size;             ///< Size of a node in bytes
    uint32_t skin_size;             ///< Size of a skin in bytes
    uint32_t num_nodes;             ///< Number of nodes
    model64_node_t *nodes;          ///< Pointer to the first node
    uint32_t root_node;
    uint32_t num_skins;
    model64_skin_t *skins;
    uint32_t num_meshes;
    mesh_t *meshes;
} model64_data_t;

/** @brief A model64 instance */
typedef struct model64_s {
    model64_data_t *data;
    node_transform_state_t *transforms;
} model64_t;

#endif
