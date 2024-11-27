#include <libdragon.h>
#include <rspq_profile.h>

#include "scene_data.h"
#include "debug_overlay.h"

#define FB_COUNT    3

#define MAX_PIPELINE_COUNT          (1<<3)
#define MAX_VERTEX_ATTRIBUTE_COUNT  3

#define ENABLE_RDPQ_DEBUG 0
#define SINGLE_FRAME      0

#define VTX_TEX_SHIFT   8
#define RDP_TEX_SHIFT   5
#define TEX_SIZE_SHIFT  (VTX_TEX_SHIFT-RDP_TEX_SHIFT)
#define RDP_HALF_TEXEL  (1<<(RDP_TEX_SHIFT-1))

#define MAX_DRAW_CALL_COUNT (OBJECT_COUNT * MAX_SUBMESH_COUNT)

#define STICK_DEADZONE       10
#define IGNORE_DEADZONE(v)   ((v) > STICK_DEADZONE || (v) < -STICK_DEADZONE ? (v) : 0)

#define CAMERA_YAW_SPEED            0.015f
#define CAMERA_PITCH_SPEED          0.015f
#define CAMERA_DISTANCE_SPEED       0.05f
#define CAMERA_DISTANCE_SPEED_FAST  0.5f
#define CAMERA_MIN_PITCH            (-M_PI_2 * 0.9)
#define CAMERA_MAX_PITCH            (M_PI_2 * 0.9)

#define ARRAY_COUNT(a) (sizeof(a)/sizeof(a[0]))

#define RAND_FLT() ((float)rand()/RAND_MAX)

typedef struct
{
    mgfx_fog_t fog;
    mgfx_lighting_t lighting;
} scene_raw_data;

typedef struct
{
    mgfx_texturing_t texturing;
    mgfx_modes_t modes;
} material_raw_data;

typedef struct
{
    mg_vertex_attribute_t attributes[MAX_VERTEX_ATTRIBUTE_COUNT];
    mg_vertex_layout_t vertex_layout;
} vertex_layout;

typedef struct
{
    material_raw_data *buffer;
    sprite_t *texture;
    rdpq_texparms_t rdpq_tex_parms;
    mgfx_modes_t modes;
    mg_geometry_flags_t geometry_flags;
    color_t color;
} material_data;

typedef struct
{
    const void *vertex_buffer;
    const uint16_t *indices;
    uint32_t index_count;
    uint32_t pipeline_id;
    rspq_block_t *block;
} submesh_data;

typedef struct
{
    model64_t *model;
    submesh_data *submeshes;
    uint32_t submesh_count;
} mesh_data;

typedef struct
{
    fm_mat4_t mvp_matrix;
    fm_mat4_t mv_matrix;
    fm_mat4_t n_matrix;
    fm_quat_t rotation;
    fm_vec3_t position;
    fm_vec3_t rotation_axis;
    float rotation_angle;
    float rotation_rate;
    uint32_t mesh_id;
    uint32_t material_ids[MAX_SUBMESH_COUNT];
} object_data;

typedef struct
{
    uint32_t pipeline_id;
    uint32_t material_id;
    uint32_t mesh_id;
    uint32_t object_id;
} draw_call;

void init();
void update(float delta_time);
void render();
void material_create(material_data *mat, sprite_t *texture, mgfx_modes_parms_t *mode_parms, mg_geometry_flags_t geometry_flags, color_t color);
void mesh_create(mesh_data *mesh, const char *model_file, vertex_layout *vertex_layout_cache);
void update_object_transform(object_data *object);

static surface_t zbuffer;

static mg_viewport_t viewport;
static mg_culling_parms_t culling;
static scene_raw_data *scene_uniform_buffer[FB_COUNT];
static const mg_uniform_t *fog_uniform;
static const mg_uniform_t *lighting_uniform;
static const mg_uniform_t *texturing_uniform;
static const mg_uniform_t *modes_uniform;
static const mg_uniform_t *matrices_uniform;

