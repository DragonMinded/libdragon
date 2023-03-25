#include <libdragon.h>
#include "scene.h"

static void *scene_ovl;
static void *scene_common_ovl;
static SceneBase *curr_scene;
static std::string curr_scene_name;
static std::string next_scene_name;

SceneBase::SceneBase()
{
    
}

SceneBase::~SceneBase()
{
    
}

void SceneMgr::Init()
{
    scene_common_ovl = dlopen("rom:/scene_common.uso", RTLD_GLOBAL);
}

void SceneMgr::SetNextScene(std::string name)
{
    next_scene_name = name;
}

void SceneMgr::Update()
{
    curr_scene->Update();
}

void SceneMgr::Draw()
{
    curr_scene->Draw();
}

bool SceneMgr::ChangingScene()
{
    return curr_scene_name != next_scene_name;
}

void SceneMgr::LoadNextScene()
{
    delete curr_scene;
    if(scene_ovl) {
        dlclose(scene_ovl);
    }
    curr_scene_name = next_scene_name;
    std::string ovl_name = "rom:/scene/"+curr_scene_name+".uso";
    scene_ovl = dlopen(ovl_name.c_str(), RTLD_LOCAL);
    SceneNewFunc *new_func = (SceneNewFunc *)dlsym(scene_ovl, "new_func");
    assertf(new_func, "Cannot construct scene %s", curr_scene_name.c_str());
    curr_scene = (*new_func)();
}