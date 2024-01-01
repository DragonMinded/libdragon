#include <libdragon.h>
#include <stdlib.h>
#include "actor.h"

#define MAX_ACTORS 24
#define MAX_ACTOR_TYPES 3

typedef struct actor_info_s {
    const char *name;
    const char *sprite_path;
    const char *ovl_path;
} actor_info_t;

static actor_info_t actor_info[MAX_ACTOR_TYPES] = {
    { "circle", "rom:/circle.sprite", "rom:/circle.dso" },
    { "triangle", "rom:/triangle.sprite", "rom:/triangle.dso" },
    { "n64brew", "rom:/n64brew.sprite", "rom:/n64brew.dso" },
};

static actor_t *actors[MAX_ACTORS];

static int find_free_actor()
{
    //Search for free actor slot
    for(int i=0; i<MAX_ACTORS; i++) {
        if(!actors[i]) {
            //Found free actor slot
            return i;
        }
    }
    //Return sentinel value if no free actor slot exists
    return -1;
}

static void create_actor(int type, float x, float y)
{
    if(type < MAX_ACTOR_TYPES) {
        void *ovl_handle;
        actor_class_t *class;
        //Try to allocate actor
        int slot = find_free_actor();
        if(slot == -1) {
            //Return if impossible
            return;
        }
        ovl_handle = dlopen(actor_info[type].ovl_path, RTLD_LOCAL);
        class = dlsym(ovl_handle, "actor_class");
        assertf(class, "Failed to find actor class for actor %s", actor_info[type].name);
        actors[slot] = calloc(1, class->instance_size); //Allocate actor instance
        //Setup actor global properties
        actors[slot]->ovl_handle = ovl_handle;
        actors[slot]->update = class->update;
        //Setup sprite graphics
        actors[slot]->sprite = sprite_load(actor_info[type].sprite_path);
        actors[slot]->x = x;
        actors[slot]->y = y;
        actors[slot]->x_scale = actors[slot]->y_scale = 1.0f;
        actors[slot]->visible = true;
        class->init(actors[slot]);
    }
}

static void draw_actors()
{
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    for(int i=0; i<MAX_ACTORS; i++) {
        if(actors[i] && actors[i]->visible) {
            //Blit sprite surface to screen
            surface_t surf = sprite_get_pixels(actors[i]->sprite);
            rdpq_tex_blit(&surf, actors[i]->x, actors[i]->y, &(rdpq_blitparms_t){
                .cx = surf.width/2, .cy = surf.height/2,
                .scale_x = actors[i]->x_scale, .scale_y = actors[i]->y_scale,
                .theta = actors[i]->angle
            });
        }
    }
}

static void update_actors(joypad_buttons_t keys)
{
    for(int i=0; i<MAX_ACTORS; i++) {
        if(actors[i]) {
            if(!actors[i]->update(actors[i], keys)) {
                //Free up actor resources
                dlclose(actors[i]->ovl_handle);
                sprite_free(actors[i]->sprite);
                //Free actor instance
                free(actors[i]);
                actors[i] = NULL;
            }
        }
    }
}

int main()
{
    float scr_width;
    float scr_height;
    //Init debug log
    debug_init_isviewer();
    debug_init_usblog();
    //Init rendering
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    rdpq_debug_start();
    scr_width = display_get_width();
    scr_height = display_get_height();
    //Init miscellaneous system
    dfs_init(DFS_DEFAULT_LOCATION);
    joypad_init();
    //Setup scene
    create_actor(2, scr_width/2, scr_height/2);
    while(1) {
        surface_t *disp;
        //Update controller
        joypad_poll();
        joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        //Do actor spawning
        if(keys.a) {
            //Spawn a random actor somewhere in the middle 80% of the screen
            float pos_x = (((float)rand()/RAND_MAX)*(scr_width*0.8f))+(scr_width*0.1f);
            float pos_y = (((float)rand()/RAND_MAX)*(scr_height*0.8f))+(scr_height*0.1f);
            create_actor(rand()%MAX_ACTOR_TYPES, pos_x, pos_y);
        }
        //Update actors
        update_actors(keys);
        //Clear display
        disp = display_get();
        rdpq_attach_clear(disp, NULL);
        //Render actors
        rdpq_set_mode_standard();
        draw_actors();
        //Finish frame
        rdpq_detach_show();
    }
}