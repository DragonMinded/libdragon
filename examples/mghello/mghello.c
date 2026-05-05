#include <libdragon.h>

typedef struct {
    int16_t pos[3];
} vertex;

typedef struct {
    mgfx_fog_t fog;
    mgfx_lighting_t lighting;
    mgfx_texturing_t texturing;
    mgfx_matrices_t matrices;
} uniforms;

int main()
{
    const resolution_t resolution = RESOLUTION_320x240;

	debug_init(DEBUG_FEATURE_LOG_EMU | DEBUG_FEATURE_LOG_USB);
    display_init(resolution, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    rdpq_init();
    mg_init();

    // Get the builtin rendering pipeline.
    // We need to configure the layout of our vertex data. Magma will then patch the pipeline according to this layout.
    mg_vertex_attribute_t vertex_attributes[] = {
        {
            .input = MGFX_ATTRIBUTE_POSITION,
            .offset = offsetof(vertex, pos)
        },
    };
    mg_pipeline_t *pipeline = mg_pipeline_create(&(mg_pipeline_parms_t) {
        .vertex_shader_ucode = mgfx_get_shader_ucode(0),
        .vertex_layout.attribute_count = sizeof(vertex_attributes)/sizeof(vertex_attributes[0]),
        .vertex_layout.attributes = vertex_attributes,
        .vertex_layout.stride = sizeof(vertex)
    });

    // Shader uniforms are not initialized to defaults automatically, so we need to create
    // a uniform buffer that sets everything to sane values. 
    uniforms *uniform_data = malloc_uncached(sizeof(uniforms));

    mgfx_get_fog(&uniform_data->fog, &(mgfx_fog_parms_t) {});

    // Lighting is never explicitly turned off, but it can be set to "pass through"
    // by configuring 0 lights and fully white ambient light.
    mgfx_get_lighting(&uniform_data->lighting, &(mgfx_lighting_parms_t) {
        .light_count = 0,
        .ambient_color = color_from_packed32(0xFFFFFFFF)
    });

    // Initialize all matrices to identity
    fm_mat4_t identity;
    fm_mat4_identity(&identity);
    mgfx_get_matrices(&uniform_data->matrices, &(mgfx_matrices_parms_t) {
        .model_view_projection = identity.m[0],
        .model_view = identity.m[0],
        .normal = identity.m[0],
    });

    const mg_uniform_t *fog_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_FOG);
    const mg_uniform_t *lighting_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_LIGHTING);
    const mg_uniform_t *texturing_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_TEXTURING);
    const mg_uniform_t *matrices_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_MATRICES);

    // Create and fill a vertex buffer.
    vertex *vertices = malloc_uncached(sizeof(vertex) * 3);
    vertices[0] = (vertex){ .pos = MGFX_POS(0, -0.5f, 0) };
    vertices[1] = (vertex){ .pos = MGFX_POS(-0.5f, 0.5f, 0) };
    vertices[2] = (vertex){ .pos = MGFX_POS(0.5f, 0.5f, 0) };

    // Everything we need is initialised. Start the main rendering loop!
    while (true)
    {
        // This is just the regular display + rdpq setup.
        surface_t *disp = display_get();

        rdpq_attach_clear(disp, NULL);

        rdpq_mode_begin();
            rdpq_set_mode_standard();
            rdpq_mode_dithering(DITHER_SQUARE_SQUARE);
            rdpq_mode_antialias(AA_STANDARD);
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_end();

        rdpq_set_prim_color(color_from_packed32(0xFFFFFFFF));

        // Set the vertex pipeline
        mg_pipeline_bind(pipeline);

        // Set the viewport to the full screen
        mg_set_viewport(&(mg_viewport_t) {
            .x = 0,
            .y = 0,
            .width = resolution.width,
            .height = resolution.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

        // Set the culling mode.
        mg_set_culling(&(mg_culling_parms_t) {
            .cull_mode = MG_CULL_MODE_NONE
        });

        // Configure the type of triangles that should be emitted to the RDP.
        // We just want vertex colors, so use SHADE.
        mg_set_geometry_flags(0);

        // Load uniforms. This must be done every frame to guarantee that the uniforms have the desired values.
        mg_uniform_load(fog_uniform, &uniform_data->fog);
        mg_uniform_load(lighting_uniform, &uniform_data->lighting);
        mg_uniform_load(texturing_uniform, &uniform_data->texturing);
        mg_uniform_load(matrices_uniform, &uniform_data->matrices);

        // Bind the vertex buffer that was created above. All subsequent drawing
        // commands will now read from this buffer.
        mg_bind_vertex_buffer(vertices);

        // All drawing commands (including mg_draw and mg_draw_indexed) must be put 
        // between mg_draw_begin and mg_draw_end, which delimit a drawing "batch". 
        // This is to guarantee proper synchronisation with render modes. 
        // Render modes must not be changed during a drawing batch.
        mg_draw_begin();
            // Load all vertices from the buffer into the internal cache
            mg_load_vertices(0, 0, 3);
            // Draw a triangle using those vertices
            mg_draw_triangle(0, 1, 2);
        mg_draw_end();

        // End the frame as usual.
        rdpq_detach_show();
    }
}
