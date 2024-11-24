#ifndef LIBDRAGON_MIXER_INTERNAL_H
#define LIBDRAGON_MIXER_INTERNAL_H

#include <stdint.h>

/** @brief RSPQ overlay ID assigned to the mixer ucode */
extern uint32_t __mixer_overlay_id;

/** Check if a certain waveform is currently playing on some channel */
void __mixer_wave_stopall(waveform_t *wave);

#endif
