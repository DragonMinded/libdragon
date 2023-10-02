/**
 * NOTE:
 * For necessary implementation steps, look at the "Step X/5" comments in this file.
 */

#include "libdragon.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>

#include "lib/microuiN64.h"
#include "demo-helper.h"

#define DEBUG_RDP 0

static int dpl_cube;
static float clear_color[3] = {0.05f, 0.05f, 0.05f};
static float hue = 0.42f;
static float box_rot = 0.0f;

#define SPRITE_COUNT 4
static char *sprite_names[SPRITE_COUNT] = {"test_1m", "test_2m", "test_5m", "test_10m"};
static sprite_t *sprites[SPRITE_COUNT] = {NULL, NULL, NULL, NULL};

static bool cube_visible = true;
static int capture_cube = 1;
static mu_Rect cube_win_rect = (mu_Rect){114, 110, 80, 80};
static int capture_screen = 1;
static int capture_count = 1;

static const surface_t *last_surface = NULL;
static uint32_t last_time_ms = 0;
static float time_delta_ms = 1000.0 / 60.0f;

void game_update()
{
    box_rot += 0.7f;

    mu64_set_mouse_speed(0.00004f * time_delta_ms); // keep cursor speed constant

    // You can create windows at any point in your game-logic.
    // This does not render the window directly, which is handled later in a single batch.

    // Basic window, you can add inputs to modify variables
    if (mu_begin_window_ex(&mu_ctx, "Settings", mu_rect(12, 20, 90, 140), MU_OPT_NOCLOSE))
    {
        mu_layout_row(&mu_ctx, 1, (int[]) { -1 }, 0);
        mu_label(&mu_ctx, "Background");

        if(mu_slider(&mu_ctx, &hue, 0.0f, 1.0f)) {
            rainbow(hue, &clear_color[0], &clear_color[1], &clear_color[2]);
        }

        if(mu_button(&mu_ctx, "Remove Screen")) {
            if(capture_count > 0)--capture_count;
        }
        if(mu_button(&mu_ctx, "Add Screen")) {
            ++capture_count;
        }

        if (mu_header_ex(&mu_ctx, "Time", MU_OPT_EXPANDED)) {
            float fps = display_get_fps();

            char fps_buffer[16] = {0};
            sprintf(fps_buffer, "FPS: %.4f", fps);
            mu_label(&mu_ctx, fps_buffer);

            sprintf(fps_buffer, "ms: %.4f", time_delta_ms);
            mu_label(&mu_ctx, fps_buffer);
        }

        mu_end_window(&mu_ctx);
    }

    // you can also temp. changes styles and request information about the window itself
    mu_Color oldColor = mu_ctx._style.colors[MU_COLOR_WINDOWBG];
    if(capture_cube) {
        mu_ctx._style.colors[MU_COLOR_WINDOWBG] = (mu_Color){0,0,0,0};    
    }

    if (mu_begin_window_ex(&mu_ctx, "3D-Cube", cube_win_rect, 0))
    {
        mu_draw_rect(&mu_ctx, (mu_Rect){cube_win_rect.x, cube_win_rect.y, cube_win_rect.w, 26}, oldColor);

        mu_layout_row(&mu_ctx, 1, (int[]) { -1 }, 0);    
        mu_checkbox(&mu_ctx, "In Window", &capture_cube);
        mu_end_window(&mu_ctx);
    }

    mu_ctx._style.colors[MU_COLOR_WINDOWBG] = oldColor;

    mu_Container *cube_cont = mu_get_container(&mu_ctx, "3D-Cube");
    cube_visible = cube_cont->open;
    cube_win_rect = cube_cont->rect;

    // Dynamic windows created in a loop:
    for(int i=0; i<capture_count; ++i) 
    {
        char name[32];
        sprintf(name, "Screen %d", i);

        if(mu_begin_window_ex(&mu_ctx, name, (mu_Rect){114+(i*4), 20+(i*4), 80, 80}, MU_OPT_NOCLOSE))
        {
            mu_Container *cont = mu_get_current_container(&mu_ctx);
            int menu_height = 26;
            mu_layout_row(&mu_ctx, 1, (int[]) {-1}, 0);    
            mu_checkbox(&mu_ctx, "Capture", &capture_screen);

            if(capture_screen) {
                mu_draw_surface(&mu_ctx, last_surface, (mu_Rect){
                    cont->rect.x, cont->rect.y + menu_height, 
                    cont->rect.w, cont->rect.h - menu_height
                });
            }
            
            mu_end_window(&mu_ctx);
        }
    }

    // Trees: can be fixed or dynamically created
    // Popups: opens a new temp. window at the cursor, closes on clicking somewhere else
    if (mu_begin_window_ex(&mu_ctx, "Files", (mu_Rect){208, 20, 100, 100}, 0))
    {
        mu_layout_row(&mu_ctx, 1, (int[]) { -1 }, 0);    
        char sprite_text_buff[32];
        int old_indent = mu_ctx._style.indent;

        if (mu_begin_treenode(&mu_ctx, "ROM")) 
        {
            if (mu_begin_treenode(&mu_ctx, "Sprites")) 
            {
                mu_ctx._style.indent = 14;
                for(int i=0; i<4; ++i) 
                {
                    sprite_t *sprite = sprites[i];
                    if (mu_begin_treenode(&mu_ctx, sprite_names[i])) 
                    {
                        sprintf(sprite_text_buff, "Size %dx%d", sprite->width, sprite->height);
                        mu_text(&mu_ctx, sprite_text_buff);
                        mu_text(&mu_ctx, tex_format_name(sprite_get_format(sprite)));
                        
                        if(mu_button(&mu_ctx, "Preview"))mu_open_popup(&mu_ctx, "Texture");

                        if (mu_begin_popup(&mu_ctx, "Texture")) {
                            mu_Container *cont = mu_get_current_container(&mu_ctx);
                            cont->rect.w = sprite->width;
                            cont->rect.h = sprite->height;
                            mu_draw_sprite(&mu_ctx, sprite, cont->rect);
                            mu_end_popup(&mu_ctx);
                        }
                        mu_end_treenode(&mu_ctx);
                    }
                }
                mu_end_treenode(&mu_ctx);
                mu_ctx._style.indent = old_indent;
            }
            mu_end_treenode(&mu_ctx);
        }
    
        mu_end_window(&mu_ctx);
    }
    mu_Container *files_cont = mu_get_container(&mu_ctx, "Files");

    // Fixed, static window
    if (mu_begin_window_ex(&mu_ctx, "Bar", mu_rect(0, display_get_height() -16, 320, 16), MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_NOCLOSE)) {
        mu_layout_begin_column(&mu_ctx);
        mu_layout_row(&mu_ctx, 4, (int[]) { 48, 48, 92, 130 }, 0);

        if(!cube_cont->open) {
            if(mu_button(&mu_ctx, "3D-Cube"))cube_cont->open = true;
        } else {
            mu_label(&mu_ctx, "");
        }

        if(!files_cont->open) {
            if(mu_button(&mu_ctx, "Files"))files_cont->open = true;
        } else {
            mu_label(&mu_ctx, "");
        }

        mu_label(&mu_ctx, "");
        mu_label(&mu_ctx, "(Press L to toggle UI)");

        mu_layout_end_column(&mu_ctx);
        mu_end_window(&mu_ctx);
    }
}

