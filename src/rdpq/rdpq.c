#include "rdpq.h"
#include "rdpq_block.h"
#include "rspq.h"
#include "rspq/rspq_commands.h"
#include <string.h>

#define RDPQ_MAX_COMMAND_SIZE 44
#define RDPQ_BLOCK_MIN_SIZE   64
#define RDPQ_BLOCK_MAX_SIZE   4192

#define RDPQ_OVL_ID (0xC << 28)

DEFINE_RSP_UCODE(rsp_rdpq);

typedef struct rdpq_state_s {
    uint64_t other_modes;
    uint8_t target_bitdepth;
} rdpq_state_t;

typedef struct rdpq_block_s {
    rdpq_block_t *next;
    uint32_t padding;
    uint32_t cmds[];
} rdpq_block_t;

volatile uint32_t *rdpq_block_pointer;
volatile uint32_t *rdpq_block_sentinel;

rdpq_block_t *rdpq_block;
static int rdpq_block_size;

static volatile uint32_t *last_rdp_cmd;

enum {
    RDPQ_CMD_NOOP                    = 0x00,
    RDPQ_CMD_TRI                     = 0x08,
    RDPQ_CMD_TRI_ZBUF                = 0x09,
    RDPQ_CMD_TRI_TEX                 = 0x0A,
    RDPQ_CMD_TRI_TEX_ZBUF            = 0x0B,
    RDPQ_CMD_TRI_SHADE               = 0x0C,
    RDPQ_CMD_TRI_SHADE_ZBUF          = 0x0D,
    RDPQ_CMD_TRI_SHADE_TEX           = 0x0E,
    RDPQ_CMD_TRI_SHADE_TEX_ZBUF      = 0x0F,
    RDPQ_CMD_MODIFY_OTHER_MODES      = 0x20, // Fixup command
    RDPQ_CMD_SET_FILL_COLOR_32       = 0x21, // Fixup command
    RDPQ_CMD_SET_COLOR_IMAGE_FIXUP   = 0x22, // Fixup command
    RDPQ_CMD_SET_SCISSOR_EX          = 0x23, // Fixup command
    RDPQ_CMD_TEXTURE_RECTANGLE       = 0x24,
    RDPQ_CMD_TEXTURE_RECTANGLE_FLIP  = 0x25,
    RDPQ_CMD_SYNC_LOAD               = 0x26,
    RDPQ_CMD_SYNC_PIPE               = 0x27,
    RDPQ_CMD_SYNC_TILE               = 0x28,
    RDPQ_CMD_SYNC_FULL               = 0x29,
    RDPQ_CMD_SET_KEY_GB              = 0x2A,
    RDPQ_CMD_SET_KEY_R               = 0x2B,
    RDPQ_CMD_SET_CONVERT             = 0x2C,
    RDPQ_CMD_SET_SCISSOR             = 0x2D,
    RDPQ_CMD_SET_PRIM_DEPTH          = 0x2E,
    RDPQ_CMD_SET_OTHER_MODES         = 0x2F,
    RDPQ_CMD_LOAD_TLUT               = 0x30,
    RDPQ_CMD_SET_TILE_SIZE           = 0x32,
    RDPQ_CMD_LOAD_BLOCK              = 0x33,
    RDPQ_CMD_LOAD_TILE               = 0x34,
    RDPQ_CMD_SET_TILE                = 0x35,
    RDPQ_CMD_FILL_RECTANGLE          = 0x36,
    RDPQ_CMD_SET_FILL_COLOR          = 0x37,
    RDPQ_CMD_SET_FOG_COLOR           = 0x38,
    RDPQ_CMD_SET_BLEND_COLOR         = 0x39,
    RDPQ_CMD_SET_PRIM_COLOR          = 0x3A,
    RDPQ_CMD_SET_ENV_COLOR           = 0x3B,
    RDPQ_CMD_SET_COMBINE_MODE        = 0x3C,
    RDPQ_CMD_SET_TEXTURE_IMAGE       = 0x3D,
    RDPQ_CMD_SET_Z_IMAGE             = 0x3E,
    RDPQ_CMD_SET_COLOR_IMAGE         = 0x3F,
};

void rdpq_init()
{
    rdpq_state_t *rdpq_state = UncachedAddr(rspq_overlay_get_state(&rsp_rdpq));

    memset(rdpq_state, 0, sizeof(rdpq_state_t));

    rspq_init();
    rspq_overlay_register_static(&rsp_rdpq, RDPQ_OVL_ID);

    rdpq_block = NULL;
}