static uint32_t pipelines_count = 0;
static mg_pipeline_t *pipelines[MAX_PIPELINE_COUNT];
static sprite_t *textures[TEXTURE_COUNT];
static material_data materials[MATERIAL_COUNT];
static mesh_data meshes[MESH_COUNT];
static object_data objects[OBJECT_COUNT];
static mgfx_light_parms_t lights[LIGHT_COUNT];

static draw_call *draw_calls;
static uint32_t draw_call_count;
static bool draw_calls_dirty = true;

static fm_mat4_t projection_matrix;
static fm_mat4_t view_matrix;
static fm_mat4_t vp_matrix;

static fm_vec3_t camera_position;
static float camera_yaw;
static float camera_pitch;

static uint32_t current_object_count = OBJECT_COUNT;
static bool animation_enabled = false;
static uint32_t fb_index = 0;

static uint64_t frames = 0;
static bool display_metrics = false;
static bool request_display_metrics = false;
static float last_3d_fps = 0.0f;
static rspq_profile_data_t profile_data;

int main()
{
    init();

#if ENABLE_RDPQ_DEBUG
    rdpq_debug_start();
    rdpq_debug_log(true);
#endif

    rspq_profile_start();

    while (true) 
    {
        update(display_get_delta_time());
        render();

#if SINGLE_FRAME
        rspq_wait();
        break;
#endif
    }
}

