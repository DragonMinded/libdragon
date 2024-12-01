#include "vi.h"
#include "vi_internal.h"
#include "interrupt.h"
#include "surface.h"
#include "n64sys.h"
#include "utils.h"
#include "debug.h"
#include "kernel/kernel_internal.h"
#include "kirq.h"
#include <stdbool.h>
#include <assert.h>

/** @brief Preset of settings for a certain TV type */
typedef struct vi_preset_s {
    int clock;                  ///< Pixel clock in Hz
    uint32_t vi_h_total;        ///< Total horizontal length (in 1/4th pixels)
    uint32_t vi_h_total_leap;   ///< Leap setting (alternate scanline lengths)
    uint32_t vi_v_total;        ///< Total vertical length (in scanlines)
    uint32_t vi_burst;          ///< Horizontal Burst settings
    uint32_t vi_v_burst;        ///< Vertical burst settings
    struct {
        int x0;                 ///< Default active area x0
        int y0;                 ///< Default active area y0
        int width;              ///< Default active area width
        int height;             ///< Default active area height (in half-lines)
    } display;                  ///< Default active area
} vi_preset_t;

/**
 * @brief Presets to begin with when setting a particular TV type
 */
static const vi_preset_t vi_presets[3] = {
    [TV_NTSC] = {
        .clock = 48681818,
        .vi_h_total = VI_H_TOTAL_SET(0b00000, 3093),
        .vi_h_total_leap = VI_H_TOTAL_LEAP_SET(3093, 3093),
        .vi_v_total = VI_V_TOTAL_SET(525),
        .vi_burst = VI_BURST_SET(62, 5, 34, 57),
        .vi_v_burst = VI_V_BURST_SET(14, 516),
        .display = {
            .x0 = 108, .y0 = 35,
            .width = 640, .height = 480,
        },
    },
    [TV_PAL] = {
        .clock = 49656530,
        .vi_h_total = VI_H_TOTAL_SET(0b10101, 3177),
        .vi_h_total_leap = VI_H_TOTAL_LEAP_SET(3183, 3182),
        .vi_v_total = VI_V_TOTAL_SET(625),
        .vi_burst = VI_BURST_SET(64, 4, 35, 58),
        .vi_v_burst = VI_V_BURST_SET(9, 619),
        .display = {
            .x0 = 128, .y0 = 45,
            .width = 640, .height = 576,
        },
    },
    [TV_MPAL] = {
        .clock = 48628322,
        .vi_h_total = VI_H_TOTAL_SET(0b00100, 3089),
        .vi_h_total_leap = VI_H_TOTAL_LEAP_SET(3097, 3098),
        .vi_v_total = VI_V_TOTAL_SET(525),
        .vi_burst = VI_BURST_SET(70, 5, 30, 57),
        .vi_v_burst = VI_V_BURST_SET(14, 516),
        .display = {
            .x0 = 108, .y0 = 35,
            .width = 640, .height = 480,
        },
    },
};

/** @brief Current VI state */
typedef struct vi_state_s {
    vi_config_t cfg;                ///< Current VI configuration
    const vi_preset_t *preset;      ///< Active TV preset
    uint16_t cfg_pending;           ///< Pending register changes (1 bit per each VI register)
    volatile int cfg_refcount;      ///< Number of active write transactions
    bool pending_blank;             ///< True if blanking was requested
} vi_state_t;

static vi_state_t vi;