void rdpq_close()
{
    rspq_overlay_unregister(RDPQ_OVL_ID);
}

void rdpq_reset_buffer()
{
    last_rdp_cmd = NULL;
}

void rdpq_block_flush(uint32_t *start, uint32_t *end)
{
    assertf(((uint32_t)start & 0x7) == 0, "start not aligned to 8 bytes: %lx", (uint32_t)start);
    assertf(((uint32_t)end & 0x7) == 0, "end not aligned to 8 bytes: %lx", (uint32_t)end);

    extern void rspq_rdp(uint32_t start, uint32_t end);

    uint32_t phys_start = PhysicalAddr(start);
    uint32_t phys_end = PhysicalAddr(end);

    // FIXME: Updating the previous command won't work across buffer switches
    uint32_t diff = rdpq_block_pointer - last_rdp_cmd;
    if (diff == 2 && (*last_rdp_cmd&0xFFFFFF) == phys_start) {
        // Update the previous command
        *last_rdp_cmd = (RSPQ_CMD_RDP<<24) | phys_end;
    } else {
        // Put a command in the regular RSP queue that will submit the last buffer of RDP commands.
        last_rdp_cmd = rdpq_block_pointer;
        rspq_write(0, RSPQ_CMD_RDP, phys_end, phys_start);
    }
}

void rdpq_block_switch_buffer(uint32_t *new, uint32_t size)
{
    assert(size >= RDPQ_MAX_COMMAND_SIZE);

    rdpq_block_pointer = new;
    rdpq_block_sentinel = new + size - RDPQ_MAX_COMMAND_SIZE;
}

void rdpq_block_next_buffer()
{
    // Allocate next chunk (double the size of the current one).
    // We use doubling here to reduce overheads for large blocks
    // and at the same time start small.
    if (rdpq_block_size < RDPQ_BLOCK_MAX_SIZE) rdpq_block_size *= 2;
    rdpq_block->next = malloc_uncached(sizeof(rdpq_block_t) + rdpq_block_size*sizeof(uint32_t));
    rdpq_block = rdpq_block->next;

    // Switch to new buffer
    rdpq_block_switch_buffer(rdpq_block->cmds, rdpq_block_size);
}

rdpq_block_t* rdpq_block_begin()
{
    rdpq_block_size = RDPQ_BLOCK_MIN_SIZE;
    rdpq_block = malloc_uncached(sizeof(rdpq_block_t) + rdpq_block_size*sizeof(uint32_t));
    rdpq_block->next = NULL;
    rdpq_block_switch_buffer(rdpq_block->cmds, rdpq_block_size);
    rdpq_reset_buffer();
    return rdpq_block;
}

void rdpq_block_end()
{
    rdpq_block = NULL;
}

void rdpq_block_free(rdpq_block_t *block)
{
    while (block) {
        void *b = block;
        block = block->next;
        free_uncached(b);
    }
}

/// @cond

#define _rdpq_write_arg(arg) \
    *ptr++ = (arg);

/// @endcond

#define rdpq_dynamic_write(cmd_id, ...) ({ \
    rspq_write(RDPQ_OVL_ID, (cmd_id), ##__VA_ARGS__); \
})

#define rdpq_static_write(cmd_id, arg0, ...) ({ \
    volatile uint32_t *ptr = rdpq_block_pointer; \
    *ptr++ = (RDPQ_OVL_ID + ((cmd_id)<<24)) | (arg0); \
    __CALL_FOREACH(_rdpq_write_arg, ##__VA_ARGS__); \
    rdpq_block_flush((uint32_t*)rdpq_block_pointer, (uint32_t*)ptr); \
    rdpq_block_pointer = ptr; \
    if (__builtin_expect(rdpq_block_pointer > rdpq_block_sentinel, 0)) \
        rdpq_block_next_buffer(); \
})

static inline bool in_block(void) {
    return rdpq_block != NULL;
}

#define rdpq_write(cmd_id, arg0, ...) ({ \
    if (in_block()) { \
        rdpq_static_write(cmd_id, arg0, ##__VA_ARGS__); \
    } else { \
        rdpq_dynamic_write(cmd_id, arg0, ##__VA_ARGS__); \
    } \
})

void rdpq_fill_triangle(bool flip, uint8_t level, uint8_t tile, int16_t yl, int16_t ym, int16_t yh, int32_t xl, int32_t dxldy, int32_t xh, int32_t dxhdy, int32_t xm, int32_t dxmdy)
{
    rdpq_write(RDPQ_CMD_TRI,
        _carg(flip ? 1 : 0, 0x1, 23) | _carg(level, 0x7, 19) | _carg(tile, 0x7, 16) | _carg(yl, 0x3FFF, 0),
        _carg(ym, 0x3FFF, 16) | _carg(yh, 0x3FFF, 0),
        xl,
        dxldy,
        xh,
        dxhdy,
        xm,
        dxmdy);
}

