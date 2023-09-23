/**
 * @file pixelfx.h
 * @brief PixelFX integration routines
 * @ingroup pixelfx
 */
#ifndef __LIBDRAGON_PIXELFX_H
#define __LIBDRAGON_PIXELFX_H

#include <stdint.h>

/**
 * @defgroup pixelfx PixelFX integration routines
 * @ingroup peripherals
 * @brief Functions for integrating with PixelFX console mods.
 * 
 * N64Digital and Retro GEM allow per-game settings when ROM metadata
 * is broadcast over Joybus. This module provides functions to easily
 * set and reset the "Game ID" to tell the console mod which game settings
 * to use.
 * 
 * @see https://gitlab.com/pixelfx-public/n64-game-id
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Notifies the PixelFX console mod of the current Game ID.
 *
 * This function is mostly intended to be used by flashcart menu software or
 * by software that will be flashed onto single-ROM flash cartridges.
 *
 * @see https://gitlab.com/pixelfx-public/n64-game-id#n64-game-id-per-game-settings-for-n64digital
 *
 * @param rom_check_code ROM check code (ROM header bytes 0x10-0x17)
 * @param media_format Media category code (ROM header byte 0x3B)
 * @param region_code Region code (ROM header byte 0x3E)
 */
void pixelfx_send_game_id(uint64_t rom_check_code, uint8_t media_format, uint8_t region_code);

/**
 * @brief Notifies the PixelFX console mod to clear the current Game ID.
 *
 * This function is mostly intended to be used by flashcart menu software or
 * by software that will be flashed onto single-ROM flash cartridges.
 *
 * @see https://gitlab.com/pixelfx-public/n64-game-id#special-ids
 */
void pixelfx_clear_game_id(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* pixelfx */

#endif
