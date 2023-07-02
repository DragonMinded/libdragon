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

/** @brief Parameters for a single vertex attribute (part of #primitive_s) */
typedef struct attribute_s {
    uint32_t size;                  ///< Number of components per vertex. If 0, this attribute is not defined
    uint32_t type;                  ///< The data type of each component (for example #GL_FLOAT)
    uint32_t stride;                ///< The byte offset between consecutive vertices. If 0, the values are tightly packed
    void *pointer;                  ///< Pointer to the first value
} attribute_t;

/** @brief A single draw call that makes up part of a mesh (part of #mesh_t) */
typedef struct primitive_s {
    uint32_t mode;                  ///< Primitive assembly mode (for example #GL_TRIANGLES)
    attribute_t position;           ///< Vertex position attribute, if defined
    attribute_t color;              ///< Vertex color attribyte, if defined
    attribute_t texcoord;           ///< Texture coordinate attribute, if defined
    attribute_t normal;             ///< Vertex normals, if defined
    attribute_t mtx_index;          ///< Matrix indices (aka bones), if defined
    uint32_t vertex_precision;      ///< If the vertex positions use fixed point values, this defines the precision
    uint32_t texcoord_precision;    ///< If the texture coordinates use fixed point values, this defines the precision
    uint32_t index_type;            ///< Data type of indices (for example #GL_UNSIGNED_SHORT)
    uint32_t num_vertices;          ///< Number of vertices
    uint32_t num_indices;           ///< Number of indices
    void *indices;                  ///< Pointer to the first index value. If NULL, indices are not used
} primitive_t;

/** @brief A mesh that is made up of multiple primitives (part of #model64_t) */
typedef struct mesh_s {
    uint32_t num_primitives;        ///< Number of primitives
    primitive_t *primitives;        ///< Pointer to the first primitive
} mesh_t;

/** @brief A model64 file containing a model */
typedef struct model64_s {
    uint32_t magic;                 ///< Magic header (#MODEL64_MAGIC)
    uint32_t version;               ///< Version of this file
    uint32_t header_size;           ///< Size of the header in bytes
    uint32_t mesh_size;             ///< Size of a mesh header in bytes
    uint32_t primitive_size;        ///< Size of a primitive header in bytes
    uint32_t num_meshes;            ///< Number of meshes
    mesh_t *meshes;                 ///< Pointer to the first mesh
} model64_t;

#endif
