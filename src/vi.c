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
        .vi_h_total = VI_H_TOTAL_SET(0b00000, 773.5),
        .vi_h_total_leap = VI_H_TOTAL_LEAP_SET(773.5, 773.5),
        .vi_v_total = VI_V_TOTAL_SET(526),
        .vi_burst = VI_BURST_SET(62, 5, 34, 57),
        .vi_v_burst = VI_V_BURST_SET(14, 516),
        .display = {
            .x0 = 108, .y0 = 35,
            .width = 640, .height = 480,
        },
    },
    [TV_PAL] = {
        .clock = 49656530,
        .vi_h_total = VI_H_TOTAL_SET(0b10101, 794.5),
        .vi_h_total_leap = VI_H_TOTAL_LEAP_SET(796.0, 795.75),
        .vi_v_total = VI_V_TOTAL_SET(626),
        .vi_burst = VI_BURST_SET(64, 4, 35, 58),
        .vi_v_burst = VI_V_BURST_SET(9, 619),
        .display = {
            .x0 = 128, .y0 = 45,
            .width = 640, .height = 576,
        },
    },
    [TV_MPAL] = {
        .clock = 48628322,
        .vi_h_total = VI_H_TOTAL_SET(0b00000, 772.25),
        .vi_h_total_leap = VI_H_TOTAL_LEAP_SET(775.25, 775.25),
        .vi_v_total = VI_V_TOTAL_SET(526),
        .vi_burst = VI_BURST_SET(70, 5, 30, 57),
        .vi_v_burst = VI_V_BURST_SET(14, 516),
        .display = {
            .x0 = 108, .y0 = 35,
            .width = 640, .height = 480,
        },
    },
};

/** @brief Line interrupt callbacks */
typedef struct {
    int line;                          ///< Line number
    void (*handler)(void);             ///< Callback function
} line_irqs_t;

#define MAX_LINE_IRQS  16                       ///< Maximum number of line interrupts
line_irqs_t line_irqs[MAX_LINE_IRQS] = {0};     ///< Line interrupt callbacks
line_irqs_t new_line_irqs[MAX_LINE_IRQS] = {0}; ///< New line interrupt callbacks

uint32_t __vi_cfg[VI_REGISTERS_COUNT]; ///< Current VI configuration
static const vi_preset_t *preset;      ///< Active TV preset
static uint16_t cfg_pending;           ///< Pending register changes (1 bit per each VI register)
static uint16_t cfg_raster;            ///< Raster register changes (1 bit per each VI register)
static bool cfg_pending_lineirqs;      ///< True if line IRQs have been changed
static volatile int cfg_refcount;      ///< Number of active write transactions
static bool pending_blank;             ///< True if blanking was requested
static line_irqs_t *cur_line_irq;      ///< Current line IRQ pointer
static int interlacing_parms[2];       ///< Interlaced parameters (offsets for ORIGIN, YSCALE)

static void __vi_validate_config(void)
{
    #ifdef NDEBUG
    // Disable validation in NDEBUG builds
    return;
    #endif

    // Check for some common mistakes in VI configuration. Since they are based
    // on VI_CTRL, VI_X_SCALE and VI_H_VIDEO, do that only if they have been changed.
    if (!(cfg_pending & ((1 << VI_TO_INDEX(VI_CTRL)) | 
                         (1 << VI_TO_INDEX(VI_X_SCALE)) |
                         (1 << VI_TO_INDEX(VI_H_VIDEO)))))
        return;

    uint32_t ctrl = vi_read(VI_CTRL); 
    uint32_t xscale = vi_read(VI_X_SCALE);
    uint32_t hstart = vi_read(VI_H_VIDEO) >> 16;
    bool bpp16 = (ctrl & VI_CTRL_TYPE) == VI_CTRL_TYPE_16_BPP;
    bool dedither = ctrl & VI_DEDITHER_FILTER_ENABLE;
    bool divot = ctrl & VI_DIVOT_ENABLE;
    int mode = ctrl & VI_AA_MODE_MASK;

    switch (mode) {
    case VI_AA_MODE_NONE:
        if (xscale <= 0x200 && bpp16 && hstart <= 128) {
            debugf("VI WARNING: setting VI_AA_MODE_NONE with 16 bpp, X_SCALE <= 0x200 and H_START <= 128 can cause visual artifacts\n");
            debugf("A common scenario where this happens: NTSC units, with default output area, and 320x240 framebuffer.\n");
            debugf("Possible workarounds: activate resampling with VI_AA_MODE_RESAMPLE, increase X_SCALE\n");
        }
        if (divot)
            debugf("VI WARNING: divot filter is only useful when the AA filter is enabled\n");
        break;

    case VI_AA_MODE_RESAMPLE:
        if (dedither)
            debugf("VI WARNING: setting VI_AA_MODE_RESAMPLE with dedithering can cause artifacts\n");
        if (divot)
            debugf("VI WARNING: divot filter is only useful when the AA filter is enabled\n");
        break;

    case VI_AA_MODE_RESAMPLE_FETCH_NEEDED:
        if (xscale > 0x280)
            debugf("VI WARNING: setting VI_AA_MODE_RESAMPLE_FETCH_NEEDED with VI_X_SCALE >= 0x280 (aka: framebuffer widths > 400) can cause artifacts\n");
        break;

    case VI_AA_MODE_RESAMPLE_FETCH_ALWAYS:
        if (!bpp16)
            debugf("VI WARNING: setting VI_AA_MODE_RESAMPLE_FETCH_ALWAYS with 32 bpp can often cause image corruption\n");
        break;
    }

    if (dedither) {
        if (!bpp16)
            debugf("VI WARNING: dedithering is only useful with 16 bpp\n");
        if (mode != VI_AA_MODE_RESAMPLE_FETCH_ALWAYS)
            debugf("VI WARNING: dedithering requires VI_AA_MODE_RESAMPLE_FETCH_ALWAYS\n");
    }
}

