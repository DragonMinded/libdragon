#ifndef __LIBDRAGON_UGFX_H
#define __LIBDRAGON_UGFX_H

void ugfx_init();
void ugfx_close();

void rdp_texture_rectangle(uint8_t tile, int16_t xh, int16_t yh, int16_t xl, int16_t yl, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);
void rdp_texture_rectangle_flip(uint8_t tile, int16_t xh, int16_t yh, int16_t xl, int16_t yl, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);
void rdp_sync_pipe();
void rdp_sync_tile();
void rdp_sync_full();
void rdp_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb);
void rdp_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr);
void rdp_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5);
void rdp_set_scissor(int16_t xh, int16_t yh, int16_t xl, int16_t yl);
void rdp_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z);
void rdp_set_other_modes(uint64_t modes);
void rdp_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx);
void rdp_sync_load();
void rdp_set_tile_size(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);
void rdp_load_block(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt);
void rdp_load_tile(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);
void rdp_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                  uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                  uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s);
void rdp_fill_rectangle(int16_t xh, int16_t yh, int16_t xl, int16_t yl);
void rdp_set_fill_color(uint32_t color);
void rdp_set_fog_color(uint32_t color);
void rdp_set_blend_color(uint32_t color);
void rdp_set_prim_color(uint32_t color);
void rdp_set_env_color(uint32_t color);
void rdp_set_combine_mode(uint64_t flags);
void rdp_set_texture_image(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width);
void rdp_set_z_image(uint32_t dram_addr);
void rdp_set_color_image(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width);

#endif
