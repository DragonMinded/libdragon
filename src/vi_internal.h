/**
 * @file vi.h
 * @brief Video Interface Subsystem
 * @ingroup display
 * 
 * This module offers a low-level interface to VI programming. Most applications
 * should use display.h, which sits on top of vi.h, and offers a higher level
 * interface, plus automatic memory management of multiple framebuffers,
 * FPS utilities, and much more.
 * 
 * ## Framebuffer resizing and video display size
 * 
 * VI has a powerful resampling engine: the framebuffer is not displayed as-is
 * on TV, but it is actually resampled (scaled, stretched), optionally with
 * bilinear filtering. This is a very powerful hardware feature that is a bit
 * complicated to configure, so an effort was made to expose an intuitive API
 * to programmers.
 *
 * By default, this module configures the VI to resample an arbitrary framebuffer
 * picture into a virtual 640x480 display output with 4:3 aspect ratio (on NTSC
 * and MPAL), or a virtual 640x576 display output though with the same 4:3
 * aspect ratio on PAL. This is done to achieve TV type independence: since
 * both resolution share the same aspect ratio on their respective TV standards
 * (given that dots are square on NTSC but they are not on PAL), it means
 * that the framebuffer will look the same on all TVs; on PAL, you will be
 * trading additional vertical resolution (assuming the framebuffer is big
 * enough to have that detail). 
 * 
 * This also means that games don't have to do anything special to handle NTSC
 * vs PAL (besides maybe accounting for the different refresh rate), which is
 * an important goal.
 * 
 * In reality, TVs didn't have a 480-lines vertical resolution so the actual
 * output depends on whether you request interlaced display or not:
 * 
 *  * In case of non interlaced display, the actual resolution is 640x240, but
 *    since dots will be configured to be twice as big vertically, the aspect
 *    ratio will be 4:3 as-if the image was 640x480 (with duplicated scanlines)
 *  * In case of interlaced display, you do get to display 480 scanlines, by
 *    alternating two slightly-shifted 640x240 pictures.
 * 
 * As an example, if you configure a framebuffer resolution like 512x320, with
 * interlacing turned off, what happens is that the image gets scaled into
 * 640x240, so horizontally some pixels will be duplicated to enlarge the
 * resolution to 640, but vertically some scanlines will be dropped. The
 * output display aspect ratio will still be 4:3, which is not the source aspect
 * ratio of the framebuffer (512 / 320 = 1.6666 = 16:10), so the image will
 * appear squished, unless obviously this was accounted for while drawing to
 * the framebuffer.
 * 
 * While resampling the framebuffer into the display output, the VI can use either
 * bilinear filtering or simple nearest sampling (duplicating or dropping pixels).
 * See #filter_options_t for more information on configuring
 * the VI image filters.
 * 
 * The 640x480 virtual display output can be fully viewed on emulators and on
 * modern screens (via grabbers, converters, etc.). When displaying on old
 * CRTs though, part of the display will be hidden because of the overscan.
 * To account for that, it is possible to reduce the 640x480 display output
 * by adding black borders. For instance, if you specify 12 dots of borders
 * on all the four edges, you will get a 616x456 display output, plus
 * the requested 12 dots of borders on all sides; the actual display output
 * will thus be smaller, and possibly get fully out of overscan. The value
 * #VI_CRT_MARGIN is a good default you can use for overscan compensation on
 * most CRT TVs.
 * 
 * Notice that adding borders also affect the aspect ratio of the display output;
 * for instance, in the above example, the 616x456 display output is not
 * exactly 4:3 anymore, but more like 4.05:3. By carefully calculating borders,
 * thus, it is possible to obtain specific display outputs with custom aspect
 * ratios (eg: 16:9).
 * 
 * To help calculating the borders by taking both potential goals into account
 * (overscan compensation and aspect ratio changes), you can use #vi_calc_borders_int.
 */
#ifndef __LIBDRAGON_VI_INTERNAL_H
#define __LIBDRAGON_VI_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "n64sys.h"
#include "vi.h"

/**
 * @addtogroup display
 * @{
 */


/**
 * @brief Video Interface register structure
 *
 * Whenever trying to configure VI registers, 
 * this struct and its index definitions below can be very useful 
 * in writing comprehensive and verbose code.
 */
typedef struct vi_config_s{
    uint32_t regs[VI_REGISTERS_COUNT];
} vi_config_t;

/** @brief VI index from register */
#define VI_TO_INDEX(reg) ((reg) - VI_REGISTERS)