void init()
{
    const resolution_t resolution = RESOLUTION_320x240;

    // Initialize the required subsystems
	debug_init(DEBUG_FEATURE_LOG_ISVIEWER | DEBUG_FEATURE_LOG_USB);
    dfs_init(DFS_DEFAULT_LOCATION);
    joypad_init();
    display_init(resolution, DEPTH_16_BPP, FB_COUNT, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    rdpq_init();
    mg_init();

    debug_overlay_init();

    // Create depth buffer
    zbuffer = surface_alloc(FMT_RGBA16, resolution.width, resolution.height);

    // Initialize viewport
    viewport = (mg_viewport_t) {
        .x = 0,
        .y = 0,
        .width = resolution.width,
        .height = resolution.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
        .z_near = camera_near_plane,
        .z_far = camera_far_plane
    };
    
    // Initialize culling mode
    culling = (mg_culling_parms_t) {
        .cull_mode = MG_CULL_MODE_BACK
    };

    // Initialize lighting parameters
    for (size_t i = 0; i < LIGHT_COUNT; i++)
    {
        lights[i].color = color_from_packed32(light_colors[i]);
        lights[i].radius = light_radii[i];
    }

    // Create meshes and initialize pipelines
    vertex_layout vertex_layout_cache[MAX_PIPELINE_COUNT];
    for (size_t i = 0; i < MESH_COUNT; i++)
    {
        mesh_create(&meshes[i], mesh_files[i], vertex_layout_cache);
    }

    assertf(pipelines_count > 0, "No pipelines were created during scene initialization!");

    fog_uniform = mg_pipeline_get_uniform(pipelines[0], MGFX_BINDING_FOG);
    lighting_uniform = mg_pipeline_get_uniform(pipelines[0], MGFX_BINDING_LIGHTING);
    texturing_uniform = mg_pipeline_get_uniform(pipelines[0], MGFX_BINDING_TEXTURING);
    modes_uniform = mg_pipeline_get_uniform(pipelines[0], MGFX_BINDING_MODES);
    matrices_uniform = mg_pipeline_get_uniform(pipelines[0], MGFX_BINDING_MATRICES);

    // Load textures
    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        textures[i] = sprite_load(texture_files[i]);
    }

    // Create materials
    for (size_t i = 0; i < MATERIAL_COUNT; i++)
    {
        material_create(
            &materials[i], 
            textures[material_texture_indices[i]], 
            &(mgfx_modes_parms_t) {
                .flags = MGFX_MODES_FLAGS_FOG_ENABLED | material_flags[i]
            },
            MG_GEOMETRY_FLAGS_Z_ENABLED | MG_GEOMETRY_FLAGS_TEX_ENABLED | MG_GEOMETRY_FLAGS_SHADE_ENABLED,
            color_from_packed32(material_diffuse_colors[i]));
    }

    // These are uniforms that are expected to stay constant during the entire scene.
    // They will be provided to the shader by writing the data to a uniform buffer.

    // Because this data changes each frame we need to create one buffer for each frame that can be in flight simultaneosly,
    // which is normally the number of framebuffers the display was initialized with.
    // This is necessary so when frame N is being prepared on the CPU we won't overwrite the data for frame N-1, which might still be
    // in the process of being rendered by RSP/RDP.

    for (size_t i = 0; i < FB_COUNT; i++)
    {
        // Create the uniform buffer. It's important that the buffer contents need to be in a format that is optimized for
        // access by the RSP. This is what the scene_raw_data struct is, which itself contains the predefined structs
        // provided by the fixed function pipeline.
        scene_uniform_buffer[i] = malloc_uncached(sizeof(scene_raw_data));
    }

    // Note that the contents of the scene uniform buffers are not initialized yet. This will happen later on.

    // Initialize objects
    for (size_t i = 0; i < OBJECT_COUNT; i++)
    {
        objects[i] = (object_data) {
            .mesh_id = object_mesh_ids[i],
        };
        memcpy(objects[i].material_ids, object_material_ids[i], sizeof(objects[i].material_ids));
        objects[i].position = object_positions[i];
        fm_quat_identity(&objects[i].rotation);

        // Create a random rotation axis (just an approximation, not actually uniformly distributed)
        objects[i].rotation_axis.x = RAND_FLT() * 2.0f - 1.0f;
        objects[i].rotation_axis.y = RAND_FLT() * 2.0f - 1.0f;
        objects[i].rotation_axis.z = RAND_FLT() * 2.0f - 1.0f;
        fm_vec3_norm(&objects[i].rotation_axis, &objects[i].rotation_axis);

        objects[i].rotation_rate = RAND_FLT() * 5.0f;
        objects[i].rotation_angle = RAND_FLT() * M_TWOPI;
        update_object_transform(&objects[i]);
    }

    // Initialize draw calls.
    draw_calls = calloc(MAX_DRAW_CALL_COUNT, sizeof(draw_call));

    // Initialize camera properties
    float aspect_ratio = (float)resolution.width / (float)resolution.height;
    mg_mat4_perspective(&projection_matrix, FM_DEG2RAD(camera_fov), aspect_ratio, camera_near_plane, camera_far_plane);
    camera_position = camera_start_position;
}

void material_create(material_data *material, sprite_t *texture, mgfx_modes_parms_t *mode_parms, mg_geometry_flags_t geometry_flags, color_t color)
{
    // Similarly to the scene uniforms, we will provide materials to the shader via uniform buffers.
    // We separate the material from scene uniforms, because they are expected to change during the scene.
    // Not all objects will have the same material after all. We will make the assumption that the materials 
    // themselves will stay constant. Therefore we only create a single buffer per material, as opposed to 
    // additional buffers for simultaneous frames.

    // Initialize the raw material data by using the functions provided by the fixed function pipeline.

    // Flip the texture upside down if environment mapping is enabled, because it will appear upside down otherwise.
    int tex_y_scale = (mode_parms->flags & MGFX_MODES_FLAGS_ENV_MAP_ENABLED) ? -1 : 1;
    material->buffer = malloc_uncached(sizeof(material_raw_data));
    mgfx_get_texturing(&material->buffer->texturing, &(mgfx_texturing_parms_t) {
        .scale[0] = texture->width  >> TEX_SIZE_SHIFT,
        .scale[1] = (texture->height * tex_y_scale) >> TEX_SIZE_SHIFT,
        .offset[0] = -RDP_HALF_TEXEL,
        .offset[1] = -RDP_HALF_TEXEL
    });
    mgfx_get_modes(&material->buffer->modes, mode_parms);

    // Additionally prepare texture data for rdpq, but this is completely unrelated to magma.
    material->texture = texture;
    material->rdpq_tex_parms = (rdpq_texparms_t) {
        .s.repeats = REPEAT_INFINITE,
        .t.repeats = REPEAT_INFINITE,
    };

    material->geometry_flags = geometry_flags;
    material->color = color;
}

