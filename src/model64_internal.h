#ifndef __LIBDRAGON_MODEL64_INTERNAL_H
#define __LIBDRAGON_MODEL64_INTERNAL_H

#define MODEL64_MAGIC           0x4D444C48 // "MDLH"
#define MODEL64_MAGIC_LOADED    0x4D444C4C // "MDLL"
#define MODEL64_MAGIC_OWNED     0x4D444C4F // "MDLO"
#define MODEL64_VERSION         1

typedef struct attribute_s {
    uint32_t size;
    uint32_t type;
    uint32_t stride;
    void *pointer;
} attribute_t;

typedef struct primitive_s {
    uint32_t mode;
    attribute_t position;
    attribute_t color;
    attribute_t texcoord;
    attribute_t normal;
    attribute_t mtx_index;
    uint32_t vertex_precision;
    uint32_t texcoord_precision;
    uint32_t index_type;
    uint32_t num_vertices;
    uint32_t num_indices;
    void *indices;
} primitive_t;

typedef struct mesh_s {
    uint32_t num_primitives;
    primitive_t *primitives;
} mesh_t;

typedef struct model64_s {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t mesh_size;
    uint32_t primitive_size;
    uint32_t num_meshes;
    mesh_t *meshes;
} model64_t;

#endif
