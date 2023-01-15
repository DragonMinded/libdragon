/**
 * @file rdpq.h
 * @brief RDP Command queue
 * @ingroup rdp
 * 
 */

#include "rdpq_quad.h"
#include "rdpq_internal.h"

// The fixup for fill rectangle and texture rectangle uses the exact same code in IMEM.
// It needs to also adjust the command ID with the same constant (via XOR), so make
// sure that we defined the fixups in the right position to make that happen.
_Static_assert(
    (RDPQ_CMD_FILL_RECTANGLE ^ RDPQ_CMD_FILL_RECTANGLE_EX) == 
    (RDPQ_CMD_TEXTURE_RECTANGLE ^ RDPQ_CMD_TEXTURE_RECTANGLE_EX),
    "invalid command numbering");


/** @brief Out-of-line implementation of #rdpq_texture_rectangle */
__attribute__((noinline))
void __rdpq_fill_rectangle(uint32_t w0, uint32_t w1)
{
    __rdpq_autosync_use(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (RDPQ_CMD_FILL_RECTANGLE_EX, w0, w1),  // RSP
        (RDPQ_CMD_FILL_RECTANGLE_EX, w0, w1)   // RDP
    );
}

void __rdpq_fill_rectangle_offline(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    __rdpq_fill_rectangle_inline(x0, y0, x1, y1);
}

/** @brief Out-of-line implementation of #rdpq_texture_rectangle */
__attribute__((noinline))
void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    int tile = (w1 >> 24) & 7;
    // FIXME: this can also use tile+1 in case the combiner refers to TEX1
    // FIXME: this can also use tile+2 and +3 in case SOM activates texture detail / sharpen
    __rdpq_autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILE(tile) | AUTOSYNC_TMEM(0));
    rdpq_fixup_write(
        (RDPQ_CMD_TEXTURE_RECTANGLE_EX, w0, w1, w2, w3),  // RSP
        (RDPQ_CMD_TEXTURE_RECTANGLE_EX, w0, w1, w2, w3)   // RDP
    );
}

void __rdpq_texture_rectangle_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0) {
    __rdpq_texture_rectangle_inline(tile, x0, y0, x1, y1, s0, t0);
}

void __rdpq_texture_rectangle_scaled_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1) {
    __rdpq_texture_rectangle_scaled_inline(tile, x0, y0, x1, y1, s0, t0, s1, t1);
}

extern inline void __rdpq_fill_rectangle_inline(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern inline void __rdpq_fill_rectangle_fx(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern inline void __rdpq_texture_rectangle_fx(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s, int32_t t);
extern inline void __rdpq_texture_rectangle_scaled_fx(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1);
extern inline void __rdpq_texture_rectangle_raw_fx(rdpq_tile_t tile, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t s0, uint16_t t0, int16_t dsdx, int16_t dtdy);
extern inline void __rdpq_texture_rectangle_flip_raw_fx(rdpq_tile_t tile, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t s, int16_t t, int16_t dsdy, int16_t dtdx);
