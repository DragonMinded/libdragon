/**
* @copyright 2023 - Max Bebök
* @license TBD
*/

#include "microuiN64.h"
#include "libdragon.h"

#define FONT_SIZE  8
#define TILE_WIDTH 10

mu_Context mu_ctx;

static sprite_t *sprite = NULL;
static uint8_t font_index = 0;
static joypad_port_t joypad_index = 0;

static float mouse_pos_raw[2] = {40,40};
static int mouse_pos[2] = {40,40};
static bool cursor_active = true;
static float cursor_speed = 0.025f;
static float n64_mouse_speed = 200.0f;
static bool is_n64_mouse = false;

// Approx. for text size
static int text_width(mu_Font fnt, const char *text, int len) {
  if (len == -1) { len = strlen(text); }
  return len * (FONT_SIZE-3);
}

static int text_height(mu_Font fnt) {
  return FONT_SIZE;
}

bool mu64_is_active() 
{
  return cursor_active;
}

void mu64_set_mouse_speed(float speed)
{
  cursor_speed = speed;
}

void mu64_init(joypad_port_t joypad_idx, uint8_t font_idx)
{
  joypad_index = joypad_idx;
  font_index = font_idx;
  if(!sprite) {
    sprite = sprite_load("rom:/mui.sprite");
  }

  mu_init(&mu_ctx);
  mu_ctx.text_width = text_width;
  mu_ctx.text_height = text_height;
  mu_ctx._style.padding = 1;
  mu_ctx._style.title_height = 12;
  mu_ctx._style.spacing = 1;
  mu_ctx._style.indent = 6;
  mu_ctx._style.thumb_size = 8;
  mu_ctx._style.colors[MU_COLOR_TITLEBG] = (mu_Color){0x1c, 0x4f, 0x97, 0xFF};
  mu_ctx._style.colors[MU_COLOR_BORDER]  = (mu_Color){0x10, 0x10, 0x10, 0xFF};

  cursor_active = true;
  is_n64_mouse = joypad_get_identifier(joypad_index) == JOYBUS_IDENTIFIER_N64_MOUSE;
}

void mu64_start_frame()
{
  joypad_buttons_t btn_press = joypad_get_buttons_pressed(joypad_index);
  if(btn_press.l) {
    cursor_active = !cursor_active;
  }

  if(cursor_active) {
    joypad_inputs_t mouse = joypad_get_inputs(joypad_index);
    joypad_buttons_t btn_release = joypad_get_buttons_released(joypad_index);

    float delta_x = (float)(mouse.stick_x);
    float delta_y = (float)(mouse.stick_y);

    if(is_n64_mouse) {
      delta_x *= n64_mouse_speed;
      delta_y *= n64_mouse_speed;
    } else {
      delta_x = fabsf(delta_x) * delta_x;
      delta_y = fabsf(delta_y) * delta_y;
    }

    mouse_pos_raw[0] += delta_x * cursor_speed;
    mouse_pos_raw[1] -= delta_y * cursor_speed;

    mouse_pos_raw[0] = fmaxf(0.0f, fminf(mouse_pos_raw[0], display_get_width()-8.0f));
    mouse_pos_raw[1] = fmaxf(0.0f, fminf(mouse_pos_raw[1], display_get_height()-8.0f));

    mouse_pos[0] = (int)mouse_pos_raw[0];
    mouse_pos[1] = (int)mouse_pos_raw[1];

    if(btn_press.a) {
      mu_input_mousedown(&mu_ctx, mouse_pos[0], mouse_pos[1], MU_MOUSE_LEFT);
    } else if(btn_release.a) {
      mu_input_mouseup(&mu_ctx, mouse_pos[0], mouse_pos[1], MU_MOUSE_LEFT);
    } else {
      mu_input_mousemove(&mu_ctx, mouse_pos[0], mouse_pos[1]);
    }
  }

  mu_begin(&mu_ctx);
}

void mu64_end_frame()
{
  mu_end(&mu_ctx);
}

void mu64_draw()
{
  if(!cursor_active)return;

  rdpq_textparms_t txt_param = {};
  rdpq_blitparms_t tile_param = {.width = TILE_WIDTH};

  rdpq_set_mode_standard();

  mu_Command *cmd = NULL;
  while (mu_next_command(&mu_ctx, &cmd))
  {
    switch (cmd->type) {
      case MU_COMMAND_TEXT:
        rdpq_text_print(&txt_param, font_index, cmd->text.pos.x, cmd->text.pos.y + FONT_SIZE - 1, cmd->text.str);
      break;

      case MU_COMMAND_RECT:
        if(cmd->rect.color.a != 0) {
          rdpq_set_mode_fill(*(color_t*)(void*)&cmd->rect.color);
          rdpq_fill_rectangle(
            cmd->rect.rect.x, cmd->rect.rect.y,
            cmd->rect.rect.x + cmd->rect.rect.w,
            cmd->rect.rect.y + cmd->rect.rect.h
          );
        }
      break;

      case MU_COMMAND_ICON:
        if(cmd->icon.id > 0) {
          rdpq_set_mode_standard();
          rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
          tile_param.s0 = (cmd->icon.id-1) * TILE_WIDTH;
          rdpq_sprite_blit(sprite, cmd->icon.rect.x, cmd->icon.rect.y, &tile_param);
        }
      break;

      case MU_COMMAND_SURFACE:
        if(cmd->surface.surface) {
          rdpq_set_mode_standard();
          rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

          surface_t *surface = (surface_t*)cmd->surface.surface;
          rdpq_blitparms_t blitParam = { 
            .width = surface->width,
            .height = surface->height,
            .scale_x = (float)cmd->surface.rect.w / (float)surface->width,
            .scale_y = (float)cmd->surface.rect.h / (float)surface->height,
          };
          rdpq_tex_blit(surface, cmd->surface.rect.x, cmd->surface.rect.y, &blitParam);
        }
      break;

       case MU_COMMAND_SPRITE:
        if(cmd->surface.surface) {
          rdpq_set_mode_standard();
          rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

          sprite_t *sprite = (sprite_t*)cmd->sprite.sprite;
          rdpq_blitparms_t blitParam = { 
            .width = sprite->width,
            .height = sprite->height,
            .scale_x = (float)cmd->sprite.rect.w / (float)sprite->width,
            .scale_y = (float)cmd->sprite.rect.h / (float)sprite->height,
          };
          rdpq_sprite_blit(sprite, cmd->sprite.rect.x, cmd->sprite.rect.y, &blitParam);
        }
      break;

      case MU_COMMAND_CLIP:
        int clipEndX = cmd->clip.rect.x + cmd->clip.rect.w;
        int clipEndY = cmd->clip.rect.y + cmd->clip.rect.h;
        rdpq_set_scissor(
          cmd->clip.rect.x,  cmd->clip.rect.y,
          clipEndX > display_get_width() ? display_get_width() : clipEndX,
          clipEndY > display_get_height() ? display_get_height() : clipEndY
        );
        
      break;
    }
  }

  rdpq_set_scissor(0, 0, display_get_width(), display_get_height());

  if(cursor_active) {
    rdpq_set_mode_standard();
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

    tile_param.s0 = (MU_ICON_MAX-1) * TILE_WIDTH;
    tile_param.scale_x = mu_ctx.mouse_down ? 0.8f : 1.0f;
    tile_param.scale_y = tile_param.scale_x;
    rdpq_sprite_blit(sprite, mouse_pos_raw[0], mouse_pos_raw[1], &tile_param);
    tile_param.scale_x = 1.0f;
    tile_param.scale_y = 1.0f;
  }
}