void get_vertex_layout_from_primitive_layout(const model64_vertex_layout_t *primitive_layout, vertex_layout *vertex_layout)
{
    uint32_t attribute_count = 0;

    for (size_t i = 0; i < primitive_layout->attribute_count; i++)
    {
        const model64_vertex_attr_t *prim_attribute = &primitive_layout->attributes[i];

        switch (prim_attribute->attribute)
        {
        case MODEL64_ATTR_POSITION:
            assertf(prim_attribute->component_count == 3, "Postition must consist of 3 components!");
            assertf(prim_attribute->type == MODEL64_ATTR_TYPE_FX16, "Postition must be in fixed point format!");

            vertex_layout->attributes[attribute_count++] = (mg_vertex_attribute_t) {
                .input = MGFX_ATTRIBUTE_POS_NORM,
                .offset = prim_attribute->offset
            };
            break;

        case MODEL64_ATTR_NORMAL:
            assertf(prim_attribute->component_count == 3, "Normal must consist of 3 components!");
            assertf(prim_attribute->type == MODEL64_ATTR_TYPE_PACKED_NORMAL_16, "Normal must be in packed format!");
            break;

        case MODEL64_ATTR_COLOR:
            assertf(prim_attribute->component_count == 4, "Color must consist of 4 components!");
            assertf(prim_attribute->type == MODEL64_ATTR_TYPE_U8, "Color must be in u8 format!");

            vertex_layout->attributes[attribute_count++] = (mg_vertex_attribute_t) {
                .input = MGFX_ATTRIBUTE_COLOR,
                .offset = prim_attribute->offset
            };
            break;

        case MODEL64_ATTR_TEXCOORD:
            assertf(prim_attribute->component_count == 2, "Texcoord must consist of 2 components!");
            assertf(prim_attribute->type == MODEL64_ATTR_TYPE_FX16, "Texcoord must be in fixed point format!");

            vertex_layout->attributes[attribute_count++] = (mg_vertex_attribute_t) {
                .input = MGFX_ATTRIBUTE_TEXCOORD,
                .offset = prim_attribute->offset
            };
            break;

        default:
            break;
        }
    }

    vertex_layout->vertex_layout = (mg_vertex_layout_t) {
        .attribute_count = attribute_count,
        .attributes = vertex_layout->attributes,
        .stride = primitive_layout->stride
    };
}

bool are_vertex_layouts_equal(const mg_vertex_layout_t *p0, const mg_vertex_layout_t *p1)
{
    if (p0->stride != p1->stride) return false;
    if (p0->attribute_count != p1->attribute_count) return false;

    for (size_t i = 0; i < p0->attribute_count; i++)
    {
        const mg_vertex_attribute_t *a0 = &p0->attributes[i];
        const mg_vertex_attribute_t *a1 = &p1->attributes[i];

        // TODO: handle differently ordered attributes
        if (a0->input != a1->input) return false;
        if (a0->offset != a1->offset) return false;
    }
    
    return true;
}