void game_draw()
{
    if(cube_visible) 
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glRotatef(box_rot, 0,1,0);
        glRotatef(box_rot*0.3f, 1,0,0);

        if(capture_cube) {
            glViewport(
                cube_win_rect.x, 
                display_get_height() - cube_win_rect.y - cube_win_rect.h,
                cube_win_rect.w, 
                cube_win_rect.h - 24
            );
        } else {
            glViewport(0,0, display_get_width(), display_get_height());
        }
        
        glCallList(dpl_cube);
        glPopMatrix();
    }
}

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    
    joypad_init();
    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
    rdpq_init();
    gl_init();
    
    #ifdef DEBUG_RDP
        rdpq_debug_start();
    #endif

    surface_t zbuffer = surface_alloc(FMT_RGBA16, display_get_width(), display_get_height());

    rainbow(hue, &clear_color[0], &clear_color[1], &clear_color[2]);
    dpl_cube = create_cube_dpl();

    char sprite_name_buff[32];
    for(int i=0; i<SPRITE_COUNT; ++i) {
        sprintf(sprite_name_buff, "rom:/%s.sprite", sprite_names[i]);
        sprites[i] = sprite_load(sprite_name_buff);
    }

    // Step 1/5: Make sure you have a small font loaded
    rdpq_font_t *font = rdpq_font_load("rom:/VCR_OSD_MONO.font64");
    uint8_t font_id = 1;
    rdpq_text_register_font(font_id, font);

    // Step 2/5: init UI libary, pass in the controller (joystick or N64-mouse) and font-id
    // (Note: take a look inside this function for styling and controls.)
    mu64_init(JOYPAD_PORT_1, font_id);

    float aspect_ratio = (float)display_get_width() / (float)display_get_height();
    float near_plane = 0.5f;
    float far_plane = 50.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-near_plane*aspect_ratio, near_plane*aspect_ratio, -near_plane, near_plane, near_plane, far_plane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        0, 1.8f, 1.8f,
        0, 0, 0,
        0, 1, 0
    );

    static const GLfloat env_color[] = {1,1,1,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, env_color);

    glDisable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    for(;;)
    {
        // Game logic
        joypad_poll();

        uint32_t now_ms = get_ticks_ms();
        time_delta_ms = now_ms - last_time_ms;
        last_time_ms = now_ms;

        mu64_start_frame();// Step 3/5: call this BEFORE your game logic starts each frame
        game_update();
        mu64_end_frame(); // Step 4/5: call this AFTER your game logic ends each frame

        // Game renderer
        rdpq_attach(display_get(), &zbuffer);

        gl_context_begin();
        glClearColor(clear_color[0], clear_color[1], clear_color[2], 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        game_draw(); // (your games renderer)
        
        gl_context_end();

        mu64_draw(); // Step 5/5: render out the UI at the very end

        last_surface = rdpq_get_attached();
        rdpq_detach_show();
    }
}
