#include <libdragon.h>
#include <math.h>
#include "bg_test.h"

#define ZOOM_RATIO 0.995f
#define ZOOM_MIN 0.25f
#define ZOOM_MAX 4.0f
#define MOVE_SPEED 0.03f
#define STICK_DEADZONE 6

BGTest::BGTest()
{
    m_background.SetImage("rom:/bg_test.sprite");
    m_pos_x = m_pos_y = 0.0f;
    m_zoom = 1.0f;
}

BGTest::~BGTest()
{
    
}

void BGTest::UpdateBackground()
{
    float scr_width = display_get_width();
    float scr_height = display_get_height();
    m_background.SetPos(m_pos_x-((scr_width/2)/m_zoom), m_pos_y-((scr_height/2)/m_zoom));
    m_background.SetScale(m_zoom, m_zoom);
}

void BGTest::Update()
{
    struct controller_data cont_data = get_keys_held();
    int8_t stick_x = cont_data.c[0].x;
    int8_t stick_y = cont_data.c[0].y;
    float new_zoom = m_zoom;
    if(cont_data.c[0].L) {
        new_zoom *= ZOOM_RATIO;
    }
    if(cont_data.c[0].R) {
        new_zoom /= ZOOM_RATIO;
    }
    if(new_zoom < ZOOM_MIN) {
        new_zoom = ZOOM_MIN;
    }
    if(new_zoom > ZOOM_MAX) {
        new_zoom = ZOOM_MAX;
    }
    m_zoom = new_zoom;
    if(abs(stick_x) >= STICK_DEADZONE) {
        m_pos_x += stick_x*MOVE_SPEED/m_zoom;
    }
    if(abs(stick_y) >= STICK_DEADZONE) {
        m_pos_y -= stick_y*MOVE_SPEED/m_zoom;
    }
    UpdateBackground();
    
}

void BGTest::Draw()
{
    rdpq_set_mode_standard();
    m_background.Draw();
}

SCENE_DEFINE_NEW_FUNC(BGTest);