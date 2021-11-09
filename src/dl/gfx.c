#include <libdragon.h>
#include <stdlib.h>

#include "gfx_internal.h"

gfx_t *__gfx;

void gfx_init()
{
    if (__gfx != NULL) {
        return;
    }

    __gfx = malloc(sizeof(gfx_t));
    __gfx->other_modes = 0;
    __gfx->dram_buffer = malloc(RDP_DRAM_BUFFER_SIZE);
    __gfx->dram_buffer_size = RDP_DRAM_BUFFER_SIZE;
    __gfx->dram_buffer_end = 0;
    __gfx->dmem_buffer_ptr = 0;
    __gfx->rdp_initialised = 0;

    data_cache_hit_writeback(__gfx, sizeof(gfx_t));

    uint8_t ovl_index = DL_OVERLAY_ADD(rsp_ovl_gfx, __gfx);
    dl_overlay_register_id(ovl_index, 2);
    dl_overlay_register_id(ovl_index, 3);
}

void gfx_close()
{
    if (__gfx == NULL) {
        return;
    }

    free(__gfx->dram_buffer);
    free(__gfx);
    __gfx = NULL;
}

void rdp_texture_rectangle(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t ds, int16_t dt)
{
    uint64_t *ptr = (uint64_t*)dl_write_begin(16);
    ptr[0] = RdpTextureRectangle1FX(tile, x0, y0, x1, y1);
    ptr[1] = RdpTextureRectangle2FX(s, t, ds, dt);
    dl_write_end();
}

void rdp_texture_rectangle_flip(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t ds, int16_t dt)
{
    uint64_t *ptr = (uint64_t*)dl_write_begin(16);
    ptr[0] = RdpTextureRectangleFlip1FX(tile, x0, y0, x1, y1);
    ptr[1] = RdpTextureRectangle2FX(s, t, ds, dt);
    dl_write_end();
}

void rdp_sync_pipe()
{
    dl_queue_u64(RdpSyncPipe());
}

void rdp_sync_tile()
{
    dl_queue_u64(RdpSyncTile());
}

void rdp_sync_full()
{
    dl_queue_u64(RdpSyncFull());
}

void rdp_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb)
{
    dl_queue_u64(RdpSetKeyGb(wg, wb, cg, sg, cb, sb));
}

void rdp_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr)
{
    dl_queue_u64(RdpSetKeyR(wr, cr, sr));
}

void rdp_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5)
{
    dl_queue_u64(RdpSetConvert(k0, k1, k2, k3, k4, k5));
}

void rdp_set_scissor(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    dl_queue_u64(RdpSetClippingFX(x0, y0, x1, y1));
}

void rdp_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z)
{
    dl_queue_u64(RdpSetPrimDepth(primitive_z, primitive_delta_z));
}

void rdp_set_other_modes(uint64_t modes)
{
    dl_queue_u64(RdpSetOtherModes(modes));
}

void rdp_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx)
{
    dl_queue_u64(RdpLoadTlut(tile, lowidx, highidx));
}

void rdp_sync_load()
{
    dl_queue_u64(RdpSyncLoad());
}

void rdp_set_tile_size(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1)
{
    dl_queue_u64(RdpSetTileSizeFX(tile, s0, t0, s1, t1));
}

void rdp_load_block(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt)
{
    dl_queue_u64(RdpLoadBlock(tile, s0, t0, s1, dxt));
}

void rdp_load_tile(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1)
{
    dl_queue_u64(RdpLoadTileFX(tile, s0, t0, s1, t1));
}

void rdp_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                  uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                  uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s)
{
    dl_queue_u64(RdpSetTile(format, size, line, tmem_addr, tile, palette, ct, mt, mask_t, shift_t, cs, ms, mask_s, shift_s));
}

void rdp_fill_rectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    dl_queue_u64(RdpFillRectangleFX(x0, y0, x1, y1));
}

void rdp_set_fill_color(uint32_t color)
{
    dl_queue_u64(RdpSetFillColor(color));
}

void rdp_set_fog_color(uint32_t color)
{
    dl_queue_u64(RdpSetFogColor(color));
}

void rdp_set_blend_color(uint32_t color)
{
    dl_queue_u64(RdpSetBlendColor(color));
}

void rdp_set_prim_color(uint32_t color)
{
    dl_queue_u64(RdpSetPrimColor(color));
}

void rdp_set_env_color(uint32_t color)
{
    dl_queue_u64(RdpSetEnvColor(color));
}

void rdp_set_combine_mode(uint64_t flags)
{
    dl_queue_u64(RdpSetCombine(flags));
}

void rdp_set_texture_image(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width)
{
    dl_queue_u64(RdpSetTexImage(format, size, dram_addr, width));
}

void rdp_set_z_image(uint32_t dram_addr)
{
    dl_queue_u64(RdpSetDepthImage(dram_addr));
}

void rdp_set_color_image(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width)
{
    dl_queue_u64(RdpSetColorImage(format, size, width, dram_addr));
}
