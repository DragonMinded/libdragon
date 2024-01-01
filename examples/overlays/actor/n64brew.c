#include "actor.h"
#include <math.h>

#define SPAWN_DURATION 1500
#define FLICKER_DURATION 120
#define MAX_ROTATION 0.7f

typedef struct n64brew_actor_s {
    actor_t actor;
    float angle_vel;
    int num_ticks;
} n64brew_actor_t;

static void init(actor_t *actor)
{
    n64brew_actor_t *this = (n64brew_actor_t *)actor;
    this->angle_vel = 0.025f;
}

static void do_rotation(n64brew_actor_t *this)
{
    this->actor.angle += this->angle_vel;
    if(this->actor.angle > MAX_ROTATION) {
        this->angle_vel = -this->angle_vel;
        this->actor.angle = MAX_ROTATION;
    }
    if(this->actor.angle < -MAX_ROTATION) {
        this->angle_vel = -this->angle_vel;
        this->actor.angle = -MAX_ROTATION;
    }
	this->actor.x_scale = this->actor.y_scale = cos(this->actor.angle);
}

static void do_crash()
{
    debugf((char *)0x1);
}

static bool update(actor_t *actor, joypad_buttons_t pressed_keys)
{
    n64brew_actor_t *this = (n64brew_actor_t *)actor;
    do_rotation(this);
    if(pressed_keys.c_right) {
        do_crash();
    }
    //Despawn after existing for too long
    if(++this->num_ticks > SPAWN_DURATION) {
        return false;
    }
    //Fast forward to flickering when pressing C-up
    if(pressed_keys.c_up) {
        this->num_ticks = SPAWN_DURATION-FLICKER_DURATION;
    }
    if(this->num_ticks > SPAWN_DURATION-FLICKER_DURATION) {
        //Do flicker when running out of time
        actor->visible = !actor->visible;
    }
    return true;
}

actor_class_t actor_class = { sizeof(n64brew_actor_t), init, update };