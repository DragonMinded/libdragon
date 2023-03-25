#include <libdragon.h>
#include "bg_test.h"

BGTest::BGTest()
{
    m_background.SetImage("rom:/bg_test.sprite");
    m_crosshair.SetImage("rom:/crosshair.sprite");
    m_pos_x = m_pos_y = 0.0f;
    m_center_pos_x = display_get_width()/2;
    m_center_pos_y = display_get_height()/2;
    m_zoom = 1.0f;
}

BGTest::~BGTest()
{
    
}

void BGTest::UpdateZoom()
{
    struct controller_data cont_data = get_keys_held();
    float new_zoom = m_zoom;
    if(cont_data.c[0].L) {
        new_zoom *= ZOOM_SPEED;
    }
    if(cont_data.c[0].R) {
        new_zoom /= ZOOM_SPEED;
    }
    if(new_zoom < ZOOM_MIN) {
        new_zoom = ZOOM_MIN;
    }
    if(new_zoom > ZOOM_MAX) {
        new_zoom = ZOOM_MAX;
    }
    m_zoom = new_zoom;
}

void BGTest::UpdatePos()
{
    struct controller_data cont_data = get_keys_held();
    int8_t stick_x = cont_data.c[0].x;
    int8_t stick_y = cont_data.c[0].y;
    if(abs(stick_x) >= STICK_DEADZONE) {
        m_pos_x += stick_x*MOVE_SPEED/m_zoom;
    }
    if(abs(stick_y) >= STICK_DEADZONE) {
        m_pos_y -= stick_y*MOVE_SPEED/m_zoom;
    }
}

void BGTest::UpdateCenterPos()
{
    struct controller_data cont_data = get_keys_held();
    if(cont_data.c[0].C_up) {
        m_center_pos_y -= CENTER_MOVE_SPEED;
        m_pos_y -= CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_y < CENTER_MARGIN_H) {
            m_center_pos_y = CENTER_MARGIN_H;
        }
    }
    if(cont_data.c[0].C_down) {
        m_center_pos_y += CENTER_MOVE_SPEED;
        m_pos_y += CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_y > display_get_height()-CENTER_MARGIN_H) {
            m_center_pos_y = display_get_height()-CENTER_MARGIN_H;
        }
    }
    if(cont_data.c[0].C_left) {
        m_center_pos_x -= CENTER_MOVE_SPEED;
        m_pos_x -= CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_x < CENTER_MARGIN_W) {
            m_center_pos_x = CENTER_MARGIN_W;
        }
    }
    if(cont_data.c[0].C_right) {
        m_center_pos_x += CENTER_MOVE_SPEED;
        m_pos_x += CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_x > display_get_width()-CENTER_MARGIN_W) {
            m_center_pos_x = display_get_width()-CENTER_MARGIN_W;
        }
    }
    m_crosshair.SetPos(m_center_pos_x, m_center_pos_y);
}

void BGTest::UpdateBackground()
{
    float pos_x = m_pos_x-(m_center_pos_x/m_zoom);
    float pos_y = m_pos_y-(m_center_pos_y/m_zoom);
    m_background.SetPos(pos_x, pos_y);
    m_background.SetScale(m_zoom, m_zoom);
}

void BGTest::Update()
{
    struct controller_data cont_data = get_keys_down();
    if(cont_data.c[0].start) {
        SceneMgr::SetNextScene("sprite_test");
    }
    UpdateZoom();
    UpdatePos();
    UpdateCenterPos();
    UpdateBackground();
}

void BGTest::Draw()
{
    rdpq_set_mode_standard();
    m_background.Draw();
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    m_crosshair.Draw();
}

SCENE_DEFINE_NEW_FUNC(BGTest);