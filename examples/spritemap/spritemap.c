#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static volatile uint32_t animcounter = 0;

void update_counter( int ovfl )
{
    animcounter++;
}

int main(void)
{
    int mode = 0;

    /* Initialize peripherals */
    display_init( RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE );
    dfs_init( DFS_DEFAULT_LOCATION );
    rdpq_init();
    joypad_init();
    timer_init();

    /* Read in single sprite */
    int fp = dfs_open("/mudkip.sprite");
    sprite_t *mudkip = malloc( dfs_size( fp ) );
    dfs_read( mudkip, 1, dfs_size( fp ), fp );
    dfs_close( fp );
    
    fp = dfs_open("/earthbound.sprite");
    sprite_t *earthbound = malloc( dfs_size( fp ) );
    dfs_read( earthbound, 1, dfs_size( fp ), fp );
    dfs_close( fp );

    fp = dfs_open("/plane.sprite");
    sprite_t *plane = malloc( dfs_size( fp ) );
    dfs_read( plane, 1, dfs_size( fp ), fp );
    dfs_close( fp );

    /* Kick off animation update timer to fire thirty times a second */
    new_timer(TIMER_TICKS(1000000 / 30), TF_CONTINUOUS, update_counter);

    /* Main loop test */
    while(1) 
    {
        surface_t* disp = display_get();
       
        /*Fill the screen */
        graphics_fill_screen( disp, 0xFFFFFFFF );

        /* Set the text output color */
        graphics_set_color( 0x0, 0xFFFFFFFF );
    
        switch( mode )
        {
            case 0:
                /* Software spritemap test */
                graphics_draw_text( disp, 20, 20, "Software spritemap test" );

                /* Display a stationary sprite of adequate size to fit in TMEM */
                graphics_draw_sprite_trans( disp, 20, 50, plane );

                /* Display a stationary sprite to demonstrate backwards compatibility */
                graphics_draw_sprite_trans( disp, 50, 50, mudkip );

                /* Display walking NESS animation */
                graphics_draw_sprite_stride( disp, 20, 100, earthbound, ((animcounter / 15) & 1) ? 1: 0 );

                /* Display rotating NESS animation */
                graphics_draw_sprite_stride( disp, 50, 100, earthbound, ((animcounter / 8) & 0x7) * 2 );

                break;
            case 1:
            {
                /* Hardware spritemap test */
                /* This example demonstrates drawing sprites using the RDP module which involves
                   lower-level functions controlling the RDP (including manually making sure that
                   textures you draw can fit into TMEM).

                   For drawing using the higher-level RDPQ module, take a look at the rdpqdemo example. */
                graphics_draw_text( disp, 20, 20, "Hardware spritemap test" );

                /* Enable transparent sprite display instead of solid color fill */
                rdpq_set_mode_copy(true);

                /* Attach RDP to display; pass NULL as Z-buffer since we don't need it */
                rdpq_attach( disp, NULL );

                /* Load the sprite into texture slot 0, at the beginning of memory, without mirroring */
                rdp_load_texture( 0, 0, MIRROR_DISABLED, plane );
                
                /* Display a stationary sprite of adequate size to fit in TMEM */
                rdp_draw_sprite( 0, 20, 50, MIRROR_DISABLED );

                /* Since the RDP is very very limited in texture memory, we will use the spritemap feature to display
                   all four pieces of this sprite individually in order to use the RDP at all */
                for( int i = 0; i < 4; i++ )
                {
                    /* Load the sprite into texture slot 0, at the beginning of memory, without mirroring */
                    rdp_load_texture_stride( 0, 0, MIRROR_DISABLED, mudkip, i );
                
                    /* Display a stationary sprite to demonstrate backwards compatibility */
                    rdp_draw_sprite( 0, 50 + (20 * (i % 2)), 50 + (20 * (i / 2)), MIRROR_DISABLED );
                }

                /* Load the sprite into texture slot 0, at the beginning of memory, without mirroring */
                rdp_load_texture_stride( 0, 0, MIRROR_DISABLED, earthbound, ((animcounter / 15) & 1) ? 1: 0 );
                
                /* Display walking NESS animation */
                rdp_draw_sprite( 0, 20, 100, MIRROR_DISABLED );

                /* Load the sprite into texture slot 0, at the beginning of memory, without mirroring */
                rdp_load_texture_stride( 0, 0, MIRROR_DISABLED, earthbound, ((animcounter / 8) & 0x7) * 2 );
                
                /* Display rotating NESS animation */
                rdp_draw_sprite( 0, 50, 100, MIRROR_DISABLED );

                /* Inform the RDP we are finished drawing and that any pending operations should be flushed */
                rdpq_detach_wait();

                break;
            }
        }

        /* Force backbuffer flip */
        display_show(disp);

        /* Do we need to switch video displays? */
        joypad_poll();
        joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if( keys.a )
        {
            /* Lazy switching */
            mode = 1 - mode;
        }
    }
}