uint32_t get_or_create_pipeline_from_primitive_layout(const model64_vertex_layout_t *primitive_layout, vertex_layout *vertex_layout_cache)
{
    // Convert the primitive layout to magma vertex layout
    static vertex_layout tmp_vertex_layout;
    get_vertex_layout_from_primitive_layout(primitive_layout, &tmp_vertex_layout);

    // Try to find a pipeline with the same vertex layout
    for (uint32_t i = 0; i < pipelines_count; i++)
    {
        if (are_vertex_layouts_equal(&tmp_vertex_layout.vertex_layout, &vertex_layout_cache[i].vertex_layout)) {
            return i;
        }
    }

    // If none was found, create a new pipeline with the vertex layout.
    // Internally, magma will patch the shader ucode to be compatible with the configured vertex layout,
    // which is why a separate pipeline needs to be created for each layout.
    pipelines[pipelines_count] = mg_pipeline_create(&(mg_pipeline_parms_t) {
        .vertex_shader_ucode = mgfx_get_shader_ucode(),
        .vertex_layout = tmp_vertex_layout.vertex_layout
    });

    // Store the vertex layout in the cache
    vertex_layout_cache[pipelines_count] = tmp_vertex_layout;

    return pipelines_count++;
}

void mesh_create(mesh_data *mesh, const char *model_file, vertex_layout *vertex_layout_cache)
{
    model64_t *model = model64_load(model_file);

    model64_vtx_fmt_t vertex_format = model64_get_vertex_format(model);
    assertf(vertex_format == MODEL64_VTX_FMT_MGFX, "The model %s has an unsupported vertex format!", model_file);

    uint32_t mesh_count = model64_get_mesh_count(model);
    assertf(mesh_count == 1, "The model %s contains more than one mesh!", model_file);
    mesh_t *in_mesh = model64_get_mesh(model, 0);

    mesh->model = model;
    mesh->submesh_count = model64_get_primitive_count(in_mesh);
    mesh->submeshes = calloc(mesh->submesh_count, sizeof(submesh_data));

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        submesh_data *submesh = &mesh->submeshes[i];
        primitive_t *primitive = model64_get_primitive(in_mesh, i);

        // Some meshes might have different vertex layouts. To account for this, we need to create a separate pipeline for each distinct layout.

        model64_vertex_layout_t primitive_layout;
        model64_get_primitive_vertex_layout(primitive, &primitive_layout);
        submesh->pipeline_id = get_or_create_pipeline_from_primitive_layout(&primitive_layout, vertex_layout_cache);

        // Preparing mesh data is straightforward. Just get pointers to the vertex and index buffers.

        submesh->vertex_buffer = model64_get_primitive_vertices(primitive);

        submesh->indices = model64_get_primitive_indices(primitive);
        submesh->index_count = model64_get_primitive_index_count(primitive);

        // To increase performance, we can record the drawing command into a block, since the topology of the mesh doesn't change in this case.
        // Note that we could still modify the vertices themselves if we wanted, by writing to the vertex buffer. This would require some manual
        // synchronisation, however.
        rspq_block_begin();
            // This function internally just reads the list of indices and emits a sequence of calls to mg_load_vertices and mg_draw_indices.
            // Those function can also be called manually, if more customisation of the mesh layout is desired.
            mg_draw_indexed(&(mg_input_assembly_parms_t) {
                .primitive_topology = MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
            }, submesh->indices, submesh->index_count, 0);
        submesh->block = rspq_block_end();
    }
}

void update_object_transform(object_data *object)
{
    fm_quat_from_axis_angle(&object->rotation, &object->rotation_axis, objects->rotation_angle);
}

void direction_from_pitch_yaw(fm_vec3_t *out, float pitch, float yaw)
{
    float ys, yc, ps, pc;
    fm_sincosf(yaw, &ys, &yc);
    fm_sincosf(pitch, &ps, &pc);
    *out = (fm_vec3_t){{
        -pc * ys,
        ps,
        -pc * yc
    }};
}

