/**
 * @file rdpqblendfx.c
 * @brief Additive, subtractive, multiply, and screen effects with BlendFX.
 *
 * This example demonstrates the BlendFX configuration helper, transformed
 * blits, repeated-source batching, source formats, large-source splitting,
 * and recording ordinary RDPQ drawing in an RSPQ block.
 *
 * Controls:
 *   Z              next scene
 *   A / B          enlarge / shrink particles
 *   L / R          decrease / increase effect strength
 *   C-left/right   select additive / subtractive / multiply / screen
 *   C-up/down      add / remove particles
 *   D-left/right   change background
 *   Start          reset
 */

#include <libdragon.h>
#include <math.h>
#include <stdlib.h>

#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240
#define MAX_OBJECTS         16
#define BACKGROUND_COUNT    7

/* The logo is a 24x24 mark in a 32x32 surface with a transparent gutter. */
#define SPRITE_SIZE         32
#define SPRITE_CONTENT_SIZE 24
#define SPRITE_UV_SIZE      26

#define MIN_SCALE           0.25f
#define MAX_SCALE           4.0f
#define SCALE_SPEED         0.015f
#define STRENGTH_SPEED      2
#define UI_FONT             10

typedef struct {
    int x, y;
    int dx, dy;
    float angle;
    float spin;
} moving_sprite_t;

typedef enum {
    SCENE_PARTICLES,
    SCENE_FORMATS,
    SCENE_LARGE_SURFACE,
    SCENE_COUNT,
} demo_scene_t;

static sprite_t *logo_sprite;
static sprite_t *ia16_sprite;
static sprite_t *i4_sprite;
static sprite_t *brew_sprite;
static sprite_t *background_sprite;

static surface_t logo_surface;
static surface_t ia16_surface;
static surface_t i4_surface;
static surface_t brew_surface;
static rspq_block_t *background_block;

static moving_sprite_t objects[MAX_OBJECTS];
static int object_count = 3;
static float object_scale = 1.0f;
static rdpq_blendfx_t effect = RDPQ_BLENDFX_ADD;
static int effect_strength = 255;
static int background_index = 0;
static demo_scene_t scene = SCENE_PARTICLES;

static const char *const effect_names[] = {
    "additive", "subtractive", "multiply", "screen",
};

static const char *const scene_names[] = {
    "moving particles",
    "source formats and color",
    "large scaled sprite",
};

static float particle_scale(void)
{
    /* Scale the 24-pixel visible mark to about 32 pixels at scale 1. */
    return object_scale * SPRITE_SIZE / SPRITE_CONTENT_SIZE;
}

static void reset_object(moving_sprite_t *obj)
{
    int size = fm_roundf(SPRITE_SIZE * particle_scale());

    obj->x = rand() % (SCREEN_WIDTH - size + 1);
    obj->y = rand() % (SCREEN_HEIGHT - size + 1);
    obj->dx = (rand() & 1 ? 1 : -1) * (1 + rand() % 2);
    obj->dy = (rand() & 1 ? 1 : -1) * (1 + rand() % 2);
    obj->angle = (float)rand() / RAND_MAX * FM_PI * 2.0f;
    obj->spin = (rand() & 1 ? 1.0f : -1.0f) *
        (0.012f + (rand() % 13) * 0.001f);
}

static void clamp_objects_to_screen(void)
{
    int size = fm_roundf(SPRITE_SIZE * particle_scale());
    int max_x = SCREEN_WIDTH - size;
    int max_y = SCREEN_HEIGHT - size;

    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (objects[i].x < 0) objects[i].x = 0;
        if (objects[i].y < 0) objects[i].y = 0;
        if (objects[i].x > max_x) objects[i].x = max_x;
        if (objects[i].y > max_y) objects[i].y = max_y;
    }
}