static void __vi_interrupt(void)
{
    // Apply any pending changes (unless suspended because
    // a larger write is ongoing). Notice that speed wise,
    // we could simply rewrite the whole state into the hardware,
    // but we opt for writing only registers that actually changed
    // because 1) we don't fully trust the VI hardware yet, and
    // 2) we want to still allow user code to manually bang the
    // registers, if anything for increased backward compatibility.
    if (vi.cfg_refcount == 0) {
        bool debug = false;
        for (int idx=0; vi.cfg_pending; idx++) {
            if (vi.cfg_pending & (1 << idx)) {
                VI_REGISTERS[idx] = vi.cfg.regs[idx];
                vi.cfg_pending ^= 1 << idx;
                debug = true;
            }
        }
        if (vi.pending_blank) {
            *VI_H_VIDEO = 0;
            vi.pending_blank = false;
        }
        if (debug) {
            debugf("VI regs:\n");
            for (int i=0; i<VI_REGISTERS_COUNT; i++) {
                debugf("  %08lx ", vi.cfg.regs[i]);
                if ((i & 3) == 3) debugf("\n");
            }
            debugf("\n");
        }
    }

    // VI adjustments in case of interlacing
    uint32_t ctrl = vi_read(VI_CTRL);
    if (ctrl & VI_CTRL_SERRATE) {
        int field = *VI_V_CURRENT & 1;
    
        // When the field changes, we adjust VI_ORIGIN by one scanline.
        int offset = 0;
        if (field == 0) {
            int bpp_shift = (vi_read(VI_CTRL) & VI_CTRL_TYPE) - 1;
            offset = vi_read(VI_WIDTH) << bpp_shift;
        } else {
            *VI_ORIGIN = vi_read(VI_ORIGIN) + offset;
        }
    
        // Workaround for a PAL-M VI bug on old boards like NUS-CPU-02. 
        // On those consoles, V_BURST must be changed every field,
        // otherwise the image seems garbled at the top.
        // It is probably a bug in old revisions of the VI chip,
        // since the problem doesn't exist on newer boards.
        if (get_tv_type() == TV_MPAL) {
            *VI_V_BURST ^= 0x000b0202 ^ 0x000e0204;
        }
    }
}

uint32_t vi_read(volatile uint32_t *reg) {
    return vi.cfg.regs[VI_TO_INDEX(reg)];
}

void vi_write_begin(void)
{
    vi.cfg_refcount++;
}

static void vi_write_maybe_flush(void)
{
    if (vi.cfg_refcount > 0) return;

    // Check if we are in vblank now, and if so, we can apply the changes
    // immediately. Notice that this is not just a latency optimization:
    // it is mandatory when VI is disabled (VI_CTRL=0, which makes VI_V_CURRENT=0),
    // because the VI does not generate interrupts in that case.
    disable_interrupts();
    int first_line = *VI_V_VIDEO >> 16;
    if ((*VI_V_CURRENT & ~1) < MAX(first_line-2, 2)) {
        __vi_interrupt();
    }
    enable_interrupts();
}

void vi_write_end(void)
{
    assertf(vi.cfg_refcount > 0, "vi_write_end without matching begin");
    --vi.cfg_refcount;
    vi_write_maybe_flush();
}

static void vi_write_idx_masked(int reg_idx, uint32_t wmask, uint32_t value)
{
    assert(reg_idx >= 0 && reg_idx < VI_REGISTERS_COUNT);
    assert((value & ~wmask) == 0);
    disable_interrupts();
    vi.cfg.regs[reg_idx] = (vi.cfg.regs[reg_idx] & ~wmask) | value;
    vi.cfg_pending |= 1 << reg_idx;
    vi_write_maybe_flush();
    enable_interrupts();
}

void vi_write_masked(volatile uint32_t *reg, uint32_t wmask, uint32_t value)
{
    vi_write_idx_masked(VI_TO_INDEX(reg), wmask, value);
}

int vi_get_display_width(void)
{
    uint32_t h_video = vi_read(VI_H_VIDEO);
    return (h_video & 0xffff) - (h_video >> 16);
}

int vi_get_display_height(void)
{
    uint32_t v_video = vi_read(VI_V_VIDEO);
    return (v_video & 0xffff) - (v_video >> 16);
}