static void __vblank_interrupt(void)
{
    // Always rewrite registers for which raster effects are activated,
    // so that they get reset at each vblank
    uint32_t writeregs = cfg_raster;

    // Apply any pending changes (unless suspended because
    // a larger write is ongoing). Notice that speed wise,
    // we could simply rewrite the whole state into the hardware,
    // but we opt for writing only registers that actually changed
    // because 1) we don't fully trust the VI hardware yet, and
    // 2) we want to still allow user code to manually bang the
    // registers, if anything for increased backward compatibility.
    if (UNLIKELY(cfg_pending)) {
        if (cfg_refcount == 0) {
            writeregs |= cfg_pending;
            cfg_pending = 0;
        }
    }
    
    if (UNLIKELY(writeregs)) {
        uint32_t to_write = writeregs;
        for (int idx=0; to_write; idx++) {
            if (to_write & (1 << idx)) {
                VI_REGISTERS[idx] = __vi_cfg[idx];
                to_write ^= (1 << idx);
            }
        }
    }

    if (UNLIKELY(pending_blank)) {
        *VI_H_VIDEO = 0;
        pending_blank = false;
    }

    // VI adjustments in case of serration, to achieve the interlaced effect.
    uint32_t ctrl = vi_read(VI_CTRL);
    if (UNLIKELY(ctrl & VI_CTRL_SERRATE)) {
        int field = *VI_V_CURRENT & 1;

        // See if we need to recalculate the interlacing parameters
        if (UNLIKELY(writeregs & (1 << VI_TO_INDEX(VI_Y_SCALE) | 1 << VI_TO_INDEX(VI_WIDTH) | 1 << VI_TO_INDEX(VI_CTRL)))) {
            uint32_t Y_SCALE = vi_read(VI_Y_SCALE);
            int yoffset = (Y_SCALE >> 16) & 0x3FF;
            int yscale = Y_SCALE & 0xFFFF;
            int origin_offset = 0;
            
            // Serration moves the odd field (field==0) by 1/2 line down. We
            // want to counter-adjust this movement by moving the field up.
            // We can use the Y_OFFSET to do so and the correct amount is the
            // one framebuffer Y advanceent offset, which is YSCALE. So we 
            // adjust Y_OFFSET by Y_SCALE / 2
            yoffset += yscale >> 1;

            // Y_OFFSET is 0.10 so it can't represent values above 0x3FF (0.9999).
            // Luckily, for whole scanlines, we can just move the framebuffer origin.
            if (yoffset > 0x3FF) {
                int num_lines = yoffset >> 10;
                int bpp_shift = (vi_read(VI_CTRL) & VI_CTRL_TYPE) - 1;
                origin_offset = (vi_read(VI_WIDTH) * num_lines) << bpp_shift;
                yoffset &= 0x3FF;
            }

            int new_yscale = (yscale & 0xFFFF) | (yoffset << 16);
            interlacing_parms[0] = origin_offset;
            interlacing_parms[1] = new_yscale - Y_SCALE;
        }

        // Apply interlacing adjustments during field 0. The parameters
        // were already calculated so we just need to adjust VI_ORIGIN / VI_YSCALE
        uint32_t origin = vi_read(VI_ORIGIN);
        uint32_t yscale = vi_read(VI_Y_SCALE);
        if (field == 0) {
            origin += interlacing_parms[0];
            yscale += interlacing_parms[1];
        } 
        *VI_ORIGIN = origin;
        *VI_Y_SCALE = yscale;

        // Workaround for a PAL-M VI bug on old boards like NUS-CPU-02. 
        // On those consoles, V_BURST must be changed every field,
        // otherwise the image seems garbled at the top.
        // It is probably a bug in old revisions of the VI chip,
        // since the problem doesn't exist on newer boards.
        if (UNLIKELY(get_tv_type() == TV_MPAL)) {
            *VI_V_BURST ^= VI_V_BURST_SET(11, 514) ^ VI_V_BURST_SET(14, 516);
        }
    }
}