static void update_objects(void)
{
    int size = fm_roundf(SPRITE_SIZE * particle_scale());
    int max_x = SCREEN_WIDTH - size;
    int max_y = SCREEN_HEIGHT - size;

    for (int i = 0; i < object_count; i++) {
        moving_sprite_t *obj = &objects[i];

        if (obj->x + obj->dx < 0 || obj->x + obj->dx > max_x)
            obj->dx = -obj->dx;
        if (obj->y + obj->dy < 0 || obj->y + obj->dy > max_y)
            obj->dy = -obj->dy;

        obj->x += obj->dx;
        obj->y += obj->dy;
        obj->angle += obj->spin;

        if (obj->angle >= FM_PI * 2.0f)
            obj->angle -= FM_PI * 2.0f;
        else if (obj->angle < 0.0f)
            obj->angle += FM_PI * 2.0f;
    }
}

static void configure_blendfx(color_t color)
{
    rdpq_set_mode_standard();
    /* Centered filtering reduces artifacts in BlendFX's packed coordinates. */
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_set_blendfx_parms(effect, &(rdpq_blendfx_parms_t){
        .color = color,
        .transparency = true,
    });
}

static void load_background(void)
{
    char path[32];
    snprintf(path, sizeof(path), "rom:/bg%d.sprite", background_index);

    rspq_wait();
    if (background_block) rspq_block_free(background_block);
    if (background_sprite) sprite_free(background_sprite);
    background_sprite = sprite_load(path);

    rspq_block_begin();
        rdpq_set_mode_standard();
        rdpq_sprite_blit(background_sprite, 0, 0, NULL);
    background_block = rspq_block_end();
}

static void draw_particle(const moving_sprite_t *obj, float scale)
{
    float size = SPRITE_SIZE * scale;

    /* Sample the visible mark while retaining the transparent rotation gutter. */
    rdpq_blendfx_blit_uv_scaled(&logo_surface,
        obj->x + size * 0.5f, obj->y + size * 0.5f,
        (float)SPRITE_UV_SIZE / SPRITE_SIZE,
        &(rdpq_blitparms_t){
            .cx = SPRITE_SIZE / 2,
            .cy = SPRITE_SIZE / 2,
            .scale_x = scale,
            .scale_y = scale,
            .theta = obj->angle,
            .filtering = true,
        });
}

static void render_particles(void)
{
    configure_blendfx(RGBA32(255, 255, 255, effect_strength));

    /* Reuse the source upload for all particles in this batch. */
    float scale = particle_scale();
    rdpq_blendfx_multi_begin();
    for (int i = 0; i < object_count; i++)
        draw_particle(&objects[i], scale);
    rdpq_blendfx_multi_end();
}

static void draw_format_sample(const surface_t *surface, float x, float y,
    color_t color)
{
    configure_blendfx(color);
    rdpq_blendfx_blit(surface, x, y, &(rdpq_blitparms_t){
        .cx = surface->width / 2,
        .cy = surface->height / 2,
        .scale_x = 2.0f,
        .scale_y = 2.0f,
        .filtering = true,
    });
}

static void render_source_formats(void)
{
    /* mksprite reads the .rgba16, .ia16, and .i4 filename suffixes. */
    draw_format_sample(&logo_surface, 80, 120,
        RGBA32(255, 100, 80, effect_strength));
    draw_format_sample(&ia16_surface, 160, 120,
        RGBA32(100, 255, 120, effect_strength));
    draw_format_sample(&i4_surface, 240, 120,
        RGBA32(100, 150, 255, effect_strength));
}

static void render_large_surface(void)
{
    /* This 64x96 source is automatically split because it exceeds BlendFX TMEM. */
    configure_blendfx(RGBA32(255, 220, 120, effect_strength));
    rdpq_blendfx_blit(&brew_surface, SCREEN_WIDTH / 2, 132,
        &(rdpq_blitparms_t){
            .cx = brew_surface.width / 2,
            .cy = brew_surface.height / 2,
            .scale_x = 2.0f,
            .scale_y = 2.0f,
            .filtering = true,
        });
}

