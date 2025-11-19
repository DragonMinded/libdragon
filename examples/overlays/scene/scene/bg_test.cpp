#include <libdragon.h>
#include "bg_test.h"

BGTest::BGTest()
{
    //Load background and crosshair images
    m_background.SetImage("rom:/bg_test.sprite");
    m_crosshair.SetImage("rom:/crosshair.sprite");
    //Setup origin in top-left
    m_pos_x = m_pos_y = 0.0f;
    //Setup center position in screen center
    m_center_pos_x = display_get_width()/2;
    m_center_pos_y = display_get_height()/2;
    //Setup zoom as normal zoom
    m_zoom = 1.0f;
}

BGTest::~BGTest()
{
    
}

void BGTest::UpdateZoom()
{
    joypad_buttons_t cont_data = joypad_get_buttons_held(JOYPAD_PORT_1);
    //Calculate next zoom (exponential)
    float new_zoom = m_zoom;
    if(cont_data.l) {
        //Zoom out
        new_zoom *= ZOOM_SPEED;
    }
    if(cont_data.r) {
        //Zoom in
        new_zoom /= ZOOM_SPEED;
    }
    //Clamp zoom
    if(new_zoom < ZOOM_MIN) {
        new_zoom = ZOOM_MIN;
    }
    if(new_zoom > ZOOM_MAX) {
        new_zoom = ZOOM_MAX;
    }
    //Update zoom
    m_zoom = new_zoom;
}

void BGTest::UpdatePos()
{
    joypad_inputs_t cont_data = joypad_get_inputs(JOYPAD_PORT_1);
    //Move by analog stick position
    int8_t stick_x = cont_data.stick_x;
    int8_t stick_y = cont_data.stick_y;
    if(abs(stick_x) >= STICK_DEADZONE) {
        m_pos_x += stick_x*MOVE_SPEED/m_zoom;
    }
    if(abs(stick_y) >= STICK_DEADZONE) {
        m_pos_y -= stick_y*MOVE_SPEED/m_zoom;
    }
}

void BGTest::UpdateCenterPos()
{
    joypad_buttons_t cont_data = joypad_get_buttons_held(JOYPAD_PORT_1);
    if(cont_data.c_up) {
        //Move center position up
        m_center_pos_y -= CENTER_MOVE_SPEED;
        m_pos_y -= CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_y < CENTER_MARGIN_H) {
            m_center_pos_y = CENTER_MARGIN_H;
        }
    }
    if(cont_data.c_down) {
        //Move center position down
        m_center_pos_y += CENTER_MOVE_SPEED;
        m_pos_y += CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_y > display_get_height()-CENTER_MARGIN_H) {
            m_center_pos_y = display_get_height()-CENTER_MARGIN_H;
        }
    }
    if(cont_data.c_left) {
        //Move center position left
        m_center_pos_x -= CENTER_MOVE_SPEED;
        m_pos_x -= CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_x < CENTER_MARGIN_W) {
            m_center_pos_x = CENTER_MARGIN_W;
        }
    }
    if(cont_data.c_right) {
        //Move center position right
        m_center_pos_x += CENTER_MOVE_SPEED;
        m_pos_x += CENTER_MOVE_SPEED/m_zoom;
        if(m_center_pos_x > display_get_width()-CENTER_MARGIN_W) {
            m_center_pos_x = display_get_width()-CENTER_MARGIN_W;
        }
    }
    //Update crosshair position
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
    //Load next scene if start is pressed
    joypad_buttons_t cont_data = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    if(cont_data.start) {
        SceneMgr::SetNextScene("sprite_test");
        return;
    }
    //Update scene
    UpdateZoom();
    UpdatePos();
    UpdateCenterPos();
    UpdateBackground();
}

void BGTest::Draw()
{
    //Draw background
    m_background.Draw();
    //Draw crosshair blended
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    m_crosshair.Draw();
}

//Define function to generate BGTest instance
SCENE_DEFINE_NEW_FUNC(BGTest);