void rdpq_texture_rectangle(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t ds, int16_t dt)
{
    rdpq_write(RDPQ_CMD_TEXTURE_RECTANGLE,
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s, 0xFFFF, 16) | _carg(t, 0xFFFF, 0),
        _carg(ds, 0xFFFF, 16) | _carg(dt, 0xFFFF, 0));
}

void rdpq_texture_rectangle_flip(uint8_t tile, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t s, int16_t t, int16_t ds, int16_t dt)
{
    rdpq_write(RDPQ_CMD_TEXTURE_RECTANGLE_FLIP,
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0),
        _carg(s, 0xFFFF, 16) | _carg(t, 0xFFFF, 0),
        _carg(ds, 0xFFFF, 16) | _carg(dt, 0xFFFF, 0));
}

void rdpq_sync_load()
{
    rdpq_write(RDPQ_CMD_SYNC_LOAD, 0, 0);
}

void rdpq_sync_pipe()
{
    rdpq_write(RDPQ_CMD_SYNC_PIPE, 0, 0);
}

void rdpq_sync_tile()
{
    rdpq_write(RDPQ_CMD_SYNC_TILE, 0, 0);
}

void rdpq_sync_full()
{
    rdpq_write(RDPQ_CMD_SYNC_FULL, 0, 0);
}

void rdpq_set_key_gb(uint16_t wg, uint8_t wb, uint8_t cg, uint16_t sg, uint8_t cb, uint8_t sb)
{
    rdpq_write(RDPQ_CMD_SET_KEY_GB,
        _carg(wg, 0xFFF, 12) | _carg(wb, 0xFFF, 0),
        _carg(cg, 0xFF, 24) | _carg(sg, 0xFF, 16) | _carg(cb, 0xFF, 8) | _carg(sb, 0xFF, 0));
}

void rdpq_set_key_r(uint16_t wr, uint8_t cr, uint8_t sr)
{
    rdpq_write(RDPQ_CMD_SET_KEY_R,
        0,
        _carg(wr, 0xFFF, 16) | _carg(cr, 0xFF, 8) | _carg(sr, 0xFF, 0));
}

void rdpq_set_convert(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3, uint16_t k4, uint16_t k5)
{
    rdpq_write(RDPQ_CMD_SET_CONVERT,
        _carg(k0, 0x1FF, 13) | _carg(k1, 0x1FF, 4) | (((uint32_t)(k2 & 0x1FF)) >> 5),
        _carg(k2, 0x1F, 27) | _carg(k3, 0x1FF, 18) | _carg(k4, 0x1FF, 9) | _carg(k5, 0x1FF, 0));
}

void __rdpq_set_scissor(uint32_t w0, uint32_t w1)
{
    rdpq_write(RDPQ_CMD_SET_SCISSOR_EX, w0, w1);
}

void rdpq_set_prim_depth(uint16_t primitive_z, uint16_t primitive_delta_z)
{
    rdpq_write(RDPQ_CMD_SET_PRIM_DEPTH,
        0,
        _carg(primitive_z, 0xFFFF, 16) | _carg(primitive_delta_z, 0xFFFF, 0));
}

void rdpq_set_other_modes(uint64_t modes)
{
    rdpq_write(RDPQ_CMD_SET_OTHER_MODES, 
        ((modes >> 32) & 0x00FFFFFF),
        modes & 0xFFFFFFFF);
}

void rdpq_modify_other_modes(uint32_t offset, uint32_t inverse_mask, uint32_t value)
{
    rdpq_write(RDPQ_CMD_MODIFY_OTHER_MODES, 
        offset & 0x4,
        inverse_mask,
        value);
}

void rdpq_load_tlut(uint8_t tile, uint8_t lowidx, uint8_t highidx)
{
    rdpq_write(RDPQ_CMD_LOAD_TLUT,
        _carg(lowidx, 0xFF, 14),
        _carg(tile, 0x7, 24) | _carg(highidx, 0xFF, 14));
}

