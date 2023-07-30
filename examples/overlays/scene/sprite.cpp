#include <libdragon.h>
#include "sprite.h"

Sprite::Sprite()
{
    //Initialize image as non-existent and not owned by this class
    m_image = NULL;
    m_image_owned = false;
    //Initialize transform
    m_pos_x = m_pos_y = 0;
    m_scale_x = m_scale_y = 1.0f;
    m_angle = 0.0f;
    //Initialize velocity
    m_vel_x = m_vel_y = 0.0f;
}

Sprite::~Sprite()
{
    FreeImage();
}

void Sprite::FreeImage()
{
    //Only free image if owned by this class and it exists
    if(m_image_owned && m_image) {
        sprite_free(m_image);
    }
}

void Sprite::Draw()
{
    //Get sprite surface
    surface_t surf = sprite_get_pixels(m_image);
    //Initialize blit parameters to rotate/scale around center
    rdpq_blitparms_t blit_params = {};
    blit_params.cx = surf.width/2;
    blit_params.cy = surf.height/2;
    blit_params.scale_x = m_scale_x;
    blit_params.scale_y = m_scale_y;
    blit_params.theta = m_angle;
    //Setup blitting
    rdpq_tex_blit(&surf, m_pos_x, m_pos_y, &blit_params);
}

void Sprite::SetPos(float x, float y)
{
    m_pos_x = x;
    m_pos_y = y;
}

void Sprite::SetScale(float x, float y)
{
    m_scale_x = x;
    m_scale_y = y;
}

void Sprite::SetVel(float x, float y)
{
    m_vel_x = x;
    m_vel_y = y;
}

void Sprite::SetAngle(float theta)
{
    m_angle = theta;
}

void Sprite::SetImage(const char *filename)
{
    //Free old image and load new image owned by this class
    FreeImage();
    m_image = sprite_load(filename);
    m_image_owned = true;
}

void Sprite::SetImage(sprite_t *image)
{
    //Free old image and mark new image as not owned by class
    FreeImage();
    m_image = image;
    m_image_owned = false;
}