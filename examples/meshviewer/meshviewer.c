#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <libdragon.h>

// Using model64_internal.h to access animation names and texture counts after
// loading because they are currently not exposed in public API for models64.h
#include "../../src/model64_internal.h"

// Limits to avoid dynamic allocation
#define MAX_MODELS 128
#define MAX_MODEL_NAME 256
#define MAX_MODEL_PATH 512
#define MAX_ANIMS 64

typedef struct
{
    char name[MAX_MODEL_NAME];
    char path[MAX_MODEL_PATH];
} model_entry_t;

typedef struct
{
    model_entry_t entries[MAX_MODELS];
    int count;
} model_list_t;

static model64_t *model = NULL;
static int model_index = 0;
static int anim_index = 0;
static bool sd_available = false;
static bool active_is_sd = false;
static bool tint_mesh = true;

// Optional Passes
static bool use_shadowpass = false;
static bool use_outlinepass = false;

static const char *anim_names[MAX_ANIMS] = {0};
static int anim_count = 0;

typedef enum
{
    MODEL_SOURCE_ROM = 0,
    MODEL_SOURCE_SD = 1,
} model_source_t;

static model_source_t current_source = MODEL_SOURCE_ROM;
static model_list_t rom_models = {0};
static model_list_t sd_models = {0};

static const int font_id = 10;

static bool str_ends_with(const char *str, const char *suffix)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static void model_list_add(model_list_t *list, const char *path,
                           const char *name)
{
    if (!list || list->count >= MAX_MODELS)
        return;

    model_entry_t *entry = &list->entries[list->count];
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    list->count++;
}

