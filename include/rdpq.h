#ifndef __LIBDRAGON_RDPQ_H
#define __LIBDRAGON_RDPQ_H

#include <stdint.h>
#include <stdbool.h>
#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

void rdpq_init();

void rdpq_close();

void rdpq_fill_triangle(bool flip, uint8_t level, uint8_t tile, int16_t yl, int16_t ym, int16_t yh, int32_t xl, int32_t dxldy, int32_t xh, int32_t dxhdy, int32_t xm, int32_t dxmdy);

/**
 * @brief Low level function to draw a textured rectangle
 */
void rdpq_texture_rectangle(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);

/**
 * @brief Low level function to draw a textured rectangle (s and t coordinates flipped)
 */
void rdpq_texture_rectangle_flip(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t dsdx, int16_t dtdy);

/**
 * @brief Low level function to sync the RDP pipeline
 */
void rdpq_sync_pipe();

/**
 * @brief Low level function to sync RDP tile operations
 */
void rdpq_sync_tile();

/**
 * @brief Wait for any operation to complete before causing a DP interrupt
 */
void rdpq_sync_full();

/**
 * @brief Low level function to set the green and blue components of the chroma key
 */
void rdpq_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb);

/**
 * @brief Low level function to set the red component of the chroma key
 */
void rdpq_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr);

/**
 * @brief Low level functions to set the matrix coefficients for texture format conversion
 */
void rdpq_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5);

/**
 * @brief Low level function to set the scissoring region
 */
void rdpq_set_scissor(int16_t xh, int16_t yh, int16_t xl, int16_t yl);

/**
 * @brief Low level function to set the primitive depth
 */
void rdpq_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z);

/**
 * @brief Low level function to set the "other modes"
 */
void rdpq_set_other_modes(uint64_t modes);

/**
 * @brief Low level function to load a texture palette into TMEM
 */
void rdpq_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx);

/**
 * @brief Low level function to synchronize RDP texture load operations
 */
void rdpq_sync_load();

/**
 * @brief Low level function to set the size of a tile descriptor
 */
void rdpq_set_tile_size(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);

/**
 * @brief Low level function to load a texture image into TMEM in a single memory transfer
 */
void rdpq_load_block(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt);

/**
 * @brief Low level function to load a texture image into TMEM
 */
void rdpq_load_tile(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1);

/**
 * @brief Low level function to set the properties of a tile descriptor
 */
void rdpq_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                      uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                      uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s);

/**
 * @brief Low level function to render a rectangle filled with a solid color
 */
void rdpq_fill_rectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/**
 * @brief Low level function to set the fill color
 */
inline void rdpq_set_fill_color(color_t color) {
    extern void __rdpq_set_fill_color32(uint32_t);
    __rdpq_set_fill_color32((color.r << 24) | (color.g << 16) | (color.b << 8) | (color.a << 0));
}

inline void rdpq_set_fill_color_pattern(color_t color1, color_t color2) {
    extern void __rdpq_set_fill_color(uint32_t);
    uint32_t c1 = (((int)color1.r >> 3) << 11) | (((int)color1.g >> 3) << 6) | (((int)color1.b >> 3) << 1) | (color1.a >> 7);
    uint32_t c2 = (((int)color2.r >> 3) << 11) | (((int)color2.g >> 3) << 6) | (((int)color2.b >> 3) << 1) | (color2.a >> 7);
    __rdpq_set_fill_color((c1 << 16) | c2);
}

/**
 * @brief Low level function to set the fog color
 */
void rdpq_set_fog_color(uint32_t color);

/**
 * @brief Low level function to set the blend color
 */
void rdpq_set_blend_color(uint32_t color);

/**
 * @brief Low level function to set the primitive color
 */
void rdpq_set_prim_color(uint32_t color);

/**
 * @brief Low level function to set the environment color
 */
void rdpq_set_env_color(uint32_t color);

/**
 * @brief Low level function to set the color combiner parameters
 */
void rdpq_set_combine_mode(uint64_t flags);

/**
 * @brief Low level function to set RDRAM pointer to a texture image
 */
void rdpq_set_texture_image(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width);

/**
 * @brief Low level function to set RDRAM pointer to the depth buffer
 */
void rdpq_set_z_image(uint32_t dram_addr);

/**
 * @brief Low level function to set RDRAM pointer to the color buffer
 */
void rdpq_set_color_image(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width);

#ifdef __cplusplus
}
#endif

#endif