void __vi_interrupt(void)
{
    // Check if there are pending line interrupts to install on this frame.
    // We do this only on the beginning of a frame, and only if cfg_refcount
    // is 0, so that we are synchronized with other VI changes and we respect
    // vi_write_begin().
    if (UNLIKELY(cfg_pending_lineirqs) && cfg_refcount == 0) {
        for (int i=0; i<MAX_LINE_IRQS; i++)
            line_irqs[i] = new_line_irqs[i];
        cfg_pending_lineirqs = false;
    }

    void (*handler)(void) = cur_line_irq->handler;
    cur_line_irq++;
    if (cur_line_irq->line == 0)
        cur_line_irq = line_irqs;
    *VI_V_INTR = cur_line_irq->line;
    handler();
}

void vi_write_begin(void)
{
    cfg_refcount++;
}

static void vi_write_maybe_flush(void)
{
    if (cfg_refcount > 0) return;

    // Validate the configuration and emit warnings if needed
    __vi_validate_config();

    // Check if we are in vblank now, and if so, we can apply the changes
    // immediately. Notice that this is not just a latency optimization:
    // it is mandatory when VI is disabled (VI_CTRL=0, which makes VI_V_CURRENT=0),
    // because the VI does not generate interrupts in that case.
    disable_interrupts();
    if ((*VI_CTRL & VI_CTRL_TYPE) == VI_CTRL_TYPE_BLANK)
        __vblank_interrupt();
    enable_interrupts();
}

void vi_write_end(void)
{
    assertf(cfg_refcount > 0, "vi_write_end without matching begin");
    --cfg_refcount;
    vi_write_maybe_flush();
}

static void vi_write_idx_masked(int reg_idx, uint32_t wmask, uint32_t value)
{
    assert(reg_idx >= 0 && reg_idx < VI_REGISTERS_COUNT);
    assert((value & ~wmask) == 0);
    disable_interrupts();
    __vi_cfg[reg_idx] = (__vi_cfg[reg_idx] & ~wmask) | value;
    cfg_pending |= 1 << reg_idx;
    vi_write_maybe_flush();
    enable_interrupts();
}

void vi_write_masked(volatile uint32_t *reg, uint32_t wmask, uint32_t value)
{
    vi_write_idx_masked(VI_TO_INDEX(reg), wmask, value);
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
    int x0, y0, x1, y1;
    __get_output(&x0, &y0, &x1, &y1);
    vi_write_masked(VI_X_SCALE, 0xFFF, VI_X_SCALE_SET(fb_width, x1-x0));
}

void vi_set_yscale(float fb_height)
{
    int x0, y0, x1, y1;
    __get_output(&x0, &y0, &x1, &y1);
    vi_write_masked(VI_Y_SCALE, 0xFFF, VI_Y_SCALE_SET(fb_height, (y1-y0)/2));
}

void vi_set_xscale_factor(float xfactor)
{
    int xscale = (int)((xfactor * 1024.0f) + 0.5f);
    assertf(xscale >= 0 && xscale <= 0xFFF, "xfactor out of range: %f", xfactor);
    vi_write_masked(VI_X_SCALE, 0xFFF, (int)((xfactor * 1024.0f) + 0.5f));
}

void vi_set_yscale_factor(float yfactor)
{
    int yscale = (int)((yfactor * 1024.0f) + 0.5f);
    assertf(yscale >= 0 && yscale <= 0xFFF, "yfactor out of range: %f", yfactor);
    vi_write_masked(VI_Y_SCALE, 0xFFF, yscale);
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
    int bpp_shift = fmt == FMT_RGBA16 ? 1 : 2;
    int bpp = 1 << bpp_shift;
    assert(PhysicalAddr(fb->buffer) % 8 == 0);
    assert(fb->stride % 8 == 0);
    vi_write_begin();
    vi_set_origin(fb->buffer, fb->stride >> bpp_shift, bpp * 8);
    vi_set_xscale(fb->width);
    vi_set_yscale(fb->height);
    vi_write_end();
}

