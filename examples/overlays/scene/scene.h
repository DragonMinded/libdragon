#ifndef SCENE_H
#define SCENE_H

#include <string>

class SceneBase {
public:
    SceneBase();
    virtual ~SceneBase();
    
public:
    virtual void Draw() = 0;
    virtual void Update() = 0;
};

typedef SceneBase *(*SceneNewFunc)();

namespace SceneMgr {
    void SetNextScene(std::string);
    void Update();
    void Draw();
    bool ChangingScene();
    void LoadNextScene();
};

#define SCENE_DEFINE_NEW_FUNC(class_name) \
    static SceneBase *new_scene() { \
        return new class_name(); \
    } \
    SceneNewFunc new_func = new_scene;
    
#endif