void update(float delta_time)
{
    joypad_poll();
    joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);
    joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    int8_t stick_x = IGNORE_DEADZONE(inputs.stick_x);
    int8_t stick_y = IGNORE_DEADZONE(inputs.stick_y);
    int8_t cstick_x = IGNORE_DEADZONE(inputs.cstick_x);
    int8_t cstick_y = IGNORE_DEADZONE(inputs.cstick_y);

    camera_yaw -= cstick_x * delta_time * CAMERA_YAW_SPEED;
    camera_pitch -= cstick_y * delta_time * CAMERA_PITCH_SPEED;

    if (camera_yaw > M_TWOPI) camera_yaw -= M_TWOPI;
    if (camera_yaw < 0.0f) camera_yaw += M_TWOPI;

    if (camera_pitch > CAMERA_MAX_PITCH) camera_pitch = CAMERA_MAX_PITCH;
    if (camera_pitch < CAMERA_MIN_PITCH) camera_pitch = CAMERA_MIN_PITCH;

    float distance_speed = delta_time * (inputs.btn.z ? CAMERA_DISTANCE_SPEED : CAMERA_DISTANCE_SPEED_FAST);

    fm_vec3_t up = {{0, 1, 0}};
    fm_vec3_t right;
    fm_vec3_t forward;
    direction_from_pitch_yaw(&forward, camera_pitch, camera_yaw);
    fm_vec3_cross(&right, &forward, &up);
    fm_vec3_scale(&forward, &forward, stick_y);
    fm_vec3_scale(&right, &right, stick_x);
    fm_vec3_add(&forward, &forward, &right);
    fm_vec3_scale(&forward, &forward, distance_speed);
    fm_vec3_add(&camera_position, &camera_position, &forward);

    // Increase/Decrease the number of drawn objects with the D-pad.
    if (btn.d_up && current_object_count < OBJECT_COUNT) {
        current_object_count++;
        draw_calls_dirty = true;
    }
    if (btn.d_down && current_object_count > 0) {
        current_object_count--;
        draw_calls_dirty = true;
    }

    // Start toggles the animation
    if (btn.start) {
        animation_enabled = !animation_enabled;
    }

    // L toggles the debug/profiler overlay on/off
    if (btn.l) {
        request_display_metrics = !request_display_metrics;
        if (!request_display_metrics) display_metrics = false;
    }

    if (animation_enabled) {
        // Compute animation based on delta time. It's enough for this demo.
        for (size_t i = 0; i < current_object_count; i++)
        {
            objects[i].rotation_angle = fm_wrap_angle(objects[i].rotation_angle + objects[i].rotation_rate * delta_time);
            update_object_transform(&objects[i]);
        }
    }
}

void update_camera()
{
    // Update camera matrices.
    fm_vec3_t forward;
    fm_vec3_t up = {{0, 1, 0}};
    direction_from_pitch_yaw(&forward, camera_pitch, camera_yaw);
    fm_mat4_look(&view_matrix, &camera_position, &forward, &up);
    fm_mat4_mul(&vp_matrix, &projection_matrix, &view_matrix);
}

void update_lights()
{
    // Here we are updating the contents of the scene uniforms that we created during initialisation above.

    // Because lighting is computed in eye space and we specify light positions/directions in world space,
    // we need to manually transform the lights into eye space each frame and update the corresponding uniform.
    for (size_t i = 0; i < LIGHT_COUNT; i++)
    {
        // Transform light position into eye space
        fm_mat4_mul_vec4(&lights[i].position, &view_matrix, &light_positions[i]);
    }

    // These mgfx_get_* functions will take the parameters in a convenient format and convert them into the RSP-optimized format that the buffer is supposed to contain.
    mgfx_get_fog(&scene_uniform_buffer[fb_index]->fog, &(mgfx_fog_parms_t) {
        .start = fog_start,
        .end = fog_end
    });
    mgfx_get_lighting(&scene_uniform_buffer[fb_index]->lighting, &(mgfx_lighting_parms_t) {
        .ambient_color = color_from_packed32(ambient_light_color),
        .light_count = ARRAY_COUNT(lights),
        .lights = lights
    });
}

void update_objects()
{
    // Recalculate object matrices.
    for (size_t i = 0; i < current_object_count; i++)
    {
        object_data *object = &objects[i];
        // Update object matrices from its current transform.

        fm_mat4_t model_matrix;
        fm_mat4_from_rt(&model_matrix, &object->rotation, &object->position);
        fm_mat4_mul(&object->mvp_matrix, &vp_matrix, &model_matrix);
        fm_mat4_mul(&object->mv_matrix, &view_matrix, &model_matrix);
        fm_mat4_affine_to_normal_mat(&object->n_matrix, &object->mv_matrix);
    }
}

