#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>

#include "plane.h"

#define TICKRATE   30
#define DELTATIME  (1.0f/(double)TICKRATE)

#define STICK_DEADZONE       10
#define IGNORE_DEADZONE(v)   ((v) > STICK_DEADZONE || (v) < -STICK_DEADZONE ? (v) : 0)

#define CAMERA_YAW_SPEED            0.015f
#define CAMERA_PITCH_SPEED          0.015f
#define CAMERA_DISTANCE_SPEED       0.05f
#define CAMERA_DISTANCE_SPEED_FAST  0.5f
#define CAMERA_MIN_PITCH            (-M_PI_2 * 0.9)
#define CAMERA_MAX_PITCH            (M_PI_2 * 0.9)

static double subtick;

static surface_t *zbuffer;

static const GLfloat environment_color[] = { 0.1f, 0.03f, 0.2f, 1.f };

static fm_vec3_t camera_position;
static float camera_yaw;
static float camera_pitch;

void init()
{
	debug_init(DEBUG_FEATURE_LOG_ISVIEWER | DEBUG_FEATURE_LOG_USB);
    joypad_init();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    zbuffer = display_get_zbuf();
    gl_init();

    setup_plane();
    make_plane_mesh();
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
    rdpq_attach(display_get(), zbuffer);

    // TODO: remove rdpq stuff by implementing it in GL
    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_mode_dithering(DITHER_SQUARE_SQUARE);
        rdpq_mode_antialias(AA_STANDARD);
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rdpq_mode_end();
    rdpq_set_prim_color(color_from_packed32(0xFFFFFFFF));

    gl_context_begin();

    glClearColor(environment_color[0], environment_color[1], environment_color[2], environment_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    render_plane();

    gl_context_end();

    rdpq_detach_show();
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
