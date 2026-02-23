/**
 * @file mixer_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_MIXER_INTERNAL_H
#define LIBDRAGON_MIXER_INTERNAL_H

#include <stdint.h>

/** @brief Get the current sample rate of the mixer */
extern uint32_t __mixer_get_frequency(void);

/** @brief RSPQ overlay ID assigned to the mixer ucode */
extern uint32_t __mixer_overlay_id;

#endif
