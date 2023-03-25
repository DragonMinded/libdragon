#ifndef BG_TEST_H
#define BG_TEST_H

#include "../scene.h"
#include "../background.h"
#include "../sprite.h"

class BGTest : public SceneBase {
public:
    BGTest();
    ~BGTest();
    
public:
    void Update();
    void Draw();
    
private:
    void UpdateZoom();
    void UpdatePos();
    void UpdateCenterPos();
    void UpdateBackground();
    
private:
    Background m_background;
    Sprite m_crosshair;
    float m_pos_x;
    float m_pos_y;
    float m_center_pos_x;
    float m_center_pos_y;
    float m_zoom;
};

#endif