void vi_set_interlaced(bool interlaced)
{
    vi_write_begin();
    vi_write_masked(VI_CTRL, VI_CTRL_SERRATE, interlaced ? VI_CTRL_SERRATE : 0);
    // Interlaced mode has one additional vertical half-line. Notice that V_TOTAL
    // is the total number of scanlines **minus one**, so the logic is reversed.
    vi_write_masked(VI_V_TOTAL, 0x1, interlaced ? 0 : 1);
    vi_write_end();
}

void vi_set_aa_mode(vi_aa_mode_t mode)
{
    vi_write_masked(VI_CTRL, VI_AA_MODE_MASK, mode);
}

void vi_set_divot(bool enable)
{
    vi_write_masked(VI_CTRL, VI_DIVOT_ENABLE, enable ? VI_DIVOT_ENABLE : 0);
}

void vi_set_dedither(bool enable)
{
    vi_write_masked(VI_CTRL, VI_DEDITHER_FILTER_ENABLE, enable ? VI_DEDITHER_FILTER_ENABLE : 0);
}

void vi_set_gamma(bool enable)
{
    vi_write_masked(VI_CTRL, VI_GAMMA_ENABLE, enable ? VI_GAMMA_ENABLE : 0);
}

float vi_get_refresh_rate(void)
{
    int clock = preset->clock;
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
    int x0 = preset->display.x0, x1 = x0 + preset->display.width;
    int y0 = preset->display.y0, y1 = y0 + preset->display.height;

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
    b.left  = +(x0 - preset->display.x0);
    b.right = -(x1 - (preset->display.x0 + preset->display.width));
    b.up    = +(y0 - preset->display.y0);
    b.down  = -(y1 - (preset->display.y0 + preset->display.height));
    return b;
}

vi_borders_t vi_calc_borders(float aspect_ratio, float overscan_margin)
{
    const int vi_width = preset->display.width;
    const int vi_height = preset->display.height;
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
        pending_blank = true;
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
        // We define the vblank as the 0->2 current line transition. We can't
        // just check if line 2 is current (VI_V_CURRENT_VBLANK) because it
        // would cause multiple subsequent calls not to wait the next vblank.
        while (vi_get_scanline(NULL) != VI_V_CURRENT_VBLANK-2) {}
        while (vi_get_scanline(NULL) != VI_V_CURRENT_VBLANK) {}
    }
}

void vi_stabilize(volatile uint32_t *reg, bool enable)
{
    int idx = VI_TO_INDEX(reg);
    if (enable)
        cfg_raster |= 1 << idx;
    else
        cfg_raster &= ~(1 << idx);
}

void vi_debug_dump(int verbose)
{
    debugf("CTRL:0x%08lx ORIGIN:0x%06lx WIDTH:0x%08lx V_INTR:0x%lx V_CURRENT:0x%lx\n",
        vi_read(VI_CTRL), vi_read(VI_ORIGIN), vi_read(VI_WIDTH), vi_read(VI_V_INTR), *VI_V_CURRENT);
    debugf("BURST:0x%08lx V_BURST:0x%08lx H_TOTAL:0x%08lx H_TOTAL_LEAP:0x%08lx V_TOTAL:0x%08lx\n",
        vi_read(VI_BURST), vi_read(VI_V_BURST), vi_read(VI_H_TOTAL), vi_read(VI_H_TOTAL_LEAP), vi_read(VI_V_TOTAL));
    debugf("H_VIDEO:0x%08lx, V_VIDEO:0x%08lx X_SCALE:0x%08lx Y_SCALE:0x%08lx\n",
        vi_read(VI_H_VIDEO), vi_read(VI_V_VIDEO), vi_read(VI_X_SCALE), vi_read(VI_Y_SCALE));

    if (verbose == 0)
        return;

    debugf("VIDEO: H:%ld-%ld V:%ld-%ld\n", vi_read(VI_H_VIDEO) >> 16, vi_read(VI_H_VIDEO) & 0xFFFF, vi_read(VI_V_VIDEO) >> 16, vi_read(VI_V_VIDEO) & 0xFFFF);

    uint32_t x_scale = vi_read(VI_X_SCALE);
    uint32_t y_scale = vi_read(VI_Y_SCALE);
    debugf("SCALE: X=%.5f (0x%03lx) Y=%.5f (0x%03lx)\n", 
        (x_scale & 0xFFF) / 1024.0f, x_scale & 0xFFF,
        (y_scale & 0xFFF) / 1024.0f, y_scale & 0xFFF);
    debugf("OFFSET: X=%.5f (0x%03lx) Y=%0.5f (0x%03lx)\n",
        ((x_scale >> 16) & 0xFFF) / 1024.0f, (x_scale >> 16) & 0xFFF,
        ((y_scale >> 16) & 0x3FF) / 1024.0f, (y_scale >> 16) & 0x3FF);
}