void vi_set_origin(void *buffer, int width, int bpp)
{
    assertf(PhysicalAddr(buffer) % 8 == 0, "Invalid VI buffer alignment: %p", buffer);
    assertf(bpp == 16 || bpp == 32, "Invalid VI buffer bpp: %d (must be 16 or 32)", bpp);
    vi_write_begin();
    vi_write(VI_ORIGIN, PhysicalAddr(buffer));
    vi_write(VI_WIDTH, width);
    vi_write_masked(VI_CTRL, VI_CTRL_TYPE, bpp == 16 ? VI_CTRL_TYPE_16_BPP : VI_CTRL_TYPE_32_BPP);
    vi_write_end();
}

void vi_set_xscale(float fb_width)
{
    int vi_width = vi_get_display_width();
    vi_write_masked(VI_X_SCALE, 0xFFFF, VI_X_SCALE_SET(fb_width, vi_width));
}

void vi_set_yscale(float fb_height)
{
    int vi_height = vi_get_display_height() / 2;
    vi_write_masked(VI_Y_SCALE, 0xFFFF, VI_Y_SCALE_SET(fb_height, vi_height));
}

void vi_show(surface_t *fb)
{
    if (!fb) {
        vi_write_begin();
        vi_write(VI_ORIGIN, 0);
        vi_write(VI_WIDTH, 0);
        vi_write_masked(VI_CTRL, VI_CTRL_TYPE, VI_CTRL_TYPE_BLANK);
        vi_write_end();
        return;
    }

    tex_format_t fmt = surface_get_format(fb);
    assert(fmt == FMT_RGBA16 || fmt == FMT_RGBA32);
    assert(TEX_FORMAT_PIX2BYTES(fmt, fb->width) == fb->stride);
    assert(PhysicalAddr(fb->buffer) % 8 == 0);
    assert(fb->stride % 8 == 0);
    vi_write_begin();
    vi_set_origin(fb->buffer, fb->width, fmt == FMT_RGBA16 ? 16 : 32);
    vi_set_xscale(fb->width);
    vi_set_yscale(fb->height);
    vi_write_end();
}

void vi_set_interlaced(bool interlaced)
{
    vi_write_begin();
    vi_write_masked(VI_CTRL, VI_CTRL_SERRATE, interlaced ? VI_CTRL_SERRATE : 0);
    // Progressive mode has one additional vertical half-line (the last odd one).
    // So we need to adjust the LSB of V_TOTAL depending on the interlaced mode.
    vi_write_masked(VI_V_TOTAL, 0x1, interlaced ? 0 : 1);
    vi_write_end();
}

float vi_get_refresh_rate(void)
{
    int clock = vi.preset->clock;
    uint32_t HTOTAL = vi_read(VI_H_TOTAL);
    uint32_t VTOTAL = vi_read(VI_V_TOTAL);
    uint32_t HTOTAL_LEAP = vi_read(VI_H_TOTAL_LEAP);

    int h_total = (HTOTAL & 0xFFF) + 1;
    int v_total = (VTOTAL & 0x3FF) + 1;
    int h_total_leap_b = (HTOTAL_LEAP >>  0) & 0xFFF;
    int h_total_leap_a = (HTOTAL_LEAP >> 16) & 0xFFF;
    int leap_bitcount = 0;
    int mask = 1<<16;
    for (int i=0; i<5; i++) {
        if (HTOTAL & mask) leap_bitcount++;
        mask <<= 1;
    }
    int h_total_leap_avg = (h_total_leap_a * leap_bitcount + h_total_leap_b * (5 - leap_bitcount)) / 5;

    return (float)clock / ((h_total * (v_total - 2) / 2 + h_total_leap_avg));
}

static void __get_output(int *x0, int *y0, int *x1, int *y1)
{
    uint32_t h_video = vi_read(VI_H_VIDEO);
    uint32_t v_video = vi_read(VI_V_VIDEO);

    *x0 = h_video >> 16; *x1 = h_video & 0xFFFF;
    *y0 = v_video >> 16; *y1 = v_video & 0xFFFF;
}

