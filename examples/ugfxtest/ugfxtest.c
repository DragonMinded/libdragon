#include <stdlib.h>
#include <libdragon.h>
#include "mesh.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define RADIANS(deg) ((deg) * (M_PI/180))

static ugfx_light_t lights[] = {
    {
        .r = 40,
        .g = 20,
        .b = 30,
    },
    {
        .r = 0xFF,
        .g = 0xFF,
        .b = 0xFF,
        .x = 0,
        .y = 0,
        .z = 127
    }
};

void perspective(float fovy,
                 float aspect,
                 float nearZ,
                 float farZ,
                 float dest[4][4]) {
  float f, fn;

  f  = 1.0f / tanf(fovy * 0.5f);
  fn = 1.0f / (nearZ - farZ);

  dest[0][0] = f / aspect;
  dest[1][1] = f;
  dest[2][2] = (nearZ + farZ) * fn;
  dest[2][3] =-1.0f;
  dest[3][2] = 2.0f * nearZ * farZ * fn;
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    ugfx_init(UGFX_DEFAULT_RDP_BUFFER_SIZE);
    dfs_init(DFS_DEFAULT_LOCATION);

    /* Load texture file */
    int fp = dfs_open("/test.sprite");
    sprite_t *sprite = malloc(dfs_size(fp));
    dfs_read(sprite, 1, dfs_size(fp), fp);
    dfs_close(fp);

    uint32_t display_width = display_get_width();
    uint32_t display_height = display_get_height();

    /* Create viewport */
    ugfx_viewport_t viewport;
    ugfx_viewport_init(&viewport, 0, 0, display_width, display_height);

    data_cache_hit_writeback(&viewport, sizeof(viewport));

    /* Construct view + projection matrix */
    ugfx_matrix_t pv_matrix;
    float pv_matrix_f[4][4];

    float near_plane = .1f;
    float far_plane = 100.f;

    perspective(RADIANS(70.f), 4.f/3.f, near_plane, far_plane, pv_matrix_f);
    ugfx_matrix_from_column_major(&pv_matrix, pv_matrix_f[0]);
    data_cache_hit_writeback(&pv_matrix, sizeof(pv_matrix));

    /* Calculate perspective normalization scale. This is needed to re-normalize W-coordinates
       after they have been distorted by the perspective matrix. */
    uint16_t perspective_normalization_scale = float_to_fixed(get_persp_norm_scale(near_plane, far_plane), 16);

    /* Allocate depth buffer */
    void *depth_buffer = malloc(display_width * display_height * 2);

    int rotation_counter = 0;

    while (1)
    {
        static display_context_t disp = 0;

        /* Grab a render buffer */
        while( !(disp = display_lock()) );

        ugfx_matrix_t m_matrix;

        /* Quick'n'dirty rotation + translation matrix */
        float z = -3.0f;
        float angle = RADIANS((float)(rotation_counter++));
        float c = cosf(angle);
        float s = sinf(angle);
        float m_matrix_f[4][4] = {
            {c,    0.0f, -s,   0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {s,    0.0f, c,    0.0f},
            {0.0f, 0.0f, z,    1.0f}
        };

        ugfx_matrix_from_column_major(&m_matrix, m_matrix_f[0]);
        data_cache_hit_writeback(&m_matrix, sizeof(m_matrix));

        /* Prepare the command list to be executed by the microcode */
        ugfx_command_t commands [] = {
            /* Set general settings */
            ugfx_set_scissor(0, 0, display_width << 2, display_height << 2, UGFX_SCISSOR_DEFAULT),
            ugfx_load_viewport(0, &viewport),
            ugfx_set_z_image(depth_buffer),

            /* Prepare for buffer clearing */
            ugfx_set_other_modes(UGFX_CYCLE_FILL),

            /* Clear depth buffer */
            ugfx_set_color_image(depth_buffer, UGFX_FORMAT_RGBA, UGFX_PIXEL_SIZE_16B, display_width - 1),
            ugfx_set_fill_color(PACK_ZDZx2(0xFFFF, 0)),
            ugfx_fill_rectangle(0, 0, display_width << 2, display_height << 2),

            /* Clear color buffer (note that color buffer will still be set afterwards) */
            ugfx_set_display(disp),
            ugfx_set_fill_color(PACK_RGBA16x2(40, 20, 30, 255)),
            ugfx_fill_rectangle(0, 0, display_width << 2, display_height << 2),

            /* Set up projection matrix */
            ugfx_set_view_persp_matrix(0, &pv_matrix),
            ugfx_set_persp_norm(perspective_normalization_scale),

            /* Set lights */
            /* The ambient light is always active at index 0. ugfx_set_num_lights(n) only sets the number of directional lights. */
            ugfx_set_num_lights(1),
            ugfx_load_light(0, &lights[0], 0),
            ugfx_load_light(0, &lights[1], 1),

            /* Set render modes for drawing the mesh */
            ugfx_set_other_modes(
                UGFX_CYCLE_1CYCLE |
                ugfx_blend_1cycle(UGFX_BLEND_IN_RGB, UGFX_BLEND_IN_ALPHA, UGFX_BLEND_MEM_RGB, UGFX_BLEND_1_MINUS_A) |
                UGFX_SAMPLE_2x2 | UGFX_Z_OPAQUE | UGFX_Z_SOURCE_PIXEL | UGFX_CVG_CLAMP | UGFX_BI_LERP_0 | UGFX_BI_LERP_1 |
                UGFX_Z_COMPARE | UGFX_Z_UPDATE | UGFX_PERSP_TEX | UGFX_ALPHA_CVG_SELECT | UGFX_IMAGE_READ | UGFX_ANTIALIAS),
            ugfx_set_combine_mode(
                UGFX_CC_SHADE_COLOR, UGFX_CC_SUB_0, UGFX_CC_T0_COLOR, UGFX_CC_ADD_0, UGFX_AC_0, UGFX_AC_0, UGFX_AC_0, UGFX_AC_1,
                UGFX_CC_SHADE_COLOR, UGFX_CC_SUB_0, UGFX_CC_T0_COLOR, UGFX_CC_ADD_0, UGFX_AC_0, UGFX_AC_0, UGFX_AC_0, UGFX_AC_1),
            ugfx_set_cull_mode(UGFX_CULL_BACK),
            ugfx_set_geometry_mode(UGFX_GEOMETRY_SHADE | UGFX_GEOMETRY_ZBUFFER | UGFX_GEOMETRY_TEXTURE | UGFX_GEOMETRY_SMOOTH | UGFX_GEOMETRY_LIGHTING),
            ugfx_set_clip_ratio(2),

            /* Point RDP towards texture data and set tile settings */
            ugfx_set_texture_image(sprite->data, UGFX_FORMAT_RGBA, UGFX_PIXEL_SIZE_32B, sprite->width - 1),
            ugfx_set_tile(UGFX_FORMAT_RGBA, UGFX_PIXEL_SIZE_32B, (2 * sprite->width) >> 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            ugfx_load_tile(0 << 2, 0 << 2, (sprite->width - 1) << 2, (sprite->height - 1) << 2, 0),
            
            /* The texture settings to use for the following primitives */
            ugfx_set_texture_settings(0x8000, 0x8000, 0, 0),

            /* Set model matrix and draw mesh by linking to a constant command list */
            ugfx_set_model_matrix(0, &m_matrix),
            ugfx_set_address_slot(1, mesh_vertices),
            ugfx_push_commands(0, mesh_commands, mesh_commands_length),
            ugfx_sync_pipe(),

            /* Finish up */
            ugfx_sync_full(),
            ugfx_finalize(),
        };

        data_cache_hit_writeback(commands, sizeof(commands));

        /* Load command list into RSP DMEM and run the microcode */
        ugfx_load(commands, sizeof(commands) / sizeof(*commands));
        rsp_run();

        /* Force backbuffer flip */
        display_show(disp);
    }
}