void vi_set_line_interrupt(int line, void (*handler)(void))
{
    // When VI_V_INTR bit 0 is set to 0, the interrupt triggers on the line
    // *before* the specified one, in the odd field. This is often surprising
    // (especially across vsync), so let's just force it to 1.
    line |= 1;
    //line &= ~1; // FIXME: this makes things a bit more stable on ares until we fix it

    debugf("VI: Setting line interrupt at line %d\n", line);

    // Insert the new line interrupt at the end of the list of line interrupts,
    // as a negative line number. It will be processed at the beginning of the
    // next frame.
    disable_interrupts();
    if (!cfg_pending_lineirqs) {
        cfg_pending_lineirqs = true;
        for (int i=0; i<MAX_LINE_IRQS; i++)
            new_line_irqs[i] = line_irqs[i];
    }

    if (handler) {
        // Insert the new line interrupt into the interrupt list, and sort it by line number.
        int i;
        for (i=0; i<MAX_LINE_IRQS; i++) {
            if (new_line_irqs[i].line == 0 || new_line_irqs[i].line > line)
                break;
        }
        assertf(i < MAX_LINE_IRQS, "Too many line interrupts");
        for (int j=MAX_LINE_IRQS-1; j>i; j--)
            new_line_irqs[j] = new_line_irqs[j-1];
        new_line_irqs[i].line = line;
        new_line_irqs[i].handler = handler;
    } else {
        // Remove the line interrupt from the interrupt list.
        bool removed = false;
        for (int i=0; i<MAX_LINE_IRQS; i++) {
            if (new_line_irqs[i].line == line) {
                for (int j=i; j<MAX_LINE_IRQS-1; j++)
                    new_line_irqs[j] = new_line_irqs[j+1];
                new_line_irqs[MAX_LINE_IRQS-1].line = 0;
                new_line_irqs[MAX_LINE_IRQS-1].handler = NULL;
                removed = true;
                break;
            }
        }
        assertf(removed, "Line interrupt %d not found", line);
    }
    enable_interrupts();
}

void vi_init(void)
{
    static bool inited = false;
    if (inited) return;
    inited = true;

    // Initialize the preset to the current TV type (progressive mode),
    // and set the pending mask to all registers, so that the whole
    // VI will be programmed at next vblank.
    memset(&__vi_cfg, 0, sizeof(__vi_cfg));
    cfg_pending = cfg_raster = 0;
    cfg_refcount = 0;
    pending_blank = 0;
    cur_line_irq = line_irqs;
    preset = &vi_presets[get_tv_type()];

    vi_write_begin();

    // Configure the timing registers from the preset. These will not change
    // at runtime as they are fixed by the TV standard.
    vi_write(VI_H_TOTAL,      preset->vi_h_total);
    vi_write(VI_H_TOTAL_LEAP, preset->vi_h_total_leap);
    vi_write(VI_V_TOTAL,      preset->vi_v_total);
    vi_write(VI_BURST,        preset->vi_burst);
    vi_write(VI_V_BURST,      preset->vi_v_burst);

    // Configure the default display area from the preset.
    __set_output(preset->display.x0, preset->display.y0,
                 preset->display.x0 + preset->display.width,
                 preset->display.y0 + preset->display.height);

    uint32_t ctrl = 0;
    ctrl |= !sys_bbplayer() ? VI_PIXEL_ADVANCE_DEFAULT : VI_PIXEL_ADVANCE_BBPLAYER;
    ctrl |= VI_AA_MODE_RESAMPLE;
    vi_write(VI_CTRL, ctrl);

    // Mark all registers as pending, so that they will be written at next vblank.
    // This make sure we fully reset the 
    cfg_pending = (1 << VI_REGISTERS_COUNT) - 1;

    vi_write_end();

    memset(line_irqs, 0, sizeof(line_irqs));
    line_irqs[0].line = VI_V_CURRENT_VBLANK;
    line_irqs[0].handler = __vblank_interrupt;

    disable_interrupts();
    register_VI_handler(__vi_interrupt);
    set_VI_interrupt(1, VI_V_CURRENT_VBLANK);
    enable_interrupts();
}

extern inline int vi_get_scanline(int *field);
extern inline void vi_write(volatile uint32_t *reg, uint32_t value);
extern inline uint32_t vi_read(volatile uint32_t *reg);
