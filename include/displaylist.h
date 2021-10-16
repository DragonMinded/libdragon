#ifndef __LIBDRAGON_DISPLAYLIST_H
#define __LIBDRAGON_DISPLAYLIST_H

#include <stdint.h>

#define DL_OVERLAY_DEFAULT 0x0

#define DL_CMD_NOOP       0x0
#define DL_CMD_INTERRUPT  0x1

#define DL_MAKE_COMMAND(ovl, cmd) ((((ovl) & 0xF) << 4) | ((cmd) & 0xF))

typedef struct dl_overlay_t {
    void* code;
    uint32_t code_size;
    void* data;
    uint32_t data_size;
} dl_overlay_t;

void dl_overlay_register(uint8_t id, dl_overlay_t *overlay);

void dl_init();
void dl_close();

uint32_t* dl_write_begin(uint32_t size);
void dl_write_end();


// TODO: Find a way to pack commands that are smaller than 4 bytes

static inline void dl_queue_u8(uint8_t cmd)
{
    *dl_write_begin(sizeof(uint32_t)) = (uint32_t)cmd << 24;
    dl_write_end();
}

static inline void dl_queue_u16(uint16_t cmd)
{
    *dl_write_begin(sizeof(uint32_t)) = (uint32_t)cmd << 16;
    dl_write_end();
}

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

static inline void dl_noop()
{
    dl_queue_u8(DL_MAKE_COMMAND(DL_OVERLAY_DEFAULT, DL_CMD_NOOP));
}

static inline void dl_interrupt()
{
    dl_queue_u8(DL_MAKE_COMMAND(DL_OVERLAY_DEFAULT, DL_CMD_INTERRUPT));
}

#endif