void rdpq_set_tile_size(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1)
{
    rdpq_write(RDPQ_CMD_SET_TILE_SIZE,
        _carg(s0, 0xFFF, 12) | _carg(t0, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(s1, 0xFFF, 12) | _carg(t1, 0xFFF, 0));
}

void rdpq_load_block(uint8_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t dxt)
{
    rdpq_write(RDPQ_CMD_LOAD_BLOCK,
        _carg(s0, 0xFFF, 12) | _carg(t0, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(s1, 0xFFF, 12) | _carg(dxt, 0xFFF, 0));
}

void rdpq_load_tile(uint8_t tile, int16_t s0, int16_t t0, int16_t s1, int16_t t1)
{
    rdpq_write(RDPQ_CMD_LOAD_TILE,
        _carg(s0, 0xFFF, 12) | _carg(t0, 0xFFF, 0),
        _carg(tile, 0x7, 24) | _carg(s1, 0xFFF, 12) | _carg(t1, 0xFFF, 0));
}

void rdpq_set_tile(uint8_t format, uint8_t size, uint16_t line, uint16_t tmem_addr,
                      uint8_t tile, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t,
                      uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s)
{
    rdpq_write(RDPQ_CMD_SET_TILE,
        _carg(format, 0x7, 21) | _carg(size, 0x3, 19) | _carg(line, 0x1FF, 9) | _carg(tmem_addr, 0x1FF, 0),
        _carg(tile, 0x7, 24) | _carg(palette, 0xF, 20) | _carg(ct, 0x1, 19) | _carg(mt, 0x1, 18) | _carg(mask_t, 0xF, 14) | 
        _carg(shift_t, 0xF, 10) | _carg(cs, 0x1, 9) | _carg(ms, 0x1, 8) | _carg(mask_s, 0xF, 4) | _carg(shift_s, 0xF, 0));
}

void rdpq_fill_rectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    rdpq_write(RDPQ_CMD_FILL_RECTANGLE,
        _carg(x1, 0xFFF, 12) | _carg(y1, 0xFFF, 0),
        _carg(x0, 0xFFF, 12) | _carg(y0, 0xFFF, 0));
}

void __rdpq_set_fill_color(uint32_t color)
{
    rdpq_write(RDPQ_CMD_SET_FILL_COLOR, 0, color);
}

void __rdpq_set_fill_color32(uint32_t color)
{
    rdpq_write(RDPQ_CMD_SET_FILL_COLOR_32, 0, color);
}

void rdpq_set_fill_color(color_t color);
void rdpq_set_fill_color_pattern(color_t color1, color_t color2);


void rdpq_set_fog_color(uint32_t color)
{
    rdpq_write(RDPQ_CMD_SET_FOG_COLOR,
        0,
        color);
}

void rdpq_set_blend_color(uint32_t color)
{
    rdpq_write(RDPQ_CMD_SET_BLEND_COLOR,
        0,
        color);
}

void rdpq_set_prim_color(uint32_t color)
{
    rdpq_write(RDPQ_CMD_SET_PRIM_COLOR,
        0,
        color);
}

void rdpq_set_env_color(uint32_t color)
{
    rdpq_write(RDPQ_CMD_SET_ENV_COLOR,
        0,
        color);
}

void rdpq_set_combine_mode(uint64_t flags)
{
    rdpq_write(RDPQ_CMD_SET_COMBINE_MODE, 
        (flags >> 32) & 0x00FFFFFF, 
        flags & 0xFFFFFFFF);
}

void rdpq_set_texture_image(uint32_t dram_addr, uint8_t format, uint8_t size, uint16_t width)
{
    rdpq_write(RDPQ_CMD_SET_TEXTURE_IMAGE,
        _carg(format, 0x7, 21) | _carg(size, 0x3, 19) | _carg(width, 0x3FF, 0),
        dram_addr & 0x1FFFFFF);
}

void rdpq_set_z_image(uint32_t dram_addr)
{
    rdpq_write(RDPQ_CMD_SET_Z_IMAGE,
        0,
        dram_addr & 0x1FFFFFF);
}

void rdp_set_color_image_internal(uint32_t arg0, uint32_t arg1)
{
    if (in_block()) {
        rdpq_static_write(RDPQ_CMD_SET_COLOR_IMAGE_FIXUP, arg0, arg1);
    } else {
        rdpq_dynamic_write(RDPQ_CMD_SET_COLOR_IMAGE, arg0, arg1);
    }
}

void rdpq_set_color_image(uint32_t dram_addr, uint32_t format, uint32_t size, uint32_t width)
{
    rdp_set_color_image_internal(
        _carg(format, 0x7, 21) | _carg(size, 0x3, 19) | _carg(width, 0x3FF, 0),
        dram_addr & 0x1FFFFFF);
}
