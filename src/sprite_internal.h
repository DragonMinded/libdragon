#ifndef __LIBDRAGON_SPRITE_INTERNAL_H
#define __LIBDRAGON_SPRITE_INTERNAL_H

#include <stdbool.h>

/** @brief Convert a sprite from the old format with implicit texture format */ 
bool __sprite_upgrade(sprite_t *sprite);

#endif
