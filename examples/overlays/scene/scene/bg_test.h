#ifndef BG_TEST_H
#define BG_TEST_H

#include "../scene.h"

class BGTest : public SceneBase {
public:
    BGTest();
    ~BGTest();
    
public:
    void Update();
    void Draw();
};

#endif