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
    Background m_background;
};

#endif