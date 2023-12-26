#ifndef SCENE_H
#define SCENE_H

#include <string>

//Scene base class
class SceneBase {
public:
    SceneBase();
    virtual ~SceneBase();
    
public:
    virtual void Draw() = 0;
    virtual void Update() = 0;
};

//Typedef for new scene instance function pointer
typedef SceneBase *(*SceneNewFunc)();

namespace SceneMgr {
    void Init(); //Load common scene overlay
    void SetNextScene(std::string); //Set new scene to load on LoadNextScene
    void Update(); //Update current scene
    void Draw(); //Draw current scene
    bool ChangingScene(); //Return whether a scene change is pending
    void LoadNextScene(); //Load pending scene
};

//Define function for allocating new scene instance 
#define SCENE_DEFINE_NEW_FUNC(class_name) \
    static SceneBase *new_scene() { \
        return new class_name(); \
    } \
    SceneNewFunc __attribute__((__visibility__("default"))) new_func = new_scene;
    
#endif