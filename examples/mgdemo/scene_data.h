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
    "rom:/pipe.mshdb",    // 256 Verts, 512 Tris
    "rom:/crate.mshdb",   // 56 Verts, 108 Tris
    "rom:/sphere.mshdb",  // 42 Verts, 80 Tris
};

static const char *mesh_names[] = {
    "Cylinder",
    "Crate",
    "Sphere"
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
    {{ -640.0f, 0.0f, 0.0f }},
    {{ 640.0f, 0.0f, 0.0f }},
    {{ 0.0f, 640.0f, -640.0f }},
    {{ 640.0f, 640.0f, 0.0f }},
    {{ 320.0f, -320.0f, 576.0f }},
    {{ -960.0f, -512.0f, 0.0f }},
    {{ 256.0f, 192.0f, -768.0f }},
    {{ -896.0f, 384.0f, 512.0f }},
    {{ -256.0f, 1024.0f, 0.0f }},
};


/* Lights */
static const uint32_t light_colors[] = {
    0x868686ff,
    0xdbbc72ff
};
static const fm_vec4_t light_positions[] = {
    {{ 0.196116f, -0.784465f, -0.588348f, 0.0f }},
    {{ 0.0f, 640.0f, 0.0f, 1.0f }}
};
static const float light_intensities[] = {
    0.0f,
    30.0f
};
static const uint32_t ambient_light_color = 0x101010ff;

/* Environment */
static const uint32_t fog_color = 0x000000ff;
static const float fog_start = 1600.0f;
static const float fog_end = 6400.0f;

/* Camera */
static const float camera_fov = 65.0f;
static const float camera_near_plane = 64.0f;
static const float camera_far_plane = 6400.0f;
static const fm_vec3_t camera_start_position = {{0,0,1800}};

#endif
