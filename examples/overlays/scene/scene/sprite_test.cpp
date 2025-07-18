#include <libdragon.h>
#include "sprite_test.h"

//List of sprite filenames
const std::string sprite_filenames[SpriteTest::NUM_SPRITE_IMAGES] = {
    "rom:/ball_rectangle.sprite",
    "rom:/ball_star.sprite",
    "rom:/ball_triangle.sprite"
};

static float RandFloat(float min, float max)
{
    //Calculate rand normalized value
    float normalized_value = (float)rand()/RAND_MAX;
    //Move normalized value into range
    return (normalized_value*(max-min))+min;
}

SpriteTest::SpriteTest()
{
    //Load background image
    m_background.SetImage("rom:/bg_tiles.sprite");
    //Load sprite images
    for(int i=0; i<NUM_SPRITE_IMAGES; i++) {
        m_images[i] = sprite_load(sprite_filenames[i].c_str());
    }
    //Mark as there being 0 sprites present
    m_num_sprites = 0;
}

SpriteTest::~SpriteTest()
{
    //Free all sprite images
    for(int i=0; i<NUM_SPRITE_IMAGES; i++) {
        sprite_free(m_images[i]);
    }
}

void SpriteTest::Update()
{
    joypad_buttons_t cont_data = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    if(cont_data.start) {
        SceneMgr::SetNextScene("bg_test");
        return;
    }
    //Add new sprite when pressing A
    if(cont_data.a && m_num_sprites < MAX_SPRITES) {
        SpawnSprite();
    }
    //Remove last sprite when pressing B
    if(cont_data.b && m_num_sprites > 0) {
        m_num_sprites--;
    }
    UpdateSprites();
}

void SpriteTest::Draw()
{
    //Draw background
    m_background.Draw();
    //Draw all loaded sprites
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    for(int i=0; i<m_num_sprites; i++) {
        m_sprites[i].Draw();
    }
}

void SpriteTest::SpawnSprite()
{
    Sprite *new_sprite = &m_sprites[m_num_sprites++];
    //Generate new position for sprite
    new_sprite->m_pos_x = RandFloat(SPRITE_WIDTH/2, display_get_width()-(SPRITE_WIDTH/2));
    new_sprite->m_pos_y = RandFloat(SPRITE_HEIGHT/2, display_get_height()-(SPRITE_HEIGHT/2));
    //Generate new velocity for sprite
    new_sprite->m_vel_x = RandFloat(MIN_SPAWN_VEL, MAX_SPAWN_VEL);
    new_sprite->m_vel_y = RandFloat(MIN_SPAWN_VEL, MAX_SPAWN_VEL);
    //Reset sprite angle
    new_sprite->m_angle = 0.0f;
    //Assign random image to sprite
    new_sprite->SetImage(m_images[rand() % NUM_SPRITE_IMAGES]);
}

void SpriteTest::UpdateSprites()
{
    //Grab screen size
    float scr_width = display_get_width();
    float scr_height = display_get_height();
    //Iterate over valid sprites
    for(int i=0; i<m_num_sprites; i++) {
        //Update sprite position
        m_sprites[i].m_pos_x += m_sprites[i].m_vel_x;
        m_sprites[i].m_pos_y += m_sprites[i].m_vel_y;
        //Update sprite angle
        m_sprites[i].m_angle += ROT_SPEED;
        //Clamp sprite position to screen boundaries
        if(m_sprites[i].m_pos_x < SPRITE_WIDTH/2) {
            m_sprites[i].m_pos_x = SPRITE_WIDTH/2;
            m_sprites[i].m_vel_x = -m_sprites[i].m_vel_x;
        }
        if(m_sprites[i].m_pos_x > scr_width-(SPRITE_WIDTH/2)) {
            m_sprites[i].m_pos_x = scr_width-(SPRITE_WIDTH/2);
            m_sprites[i].m_vel_x = -m_sprites[i].m_vel_x;
        }
        if(m_sprites[i].m_pos_y < SPRITE_HEIGHT/2) {
            m_sprites[i].m_pos_y = SPRITE_HEIGHT/2;
            m_sprites[i].m_vel_y = -m_sprites[i].m_vel_y;
        }
        if(m_sprites[i].m_pos_y > scr_height-(SPRITE_HEIGHT/2)) {
            m_sprites[i].m_pos_y = scr_height-(SPRITE_HEIGHT/2);
            m_sprites[i].m_vel_y = -m_sprites[i].m_vel_y;
        }
    }
}

//Define function to generate BGTest instance
SCENE_DEFINE_NEW_FUNC(SpriteTest);