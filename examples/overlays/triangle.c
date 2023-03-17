#include "actor.h"

typedef struct triangle_actor_s {
    actor_t actor;
} triangle_actor_t;

static void init(actor_t *this)
{
    
}

static bool update(actor_t *this, struct controller_data keys)
{
    return true;
}

actor_class_t actor_class = { sizeof(triangle_actor_t), init, update };