#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

int main(void)
{
    /* Initialize peripherals */
    display_init( RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    dfs_init( DFS_DEFAULT_LOCATION );
    rdp_init();
    controller_init();
    timer_init();

    /* Read in the custom font */
    int fp = dfs_open("/libdragon-font.sprite");
    sprite_t *custom_font = malloc( dfs_size( fp ) );
    dfs_read( custom_font, 1, dfs_size( fp ), fp );
    dfs_close( fp );

    static display_context_t disp = 0;

    /* Grab a render buffer */
    while( !(disp = display_lock()) );
    
    /*Fill the screen */
    graphics_fill_screen( disp, 0x0 );

    /* Set the text output color */
    graphics_set_color( 0xFFFFFFFF, 0x0 );

    graphics_draw_text( disp, 20, 20, "Using default font!" );

    graphics_set_font_sprite( custom_font );

    graphics_draw_text( disp, 20, 40, "Using custom font!" );

    /* Force backbuffer flip */
    display_show(disp);

    
    /* Main loop test */
    while(1) 
    {
    }
}
