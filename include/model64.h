#ifndef __LIBDRAGON_MODEL64_H
#define __LIBDRAGON_MODEL64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct model64_s;
typedef struct model64_s model64_t;

struct mesh_s;
typedef struct mesh_s mesh_t;

struct primitive_s;
typedef struct primitive_s primitive_t;

model64_t *model64_load(const char *fn);
model64_t *model64_load_buf(void *buf, int sz);
void model64_free(model64_t *model);

/**
 * @brief Return the number of meshes in this model.
 */
uint32_t model64_get_mesh_count(model64_t *model);

/**
 * @brief Return the mesh at the specified index.
 */
mesh_t *model64_get_mesh(model64_t *model, uint32_t mesh_index);

/**
 * @brief Return the number of primitives in this mesh.
 */
uint32_t model64_get_primitive_count(mesh_t *mesh);

/**
 * @brief Return the primitive at the specified index.
 */
primitive_t *model64_get_primitive(mesh_t *mesh, uint32_t primitive_index);

/**
 * @brief Draw an entire model.
 * 
 * This will draw all primitives of all meshes that are contained the given model.
 */
void model64_draw(model64_t *model);

/**
 * @brief Draw a single mesh.
 * 
 * This will draw all of the given mesh's primitives.
 */
void model64_draw_mesh(mesh_t *mesh);

/**
 * @brief Draw a single primitive.
 */
void model64_draw_primitive(primitive_t *primitive);

#ifdef __cplusplus
}
#endif

#endif