static void __set_output(int x0, int y0, int x1, int y1)
{
    vi_write_begin();
    vi_write(VI_H_VIDEO, VI_H_VIDEO_SET(x0, x1));
    vi_write(VI_V_VIDEO, VI_V_VIDEO_SET(y0, y1));
    vi_write_end();

    debugf("VI output: %d-%d %d-%d\n", x0, x1, y0, y1);
}

void vi_get_output(int *x0, int *y0, int *x1, int *y1)
{
    __get_output(x0, y0, x1, y1);
}

void vi_get_output_bounds(int *x0, int *y0, int *x1, int *y1)
{
    uint32_t H_TOTAL = vi_read(VI_H_TOTAL);
    uint32_t V_TOTAL = vi_read(VI_V_TOTAL);
    uint32_t BURST = vi_read(VI_BURST);
    int burst_start = (BURST >> 20) & 0x3FF;
    int vsync_height = (BURST >> 16) & 0xF;
    int burst_width = (BURST >> 8) & 0xFF;
    int htotal = ((H_TOTAL & 0xFFF) + 1) / 4;
    int vtotal = (V_TOTAL & 0x3FF) + 1;

    // On the left, the display area is bound by the color burst. If the display
    // area is configured to begin before the color burst is finished, it will
    // interrupt it, causing chrominance artifacts.
    *x0 = burst_start + burst_width;

    // On the top, the display area is bound by the initial vsync period. Notice
    // that the vertical color burst enable/disable 
    *y0 = vsync_height;

    // On the right/bottom, the display area is bound by the total picture length.
    // The bounds are exclusive but V_VIDEO is compared to V_CURRENT to decide
    // when to statp the display, and V_CURRENT will never be V_TOTAL (it'll wrap
    // around). This means that it's impossible to draw on the last line.
    *x1 = htotal;
    *y1 = vtotal - 1;
}

void vi_set_output(int x0, int y0, int x1, int y1)
{
    int bounds[4];
    vi_get_output_bounds(&bounds[0], &bounds[2], &bounds[1], &bounds[3]);

    if (x0 > bounds[1] || x1 < bounds[0] || y0 > bounds[3] || y1 < bounds[2]) {
        // If the image is totally out of bounds, just force zero display are
        // so that nothing is displayed.
        x0 = x1 = y0 = y1 = 0;
    } else {
        // If the image is partially out of bounds, crop it.
        if (x0 < bounds[0]) {
            x1 += bounds[0] - x0;
            x0 = bounds[0];
        }
        if (x1 > bounds[1]) {
            x0 -= x1 - bounds[1];
            x1 = bounds[1];
        }
        if (y0 < bounds[2]) {
            y1 += bounds[2] - y0;
            y0 = bounds[2];
        }
        if (y1 > bounds[3]) {
            y0 -= y1 - bounds[3];
            y1 = bounds[3];
        }
    }

    __set_output(x0, y0, x1, y1);
}

void vi_set_borders(vi_borders_t b)
{
    int x0 = vi.preset->display.x0, x1 = x0 + vi.preset->display.width;
    int y0 = vi.preset->display.y0, y1 = y0 + vi.preset->display.height;

    x0 += b.left;
    x1 -= b.right;
    y0 += b.up;
    y1 -= b.down;

    vi_set_output(x0, y0, x1, y1);
}

vi_borders_t vi_get_borders(void)
{
    int x0, y0, x1, y1;
    __get_output(&x0, &y0, &x1, &y1);

    vi_borders_t b;
    b.left  = +(x0 - vi.preset->display.x0);
    b.right = -(x1 - (vi.preset->display.x0 + vi.preset->display.width));
    b.up    = +(y0 - vi.preset->display.y0);
    b.down  = -(y1 - (vi.preset->display.y0 + vi.preset->display.height));
    return b;
}

