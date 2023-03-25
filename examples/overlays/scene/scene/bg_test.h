#ifndef BG_TEST_H
#define BG_TEST_H

#include "../scene.h"
#include "../background.h"

class BGTest : public SceneBase {
public:
    BGTest();
    ~BGTest();
    
public:
    void Update();
    void Draw();
    
private:
    void UpdateBackground();
    
private:
    Background m_background;
    float m_pos_x;
    float m_pos_y;
    float m_zoom;
};

#endif