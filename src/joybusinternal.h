#ifndef __LIBDRAGON_JOYBUSINTERNAL_H
#define __LIBDRAGON_JOYBUSINTERNAL_H

void joybus_exec_async(const void * input, void (*callback)(uint64_t *output, void *ctx), void *ctx);

#endif
