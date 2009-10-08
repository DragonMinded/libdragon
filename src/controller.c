#include <libn64.h>
#include <string.h>
#include "controller.h"

static struct controller_data current;
static struct controller_data last;

void controller_init()
{
    memset(&current, 0, sizeof(current));
    memset(&last, 0, sizeof(last));
}

void controller_scan()
{
    /* Remember last */
    memcpy(&last, &current, sizeof(current));

    /* Grab current */
    memset(&current, 0, sizeof(current));
    controller_read(&current);
}

struct controller_data get_keys_down()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].buttons = (current.c[i].buttons) & ~(last.c[i].buttons);
    }

    return ret;
}

struct controller_data get_keys_up()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].buttons = ~(current.c[i].buttons) & (last.c[i].buttons);
    }

    return ret;
}

struct controller_data get_keys_held()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].buttons = (current.c[i].buttons) & (last.c[i].buttons);
    }

    return ret;
}

