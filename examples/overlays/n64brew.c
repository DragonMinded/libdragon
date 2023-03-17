#include "actor.h"

typedef struct n64brew_actor_s {
    actor_t actor;
} n64brew_actor_t;

static void init(actor_t *this)
{
    
}

static bool update(actor_t *this, struct controller_data keys)
{
    return true;
}

actor_class_t actor_class = { sizeof(n64brew_actor_t), init, update };