static void render_labels(void)
{
    rdpq_set_mode_standard();
    rdpq_text_printf(NULL, UI_FONT, 6, 8, "BlendFX: %s", scene_names[scene]);
    rdpq_text_printf(NULL, UI_FONT, 6, 20, "%s, strength %d%%, background %d",
        effect_names[effect], effect_strength * 100 / 255, background_index);
    rdpq_text_printf(NULL, UI_FONT, 6, 32,
        "Z scene  D-left/right background  C mode  Start reset");

    if (scene == SCENE_FORMATS) {
        rdpq_text_printf(NULL, UI_FONT, 56, 174, "RGBA16");
        rdpq_text_printf(NULL, UI_FONT, 144, 174, "IA16");
        rdpq_text_printf(NULL, UI_FONT, 232, 174, "I4");
    } else if (scene == SCENE_LARGE_SURFACE) {
        rdpq_text_printf(NULL, UI_FONT, 66, 218,
            "64x96 source, 2x, automatic split");
    }
}

static void render_frame(void)
{
    surface_t *framebuffer = display_get();
    rdpq_attach(framebuffer, NULL);
    rspq_block_run(background_block);

    switch (scene) {
    case SCENE_PARTICLES:     render_particles();      break;
    case SCENE_FORMATS:       render_source_formats(); break;
    case SCENE_LARGE_SURFACE: render_large_surface();  break;
    default:                                             break;
    }

    render_labels();
    rdpq_detach_show();
}

static void handle_input(void)
{
    joypad_poll();
    joypad_buttons_t held = joypad_get_buttons(JOYPAD_PORT_1);
    joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    float old_scale = object_scale;

    if (held.a)
        object_scale = fminf(MAX_SCALE, object_scale + SCALE_SPEED);
    if (held.b)
        object_scale = fmaxf(MIN_SCALE, object_scale - SCALE_SPEED);

    if (held.l) {
        effect_strength -= STRENGTH_SPEED;
        if (effect_strength < 0) effect_strength = 0;
    }
    if (held.r) {
        effect_strength += STRENGTH_SPEED;
        if (effect_strength > 255) effect_strength = 255;
    }

    if (pressed.z)
        scene = (scene + 1) % SCENE_COUNT;

    int background_delta = pressed.d_right - pressed.d_left;
    if (background_delta) {
        background_index = (background_index + background_delta +
            BACKGROUND_COUNT) % BACKGROUND_COUNT;
        load_background();
    }
    if (pressed.c_up && object_count < MAX_OBJECTS)
        object_count++;
    if (pressed.c_down && object_count > 1)
        object_count--;

    int effect_count = sizeof(effect_names) / sizeof(effect_names[0]);
    if (pressed.c_left)
        effect = (rdpq_blendfx_t)((effect + effect_count - 1) % effect_count);
    if (pressed.c_right)
        effect = (rdpq_blendfx_t)((effect + 1) % effect_count);

    if (pressed.start) {
        object_scale = 1.0f;
        object_count = 3;
        effect = RDPQ_BLENDFX_ADD;
        effect_strength = 255;
        scene = SCENE_PARTICLES;
        if (background_index != 0) {
            background_index = 0;
            load_background();
        }
    }

    if (object_scale != old_scale)
        clamp_objects_to_screen();
}

int main(void)
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3,
        GAMMA_NONE, FILTERS_RESAMPLE);
    dfs_init(DFS_DEFAULT_LOCATION);
    joypad_init();
    rdpq_init();
    rdpq_text_register_font(UI_FONT,
        rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    logo_sprite = sprite_load("rom:/n64brew.rgba16.sprite");
    ia16_sprite = sprite_load("rom:/n64brew.ia16.sprite");
    i4_sprite = sprite_load("rom:/n64brew.i4.sprite");
    brew_sprite = sprite_load("rom:/n64brew.sprite");

    logo_surface = sprite_get_pixels(logo_sprite);
    ia16_surface = sprite_get_pixels(ia16_sprite);
    i4_surface = sprite_get_pixels(i4_sprite);
    brew_surface = sprite_get_pixels(brew_sprite);

    load_background();
    for (int i = 0; i < MAX_OBJECTS; i++)
        reset_object(&objects[i]);


    while (1) {
        render_frame();
        handle_input();
        update_objects();
    }
}
