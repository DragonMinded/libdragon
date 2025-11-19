#include "actor.h"

#define SPAWN_DURATION 1500
#define FLICKER_DURATION 120

typedef struct circle_actor_s {
    actor_t actor;
    int num_ticks;
    float home_x;
    float home_y;
    float vel_x;
    float vel_y;
} circle_actor_t;

static void init(actor_t *actor)
{
    circle_actor_t *this = (circle_actor_t *)actor;
    this->home_x = actor->x;
    this->home_y = actor->y;
    this->vel_x = 2.0f;
    this->vel_y = 2.0f;
}

static void apply_accel(float *pos, float *origin_pos, float *vel, float accel)
{
    //Accelerate towards origin
    if(*pos > *origin_pos) {
        *vel -= accel;
    } else {
        *vel += accel;
    }
    *pos += *vel;
}

static bool update(actor_t *actor, joypad_buttons_t pressed_keys)
{
    circle_actor_t *this = (circle_actor_t *)actor;
    apply_accel(&actor->x, &this->home_x, &this->vel_x, 0.2f);
    apply_accel(&actor->y, &this->home_y, &this->vel_y, 0.1f);
    //Despawn after existing for too long
    if(++this->num_ticks > SPAWN_DURATION) {
        return false;
    }
    //Fast forward to flickering when pressing B
    if(pressed_keys.b) {
        this->num_ticks = SPAWN_DURATION-FLICKER_DURATION;
    }
    if(this->num_ticks > SPAWN_DURATION-FLICKER_DURATION) {
        //Do flicker when running out of time
        actor->visible = !actor->visible;
    }
    return true;
}

actor_class_t actor_class = { sizeof(circle_actor_t), init, update };