#ifndef __LIBDRAGON_JOYBUSINTERNAL_H
#define __LIBDRAGON_JOYBUSINTERNAL_H

/** @brief Joybus execution callback pointer */
typedef void (*joybus_callback_t)(uint64_t *out_dwords, void *ctx);

void joybus_exec_async(const void * input, joybus_callback_t callback, void *ctx);

#endif
