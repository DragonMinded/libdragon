/**
 * @file joybus_internal.h
 * @brief Joybus internal API
 * @ingroup joybus
 */

#ifndef __LIBDRAGON_JOYBUS_INTERNAL_H
#define __LIBDRAGON_JOYBUS_INTERNAL_H

#include <stdint.h>

void joybus_exec_async(const void * input, void (*callback)(uint64_t *output, void *ctx), void *ctx);

#endif