int compare_draw_call(const draw_call *a, const draw_call *b)
{
    if (a->pipeline_id < b->pipeline_id) return -1;
    if (a->pipeline_id > b->pipeline_id) return 1;
    if (a->material_id < b->material_id) return -1;
    if (a->material_id > b->material_id) return 1;
    if (a->mesh_id < b->mesh_id) return -1;
    if (a->mesh_id > b->mesh_id) return 1;
    return 0;
}

void update_draw_calls()
{
    if (!draw_calls_dirty) return;

    // Collect draw calls from all objects.
    // For each objects, we generate one draw call per submesh.
    draw_call_count = 0;
    for (size_t i = 0; i < current_object_count; i++)
    {
        object_data *object = &objects[i];
        mesh_data *mesh = &meshes[object->mesh_id];

        for (size_t j = 0; j < mesh->submesh_count; j++)
        {
            draw_calls[draw_call_count++] = (draw_call) {
                .pipeline_id = mesh->submeshes[j].pipeline_id,
                .material_id = object->material_ids[j],
                // Pack both mesh id and submesh id into a 32 bit value for faster comparison
                .mesh_id = (object->mesh_id << 16) | (j & 0xFFFF),
                .object_id = i
            };
        }
    }

    // Sort draw calls by pipeline, then material, then (sub)mesh
    qsort(draw_calls, draw_call_count, sizeof(draw_call), (int (*)(const void*, const void*))compare_draw_call);

    draw_calls_dirty = false;
}

