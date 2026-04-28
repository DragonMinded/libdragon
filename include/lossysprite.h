/**
 * @file lossysprite.h
 * @brief LSPR (lossy sprite) decoder
 */
#ifndef __LIBDRAGON_LOSSYSPRITE_H
#define __LIBDRAGON_LOSSYSPRITE_H

#include <stdbool.h>
#include "sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open and decode a LSPR file into memory.
 *
 * The sprite is fully decoded during this function call and becomes
 * a regular RGBA16 sprite that can be used like any other sprite
 * using regular sprite functions.
 *
 * @param fn    Path to the LSPR file
 * @return      The loaded sprite
 */
sprite_t* lossysprite_load(const char *fn);

#ifdef __cplusplus
}
#endif

#endif

