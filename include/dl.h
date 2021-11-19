#ifndef __LIBDRAGON_DL_H
#define __LIBDRAGON_DL_H

#include <stdint.h>

#define DL_MAKE_COMMAND(ovl, cmd) ((((ovl) & 0xF) << 4) | ((cmd) & 0xF))

#define DL_OVERLAY_ADD(ovl_name, data_buf) ({ \
    extern uint8_t ovl_name ## _text_start[]; \
    extern uint8_t ovl_name ## _data_start[]; \
    extern uint8_t ovl_name ## _text_end[0]; \
    extern uint8_t ovl_name ## _data_end[0]; \
    dl_overlay_add( \
        ovl_name ## _text_start, \
        ovl_name ## _data_start, \
        (uint16_t)(ovl_name ## _text_end - ovl_name ## _text_start), \
        (uint16_t)(ovl_name ## _data_end - ovl_name ## _data_start), \
        data_buf); }) \

void dl_init();

uint8_t dl_overlay_add(void* code, void *data, uint16_t code_size, uint16_t data_size, void *data_buf);
void dl_overlay_register_id(uint8_t overlay_index, uint8_t id);

void dl_start();
void dl_close();

uint32_t* dl_write_begin(uint32_t size);
void dl_write_end();

void dl_queue_u8(uint8_t cmd);
void dl_queue_u16(uint16_t cmd);
void dl_queue_u32(uint32_t cmd);
void dl_queue_u64(uint64_t cmd);

void dl_noop();
void dl_interrupt();
void dl_signal(uint32_t signal);

#endif
