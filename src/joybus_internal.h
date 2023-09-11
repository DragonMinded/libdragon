/**
 * @file joybus_internal.h
 * @brief Joybus internal API
 * @ingroup joybus
 */

#ifndef __LIBDRAGON_JOYBUS_INTERNAL_H
#define __LIBDRAGON_JOYBUS_INTERNAL_H

#include <stdint.h>

/**
 * @addtogroup joybus
 * @{
 */

/** @brief Callback function signature for #joybus_exec_async */
typedef void (*joybus_callback_t)(uint64_t *out_dwords, void *ctx);

void joybus_exec_async(const void * input, joybus_callback_t callback, void *ctx);

/** @} */ /* joybus */

#endif
