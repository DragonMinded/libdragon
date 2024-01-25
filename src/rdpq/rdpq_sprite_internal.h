#ifndef LIBDRAGON_RDPQ_SPRITE_INTERNAL_H
#define LIBDRAGON_RDPQ_SPRITE_INTERNAL_H

#include "rdpq.h"

///@cond
typedef struct rdpq_texparms_s rdpq_texparms_t;
///@endcond

int __rdpq_sprite_upload(rdpq_tile_t tile, sprite_t *sprite, const rdpq_texparms_t *parms, bool set_mode);

#endif
