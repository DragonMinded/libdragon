#ifndef SPRITE_TEST_H
#define SPRITE_TEST_H

#include "../scene.h"
#include "../background.h"
#include "../sprite.h"

class SpriteTest : public SceneBase {
public:
    SpriteTest();
    ~SpriteTest();
    
public:
    void Update();
    void Draw();
    
private:
    void SpawnSprite();
    void UpdateSprites();
    
public:
    enum {
        NUM_SPRITE_IMAGES = 3,
        MAX_SPRITES = 100
    };
    
private:
    const int SPRITE_WIDTH = 32;
    const int SPRITE_HEIGHT = 32;
    const float MIN_SPAWN_VEL = 1.0f;
    const float MAX_SPAWN_VEL = 2.0f;
    const float ROT_SPEED = 0.05f;
    
private:
    Background m_background;
    sprite_t *m_images[NUM_SPRITE_IMAGES];
    Sprite m_sprites[MAX_SPRITES];
    int m_num_sprites;
};

#endif