#include <libdragon.h>
#include <math.h>
#include "background.h"

static float fmod_positive(float x, float y)
{
    float modulo = fmodf(x, y);
    //Adjust for negative input
    if(modulo < 0) {
        modulo += y;
    }
    return modulo;
}

Background::Background()
{
    //Initialize image properties
    m_image = NULL;
    m_pos_x = m_pos_y = 0.0f;
    m_scale_x = m_scale_y = 1.0f;
}

Background::~Background()
{
    FreeImage();
}

void Background::Draw()
{
    //Initialize surface and blit parameters
    surface_t img_surface = sprite_get_pixels(m_image);
    rdpq_blitparms_t blit_params = {.scale_x = m_scale_x, .scale_y = m_scale_y };
    //Get screen size
    float scr_width = display_get_width();
    float scr_height = display_get_height();
    //Calculate tile screen size
    float tile_w = img_surface.width*m_scale_x;
    float tile_h = img_surface.height*m_scale_y;
    //Calculate number of visible tiles (+2 is for potentially offscreen tiles)
    int num_tiles_x = (scr_width/tile_w)+2;
    int num_tiles_y = (scr_height/tile_h)+2;
    //Calculate screen offset of top-left tile
    float ofs_x = -fmod_positive(m_pos_x, img_surface.width)*m_scale_x;
    float ofs_y = -fmod_positive(m_pos_y, img_surface.height)*m_scale_y;
    //Iterate over visible tiles
    for(int i=0; i<num_tiles_y; i++) {
        for(int j=0; j<num_tiles_x; j++) {
            rdpq_tex_blit(&img_surface, ofs_x+(j*tile_w), ofs_y+(i*tile_h), &blit_params);
        }
    }
}

void Background::SetPos(float x, float y)
{
    m_pos_x = x;
    m_pos_y = y;
}

void Background::SetScale(float x, float y)
{
    m_scale_x = x;
    m_scale_y = y;
}

void Background::SetImage(const char *filename)
{
    //Free previous image and load new image
    FreeImage();
    m_image = sprite_load(filename);
}

void Background::FreeImage()
{
    //Free if image has been assigned
    if(m_image) {
        sprite_free(m_image);
    }
}