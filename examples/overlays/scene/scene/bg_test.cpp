#include <libdragon.h>
#include "bg_test.h"

BGTest::BGTest()
{
    m_background.SetImage("rom:/bg_test.sprite");
}

BGTest::~BGTest()
{
    
}

void BGTest::Update()
{
    
}

void BGTest::Draw()
{
    rdpq_set_mode_standard();
    m_background.Draw();
}

SCENE_DEFINE_NEW_FUNC(BGTest);