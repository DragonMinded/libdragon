#include <libdragon.h>
#include "scene.h"

int main()
{
    //Init debug log
    debug_init_isviewer();
    debug_init_usblog();
    //Init rendering
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    rdpq_init();
    rdpq_debug_start();
    //Init miscellaneous system
    dfs_init(DFS_DEFAULT_LOCATION);
    controller_init();
    SceneMgr::SetNextScene("bg_test");
    while(1) {
        SceneMgr::LoadNextScene();
        while(!SceneMgr::ChangingScene()) {
            controller_scan();
            SceneMgr::Update();
            surface_t *disp = display_get();
            rdpq_attach_clear(disp, NULL);
            SceneMgr::Draw();
            rdpq_detach_show();
        }
    }
}