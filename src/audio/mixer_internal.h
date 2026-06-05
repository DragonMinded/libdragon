/**
 * @file mixer_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_MIXER_INTERNAL_H
#define LIBDRAGON_MIXER_INTERNAL_H

#include <stdint.h>

/** @brief RSPQ overlay ID assigned to the mixer ucode */
extern uint32_t __mixer_overlay_id;

/**
 * @brief Type of a global post-process effect callback.
 *
 * The function is called after mixer_exec() with the int32-packed stereo
 * output buffer and the number of stereo samples to process. It applies
 * the effect in place.
 */
typedef void (*mixer_global_effect_f)(int32_t *out, int num_samples);

/**
 * @brief Register a global post-process effect.
 *
 * The registered function is called from mixer_poll() after the RSP mix
 * completes. Only one global effect may be active at a time; registering
 * a new one replaces the previous.
 */
void mixer_register_global_effect(mixer_global_effect_f fn);

/**
 * @brief Unregister the global post-process effect.
 *
 * If \p fn matches the currently registered effect, it is removed.
 */
void mixer_unregister_global_effect(mixer_global_effect_f fn);

#endif
