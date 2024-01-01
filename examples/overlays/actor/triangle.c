#include "actor.h"

#define SCALE_ANIM_ACCEL 0.001f
#define ROT_SPEED 0.005f

typedef struct triangle_actor_s {
    actor_t actor;
    float scale_vel;
    int vanish_timer;
    bool vanish;
} triangle_actor_t;

static void init(actor_t *actor)
{
    triangle_actor_t *this = (triangle_actor_t *)actor;
    this->scale_vel = 0.02f;
    this->vanish_timer = 120;
}

static void do_animation(triangle_actor_t *this)
{
    if(this->actor.x_scale > 1.0f) {
        this->scale_vel -= SCALE_ANIM_ACCEL;
    } else {
        this->scale_vel += SCALE_ANIM_ACCEL;
    }
    this->actor.x_scale += this->scale_vel;
    this->actor.y_scale = 1/this->actor.x_scale;
    this->actor.angle += ROT_SPEED;
}

static bool do_vanish(triangle_actor_t *this)
{
    if(this->vanish) {
        this->actor.visible = !this->actor.visible;
        if(--this->vanish_timer == 0) {
            //Make actor go away when timer expires
            return false;
        }
    }
    return true;
}

static bool update(actor_t *actor, joypad_buttons_t pressed_keys)
{
    triangle_actor_t *this = (triangle_actor_t *)actor;
    do_animation(this);
    //Activate vanish when pressing Z
    if(pressed_keys.z) {
        this->vanish = true;
    }
    return do_vanish(this);
}

actor_class_t actor_class = { sizeof(triangle_actor_t), init, update };