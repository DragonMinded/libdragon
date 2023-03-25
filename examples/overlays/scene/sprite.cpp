#include <libdragon.h>
#include "sprite.h"


Sprite::Sprite()
{
    m_image = NULL;
    m_image_owned = false;
    m_pos_x = m_pos_y = 0;
    m_scale_x = m_scale_y = 1.0f;
    m_angle = 0.0f;
}

Sprite::~Sprite()
{
    FreeImage();
}

void Sprite::FreeImage()
{
    if(m_image_owned && m_image) {
        sprite_free(m_image);
    }
}

void Sprite::Draw()
{
    surface_t surf = sprite_get_pixels(m_image);
    rdpq_blitparms_t blit_params = {};
    blit_params.cx = surf.width/2;
    blit_params.cy = surf.height/2;
    blit_params.scale_x = m_scale_x;
    blit_params.scale_y = m_scale_y;
    blit_params.theta = m_angle;
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

void Sprite::SetAngle(float theta)
{
    m_angle = theta;
}

void Sprite::SetImage(const char *filename)
{
    FreeImage();
    m_image = sprite_load(filename);
    m_image_owned = true;
}

void Sprite::SetImage(sprite_t *image)
{
    FreeImage();
    m_image = image;
    m_image_owned = false;
}