#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
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

mesh_t *create_cube() 
{
    mesh_t* mesh = malloc( sizeof(mesh_t) + (4 * 6) * sizeof(vertex_t) );

    // neg y
    int v = 0;
    mesh->vdata[v++].v = f4_set3(-1,-1,-1);
    mesh->vdata[v++].v = f4_set3(1,-1,-1);
    mesh->vdata[v++].v = f4_set3(1,-1,1);
    mesh->vdata[v++].v = f4_set3(-1,-1,1);
    
    // neg z
    mesh->vdata[v++].v = f4_set3(-1,-1,-1);
    mesh->vdata[v++].v = f4_set3(1,-1,-1);
    mesh->vdata[v++].v = f4_set3(1,1,-1);
    mesh->vdata[v++].v = f4_set3(-1,1,-1);
    
    // pos z
    mesh->vdata[v++].v = f4_set3(-1,-1,1);
    mesh->vdata[v++].v = f4_set3(1,-1,1);
    mesh->vdata[v++].v = f4_set3(1,1,1);
    mesh->vdata[v++].v = f4_set3(-1,1,1);
    
    // neg x
    mesh->vdata[v++].v = f4_set3(-1,-1,-1);
    mesh->vdata[v++].v = f4_set3(-1,-1,1);
    mesh->vdata[v++].v = f4_set3(-1,1,1);
    mesh->vdata[v++].v = f4_set3(-1,1,-1);
    
    // pos x
    mesh->vdata[v++].v = f4_set3(1,-1,-1);
    mesh->vdata[v++].v = f4_set3(1,-1,1);
    mesh->vdata[v++].v = f4_set3(1,1,1);
    mesh->vdata[v++].v = f4_set3(1,1,-1);
     
    // pos y
    mesh->vdata[v++].v = f4_set3(-1,1,-1);
    mesh->vdata[v++].v = f4_set3(1,1,-1);
    mesh->vdata[v++].v = f4_set3(1,1,1);
    mesh->vdata[v++].v = f4_set3(-1,1,1);

    mesh->vcount = v;

    // add tex coords
    for (int i = 0; i < v; i+=4)
    {
        mesh->vdata[i+0].t = 0;
        mesh->vdata[i+0].s = 0;
        mesh->vdata[i+1].t = 1;
        mesh->vdata[i+1].s = 0;
        mesh->vdata[i+2].t = 1;
        mesh->vdata[i+2].s = 1;
        mesh->vdata[i+3].t = 0;
        mesh->vdata[i+3].s = 1;
    }

    // index processing
    mesh->icount = (v / 4) * 6;    
    for (int i = 0; i < (v / 4); ++i)
    {
        int ver = i * 4;
        int id = i * 6;
        mesh->idata[id + 0] = ver + 0;
        mesh->idata[id + 1] = ver + 1;
        mesh->idata[id + 2] = ver + 2;
        mesh->idata[id + 3] = ver + 2;
        mesh->idata[id + 4] = ver + 3;
        mesh->idata[id + 5] = ver + 0;
    }
    
    return mesh;
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

    /* Initialize peripherals */
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    dfs_init( DFS_DEFAULT_LOCATION );
    rdp_init();

    /* Read in sprite */
    /* NOTE: For RDP textured triangles, images must be single, power of two, TLUT images. */
    sprite_t *mario = read_sprite( "rom://brick.sprite" );

    /* 3d mesh object */
    mesh_t* mesh = create_cube();    
    float rotate = 0.01f;

    /* setup matrix and camera information */
    float4 camera = f4_set3(1,3,1);
    matrix4 proj = m4_projection(3.14f / 2.0f, 1.333333f, 0.5f, 100.0f);

    /* Main loop test */
    while(1) 
    {
        char tStr[256];
        static display_context_t disp = 0;

        /* Grab a render buffer */
        while( !(disp = display_lock()) );
       
        /*Fill the screen */
        graphics_fill_screen( disp, 0 );
        graphics_set_color(  0xFFFFFFFF, 0x0 );

        /* test draw */
        graphics_draw_sprite( disp, 20, 150, mario );
        
        /* draw texture debug information */
        sprintf(tStr, "X: %d - Y: %d\n", mario->width , mario->height);
        graphics_draw_text( disp, 20, 40, tStr );
        sprintf(tStr, "bpp: %d\n", mario->bitdepth);
        graphics_draw_text( disp, 20, 50, tStr );

        /* setup camera for frame */
        rotate += 0.01f;
        if(rotate > 6.28f) rotate = 0.01f;
        matrix4 mat = m4_identity();
        m4_rotate(&mat, f4_set3(sin(rotate) * 3.14f,sin(rotate) * 3.14f,0));

        matrix4 view = m4_lookAt(camera,f4_zero(), f4_set3(0,1,0));
        matrix4 viewproj = m4_mul_m(proj, view);
        mat = m4_mul_m(viewproj, mat);

        /* draw mesh */
        rdp_sync( SYNC_PIPE );
        rdp_set_default_clipping();
        rdp_attach_display( disp );

        rdp_sync( SYNC_PIPE );
        rdp_set_tri_prim_color( 0xffffffff );
        rdp_texture_cycle(_1CYCLE, ATOMIC_PRIM | SAMPLE_TYPE | IMAGE_READ_EN /* | PERSP_TEX_EN */);
        rdp_load_texture( 0, 0, MIRROR_DISABLED, mario );
        rdp_draw_textured_mesh(0, mat, mesh);
        
        rdp_detach_display();

        display_show(disp);
    }
}