/** Under VI_BURST     */
/** @brief VI_BURST Register: set start of color burst in pixels from hsync. */
#define VI_BURST_START(value)               ((value & 0x3FF) << 20)
/** @brief VI_BURST Register: set vertical sync width in half lines. */
#define VI_VSYNC_WIDTH(value)               ((value & 0xF)  << 16)
/** @brief VI_BURST Register: set color burst width in pixels. */
#define VI_BURST_WIDTH(value)               ((value & 0xFF) << 8)
/** @brief VI_BURST Register: set horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH(value)               ((value & 0xFF) << 0)
/** @brief VI_BURST Register: set all values. */
#define VI_BURST_SET(burst_start, vsync_width, burst_width, hsync_width)                                \
                                            VI_BURST_START(burst_start) | VI_VSYNC_WIDTH(vsync_width) | \
                                            VI_BURST_WIDTH(burst_width) | VI_HSYNC_WIDTH(hsync_width)

/** @brief VI_BURST Register: NTSC default start of color burst in pixels from hsync. */
#define VI_BURST_START_NTSC                 62
/** @brief VI_BURST Register: NTSC default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_NTSC                 5
/** @brief VI_BURST Register: NTSC default color burst width in pixels. */
#define VI_BURST_WIDTH_NTSC                 34
/** @brief VI_BURST Register: NTSC default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_NTSC                 57
/** @brief VI_BURST Register: NTSC default setting. */
#define VI_BURST_NTSC                       VI_BURST_SET(VI_BURST_START_NTSC, VI_VSYNC_WIDTH_NTSC, VI_BURST_WIDTH_NTSC, VI_HSYNC_WIDTH_NTSC)

/** @brief VI_BURST Register: PAL default start of color burst in pixels from hsync. */
#define VI_BURST_START_PAL                  64
/** @brief VI_BURST Register: PAL default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_PAL                  4
/** @brief VI_BURST Register: PAL default color burst width in pixels.  */
#define VI_BURST_WIDTH_PAL                  35
/** @brief VI_BURST Register: PAL default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_PAL                  58
/** @brief VI_BURST Register: PAL default setting. */
#define VI_BURST_PAL                        VI_BURST_SET(VI_BURST_START_PAL, VI_VSYNC_WIDTH_PAL, VI_BURST_WIDTH_PAL, VI_HSYNC_WIDTH_PAL)

/** @brief VI_BURST Register: MPAL default start of color burst in pixels from hsync. */
#define VI_BURST_START_MPAL                 70
/** @brief VI_BURST Register: MPAL default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_MPAL                 5
/** @brief VI_BURST Register: MPAL default color burst width in pixels.  */
#define VI_BURST_WIDTH_MPAL                 30
/** @brief VI_BURST Register: MPAL default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_MPAL                 57
/** @brief VI_BURST Register: MPAL default setting. */
#define VI_BURST_MPAL                       VI_BURST_SET(VI_BURST_START_MPAL, VI_VSYNC_WIDTH_MPAL, VI_BURST_WIDTH_MPAL, VI_HSYNC_WIDTH_MPAL)

/**
 * @name Video Mode Register Presets
 * @brief Presets to begin with when setting a particular video mode
 * @{
 */
static const vi_config_t vi_ntsc_p = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_NTSC,
    VI_V_TOTAL_SET(526),
    VI_H_TOTAL_SET(0b00000, 773.5),
    VI_H_TOTAL_LEAP_SET(773.5, 773.5),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(35, 515),
    VI_V_BURST_SET(14, 516),
    VI_X_SCALE_SET(0, 640),
    VI_Y_SCALE_SET(0, 240),
}};
static const vi_config_t vi_pal_p =  {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_PAL,
    VI_V_TOTAL_SET(626),
    VI_H_TOTAL_SET(0b10101, 794.5),
    VI_H_TOTAL_LEAP_SET(796, 795.75),
    VI_H_VIDEO_SET(128, 768),
    VI_V_VIDEO_SET(45, 621),
    VI_V_BURST_SET(9, 619),
    VI_X_SCALE_SET(0, 640),
    VI_Y_SCALE_SET(0, 288),
}};
static const vi_config_t vi_mpal_p = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_MPAL,
    VI_V_TOTAL_SET(526),
    VI_H_TOTAL_SET(0b00000, 772.25),
    VI_H_TOTAL_LEAP_SET(775.25, 775.25),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(37, 511),
    VI_V_BURST_SET(14, 516),
    VI_X_SCALE_SET(0, 640),
    VI_Y_SCALE_SET(0, 240)
}};
static const vi_config_t vi_ntsc_i = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_NTSC,
    VI_V_TOTAL_SET(525),
    VI_H_TOTAL_SET(0b00000, 773.5),
    VI_H_TOTAL_LEAP_SET(773.5, 773.5),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(35, 515),
    VI_V_BURST_SET(14, 516),
    VI_X_SCALE_SET(0, 640),
    VI_Y_SCALE_SET(0, 240)
}};
static const vi_config_t vi_pal_i = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_PAL,
    VI_V_TOTAL_SET(625),
    VI_H_TOTAL_SET(0b10101, 794.5),
    VI_H_TOTAL_LEAP_SET(796, 795.75),
    VI_H_VIDEO_SET(128, 768),
    VI_V_VIDEO_SET(45, 621),
    VI_V_BURST_SET(9, 619),
    VI_X_SCALE_SET(0, 640),
    VI_Y_SCALE_SET(0, 288)
}};
static const vi_config_t vi_mpal_i = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_MPAL,
    VI_V_TOTAL_SET(525),
    VI_H_TOTAL_SET(0b00000, 772.25),
    VI_H_TOTAL_LEAP_SET(775.25, 775.25),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(35, 509),
    VI_V_BURST_SET(11, 514),
    VI_X_SCALE_SET(0, 640),
    VI_Y_SCALE_SET(0, 240)
}};
/** @} */

