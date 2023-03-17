#include "actor.h"

typedef struct circle_actor_s {
    actor_t actor;
} circle_actor_t;

static void init(actor_t *this)
{
    
}

static bool update(actor_t *this, struct controller_data keys)
{
    this->angle += 0.01f;
    return true;
}

actor_class_t actor_class = { sizeof(circle_actor_t), init, update };