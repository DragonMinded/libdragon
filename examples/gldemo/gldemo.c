#include <libdragon.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>

#include "plane.h"
#include "debug_overlay.h"

#define TICKRATE   60
#define DELTATIME  (1.0f/(double)TICKRATE)

#define STICK_DEADZONE       10
#define IGNORE_DEADZONE(v)   ((v) > STICK_DEADZONE || (v) < -STICK_DEADZONE ? (v) : 0)

#define CAMERA_YAW_SPEED            0.015f
#define CAMERA_PITCH_SPEED          0.015f
#define CAMERA_DISTANCE_SPEED       0.05f
#define CAMERA_DISTANCE_SPEED_FAST  0.5f
#define CAMERA_MIN_PITCH            (-M_PI_2 * 0.9)
#define CAMERA_MAX_PITCH            (M_PI_2 * 0.9)

#define TEXTURE_COUNT 4

static double subtick;

static surface_t *zbuffer;

static const GLfloat environment_color[] = { 0.1f, 0.03f, 0.2f, 1.f };

static fm_vec3_t camera_position = {{4.47f * 0.051812f, 134.180f * 0.051812f, 124.243f * 0.051812f}};
static float camera_yaw = 0.030796327f;
static float camera_pitch = -0.908407346f;

static uint64_t frames = 0;
static bool display_metrics = false;
static bool request_display_metrics = false;
static float last_3d_fps = 0.0f;

static const char *texture_path[TEXTURE_COUNT] = {
    "rom:/unit1m.i8.sprite",
    "rom:/diamond0.sprite",
    "rom:/pentagon0.sprite",
    "rom:/triangle0.sprite",
};

static sprite_t *sprites[TEXTURE_COUNT];
static GLuint textures[TEXTURE_COUNT];

static model64_t *model;

void init()
{
	debug_init(DEBUG_FEATURE_LOG_ISVIEWER | DEBUG_FEATURE_LOG_USB);
    dfs_init(DFS_DEFAULT_LOCATION);
    joypad_init();
    resolution_t resolution = RESOLUTION_320x240;
    display_init(resolution, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    zbuffer = display_get_zbuf();
    gl_init();
    //rdpq_debug_start();
    //rdpq_debug_log(true);

    rspq_profile_start();
    debug_overlay_init();

    glGenTextures(TEXTURE_COUNT, textures);
    for (uint32_t i = 0; i < TEXTURE_COUNT; i++)
    {
        sprites[i] = sprite_load(texture_path[i]);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glSpriteTextureN64(GL_TEXTURE_2D, sprites[i], &(rdpq_texparms_t){.s.repeats = REPEAT_INFINITE, .t.repeats = REPEAT_INFINITE});
    }

    setup_plane();
    make_plane_mesh();

    glMatrixMode(GL_PROJECTION);
    float aspect_ratio = (float)resolution.width / (float)resolution.height;
    gluPerspective(85.f, aspect_ratio, 1.f, 100.f);

    float ambient[4] = {0};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    float light_radius = 30.0f;
    float light_color[4] = { 0.5f, 0.7f, 0.2f, 1.0f };

    //glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_color);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 1.0f/(light_radius*light_radius));

    glEnable(GL_LIGHT4);
    float light_color2[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glLightfv(GL_LIGHT4, GL_DIFFUSE, light_color2);

    //GLfloat mat_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    //glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_diffuse);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glEnable(GL_FOG);
    glFogf(GL_FOG_START, 20);
    glFogf(GL_FOG_END, 90);
    glFogfv(GL_FOG_COLOR, environment_color);

    model = model64_load("rom:/scene.model64");
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

void update_fixed(float deltatime)
{
    joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);

    int8_t stick_x = IGNORE_DEADZONE(inputs.stick_x);
    int8_t stick_y = IGNORE_DEADZONE(inputs.stick_y);
    int8_t cstick_x = IGNORE_DEADZONE(inputs.cstick_x);
    int8_t cstick_y = IGNORE_DEADZONE(inputs.cstick_y);

    camera_yaw -= cstick_x * deltatime * CAMERA_YAW_SPEED;
    camera_pitch -= cstick_y * deltatime * CAMERA_PITCH_SPEED;

    if (camera_yaw > M_TWOPI) camera_yaw -= M_TWOPI;
    if (camera_yaw < 0.0f) camera_yaw += M_TWOPI;

    if (camera_pitch > CAMERA_MAX_PITCH) camera_pitch = CAMERA_MAX_PITCH;
    if (camera_pitch < CAMERA_MIN_PITCH) camera_pitch = CAMERA_MIN_PITCH;

    float distance_speed = deltatime * (inputs.btn.z ? CAMERA_DISTANCE_SPEED : CAMERA_DISTANCE_SPEED_FAST);

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
}

void update(float deltatime)
{
    joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    // L toggles the debug/profiler overlay on/off
    if (btn.l) {
        request_display_metrics = !request_display_metrics;
        if (!request_display_metrics) display_metrics = false;
    }

    if (btn.d_up) {
        if (end_idx < plane_index_count) end_idx += 3;
    }
    if (btn.d_down) {
        if (end_idx > first_idx) end_idx -= 3;
    }
    if (btn.d_right) {
        if (first_idx < end_idx) first_idx += 3;
    }
    if (btn.d_left) {
        if (first_idx > 0) first_idx -= 3;
    }

    joypad_buttons_t btn_held = joypad_get_buttons_held(JOYPAD_PORT_1);
    if (btn_held.z) {
        modify_vertices(get_ticks_ms() / 1000.0f);
    }

    surface_t *framebuffer = display_get();
    rdpq_attach(framebuffer, zbuffer);

    gl_context_begin();

    glClearColor(environment_color[0], environment_color[1], environment_color[2], environment_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    fm_vec3_t forward;
    direction_from_pitch_yaw(&forward, camera_pitch, camera_yaw);
    fm_vec3_add(&forward, &camera_position, &forward);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        camera_position.x, camera_position.y, camera_position.z,
        forward.x, forward.y, forward.z,
        0.f, 1.f, 0.f);

    fm_vec4_t light_position = {{ 0, 10, 0, 1 }};
    glLightfv(GL_LIGHT0, GL_POSITION, light_position.v);
    fm_vec4_t light_position2 = {{ 0.196116f, -0.784465f, -0.588348f, 0.0f }};
    glLightfv(GL_LIGHT4, GL_POSITION, light_position2.v);

    //glEnable(GL_MULTISAMPLE_ARB);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);

    fm_vec4_t mat_color = {{0, 1, 1, 1}};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_color.v);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    //render_plane();
    model64_draw(model);

    gl_context_end();

    if (display_metrics) {
        debug_draw_perf_overlay(last_3d_fps);
    }

    rdpq_detach_show();

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
}

int main()
{
    init();

    float accumulator = 0;
    const float dt = DELTATIME;

    while (true)
    {
        joypad_poll();
        
        float frametime = display_get_delta_time();
        
        // In order to prevent problems if the game slows down significantly, we will clamp the maximum timestep the simulation can take
        if (frametime > 0.25f)
            frametime = 0.25f;
        
        // Perform the update in discrete steps (ticks)
        accumulator += frametime;
        while (accumulator >= dt)
        {
            update_fixed(dt);
            accumulator -= dt;
        }

        subtick = ((double)accumulator)/((double)dt);
        update(frametime);
    }
}
