#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <GL/glu.h>
#include <libdragon.h>
#include <math.h>
#include <stdio.h>

typedef struct
{
    const char *name;
    const char *rom_path;
    const char *sd_path;
    const char *anims[4];
    int anim_count;
} model_entry_t;

// clang-format off
static const model_entry_t models[] = {
    {
        "Animated Cylinder",
        "rom:/models/animated_cylinder.model64",
        "sd:/models/animated_cylinder.model64",
        { "Rotate", "Pulse" },
        2
    },
    {
        "Animated Cube",
        "rom:/models/animated_cube.model64",
        "sd:/models/animated_cube.model64",
        { "Bounce", "Pulse" },
        2
    },
    {
        "Animated Pyramid",
        "rom:/models/animated_pyramid.model64",
        "sd:/models/animated_pyramid.model64",
        { "Slide", "Pulse" },
        2
    },
};
// clang-format on

static const int model_count = sizeof(models) / sizeof(models[0]);

static model64_t *model = NULL;
static int model_index = 0;
static int anim_index = 0;
static bool use_sd = false;
static bool sd_available = false;
static bool active_is_sd = false;

static const int font_id = 10;

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

static void reset_root_transform(void)
{
    if (!model)
        return;
    model64_node_t *node = model64_get_node(model, 0);
    model64_set_node_pos(model, node, 0.0f, 0.0f, 0.0f);
    model64_set_node_scale(model, node, 1.0f, 1.0f, 1.0f);
    model64_set_node_rot_quat(model, node, 0.0f, 0.0f, 0.0f, 1.0f);
}

static void play_current_anim(void)
{
    if (!model)
        return;
    if (models[model_index].anim_count <= 0)
        return;
    const char *anim = models[model_index].anims[anim_index];
    reset_root_transform();
    model64_anim_play(model, anim, MODEL64_ANIM_SLOT_0, false, 0.0f);
    model64_anim_set_loop(model, MODEL64_ANIM_SLOT_0, true);
}

static void load_current_model(void)
{
    if (model) {
        model64_free(model);
        model = NULL;
    }

    const model_entry_t *entry = &models[model_index];
    const char *path = entry->rom_path;
    active_is_sd = false;

    if (use_sd && sd_available && file_exists(entry->sd_path)) {
        path = entry->sd_path;
        active_is_sd = true;
    } else if (!file_exists(entry->rom_path)) {
        // Neither ROM nor SD found -> keep NULL
        return;
    }

    model = model64_load(path);
    anim_index = 0;
    play_current_anim();
}

static void setup_gl(void)
{
    float aspect = (float)display_get_width() / (float)display_get_height();
    float near_plane = 1.0f;
    float far_plane = 50.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-near_plane * aspect, near_plane * aspect, -near_plane,
              near_plane, near_plane, far_plane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE,
                 FILTERS_DISABLED);
    rdpq_init();
    gl_init();
    joypad_init();
    timer_init();

    sd_available = debug_init_sdfs("sd:/", -1);

    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(font_id, font);

    setup_gl();
    load_current_model();

    uint32_t last_ms = get_ticks_ms();
    float rotation = 0.0f;

    while (1) {
        joypad_poll();
        joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if (pressed.l) {
            model_index = (model_index - 1 + model_count) % model_count;
            load_current_model();
        }
        if (pressed.r) {
            model_index = (model_index + 1) % model_count;
            load_current_model();
        }
        if (pressed.z) {
            use_sd = !use_sd;
            load_current_model();
        }
        if (pressed.c_left && models[model_index].anim_count > 0) {
            anim_index = (anim_index - 1 + models[model_index].anim_count) %
                         models[model_index].anim_count;
            play_current_anim();
        }
        if (pressed.c_right && models[model_index].anim_count > 0) {
            anim_index = (anim_index + 1) % models[model_index].anim_count;
            play_current_anim();
        }

        uint32_t now_ms = get_ticks_ms();
        float dt = (now_ms - last_ms) / 1000.0f;
        last_ms = now_ms;

        rotation += dt * 30.0f;
        if (rotation >= 360.0f)
            rotation -= 360.0f;

        if (model) {
            model64_update(model, dt);
        }

        surface_t *disp = display_get();
        surface_t *zbuf = display_get_zbuf();

        rdpq_attach(disp, zbuf);
        gl_context_begin();

        glClearColor(0.243f, 0.25f, 0.33f, 1.0f); // BG color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -3.0f);
        glRotatef(rotation, 0.0f, 1.0f, 0.0f);

        // Shadow-Pass
        glPushMatrix();
        glTranslatef(0.0f, -0.7f, 0.0f);
        glScalef(1.1f, 0.02f, 1.1f);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
        model64_draw(model);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glPopMatrix();

        if (model) {
            // Outline pass
            glPushMatrix();
            glScalef(1.03f, 1.03f, 1.03f);
            glCullFace(GL_FRONT);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
            model64_draw(model);
            glCullFace(GL_BACK);
            glPopMatrix();

            // Normal pass
            // Mesh color based on source location
            if (active_is_sd) {
                glColor4f(0.30f, 0.454f, 0.6f, 1.0f);
            } else {
                glColor4f(0.2313f, 0.427f, 0.384f, 1.0f);
            }
            model64_draw(model);
        }

        gl_context_end();

        rdpq_set_mode_standard();
        const char *source = active_is_sd ? "SD" : "ROM";
        const char *anim = (models[model_index].anim_count > 0)
                               ? models[model_index].anims[anim_index]
                               : "None";

        rdpq_text_printf(NULL, font_id, 8, 8,
                         "\nModel: %s\nAnim: %s\nSource: %s%s\nL/R: model  "
                         "C-Left/Right: anim  Z: source",
                         models[model_index].name, anim, source,
                         (use_sd && !sd_available) ? " (SD not mounted)" : "");

        rdpq_detach_show();
    }
}