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
    const float CENTER_MOVE_SPEED = 1.3f;
    const int CENTER_MARGIN_W = 12;
    const int CENTER_MARGIN_H = 12;
    const float ZOOM_SPEED = 0.995f;
    const float ZOOM_MIN = 0.25f;
    const float ZOOM_MAX = 4.0f;
    const float MOVE_SPEED = 0.03f;
    const int STICK_DEADZONE = 6;
    
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