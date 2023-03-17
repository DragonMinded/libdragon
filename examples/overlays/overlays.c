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
    { "circle", "rom:/circle.sprite", "rom:/circle.uso" },
    { "triangle", "rom:/triangle.sprite", "rom:/triangle.uso" },
    { "n64brew", "rom:/n64brew.sprite", "rom:/n64brew.uso" },
};

static actor_t *actors[MAX_ACTORS];

static int find_free_actor()
{
    for(int i=0; i<MAX_ACTORS; i++) {
        if(!actors[i]) {
            return i;
        }
    }
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
        class->init(actors[slot]);
    }
}

static void draw_actors()
{
    for(int i=0; i<MAX_ACTORS; i++) {
        if(actors[i]) {
            surface_t surf = sprite_get_pixels(actors[i]->sprite);
            rdpq_tex_blit(&surf, actors[i]->x, actors[i]->y, &(rdpq_blitparms_t){
                .cx = surf.width/2, .cy = surf.height/2,
                .scale_x = actors[i]->x_scale, .scale_y = actors[i]->y_scale,
                .theta = actors[i]->angle
            });
        }
    }
}

static void update_actors(struct controller_data keys)
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
    //Init debug log
    debug_init_isviewer();
    debug_init_usblog();
    //Init rendering
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    rdpq_init();
    rdpq_debug_start();
    //Init miscellaneous system
    dfs_init(DFS_DEFAULT_LOCATION);
    controller_init();
    //Setup scene
    create_actor(0, 160, 120);
    while(1) {
        surface_t *disp;
        struct controller_data keys;
        controller_scan();
        keys = get_keys_down();
        update_actors(keys);
        disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rdpq_set_mode_standard();
        draw_actors();
        rdpq_detach_show();
    }
}