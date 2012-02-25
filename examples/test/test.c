#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

int filesize( FILE *pFile )
{
    fseek( pFile, 0, SEEK_END );
    int lSize = ftell( pFile );
    rewind( pFile );

    return lSize;
}

sprite_t *read_sprite( const char * const spritename )
{
    FILE *fp = fopen( spritename, "r" );

    if( fp )
    {
        sprite_t *sp = malloc( filesize( fp ) );
        fread( sp, 1, filesize( fp ), fp );
        fclose( fp );

        return sp;
    }
    else
    {
        return 0;
    }
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    dfs_init( DFS_DEFAULT_LOCATION );

    /* Read in sprite */
    sprite_t *mario = read_sprite( "rom://mario.sprite" );
    sprite_t *mariotrans = read_sprite( "rom://mariotrans.sprite" ); 
    sprite_t *mario16 = read_sprite( "rom://mario16.sprite" );
    sprite_t *mariotrans16 = read_sprite( "rom://mariotrans16.sprite" );

    /* Alpha test sprites */
    sprite_t *red = read_sprite( "rom://red.sprite" );
    sprite_t *green = read_sprite( "rom://green.sprite" );
    sprite_t *blue = read_sprite( "rom://blue.sprite" );

    /* Trans test sprites */
    sprite_t *red16 = read_sprite( "rom://red16.sprite" );
    sprite_t *green16 = read_sprite( "rom://green16.sprite" );
    sprite_t *blue16 = read_sprite( "rom://blue16.sprite" );

    /* Main loop test */
    while(1) 
    {
        char tStr[256];
        static display_context_t disp = 0;

        /* Grab a render buffer */
        while( !(disp = display_lock()) );
       
        /*Fill the screen */
        graphics_fill_screen( disp, 0 );

        sprintf(tStr, "Video mode: %lu\n", *((uint32_t *)0x80000300));
        graphics_draw_text( disp, 20, 20, tStr );
        sprintf(tStr, "Status: %08lX\n", *((uint32_t *)0xa4400000));
        graphics_draw_text( disp, 20, 30, tStr );

        /* Full bright color */
        graphics_draw_box(disp, 20, 40, 20, 20, graphics_make_color(255, 0, 0, 255));
        graphics_draw_box(disp, 50, 40, 20, 20, graphics_make_color(0, 255, 0, 255));
        graphics_draw_box(disp, 80, 40, 20, 20, graphics_make_color(0, 0, 255, 255));
        graphics_draw_box(disp, 110, 40, 20, 20, graphics_make_color(255, 255, 255, 255));

        /* 2/3 bright color */
        graphics_draw_box(disp, 20, 60, 20, 20, graphics_make_color(171, 0, 0, 255));
        graphics_draw_box(disp, 50, 60, 20, 20, graphics_make_color(0, 171, 0, 255));
        graphics_draw_box(disp, 80, 60, 20, 20, graphics_make_color(0, 0, 171, 255));
        graphics_draw_box(disp, 110, 60, 20, 20, graphics_make_color(171, 171, 171, 255));

        /* 1/3 bright color */
        graphics_draw_box(disp, 20, 80, 20, 20, graphics_make_color(85, 0, 0, 255));
        graphics_draw_box(disp, 50, 80, 20, 20, graphics_make_color(0, 85, 0, 255));
        graphics_draw_box(disp, 80, 80, 20, 20, graphics_make_color(0, 0, 85, 255));
        graphics_draw_box(disp, 110, 80, 20, 20, graphics_make_color(85, 85, 85, 255));

        /* Shade box */
        for(int i = 0; i < 256; i++)
        {
            graphics_draw_box(disp, 20 + i, 120, 1, 20, graphics_make_color(i, i, i, 255));
        }

        /* Display sprite (16bpp ones will only display in 16bpp mode, same with 32bpp) */
        graphics_draw_sprite( disp, 20, 150, mario );
        graphics_draw_sprite_trans( disp, 150, 150, mariotrans );
        
        graphics_draw_sprite( disp, 20, 150, mario16 );
        graphics_draw_sprite_trans( disp, 150, 150, mariotrans16 );

        /* 32BPP alpha blending test */
        graphics_draw_sprite_trans( disp, 150, 20, red );
        graphics_draw_sprite_trans( disp, 170, 20, green );
        graphics_draw_sprite_trans( disp, 160, 30, blue );

        /* 16BPP trans test */
        graphics_draw_sprite_trans( disp, 150, 20, red16 );
        graphics_draw_sprite_trans( disp, 170, 20, green16 );
        graphics_draw_sprite_trans( disp, 160, 30, blue16 );

        display_show(disp);

        /* Do we need to switch video displays? */
        controller_scan();
        struct controller_data keys = get_keys_down();

        if( keys.c[0].up )
        {
            display_close();

            res = RESOLUTION_640x480;
            display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
        }

        if( keys.c[0].down )
        {
            display_close();

            res = RESOLUTION_320x240;
            display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
        }

        if( keys.c[0].left )
        {
            display_close();

            bit = DEPTH_16_BPP;
            display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
        }

        if( keys.c[0].right )
        {
            display_close();

            bit = DEPTH_32_BPP;
            display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
        }
    }
}
