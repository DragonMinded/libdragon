#ifndef SCENE_DATA_H
#define SCENE_DATA_H

#include <fgeom.h>

/*
    A minimalistic "asset database" for this demo.
    The only purpose is to remove some clutter from the main file.
    Using this system for a full game is probably not recommended.
*/

#define TEXTURE_COUNT       4
#define MATERIAL_COUNT      5
#define MESH_COUNT          3
#define OBJECT_COUNT        10
#define LIGHT_COUNT         2
#define MAX_SUBMESH_COUNT   8


/* Textures */
static const char *texture_files[] = {
    "rom:/texture0.ci4.sprite",
    "rom:/texture1.ci4.sprite",
    "rom:/texture2.ci4.sprite",
    "rom:/env_gold.rgba16.sprite",
};


/* Materials */
static const uint32_t material_texture_indices[] = {
    0, 0, 1, 2, 3
};
static const uint32_t material_diffuse_colors[] = {
    0xffffffff,
    0x5a81e6ff,
    0x3b5c34ff,
    0xffffffff,
    0xffffffff,
};
static const mgfx_features_t material_features[] = {
    0,
    0,
    0,
    0,
    MGFX_FEATURE_ENV_MAP
};


/* Meshes */
static const char *mesh_files[] = {
    "rom:/pipe.mgfx",    // 256 Verts, 512 Tris
    "rom:/crate.mgfx",   // 56 Verts, 108 Tris
    "rom:/sphere.mgfx",  // 42 Verts, 80 Tris
};


/* Objects */
static const uint32_t object_mesh_ids[] = {
    2, 1, 1, 0, 0, 1, 1, 0, 1, 1
};
static const uint32_t object_material_ids[][MAX_SUBMESH_COUNT] = {
    { 4 }, 
    { 1 }, 
    { 2 }, 
    { 4, 0 }, 
    { 4, 2 }, 
    { 3 }, 
    { 2 }, 
    { 4, 2 }, 
    { 0 }, 
    { 4 }
};
static const fm_vec3_t object_positions[] = {
    {{ 0.0f, 0.0f, 0.0f }},
    {{ -10.0f, 0.0f, 0.0f }},
    {{ 10.0f, 0.0f, 0.0f }},
    {{ 0.0f, 10.0f, -10.0f }},
    {{ 10.0f, 10.0f, 0.0f }},
    {{ 5.0f, -5.0f, 9.0f }},
    {{ -15.0f, -8.0f, 0.0f }},
    {{ 4.0f, 3.0f, -12.0f }},
    {{ -14.0f, 6.0f, 8.0f }},
    {{ -4.0f, 16.0f, 0.0f }},
};


/* Lights */
static const uint32_t light_colors[] = {
    0x868686ff,
    0xdbbc72ff
};
static const fm_vec4_t light_positions[] = {
    {{ 0.196116f, -0.784465f, -0.588348f, 0.0f }},
    {{ 0.0f, 10.0f, 0.0f, 1.0f }}
};
static const float light_radii[] = {
    0.0f,
    50.0f
};
static const uint32_t ambient_light_color = 0x101010ff;

/* Environment */
static const uint32_t fog_color = 0x000000ff;
static const float fog_start = 25.0f;
static const float fog_end = 100.0f;

/* Camera */
static const float camera_fov = 65.0f;
static const float camera_near_plane = 1.0f;
static const float camera_far_plane = 100.0f;
static const fm_vec3_t camera_start_position = {{0,0,30}};

#endif
