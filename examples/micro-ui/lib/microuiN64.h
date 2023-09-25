#ifndef H_MICROUI_N64
#define H_MICROUI_N64

#include "microui.h"
#include <stdbool.h>
#include <stdint.h>
#include <libdragon.h>

void mu64_init(joypad_port_t joypad_idx, uint8_t font_idx);

void mu64_start_frame();
void mu64_end_frame();
void mu64_draw();

bool mu64_is_active();
void mu64_set_mouse_speed(float speed);

extern mu_Context mu_ctx;

#endif