vi_borders_t vi_calc_borders(float aspect_ratio, float overscan_margin)
{
    const int vi_width = vi.preset->display.width;
    const int vi_height = vi.preset->display.height;
    const float vi_par = (float)vi_width / vi_height;
    const float vi_dar = 4.0f / 3.0f;
    float correction = (aspect_ratio / vi_dar) * vi_par;

    vi_borders_t b;
    b.left = b.right = vi_width * overscan_margin;
    b.up = b.down = vi_height * overscan_margin;

    int width = vi_width - b.left - b.right;
    int height = vi_height - b.up - b.down;

    if (correction > 1) {
        int vborders = (int)(height - width / correction + 0.5f);
        b.up += vborders / 2;
        b.down += vborders / 2;
    } else {
        int hborders = (int)(width - height * correction + 0.5f);
        b.left += hborders / 2;
        b.right += hborders / 2;
    }

    return b;
}

void vi_move_output(int x, int y)
{
    int x0, y0, x1, y1;
    __get_output(&x0, &y0, &x1, &y1);
    
    int dx = x1 - x0, dy = y1 - y0;
    vi_write_begin();
    x0 += dx; x1 += dx;
    y0 += dy; y1 += dy;
    vi_set_output(x0, y0, x1, y1);
    vi_write_end();
}

void vi_scroll_output(int dx, int dy)
{
    int x0, y0, x1, y1;
    __get_output(&x0, &y0, &x1, &y1);

    vi_write_begin();
    x0 += dx; x1 += dx;
    y0 += dy; y1 += dy;
    vi_set_output(x0, y0, x1, y1);
    vi_write_end();
}

void vi_blank(bool set_blank)
{
    if (set_blank)
        vi.pending_blank = true;
    else
        vi_write(VI_H_VIDEO, vi_read(VI_H_VIDEO));
}

void vi_wait_vblank(void)
{
    uint32_t ctrl = vi_read(VI_CTRL);
    if ((ctrl & VI_CTRL_TYPE) == VI_CTRL_TYPE_BLANK)
        return;

    if (__kernel) {
        kirq_wait_t w = kirq_begin_wait_vi();
        kirq_wait(&w);
        return;
    } else {
        while ((*VI_V_CURRENT & ~1) != VI_V_CURRENT_VBLANK) {}
    }
}

void vi_init(void)
{
    static bool inited = false;
    if (inited) return;
    inited = true;

    // Initialize the preset to the current TV type (progressive mode),
    // and set the pending mask to all registers, so that the whole
    // VI will be programmed at next vblank.
    memset(&vi, 0, sizeof(vi_state_t));
    vi.preset = &vi_presets[get_tv_type()];

    vi_write_begin();

    // Configure the timing registers from the preset. These will not change
    // at runtime as they are fixed by the TV standard.
    vi_write(VI_H_TOTAL, vi.preset->vi_h_total);
    vi_write(VI_H_TOTAL_LEAP, vi.preset->vi_h_total_leap);
    vi_write(VI_V_TOTAL, vi.preset->vi_v_total);
    vi_write(VI_BURST, vi.preset->vi_burst);
    vi_write(VI_V_BURST, vi.preset->vi_v_burst);

    // Configure the default display area from the preset.
    __set_output(vi.preset->display.x0, vi.preset->display.y0,
                        vi.preset->display.x0 + vi.preset->display.width,
                        vi.preset->display.y0 + vi.preset->display.height);

    uint32_t ctrl = 0;
    ctrl |= !sys_bbplayer() ? VI_PIXEL_ADVANCE_DEFAULT : VI_PIXEL_ADVANCE_BBPLAYER;
    vi_write(VI_CTRL, ctrl);

    // Mark all registers as pending, so that they will be written at next vblank.
    // This make sure we fully reset the VI.
    vi.cfg_pending = (1 << VI_REGISTERS_COUNT) - 1;

    vi_write_end();

    disable_interrupts();
    register_VI_handler(__vi_interrupt);
    set_VI_interrupt(1, VI_V_CURRENT_VBLANK);
    enable_interrupts();
}
