#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <libdragon.h>

class __attribute__((__visibility__("default"))) Background {
public:
    Background();
    ~Background();
    
public:
    void Draw();
    void SetPos(float x, float y);
    void SetScale(float x, float y);
    void SetImage(const char *filename);
    
private:
    void FreeImage();
    
private:
    sprite_t *m_image;
    float m_pos_x, m_pos_y;
    float m_scale_x, m_scale_y;
};

#endif