static void scan_models(const char *base_path, model_list_t *list)
{
    if (list) {
        list->count = 0;
    }

    dir_t dir = {0};
    if (dir_findfirst(base_path, &dir) != 0)
        return;

    do {
        if (dir.d_type != DT_REG)
            continue;
        if (!str_ends_with(dir.d_name, ".model64"))
            continue;

        char full_path[MAX_MODEL_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, dir.d_name);

        char name_buf[MAX_MODEL_NAME];
        strncpy(name_buf, dir.d_name, sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        char *ext = strstr(name_buf, ".model64");
        if (ext)
            *ext = '\0';

        model_list_add(list, full_path, name_buf);
    } while (dir_findnext(base_path, &dir) == 0);
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

static void build_anim_list(void)
{
    anim_count = 0;

    if (!model || !model->data || model->data->num_anims == 0)
        return;

    for (uint32_t i = 0; i < model->data->num_anims; i++) {
        if (anim_count >= MAX_ANIMS)
            break;
        if (!model->data->anims[i].name)
            continue;
        anim_names[anim_count++] = model->data->anims[i].name;
    }
}

static void play_current_anim(void)
{
    if (!model)
        return;
    if (anim_count <= 0)
        return;
    if (anim_index < 0 || anim_index >= anim_count)
        anim_index = 0;
    const char *anim = anim_names[anim_index];
    if (!anim)
        return;
    reset_root_transform();
    model64_anim_play(model, anim, MODEL64_ANIM_SLOT_0, false, 0.0f);
    model64_anim_set_loop(model, MODEL64_ANIM_SLOT_0, true);
}

static model_list_t *active_model_list(void)
{
    return (current_source == MODEL_SOURCE_SD) ? &sd_models : &rom_models;
}

static void load_current_model(void)
{
    if (model) {
        model64_free(model);
        model = NULL;
    }
    anim_count = 0; // Clear list
    tint_mesh = true;

    model_list_t *list = active_model_list();
    active_is_sd = (current_source == MODEL_SOURCE_SD);
    if (!list || list->count <= 0)
        return;

    if (model_index < 0 || model_index >= list->count)
        model_index = 0;

    const model_entry_t *entry = &list->entries[model_index];
    const char *path = entry->path;
    model = model64_load(path);
    if (!model)
        return;

    if (model->data && model->data->num_textures > 0)
        tint_mesh = false;

    build_anim_list();
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
    scan_models("rom:/models", &rom_models);
    if (sd_available)
        scan_models("sd:/models", &sd_models);
    load_current_model();

    uint32_t last_ms = get_ticks_ms();
    float rotation = 0.0f;

    while (1) {
        joypad_poll();
        joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if (pressed.l) {
            if (current_source != MODEL_SOURCE_ROM) {
                current_source = MODEL_SOURCE_ROM;
                model_index = 0;
                load_current_model();
            }
        }
        if (pressed.r) {
            if (current_source != MODEL_SOURCE_SD) {
                current_source = MODEL_SOURCE_SD;
                model_index = 0;
                load_current_model();
            }
        }

        model_list_t *list = active_model_list();
        if (pressed.d_left && list && list->count > 0) {
            model_index = (model_index - 1 + list->count) % list->count;
            load_current_model();
        }
        if (pressed.d_right && list && list->count > 0) {
            model_index = (model_index + 1) % list->count;
            load_current_model();
        }
        if (pressed.c_left && anim_count > 0) {
            anim_index = (anim_index - 1 + anim_count) % anim_count;
            play_current_anim();
        }
        if (pressed.c_right && anim_count > 0) {
            anim_index = (anim_index + 1) % anim_count;
            play_current_anim();
        }

        // Optional additonal render passes
        if (pressed.c_up) {
            use_shadowpass = !use_shadowpass;
        }
        if (pressed.c_down) {
            use_outlinepass = !use_outlinepass;
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
        glTranslatef(0.0f, -0.5f, -3.0f);
        glRotatef(rotation, 0.0f, 1.0f, 0.0f);

        if (model) {
            if (use_shadowpass) {
                glPushMatrix();
                glTranslatef(0.0f, -0.7f, 0.0f);
                glScalef(1.1f, 0.02f, 1.1f);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
                model64_draw(model);
                glPopMatrix();
            }

            if (use_outlinepass) {
                glPushMatrix();
                glScalef(1.03f, 1.03f, 1.03f);
                glCullFace(GL_FRONT);
                glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
                model64_draw(model);
                glCullFace(GL_BACK);
                glPopMatrix();
            }

            // Normal pass
            // Mesh color based on source location
            if (tint_mesh) {
                if (active_is_sd) {
                    glColor4f(0.30f, 0.454f, 0.6f, 1.0f);
                } else {
                    glColor4f(0.2313f, 0.427f, 0.384f, 1.0f);
                }
            } else {
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            }
            model64_draw(model);
        }

        gl_context_end();

        rdpq_set_mode_standard();
        const char *source = active_is_sd ? "SD" : "ROM";
        const char *anim = (anim_count > 0 && anim_names[anim_index])
                               ? anim_names[anim_index]
                               : "None";
        const char *model_name = (list && list->count > 0)
                                     ? list->entries[model_index].name
                                     : "None";

        const char *no_models_msg = "";
        if (rom_models.count + sd_models.count == 0) {
            no_models_msg = sd_available
                                ? "\nNo models found in ROM or SD."
                                : "\nNo models found in ROM. SD not mounted.";
        } else if (!list || list->count == 0) {
            if (active_is_sd && !sd_available) {
                no_models_msg = "\nSD not mounted.";
            } else {
                no_models_msg = (active_is_sd) ? "\nNo models found on SD."
                                               : "\nNo models found in ROM.";
            }
        }

        rdpq_text_printf(NULL, font_id, 8, 8,
                         "\nModel: %s\nAnim: %s\nSource: %s%s\nD-Left/Right: "
                         "model  C-Left/Right: anim\nC-Up: shadow  C-Down: "
                         "outline\nL: ROM  R: SD%s",
                         model_name, anim, source,
                         (!sd_available && active_is_sd) ? " (SD not mounted)"
                                                         : "",
                         no_models_msg);

        rdpq_detach_show();
    }
}