/** @brief Register initial value array */
static const vi_config_t vi_config_presets[2][3] = {
    {vi_pal_p, vi_ntsc_p, vi_mpal_p},
    {vi_pal_i, vi_ntsc_i, vi_mpal_i}
};

/**
 * @brief Calculate correct VI borders for a target aspect ratio.
 * 
 * This function calculates the appropriate VI borders to obtain the specified
 * aspect ratio, and optionally adding a margin to make the picture CRT-safe.
 * 
 * The margin is expressed as a percentage relative to the virtual VI display
 * output (640x480). A good default for this margin for most CRTs is
 * #VI_CRT_MARGIN (5%).
 * 
 * For instance, to create a 16:9 resolution, you can do:
 * 
 * \code{.c}
 *      vi_borders_t borders = vi_calc_borders_int(TV_NTSC, 16./9, false);
 * \endcode
 * 
 * @param tv_type           TV type for which the calculation should be performed
 * @param aspect_ratio      Target aspect ratio
 * @param overscan_margin   Margin to add to compensate for TV overscan. Use 0
 *                          to use full picture (eg: for emulators), and something
 *                          like #VI_CRT_MARGIN to get a good CRT default.
 * 
 * @return vi_borders_t The requested border settings
 */
static inline vi_borders_t vi_calc_borders_int(int tv_type, float aspect_ratio, float overscan_margin)
{
    const int vi_width = 640;
    const int vi_height = tv_type == TV_PAL ? 576 : 480;
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

/**
 * @brief Write a set of video registers to the VI
 *
 * @param[in] reg
 *            A pointer to a register to be written
 * @param[in] value
 *            Value to be written to the register
 */
inline void vi_write_safe(volatile uint32_t *reg, uint32_t value){
    assert(reg); /* This should never happen */
    *reg = value;
    MEMORY_BARRIER();
}

/**
 * @brief Write a set of video registers to the VI
 *
 * @param[in] config
 *            A pointer to a set of register values to be written
 */
inline void vi_write_config(const vi_config_t* config)
{
    /* This should never happen */
    assert(config);

    /* Just straight copy */
    for( int i = 0; i < VI_REGISTERS_COUNT; i++ )
    {
        /* Don't clear interrupts */
        if( i == 3 ) { continue; }
        if( i == 4 ) { continue; }

        *VI_TO_REGISTER(i) = config->regs[i];
        MEMORY_BARRIER();
    }
}

/**
 * @brief Update the framebuffer pointer in the VI
 *
 * @param[in] dram_val
 *            The new framebuffer to use for display.  Should be aligned and uncached.
 */
static inline void vi_write_dram_register( void const * const dram_val )
{
    *VI_ORIGIN = VI_ORIGIN_SET(PhysicalAddr(dram_val));
    MEMORY_BARRIER();
}

/** @brief Wait until entering the vblank period */
static inline void vi_wait_for_vblank()
{
    while((*VI_V_CURRENT & ~1) != VI_V_CURRENT_VBLANK ) {  }
}

/** @brief Return true if VI is sending a video signal (16-bit or 32-bit color set) */
static inline bool vi_is_active()
{
    return (*VI_CTRL & VI_CTRL_TYPE) != VI_CTRL_TYPE_BLANK;
}

/** @brief Set active image width to 0, which keeps VI signal active but only sending a blank image */
static inline void vi_set_blank_image()
{
    vi_write_safe(VI_H_VIDEO, 0);
}

/** @brief Install an interrupt on a specific screen line */
void vi_set_line_interrupt(int line, void (*handler)(void));

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

/** @} */ /* vi */

#endif