void render()
{
    // Update frame specific data
    update_camera();
    update_lights();
    update_objects();
    update_draw_calls();

    // Get framebuffer
    surface_t *disp = display_get();
    rdpq_debug_log_msg("---> Frame");
    rdpq_attach_clear(disp, &zbuffer);

    // Set up render modes with rdpq. This could be set per material, but for simplicity's sake we use the same render mode for all objects in this demo.
    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_mode_dithering(DITHER_SQUARE_SQUARE);
        rdpq_mode_zbuf(true, true);
        rdpq_mode_antialias(AA_STANDARD);
        rdpq_mode_persp(true);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER2((TEX0,0,SHADE,0), (TEX0,0,SHADE,0), (COMBINED,0,PRIM,0), (COMBINED,0,PRIM,0)));
        rdpq_mode_fog(RDPQ_BLENDER((FOG_RGB, SHADE_ALPHA, IN_RGB, INV_MUX_ALPHA)));
    rdpq_mode_end();

    rdpq_set_fog_color(color_from_packed32(fog_color));

    // Set viewport, culling mode and geometry flags
    mg_set_viewport(&viewport);
    mg_set_culling(&culling);

    // In this demo, all our materials use variations of the same pipeline which are compatible with respect to their uniforms.
    // When binding a pipeline, uniforms are not automatically invalidated, which means we can load uniforms that stay constant 
    // for the entire scene here (for example lighting).
    mg_uniform_load(fog_uniform, &scene_uniform_buffer[fb_index]->fog);
    mg_uniform_load(lighting_uniform, &scene_uniform_buffer[fb_index]->lighting);

    uint32_t current_pipeline_id = -1;
    uint32_t current_material_id = -1;
    uint32_t current_mesh_id = -1;
    uint32_t current_object_id = -1;

    material_data *current_material = NULL;
    submesh_data *current_submesh = NULL;
    object_data *current_object = NULL;
    draw_call *draw_call = NULL;

    // Iterate over all draw calls
    for (size_t i = 0; i < draw_call_count; i++)
    {
        draw_call = &draw_calls[i];
        rdpq_debug_log_msg("-----> Draw call");

        // Bind the correct pipeline for the current draw call.
        if (draw_call->pipeline_id != current_pipeline_id) {
            current_pipeline_id = draw_call->pipeline_id;
            mg_pipeline_bind(pipelines[current_pipeline_id]);
        }

        // Set the current material uniforms.
        // To avoid redundant uploads, we keep track of the current material and only make this call when it actually changes.
        // This will be most optimal if the list of objects has been sorted by material.
        if (draw_call->material_id != current_material_id) {
            rdpq_debug_log_msg("-------> Material");
            current_material_id = draw_call->material_id;
            current_material = &materials[current_material_id];
            mg_uniform_load(texturing_uniform, &current_material->buffer->texturing);
            mg_uniform_load(modes_uniform, &current_material->buffer->modes);

            // Also set the geometry flags, which determine the type of triangle to be drawn.
            mg_set_geometry_flags(current_material->geometry_flags);

            // Additionally, upload the material's texture and change the material color via rdpq, which can be done completely independently from magma.
            rdpq_set_prim_color(current_material->color);
            if (current_material->texture) {
                rdpq_sprite_upload(TILE0, current_material->texture, &current_material->rdpq_tex_parms);
            }
            rdpq_debug_log_msg("<------- Material");
        }

        // Swap out the currently bound vertex buffer. Similar to materials, we only do this when it changes.
        // Since swapping buffers is very quick, objects should be sorted by material first, and then by mesh.
        if (draw_call->mesh_id != current_mesh_id) {
            current_mesh_id = draw_call->mesh_id;
            // Unpack mesh id and submesh id
            uint16_t mesh_id = current_mesh_id >> 16;
            uint16_t submesh_id = current_mesh_id & 0xFFFF;
            current_submesh = &meshes[mesh_id].submeshes[submesh_id];
            mg_bind_vertex_buffer(current_submesh->vertex_buffer);
        }

        if (draw_call->object_id != current_object_id) {
            current_object_id = draw_call->object_id;
            current_object = &objects[current_object_id];

            // Because the matrices are expected to change every frame and for every object, we upload them "inline", which embeds their data within the command stream itself.
            // After the call returns, the matrix data has been consumed entirely and we won't need to worry about keeping it in memory.
            // If we were to use uniform buffers for matrices as well, we would have to manually synchronize updating them on the CPU.
            // This function uses "mg_uniform_load_inline" internally with a predefined offset and size, and automatically converts the data to a RSP-native format.
            mgfx_set_matrices_inline(matrices_uniform, &(mgfx_matrices_parms_t) {
                .model_view_projection = current_object->mvp_matrix.m[0],
                .model_view = current_object->mv_matrix.m[0],
                .normal = current_object->mv_matrix.m[0],
            });
        }

        assert(current_submesh != NULL);

        // Perform the draw call. This will assemble the triangles from the currently bound vertex/index buffers, process them with the
        // currently bound pipeline (using the attached vertex shader), using all currently loaded uniforms such as matrices, lighting and material parameters etc.
        rdpq_debug_log_msg("-------> Draw");

        // Even when drawing commands are recorded into a block, we need to put them into a drawing batch to ensure proper synchronisation with rdpq.
        mg_draw_begin();
            rspq_block_run(current_submesh->block);
        mg_draw_end();

        rdpq_debug_log_msg("<------- Draw");

        rdpq_debug_log_msg("<----- Draw call");
    }

    if (display_metrics) {
        debug_draw_perf_overlay(last_3d_fps);
    }

    // Done. Detach from the framebuffer and present it.
    rdpq_detach_show();

    rdpq_debug_log_msg("<--- Frame");

    rspq_profile_next_frame();

    if (frames == 30) {
        if (!display_metrics) {
            last_3d_fps = display_get_fps();
            rspq_wait();
            rspq_profile_get_data(&profile_data);
            if (request_display_metrics) display_metrics = true;
        }

        frames = 0;
        rspq_profile_reset();
    }

    frames++;

    // Cycle the index used for accessing buffers that change per frame.
    fb_index = (fb_index + 1) % FB_COUNT;
}
