#ifndef __LIBDRAGON_DISPLAYLIST_H
#define __LIBDRAGON_DISPLAYLIST_H

#include <stdint.h>

void dl_init();
void dl_close();

uint32_t* dl_write_begin(uint32_t size);
void dl_write_end();

static inline void dl_queue_u32(uint32_t cmd)
{
    *dl_write_begin(sizeof(uint32_t)) = cmd;
    dl_write_end();
}

static inline void dl_queue_u64(uint64_t cmd)
{
    *((uint64_t*)dl_write_begin(sizeof(uint64_t))) = cmd;
    dl_write_end();
}

#endif
